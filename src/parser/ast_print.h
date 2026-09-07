#ifndef AST_PRINT_H
#define AST_PRINT_H

#include "ast.h"
#include <stdio.h>

typedef const char *(*AstPrintAnnotator)(void *context, const ASTNode *block);

size_t ast_print_program(FILE *out, const ASTNode *program,
                         AstPrintAnnotator annotate, void *context);

#endif
