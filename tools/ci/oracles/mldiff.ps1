# TRAP: the cmd /c around the redirect is load-bearing. PowerShell 5.1 wraps a
# native exe's stderr in ErrorRecords stamped with the exe's own filename, so
# rewriting this as `& $exe ... 2>&1` makes two differently-named binaries
# differ on every file. See README.md.
# TRAP: both binaries must compile to the SAME -o path; the compiler prints it.
param(
  [string]$Prev = "$env:TEMP\mettle_prev.exe",
  [string]$Curr = ".\bin\mettle.exe",
  [switch]$Wide,
  [int]$MaxLines = 20000
)
$out = "$env:TEMP\mldiff"
if (Test-Path $out) { Remove-Item $out -Recurse -Force }
New-Item -ItemType Directory -Force -Path $out | Out-Null
$obj = Join-Path $out "o.obj"
$act = Join-Path $out "o.act"
$log = Join-Path $out "o.log"

if ($Wide) {
  $env:METTLE_ML_AFFINE = "1"
  $env:METTLE_ML_PTR = "1"
  $env:METTLE_ML_SPECULATIVE = "1"
}

$files = Get-ChildItem -Recurse -Path tests, examples -Filter *.mettle -ErrorAction SilentlyContinue |
  Where-Object { (Get-Content $_.FullName -ReadCount 0).Count -le $MaxLines }
$same = 0; $diff = 0; $skip = 0; $acts = 0; $names = @()

function Run-One([string]$exe, [string]$src, [string]$keepObj, [string]$keepAct, [string]$keepLog) {
  Remove-Item $obj, $act, $log -ErrorAction SilentlyContinue
  $env:METTLE_ML_ACTIONS = $act
  cmd /c "`"$exe`" --release --ml-opt -o `"$obj`" `"$src`" > `"$log`" 2>&1"
  $code = $LASTEXITCODE
  $env:METTLE_ML_ACTIONS = $null
  if (Test-Path $obj) { Copy-Item $obj $keepObj -Force }
  if (Test-Path $act) { Copy-Item $act $keepAct -Force }
  if (Test-Path $log) { Copy-Item $log $keepLog -Force }
  return $code
}

function Hash-Or([string]$p) {
  if (Test-Path $p) { return (Get-FileHash $p -Algorithm SHA256).Hash }
  return "-"
}

foreach ($f in $files) {
  $ao = "$out\a.obj"; $bo = "$out\b.obj"
  $aa = "$out\a.act"; $ba = "$out\b.act"
  $al = "$out\a.log"; $bl = "$out\b.log"
  Remove-Item $ao, $bo, $aa, $ba, $al, $bl -ErrorAction SilentlyContinue
  $code = Run-One $Prev $f.FullName $ao $aa $al
  if ($code -ne 0 -or -not (Test-Path $ao)) { $skip++; continue }
  Run-One $Curr $f.FullName $bo $ba $bl | Out-Null
  $ok = ((Hash-Or $ao) -eq (Hash-Or $bo)) -and ((Hash-Or $aa) -eq (Hash-Or $ba)) `
        -and ((Hash-Or $al) -eq (Hash-Or $bl))
  if (Test-Path $aa) { $acts++ }
  if ($ok) { $same++ }
  else {
    $diff++
    $why = @()
    if ((Hash-Or $ao) -ne (Hash-Or $bo)) { $why += "obj" }
    if ((Hash-Or $aa) -ne (Hash-Or $ba)) { $why += "actions" }
    if ((Hash-Or $al) -ne (Hash-Or $bl)) { $why += "log" }
    $names += "$($f.Name) [$($why -join ',')]"
  }
}
$env:METTLE_ML_AFFINE = $null; $env:METTLE_ML_PTR = $null
$env:METTLE_ML_SPECULATIVE = $null
Write-Output "[ml-opt wide=$Wide] identical=$same differing=$diff skipped=$skip withActionDump=$acts"
if ($names.Count -gt 0) { $names | Select-Object -First 25 }
