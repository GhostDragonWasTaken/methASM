param(
    [string]$Compiler = "bin\mettle.exe",
    [string]$Out = "tools\ir_census\baseline.txt",
    [switch]$Examples,
    [switch]$NoCompile,
    [string]$WorkDir = "",
    [ValidateSet("off", "check", "strict")]
    [string]$ValueIds = "off",
    [ValidateSet("off", "census", "strict")]
    [string]$Structure = "off",
    [ValidateSet("off", "on", "verbose")]
    [string]$DomCrossCheck = "off",
    [switch]$Quiet
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

$CompilerPath = [System.IO.Path]::GetFullPath((Join-Path $Root $Compiler))
if (-not (Test-Path $CompilerPath)) { throw "compiler not found: $CompilerPath" }

if ([string]::IsNullOrEmpty($WorkDir)) {
    $WorkDir = Join-Path $env:TEMP ("mettle_ir_census_" + [System.Diagnostics.Process]::GetCurrentProcess().Id)
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

$reInstruction = [regex]::new('^\s+(\d+): (.*)$', 'Compiled')
$reFunction = [regex]::new('^function ([^\s]+)', 'Compiled')
$reBlock = [regex]::new('^  block (\d+)', 'Compiled')
$reDefine = [regex]::new('^(%[A-Za-z0-9_.$]+|@[A-Za-z0-9_.$]+) (=|<-) ', 'Compiled')
$reLocal = [regex]::new('^local (@[A-Za-z0-9_.$]+)', 'Compiled')
$reAddressOf = [regex]::new('&(@[A-Za-z0-9_.$]+)', 'Compiled')
$rePhi = [regex]::new('^(%[A-Za-z0-9_.$]+|@[A-Za-z0-9_.$]+) = phi ', 'Compiled')

function Measure-IrFile {
    param([string]$Path, [string]$Unit)

    $results = New-Object System.Collections.ArrayList
    $reader = [System.IO.StreamReader]::new($Path)
    try {
        $fn = $null
        $defs = $null
        $locals = $null
        $addressTaken = $null
        $blocks = 0
        $instructions = 0
        $phis = 0

        $flush = {
            if ($null -ne $fn) {
                $temps = 0; $tempsMulti = 0; $syms = 0; $symsMulti = 0
                foreach ($e in $defs.GetEnumerator()) {
                    if ($e.Key[0] -eq '%') {
                        $temps++
                        if ($e.Value -gt 1) { $tempsMulti++ }
                    } else {
                        $syms++
                        if ($e.Value -gt 1) { $symsMulti++ }
                    }
                }
                [void]$results.Add([pscustomobject]@{
                    Unit = $Unit
                    Function = $fn
                    Blocks = $blocks
                    Instructions = $instructions
                    Temps = $temps
                    TempsMultiDef = $tempsMulti
                    Symbols = $syms
                    SymbolsMultiDef = $symsMulti
                    Locals = $locals.Count
                    LocalsAddressTaken = $addressTaken.Count
                    Phis = $phis
                })
            }
        }

        while ($null -ne ($line = $reader.ReadLine())) {
            $mf = $reFunction.Match($line)
            if ($mf.Success) {
                & $flush
                $fn = $mf.Groups[1].Value
                $defs = @{}
                $locals = @{}
                $addressTaken = @{}
                $blocks = 0
                $instructions = 0
                $phis = 0
                continue
            }
            if ($null -eq $fn) { continue }
            if ($reBlock.IsMatch($line)) { $blocks++; continue }
            $mi = $reInstruction.Match($line)
            if (-not $mi.Success) { continue }
            $instructions++
            $body = $mi.Groups[2].Value

            $ml = $reLocal.Match($body)
            if ($ml.Success) {
                $locals[$ml.Groups[1].Value] = $true
                continue
            }
            if ($rePhi.IsMatch($body)) { $phis++ }
            $md = $reDefine.Match($body)
            if ($md.Success) {
                $name = $md.Groups[1].Value
                if ($defs.ContainsKey($name)) { $defs[$name] = $defs[$name] + 1 } else { $defs[$name] = 1 }
            }
            foreach ($ma in $reAddressOf.Matches($body)) {
                $addressTaken[$ma.Groups[1].Value] = $true
            }
        }
        & $flush
    } finally {
        $reader.Dispose()
    }
    return $results
}

function Measure-SourceBurndown {
    $dirs = @(
        @{ Label = "src/ir/optimizer"; Path = Join-Path $Root "src\ir\optimizer" },
        @{ Label = "src/ir"; Path = Join-Path $Root "src\ir" },
        @{ Label = "src/codegen/binary"; Path = Join-Path $Root "src\codegen\binary" },
        @{ Label = "src/codegen"; Path = Join-Path $Root "src\codegen" }
    )
    $rows = New-Object System.Collections.ArrayList
    $reName = [regex]::new('\.name\b|->name\b', 'Compiled')
    $reStrcmpName = [regex]::new('str(n?)cmp\s*\([^;]*\bname\b', 'Compiled')
    $reForwardScan = [regex]::new('for\s*\(\s*size_t\s+(\w+)\s*=\s*\w+\s*\+\s*1\s*;\s*\1\s*<\s*[^;]*instruction_count', 'Compiled')
    foreach ($d in $dirs) {
        if (-not (Test-Path $d.Path)) { continue }
        $files = Get-ChildItem -Path $d.Path -Filter "*.c" -File
        foreach ($f in $files) {
            $text = [System.IO.File]::ReadAllText($f.FullName)
            $nameReads = $reName.Matches($text).Count
            $strcmps = $reStrcmpName.Matches($text).Count
            $scans = $reForwardScan.Matches($text).Count
            if ($nameReads -eq 0 -and $strcmps -eq 0 -and $scans -eq 0) { continue }
            [void]$rows.Add([pscustomobject]@{
                Dir = $d.Label
                File = $f.Name
                NameReads = $nameReads
                NameCompares = $strcmps
                ForwardScans = $scans
            })
        }
    }
    return $rows
}

$sources = Get-CensusSources
if (-not $Quiet) { Write-Host ("census over " + $sources.Count + " sources") }

$irFiles = New-Object System.Collections.ArrayList
$compileFailures = New-Object System.Collections.ArrayList
$valueIdReports = New-Object System.Collections.ArrayList
$structureReports = New-Object System.Collections.ArrayList
$structureRegressions = New-Object System.Collections.ArrayList
$domReports = New-Object System.Collections.ArrayList
$domAgreements = 0
if ($DomCrossCheck -ne "off") { $env:METTLE_DOM_CROSSCHECK = $(if ($DomCrossCheck -eq "verbose") { "verbose" } else { "1" }) } else { Remove-Item Env:\METTLE_DOM_CROSSCHECK -ErrorAction SilentlyContinue }
if ($ValueIds -ne "off") { $env:METTLE_VALUE_IDS = $ValueIds } else { Remove-Item Env:\METTLE_VALUE_IDS -ErrorAction SilentlyContinue }
if ($Structure -ne "off") { $env:METTLE_IR_STRUCT = $Structure } else { Remove-Item Env:\METTLE_IR_STRUCT -ErrorAction SilentlyContinue }
$index = 0
foreach ($s in $sources) {
    $index++
    $stem = ($s.Name -replace '[^A-Za-z0-9_]', '_')
    $objPath = Join-Path $WorkDir ($stem + ".obj")
    $irPath = $objPath + ".ir"
    if (-not $NoCompile) {
        if (Test-Path $irPath) { Remove-Item $irPath -Force }
        $argList = @("--emit-obj", "--release", "--dump-ir",
                     ('"' + $s.Path + '"'), "-o", ('"' + $objPath + '"'))
        $logOut = Join-Path $WorkDir "compile.out"
        $logErr = Join-Path $WorkDir "compile.err"
        $proc = Start-Process -FilePath $CompilerPath -ArgumentList $argList -NoNewWindow -Wait -PassThru `
                              -RedirectStandardOutput $logOut -RedirectStandardError $logErr
        if (($ValueIds -ne "off" -or $Structure -ne "off" -or $DomCrossCheck -ne "off") -and (Test-Path $logErr)) {
            foreach ($ln in (Get-Content $logErr)) {
                if ($ln -match "value id disagrees") {
                    [void]$valueIdReports.Add([pscustomobject]@{ Name = $s.Name; Detail = $ln.Trim() })
                } elseif ($ln -match "ir structure broken") {
                    [void]$structureReports.Add([pscustomobject]@{ Name = $s.Name; Detail = $ln.Trim() })
                } elseif ($ln -match "dominance (cross-check|self-check) agrees") {
                    $domAgreements++
                } elseif ($ln -match "dominance (cross-check|self-check)") {
                    [void]$domReports.Add([pscustomobject]@{ Name = $s.Name; Detail = $ln.Trim() })
                } elseif ($ln -match "raised the values defined more than once") {
                    $pass = "unknown"
                    if ($ln -match "pass '([^']+)'") { $pass = $Matches[1] }
                    [void]$structureRegressions.Add([pscustomobject]@{ Name = $s.Name; Pass = $pass; Detail = $ln.Trim() })
                }
            }
        }
        if ($proc.ExitCode -ne 0) {
            $detail = ""
            if (Test-Path $logErr) { $detail = (Get-Content $logErr -Tail 1) }
            [void]$compileFailures.Add([pscustomobject]@{ Name = $s.Name; Detail = ("exit " + $proc.ExitCode + " " + $detail) })
            continue
        }
    }
    if (-not (Test-Path $irPath)) {
        [void]$compileFailures.Add([pscustomobject]@{ Name = $s.Name; Detail = "no ir sidecar" })
        continue
    }
    [void]$irFiles.Add([pscustomobject]@{ Name = $s.Name; Suite = $s.Suite; Path = $irPath })
    if (-not $Quiet -and ($index % 10 -eq 0)) { Write-Host ("  compiled " + $index + "/" + $sources.Count) }
}

$rows = New-Object System.Collections.ArrayList
foreach ($f in $irFiles) {
    foreach ($r in (Measure-IrFile -Path $f.Path -Unit $f.Name)) { [void]$rows.Add($r) }
}

$burndown = Measure-SourceBurndown

$sb = New-Object System.Text.StringBuilder
function Add-Line { param([string]$Text) [void]$sb.AppendLine($Text) }

Add-Line ("mettle ir census")
Add-Line ("generated " + (Get-Date).ToString("yyyy-MM-dd HH:mm:ss"))
$head = (& git -C $Root rev-parse --short HEAD 2>$null)
Add-Line ("commit " + $head)
Add-Line ("compiler " + $CompilerPath)
Add-Line ("units " + $irFiles.Count + " of " + $sources.Count)
Add-Line ("")

if ($compileFailures.Count -gt 0) {
    Add-Line ("compile failures " + $compileFailures.Count)
    foreach ($e in $compileFailures) { Add-Line ("  " + $e.Name + ": " + $e.Detail) }
    Add-Line ("")
}

if ($ValueIds -ne "off") {
    Add-Line ("value id mode " + $ValueIds)
    Add-Line ("value id disagreements " + $valueIdReports.Count)
    foreach ($e in ($valueIdReports | Select-Object -First 40)) { Add-Line ("  " + $e.Name + ": " + $e.Detail) }
    Add-Line ("")
}

if ($DomCrossCheck -ne "off") {
    Add-Line ("dominance cross-check mode " + $DomCrossCheck)
    Add-Line ("dominance functions agreeing " + $domAgreements)
    Add-Line ("dominance mismatches " + $domReports.Count)
    foreach ($e in ($domReports | Select-Object -First 40)) { Add-Line ("  " + $e.Name + ": " + $e.Detail) }
    Add-Line ("")
}

if ($Structure -ne "off") {
    Add-Line ("structure mode " + $Structure)
    Add-Line ("structure violations " + $structureReports.Count)
    foreach ($e in ($structureReports | Select-Object -First 40)) { Add-Line ("  " + $e.Name + ": " + $e.Detail) }
    Add-Line ("multi-def raised by a pass: " + $structureRegressions.Count + " reports")
    $byPass = $structureRegressions | Group-Object -Property Pass | Sort-Object -Property Count -Descending
    foreach ($g in $byPass) {
        Add-Line ("  {0,-36} {1,5} reports over {2} units" -f $g.Name, $g.Count, (($g.Group | Group-Object -Property Name).Count))
    }
    Add-Line ("")
}

$totalFunctions = $rows.Count
$totalBlocks = ($rows | Measure-Object -Property Blocks -Sum).Sum
$totalInstructions = ($rows | Measure-Object -Property Instructions -Sum).Sum
$totalTemps = ($rows | Measure-Object -Property Temps -Sum).Sum
$totalTempsMulti = ($rows | Measure-Object -Property TempsMultiDef -Sum).Sum
$totalSymbols = ($rows | Measure-Object -Property Symbols -Sum).Sum
$totalSymbolsMulti = ($rows | Measure-Object -Property SymbolsMultiDef -Sum).Sum
$totalLocals = ($rows | Measure-Object -Property Locals -Sum).Sum
$totalAddressTaken = ($rows | Measure-Object -Property LocalsAddressTaken -Sum).Sum
$totalPhis = ($rows | Measure-Object -Property Phis -Sum).Sum

function Format-Pct {
    param([double]$Num, [double]$Den)
    if ($Den -le 0) { return "n/a" }
    return ((100.0 * $Num / $Den).ToString("F2", [System.Globalization.CultureInfo]::InvariantCulture) + "%")
}

Add-Line ("value census")
Add-Line ("  functions              " + $totalFunctions)
Add-Line ("  blocks                 " + $totalBlocks)
Add-Line ("  instructions           " + $totalInstructions)
Add-Line ("  temps                  " + $totalTemps)
Add-Line ("  temps multi-def        " + $totalTempsMulti + "  " + (Format-Pct $totalTempsMulti $totalTemps))
Add-Line ("  symbols                " + $totalSymbols)
Add-Line ("  symbols multi-def      " + $totalSymbolsMulti + "  " + (Format-Pct $totalSymbolsMulti $totalSymbols))
Add-Line ("  locals declared        " + $totalLocals)
Add-Line ("  locals address-taken   " + $totalAddressTaken + "  " + (Format-Pct $totalAddressTaken $totalLocals))
Add-Line ("  phi instructions       " + $totalPhis)
Add-Line ("")

Add-Line ("multi-def by unit")
$byUnit = $rows | Group-Object -Property Unit | ForEach-Object {
    $t = ($_.Group | Measure-Object -Property Temps -Sum).Sum
    $tm = ($_.Group | Measure-Object -Property TempsMultiDef -Sum).Sum
    $s = ($_.Group | Measure-Object -Property Symbols -Sum).Sum
    $sm = ($_.Group | Measure-Object -Property SymbolsMultiDef -Sum).Sum
    [pscustomobject]@{ Unit = $_.Name; Temps = $t; TempsMulti = $tm; Symbols = $s; SymbolsMulti = $sm }
} | Sort-Object -Property @{Expression = { $_.TempsMulti + $_.SymbolsMulti }; Descending = $true }
foreach ($u in $byUnit) {
    Add-Line ("  {0,-24} temps {1,6} multi {2,5}   symbols {3,6} multi {4,5}" -f $u.Unit, $u.Temps, $u.TempsMulti, $u.Symbols, $u.SymbolsMulti)
}
Add-Line ("")

Add-Line ("functions with multi-def temps")
$multiFns = $rows | Where-Object { $_.TempsMultiDef -gt 0 } | Sort-Object -Property TempsMultiDef -Descending
Add-Line ("  count " + $multiFns.Count)
foreach ($m in ($multiFns | Select-Object -First 40)) {
    Add-Line ("  {0,-24} {1,-28} temps {2,5} multi {3,4}" -f $m.Unit, $m.Function, $m.Temps, $m.TempsMultiDef)
}
Add-Line ("")

Add-Line ("source burn-down")
$byDir = $burndown | Group-Object -Property Dir
foreach ($g in $byDir) {
    $n = ($g.Group | Measure-Object -Property NameReads -Sum).Sum
    $c = ($g.Group | Measure-Object -Property NameCompares -Sum).Sum
    $f = ($g.Group | Measure-Object -Property ForwardScans -Sum).Sum
    Add-Line ("  {0,-22} name reads {1,6}   name compares {2,5}   forward scans {3,4}   files {4}" -f $g.Name, $n, $c, $f, $g.Group.Count)
}
Add-Line ("")
Add-Line ("source burn-down by file")
foreach ($r in ($burndown | Sort-Object -Property NameReads -Descending | Select-Object -First 60)) {
    Add-Line ("  {0,-22} {1,-44} reads {2,5}  compares {3,4}  scans {4,3}" -f $r.Dir, $r.File, $r.NameReads, $r.NameCompares, $r.ForwardScans)
}
Add-Line ("")

Add-Line ("binary hashes")
$hashRows = New-Object System.Collections.ArrayList
foreach ($f in $irFiles) {
    $obj = $f.Path -replace '\.ir$', ''
    if (Test-Path $obj) {
        $h = (Get-FileHash -Path $obj -Algorithm SHA256).Hash
        [void]$hashRows.Add([pscustomobject]@{ Name = $f.Name; Hash = $h })
    }
}
foreach ($h in ($hashRows | Sort-Object -Property Name)) {
    Add-Line ("  {0,-28} {1}" -f $h.Name, $h.Hash)
}

[System.IO.File]::WriteAllText($OutPath, $sb.ToString())
if (-not $Quiet) {
    Write-Host ("wrote " + $OutPath)
    Write-Host ("temps " + $totalTemps + " multi-def " + $totalTempsMulti + " (" + (Format-Pct $totalTempsMulti $totalTemps) + ")")
    Write-Host ("symbols " + $totalSymbols + " multi-def " + $totalSymbolsMulti + " (" + (Format-Pct $totalSymbolsMulti $totalSymbols) + ")")
    Write-Host ("locals " + $totalLocals + " address-taken " + $totalAddressTaken)
    $optRow = $byDir | Where-Object { $_.Name -eq "src/ir/optimizer" }
    if ($optRow) {
        Write-Host ("optimizer name reads " + (($optRow.Group | Measure-Object -Property NameReads -Sum).Sum))
    }
    if ($ValueIds -ne "off") {
        Write-Host ("value id disagreements " + $valueIdReports.Count)
    }
    if ($Structure -ne "off") {
        Write-Host ("structure violations " + $structureReports.Count)
        Write-Host ("multi-def raised reports " + $structureRegressions.Count)
    }
    if ($DomCrossCheck -ne "off") {
        Write-Host ("dominance functions agreeing " + $domAgreements)
        Write-Host ("dominance mismatches " + $domReports.Count)
    }
}
Remove-Item Env:\METTLE_VALUE_IDS -ErrorAction SilentlyContinue
Remove-Item Env:\METTLE_IR_STRUCT -ErrorAction SilentlyContinue
Remove-Item Env:\METTLE_DOM_CROSSCHECK -ErrorAction SilentlyContinue
exit ([int](($valueIdReports.Count + $structureReports.Count + $domReports.Count) -gt 0))
