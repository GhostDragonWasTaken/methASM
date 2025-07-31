#ifndef MAIN_H
#define MAIN_H

#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/symbol_table.h"
#include "semantic/type_checker.h"
#include "semantic/register_allocator.h"
#include "codegen/code_generator.h"

typedef struct {
    const char* input_filename;
    const char* output_filename;
    int debug_mode;
    int optimize;
} CompilerOptions;

// Function declarations
int compile_file(const char* input_filename, const char* output_filename, CompilerOptions* options);
void print_usage(const char* program_name);
char* read_file(const char* filename);

#endif // MAIN_H