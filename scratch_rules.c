static int ir_simd_bail_rule00(IrSimdBail *d) {
  if (d->callee) {
    for (size_t i = 0; i < d->function->instruction_count; i++) {
      const IRInstruction *in = &d->function->instructions[i];
      if (!in->text || strcmp(in->text, d->callee) ||
          ir_safety_intrinsic(in) == IR_SAFETY_INTRINSIC_NONE) continue;
      snprintf(d->reason, d->reason_cap,
               "each iteration retains a memory safety operation; its bounds "
               "or pointer origin could not be proved independent of the iteration");
      snprintf(d->fix, d->fix_cap,
               "keep the checked scalar loop, or expose a fixed array extent "
               "and an affine index so the compiler can prove the range");
      ir_simd_advisory(d);
      ir_simd_diag(d, IR_SIMD_BAIL_SAFETY_IN_BODY);
      return 1;
    }
    IRFunction *callee_fn =
        g_explain_program ? ir_program_find_function(g_explain_program, d->callee)
                          : NULL;
    if (g_explain_program && !callee_fn) {
      snprintf(d->reason, d->reason_cap,
               "each iteration calls `%s`, an external function with no body "
               "this compiler can see, so it can never be inlined and this "
               "loop cannot vectorize as written",
               d->callee);
      snprintf(d->fix, d->fix_cap,
               "none needed if the call IS the work (I/O, OS calls): the "
               "scalar loop is the right code; if the call is loop-invariant, "
               "hoist it; if it is hot compute, replace it with Mettle code "
               "so the inliner can take it");
      ir_simd_advisory(d);
      ir_simd_diag(d, IR_SIMD_BAIL_EXTERN_CALL_IN_BODY);
      return 1;
    }
    snprintf(d->reason, d->reason_cap,
             "each iteration calls `%s`; loops vectorize only after every "
             "call in the body has been inlined away",
             d->callee);
    snprintf(d->fix, d->fix_cap,
             "make `%s` inline-eligible (small body, or mark it @inline), or "
             "hoist the call out of the loop",
             d->callee);
    ir_simd_diag(d, IR_SIMD_BAIL_CALL_IN_BODY);
    return 1;
  }
  return 0;
}

static int ir_simd_bail_rule01(IrSimdBail *d) {
  if (d->has_indirect_call) {
    snprintf(d->reason, d->reason_cap,
             "each iteration calls through a function pointer, which can "
             "never be inlined away");
    snprintf(d->fix, d->fix_cap,
             "call the target directly if it is known at compile time");
    ir_simd_diag(d, IR_SIMD_BAIL_INDIRECT_CALL);
    return 1;
  }
  return 0;
}

static int ir_simd_bail_rule02(IrSimdBail *d) {
  if (d->has_new) {
    snprintf(d->reason, d->reason_cap, "the loop body allocates memory (`new`) "
                                 "every iteration");
    snprintf(d->fix, d->fix_cap, "hoist the allocation out of the loop");
    ir_simd_diag(d, IR_SIMD_BAIL_ALLOC_IN_BODY);
    return 1;
  }
  return 0;
}

static int ir_simd_bail_rule03(IrSimdBail *d) {
  if (d->has_asm) {
    snprintf(d->reason, d->reason_cap,
             "the loop body contains inline assembly, which is opaque to the "
             "vectorizer");
    ir_simd_diag(d, IR_SIMD_BAIL_INLINE_ASM);
    return 1;
  }
  return 0;
}

static int ir_simd_bail_rule04(IrSimdBail *d) {
  if (d->branch_count > 1 || d->jump_count > 1) {
    int outward_branches = 0;
    for (size_t t = 0; t < d->branch_target_count; t++) {
      int inside = 0;
      for (size_t i = d->begin + 1; i < d->end && !inside; i++) {
        const IRInstruction *lab = &d->function->instructions[i];
        if (lab->op == IR_OP_LABEL && lab->text &&
            strcmp(lab->text, d->branch_targets[t]) == 0) {
          inside = 1;
        }
      }
      if (!inside) {
        outward_branches++;
      }
    }
    if (d->has_return_in_body || outward_branches > 1) {
      snprintf(d->reason, d->reason_cap,
               "the loop can exit before its trip count, and its body does "
               "more than test the exit condition; the search skip-ahead "
               "kernel only covers pure find/mismatch loops (it must be safe "
               "to skip the iterations before the first hit)");
      snprintf(d->fix, d->fix_cap,
               "pure searches DO vectorize: `if (a[i] == key) ...` (or != < > "
               "<= >=, key a constant/variable, or a[i] != b[i]) with nothing "
               "else in the body becomes an 8-wide compare+movemask scan. "
               "Split any per-iteration work out of this loop, or hoist the "
               "search into its own loop and process from the found index");
      ir_simd_diag(d, IR_SIMD_BAIL_EARLY_EXIT);
      return 1;
    }
    {
      const char *written = NULL;
      IRBranchShape shape =
          ir_region_branch_shape(d->function, d->begin, d->end, &written);
      if (shape == IR_BRANCH_SHAPE_EXTREMUM) {
        const char *acc_type =
            written ? ir_function_local_declared_type(d->function, written) : NULL;
        snprintf(d->reason, d->reason_cap,
                 "this is a running %s, a shape that does vectorize, but "
                 "the kernel did not claim it%s%s%s",
                 "minimum/maximum",
                 acc_type ? " (`" : "", acc_type ? written : "",
                 acc_type ? "` is the accumulator)" : "");
        if (acc_type && strcmp(acc_type, "float64") != 0 &&
            strcmp(acc_type, "float32") != 0 && strcmp(acc_type, "int32") != 0) {
          snprintf(d->fix, d->fix_cap,
                   "widen the array to int32 elements (float32 and float64 "
                   "work too): the extremum kernel carries those lane widths "
                   "and these are %s. Declaring `%s` alone does not move it, "
                   "because the width the kernel reads is the element's",
                   acc_type, written);
        } else {
          snprintf(d->fix, d->fix_cap,
                   "the extremum kernel needs the compared value to come from "
                   "`base[i]` over float32, float64 or int32 elements, and the "
                   "counter to start at 0 or at 1 after seeding from `a[0]`; "
                   "one that widens narrower elements, or indexes anything "
                   "else, falls outside it");
        }
        ir_simd_diag(d, IR_SIMD_BAIL_EXTREMUM_SHAPE);
        return 1;
      }
      if (shape == IR_BRANCH_SHAPE_COUNT) {
        snprintf(d->reason, d->reason_cap,
                 "the body accumulates into `%s` under a condition. Over int32 "
                 "elements that is made unconditional and vectorized, so this "
                 "one is out for one of three reasons: the elements are float "
                 "(no kernel, and the branchless form has none either), the "
                 "guard is not a comparison (`if (x & 6)` fires on 2, 4 and 6 "
                 "alike, and the addend would be multiplied by those), or an "
                 "else arm writes `%s` too, which is a select rather than an "
                 "accumulate",
                 written ? written : "the accumulator",
                 written ? written : "the accumulator");
        snprintf(d->fix, d->fix_cap,
                 "over int32 elements, add the comparison rather than "
                 "branching on it: `%s = %s + ((a[i] & 6) != 0);` vectorizes, "
                 "and so does any other comparison written that way. Over "
                 "float elements there is nothing to change here",
                 written ? written : "count", written ? written : "count");
        ir_simd_diag(d, IR_SIMD_BAIL_PREDICATED_COUNT);
        return 1;
      }
      if (shape == IR_BRANCH_SHAPE_CLAMP_STORE) {
        snprintf(d->reason, d->reason_cap,
                 "the body chooses `%s` under a condition before storing it. "
                 "That shape does vectorize over int32 elements, as vpminsd, "
                 "vpmaxsd or a lane select, so what stopped this one is either "
                 "the element type (no float select kernel yet) or a nest too "
                 "deep for the six ymm registers an int32 map has",
                 written ? written : "a local");
        snprintf(d->fix, d->fix_cap,
                 "if the elements are int32, split the deepest arm into its own "
                 "loop so fewer values are live at once; otherwise this is a "
                 "gap in the compiler, not a problem with the loop");
        ir_simd_advisory(d);
        ir_simd_diag(d, IR_SIMD_BAIL_CLAMP_STORE);
        return 1;
      }
    }
    snprintf(d->reason, d->reason_cap,
             "the loop body branches on data (an `if` or `&&`/`||` per "
             "iteration). An `if` that only chooses a value is converted to a "
             "lane select and does vectorize, so this one does something a "
             "masked lane cannot: a store, a call, or a nest deeper than the "
             "kernel has registers for");
    snprintf(d->fix, d->fix_cap,
             "keep the arms to choosing a value, or split the work into two "
             "simpler loops");
    ir_simd_diag(d, IR_SIMD_BAIL_CONTROL_FLOW);
    return 1;
  }
  return 0;
}

static int ir_simd_bail_rule05(IrSimdBail *d) {
  if (d->has_i16) {
    snprintf(d->reason, d->reason_cap,
             "the loop reads/writes 16-bit integers, and no 16-bit kernels "
             "exist");
    if (d->has_int_accum) {
      const char *acc_type =
          d->int_accum_sym ? ir_function_local_declared_type(d->function,
                                                          d->int_accum_sym)
                        : NULL;
      if (acc_type && strcmp(acc_type, "int64") == 0) {
        snprintf(d->fix, d->fix_cap, "use int32 elements");
      } else {
        snprintf(d->fix, d->fix_cap,
                 "use int32 elements and declare the accumulator as int64");
      }
    } else {
      snprintf(d->fix, d->fix_cap, "use int32 (or int8 if the values fit)");
    }
    ir_simd_diag(d, IR_SIMD_BAIL_INT16_ELEMENTS);
    return 1;
  }
  return 0;
}

static int ir_simd_bail_rule06(IrSimdBail *d) {
  if (d->has_i64) {
    snprintf(d->reason, d->reason_cap,
             "the loop reads/writes 64-bit integer arrays, and no 64-bit "
             "integer kernels exist");
    snprintf(d->fix, d->fix_cap, "use int32 arrays if the values fit");
    ir_simd_diag(d, IR_SIMD_BAIL_INT64_ELEMENTS);
    return 1;
  }
  return 0;
}

static int ir_simd_bail_rule07(IrSimdBail *d) {
  if (d->reloaded_base_count > 0 &&
      d->load_count + d->store_count > d->reloaded_base_count) {
    snprintf(d->reason, d->reason_cap,
             "the body re-reads the array's base pointer out of `%s` every "
             "iteration; a store in the body could change that pointer, so the "
             "read cannot be lifted out and the kernels have no base to hold "
             "fixed for the whole loop",
             d->reloaded_base_sym ? d->reloaded_base_sym : "a struct");
    snprintf(d->fix, d->fix_cap,
             "bind the base pointer to a local before the loop and index that "
             "local in the body");
    ir_simd_diag(d, IR_SIMD_BAIL_RELOADED_BASE);
    return 1;
  }
  return 0;
}

static int ir_simd_bail_rule08(IrSimdBail *d) {
  {
    const char *ops[IR_RECUR_MAX_OPS];
    size_t n_ops = 0;
    const char *recur_symbol =
        ir_region_find_serial_recurrence(d->function, d->begin, d->end, ops, &n_ops);
    if (recur_symbol) {
      char op_list[64];
      size_t w = 0;
      op_list[0] = '\0';
      for (size_t i = 0; i < n_ops && w + 10 < sizeof(op_list); i++) {
        int n = snprintf(op_list + w, sizeof(op_list) - w, "%s`%s`",
                         i ? ", " : "", ops[i]);
        if (n < 0) {
          break;
        }
        w += (size_t)n;
      }
      snprintf(d->reason, d->reason_cap,
               "`%s` carries a loop-carried recurrence: each iteration computes "
               "it from its own previous value (through %s), so the iterations "
               "form a dependency chain that cannot run as independent SIMD "
               "lanes",
               recur_symbol, n_ops ? op_list : "a non-reassociable operation");
      snprintf(d->fix, d->fix_cap,
               "'+'/'-' reductions reassociate and DO vectorize; multiply, "
               "divide, shift, and bitwise/xor recurrences are inherently "
               "serial. If this running state IS the algorithm (a hash, an "
               "RNG, an IIR filter), the loop is already at its scalar floor");
      ir_simd_advisory(d);
      ir_simd_diag(d, IR_SIMD_BAIL_SERIAL_RECURRENCE);
      return 1;
    }
  }
  return 0;
}

static int ir_simd_bail_rule09(IrSimdBail *d) {
  if (d->has_f32 && d->has_f64) {
    long long keep = ir_simd_majority_float_width(d->function, d->begin, d->end);
    snprintf(d->reason, d->reason_cap,
             "the loop mixes float32 and float64 elements; each kernel "
             "handles one width");
    if (keep == 4) {
      snprintf(d->fix, d->fix_cap,
               "keep the loop in float32: the float64 accesses are the "
               "minority, so retype those arrays (or convert outside the "
               "loop)");
    } else if (keep == 8) {
      snprintf(d->fix, d->fix_cap,
               "keep the loop in float64: the float32 accesses are the "
               "minority, so retype those arrays (or convert outside the "
               "loop)");
    } else {
      snprintf(d->fix, d->fix_cap, "keep the loop in a single float width");
    }
    ir_simd_diag(d, IR_SIMD_BAIL_MIXED_FLOAT_WIDTHS);
    return 1;
  }
  return 0;
}

static int ir_simd_bail_rule10(IrSimdBail *d) {
  if (d->has_byte_load && d->has_int_accum) {
    snprintf(d->reason, d->reason_cap,
             "this is a byte-sum loop, but the vpsadbw kernel accumulates "
             "into int64 and this loop's accumulator is narrower");
    snprintf(d->fix, d->fix_cap,
             "declare the accumulator as int64 (sum bytes as "
             "`total = total + (int64)data[i]`)");
    ir_simd_diag(d, IR_SIMD_BAIL_BYTE_SUM_NARROW_ACC);
    return 1;
  }
  return 0;
}

static int ir_simd_bail_rule11(IrSimdBail *d) {
  if (d->has_i32_load && d->int_accum_sym) {
    const char *acc_type =
        ir_function_local_declared_type(d->function, d->int_accum_sym);
    if (!acc_type || strcmp(acc_type, "int64") != 0) {
      snprintf(d->reason, d->reason_cap,
               "this loop sums int32 values into `%s`, but the int32 "
               "reduction kernel accumulates into int64 (eight lanes are "
               "summed without overflow only there) and `%s` is %s",
               d->int_accum_sym, d->int_accum_sym, acc_type ? acc_type : "narrower");
      snprintf(d->fix, d->fix_cap, "declare the accumulator `%s` as int64",
               d->int_accum_sym);
      ir_simd_diag(d, IR_SIMD_BAIL_I32_SUM_NARROW_ACC);
      return 1;
    }
  }
  return 0;
}

static int ir_simd_bail_rule12(IrSimdBail *d) {
  if (d->body_local) {
    if (strstr(d->body_local, "__inl_") != NULL) {
      snprintf(d->reason, d->reason_cap,
               "the body's data flow passes through the local `%s`, left over "
               "from an inlined call; the recognizers' "
               "load\xE2\x86\x92" "compute\xE2\x86\x92" "store matching "
               "cannot see through it",
               d->body_local);
      snprintf(d->fix, d->fix_cap,
               "a compiler limitation, not a code problem; write the "
               "expression directly in the loop body to vectorize today");
      ir_simd_diag(d, IR_SIMD_BAIL_INLINED_PARAM_LOCAL);
    } else {
      snprintf(d->reason, d->reason_cap,
               "the body declares the local `%s` each iteration; the "
               "recognizers' load\xE2\x86\x92" "compute\xE2\x86\x92" "store "
               "matching cannot see through it",
               d->body_local);
      snprintf(d->fix, d->fix_cap,
               "declare `%s` before the loop, or fold the expression in "
               "directly",
               d->body_local);
      ir_simd_diag(d, IR_SIMD_BAIL_BODY_LOCAL);
    }
    return 1;
  }
  return 0;
}

static int ir_simd_bail_rule13(IrSimdBail *d) {
  if (d->has_float_mul && d->has_float_accum && d->load_count >= 2) {
    snprintf(d->reason, d->reason_cap,
             "this is a float multiply-accumulate (dot-product shape), but no "
             "kernel matched its address pattern. The bases must be plain "
             "pointers indexed by the loop counter (base[i])");
    snprintf(d->fix, d->fix_cap,
             "hoist invariant index math into a pointer before the loop "
             "(e.g. `var row: float32* = &m[r * cols];` then `row[c]`)");
    ir_simd_diag(d, IR_SIMD_BAIL_DOT_SHAPE_ADDRESS);
    return 1;
  }
  return 0;
}

static int ir_simd_bail_rule14(IrSimdBail *d) {
  if (d->store_count > 0 && d->load_count == 0) {
    if (d->store_count > 1) {
      snprintf(d->reason, d->reason_cap,
               "the body writes %d destinations; the fill kernel fills one "
               "region per loop, so a body with several stores has no single "
               "region to fill",
               d->store_count);
      snprintf(d->fix, d->fix_cap,
               "split it into one loop per destination; each becomes its own "
               "fill kernel");
      ir_simd_diag(d, IR_SIMD_BAIL_STORE_ONLY_FILL);
      return 1;
    }
    if (d->byte_store_count == d->store_count) {
      snprintf(d->reason, d->reason_cap,
               "the loop fills 1-byte elements, and the fill kernel covers "
               "2-, 4- and 8-byte elements only");
      snprintf(d->fix, d->fix_cap,
               "nothing to change here: this is a gap in the compiler, not a "
               "problem with the loop");
      ir_simd_advisory(d);
      ir_simd_diag(d, IR_SIMD_BAIL_STORE_ONLY_FILL);
      return 1;
    }
    if (d->rebased_array) {
      char element[64];
      ir_type_element_name(
          ir_function_local_declared_type(d->function, d->rebased_array), element,
          sizeof(element));
      snprintf(d->reason, d->reason_cap,
               "the loop fills the stack array `%s`, whose address is retaken "
               "on every iteration; the fill kernel indexes off one invariant "
               "base pointer, and a fresh base each iteration is not one",
               d->rebased_array);
      snprintf(d->fix, d->fix_cap,
               "bind the array to a pointer once before the loop "
               "(`var p: %s* = &%s[0];`) and write `p[i]` in the body",
               element, d->rebased_array);
    } else {
      snprintf(d->reason, d->reason_cap,
               "the loop only writes an invariant value (a fill/init pattern), "
               "but its store address did not match the fill vectorizer's "
               "shapes: a unit-stride element `a[i]`, `a[c + i]` with `c` a "
               "loop-invariant scalar, or a pointer walked by a constant "
               "stride");
      snprintf(d->fix, d->fix_cap,
               "hoist the invariant part of the index into a base pointer "
               "before the loop (`var row = &a[c]; ... row[i] = v;`) so the "
               "write is a plain unit-stride `row[i]`");
    }
    ir_simd_diag(d, IR_SIMD_BAIL_STORE_ONLY_FILL);
    return 1;
  }
  return 0;
}

static int ir_simd_bail_rule15(IrSimdBail *d) {
  {
    long long stride = ir_region_strided_access(d->function, d->begin, d->end);
    if (stride > 1) {
      snprintf(d->reason, d->reason_cap,
               "the loop steps %lld elements at a time (`a[i*%lld]` or "
               "similar); every kernel walks its arrays one contiguous vector "
               "per iteration, so no gather/scatter shape is covered",
               stride, stride);
      snprintf(d->fix, d->fix_cap,
               "nothing to change here unless the layout can change: %s "
               "arrays (one per component) make each loop unit-stride and all "
               "of them vectorize",
               "separate");
      ir_simd_advisory(d);
      ir_simd_diag(d, IR_SIMD_BAIL_STRIDED_ACCESS);
      return 1;
    }
  }
  return 0;
}

static int ir_simd_bail_rule16(IrSimdBail *d) {
  {
    int shift_count = 0;
    int variable_shift = 0;
    for (size_t i = d->begin + 1; i < d->end; i++) {
      const IRInstruction *ins = &d->function->instructions[i];
      if (ins->op != IR_OP_BINARY || ins->is_float || !ins->text) {
        continue;
      }
      int is_shift = strcmp(ins->text, ">>") == 0 ||
                     strcmp(ins->text, "<<") == 0;
      if (!is_shift) {
        continue;
      }
      if (ins->rhs.kind == IR_OPERAND_INT) {
        if (strcmp(ins->text, ">>") == 0) {
          shift_count++;
        }
      } else {
        variable_shift++;
      }
    }
    if (variable_shift > 0 && d->load_count > 0) {
      snprintf(d->reason, d->reason_cap,
               "the body shifts by a value that is not a constant; the kernels "
               "carry a shift only when the distance is written in the source, "
               "so a distance read at run time has none");
      snprintf(d->fix, d->fix_cap,
               "nothing to change here unless the distance can be a constant: "
               "a loop per distance, or a constant, both vectorize. This is a "
               "gap in the compiler rather than a problem with the loop");
      ir_simd_advisory(d);
      ir_simd_diag(d, IR_SIMD_BAIL_VARIABLE_SHIFT);
      return 1;
    }
    if (shift_count > 0 && d->load_count > 0 && d->store_count > 0) {
      snprintf(d->reason, d->reason_cap,
               "the body shifts right, and the value being shifted cannot be "
               "shown to stay inside int32; the lanes are 32 bits wide, so a "
               "shift is only reproduced exactly when no wider intermediate "
               "could have been shifted");
      snprintf(d->fix, d->fix_cap,
               "mask the value down to the bits the shift needs: "
               "`(x & 65535) >> 8` bounds it inside int32 and the kernel then "
               "takes it. A constant multiplier is not enough on its own");
      ir_simd_diag(d, IR_SIMD_BAIL_UNBOUNDED_SHIFT);
      return 1;
    }
  }
  return 0;
}

static int ir_simd_bail_rule17(IrSimdBail *d) {
  {
    const char *base = NULL;
    if (ir_region_invariant_index_term(d->function, d->begin, d->end, &base)) {
      snprintf(d->reason, d->reason_cap,
               "the accesses into `%s` add a loop-invariant term to the "
               "counter (`%s[k + i]`); the kernels walk one base pointer from "
               "element 0, so the index must be the counter alone",
               base, base);
      snprintf(d->fix, d->fix_cap,
               "bind the row to a pointer before the loop (`var row = &%s[k];`) "
               "and index it with the counter (`row[i]`): same addresses, and "
               "the shape the kernels read",
               base);
      ir_simd_diag(d, IR_SIMD_BAIL_DOT_SHAPE_ADDRESS);
      return 1;
    }
  }
  return 0;
}
