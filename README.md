# MethASM

MethASM is a compiler for a typed assembly-like language that targets x86-64 assembly.

## Project Status

MethASM provides an end-to-end compilation pipeline from `.masm` source to x86-64 assembly output.

- Lexing
- Parsing
- Semantic and type analysis
- Code generation
- Runtime garbage collector integration

Core compiler and runtime paths are implemented and currently validated by project tests.

Current implementation status:

- Core language front-end and backend pipeline are operational.
- Fail-fast diagnostics are enforced across lexer, parser, semantic analysis, and codegen.
- Arrays, pointers, structured types, and major control-flow constructs are implemented.
- Runtime GC integration is compiled and exercised by runtime tests.

## Compiler Guarantees

The compiler uses fail-fast behavior across all major phases.

- Lexical errors report precise line and column and stop compilation.
- Parser recovery is used for diagnostics, but any parser error still causes final failure.
- Semantic errors include source locations and block backend execution.
- Code generation rejects unresolved symbols and unsupported constructs instead of emitting fallback output.
- Unsupported top-level constructs are rejected explicitly.

This reduces silent failures and minimizes incorrect cascading diagnostics.

## Implemented Language Features

- Variable declarations with explicit types and initializer-based inference
- Pointer type annotations (e.g. `int32*`, `Pair*`, `int32**`)
- Dereference (`*p`) and address-of (`&x`) operators
- Pointer-to-struct arrow notation (`p->field`)
- Fixed-size array type annotations (for example `int32[10]`)
- Functions and function calls
- Function forward declarations (e.g. `function add(a: int32, b: int32) -> int32;`)
- Function return type syntax with both `->` and `:`
- Struct declarations
- Struct member access and assignment
- Struct methods and method calls (`obj.method(args)`)
- Array and pointer indexing (`arr[i]`, `ptr[i]`) and indexed assignment
- `if` and `else`
- `while`
- `for`
- `switch`, `case`, `default`
- `break` and `continue`
- `return`
- Inline assembly blocks

## Type and Semantic Analysis

- Built-in integer and floating-point types
- Pointer type resolution (multi-level `T*`, `T**`, etc.) and base-type inference
- Null pointer constant handling (`0` as valid pointer initializer and in comparisons)
- Fixed-size array type resolution and element type inference
- Struct type registration and lookup
- Method call validation
- Pointer type compatibility (same base type, or pointer vs null)
- Assignment compatibility validation
- Field assignment validation
- Array index expression validation (index type and target type checks)
- Loop/switch context checks for `break` and `continue`
- Switch expression validation and compile-time case constant evaluation
- Duplicate `case` detection and default-clause uniqueness checks
- Undefined symbol detection with source locations
- Forward declaration signature compatibility checks in symbol resolution

## Code Generation

- x86-64 assembly emission (Intel/NASM-style syntax)
- Function prologue and epilogue generation
- Statement and expression generation for supported AST nodes
- Struct field offset-based access and assignment
- Method call emission (mangled names, `this` as first parameter)
- Pointer dereference and address-of code generation
- Array and pointer element address calculation and typed indexed load/store emission
- Code generation for `if`, `while`, `for`, and `switch` control flow
- Nested control-flow label management for `break` and `continue`
- `_start` entry emission that calls `main` when present
- Hard failure on unresolved symbols or unsupported generation paths

## Garbage Collector Runtime

- Heap allocation via `gc_alloc`
- Conservative mark-and-sweep collection
- Iterative mark traversal using a worklist
- Stack root scanning
- Root registration API: `gc_register_root`, `gc_unregister_root` (pointer globals auto-registered)
- Collection controls: `gc_collect`, `gc_collect_now`, `gc_set_collection_threshold`, `gc_get_collection_threshold`
- Runtime cleanup: `gc_shutdown`

## Build

### Windows

```powershell
.\build.bat
```

### Linux and macOS

```bash
make
```

Build output:

- Windows: `bin\methasm.exe`
- Linux and macOS: `bin/methasm`

## Usage

```bash
methasm [options] <input.masm>
```

Options:

- `-i <file>` input file
- `-o <file>` output file
- `-d`, `--debug` enable debug mode
- `-O`, `--optimize` enable optimization flag
- `-h`, `--help` print usage

Example:

```bash
./bin/methasm tests/test_gc_alloc.masm -o out.s
```

## Testing

Run runtime and compiler tests with:

```bash
make test
```

Windows manual runtime test:

```powershell
gcc -Wall -Wextra -std=c99 -g -O0 -D_GNU_SOURCE tests\gc_runtime_test.c src\runtime\gc.c -o bin\gc_runtime_test.exe
.\bin\gc_runtime_test.exe
```

Windows compiler smoke tests:

```powershell
.\bin\methasm.exe test_simple.masm -o out_simple.s
.\bin\methasm.exe tests\test_gc_alloc.masm -o out_gc.s
.\bin\methasm.exe tests\test_array_index.masm -o out_array.s
.\bin\methasm.exe tests\test_control_flow.masm -o out_control_flow.s
.\bin\methasm.exe tests\test_switch_const_expr.masm -o out_switch_const_expr.s
.\bin\methasm.exe tests\test_switch_continue_loop.masm -o out_switch_continue_loop.s
.\bin\methasm.exe tests\test_pointers.masm -o out_pointers.s
.\bin\methasm.exe tests\test_pointer_null.masm -o out_pointer_null.s
.\bin\methasm.exe tests\test_forward_decl_pointer.masm -o out_forward_decl_pointer.s
```

## Repository Layout

```text
src/
  lexer/      Tokenization
  parser/     Parsing and AST creation
  semantic/   Type checking, symbol table, register allocation
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
- Some advanced language and backend scenarios are not implemented.
- `switch` case labels currently require compile-time integer constant expressions and do not yet support range-style cases.

## Contributing

Contributions should include:

- A clear problem statement
- Tests that validate behavior changes
- Notes on compatibility impact
