#ifndef MONOMORPHIZE_H
#define MONOMORPHIZE_H

#include "../error/error_reporter.h"
#include "../parser/ast.h"

#define MONO_PTR_RECEIVER_SUFFIX "__ptr"

int monomorphize_program(ASTNode *program, ErrorReporter *reporter);

int closure_convert_program(ASTNode *program, ErrorReporter *reporter);

int closure_adapt_program(ASTNode *program, ErrorReporter *reporter);

#endif
