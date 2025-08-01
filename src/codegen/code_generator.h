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
    int current_stack_offset;
    int function_stack_size;
    char* current_function_name;
    char* global_variables_buffer;
    size_t global_variables_size;
    size_t global_variables_capacity;
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

// Stack frame management
void code_generator_function_prologue(CodeGenerator* generator, const char* function_name, int stack_size);
void code_generator_function_epilogue(CodeGenerator* generator);
char* code_generator_generate_label(CodeGenerator* generator, const char* prefix);

// Assembly helpers
void code_generator_emit_data_section(CodeGenerator* generator);
void code_generator_emit_text_section(CodeGenerator* generator);
void code_generator_emit_global_symbol(CodeGenerator* generator, const char* symbol);
void code_generator_emit_instruction(CodeGenerator* generator, const char* mnemonic, const char* operands);

// Variable declaration helpers
void code_generator_generate_global_variable(CodeGenerator* generator, ASTNode* var_declaration);
void code_generator_generate_local_variable(CodeGenerator* generator, ASTNode* var_declaration);
void code_generator_generate_variable_initialization(CodeGenerator* generator, Symbol* symbol, ASTNode* initializer);
int code_generator_calculate_variable_size(CodeGenerator* generator, const char* type_name);
int code_generator_allocate_stack_space(CodeGenerator* generator, int size, int alignment);

// Function call helpers
void code_generator_generate_function_call(CodeGenerator* generator, ASTNode* call_expression);
void code_generator_generate_parameter_passing(CodeGenerator* generator, ASTNode** arguments, size_t argument_count);
void code_generator_generate_parameter(CodeGenerator* generator, ASTNode* argument, int param_index, Type* param_type);
void code_generator_save_caller_saved_registers(CodeGenerator* generator);
void code_generator_restore_caller_saved_registers(CodeGenerator* generator);
void code_generator_align_stack_for_call(CodeGenerator* generator, int param_count);
const char* code_generator_get_register_name(x86Register reg);

// Expression and assignment helpers
void code_generator_generate_binary_operation(CodeGenerator* generator, ASTNode* left, const char* op, ASTNode* right);
void code_generator_generate_unary_operation(CodeGenerator* generator, const char* op, ASTNode* operand);
void code_generator_generate_assignment_statement(CodeGenerator* generator, ASTNode* assignment);
void code_generator_load_variable(CodeGenerator* generator, const char* variable_name);
void code_generator_store_variable(CodeGenerator* generator, const char* variable_name, const char* source_reg);
void code_generator_load_string_literal(CodeGenerator* generator, const char* string_value);
int code_generator_get_operator_precedence(const char* op);
const char* code_generator_get_arithmetic_instruction(const char* op, int is_float);

// Struct and method helpers
void code_generator_generate_struct_declaration(CodeGenerator* generator, ASTNode* struct_declaration);
void code_generator_generate_method_call(CodeGenerator* generator, ASTNode* method_call, ASTNode* object);
void code_generator_generate_member_access(CodeGenerator* generator, ASTNode* member_access);
void code_generator_calculate_struct_layout(CodeGenerator* generator, Type* struct_type);
int code_generator_get_field_offset(CodeGenerator* generator, Type* struct_type, const char* field_name);
int code_generator_calculate_struct_alignment(int field_size);

// Inline assembly helpers
void code_generator_generate_inline_assembly(CodeGenerator* generator, ASTNode* inline_asm);
void code_generator_preserve_registers_for_inline_asm(CodeGenerator* generator);
void code_generator_restore_registers_after_inline_asm(CodeGenerator* generator);
void code_generator_emit_inline_asm_block(CodeGenerator* generator, const char* assembly_code);

#endif // CODE_GENERATOR_H