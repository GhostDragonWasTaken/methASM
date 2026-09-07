  for (size_t i = d->begin + 1; i < d->end; i++) {
    const IRInstruction *ins = &d->function->instructions[i];
    if (ins->op == IR_OP_LABEL) {
      if (ins->text && (strstr(ins->text, "ir_while_") != NULL ||
                        strstr(ins->text, "ir_for_cond_") != NULL)) {
        d->past_header = 1;
      }
      continue;
    }
    if (!d->past_header) {
      continue;
    }
    switch (ins->op) {
    case IR_OP_DECLARE_LOCAL:
      if (d->past_header && !d->body_local && ins->dest.kind == IR_OPERAND_SYMBOL &&
          ins->dest.name) {
        d->body_local = ins->dest.name;
      }
      break;
    case IR_OP_ADDRESS_OF:
      if (!d->rebased_array && ins->lhs.kind == IR_OPERAND_SYMBOL &&
          ins->lhs.name) {
        const char *declared =
            ir_function_local_declared_type(d->function, ins->lhs.name);
        if (declared && strchr(declared, '[')) {
          d->rebased_array = ins->lhs.name;
        }
      }
      break;
    case IR_OP_CALL:
      if (!(ins->text && strstr(ins->text, "crash_trap")) && !d->callee) {
        d->callee = ins->text ? ins->text : "?";
      }
      break;
    case IR_OP_CALL_INDIRECT:
      d->has_indirect_call = 1;
      break;
    case IR_OP_NEW:
      d->has_new = 1;
      break;
    case IR_OP_INLINE_ASM:
      d->has_asm = 1;
      break;
    case IR_OP_BRANCH_ZERO:
    case IR_OP_BRANCH_EQ:
      if (!ir_label_is_runtime_check(ins->text)) {
        d->branch_count++;
        if (ins->text && d->branch_target_count < 8) {
          d->branch_targets[d->branch_target_count++] = ins->text;
        }
      }
      break;
    case IR_OP_JUMP:
      if (!ir_label_is_runtime_check(ins->text)) {
        d->jump_count++;
      }
      break;
    case IR_OP_RETURN:
      d->has_return_in_body = 1;
      break;
    case IR_OP_LOAD:
    case IR_OP_STORE: {
      long long sz = (ins->rhs.kind == IR_OPERAND_INT) ? ins->rhs.int_value : 4;
      if (ins->op == IR_OP_LOAD) {
        d->load_count++;
      } else {
        d->store_count++;
        if (sz == 1) {
          d->byte_store_count++;
        }
      }
      if (ins->is_float) {
        if (sz == 4) {
          d->has_f32 = 1;
        } else if (sz == 8) {
          d->has_f64 = 1;
        }
      } else {
        if (sz == 1 && ins->op == IR_OP_LOAD) {
          d->has_byte_load = 1;
        } else if (sz == 2) {
          d->has_i16 = 1;
        } else if (sz == 4 && ins->op == IR_OP_LOAD) {
          d->has_i32_load = 1;
        } else if (sz == 8) {
          const char *base =
              (ins->op == IR_OP_LOAD && ins->dest.kind == IR_OPERAND_TEMP)
                  ? ir_simd_field_base_symbol(d->function, d->begin, i, &ins->lhs)
                  : NULL;
          if (base) {
            d->reloaded_base_count++;
            if (!d->reloaded_base_sym) {
              d->reloaded_base_sym = base;
            }
          } else {
            d->has_i64 = 1;
          }
        }
      }
      break;
    }
    case IR_OP_BINARY: {
      if (!ins->is_float && ins->text && ins->text[0] == '+' &&
          !ins->text[1] && ins->dest.kind == IR_OPERAND_SYMBOL &&
          ins->dest.name &&
          ((ins->lhs.kind == IR_OPERAND_SYMBOL && ins->lhs.name &&
            strcmp(ins->lhs.name, ins->dest.name) == 0 &&
            ins->rhs.kind == IR_OPERAND_TEMP) ||
           (ins->rhs.kind == IR_OPERAND_SYMBOL && ins->rhs.name &&
            strcmp(ins->rhs.name, ins->dest.name) == 0 &&
            ins->lhs.kind == IR_OPERAND_TEMP))) {
        d->has_int_accum = 1;
        d->int_accum_sym = ins->dest.name;
      }
      if (ins->is_float && ins->text && ins->text[0] == '*' && !ins->text[1]) {
        d->has_float_mul = 1;
      }
      if (ins->is_float && ins->text && ins->text[0] == '+' &&
          !ins->text[1] &&
          ((ins->lhs.kind == IR_OPERAND_SYMBOL && ins->lhs.name) ||
           (ins->rhs.kind == IR_OPERAND_SYMBOL && ins->rhs.name))) {
        d->has_float_accum = 1;
      }
      break;
    }
    default:
      break;
    }
  }

