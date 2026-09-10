#ifndef IR_VERIFY_STRUCTURE_H
#define IR_VERIFY_STRUCTURE_H

#include <stddef.h>

typedef struct {
  size_t temps;
  size_t temps_multi_def;
  size_t symbols;
  size_t symbols_multi_def;
  size_t duplicate_labels;
  size_t unresolved_targets;
  size_t missing_destinations;
  size_t unnumbered_values;
  size_t phis;
  size_t phi_arity_errors;
  size_t phis_outside_block_head;
} IRStructureReport;

int ir_structure_enabled(void);
size_t ir_structure_violation_count(void);
size_t ir_structure_regression_count(void);

#endif
