#ifndef IR_TWINS_H
#define IR_TWINS_H

#include "ir.h"
#include "../error/error_reporter.h"
#include <stddef.h>
#include <stdio.h>

typedef struct {
  size_t pairs;
  size_t validated;
  size_t diverged;
  size_t gapped;
  size_t input_sets;
} IRTwinStats;

typedef struct IRTwinSnapshots IRTwinSnapshots;

int ir_program_has_twins(const IRProgram *program);
int ir_twins_check(IRProgram *program, ErrorReporter *reporter, FILE *report,
                   const char *stage, IRTwinStats *stats);

IRTwinSnapshots *ir_twins_capture(IRProgram *program);
void ir_twins_snapshots_free(IRTwinSnapshots *snapshots);
int ir_twins_recheck(IRProgram *program, const IRTwinSnapshots *snapshots,
                     ErrorReporter *reporter, FILE *report, const char *stage,
                     IRTwinStats *stats);

#endif
