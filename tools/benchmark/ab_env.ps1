param(
    [string]$Compiler = "bin\mettle.exe",
    [string[]]$Benchmark = @(),
    [string]$EnvName = "METTLE_IR_SSA",
    [string]$EnvValue = "1",
    [int]$Runs = 7,
    [int]$AffinityMask = 0x10
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $Root

$CompilerPath = [System.IO.Path]::GetFullPath((Join-Path $Root $Compiler))
if (-not (Test-Path $CompilerPath)) { throw "compiler not found: $CompilerPath" }

$cfgRaw = [System.IO.File]::ReadAllText((Join-Path $Root "docs\benchmarks\harness.json"))
$cfg = $cfgRaw.TrimStart([char]0xFEFF) | ConvertFrom-Json
$all = @($cfg.benchmarks | Where-Object { $_.kind -eq "runtime" })

$want = @()
foreach ($b in $Benchmark) { $want += ($b -split ",") | ForEach-Object { $_.Trim() } }
$want = @($want | Where-Object { $_ })
if ($want.Count -gt 0) { $all = @($all | Where-Object { $want -contains $_.name }) }
if ($all.Count -eq 0) { throw "no benchmarks selected" }

$work = Join-Path $env:TEMP ("mettle_abenv_" + [System.Diagnostics.Process]::GetCurrentProcess().Id)
if (-not (Test-Path $work)) { New-Item -ItemType Directory -Path $work -Force | Out-Null }

function Build-Arm {
    param([string]$Source, [string]$Output, [hashtable]$Extra)
    foreach ($k in $Extra.Keys) { Set-Item -Path "Env:\$k" -Value $Extra[$k] }
    $argList = @("--build", "--release", ('"' + $Source + '"'), "-o", ('"' + $Output + '"'))
    $p = Start-Process -FilePath $CompilerPath -ArgumentList $argList -NoNewWindow -Wait -PassThru `
                       -RedirectStandardOutput (Join-Path $work "b.out") `
                       -RedirectStandardError (Join-Path $work "b.err")
    foreach ($k in $Extra.Keys) { Remove-Item -Path "Env:\$k" -ErrorAction SilentlyContinue }
    return $p.ExitCode -eq 0
}

function Time-Run {
    param([string]$Exe)
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $Exe
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $proc = [System.Diagnostics.Process]::Start($psi)
    try { $proc.ProcessorAffinity = [IntPtr]$AffinityMask } catch {}
    try { $proc.PriorityClass = [System.Diagnostics.ProcessPriorityClass]::High } catch {}
    $null = $proc.StandardOutput.ReadToEnd()
    $null = $proc.StandardError.ReadToEnd()
    $proc.WaitForExit()
    $sw.Stop()
    return $sw.Elapsed.TotalMilliseconds
}

$rows = New-Object System.Collections.ArrayList
foreach ($b in $all) {
    $source = Join-Path $Root $b.mettle_source
    if (-not (Test-Path $source)) { continue }
    $baseExe = Join-Path $work ($b.name + "_base.exe")
    $newExe = Join-Path $work ($b.name + "_new.exe")
    if (-not (Build-Arm $source $baseExe @{})) { Write-Host ("skip " + $b.name + " (base build failed)"); continue }
    if (-not (Build-Arm $source $newExe @{ $EnvName = $EnvValue })) { Write-Host ("skip " + $b.name + " (new build failed)"); continue }

    $baseHash = (Get-FileHash $baseExe -Algorithm SHA256).Hash
    $newHash = (Get-FileHash $newExe -Algorithm SHA256).Hash

    $baseBest = [double]::MaxValue
    $newBest = [double]::MaxValue
    for ($i = 0; $i -lt $Runs; $i++) {
        if ($i % 2 -eq 0) {
            $t = Time-Run $baseExe; if ($t -lt $baseBest) { $baseBest = $t }
            $t = Time-Run $newExe;  if ($t -lt $newBest)  { $newBest = $t }
        } else {
            $t = Time-Run $newExe;  if ($t -lt $newBest)  { $newBest = $t }
            $t = Time-Run $baseExe; if ($t -lt $baseBest) { $baseBest = $t }
        }
    }
    $ratio = if ($baseBest -gt 0) { $newBest / $baseBest } else { 0 }
    [void]$rows.Add([pscustomobject]@{
        Name = $b.name
        Base = [math]::Round($baseBest, 2)
        New = [math]::Round($newBest, 2)
        Ratio = [math]::Round($ratio, 4)
        Identical = ($baseHash -eq $newHash)
    })
    Write-Host ("{0,-16} base {1,9:F2} ms  new {2,9:F2} ms  ratio {3,7:F4}{4}" -f $b.name, $baseBest, $newBest, $ratio, $(if ($baseHash -eq $newHash) { "  identical" } else { "" }))
}

if ($rows.Count -gt 0) {
    $logSum = 0.0
    foreach ($r in $rows) { if ($r.Ratio -gt 0) { $logSum += [math]::Log($r.Ratio) } }
    $geo = [math]::Exp($logSum / $rows.Count)
    $identical = ($rows | Where-Object { $_.Identical }).Count
    Write-Host ""
    Write-Host ("benchmarks {0}  byte-identical {1}  geomean ratio {2:F4}" -f $rows.Count, $identical, $geo)
}
