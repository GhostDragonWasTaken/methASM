param(
    [string]$Compiler = "bin\mettle.exe",
    [string]$Out = "tools\regalloc_census\latest.txt",
    [switch]$Examples,
    [switch]$Ssa,
    [switch]$Verify,
    [int]$Worst = 10,
    [string]$WorkDir = "",
    [switch]$Quiet
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

$CompilerPath = if ([System.IO.Path]::IsPathRooted($Compiler)) { $Compiler } else { [System.IO.Path]::GetFullPath((Join-Path $Root $Compiler)) }
if (-not (Test-Path $CompilerPath)) { throw "compiler not found: $CompilerPath" }

if ([string]::IsNullOrEmpty($WorkDir)) {
    $WorkDir = Join-Path $env:TEMP ("mettle_ra_census_" + [System.Diagnostics.Process]::GetCurrentProcess().Id)
}
if (-not (Test-Path $WorkDir)) { New-Item -ItemType Directory -Path $WorkDir -Force | Out-Null }

$OutPath = [System.IO.Path]::GetFullPath((Join-Path $Root $Out))
$OutDir = Split-Path -Parent $OutPath
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir -Force | Out-Null }

function Get-CensusSources {
    $sources = New-Object System.Collections.ArrayList
    $cfgPath = Join-Path $Root "docs\benchmarks\harness.json"
    if (Test-Path $cfgPath) {
        $cfgRaw = [System.IO.File]::ReadAllText($cfgPath)
        $cfg = $cfgRaw.TrimStart([char]0xFEFF) | ConvertFrom-Json
        foreach ($b in $cfg.benchmarks) {
            if ($b.kind -ne "runtime") { continue }
            if (-not $b.mettle_source) { continue }
            $p = Join-Path $Root $b.mettle_source
            if (Test-Path $p) {
                [void]$sources.Add([pscustomobject]@{ Name = $b.name; Path = $p; Suite = $b.suite })
            }
        }
    }
    if ($Examples) {
        $seen = @{}
        foreach ($s in $sources) { $seen[$s.Path.ToLowerInvariant()] = $true }
        foreach ($d in (Get-ChildItem -Path (Join-Path $Root "examples") -Directory)) {
            foreach ($f in (Get-ChildItem -Path $d.FullName -Filter "*.mettle" -File)) {
                $key = $f.FullName.ToLowerInvariant()
                if ($seen.ContainsKey($key)) { continue }
                $seen[$key] = $true
                [void]$sources.Add([pscustomobject]@{ Name = ($d.Name + "/" + $f.BaseName); Path = $f.FullName; Suite = 0 })
            }
        }
    }
    return $sources
}

function Invoke-Arm {
    param([pscustomobject]$Source, [string]$Arm, [hashtable]$Extra)
    $exe = Join-Path $WorkDir (($Source.Name -replace '[\\/]', '_') + "_" + $Arm + ".exe")
    $err = Join-Path $WorkDir (($Source.Name -replace '[\\/]', '_') + "_" + $Arm + ".err")
    $outFile = Join-Path $WorkDir "compile.out"
    $env:METTLE_REGALLOC_TRACE = "1"
    if ($Verify) { $env:METTLE_REGALLOC_VERIFY = "1" }
    foreach ($k in $Extra.Keys) { Set-Item -Path "Env:\$k" -Value $Extra[$k] }
    $argList = @("--build", "--release", ('"' + $Source.Path + '"'), "-o", ('"' + $exe + '"'))
    $p = Start-Process -FilePath $CompilerPath -ArgumentList $argList -NoNewWindow -Wait -PassThru `
                       -RedirectStandardOutput $outFile -RedirectStandardError $err
    foreach ($k in $Extra.Keys) { Remove-Item -Path "Env:\$k" -ErrorAction SilentlyContinue }
    Remove-Item -Path "Env:\METTLE_REGALLOC_TRACE" -ErrorAction SilentlyContinue
    Remove-Item -Path "Env:\METTLE_REGALLOC_VERIFY" -ErrorAction SilentlyContinue

    $rows = New-Object System.Collections.ArrayList
    $failures = 0
    if (Test-Path $err) {
        foreach ($line in ([System.IO.File]::ReadAllText($err) -split "`n")) {
            $line = $line.TrimEnd("`r")
            if ($line.StartsWith("RA-VERIFY") -and $line.Contains("FAIL")) { $failures++ }
            if (-not $line.StartsWith("RA-DONE")) { continue }
            $parts = $line -split "`t"
            if ($parts.Count -lt 3) { continue }
            $row = [ordered]@{ Unit = $Source.Name; Suite = $Source.Suite; Function = $parts[1]; Arm = $Arm
                               Kept = 0; Spilled = 0; Copies = 0; Coalesced = 0; SpillSide = 0; Merged = 0 }
            foreach ($kv in $parts[2..($parts.Count - 1)]) {
                $eq = $kv.IndexOf("=")
                if ($eq -lt 0) { continue }
                $k = $kv.Substring(0, $eq)
                $v = [int]$kv.Substring($eq + 1)
                switch ($k) {
                    "kept" { $row.Kept = $v }
                    "spilled" { $row.Spilled = $v }
                    "copies" { $row.Copies = $v }
                    "coalesced" { $row.Coalesced = $v }
                    "spill_side" { $row.SpillSide = $v }
                    "merged" { $row.Merged = $v }
                }
            }
            [void]$rows.Add([pscustomobject]$row)
        }
    }
    return [pscustomobject]@{ Rows = $rows; ExitCode = $p.ExitCode; Failures = $failures }
}

$sources = Get-CensusSources
if ($sources.Count -eq 0) { throw "no sources" }

$arms = @(@{ Name = "base"; Extra = @{} })
if ($Ssa) { $arms += @{ Name = "ssa"; Extra = @{ METTLE_IR_SSA = "1" } } }

$all = New-Object System.Collections.ArrayList
$compileFailures = New-Object System.Collections.ArrayList
$verifyFailures = New-Object System.Collections.ArrayList
foreach ($s in $sources) {
    foreach ($arm in $arms) {
        $r = Invoke-Arm -Source $s -Arm $arm.Name -Extra $arm.Extra
        if ($r.ExitCode -ne 0) { [void]$compileFailures.Add($s.Name + " [" + $arm.Name + "]") }
        if ($r.Failures -gt 0) { [void]$verifyFailures.Add($s.Name + " [" + $arm.Name + "] " + $r.Failures) }
        foreach ($row in $r.Rows) { [void]$all.Add($row) }
        if (-not $Quiet) {
            $kept = ($r.Rows | Measure-Object -Property Kept -Sum).Sum
            $sp = ($r.Rows | Measure-Object -Property Spilled -Sum).Sum
            $cp = ($r.Rows | Measure-Object -Property Copies -Sum).Sum
            $co = ($r.Rows | Measure-Object -Property Coalesced -Sum).Sum
            Write-Host ("{0,-40} {1,-5} exit={2} kept={3} spilled={4} copies={5} coalesced={6}" -f $s.Name, $arm.Name, $r.ExitCode, $kept, $sp, $cp, $co)
        }
    }
}

$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("regalloc census  compiler=" + $CompilerPath)
[void]$sb.AppendLine("sources=" + $sources.Count + "  arms=" + (($arms | ForEach-Object { $_.Name }) -join ","))
[void]$sb.AppendLine("")
foreach ($arm in $arms) {
    $rows = @($all | Where-Object { $_.Arm -eq $arm.Name })
    $kept = ($rows | Measure-Object -Property Kept -Sum).Sum
    $sp = ($rows | Measure-Object -Property Spilled -Sum).Sum
    $cp = ($rows | Measure-Object -Property Copies -Sum).Sum
    $co = ($rows | Measure-Object -Property Coalesced -Sum).Sum
    $ss = ($rows | Measure-Object -Property SpillSide -Sum).Sum
    $share = if (($kept + $sp) -gt 0) { [math]::Round(100.0 * $sp / ($kept + $sp), 2) } else { 0 }
    [void]$sb.AppendLine(("arm {0}: functions={1} kept={2} spilled={3} ({4}%) copies={5} coalesced={6} spill_side={7}" -f $arm.Name, $rows.Count, $kept, $sp, $share, $cp, $co, $ss))
    foreach ($suite in @(3, 2, 1, 0)) {
        $sr = @($rows | Where-Object { $_.Suite -eq $suite })
        if ($sr.Count -eq 0) { continue }
        $k2 = ($sr | Measure-Object -Property Kept -Sum).Sum
        $s2 = ($sr | Measure-Object -Property Spilled -Sum).Sum
        $c2 = ($sr | Measure-Object -Property Copies -Sum).Sum
        $o2 = ($sr | Measure-Object -Property Coalesced -Sum).Sum
        [void]$sb.AppendLine(("  suite {0}: kept={1} spilled={2} copies={3} coalesced={4}" -f $suite, $k2, $s2, $c2, $o2))
    }
    [void]$sb.AppendLine("  worst by spills:")
    foreach ($r in ($rows | Sort-Object -Property Spilled -Descending | Select-Object -First $Worst)) {
        [void]$sb.AppendLine(("    {0,-28} {1,-24} kept={2,-5} spilled={3,-5} copies={4,-5} coalesced={5}" -f $r.Unit, $r.Function, $r.Kept, $r.Spilled, $r.Copies, $r.Coalesced))
    }
    [void]$sb.AppendLine("  worst by surviving copies:")
    foreach ($r in ($rows | Sort-Object -Property @{ Expression = { $_.Copies - $_.Coalesced }; Descending = $true } | Select-Object -First $Worst)) {
        [void]$sb.AppendLine(("    {0,-28} {1,-24} copies={2,-5} coalesced={3,-5} spill_side={4,-5} spilled={5}" -f $r.Unit, $r.Function, $r.Copies, $r.Coalesced, $r.SpillSide, $r.Spilled))
    }
    [void]$sb.AppendLine("")
}
if ($arms.Count -eq 2) {
    [void]$sb.AppendLine("ssa minus base, per function (spilled delta, worst first):")
    $base = @{}
    foreach ($r in ($all | Where-Object { $_.Arm -eq "base" })) { $base[$r.Unit + "::" + $r.Function] = $r }
    $deltas = New-Object System.Collections.ArrayList
    foreach ($r in ($all | Where-Object { $_.Arm -eq "ssa" })) {
        $key = $r.Unit + "::" + $r.Function
        if (-not $base.ContainsKey($key)) { continue }
        $b = $base[$key]
        [void]$deltas.Add([pscustomobject]@{ Key = $key; Spilled = $r.Spilled - $b.Spilled; Copies = ($r.Copies - $r.Coalesced) - ($b.Copies - $b.Coalesced) })
    }
    foreach ($d in ($deltas | Sort-Object -Property Spilled -Descending | Select-Object -First $Worst)) {
        [void]$sb.AppendLine(("    {0,-52} spilled{1,5} surviving_copies{2,5}" -f $d.Key, $d.Spilled, $d.Copies))
    }
    [void]$sb.AppendLine("")
}
if ($compileFailures.Count -gt 0) {
    [void]$sb.AppendLine("COMPILE FAILURES:")
    foreach ($f in $compileFailures) { [void]$sb.AppendLine("  " + $f) }
}
if ($verifyFailures.Count -gt 0) {
    [void]$sb.AppendLine("VERIFIER FAILURES:")
    foreach ($f in $verifyFailures) { [void]$sb.AppendLine("  " + $f) }
}

[System.IO.File]::WriteAllText($OutPath, $sb.ToString())
Write-Host $sb.ToString()
Write-Host ("written: " + $OutPath)
if ($compileFailures.Count -gt 0 -or $verifyFailures.Count -gt 0) { exit 1 }
