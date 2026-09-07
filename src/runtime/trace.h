#ifndef METTLE_RUNTIME_TRACE_H
#define METTLE_RUNTIME_TRACE_H

#include <stdint.h>

void mettle_trace_enter(const char *name, const char *file, int64_t line,
                        int64_t column);
void mettle_trace_leave(void);
void mettle_trace_event(const char *kind, const char *name, const char *file,
                        int64_t line, int64_t column, int64_t value);
void mettle_trace_flush(void);

#endif
