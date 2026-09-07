#ifndef IR_DEADLINE_H
#define IR_DEADLINE_H

#include "ir.h"
#include "../error/error_reporter.h"
#include <stdio.h>

typedef struct {
  long long op;
  long long load;
  long long store;
  long long branch;
  long long multiply;
  long long multiply_float;
  long long divide;
  long long divide_float;
  long long call;
  long long allocate;
  int described;
} IRDeadlineCosts;

typedef struct {
  size_t declared;
  size_t proven;
  size_t on_evidence;
  long long worst_slack;
  const char *worst_function;
} IRDeadlineStats;

int ir_deadline_run(IRProgram *program, ErrorReporter *reporter,
                    const IRDeadlineCosts *costs, int instrument,
                    int instrumented, FILE *report, IRDeadlineStats *stats);

#endif
