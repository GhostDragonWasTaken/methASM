# MethASM

MethASM is a compiler for a typed assembly-like language that targets x86-64 assembly.

## Project Status

The project currently has a working front end and code generation pipeline:

- Lexing and parsing for core language constructs
- AST generation and semantic checks
- x86-64 assembly output generation
- Runtime garbage collector integration for `new` allocations

This is an active development codebase. Several parts are implemented and tested, and several parts are still incomplete.

## Implemented Features

### Language

- Variable declarations with explicit types and initializer-based type inference
  - `var x: int32 = 42;`
  - `var y = new Vector3;`
- Function declarations and calls
  - Return type syntax supports both `->` and `:`
- Struct declarations
- Struct field access and field assignment
  - `obj.field`
  - `obj.field = expr`
- `if` / `else` and `while`
- `return`
- Inline assembly blocks

### Type and Semantic Analysis

- Built-in integer and floating types
- Struct type registration and lookup
- Assignment compatibility checks
- Field assignment semantic checks

### Code Generation

- x86-64 assembly emission
- Function prologue and epilogue generation
- Expression and statement code generation for implemented AST nodes
- Struct field offset based access and assignment
- Process entry generation (`_start`) that calls `main` when present

### Garbage Collector Runtime

- Heap allocation via `gc_alloc`
- Conservative mark-and-sweep collection
- Stack scanning roots
- Explicit root registration API
  - `gc_register_root`
  - `gc_unregister_root`
- Auto-collection threshold controls
  - `gc_set_collection_threshold`
  - `gc_get_collection_threshold`
- Runtime cleanup
  - `gc_shutdown`

## Build

### Windows

```powershell
.\build.bat
```

### Linux or macOS

```bash
make
```

Output binary:

- Windows: `bin\methasm.exe`
- Linux or macOS: `bin/methasm`

## Usage

```bash
methasm [options] <input.masm>
```

Common options:

- `-i <file>`: input file
- `-o <file>`: output assembly file
- `-d`, `--debug`: enable debug mode
- `-O`, `--optimize`: enable optimization flag (framework exists, passes are limited)
- `-h`, `--help`: show help

Example:

```bash
./bin/methasm tests/test_gc_alloc.masm -o out.s
```

## Tests

Runtime GC test:

```bash
make test
```

On Windows, equivalent manual command:

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

- Optimization framework exists, but optimization passes are limited.
- The language and runtime are still evolving; syntax and behavior may change.
- Coverage is strongest for currently used test scenarios and core language paths.

## Contributing

Contributions are welcome. Useful contribution areas include:

- Additional language features
- Better diagnostics and error recovery
- More test coverage
- Code generation correctness and performance
- Runtime and GC improvements
