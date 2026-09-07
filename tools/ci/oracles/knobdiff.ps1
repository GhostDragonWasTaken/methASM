# TRAP: the cmd /c around the redirect is load-bearing. PowerShell 5.1 wraps a
# native exe's stderr in ErrorRecords stamped with the exe's own filename, so
# rewriting this as `& $exe ... 2>&1` makes two differently-named binaries
# differ on every file. See README.md.
# TRAP: both binaries must compile to the SAME -o path; the compiler prints it.
#
# Sweeps the optimizer knob modes the suite uses (METTLE_SKIP_PASS) and the
# debug/release axis, comparing object bytes between two compilers in each.
# This is the axis a plain -O object diff does not cover: see README.md, "an
# oracle is only as broad as the flag matrix you run it under".
param(
  [string]$Prev = "$env:TEMP\mettle_base57.exe",
  [string]$Curr = ".\bin\mettle.exe",
  [string[]]$Paths = @("tests"),
  [int]$MaxLines = 20000,
  [string[]]$Modes = @("release", "debug", "no_vec:auto_vectorize_int",
                       "no_accum:if_convert_accumulate",
                       "no_hoist:hoist_global_bases",
                       "no_scan:scan_from_first")
)
$out = "$env:TEMP\knobdiff"
if (Test-Path $out) { Remove-Item $out -Recurse -Force }
New-Item -ItemType Directory -Force -Path $out | Out-Null
$obj = Join-Path $out "o.obj"
$log = Join-Path $out "o.log"

$files = Get-ChildItem -Recurse -Path $Paths -Filter *.mettle -ErrorAction SilentlyContinue |
  Where-Object { (Get-Content $_.FullName -ReadCount 0).Count -le $MaxLines }
Write-Output "corpus: $($files.Count) files across $($Modes.Count) modes"

function Compile([string]$exe, [string]$src, [string]$relFlag, [string]$keep) {
  Remove-Item $obj, $log -ErrorAction SilentlyContinue
  cmd /c "`"$exe`" $relFlag -o `"$obj`" `"$src`" > `"$log`" 2>&1"
  $code = $LASTEXITCODE
  if (Test-Path $obj) { Copy-Item $obj $keep -Force }
  return $code
}

$grand = 0
foreach ($mode in $Modes) {
  $name, $pass = $mode -split ":", 2
  $relFlag = if ($name -eq "debug") { "" } else { "--release" }
  if ($pass) { $env:METTLE_SKIP_PASS = $pass } else { Remove-Item Env:\METTLE_SKIP_PASS -ErrorAction SilentlyContinue }

  $same = 0; $diff = 0; $skip = 0; $names = @()
  foreach ($f in $files) {
    $a = "$out\a.obj"; $b = "$out\b.obj"
    Remove-Item $a, $b -ErrorAction SilentlyContinue
    if ((Compile $Prev $f.FullName $relFlag $a) -ne 0 -or -not (Test-Path $a)) { $skip++; continue }
    Compile $Curr $f.FullName $relFlag $b | Out-Null
    if (-not (Test-Path $b)) { $diff++; $names += "$($f.Name) [no output]"; continue }
    if ((Get-FileHash $a).Hash -eq (Get-FileHash $b).Hash) { $same++ }
    else { $diff++; $names += $f.Name }
  }
  Remove-Item Env:\METTLE_SKIP_PASS -ErrorAction SilentlyContinue
  Write-Output ("[{0,-9}] identical={1} differing={2} skipped={3}" -f $name, $same, $diff, $skip)
  if ($names.Count -gt 0) { $names | Select-Object -First 10 | ForEach-Object { "    $_" } }
  $grand += $diff
}
Write-Output "TOTAL DIFFERING ACROSS ALL MODES: $grand"
