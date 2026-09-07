# TRAP: the cmd /c around the redirect is load-bearing. PowerShell 5.1 wraps a
# native exe's stderr in ErrorRecords stamped with the exe's own filename, so
# rewriting this as `& $exe ... 2>&1` makes two differently-named binaries
# differ on every file. See README.md.
# TRAP: both binaries must compile to the SAME -o path; the compiler prints it.
param([string]$Prev = "$env:TEMP\mettle_prev.exe", [string]$Curr = ".\bin\mettle.exe")
$out = "$env:TEMP\exdiff"
New-Item -ItemType Directory -Force -Path $out | Out-Null
$obj = Join-Path $out "x.obj"
$fa = Join-Path $out "a.txt"
$fb = Join-Path $out "b.txt"
$files = Get-ChildItem -Recurse -Path tests, examples -Filter *.mettle -ErrorAction SilentlyContinue
$same = 0; $diff = 0; $skip = 0; $names = @()
foreach ($f in $files) {
  Get-ChildItem $out -Filter *.explain.* -ErrorAction SilentlyContinue | Remove-Item -Force
  Remove-Item $fa, $fb -ErrorAction SilentlyContinue
  cmd /c "`"$Prev`" --release --explain -o `"$obj`" `"$($f.FullName)`" > nul 2>&1"
  if ($LASTEXITCODE -ne 0) { $skip++; continue }
  cmd /c "`"$Prev`" --release --explain -o `"$obj`" `"$($f.FullName)`" > `"$fa`" 2>&1"
  Get-ChildItem $out -Filter *.explain.* -ErrorAction SilentlyContinue | Remove-Item -Force
  cmd /c "`"$Curr`" --release --explain -o `"$obj`" `"$($f.FullName)`" > nul 2>&1"
  cmd /c "`"$Curr`" --release --explain -o `"$obj`" `"$($f.FullName)`" > `"$fb`" 2>&1"
  $ha = (Get-FileHash $fa -Algorithm SHA256).Hash
  $hb = (Get-FileHash $fb -Algorithm SHA256).Hash
  if ($ha -eq $hb) { $same++ } else { $diff++; $names += $f.FullName }
}
Write-Output "identical=$same differing=$diff skipped=$skip"
if ($names.Count -gt 0) { $names | Select-Object -First 20 }
