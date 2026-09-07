# TRAP: the cmd /c around the redirect is load-bearing. PowerShell 5.1 wraps a
# native exe's stderr in ErrorRecords stamped with the exe's own filename, so
# rewriting this as `& $exe ... 2>&1` makes two differently-named binaries
# differ on every file. See README.md.
# TRAP: both binaries must compile to the SAME -o path; the compiler prints it.
param([string]$Prev = "$env:TEMP\mettle_prev.exe", [string]$Curr = ".\bin\mettle.exe")
$out = "$env:TEMP\objdiff"
New-Item -ItemType Directory -Force -Path $out | Out-Null
$files = Get-ChildItem -Recurse -Path tests, examples -Filter *.mettle -ErrorAction SilentlyContinue
$same = 0; $diff = 0; $skip = 0
$diffs = @()
foreach ($f in $files) {
  $a = Join-Path $out "a.obj"; $b = Join-Path $out "b.obj"
  Remove-Item $a, $b -ErrorAction SilentlyContinue
  & $Prev --release -o $a $f.FullName *> $null
  if ($LASTEXITCODE -ne 0 -or -not (Test-Path $a)) { $skip++; continue }
  & $Curr --release -o $b $f.FullName *> $null
  if ($LASTEXITCODE -ne 0 -or -not (Test-Path $b)) { $diff++; $diffs += "$($f.Name) [new failed]"; continue }
  $ha = (Get-FileHash $a -Algorithm SHA256).Hash
  $hb = (Get-FileHash $b -Algorithm SHA256).Hash
  if ($ha -eq $hb) { $same++ } else { $diff++; $diffs += $f.Name }
}
Write-Output "identical=$same differing=$diff skipped=$skip"
if ($diffs.Count -gt 0) { $diffs | Select-Object -First 25 }
