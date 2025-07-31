#ifndef CODE_GENERATOR_H
#define CODE_GENERATOR_H

#include "../parser/ast.h"
#include "../semantic/symbol_table.h"
#include "../semantic/register_allocator.h"
#include <stdio.h>

typedef struct {
    SymbolTable* symbol_table;
    RegisterAllocator* register_allocator;
    FILE* output_file;
    char* output_buffer;
    size_t buffer_size;
    size_t buffer_capacity;
    int current_label_id;
} CodeGenerator;

// Function declarations
CodeGenerator* code_generator_create(SymbolTable* symbol_table, RegisterAllocator* allocator);
void code_generator_destroy(CodeGenerator* generator);
int code_generator_generate_program(CodeGenerator* generator, ASTNode* program);
void code_generator_generate_function(CodeGenerator* generator, ASTNode* function);
void code_generator_generate_statement(CodeGenerator* generator, ASTNode* statement);
void code_generator_generate_expression(CodeGenerator* generator, ASTNode* expression);
void code_generator_emit(CodeGenerator* generator, const char* format, ...);
char* code_generator_get_output(CodeGenerator* generator);

#endif // CODE_GENERATOR_H