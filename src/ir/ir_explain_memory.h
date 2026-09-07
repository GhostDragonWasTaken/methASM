#ifndef IR_EXPLAIN_MEMORY_H
#define IR_EXPLAIN_MEMORY_H

#include <stddef.h>

void ir_explain_memory_set_collect(int enabled, const char *focus_file);

void ir_explain_memory_note(const char *file, int severity, size_t line,
                            const char *code, const char *headline,
                            const char *fix);

#endif
