#ifndef MAIN_H
#define MAIN_H

#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/symbol_table.h"
#include "semantic/type_checker.h"
#include "semantic/register_allocator.h"
#include "codegen/code_generator.h"
#include "error/error_reporter.h"
#include "debug/debug_info.h"

typedef struct {
    const char* input_filename;
    const char* output_filename;
    int debug_mode;
    int optimize;
    int generate_debug_symbols;
    int generate_line_mapping;
    int generate_stack_trace_support;
    const char* debug_format; // "dwarf", "stabs", or "map"
} CompilerOptions;

// Function declarations
int compile_file(const char* input_filename, const char* output_filename, CompilerOptions* options);
void print_usage(const char* program_name);
char* read_file(const char* filename);

#endif // MAIN_H