# MethASM

MethASM is a compiler for a typed assembly-like language that targets x86-64 assembly.

## Status

MethASM currently provides an end-to-end pipeline from `.masm` source to x86-64 assembly output:

- Lexing
- Parsing
- Semantic and type analysis
- Code generation
- Runtime garbage collector integration

The project is under active development. Core features are implemented, tested, and usable for current language scenarios.

## Current Guarantees

The compiler now uses strict fail-fast behavior across all major phases.

- Lexical errors are reported with exact line and column and stop compilation.
- Parser recovery is used for diagnostics, but compilation still fails if any parser error occurred.
- Semantic errors are reported with location and suggestions where available.
- Code generation no longer silently falls back on unsupported or unresolved constructs.
- Unsupported top-level constructs are rejected explicitly.

This design is intended to reduce both false negatives (missed errors) and false positives (incorrect phase fallout).

## Implemented Language Features

- Variables with explicit types and initializer-based inference
  - `var x: int32 = 42;`
  - `var y = new Vector3;`
- Functions and function calls
  - Return type syntax supports both `->` and `:`
- Struct declarations
- Struct member access and assignment
  - `obj.field`
  - `obj.field = expr`
- `if` / `else`
- `while`
- `return`
- Inline assembly blocks

## Type and Semantic Analysis

- Built-in integer and floating-point types
- Struct type registration and lookup
- Assignment compatibility checks
- Field assignment checks
- Undefined symbol detection with source location

## Code Generation

- x86-64 assembly emission
- Function prologue and epilogue generation
- Expression and statement generation for supported AST nodes
- Struct field offset based access and assignment
- `_start` entry emission that calls `main` when present
- Hard error on unresolved symbols or unsupported generation paths

## Garbage Collector Runtime

- Heap allocation via `gc_alloc`
- Conservative mark-and-sweep collection
- Iterative mark traversal (worklist-based)
- Stack root scanning
- Explicit root registration API
  - `gc_register_root`
  - `gc_unregister_root`
- Collection control
  - `gc_collect`
  - `gc_collect_now`
  - `gc_set_collection_threshold`
  - `gc_get_collection_threshold`
- Runtime shutdown and cleanup
  - `gc_shutdown`

## Build

### Windows

```powershell
.\build.bat
```

### Linux/macOS

```bash
make
```

Output binary:

- Windows: `bin\methasm.exe`
- Linux/macOS: `bin/methasm`

## Usage

```bash
methasm [options] <input.masm>
```

Common options:

- `-i <file>` input file
- `-o <file>` output file
- `-d`, `--debug` enable debug mode
- `-O`, `--optimize` enable optimization flag
- `-h`, `--help` print usage

Example:

```bash
./bin/methasm tests/test_gc_alloc.masm -o out.s
```

## Tests

Runtime GC test:

```bash
make test
```

Windows manual equivalent:

```powershell
gcc -Wall -Wextra -std=c99 -g -O0 -D_GNU_SOURCE tests\gc_runtime_test.c src\runtime\gc.c -o bin\gc_runtime_test.exe
.\bin\gc_runtime_test.exe
```

Compiler smoke tests:

```powershell
.\bin\methasm.exe test_simple.masm -o out_simple.s
.\bin\methasm.exe tests\test_gc_alloc.masm -o out_gc.s
```

## Repository Layout

```text
src/
  lexer/      Tokenization
  parser/     Parsing and AST creation
  semantic/   Type checking, symbol table, register allocator
  codegen/    Assembly generation
  runtime/    Garbage collector runtime
  debug/      Debug information helpers
  error/      Error reporting
  main.c      CLI entry and compilation flow

tests/        Language and runtime tests
build.bat     Windows build
Makefile      Linux/macOS build and test targets
```

## Known Limitations

- Optimization passes are limited.
- Language surface area is still evolving.
- Some advanced language and backend scenarios are not implemented yet.

## Contributing

Contributions are welcome in:

- Language features
- Diagnostics and recovery quality
- Test coverage
- Code generation correctness and performance
- Runtime and garbage collector improvements
