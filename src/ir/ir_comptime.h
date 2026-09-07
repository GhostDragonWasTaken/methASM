#ifndef IR_COMPTIME_H
#define IR_COMPTIME_H

#include "ir.h"
#include "../error/error_reporter.h"

void ir_comptime_set_trace_rules(int (*hook)(void *, const char *),
                                 void *context);

int ir_comptime_run_tests(IRProgram *program, ErrorReporter *reporter,
                          const char *filename, const char *filter);

int ir_comptime_trace(IRProgram *program, ErrorReporter *reporter,
                      const char *filename, const char *source,
                      const char *function_name, const char *const *args,
                      size_t arg_count);

#endif
