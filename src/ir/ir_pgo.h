#ifndef IR_PGO_H
#define IR_PGO_H

#include "ir.h"

int ir_pgo_profile_program(IRProgram *program);

int ir_pgo_enabled(void);
void ir_pgo_reset(void);

long long ir_pgo_callee_calls(const char *name);

long long ir_pgo_function_body_steps(const char *name);

long long ir_pgo_site_count(const char *function_name,
                            SourceLocation location);

long long ir_pgo_hot_threshold(void);

int ir_pgo_function_is_hot(const char *name);

int ir_pgo_load_profile(const char *path, IRProgram *program);

long long ir_pgo_max_block_count(void);

void ir_pgo_print_summary(void);

#endif
