#include "ir_loop_shape.h"

int ir_loop_shape_at(IRFunction *function, size_t header_index,
                     IRLoopShape *shape) {
  IRInstruction *header = NULL;
  IRInstruction *compare = NULL;
  IRInstruction *branch = NULL;
  size_t compare_index = 0;
  size_t branch_index = 0;

  if (!function || !shape ||
      header_index + 4 >= function->instruction_count) {
    return 0;
  }
  header = &function->instructions[header_index];
  if (header->op != IR_OP_LABEL || !ir_label_is_while_header(header->text)) {
    return 0;
  }
  if (!ir_find_next_non_nop(function, header_index + 1, &compare_index) ||
      !ir_find_next_non_nop(function, compare_index + 1, &branch_index)) {
    return 0;
  }
  compare = &function->instructions[compare_index];
  branch = &function->instructions[branch_index];
  if (compare->op != IR_OP_BINARY || compare->is_float || !compare->text ||
      compare->lhs.kind != IR_OPERAND_SYMBOL || !compare->lhs.name ||
      compare->dest.kind != IR_OPERAND_TEMP || !compare->dest.name ||
      branch->op != IR_OP_BRANCH_ZERO ||
      !ir_operand_is_temp_named(&branch->lhs, compare->dest.name)) {
    return 0;
  }
  shape->header = header;
  shape->compare = compare;
  shape->branch = branch;
  shape->compare_index = compare_index;
  shape->branch_index = branch_index;
  shape->loop_label = header->text;
  shape->iv_symbol = compare->lhs.name;
  return 1;
}
