#include "ir_optimize_internal.h"
#include "../../common.h"

#define IR_PREFETCH_MAX_SLICE 12
#define IR_PREFETCH_DEFAULT_DIST 64

static size_t g_prefetch_id;

static long long ir_prefetch_distance_override(void) {
  static long long cached = -1;
  if (cached < 0) {
    const char *env = getenv("METTLE_PREFETCH_DIST");
    cached = 0;
    if (!env || !*env) {
      return -1;
    }
    long long v = atoll(env);
    if (v >= 1 && v <= 4096) {
      cached = v;
    }
  }
  return cached > 0 ? cached : -1;
}

static long long ir_prefetch_distance_for_loop(const IRFunction *function,
                                               SourceLocation location) {
  long long override = ir_prefetch_distance_override();
  if (override > 0) {
    return override;
  }
  return ir_opt_prefetch_distance_for_site(function, location,
                                          IR_PREFETCH_DEFAULT_DIST);
}

typedef struct {
  size_t indices[IR_PREFETCH_MAX_SLICE];
  size_t interior_load_index;
  size_t count;
  size_t interior_loads;
} IRPrefetchSlice;

static int ir_prefetch_slice_contains(const IRPrefetchSlice *slice,
                                      size_t index) {
  for (size_t i = 0; i < slice->count; i++) {
    if (slice->indices[i] == index) {
      return 1;
    }
  }
  return 0;
}

static int ir_prefetch_collect_slice(const IRFunction *function,
                                     size_t body_start, size_t body_end,
                                     size_t at, const char *temp,
                                     const char *iv_symbol,
                                     IRPrefetchSlice *slice) {
  const IRInstruction *producer =
      ir_find_temp_producer_before(function, at, temp);
  if (!producer) {
    return 0;
  }
  size_t prod_index = (size_t)(producer - function->instructions);
  if (prod_index < body_start || prod_index >= body_end) {
    return 0;
  }
  if (ir_prefetch_slice_contains(slice, prod_index)) {
    return 1;
  }
  if (slice->count >= IR_PREFETCH_MAX_SLICE) {
    return 0;
  }

  if (producer->op != IR_OP_BINARY && producer->op != IR_OP_CAST &&
      producer->op != IR_OP_LOAD) {
    return 0;
  }
  if (producer->is_float) {
    return 0;
  }
  if (producer->op == IR_OP_LOAD) {
    slice->interior_loads++;
    slice->interior_load_index = prod_index;
  }

  const IROperand *sources[2] = {&producer->lhs, NULL};
  size_t source_count = 1;
  if (producer->op == IR_OP_BINARY) {
    sources[1] = &producer->rhs;
    source_count = 2;
  }
  for (size_t s = 0; s < source_count; s++) {
    const IROperand *op = sources[s];
    if (op->kind == IR_OPERAND_INT) {
      continue;
    }
    if (op->kind == IR_OPERAND_SYMBOL && op->name) {
      if (strcmp(op->name, iv_symbol) == 0) {
        continue;
      }
      if (!ir_affine_symbol_written_in(function, body_start, body_end,
                                         op->name)) {
        continue;
      }
      return 0;
    }
    if (op->kind == IR_OPERAND_TEMP && op->name) {
      if (!ir_prefetch_collect_slice(function, body_start, body_end,
                                     prod_index, op->name, iv_symbol, slice)) {
        return 0;
      }
      continue;
    }
    return 0;
  }

  slice->indices[slice->count++] = prod_index;
  return 1;
}

static int ir_prefetch_slice_uses_iv(const IRFunction *function,
                                     const IRPrefetchSlice *slice,
                                     const char *iv_symbol) {
  for (size_t i = 0; i < slice->count; i++) {
    const IRInstruction *ins = &function->instructions[slice->indices[i]];
    if (ir_operand_is_symbol_named(&ins->lhs, iv_symbol) ||
        ir_operand_is_symbol_named(&ins->rhs, iv_symbol)) {
      return 1;
    }
  }
  return 0;
}

static int ir_prefetch_interior_load_uses_iv(const IRFunction *function,
                                             size_t body_start,
                                             size_t body_end,
                                             const IRPrefetchSlice *slice,
                                             const char *iv_symbol) {
  const IRInstruction *load = &function->instructions[slice->interior_load_index];
  IRPrefetchSlice inner;
  if (load->lhs.kind != IR_OPERAND_TEMP || !load->lhs.name) {
    return 0;
  }
  memset(&inner, 0, sizeof(inner));
  if (!ir_prefetch_collect_slice(function, body_start, body_end,
                                 slice->interior_load_index, load->lhs.name,
                                 iv_symbol, &inner)) {
    return 0;
  }
  return ir_prefetch_slice_uses_iv(function, &inner, iv_symbol);
}

static char *ir_prefetch_temp_name(void) {
  char buf[32];
  snprintf(buf, sizeof(buf), ".pf%zu", g_prefetch_id++);
  return mettle_strdup(buf);
}

static const char *ir_prefetch_emit_clone(IRInstructionVector *vec,
                                          const IRInstruction *src,
                                          IRNameMap *names,
                                          const char *iv_symbol,
                                          const char *lookahead_temp) {
  IRInstruction cloned = {0};
  if (!ir_clone_instruction_plain(src, &cloned)) {
    ir_instruction_destroy_storage(&cloned);
    return NULL;
  }
  IROperand *ops[2] = {&cloned.lhs, &cloned.rhs};
  for (int k = 0; k < 2; k++) {
    IROperand *op = ops[k];
    if (op->kind == IR_OPERAND_TEMP && op->name) {
      const char *mapped = ir_name_map_lookup(names, op->name);
      if (mapped) {
        mettle_free_string(op->name);
        op->name = mettle_strdup(mapped);
        if (!op->name) {
          ir_instruction_destroy_storage(&cloned);
          return NULL;
        }
      }
    } else if (op->kind == IR_OPERAND_SYMBOL && op->name &&
               strcmp(op->name, iv_symbol) == 0) {
      mettle_free_string(op->name);
      op->kind = IR_OPERAND_TEMP;
      op->name = mettle_strdup(lookahead_temp);
      if (!op->name) {
        ir_instruction_destroy_storage(&cloned);
        return NULL;
      }
    }
  }
  char *fresh = ir_prefetch_temp_name();
  if (!fresh || !ir_name_map_add(names, cloned.dest.name, fresh)) {
    free(fresh);
    ir_instruction_destroy_storage(&cloned);
    return NULL;
  }
  ir_operand_destroy(&cloned.dest);
  cloned.dest = ir_operand_temp(fresh);
  free(fresh);
  if (!cloned.dest.name || !ir_instruction_vector_append_move(vec, &cloned)) {
    ir_instruction_destroy_storage(&cloned);
    return NULL;
  }
  return vec->items[vec->count - 1].dest.name;
}

typedef struct {
  size_t insert_after;
  IRInstructionVector seq;
} IRPrefetchPlan;

static int ir_pf_append(IRInstructionVector *seq, IRInstruction *in,
                        int ready) {
  if (ready && ir_instruction_vector_append_move(seq, in)) {
    return 1;
  }
  ir_instruction_destroy_storage(in);
  return 0;
}

static int ir_pf_emit_ahead(IRInstructionVector *seq, SourceLocation loc,
                            const char *ahead, const char *iv_symbol,
                            long long distance) {
  IRInstruction add = {0};

  add.op = IR_OP_BINARY;
  add.location = loc;
  add.text = mettle_strdup("+");
  add.dest = ir_operand_temp(ahead);
  add.lhs = ir_operand_symbol(iv_symbol);
  add.rhs = ir_operand_int(distance);
  return ir_pf_append(seq, &add,
                      add.text && add.dest.name && add.lhs.name);
}

static int ir_pf_emit_in_range(IRInstructionVector *seq, SourceLocation loc,
                               const char *cond, const char *ahead,
                               const IRInstruction *compare) {
  IRInstruction cmp = {0};

  cmp.op = IR_OP_BINARY;
  cmp.location = loc;
  cmp.text = mettle_strdup("<");
  cmp.dest = ir_operand_temp(cond);
  cmp.lhs = ir_operand_temp(ahead);
  cmp.rhs = compare->rhs.kind == IR_OPERAND_INT
                ? ir_operand_int(compare->rhs.int_value)
                : ir_operand_symbol(compare->rhs.name);
  return ir_pf_append(seq, &cmp,
                      cmp.text && cmp.dest.name && cmp.lhs.name &&
                          (cmp.rhs.kind == IR_OPERAND_INT || cmp.rhs.name));
}

static int ir_pf_emit_skip_branch(IRInstructionVector *seq, SourceLocation loc,
                                  const char *cond, const char *skip_label) {
  IRInstruction br = {0};

  br.op = IR_OP_BRANCH_ZERO;
  br.location = loc;
  br.lhs = ir_operand_temp(cond);
  br.text = mettle_strdup(skip_label);
  return ir_pf_append(seq, &br, br.lhs.name && br.text);
}

static int ir_pf_emit_prefetch(IRInstructionVector *seq, SourceLocation loc,
                               const char *addr_name) {
  IRInstruction pf = {0};

  if (!addr_name) {
    return 0;
  }
  pf.op = IR_OP_PREFETCH;
  pf.location = loc;
  pf.lhs = ir_operand_temp(addr_name);
  return ir_pf_append(seq, &pf, pf.lhs.name != NULL);
}

static int ir_pf_emit_label(IRInstructionVector *seq, SourceLocation loc,
                            const char *label) {
  IRInstruction lbl = {0};

  lbl.op = IR_OP_LABEL;
  lbl.location = loc;
  lbl.text = mettle_strdup(label);
  return ir_pf_append(seq, &lbl, lbl.text != NULL);
}

static size_t ir_pf_find_target_load(const IRFunction *function,
                                     size_t body_start, size_t body_end,
                                     const char *iv_symbol,
                                     IRPrefetchSlice *slice) {
  for (size_t i = body_start; i < body_end; i++) {
    const IRInstruction *ins = &function->instructions[i];

    if (ins->op != IR_OP_LOAD || ins->lhs.kind != IR_OPERAND_TEMP ||
        !ins->lhs.name) {
      continue;
    }
    memset(slice, 0, sizeof(*slice));
    if (!ir_prefetch_collect_slice(function, body_start, body_end, i,
                                   ins->lhs.name, iv_symbol, slice)) {
      continue;
    }
    if (slice->interior_loads == 1 &&
        ir_prefetch_slice_uses_iv(function, slice, iv_symbol) &&
        ir_prefetch_interior_load_uses_iv(function, body_start, body_end, slice,
                                          iv_symbol)) {
      return i;
    }
  }
  return (size_t)-1;
}

static int ir_pf_clone_address_slice(IRInstructionVector *seq,
                                     const IRFunction *function,
                                     const IRPrefetchSlice *slice,
                                     IRNameMap *names, const char *iv_symbol,
                                     const char *ahead) {
  for (size_t s = 0; s < slice->count; s++) {
    if (!ir_prefetch_emit_clone(seq, &function->instructions[slice->indices[s]],
                                names, iv_symbol, ahead)) {
      return 0;
    }
  }
  return 1;
}

static int ir_prefetch_plan_loop(const IRFunction *function,
                                 size_t header_index, IRPrefetchPlan *plan) {
  IRAffineLoop loop;
  IRPrefetchSlice slice;
  IRNameMap names = {0};
  IRInstructionVector *seq = &plan->seq;
  const IRInstruction *compare;
  SourceLocation loc;
  size_t target_load;
  long long distance;
  char *ahead;
  char *cond;
  char skip_label[48];
  int ok;

  if (!ir_affine_model_loop(function, header_index, &loop) ||
      !ir_affine_straight_line_body(&loop) || !ir_affine_unit_step(&loop) ||
      !ir_affine_starts_at_zero(&loop) || !ir_affine_bound_invariant(&loop)) {
    return 0;
  }
  compare = &function->instructions[loop.bounds.compare_index];
  target_load = ir_pf_find_target_load(function, loop.body_start, loop.body_end,
                                       loop.iv, &slice);
  if (target_load == (size_t)-1) {
    return 0;
  }
  memset(seq, 0, sizeof(*seq));
  loc = function->instructions[target_load].location;
  if (ir_prefetch_distance_override() < 0 &&
      !ir_opt_should_prefetch_site(function, loc)) {
    return 0;
  }
  distance = ir_prefetch_distance_for_loop(function, loc);
  ahead = ir_prefetch_temp_name();
  cond = ir_prefetch_temp_name();
  snprintf(skip_label, sizeof(skip_label), "ir_pf_skip_%zu", g_prefetch_id++);
  ok = ahead && cond &&
       ir_pf_emit_ahead(seq, loc, ahead, loop.iv, distance) &&
       ir_pf_emit_in_range(seq, loc, cond, ahead, compare) &&
       ir_pf_emit_skip_branch(seq, loc, cond, skip_label) &&
       ir_pf_clone_address_slice(seq, function, &slice, &names, loop.iv,
                                 ahead) &&
       ir_pf_emit_prefetch(
           seq, loc,
           ir_name_map_lookup(&names,
                              function->instructions[target_load].lhs.name)) &&
       ir_pf_emit_label(seq, loc, skip_label);
  free(ahead);
  free(cond);
  ir_name_map_destroy(&names);
  if (!ok) {
    ir_instruction_vector_destroy(seq);
    return 0;
  }
  plan->insert_after = loop.bounds.branch_index;
  return 1;
}

int ir_prefetch_indirect_pass(IRFunction *function, int *changed) {
  if (!function) {
    return 0;
  }

  for (size_t i = 0; i < function->instruction_count; i++) {
    if (function->instructions[i].op != IR_OP_LABEL ||
        !ir_label_is_while_header(function->instructions[i].text)) {
      continue;
    }
    IRPrefetchPlan plan;
    memset(&plan, 0, sizeof(plan));
    if (!ir_prefetch_plan_loop(function, i, &plan)) {
      continue;
    }

    IRInstructionVector vec = {0};
    int ok = 1;
    for (size_t k = 0; k < function->instruction_count && ok; k++) {
      IRInstruction cloned = {0};
      if (!ir_clone_instruction_plain(&function->instructions[k], &cloned) ||
          !ir_instruction_vector_append_move(&vec, &cloned)) {
        ir_instruction_destroy_storage(&cloned);
        ok = 0;
        break;
      }
      if (k == plan.insert_after) {
        for (size_t s = 0; s < plan.seq.count && ok; s++) {
          IRInstruction c2 = {0};
          if (!ir_clone_instruction_plain(&plan.seq.items[s], &c2) ||
              !ir_instruction_vector_append_move(&vec, &c2)) {
            ir_instruction_destroy_storage(&c2);
            ok = 0;
          }
        }
      }
    }
    ir_instruction_vector_destroy(&plan.seq);
    if (!ok) {
      ir_instruction_vector_destroy(&vec);
      return 0;
    }
    if (!ir_function_replace_instructions(function, &vec)) {
      ir_instruction_vector_destroy(&vec);
      return 0;
    }
    if (ir_explain_enabled()) {
      ir_explain_remark(function->name, "loop",
                        function->instructions[i].location, 1,
                        "software prefetch inserted for indirect access "
                        "(look-ahead distance covers the miss latency)",
                        NULL, NULL, NULL);
      ir_explain_remark_code("prefetched");
    }
    if (changed) {
      *changed = 1;
    }
  }
  return 1;
}
