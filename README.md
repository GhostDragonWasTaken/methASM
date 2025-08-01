# MethASM - Enhanced x86 Assembly Language

MethASM is a transpiler that extends x86 assembly with high-level programming constructs while maintaining full backward compatibility with standard x86 assembly. It provides a more developer-friendly syntax while generating standard x86 assembly code that can be assembled with traditional tools like NASM, MASM, or GAS.

## Current Status

**Development Phase**: Core compiler fully implemented with working end-to-end compilation

### ✅ Fully Implemented Features
- **Complete Compilation Pipeline**: Source code → lexing → parsing → type checking → code generation → x86 assembly
- **Lexer**: Full tokenization with support for enhanced syntax, x86 mnemonics, and position tracking
- **Parser**: Complete AST construction for all language constructs with error recovery
- **Variable Declarations**: `var counter: int32 = 42;` with type inference and initialization
- **Function Definitions**: `function add(a: int32, b: int32) -> int32` with parameter handling and return types
- **Struct Declarations**: Complete struct syntax with fields and method definitions
- **Inline Assembly**: Seamless integration of raw x86 assembly with register preservation
- **Type System**: Full built-in type support (int8-64, uint8-64, float32/64, string, void)
- **Symbol Table**: Hierarchical scope management (global, function, block scopes)
- **Type Checking**: Complete type validation, inference, and error reporting
- **Register Allocation**: Automatic register management with stack allocation
- **Code Generation**: Full x86-64 assembly output with function prologues/epilogues
- **CLI Interface**: Working command-line tool with file I/O and options
- **Build System**: Complete Makefile and Windows batch build scripts

### ✅ Tested and Working
- **Inline Assembly Pass-through**: Comprehensive test suite for assembly code preservation
- **Variable Declarations**: Global and local variable handling
- **Function Generation**: Function prologue/epilogue generation with proper calling conventions
- **Expression Evaluation**: Arithmetic operations and literal handling
- **Memory Management**: Stack frame allocation and variable storage

### 🚧 Partially Implemented
- **Control Flow Statements**: if/while statement parsing complete, code generation in progress
- **Method Calls**: Struct method syntax parsed, runtime dispatch implementation needed
- **Optimizations**: Basic optimization framework exists, specific optimizations pending
- **Advanced Expressions**: Complex expression evaluation and operator precedence

### 📋 Planned Features
- **Advanced Control Flow**: for loops, switch statements, break/continue
- **Complete Method System**: Virtual method calls and inheritance
- **Standard Library**: Common functions, string operations, and I/O routines
- **Linker Integration**: Direct object file generation without intermediate assembly
- **Debug Information**: Source mapping and debugging symbol generation
- **Cross-Platform**: Support for different calling conventions (Windows/System V)

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
- **C Compiler**: GCC (Linux/macOS) or MinGW/MSYS2 (Windows)
- **Build System**: Make (included with most Unix systems and MSYS2)
- **Operating System**: Windows, Linux, or macOS

### Build Commands

#### Linux/macOS
```bash
# Build the compiler
make

# Build with debug information
make debug

# Clean build artifacts
make clean

# Install to system (optional)
sudo make install
```

#### Windows
```batch
# Build using the batch script (recommended)
build.bat

# Or using make if MSYS2/MinGW is installed
make
```

The compiled binary will be created as:
- **Windows**: `bin/methasm.exe`
- **Linux/macOS**: `bin/methasm`

## Usage

The MethASM compiler takes `.masm` source files and generates standard x86-64 assembly output.

### Command Line Interface
```bash
# Show help and usage information
./bin/methasm --help

# Basic compilation (generates output.s by default)
./bin/methasm input.masm

# Specify output file
./bin/methasm input.masm -o output.s

# Enable debug output
./bin/methasm input.masm -o output.s --debug

# Enable optimizations
./bin/methasm input.masm -o output.s --optimize

# Combine options
./bin/methasm input.masm -o optimized_output.s --debug --optimize
```

### Complete Compilation Workflow
```bash
# 1. Compile MethASM source to assembly
./bin/methasm program.masm -o program.s

# 2. Assemble and link with system assembler
gcc program.s -o program

# 3. Run the executable
./program
```

### Windows Usage
```batch
# Basic compilation
bin\methasm.exe input.masm -o output.s

# With options
bin\methasm.exe input.masm -o output.s -d -O
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
