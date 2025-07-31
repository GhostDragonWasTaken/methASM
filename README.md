# methASM - Enhanced x86 Assembly Language

methASM is a transpiler that extends x86 assembly with high-level programming constructs while maintaining full backward compatibility with standard x86 assembly.

## Features

- **Simplified Variable Declarations**: `var counter: int32 = 42`
- **Function Definitions**: `function add(a: int32, b: int32) -> int32`
- **Struct Support**: Define structs with methods
- **Automatic Register Allocation**: No manual register management required
- **Type System**: Built-in types with type inference
- **Backward Compatibility**: Mix high-level syntax with standard x86 assembly

## Building

```bash
make
```

## Usage

```bash
./bin/easyasm input.masm -o output.s
```

## Project Structure

```
src/
├── lexer/          # Tokenization
├── parser/         # AST construction
├── semantic/       # Symbol tables, type checking, register allocation
├── codegen/        # x86 assembly code generation
└── main.c          # CLI interface
```

## Example Code

```masm
// Variable declarations
var counter: int32 = 0;
var message = "Hello, World!";

// Function definition
function fibonacci(n: int32) -> int32 {
    if (n <= 1) return n;
    return fibonacci(n-1) + fibonacci(n-2);
}

// Struct with methods
struct Point {
    x: float64;
    y: float64;
    
    method distance() -> float64 {
        return sqrt(this.x * this.x + this.y * this.y);
    }
}

// Mixed with inline assembly
function optimized_add(a: int32, b: int32) -> int32 {
    var result: int32;
    asm {
        mov eax, a
        add eax, b
        mov result, eax
    }
    return result;
}
```