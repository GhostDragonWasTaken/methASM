# Compilation

This document describes how to compile MethASM programs and the available compiler options.

## Compiler Usage

```bash
methasm [options] <input.masm>
```

The input file is the main source file. Imports are resolved relative to it. The compiler produces assembly (default `output.s`).

## Options

`-o <file>` output assembly file (default `output.s`). `-i <file>` input file (alternative to positional argument). `-I <dir>` add import search directory (repeatable). `--stdlib <dir>` set stdlib root (default `stdlib`). `--prelude` auto-import `std/prelude`. `-d`/`--debug` debug mode. `-g`/`--debug-symbols` generate debug symbols. `-l`/`--line-mapping` source line mapping. `-O`/`--optimize` enable optimizations. `-h`/`--help` print usage.

## Build Pipeline

1. Compile: `methasm main.masm -o main.s`
2. Assemble: `nasm -f win64 main.s -o main.o` (or `-f elf64` on Linux)
3. Link: `gcc main.o gc.o -o main` (plus libraries such as `-lws2_32` for networking)

The output format depends on the target. Use `-f win64` for Windows, `-f elf64` for Linux. NASM is required for assembly; install from https://www.nasm.us/ if needed. On Linux and macOS, use `make` to build the compiler and run tests.

## Web Server Example

The `web/` directory contains a complete HTTP server example. Build and run:

```bash
cd web
.\build.bat
.\server.exe
```

Then open http://localhost:5000 in a browser.

## Testing

The test suite compiles and runs a set of programs. Run:

```powershell
.\tests\run_tests.ps1
.\tests\run_tests.ps1 -BuildCompiler
.\tests\run_tests.ps1 -SkipRuntime
```

`-BuildCompiler` rebuilds the compiler before running. `-SkipRuntime` skips the GC runtime executable test.
