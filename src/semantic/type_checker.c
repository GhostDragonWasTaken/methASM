#include "type_checker.h"
#include <stdlib.h>

TypeChecker* type_checker_create(SymbolTable* symbol_table) {
    TypeChecker* checker = malloc(sizeof(TypeChecker));
    if (!checker) return NULL;
    
    checker->symbol_table = symbol_table;
    checker->has_error = 0;
    checker->error_message = NULL;
    
    return checker;
}

void type_checker_destroy(TypeChecker* checker) {
    if (checker) {
        free(checker->error_message);
        free(checker);
    }
}

int type_checker_check_program(TypeChecker* checker, ASTNode* program) {
    // Stub implementation - always succeeds for now
    return 1;
}

Type* type_checker_infer_type(TypeChecker* checker, ASTNode* expression) {
    // Stub implementation
    return NULL;
}

int type_checker_are_compatible(Type* type1, Type* type2) {
    // Stub implementation
    return 1;
}