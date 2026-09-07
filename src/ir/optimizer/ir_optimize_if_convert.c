#include "ir_optimize_internal.h"

#define IR_IFCONV_MAX_ARM 10
#define IR_IFCONV_MAX_LOCALS 8

typedef struct {
  const char *name;
  IROperand value;
} IRIfConvAssign;

typedef struct {
  size_t insns[IR_IFCONV_MAX_ARM];
  size_t insn_count;
  IRIfConvAssign locals[IR_IFCONV_MAX_LOCALS];
  size_t local_count;
} IRIfConvArm;

static int ir_ifconv_binary_is_trapping(const IRInstruction *in) {
  return in->text && (strcmp(in->text, "/") == 0 || strcmp(in->text, "%") == 0);
}

static int ir_ifconv_writes_local(const IRInstruction *in, const char **out) {
  if (in->dest.kind == IR_OPERAND_SYMBOL && in->dest.name) {
    *out = in->dest.name;
    return 1;
  }
  return 0;
}

static int ir_ifconv_arm_reads_local(const IRInstruction *in, const char *name) {
  if (ir_operand_is_symbol_named(&in->lhs, name) ||
      ir_operand_is_symbol_named(&in->rhs, name)) {
    return 1;
  }
  for (size_t a = 0; a < in->argument_count; a++) {
    if (ir_operand_is_symbol_named(&in->arguments[a], name)) {
      return 1;
    }
  }
  return 0;
}

static int ir_ifconv_scan_arm(const IRFunction *function, size_t start,
                              size_t end, IRIfConvArm *arm) {
  memset(arm, 0, sizeof(*arm));
  for (size_t i = start; i < end; i++) {
    const IRInstruction *in = &function->instructions[i];
    if (in->op == IR_OP_NOP) {
      continue;
    }
    if (in->op == IR_OP_DECLARE_LOCAL) {
      return 0;
    }
    if (in->op != IR_OP_ASSIGN && in->op != IR_OP_BINARY &&
        in->op != IR_OP_UNARY && in->op != IR_OP_CAST) {
      return 0;
    }
    if (in->op == IR_OP_BINARY && ir_ifconv_binary_is_trapping(in)) {
      return 0;
    }

    const char *local = NULL;
    if (ir_ifconv_writes_local(in, &local)) {
      if (in->op != IR_OP_ASSIGN || in->is_float) {
        return 0;
      }
      if (in->lhs.kind != IR_OPERAND_TEMP && in->lhs.kind != IR_OPERAND_SYMBOL &&
          in->lhs.kind != IR_OPERAND_INT) {
        return 0;
      }
      for (size_t k = 0; k < arm->local_count; k++) {
        if (strcmp(arm->locals[k].name, local) == 0) {
          return 0;
        }
      }
      if (arm->local_count >= IR_IFCONV_MAX_LOCALS) {
        return 0;
      }
      arm->locals[arm->local_count].name = local;
      arm->locals[arm->local_count].value = in->lhs;
      arm->local_count++;
      continue;
    }

    if (in->dest.kind != IR_OPERAND_TEMP || !in->dest.name) {
      return 0;
    }
    if (arm->insn_count >= IR_IFCONV_MAX_ARM) {
      return 0;
    }
    arm->insns[arm->insn_count++] = i;
  }

  for (size_t k = 0; k < arm->local_count; k++) {
    for (size_t i = start; i < end; i++) {
      if (ir_ifconv_arm_reads_local(&function->instructions[i],
                                    arm->locals[k].name)) {
        return 0;
      }
    }
  }
  return 1;
}

static const IROperand *ir_ifconv_arm_value(const IRIfConvArm *arm,
                                            const char *name) {
  for (size_t k = 0; k < arm->local_count; k++) {
    if (strcmp(arm->locals[k].name, name) == 0) {
      return &arm->locals[k].value;
    }
  }
  return NULL;
}

static int ir_ifconv_label_referenced_outside(const IRFunction *function,
                                              const char *label, size_t lo,
                                              size_t hi) {
  for (size_t i = 0; i < function->instruction_count; i++) {
    const IRInstruction *in = &function->instructions[i];
    if (i >= lo && i < hi) {
      continue;
    }
    if ((in->op == IR_OP_JUMP || in->op == IR_OP_BRANCH_ZERO ||
         in->op == IR_OP_BRANCH_EQ) &&
        in->text && strcmp(in->text, label) == 0) {
      return 1;
    }
  }
  return 0;
}

static int ir_ifconv_try_at(IRFunction *function, size_t branch_index,
                            int *changed) {
  const IRInstruction *branch = &function->instructions[branch_index];
  if (branch->op != IR_OP_BRANCH_ZERO || !branch->text ||
      (branch->lhs.kind != IR_OPERAND_TEMP &&
       branch->lhs.kind != IR_OPERAND_SYMBOL)) {
    return 1;
  }
  const char *else_label = branch->text;

  size_t then_jump = (size_t)-1;
  for (size_t i = branch_index + 1; i < function->instruction_count; i++) {
    IROpcode op = function->instructions[i].op;
    if (op == IR_OP_JUMP) {
      then_jump = i;
      break;
    }
    if (op == IR_OP_LABEL || op == IR_OP_BRANCH_ZERO || op == IR_OP_BRANCH_EQ ||
        op == IR_OP_RETURN) {
      return 1;
    }
  }
  if (then_jump == (size_t)-1 || !function->instructions[then_jump].text) {
    return 1;
  }
  const char *join_label = function->instructions[then_jump].text;

  size_t else_label_index = then_jump + 1;
  while (else_label_index < function->instruction_count &&
         function->instructions[else_label_index].op == IR_OP_NOP) {
    else_label_index++;
  }
  if (else_label_index >= function->instruction_count ||
      function->instructions[else_label_index].op != IR_OP_LABEL ||
      !function->instructions[else_label_index].text ||
      strcmp(function->instructions[else_label_index].text, else_label) != 0) {
    return 1;
  }

  size_t join_label_index = (size_t)-1;
  for (size_t i = else_label_index + 1; i < function->instruction_count; i++) {
    const IRInstruction *in = &function->instructions[i];
    if (in->op == IR_OP_LABEL) {
      if (in->text && strcmp(in->text, join_label) == 0) {
        join_label_index = i;
      }
      break;
    }
  }
  if (join_label_index == (size_t)-1) {
    return 1;
  }

  if (ir_ifconv_label_referenced_outside(function, else_label, branch_index,
                                         join_label_index)) {
    return 1;
  }

  IRIfConvArm then_arm, else_arm;
  if (!ir_ifconv_scan_arm(function, branch_index + 1, then_jump, &then_arm) ||
      !ir_ifconv_scan_arm(function, else_label_index + 1, join_label_index,
                          &else_arm)) {
    return 1;
  }
  if (then_arm.local_count == 0 && else_arm.local_count == 0) {
    return 1;
  }

  IRInstructionVector seq = {0};
  int ok = 1;

  const IRIfConvArm *arms[2] = {&then_arm, &else_arm};
  for (int a = 0; a < 2 && ok; a++) {
    for (size_t k = 0; k < arms[a]->insn_count && ok; k++) {
      IRInstruction cloned = {0};
      if (!ir_clone_instruction_plain(
              &function->instructions[arms[a]->insns[k]], &cloned) ||
          !ir_instruction_vector_append_move(&seq, &cloned)) {
        ir_instruction_destroy_storage(&cloned);
        ok = 0;
      }
    }
  }

  const char *seen[IR_IFCONV_MAX_LOCALS * 2];
  size_t seen_count = 0;
  for (int a = 0; a < 2 && ok; a++) {
    for (size_t k = 0; k < arms[a]->local_count && ok; k++) {
      const char *name = arms[a]->locals[k].name;
      int dup = 0;
      for (size_t s = 0; s < seen_count; s++) {
        if (strcmp(seen[s], name) == 0) {
          dup = 1;
          break;
        }
      }
      if (dup) {
        continue;
      }
      seen[seen_count++] = name;

      const IROperand *tv = ir_ifconv_arm_value(&then_arm, name);
      const IROperand *ev = ir_ifconv_arm_value(&else_arm, name);
      IROperand pre = ir_operand_symbol(name);
      IRInstruction sel = {0};
      sel.op = IR_OP_SELECT;
      sel.location = branch->location;
      sel.dest = ir_operand_symbol(name);
      sel.arguments = (IROperand *)calloc(1, sizeof(IROperand));
      int built = pre.name && sel.dest.name && sel.arguments &&
                  ir_operand_clone(&branch->lhs, &sel.lhs) &&
                  ir_operand_clone(tv ? tv : &pre, &sel.rhs) &&
                  ir_operand_clone(ev ? ev : &pre, &sel.arguments[0]);
      sel.argument_count = 1;
      ir_operand_destroy(&pre);
      if (!built || !ir_instruction_vector_append_move(&seq, &sel)) {
        ir_instruction_destroy_storage(&sel);
        ok = 0;
        break;
      }
    }
  }

  if (!ok) {
    ir_instruction_vector_destroy(&seq);
    return 0;
  }

  IRInstructionVector out = {0};
  for (size_t i = 0; i < branch_index && ok; i++) {
    IRInstruction c = {0};
    if (!ir_clone_instruction_plain(&function->instructions[i], &c) ||
        !ir_instruction_vector_append_move(&out, &c)) {
      ir_instruction_destroy_storage(&c);
      ok = 0;
    }
  }
  for (size_t k = 0; k < seq.count && ok; k++) {
    IRInstruction c = {0};
    if (!ir_clone_instruction_plain(&seq.items[k], &c) ||
        !ir_instruction_vector_append_move(&out, &c)) {
      ir_instruction_destroy_storage(&c);
      ok = 0;
    }
  }
  for (size_t i = join_label_index; i < function->instruction_count && ok; i++) {
    IRInstruction c = {0};
    if (!ir_clone_instruction_plain(&function->instructions[i], &c) ||
        !ir_instruction_vector_append_move(&out, &c)) {
      ir_instruction_destroy_storage(&c);
      ok = 0;
    }
  }
  ir_instruction_vector_destroy(&seq);
  if (!ok) {
    ir_instruction_vector_destroy(&out);
    return 0;
  }
  if (!ir_function_replace_instructions(function, &out)) {
    ir_instruction_vector_destroy(&out);
    return 0;
  }
  if (ir_explain_enabled()) {
    ir_explain_remark(function->name, "branch", branch->location, 1,
                      "if-converted to branchless select (cmov): a "
                      "data-dependent branch became straight-line code",
                      NULL, NULL, NULL);
    ir_explain_remark_code("if-converted");
  }
  if (changed) {
    *changed = 1;
  }
  return 1;
}

int ir_if_convert_pass(IRFunction *function, int *changed) {
  if (!function) {
    return 0;
  }
  {
    static int enabled = -1;
    if (enabled < 0) {
      const char *e = getenv("METTLE_IF_CONVERT");
      enabled = e && *e && strcmp(e, "0") != 0;
    }
    if (!enabled) {
      return 1;
    }
  }
  for (size_t i = 0; i < function->instruction_count; i++) {
    if (function->instructions[i].op == IR_OP_BRANCH_ZERO) {
      int local_changed = 0;
      if (!ir_ifconv_try_at(function, i, &local_changed)) {
        return 0;
      }
      if (local_changed) {
        if (changed) {
          *changed = 1;
        }
        i = (size_t)-1;
      }
    }
  }
  return 1;
}
