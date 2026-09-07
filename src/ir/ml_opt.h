#ifndef ML_OPT_H
#define ML_OPT_H

#include "ir.h"

typedef struct {
  int proposals;
  int validated;
  int proven;
  int rejected;
  int skipped;
} MLOptStats;

int ir_apply_ml_opt(IRProgram *program, MLOptStats *stats);
int ir_hoist_constants(IRProgram *program);

#endif
