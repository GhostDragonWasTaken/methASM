#include "ir_optimize_internal.h"
#include "ir_loop_shape.h"

static int ir_fill_value_operand(const IRFunction *function, size_t begin,
                                 size_t end, const IROperand *value,
                                 long long size, IROperand *out) {
  if (value->kind == IR_OPERAND_INT) {
    *out = ir_operand_int(value->int_value);
    return 1;
  }
  if (value->kind == IR_OPERAND_FLOAT) {
    if (size == 4) {
      float f = (float)value->float_value;
      unsigned int bits;
      memcpy(&bits, &f, sizeof(bits));
      *out = ir_operand_int((long long)bits);
      return 1;
    }
    if (size == 8) {
      double d = value->float_value;
      unsigned long long bits;
      memcpy(&bits, &d, sizeof(bits));
      *out = ir_operand_int((long long)bits);
      return 1;
    }
    return 0;
  }
  if (value->kind != IR_OPERAND_SYMBOL || !value->name) {
    return 0;
  }
  if (ir_symbol_address_taken(function, value->name)) {
    return 0;
  }
  for (size_t i = begin; i < end; i++) {
    const IRInstruction *ins = &function->instructions[i];
    if (ir_instruction_writes_destination(ins) &&
        ins->dest.kind == IR_OPERAND_SYMBOL && ins->dest.name &&
        strcmp(ins->dest.name, value->name) == 0) {
      return 0;
    }
  }
  *out = ir_operand_symbol(value->name);
  return out->name != NULL;
}

static int ir_fill_symbol_is_invariant(const IRFunction *function,
                                       size_t begin, size_t end,
                                       const char *name) {
  if (!name) {
    return 0;
  }
  if (!ir_function_symbol_is_parameter(function, name) &&
      !ir_function_local_declared_type(function, name) &&
      ir_symbol_address_taken(function, name)) {
    return 0;
  }
  for (size_t i = begin; i < end; i++) {
    const IRInstruction *ins = &function->instructions[i];
    if (ir_instruction_writes_destination(ins) &&
        ins->dest.kind == IR_OPERAND_SYMBOL && ins->dest.name &&
        strcmp(ins->dest.name, name) == 0) {
      return 0;
    }
  }
  return 1;
}

static int ir_fill_local_has_type(const IRFunction *function, const char *name,
                                  const char *type) {
  const char *declared = ir_function_local_declared_type(function, name);
  return declared && strcmp(declared, type) == 0;
}

static int ir_fill_install(IRFunction *function, size_t header_index,
                           size_t jump_index, int mode, long long size,
                           const IROperand *lhs_sym, const IROperand *rhs_op,
                           const IROperand *value, const IROperand *start_op,
                           const IROperand *offset_op,
                           const IRInstruction *offset_producer,
                           const char *final_assign_to,
                           const char *final_assign_from, int *changed) {
  IRInstruction fused = {0};
  fused.op = IR_OP_SIMD_FILL;
  fused.location = function->instructions[header_index].location;
  if (!ir_operand_clone(lhs_sym, &fused.lhs) ||
      !ir_operand_clone(rhs_op, &fused.rhs)) {
    ir_instruction_destroy_storage(&fused);
    return 0;
  }
  fused.arguments = calloc(5, sizeof(IROperand));
  if (!fused.arguments) {
    ir_instruction_destroy_storage(&fused);
    return 0;
  }
  fused.argument_count = 5;
  fused.arguments[0] = ir_operand_int(size);
  fused.arguments[1] = ir_operand_int(mode);
  if (!ir_operand_clone(value, &fused.arguments[2])) {
    ir_instruction_destroy_storage(&fused);
    return 0;
  }
  if (start_op) {
    if (!ir_operand_clone(start_op, &fused.arguments[3])) {
      ir_instruction_destroy_storage(&fused);
      return 0;
    }
  } else {
    fused.arguments[3] = ir_operand_int(0);
  }
  if (offset_op) {
    if (!ir_operand_clone(offset_op, &fused.arguments[4])) {
      ir_instruction_destroy_storage(&fused);
      return 0;
    }
  } else {
    fused.arguments[4] = ir_operand_int(0);
  }

  IRInstruction producer_clone = {0};
  if (offset_producer) {
    if (!ir_clone_instruction_plain(offset_producer, &producer_clone)) {
      ir_instruction_destroy_storage(&fused);
      return 0;
    }
  }

  IRInstruction final_assign = {0};
  int want_final_assign =
      final_assign_to && final_assign_from && header_index + 2 <= jump_index;
  if (want_final_assign) {
    final_assign.op = IR_OP_ASSIGN;
    final_assign.location = fused.location;
    final_assign.dest = ir_operand_symbol(final_assign_to);
    final_assign.lhs = ir_operand_symbol(final_assign_from);
    if (!final_assign.dest.name || !final_assign.lhs.name) {
      ir_instruction_destroy_storage(&final_assign);
      ir_instruction_destroy_storage(&producer_clone);
      ir_instruction_destroy_storage(&fused);
      return 0;
    }
  }

  ir_instruction_destroy_storage(&function->instructions[header_index]);
  memset(&function->instructions[header_index], 0,
         sizeof(function->instructions[header_index]));
  for (size_t i = header_index + 1; i <= jump_index; i++) {
    ir_instruction_make_nop(&function->instructions[i]);
  }
  if (offset_producer) {
    function->instructions[header_index] = producer_clone;
    function->instructions[header_index + 1] = fused;
  } else {
    function->instructions[header_index] = fused;
  }

  if (want_final_assign) {
    function->instructions[header_index + (offset_producer ? 2 : 1)] =
        final_assign;
  }

  if (changed) {
    *changed = 1;
  }
  return 1;
}

static int ir_fill_frame(IRFunction *function, size_t header_index,
                         size_t *compare_out, size_t *branch_out,
                         size_t *jump_out, int *inclusive, int *matched) {
  *matched = 0;
  *inclusive = 0;
  IRLoopShape loop;
  if (!ir_loop_shape_at(function, header_index, &loop)) {
    return 1;
  }
  if (!loop.branch->text) {
    return 1;
  }
  if (strcmp(loop.compare->text, "<=") == 0) {
    if (loop.compare->rhs.kind != IR_OPERAND_INT ||
        loop.compare->rhs.int_value < 0 ||
        loop.compare->rhs.int_value >= (1LL << 30)) {
      return 1;
    }
    *inclusive = 1;
  } else if (strcmp(loop.compare->text, "<") != 0) {
    return 1;
  }
  IRInstruction *header = loop.header;
  IRInstruction *branch = loop.branch;
  size_t compare_index = loop.compare_index;
  size_t branch_index = loop.branch_index;
  size_t jump_index = (size_t)-1;
  for (size_t i = branch_index + 1; i < function->instruction_count; i++) {
    if (function->instructions[i].op == IR_OP_JUMP &&
        function->instructions[i].text &&
        strcmp(function->instructions[i].text, header->text) == 0) {
      jump_index = i;
      break;
    }
    if (function->instructions[i].op == IR_OP_LABEL &&
        function->instructions[i].text &&
        strcmp(function->instructions[i].text, branch->text) == 0) {
      break;
    }
  }
  if (jump_index == (size_t)-1 ||
      ir_loop_body_is_unclaimable(function, branch_index + 1, jump_index)) {
    return 1;
  }
  if (!ir_fused_loop_exit_is_adjacent(function, jump_index, branch->text)) {
    return 1;
  }
  *compare_out = compare_index;
  *branch_out = branch_index;
  *jump_out = jump_index;
  *matched = 1;
  return 1;
}

#define FILL_NO_MATCH (-1)

static int ir_fill_try_pointer_walk(IRFunction *function, size_t header_index,
                                    size_t branch_index, size_t jump_index,
                                    const IRInstruction *compare,
                                    const IRInstruction *const *body,
                                    size_t body_count, int *changed) {
  if (compare->rhs.kind != IR_OPERAND_SYMBOL || !compare->rhs.name) {
    return FILL_NO_MATCH;
  }
  const char *p = compare->lhs.name;
  const char *pend = compare->rhs.name;
  const IRInstruction *store = NULL;
  const IRInstruction *advance = NULL;
  const char *dead_counter = NULL;
  for (size_t k = 0; k < body_count; k++) {
    const IRInstruction *ins = body[k];
    if (ins->op == IR_OP_STORE && !store &&
        ir_operand_is_symbol_named(&ins->dest, p) &&
        ins->rhs.kind == IR_OPERAND_INT) {
      store = ins;
    } else if (ins->op == IR_OP_BINARY && ins->text &&
               strcmp(ins->text, "+") == 0 && !ins->is_float &&
               ins->dest.kind == IR_OPERAND_SYMBOL && ins->dest.name &&
               strcmp(ins->dest.name, p) == 0 &&
               ir_operand_is_symbol_named(&ins->lhs, p) &&
               ins->rhs.kind == IR_OPERAND_INT && !advance) {
      advance = ins;
    } else if (ins->op == IR_OP_BINARY && ins->text &&
               strcmp(ins->text, "+") == 0 && !ins->is_float &&
               ins->dest.kind == IR_OPERAND_SYMBOL && ins->dest.name &&
               strcmp(ins->dest.name, p) != 0 && !dead_counter &&
               ir_operand_is_symbol_named(&ins->lhs, ins->dest.name) &&
               ins->rhs.kind == IR_OPERAND_INT) {
      dead_counter = ins->dest.name;
    } else {
      return FILL_NO_MATCH;
    }
  }
  if (!store || !advance) {
    return FILL_NO_MATCH;
  }
  long long size = store->rhs.int_value;
  if ((size != 1 && size != 2 && size != 4 && size != 8) ||
      advance->rhs.int_value != size ||
      !ir_fill_symbol_is_invariant(function, branch_index + 1, jump_index,
                                   pend) ||
      (dead_counter &&
       ir_symbol_live_after_loop(function, jump_index + 1, dead_counter))) {
    return FILL_NO_MATCH;
  }
  IROperand value = {0};
  if (!ir_fill_value_operand(function, branch_index + 1, jump_index,
                             &store->lhs, size, &value)) {
    return FILL_NO_MATCH;
  }
  int p_live_after = ir_symbol_live_after_loop(function, jump_index + 1, p);
  int ok = ir_fill_install(function, header_index, jump_index, 1,
                            size, &compare->lhs, &compare->rhs, &value, NULL,
                            NULL, NULL, p_live_after ? p : NULL,
                            p_live_after ? pend : NULL, changed);
  ir_operand_destroy(&value);
  return ok;
}

static int ir_fill_dest_is_store_address(const IRInstruction *const *body,
                                         size_t body_count,
                                         const IRInstruction *ins) {
  if (ins->dest.kind != IR_OPERAND_TEMP || !ins->dest.name) {
    return 0;
  }
  for (size_t k = 0; k < body_count; k++) {
    const IRInstruction *other = body[k];
    if (other->op == IR_OP_STORE &&
        ir_operand_is_temp_named(&other->dest, ins->dest.name)) {
      return 1;
    }
  }
  return 0;
}

static int ir_fill_try_indexed(IRFunction *function, size_t header_index,
                               size_t branch_index, size_t jump_index,
                               const IRInstruction *compare,
                               const IRInstruction *const *body,
                               size_t body_count, int inclusive,
                               int *changed) {
  const char *iv = compare->lhs.name;
  IROperand count = {0};
  const IROperand *count_op = &compare->rhs;
  if (inclusive) {
    if (compare->rhs.kind != IR_OPERAND_INT) {
      return FILL_NO_MATCH;
    }
    count = ir_operand_int(compare->rhs.int_value + 1);
    count_op = &count;
  }
  if (compare->rhs.kind != IR_OPERAND_SYMBOL &&
      compare->rhs.kind != IR_OPERAND_INT) {
    return FILL_NO_MATCH;
  }
  if (compare->rhs.kind == IR_OPERAND_SYMBOL &&
      !ir_fill_symbol_is_invariant(function, branch_index + 1, jump_index,
                                   compare->rhs.name)) {
    return FILL_NO_MATCH;
  }
  int iv_from_zero = ir_iv_zero_at_header(function, header_index, iv);

  const IRInstruction *offset_producer_seen = NULL;
  const IRInstruction *idx_add = NULL;
  const IRInstruction *shl = NULL;
  const IRInstruction *addr = NULL;
  const IRInstruction *store = NULL;
  const IRInstruction *increment = NULL;
  for (size_t k = 0; k < body_count; k++) {
    const IRInstruction *ins = body[k];
    if (ins->op == IR_OP_BINARY && ins->text && strcmp(ins->text, "+") == 0 &&
        !ins->is_float && !idx_add && !shl && !addr &&
        ins->dest.kind == IR_OPERAND_TEMP && ins->dest.name &&
        !ir_fill_dest_is_store_address(body, body_count, ins) &&
        (ir_operand_is_symbol_named(&ins->lhs, iv) ||
         ir_operand_is_symbol_named(&ins->rhs, iv))) {
      idx_add = ins;
    } else if (ins->op == IR_OP_BINARY && ins->text &&
               strcmp(ins->text, "<<") == 0 && !ins->is_float && !shl &&
               (ir_operand_is_symbol_named(&ins->lhs, iv) ||
                (idx_add && ir_operand_is_temp_named(&ins->lhs,
                                                     idx_add->dest.name))) &&
               ins->rhs.kind == IR_OPERAND_INT &&
               ins->dest.kind == IR_OPERAND_TEMP) {
      shl = ins;
    } else if (ins->op == IR_OP_BINARY && ins->text &&
               strcmp(ins->text, "+") == 0 && !ins->is_float && !addr &&
               ins->dest.kind == IR_OPERAND_TEMP && ins->dest.name &&
               ins->lhs.kind == IR_OPERAND_SYMBOL && ins->lhs.name &&
               ((shl && ir_operand_is_temp_named(&ins->rhs, shl->dest.name)) ||
                (!shl && ir_operand_is_symbol_named(&ins->rhs, iv)) ||
                (!shl && idx_add &&
                 ir_operand_is_temp_named(&ins->rhs, idx_add->dest.name)))) {
      addr = ins;
    } else if (ins->op == IR_OP_STORE && !store && addr &&
               ir_operand_is_temp_named(&ins->dest, addr->dest.name) &&
               ins->rhs.kind == IR_OPERAND_INT) {
      store = ins;
    } else if (ins->op == IR_OP_BINARY && ins->text &&
               strcmp(ins->text, "+") == 0 && !ins->is_float && !increment &&
               ins->dest.kind == IR_OPERAND_SYMBOL && ins->dest.name &&
               strcmp(ins->dest.name, iv) == 0 &&
               ir_operand_is_symbol_named(&ins->lhs, iv) &&
               ins->rhs.kind == IR_OPERAND_INT && ins->rhs.int_value == 1) {
      increment = ins;
    } else if (ins->op == IR_OP_BINARY && !ins->is_float && k == 0 &&
               ins->dest.kind == IR_OPERAND_TEMP && ins->dest.name) {
      offset_producer_seen = ins;
    } else {
      return FILL_NO_MATCH;
    }
  }
  if (!store || !addr || !increment) {
    return FILL_NO_MATCH;
  }

  if (idx_add) {
    const IROperand *consumer = shl ? &shl->lhs : &addr->rhs;
    if (!ir_operand_is_temp_named(consumer, idx_add->dest.name)) {
      return FILL_NO_MATCH;
    }
  }

  const IROperand *offset_op = NULL;
  const IRInstruction *offset_producer = NULL;
  if (idx_add) {
    const IROperand *other = ir_operand_is_symbol_named(&idx_add->lhs, iv)
                                 ? &idx_add->rhs
                                 : &idx_add->lhs;
    if (other->kind == IR_OPERAND_INT) {
      offset_op = other;
    } else if (other->kind == IR_OPERAND_SYMBOL) {
      if (strcmp(other->name, iv) == 0 ||
          !ir_fill_symbol_is_invariant(function, branch_index + 1, jump_index,
                                       other->name)) {
        return FILL_NO_MATCH;
      }
      offset_op = other;
    } else if (other->kind == IR_OPERAND_TEMP && other->name &&
               offset_producer_seen &&
               ir_operand_is_temp_named(other, offset_producer_seen->dest.name)) {
      const IRInstruction *prod = offset_producer_seen;
      const IROperand *sides[2] = {&prod->lhs, &prod->rhs};
      for (int s = 0; s < 2; s++) {
        if (sides[s]->kind == IR_OPERAND_SYMBOL) {
          if (strcmp(sides[s]->name, iv) == 0 ||
              !ir_fill_symbol_is_invariant(function, branch_index + 1,
                                           jump_index, sides[s]->name)) {
            return FILL_NO_MATCH;
          }
        } else if (sides[s]->kind != IR_OPERAND_INT) {
          return FILL_NO_MATCH;
        }
      }
      offset_op = other;
      offset_producer = prod;
    } else if (other->kind == IR_OPERAND_TEMP && other->name) {
      int defined_before = 0;
      for (size_t i = 0; i < header_index; i++) {
        const IRInstruction *ins = &function->instructions[i];
        if (ir_instruction_writes_destination(ins) &&
            ins->dest.kind == IR_OPERAND_TEMP && ins->dest.name &&
            strcmp(ins->dest.name, other->name) == 0) {
          defined_before = 1;
          break;
        }
      }
      if (!defined_before) {
        return FILL_NO_MATCH;
      }
      offset_op = other;
    } else {
      return FILL_NO_MATCH;
    }
  } else if (offset_producer_seen) {
    return FILL_NO_MATCH;
  }

  long long index_width = 0;
  if (!iv_from_zero || idx_add) {
    if (ir_fill_local_has_type(function, iv, "int32")) {
      index_width = 32;
    } else if (ir_fill_local_has_type(function, iv, "int64")) {
      index_width = 64;
    } else {
      return FILL_NO_MATCH;
    }
  }

  long long size = store->rhs.int_value;
  if (size != 1 && size != 2 && size != 4 && size != 8) {
    return FILL_NO_MATCH;
  }
  if (shl) {
    if ((1LL << shl->rhs.int_value) != size) {
      return FILL_NO_MATCH;
    }
  } else if (size != 1) {
    return FILL_NO_MATCH;
  }
  if (!ir_fill_symbol_is_invariant(function, branch_index + 1, jump_index,
                                   addr->lhs.name)) {
    return FILL_NO_MATCH;
  }
  int iv_live_after = ir_symbol_live_after_loop(function, jump_index + 1, iv);
  if (iv_live_after && index_width == 0) {
    if (ir_fill_local_has_type(function, iv, "int32")) {
      index_width = 32;
    } else if (ir_fill_local_has_type(function, iv, "int64")) {
      index_width = 64;
    } else {
      return FILL_NO_MATCH;
    }
  }
  IROperand value = {0};
  if (!ir_fill_value_operand(function, branch_index + 1, jump_index,
                             &store->lhs, size, &value)) {
    return FILL_NO_MATCH;
  }
  IROperand start = {0};
  const IROperand *start_op = NULL;
  if (!iv_from_zero) {
    start = ir_operand_symbol(iv);
    if (!start.name) {
      ir_operand_destroy(&value);
      return 0;
    }
    start_op = &start;
  }
  char *iv_name = iv_live_after ? mettle_strdup(iv) : NULL;
  if (iv_live_after && !iv_name) {
    ir_operand_destroy(&value);
    ir_operand_destroy(&start);
    return 0;
  }
  int ok = ir_fill_install(function, header_index, jump_index, 0,
                           size, &addr->lhs, count_op, &value, start_op,
                           offset_op, offset_producer, NULL, NULL, changed);
  if (ok && iv_name) {
    IRInstruction *fused =
        &function->instructions[header_index + (offset_producer ? 1 : 0)];
    ir_operand_destroy(&fused->dest);
    fused->dest = ir_operand_symbol(iv_name);
  }
  mettle_free_string(iv_name);
  if (ok && index_width == 64) {
    IRInstruction *fused =
        &function->instructions[header_index + (offset_producer ? 1 : 0)];
    IROperand *grown = realloc(fused->arguments, 6 * sizeof(IROperand));
    if (grown) {
      fused->arguments = grown;
      fused->arguments[5] = ir_operand_int(64);
      fused->argument_count = 6;
    }
  }
  ir_operand_destroy(&value);
  ir_operand_destroy(&start);
  return ok;
}

static const IROperand *ir_fill_uncast(const IRInstruction *const *casts,
                                       size_t cast_count,
                                       const IROperand *operand) {
  if (operand->kind != IR_OPERAND_TEMP || !operand->name) {
    return operand;
  }
  for (size_t c = 0; c < cast_count; c++) {
    if (ir_operand_is_temp_named(&casts[c]->dest, operand->name)) {
      return casts[c]->is_float ? NULL : &casts[c]->lhs;
    }
  }
  return operand;
}

static int ir_fill_try_byte_walk(IRFunction *function, size_t header_index,
                                 size_t branch_index, size_t jump_index,
                                 const IRInstruction *compare,
                                 const IRInstruction *const *body,
                                 size_t body_count, int *changed) {
  const char *iv = compare->lhs.name;
  if (compare->rhs.kind != IR_OPERAND_SYMBOL &&
      compare->rhs.kind != IR_OPERAND_INT) {
    return FILL_NO_MATCH;
  }
  if (compare->rhs.kind == IR_OPERAND_SYMBOL &&
      !ir_fill_symbol_is_invariant(function, branch_index + 1, jump_index,
                                   compare->rhs.name)) {
    return FILL_NO_MATCH;
  }
  if (!ir_fill_local_has_type(function, iv, "int64")) {
    return FILL_NO_MATCH;
  }

  const IRInstruction *casts[4];
  size_t cast_count = 0;
  const IRInstruction *addr = NULL;
  const IRInstruction *store = NULL;
  const IRInstruction *advance = NULL;
  for (size_t k = 0; k < body_count; k++) {
    const IRInstruction *ins = body[k];
    if (ins->op == IR_OP_CAST && ins->dest.kind == IR_OPERAND_TEMP &&
        ins->dest.name && cast_count < 4) {
      casts[cast_count++] = ins;
    } else if (ins->op == IR_OP_BINARY && ins->text &&
               strcmp(ins->text, "+") == 0 && !ins->is_float && !addr &&
               ins->dest.kind == IR_OPERAND_TEMP && ins->dest.name &&
               ins->lhs.kind == IR_OPERAND_SYMBOL && ins->lhs.name &&
               ir_operand_is_symbol_named(&ins->rhs, iv)) {
      addr = ins;
    } else if (ins->op == IR_OP_STORE && !store && addr &&
               ins->rhs.kind == IR_OPERAND_INT) {
      const IROperand *dest = ir_fill_uncast(casts, cast_count, &ins->dest);
      if (!dest || !ir_operand_is_temp_named(dest, addr->dest.name)) {
        return FILL_NO_MATCH;
      }
      store = ins;
    } else if (ins->op == IR_OP_BINARY && ins->text &&
               strcmp(ins->text, "+") == 0 && !ins->is_float && !advance &&
               ins->dest.kind == IR_OPERAND_SYMBOL && ins->dest.name &&
               strcmp(ins->dest.name, iv) == 0 &&
               ir_operand_is_symbol_named(&ins->lhs, iv)) {
      advance = ins;
    } else {
      return FILL_NO_MATCH;
    }
  }
  if (!store || !addr || !advance) {
    return FILL_NO_MATCH;
  }
  const IROperand *stride = ir_fill_uncast(casts, cast_count, &advance->rhs);
  if (!stride || stride->kind != IR_OPERAND_INT) {
    return FILL_NO_MATCH;
  }
  long long size = store->rhs.int_value;
  if ((size != 1 && size != 2 && size != 4 && size != 8) ||
      stride->int_value != size ||
      !ir_fill_symbol_is_invariant(function, branch_index + 1, jump_index,
                                   addr->lhs.name)) {
    return FILL_NO_MATCH;
  }
  const IROperand *raw_value = ir_fill_uncast(casts, cast_count, &store->lhs);
  if (!raw_value) {
    return FILL_NO_MATCH;
  }
  IROperand value = {0};
  if (!ir_fill_value_operand(function, branch_index + 1, jump_index,
                             raw_value, size, &value)) {
    return FILL_NO_MATCH;
  }
  IROperand start = ir_operand_symbol(iv);
  if (!start.name) {
    ir_operand_destroy(&value);
    return 0;
  }
  int iv_live_after = ir_symbol_live_after_loop(function, jump_index + 1, iv);
  char *iv_name = iv_live_after ? mettle_strdup(iv) : NULL;
  if (iv_live_after && !iv_name) {
    ir_operand_destroy(&value);
    ir_operand_destroy(&start);
    return 0;
  }
  int ok = ir_fill_install(function, header_index, jump_index, 2,
                           size, &addr->lhs, &compare->rhs, &value, &start,
                           NULL, NULL, NULL, NULL, changed);
  if (ok && iv_name) {
    IRInstruction *fused = &function->instructions[header_index];
    ir_operand_destroy(&fused->dest);
    fused->dest = ir_operand_symbol(iv_name);
  }
  mettle_free_string(iv_name);
  ir_operand_destroy(&value);
  ir_operand_destroy(&start);
  return ok;
}

static int ir_try_vectorize_fill_at(IRFunction *function, size_t header_index,
                                    int *changed) {
  size_t compare_index = 0, branch_index = 0, jump_index = 0;
  int matched = 0;
  int inclusive = 0;
  if (!ir_fill_frame(function, header_index, &compare_index, &branch_index,
                     &jump_index, &inclusive, &matched)) {
    return 0;
  }
  if (!matched) {
    return 1;
  }
  const IRInstruction *compare = &function->instructions[compare_index];

  const IRInstruction *body[6];
  size_t body_count = 0;
  for (size_t i = branch_index + 1; i < jump_index; i++) {
    const IRInstruction *ins = &function->instructions[i];
    if (ins->op == IR_OP_NOP) {
      continue;
    }
    if (body_count >= 6) {
      return 1;
    }
    body[body_count++] = ins;
  }
  if (body_count < 2) {
    return 1;
  }

  int r = inclusive
              ? FILL_NO_MATCH
              : ir_fill_try_pointer_walk(function, header_index, branch_index,
                                         jump_index, compare, body,
                                         body_count, changed);
  if (r != FILL_NO_MATCH) {
    return r;
  }
  r = ir_fill_try_indexed(function, header_index, branch_index, jump_index,
                          compare, body, body_count, inclusive, changed);
  if (r != FILL_NO_MATCH) {
    return r;
  }
  r = inclusive
          ? FILL_NO_MATCH
          : ir_fill_try_byte_walk(function, header_index, branch_index,
                                  jump_index, compare, body, body_count,
                                  changed);
  if (r != FILL_NO_MATCH) {
    return r;
  }
  return 1;
}

int ir_simd_fill_pass(IRFunction *function, int *changed) {
  if (!function) {
    return 0;
  }
  for (size_t i = 0; i < function->instruction_count; i++) {
    if (function->instructions[i].op == IR_OP_LABEL &&
        ir_label_is_while_header(function->instructions[i].text)) {
      if (!ir_try_vectorize_fill_at(function, i, changed)) {
        return 0;
      }
    }
  }
  return 1;
}

static int ir_copy_symbol_is_pointer(const IRFunction *function,
                                     const char *name) {
  const char *type = ir_function_local_declared_type(function, name);
  if (!type && function->parameter_names && function->parameter_types) {
    for (size_t i = 0; i < function->parameter_count; i++) {
      if (function->parameter_names[i] &&
          strcmp(function->parameter_names[i], name) == 0) {
        type = function->parameter_types[i];
        break;
      }
    }
  }
  return type && strchr(type, '*') != NULL;
}

static size_t ir_copy_temp_reads(const IRFunction *function, size_t lo,
                                 size_t hi, const char *temp) {
  size_t reads = 0;
  for (size_t i = lo; i < hi; i++) {
    const IRInstruction *in = &function->instructions[i];
    if (ir_operand_is_temp_named(&in->lhs, temp)) {
      reads++;
    }
    if (ir_operand_is_temp_named(&in->rhs, temp)) {
      reads++;
    }
    if (in->op == IR_OP_STORE && ir_operand_is_temp_named(&in->dest, temp)) {
      reads++;
    }
    for (size_t a = 0; a < in->argument_count; a++) {
      if (ir_operand_is_temp_named(&in->arguments[a], temp)) {
        reads++;
      }
    }
  }
  return reads;
}

static int ir_copy_index_matches(const IRInstruction *const *body,
                                 size_t body_count, const IROperand *index,
                                 const char *iv, long long size) {
  if (size == 1) {
    return ir_operand_is_symbol_named(index, iv);
  }
  if (index->kind != IR_OPERAND_TEMP || !index->name) {
    return 0;
  }
  for (size_t k = 0; k < body_count; k++) {
    const IRInstruction *in = body[k];
    if (in->op != IR_OP_BINARY || in->is_float || !in->text ||
        in->dest.kind != IR_OPERAND_TEMP || !in->dest.name ||
        strcmp(in->dest.name, index->name) != 0) {
      continue;
    }
    if (strcmp(in->text, "<<") == 0 && in->rhs.kind == IR_OPERAND_INT &&
        (1LL << in->rhs.int_value) == size) {
      return ir_operand_is_symbol_named(&in->lhs, iv);
    }
    if (strcmp(in->text, "*") == 0 && in->rhs.kind == IR_OPERAND_INT &&
        in->rhs.int_value == size) {
      return ir_operand_is_symbol_named(&in->lhs, iv);
    }
    return 0;
  }
  return 0;
}

static const char *ir_copy_address_base(const IRInstruction *const *body,
                                        size_t body_count,
                                        const IROperand *address,
                                        const char *iv, long long size) {
  if (address->kind != IR_OPERAND_TEMP || !address->name) {
    return NULL;
  }
  for (size_t k = 0; k < body_count; k++) {
    const IRInstruction *in = body[k];
    if (in->op != IR_OP_BINARY || in->is_float || !in->text ||
        strcmp(in->text, "+") != 0 || in->dest.kind != IR_OPERAND_TEMP ||
        !in->dest.name || strcmp(in->dest.name, address->name) != 0) {
      continue;
    }
    if (in->lhs.kind != IR_OPERAND_SYMBOL || !in->lhs.name ||
        !ir_copy_index_matches(body, body_count, &in->rhs, iv, size)) {
      return NULL;
    }
    return in->lhs.name;
  }
  return NULL;
}

#define IR_COPY_MIN_ELEMENTS 64

static int ir_copy_install(IRFunction *function, size_t header_index,
                           size_t branch_index, const IROperand *count,
                           const char *dst_base, const char *src_base,
                           long long size, int *changed) {
  static int g_copy_counter;
  const char *exit_label = function->instructions[branch_index].text;
  char kernel_label[64];
  char scalar_label[64];
  char guard_temp[64];
  IRInstruction guard = {0};
  IRInstruction take = {0};
  IRInstruction skip = {0};
  IRInstruction at_kernel = {0};
  IRInstruction fused = {0};
  IRInstruction rejoin = {0};
  IRInstruction at_scalar = {0};
  int always = 0;
  int failed = 0;
  size_t at = header_index;

  if (!exit_label) {
    return 1;
  }
  if (count->kind == IR_OPERAND_INT) {
    if (count->int_value < IR_COPY_MIN_ELEMENTS) {
      return 1;
    }
    always = 1;
  }
  snprintf(kernel_label, sizeof(kernel_label), "ir_copy_k_%d", g_copy_counter);
  snprintf(scalar_label, sizeof(scalar_label), "ir_copy_s_%d", g_copy_counter);
  snprintf(guard_temp, sizeof(guard_temp), "__copy_g_%d", g_copy_counter);
  g_copy_counter++;

  fused.op = IR_OP_SIMD_COPY;
  fused.location = function->instructions[header_index].location;
  fused.dest = ir_operand_symbol(dst_base);
  fused.lhs = ir_operand_symbol(src_base);
  if (!ir_operand_clone(count, &fused.rhs)) {
    ir_instruction_destroy_storage(&fused);
    return 0;
  }
  fused.arguments = calloc(1, sizeof(IROperand));
  if (!fused.dest.name || !fused.lhs.name || !fused.arguments) {
    ir_instruction_destroy_storage(&fused);
    return 0;
  }
  fused.argument_count = 1;
  fused.arguments[0] = ir_operand_int(size);

  if (always) {
    if (!ir_function_insert_instruction(function, at, &fused)) {
      failed = 1;
    }
    ir_instruction_destroy_storage(&fused);
    if (failed) {
      return 0;
    }
    at++;
    rejoin.op = IR_OP_JUMP;
    rejoin.text = mettle_strdup(exit_label);
    rejoin.location = function->instructions[header_index].location;
    if (!rejoin.text ||
        !ir_function_insert_instruction(function, at, &rejoin)) {
      ir_instruction_destroy_storage(&rejoin);
      return 0;
    }
    ir_instruction_destroy_storage(&rejoin);
    if (changed) {
      *changed = 1;
    }
    return 1;
  }

  guard.op = IR_OP_BINARY;
  guard.text = mettle_strdup("<");
  guard.dest = ir_operand_temp(guard_temp);
  guard.location = fused.location;
  if (!ir_operand_clone(count, &guard.lhs)) {
    failed = 1;
  }
  guard.rhs = ir_operand_int(IR_COPY_MIN_ELEMENTS);

  take.op = IR_OP_BRANCH_ZERO;
  take.text = mettle_strdup(kernel_label);
  take.lhs = ir_operand_temp(guard_temp);
  take.location = fused.location;

  skip.op = IR_OP_JUMP;
  skip.text = mettle_strdup(scalar_label);
  skip.location = fused.location;

  at_kernel.op = IR_OP_LABEL;
  at_kernel.text = mettle_strdup(kernel_label);
  at_kernel.location = fused.location;

  rejoin.op = IR_OP_JUMP;
  rejoin.text = mettle_strdup(exit_label);
  rejoin.location = fused.location;

  at_scalar.op = IR_OP_LABEL;
  at_scalar.text = mettle_strdup(scalar_label);
  at_scalar.location = fused.location;

  if (!guard.text || !guard.dest.name || !take.text || !take.lhs.name ||
      !skip.text || !at_kernel.text || !rejoin.text || !at_scalar.text) {
    failed = 1;
  }
  if (!failed) {
    IRInstruction *steps[6] = {&guard, &take, &skip, &at_kernel, &fused,
                               &rejoin};
    for (int s = 0; s < 6 && !failed; s++) {
      if (!ir_function_insert_instruction(function, at, steps[s])) {
        failed = 1;
      } else {
        at++;
      }
    }
    if (!failed && !ir_function_insert_instruction(function, at, &at_scalar)) {
      failed = 1;
    }
  }
  ir_instruction_destroy_storage(&guard);
  ir_instruction_destroy_storage(&take);
  ir_instruction_destroy_storage(&skip);
  ir_instruction_destroy_storage(&at_kernel);
  ir_instruction_destroy_storage(&fused);
  ir_instruction_destroy_storage(&rejoin);
  ir_instruction_destroy_storage(&at_scalar);
  if (failed) {
    return 0;
  }
  if (changed) {
    *changed = 1;
  }
  return 1;
}

static int ir_try_vectorize_copy_walk(IRFunction *function, size_t header_index,
                                      size_t branch_index, size_t jump_index,
                                      const IRInstruction *compare,
                                      const IRInstruction *const *body,
                                      size_t body_count, int *changed) {
  const char *src_p = NULL;
  const char *end_p = NULL;
  const char *dst_p = NULL;
  const char *src_base = NULL;
  const char *dst_base = NULL;
  const IRInstruction *load = NULL;
  const IRInstruction *store = NULL;
  const IRInstruction *src_step = NULL;
  const IRInstruction *dst_step = NULL;
  const char *dead_counter = NULL;
  IROperand len = {0};
  long long size = 0;
  (void)dead_counter;

  if (compare->lhs.kind != IR_OPERAND_SYMBOL || !compare->lhs.name ||
      compare->rhs.kind != IR_OPERAND_SYMBOL || !compare->rhs.name) {
    return 1;
  }
  src_p = compare->lhs.name;
  end_p = compare->rhs.name;

  for (size_t k = 0; k < body_count; k++) {
    const IRInstruction *ins = body[k];
    if (ins->op == IR_OP_LOAD && !load && !ins->is_float && !ins->is_volatile &&
        ins->rhs.kind == IR_OPERAND_INT &&
        ir_operand_is_symbol_named(&ins->lhs, src_p) &&
        ins->dest.kind == IR_OPERAND_TEMP && ins->dest.name) {
      load = ins;
      continue;
    }
    if (ins->op == IR_OP_STORE && !store && !ins->is_float &&
        !ins->is_volatile && ins->rhs.kind == IR_OPERAND_INT && load &&
        ir_operand_is_temp_named(&ins->lhs, load->dest.name) &&
        ins->dest.kind == IR_OPERAND_SYMBOL && ins->dest.name) {
      store = ins;
      dst_p = ins->dest.name;
      continue;
    }
    if (ins->op == IR_OP_BINARY && ins->text && strcmp(ins->text, "+") == 0 &&
        !ins->is_float && ins->dest.kind == IR_OPERAND_SYMBOL &&
        ins->dest.name && ins->rhs.kind == IR_OPERAND_INT &&
        ir_operand_is_symbol_named(&ins->lhs, ins->dest.name)) {
      if (!src_step && strcmp(ins->dest.name, src_p) == 0) {
        src_step = ins;
        continue;
      }
      if (!dst_step && dst_p && strcmp(ins->dest.name, dst_p) == 0) {
        dst_step = ins;
        continue;
      }
      if (!dead_counter &&
          !ir_symbol_live_after_loop(function, jump_index + 1,
                                     ins->dest.name)) {
        dead_counter = ins->dest.name;
        continue;
      }
    }
    return 1;
  }

  if (!load || !store || !src_step || !dst_step || !dst_p ||
      strcmp(src_p, dst_p) == 0) {
    return 1;
  }
  size = load->rhs.int_value;
  if (size != store->rhs.int_value || size != src_step->rhs.int_value ||
      size != dst_step->rhs.int_value ||
      (size != 1 && size != 2 && size != 4 && size != 8)) {
    return 1;
  }
  if (ir_copy_temp_reads(function, branch_index + 1, jump_index,
                         load->dest.name) != 1) {
    return 1;
  }
  if (ir_symbol_live_after_loop(function, jump_index + 1, src_p) ||
      ir_symbol_live_after_loop(function, jump_index + 1, dst_p)) {
    return 1;
  }

  src_base = ir_find_ptr_init_base(function, header_index, src_p);
  dst_base = ir_find_ptr_init_base(function, header_index, dst_p);
  if (!src_base || !dst_base || strcmp(src_base, dst_base) == 0 ||
      !ir_copy_symbol_is_pointer(function, src_base) ||
      !ir_copy_symbol_is_pointer(function, dst_base) ||
      !ir_fill_symbol_is_invariant(function, branch_index + 1, jump_index,
                                   src_base) ||
      !ir_fill_symbol_is_invariant(function, branch_index + 1, jump_index,
                                   dst_base)) {
    return 1;
  }
  if (!ir_find_ptr_loop_len_operand(function, header_index, end_p, src_base,
                                    &len)) {
    return 1;
  }

  return ir_copy_install(function, header_index, branch_index, &len,
                         dst_base, src_base, size, changed);
  return 1;
}

static int ir_copy_already_versioned(const IRFunction *function,
                                     size_t header_index) {
  for (size_t i = header_index; i-- > 0;) {
    const IRInstruction *in = &function->instructions[i];
    if (in->op == IR_OP_NOP) {
      continue;
    }
    return in->op == IR_OP_LABEL && in->text &&
           strncmp(in->text, "ir_copy_s_", 10) == 0;
  }
  return 0;
}

static int ir_try_vectorize_copy_at(IRFunction *function, size_t header_index,
                                    int *changed) {
  size_t compare_index = 0, branch_index = 0, jump_index = 0;
  int matched = 0;
  int inclusive = 0;
  const IRInstruction *body[8];
  size_t body_count = 0;
  const IRInstruction *load = NULL;
  const IRInstruction *store = NULL;
  const IRInstruction *increment = NULL;
  const IRInstruction *compare = NULL;
  const char *iv = NULL;
  const char *src_base = NULL;
  const char *dst_base = NULL;
  long long size = 0;

  if (ir_copy_already_versioned(function, header_index)) {
    return 1;
  }
  if (!ir_fill_frame(function, header_index, &compare_index, &branch_index,
                     &jump_index, &inclusive, &matched)) {
    return 0;
  }
  if (!matched) {
    return 1;
  }
  compare = &function->instructions[compare_index];
  if (compare->rhs.kind != IR_OPERAND_SYMBOL &&
      compare->rhs.kind != IR_OPERAND_INT) {
    return 1;
  }
  iv = compare->lhs.name;
  if (!iv) {
    return 1;
  }

  for (size_t i = branch_index + 1; i < jump_index; i++) {
    const IRInstruction *ins = &function->instructions[i];
    if (ins->op == IR_OP_NOP) {
      continue;
    }
    if (body_count >= 8) {
      return 1;
    }
    body[body_count++] = ins;
  }
  if (body_count < 3) {
    return 1;
  }

  if (compare->rhs.kind == IR_OPERAND_SYMBOL && !inclusive &&
      ir_copy_symbol_is_pointer(function, iv)) {
    return ir_try_vectorize_copy_walk(function, header_index, branch_index,
                                      jump_index, compare, body, body_count,
                                      changed);
  }

  if (!ir_iv_zero_at_header(function, header_index, iv) ||
      ir_symbol_live_after_loop(function, jump_index + 1, iv)) {
    return 1;
  }

  for (size_t k = 0; k < body_count; k++) {
    const IRInstruction *ins = body[k];
    switch (ins->op) {
    case IR_OP_LOAD:
      if (load || ins->is_float || ins->is_volatile ||
          ins->rhs.kind != IR_OPERAND_INT ||
          ins->dest.kind != IR_OPERAND_TEMP || !ins->dest.name) {
        return 1;
      }
      load = ins;
      break;
    case IR_OP_STORE:
      if (store || ins->is_float || ins->is_volatile ||
          ins->rhs.kind != IR_OPERAND_INT) {
        return 1;
      }
      store = ins;
      break;
    case IR_OP_BINARY:
      if (ins->text && strcmp(ins->text, "+") == 0 &&
          ins->dest.kind == IR_OPERAND_SYMBOL && ins->dest.name &&
          strcmp(ins->dest.name, iv) == 0) {
        if (increment || !ir_operand_is_symbol_named(&ins->lhs, iv) ||
            ins->rhs.kind != IR_OPERAND_INT || ins->rhs.int_value != 1) {
          return 1;
        }
        increment = ins;
        break;
      }
      if (ins->dest.kind != IR_OPERAND_TEMP) {
        return 1;
      }
      break;
    default:
      return 1;
    }
  }
  if (!load || !store || !increment) {
    return 1;
  }

  size = load->rhs.int_value;
  if (size != store->rhs.int_value ||
      (size != 1 && size != 2 && size != 4 && size != 8)) {
    return 1;
  }
  if (!ir_operand_is_temp_named(&store->lhs, load->dest.name)) {
    return 1;
  }
  if (ir_copy_temp_reads(function, branch_index + 1, jump_index,
                         load->dest.name) != 1) {
    return 1;
  }

  src_base = ir_copy_address_base(body, body_count, &load->lhs, iv, size);
  dst_base = ir_copy_address_base(body, body_count, &store->dest, iv, size);
  if (!src_base || !dst_base || strcmp(src_base, dst_base) == 0) {
    return 1;
  }
  if (!ir_fill_symbol_is_invariant(function, branch_index + 1, jump_index,
                                   src_base) ||
      !ir_fill_symbol_is_invariant(function, branch_index + 1, jump_index,
                                   dst_base) ||
      !ir_copy_symbol_is_pointer(function, src_base) ||
      !ir_copy_symbol_is_pointer(function, dst_base)) {
    return 1;
  }
  if (compare->rhs.kind == IR_OPERAND_SYMBOL &&
      !ir_fill_symbol_is_invariant(function, branch_index + 1, jump_index,
                                   compare->rhs.name)) {
    return 1;
  }

  {
    IROperand count = {0};
    int ok;
    if (inclusive) {
      count = ir_operand_int(compare->rhs.int_value + 1);
    } else if (!ir_operand_clone(&compare->rhs, &count)) {
      return 0;
    }
    ok = ir_copy_install(function, header_index, branch_index, &count,
                         dst_base, src_base, size, changed);
    ir_operand_destroy(&count);
    return ok;
  }
}

int ir_simd_copy_pass(IRFunction *function, int *changed) {
  if (!function) {
    return 0;
  }
  for (size_t i = 0; i < function->instruction_count; i++) {
    if (function->instructions[i].op == IR_OP_LABEL &&
        ir_label_is_while_header(function->instructions[i].text)) {
      if (!ir_try_vectorize_copy_at(function, i, changed)) {
        return 0;
      }
    }
  }
  return 1;
}
