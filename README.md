# MethASM - Enhanced x86 Assembly Language

MethASM is a transpiler that extends x86 assembly with high-level programming constructs while maintaining full backward compatibility with standard x86 assembly. It provides a more developer-friendly syntax while generating standard x86 assembly code that can be assembled with traditional tools like NASM, MASM, or GAS.

## Current Status

**Development Phase**: Core infrastructure implemented, basic features working

### ✅ Implemented Features
- **Lexer**: Complete tokenization with support for enhanced syntax and x86 mnemonics
- **Parser**: AST construction for high-level constructs
- **Variable Declarations**: `var counter: int32 = 42;` with type inference
- **Function Definitions**: `function add(a: int32, b: int32) -> int32`
- **Struct Declarations**: Basic struct syntax with fields and methods
- **Inline Assembly**: Seamless integration of raw x86 assembly within high-level code
- **Type System**: Built-in types (int8-64, uint8-64, float32/64, string)
- **Symbol Table**: Variable and function scope management
- **Type Checking**: Basic type validation and inference
- **Register Allocation**: Automatic register management framework
- **Code Generation**: x86 assembly output generation

### 🚧 In Progress / Partially Implemented
- **Control Flow**: Basic if/while statements (syntax parsed, codegen in progress)
- **Method Calls**: Struct method invocation (syntax supported)
- **Optimizations**: Basic optimization framework exists
- **Error Handling**: Improved error reporting and recovery

### 📋 Planned Features
- **Advanced Control Flow**: for loops, switch statements
- **Standard Library**: Common functions and macros
- **Linker Integration**: Direct object file generation
- **Debug Information**: Source mapping and debugging support
- **Cross-Platform**: Support for different x86 calling conventions

## How It Compares to Standard x86 Assembly

### Traditional x86 Assembly
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
    
    ; Function call
    push dword 5
    push dword 3
    call add_numbers
    add esp, 8
    
    ; Exit
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

### MethASM Equivalent
```masm
// Variable declarations with type safety
var counter: int32 = 42;
var message = "Hello, World!";

// High-level function definition
function add_numbers(a: int32, b: int32) -> int32 {
    return a + b;
}

// Mixed with inline assembly when needed
function optimized_add(a: int32, b: int32) -> int32 {
    var result: int32;
    asm {
        mov eax, a
        add eax, b
        mov result, eax
    }
    return result;
}

// Struct with methods
struct Point {
    x: float64;
    y: float64;
    
    method distance() -> float64 {
        return sqrt(this.x * this.x + this.y * this.y);
    }
}
```

### Key Advantages

1. **Type Safety**: Compile-time type checking prevents common assembly errors
2. **Automatic Register Management**: No need to manually track register usage
3. **Higher-Level Abstractions**: Functions, structs, and control flow without boilerplate
4. **Backward Compatibility**: Mix high-level syntax with raw x86 assembly
5. **Better Readability**: More intuitive syntax for complex operations
6. **Error Prevention**: Catches many common assembly programming mistakes

## Building

### Prerequisites
- GCC compiler
- Make build system

### Build Commands
```bash
# Build the compiler
make

# Build with debug information
make debug

# Clean build artifacts
make clean
```

## Usage

```bash
# Basic compilation
./bin/methasm input.masm -o output.s

# With debug output
./bin/methasm input.masm -o output.s -d

# With optimizations
./bin/methasm input.masm -o output.s -O
```

## Project Structure

```
src/
├── lexer/          # Tokenization and lexical analysis
├── parser/         # AST construction and syntax parsing
├── semantic/       # Symbol tables, type checking, register allocation
├── codegen/        # x86 assembly code generation
└── main.c          # CLI interface and compilation pipeline
```

## Example Code

```masm
// Simple variable declarations
var counter: int32 = 0;
var message = "Hello, World!";
var pi: float64 = 3.14159;

// Function with type safety
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

// Mixed high-level and assembly
function optimized_string_copy(src: string, dest: string) -> void {
    asm {
        mov esi, src
        mov edi, dest
        mov ecx, 0
    copy_loop:
        mov al, [esi + ecx]
        mov [edi + ecx], al
        inc ecx
        cmp al, 0
        jne copy_loop
    }
}
```

## Development

The project uses a modular architecture with clear separation between lexical analysis, parsing, semantic analysis, and code generation. Each component can be tested independently, and the codebase is designed for extensibility.

### Testing
Individual components can be tested using the provided test files:
- `test_lexer.c` - Lexical analysis tests
- `test_parser_core.c` - Parser functionality tests
- `test_ast.c` - Abstract syntax tree tests

## Contributing

This is an educational project demonstrating compiler construction techniques. Contributions are welcome, especially for:
- Additional language features
- Improved error handling
- Performance optimizations
- Documentation and examples
