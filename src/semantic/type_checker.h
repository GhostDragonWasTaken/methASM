#ifndef TYPE_CHECKER_H
#define TYPE_CHECKER_H

#include "symbol_table.h"
#include "../parser/ast.h"

typedef struct {
    SymbolTable* symbol_table;
    int has_error;
    char* error_message;
} TypeChecker;

// Function declarations
TypeChecker* type_checker_create(SymbolTable* symbol_table);
void type_checker_destroy(TypeChecker* checker);
int type_checker_check_program(TypeChecker* checker, ASTNode* program);
Type* type_checker_infer_type(TypeChecker* checker, ASTNode* expression);
int type_checker_are_compatible(Type* type1, Type* type2);

#endif // TYPE_CHECKER_H