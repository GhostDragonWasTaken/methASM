# MethASM

**Enhanced x86 Assembly Language — Type-safe, high-level syntax that compiles to standard x86-64 assembly.**

MethASM is a transpiler that extends x86 assembly with modern programming constructs—typed variables, functions, structs, and inline assembly—while remaining fully compatible with standard x86. Write cleaner, safer low-level code; emit assembly that works with NASM, MASM, or GAS.

---

## Table of Contents

- [Features](#features)
- [Quick Start](#quick-start)
- [Language Overview](#language-overview)
- [Building](#building)
- [Usage](#usage)
- [Project Structure](#project-structure)
- [Development Status](#development-status)
- [Contributing](#contributing)

---

## Features

| Capability | Description |
|------------|-------------|
| **Type Safety** | Compile-time type checking (int8–64, uint8–64, float32/64, string, void) |
| **Variables** | `var x: int32 = 42;` with type inference and initialization |
| **Functions** | `function add(a: int32, b: int32) -> int32` with proper calling conventions |
| **Structs** | Struct definitions with fields and methods |
| **Inline Assembly** | Raw x86 assembly blocks with register preservation |
| **Debug Support** | DWARF/STABS symbols, source mapping, stack trace generation |
| **Register Allocation** | Automatic register management and stack allocation |

---

## Quick Start

```bash
# Build the compiler
make                    # Linux/macOS
build.bat               # Windows

# Compile a MethASM source file
./bin/methasm program.masm -o program.s

# Assemble and link
gcc program.s -o program

# Run
./program
```

---

## Language Overview

### Traditional x86 vs MethASM

**Traditional x86 Assembly:**
```nasm
section .data
    counter dd 42
    message db "Hello, World!", 0

section .text
global _start

_start:
    mov eax, [counter]
    add eax, 10
    mov [counter], eax
    
    push dword 5
    push dword 3
    call add_numbers
    add esp, 8
    
    mov eax, 1
    int 0x80

add_numbers:
    push ebp
    mov ebp, esp
    mov eax, [ebp+8]
    add eax, [ebp+12]
    pop ebp
    ret
```

**MethASM Equivalent:**
```masm
var counter: int32 = 42;
var message = "Hello, World!";

function add_numbers(a: int32, b: int32) -> int32 {
    return a + b;
}

function optimized_add(a: int32, b: int32) -> int32 {
    var result: int32;
    asm {
        mov eax, a
        add eax, b
        mov result, eax
    }
    return result;
}

struct Point {
    x: float64;
    y: float64;
    
    method distance() -> float64 {
        return sqrt(this.x * this.x + this.y * this.y);
    }
}
```

### Why MethASM?

1. **Type Safety** — Catch mismatches and misuse at compile time
2. **Automatic Register Management** — No manual register tracking
3. **Higher-Level Abstractions** — Functions, structs, and control flow without boilerplate
4. **Backward Compatibility** — Mix high-level syntax with raw x86 assembly
5. **Readability** — Clearer syntax for complex operations
6. **Error Prevention** — Reduces common assembly programming mistakes

---

## Building

### Prerequisites

- **C Compiler:** GCC (Linux/macOS) or MinGW-w64 / MSYS2 (Windows)
- **Build System:** Make (Linux/macOS) or use `build.bat` (Windows)

### Build Commands

| Platform | Command | Output |
|----------|---------|--------|
| Linux/macOS | `make` | `bin/methasm` |
| Linux/macOS (debug) | `make debug` | Build with `-DDEBUG` |
| Linux/macOS (install) | `sudo make install` | Installs to `/usr/local/bin` |
| Windows | `build.bat` | `bin\methasm.exe` |
| Any (clean) | `make clean` | Removes `obj/` and `bin/` |

---

## Usage

MethASM compiles `.masm` source files to x86-64 assembly (GAS syntax by default).

### Command Line

```bash
methasm [options] <input.masm>
```

| Option | Description |
|--------|-------------|
| `-i <file>` | Input file (alternative to positional argument) |
| `-o <file>` | Output file (default: `output.s`) |
| `-d`, `--debug` | Enable debug output and symbols |
| `-g`, `--debug-symbols` | Generate debug symbols only |
| `-l`, `--line-mapping` | Generate source line mapping |
| `-s`, `--stack-trace` | Generate stack trace support |
| `--debug-format <fmt>` | Debug format: `dwarf`, `stabs`, or `map` (default: dwarf) |
| `-O`, `--optimize` | Enable optimizations |
| `-h`, `--help` | Show usage information |

### Debug Output Formats

- **DWARF** — Industry-standard, GDB-compatible
- **STABS** — Alternative format for older systems
- **Map** — Human-readable symbol/location map

### Generated Debug Files

When debug features are enabled:

| File | Description |
|------|-------------|
| `output.s.debug` | DWARF debug information |
| `output.s.stabs` | STABS debug information |
| `output.s.map` | Human-readable debug map |
| `output.s.stacktrace.s` | Runtime stack trace code |

For detailed debug documentation, see [DEBUG_FUNCTIONALITY.md](DEBUG_FUNCTIONALITY.md).

---

## Project Structure

```
MethASM/
├── src/
│   ├── lexer/          # Tokenization and lexical analysis
│   ├── parser/         # AST construction and syntax parsing
│   ├── semantic/       # Symbol tables, type checking, register allocation
│   ├── codegen/        # x86 assembly code generation
│   ├── debug/          # Debug symbol generation and source mapping
│   ├── error/          # Error reporting and handling
│   └── main.c          # CLI and compilation pipeline
├── bin/                # Compiled binary and example sources
├── Makefile            # Linux/macOS build
├── build.bat           # Windows build
└── README.md
```

### Compilation Pipeline

```
Source (.masm) → Lexer → Parser → AST → Type Checker → Code Generator → x86 Assembly (.s)
                                    ↘
                              Symbol Table
                              Register Allocator
```

---

## Development Status

### Implemented

- Full compilation pipeline (lex → parse → type-check → codegen)
- Variables, functions, structs with methods
- **Control flow**: `if` / `else` and `while` (parsing + codegen)
- **Method calls**: `obj.method(args)` with name mangling
- **Struct field assignment**: `obj.field = expr`
- Inline assembly with register preservation
- Type system (int8–64, uint8–64, float32/64, string, void)
- Symbol table with hierarchical scopes
- Register allocation and stack management
- Debug symbols (DWARF, STABS, map), line mapping, stack traces
- CLI with file I/O and debug/optimization options

### In Progress

- Optimization passes (framework present; no concrete passes yet)

### Planned

- Advanced control flow (for, switch, break/continue)
- Virtual methods and inheritance
- Standard library (I/O, strings, common routines)
- Direct object file generation
- Cross-platform calling conventions (Windows vs System V)

---

## Contributing

MethASM is an educational project demonstrating compiler construction. Contributions are welcome, especially for:

- Additional language features
- Improved error messages and diagnostics
- Performance optimizations
- Documentation and examples

---

*MethASM — bringing modern ergonomics to x86 assembly.*
