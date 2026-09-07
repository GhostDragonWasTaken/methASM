#ifndef IR_LOOP_SHAPE_H
#define IR_LOOP_SHAPE_H

#include "ir_optimize_internal.h"

typedef struct {
  IRInstruction *header;
  IRInstruction *compare;
  IRInstruction *branch;
  size_t compare_index;
  size_t branch_index;
  const char *loop_label;
  const char *iv_symbol;
} IRLoopShape;

int ir_loop_shape_at(IRFunction *function, size_t header_index,
                     IRLoopShape *shape);

#endif
