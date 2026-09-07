#ifndef MTLC_LOWER_MODULE_H
#define MTLC_LOWER_MODULE_H

#include "ir/ir.h"
#include "parser/ast.h"
#include "semantic/symbol_table.h"
#include "semantic/type_checker.h"

void mtlc_lower_populate_module(IRProgram *program, ASTNode *ast_program,
                                TypeChecker *tc, SymbolTable *st);

#endif
