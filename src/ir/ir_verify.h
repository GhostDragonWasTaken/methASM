#ifndef IR_VERIFY_H
#define IR_VERIFY_H

#include "ir.h"

void ir_verify_set_enabled(int enabled);
int ir_verify_enabled(void);

void ir_verify_begin_program(IRProgram *program);
void ir_verify_end_program(void);

int ir_verify_pass_quarantined(const IRFunction *function,
                               const char *pass_name);

typedef struct IRVerifySnapshot IRVerifySnapshot;

IRVerifySnapshot *ir_verify_snapshot_take(IRFunction *function);
void ir_verify_snapshot_free(IRVerifySnapshot *snapshot);

void ir_verify_maybe_sabotage(IRFunction *function, const char *pass_name,
                              int *changed);

int ir_verify_check_pass(IRFunction *function, IRVerifySnapshot *snapshot,
                         const char *pass_name, int *changed);

int ir_verify_divergence_count(void);

int ir_verify_input_run_count(void);

int ir_verify_last_input_run_count(void);

typedef enum {
  IR_VERIFY_REWRITE_VALIDATED,
  IR_VERIFY_REWRITE_DIVERGED,
  IR_VERIFY_REWRITE_UNVERIFIABLE
} IRVerifyRewriteVerdict;

IRVerifySnapshot *ir_verify_snapshot_capture(IRFunction *function);
int ir_verify_snapshot_restore(IRFunction *function,
                               const IRVerifySnapshot *snapshot);

IRVerifyRewriteVerdict ir_verify_check_rewrite(
    IRProgram *program, IRFunction *function, const IRVerifySnapshot *snapshot,
    char *why, size_t why_capacity, char *counterexample, size_t cex_capacity,
    char *skip_reason, size_t skip_capacity);

IRVerifyRewriteVerdict ir_verify_check_rewrite_guarded(
    IRProgram *program, IRFunction *function, const IRVerifySnapshot *snapshot,
    IRFunction *guard, int *guard_hits, char *why, size_t why_capacity,
    char *counterexample, size_t cex_capacity, char *skip_reason,
    size_t skip_capacity);

IRVerifyRewriteVerdict ir_verify_check_rewrite_probed(
    IRProgram *program, IRFunction *function, const IRVerifySnapshot *snapshot,
    IRFunction *guard, int *guard_hits, const long long *probes,
    int probe_count, char *why, size_t why_capacity, char *counterexample,
    size_t cex_capacity, char *skip_reason, size_t skip_capacity);

#endif
