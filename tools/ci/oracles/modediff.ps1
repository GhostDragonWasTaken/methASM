# TRAP: the cmd /c around the redirect is load-bearing. PowerShell 5.1 wraps a
# native exe's stderr in ErrorRecords stamped with the exe's own filename, so
# rewriting this as `& $exe ... 2>&1` makes two differently-named binaries
# differ on every file. See README.md.
# TRAP: both binaries must compile to the SAME -o path; the compiler prints it.
param(
  [string]$Prev = "$env:TEMP\mettle_prev.exe",
  [string]$Curr = ".\bin\mettle.exe",
  [string[]]$ExtraArgs = @(),
  [string]$Tag = "mode",
  [string[]]$Purge = @()
)
$out = "$env:TEMP\modediff-$Tag"
if (Test-Path $out) { Remove-Item $out -Recurse -Force }
New-Item -ItemType Directory -Force -Path $out | Out-Null
$obj = Join-Path $out "o.out"
$log = Join-Path $out "o.log"
$files = Get-ChildItem -Recurse -Path tests, examples -Filter *.mettle -ErrorAction SilentlyContinue
$same = 0; $diff = 0; $skip = 0; $names = @()
$argline = $ExtraArgs -join " "

function Invoke-One([string]$exe, [string]$src, [string]$keepObj, [string]$keepLog) {
  foreach ($p in $Purge) {
    Get-ChildItem $out -Filter $p -ErrorAction SilentlyContinue | Remove-Item -Force
  }
  Remove-Item $obj, $log -ErrorAction SilentlyContinue
  cmd /c "`"$exe`" $argline -o `"$obj`" `"$src`" > `"$log`" 2>&1"
  $code = $LASTEXITCODE
  if (Test-Path $obj) { Copy-Item $obj $keepObj -Force }
  if (Test-Path $log) { Copy-Item $log $keepLog -Force }
  return $code
}

foreach ($f in $files) {
  $ao = Join-Path $out "a.keep"; $bo = Join-Path $out "b.keep"
  $al = Join-Path $out "a.klog"; $bl = Join-Path $out "b.klog"
  Remove-Item $ao, $bo, $al, $bl -ErrorAction SilentlyContinue
  $code = Invoke-One $Prev $f.FullName $ao $al
  if ($code -ne 0 -or -not (Test-Path $ao)) { $skip++; continue }
  Invoke-One $Curr $f.FullName $bo $bl | Out-Null
  if (-not (Test-Path $bo)) { $diff++; $names += "$($f.Name) [new produced nothing]"; continue }
  $ha = (Get-FileHash $ao -Algorithm SHA256).Hash
  $hb = (Get-FileHash $bo -Algorithm SHA256).Hash
  $ta = if (Test-Path $al) { (Get-FileHash $al -Algorithm SHA256).Hash } else { "" }
  $tb = if (Test-Path $bl) { (Get-FileHash $bl -Algorithm SHA256).Hash } else { "" }
  if ($ha -eq $hb -and $ta -eq $tb) { $same++ }
  elseif ($ha -ne $hb) { $diff++; $names += "$($f.Name) [output]" }
  else { $diff++; $names += "$($f.Name) [log]" }
}
Write-Output "[$Tag] identical=$same differing=$diff skipped=$skip"
if ($names.Count -gt 0) { $names | Select-Object -First 20 }
