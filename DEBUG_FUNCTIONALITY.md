# Debug Functionality Implementation

## Overview

Task 8.3 "Create debugging support" has been successfully implemented. The debug system provides comprehensive debugging capabilities for the x86 Assembly Fork compiler, including debug symbol generation, source line mapping, and stack trace support.

## Features Implemented

### 1. Debug Symbol Generation

The system generates debug symbols for:
- **Variables**: Global and local variables with type information and memory locations
- **Functions**: Function definitions with parameter and return type information  
- **Parameters**: Function parameters with type and location information
- **Struct Fields**: Structure field definitions with offset information

Debug symbols include:
- Symbol name and type
- Source location (line and column)
- Memory location (register or stack offset)
- Size information
- Type information

### 2. Source Line Mapping

The system maintains mappings between:
- Source code line numbers and assembly line numbers
- Source file names and generated assembly
- Column information for precise error reporting

Line mappings enable:
- Debugger integration for step-through debugging
- Accurate error reporting with source locations
- Source-level debugging of generated assembly

### 3. Stack Trace Generation

The system provides runtime stack trace capabilities:
- **Stack Frame Walking**: Automatic stack frame traversal
- **Function Name Resolution**: Maps addresses to function names
- **Source Location Mapping**: Shows source file and line numbers
- **Runtime Stack Trace Code**: Generates assembly code for runtime stack traces

## Debug Output Formats

### DWARF Debug Information
- Industry-standard debug format
- Compatible with GDB and other debuggers
- Includes compilation unit information
- Provides variable location information
- Supports line number mapping

### STABS Debug Information  
- Alternative debug format for older systems
- Simpler format than DWARF
- Compatible with legacy debuggers
- Includes symbol type information

### Debug Map Format
- Human-readable debug information
- Easy to parse for custom tools
- Shows symbol locations and mappings
- Useful for debugging the compiler itself

## Integration with Code Generator

The debug system is fully integrated with the code generator:

### Symbol Tracking
- Variables are automatically registered in debug info during code generation
- Function definitions create debug symbols with proper type information
- Stack offsets and register assignments are tracked

### Line Mapping
- Each generated statement includes source line mapping
- Debug labels are emitted for debugger breakpoints
- Assembly line numbers are tracked automatically

### Memory Location Tracking
- Stack-allocated variables include offset information
- Register-allocated variables include register names
- Global variables include memory addresses

## Command Line Interface

The compiler supports several debug-related options:

```bash
# Enable all debug features
./methasm input.masm -d

# Generate only debug symbols
./methasm input.masm -g

# Generate line mapping
./methasm input.masm -l

# Generate stack trace support
./methasm input.masm -s

# Specify debug format
./methasm input.masm -g --debug-format dwarf
./methasm input.masm -g --debug-format stabs
./methasm input.masm -g --debug-format map
```

## Generated Files

When debug features are enabled, the compiler generates:

- `output.s.debug` - DWARF debug information
- `output.s.stabs` - STABS debug information  
- `output.s.map` - Human-readable debug map
- `output.s.stacktrace.s` - Runtime stack trace code

## Usage Examples

### Basic Debug Compilation
```bash
./methasm program.masm -d -o program.s
```

### Custom Debug Format
```bash
./methasm program.masm --debug-symbols --debug-format dwarf -o program.s
```

### Stack Trace Support
```bash
./methasm program.masm --stack-trace -o program.s
```

## Testing

The debug functionality has been tested with:

1. **Unit Tests**: Individual debug functions tested in isolation
2. **Integration Tests**: Full compilation pipeline with debug enabled
3. **Output Verification**: Generated debug files validated for correctness
4. **Stack Trace Tests**: Runtime stack trace functionality verified
## Technical Details

### Data Structures
- `DebugInfo`: Main container for all debug information
- `DebugSymbol`: Individual symbol information
- `SourceLineMapping`: Source-to-assembly line mappings
- `StackTrace`: Runtime stack trace representation

### File Organization
- `src/debug/debug_info.h` - Debug system interface
- `src/debug/debug_info.c` - Debug system implementation
- Integration in `src/codegen/code_generator.c`
- Command line support in `src/main.c`

### Memory Management
- Proper allocation and deallocation of debug structures
- String duplication for symbol names and types
- Dynamic array management for symbols and mappings

## Future Enhancements

Potential improvements for the debug system:
- DWARF version 5 support
- Optimized debug information compression
- Source-level variable inspection
- Advanced breakpoint support
- Debug information validation tools