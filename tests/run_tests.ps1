param(
  [string]$CompilerPath = ".\bin\methasm.exe",
  [switch]$BuildCompiler,
  [switch]$SkipRuntime
)

$ErrorActionPreference = "Stop"

function Write-CaseResult {
  param(
    [string]$Name,
    [bool]$Passed,
    [string]$Reason = ""
  )

  if ($Passed) {
    Write-Host "[PASS] $Name"
  } else {
    if ($Reason) {
      Write-Host "[FAIL] $Name :: $Reason"
    } else {
      Write-Host "[FAIL] $Name"
    }
  }
}

if ($BuildCompiler) {
  Write-Host "Building compiler..."
  & .\build.bat
  if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed."
    exit 1
  }
}

if (-not (Test-Path $CompilerPath)) {
  Write-Error "Compiler not found at '$CompilerPath'."
  exit 1
}

$tmpDir = Join-Path $env:TEMP "methasm-test-artifacts"
if (-not (Test-Path $tmpDir)) {
  New-Item -Path $tmpDir -ItemType Directory | Out-Null
}

$cases = @(
  @{ Name="ok_global_int"; Path="tests/ok_global_int.masm"; ShouldSucceed=$true },
  @{ Name="array_index"; Path="tests/test_array_index.masm"; ShouldSucceed=$true },
  @{ Name="control_flow"; Path="tests/test_control_flow.masm"; ShouldSucceed=$true },
  @{ Name="switch_const_expr"; Path="tests/test_switch_const_expr.masm"; ShouldSucceed=$true },
  @{ Name="switch_continue_loop"; Path="tests/test_switch_continue_loop.masm"; ShouldSucceed=$true },
  @{ Name="forward_decl"; Path="tests/test_forward_decl.masm"; ShouldSucceed=$true },
  @{ Name="forward_decl_pointer"; Path="tests/test_forward_decl_pointer.masm"; ShouldSucceed=$true },
  @{ Name="gc_alloc"; Path="tests/test_gc_alloc.masm"; ShouldSucceed=$true },
  @{ Name="gc_alloc_fixed"; Path="tests/test_gc_alloc_fixed.masm"; ShouldSucceed=$true },
  @{ Name="pointers"; Path="tests/test_pointers.masm"; ShouldSucceed=$true },
  @{ Name="pointer_null"; Path="tests/test_pointer_null.masm"; ShouldSucceed=$true },
  @{ Name="pointer_param_address"; Path="tests/test_pointer_param_address.masm"; ShouldSucceed=$true },

  @{ Name="err_unknown_char"; Path="tests/err_unknown_char.masm"; ShouldSucceed=$false; Pattern="Lexical error|error" },
  @{ Name="err_invalid_hex"; Path="tests/err_invalid_hex.masm"; ShouldSucceed=$false; Pattern="Invalid hexadecimal literal" },
  @{ Name="err_invalid_bin"; Path="tests/err_invalid_bin.masm"; ShouldSucceed=$false; Pattern="Invalid binary literal" },
  @{ Name="err_missing_brace"; Path="tests/err_missing_brace.masm"; ShouldSucceed=$false },
  @{ Name="err_undefined_var"; Path="tests/err_undefined_var.masm"; ShouldSucceed=$false; Pattern="Undefined variable" },
  @{ Name="err_top_level_return"; Path="tests/err_top_level_return.masm"; ShouldSucceed=$false; Pattern="Return statement outside of a function|Unsupported top-level construct in declaration context" },
  @{ Name="err_break_outside_loop"; Path="tests/err_break_outside_loop.masm"; ShouldSucceed=$false; Pattern="'break' can only be used inside a loop or switch" },
  @{ Name="err_continue_in_switch"; Path="tests/err_continue_in_switch.masm"; ShouldSucceed=$false; Pattern="'continue' can only be used inside a loop" },
  @{ Name="err_switch_duplicate_case"; Path="tests/err_switch_duplicate_case.masm"; ShouldSucceed=$false; Pattern="Duplicate case value|duplicate case" },
  @{ Name="err_switch_nonconst_case"; Path="tests/err_switch_nonconst_case.masm"; ShouldSucceed=$false; Pattern="compile-time integer constant expression" },
  @{ Name="err_forward_decl_mismatch"; Path="tests/err_forward_decl_mismatch.masm"; ShouldSucceed=$false; Pattern="does not match existing declaration" },
  @{ Name="err_forward_decl_pointer_mismatch"; Path="tests/err_forward_decl_pointer_mismatch.masm"; ShouldSucceed=$false; Pattern="does not match existing declaration" },
  @{ Name="err_deref_non_pointer"; Path="tests/err_deref_non_pointer.masm"; ShouldSucceed=$false; Pattern="Dereference operator requires a pointer operand" },
  @{ Name="err_address_of_non_lvalue"; Path="tests/err_address_of_non_lvalue.masm"; ShouldSucceed=$false; Pattern="Address-of operator requires an assignable expression" },
  @{ Name="err_pointer_type_mismatch"; Path="tests/err_pointer_type_mismatch.masm"; ShouldSucceed=$false; Pattern="Type mismatch" },
  @{ Name="err_codegen_member_expr"; Path="tests/err_codegen_member_expr.masm"; ShouldSucceed=$false }
)

$total = 0
$failed = 0

foreach ($case in $cases) {
  $total++
  $outFile = Join-Path $tmpDir ("{0}.s" -f $case.Name)
  if (Test-Path $outFile) {
    Remove-Item -Path $outFile -Force
  }

  $output = & $CompilerPath $case.Path -o $outFile 2>&1 | Out-String
  $exitCode = $LASTEXITCODE

  $passed = $true
  $reason = ""

  if ($case.ShouldSucceed) {
    if ($exitCode -ne 0) {
      $passed = $false
      $reason = "Expected success, got exit code $exitCode"
    } elseif (-not (Test-Path $outFile)) {
      $passed = $false
      $reason = "Output file not produced"
    } else {
      $asmText = Get-Content -Path $outFile -Raw
      if ($asmText -match "\%[a-z]{2,3}" -or $asmText -match "\$[0-9]+") {
        $passed = $false
        $reason = "Found AT&T-style syntax fragments in generated assembly"
      }
    }
  } else {
    if ($exitCode -eq 0) {
      $passed = $false
      $reason = "Expected failure, got success"
    } elseif ($case.ContainsKey("Pattern") -and $case.Pattern) {
      if ($output -notmatch $case.Pattern) {
        $passed = $false
        $reason = "Failure message did not match expected pattern '$($case.Pattern)'"
      }
    }
  }

  if (-not $passed) {
    $failed++
    Write-CaseResult -Name $case.Name -Passed $false -Reason $reason
    if ($output) {
      Write-Host ($output.TrimEnd())
    }
  } else {
    Write-CaseResult -Name $case.Name -Passed $true
  }
}

if (-not $SkipRuntime) {
  $total++
  try {
    $runtimeExe = "bin\gc_runtime_test.exe"
    & gcc -Wall -Wextra -std=c99 -g -O0 -D_GNU_SOURCE tests\gc_runtime_test.c src\runtime\gc.c -o $runtimeExe
    if ($LASTEXITCODE -ne 0) {
      throw "Failed to compile GC runtime test"
    }

    $runtimeOutput = & $runtimeExe 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) {
      throw "GC runtime test exited with code $LASTEXITCODE"
    }

    if ($runtimeOutput -notmatch "GC runtime tests passed") {
      throw "GC runtime test output did not contain pass marker"
    }

    Write-CaseResult -Name "gc_runtime" -Passed $true
  } catch {
    $failed++
    Write-CaseResult -Name "gc_runtime" -Passed $false -Reason $_.Exception.Message
  }
}

Write-Host ""
Write-Host "Test summary: $($total - $failed)/$total passed"

if ($failed -ne 0) {
  exit 1
}

exit 0
