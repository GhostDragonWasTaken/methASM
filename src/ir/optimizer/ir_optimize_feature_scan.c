#include "ir_optimize_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned ir_opt_feature_flags(const IROptFunctionFeatures *features) {
  unsigned flags = 0;
  if (features->has_label) {
    flags |= IR_OPT_FEATURE_LABEL;
  }
  if (features->has_while_label) {
    flags |= IR_OPT_FEATURE_WHILE_LABEL;
  }
  if (features->has_jump) {
    flags |= IR_OPT_FEATURE_JUMP;
  }
  if (features->has_branch_zero) {
    flags |= IR_OPT_FEATURE_BRANCH_ZERO;
  }
  if (features->has_branch_eq) {
    flags |= IR_OPT_FEATURE_BRANCH_EQ;
  }
  if (features->has_call) {
    flags |= IR_OPT_FEATURE_CALL;
  }
  if (features->has_load) {
    flags |= IR_OPT_FEATURE_LOAD;
  }
  if (features->has_assign) {
    flags |= IR_OPT_FEATURE_ASSIGN;
  }
  if (features->has_temp_write) {
    flags |= IR_OPT_FEATURE_TEMP_WRITE;
  }
  if (features->has_binary) {
    flags |= IR_OPT_FEATURE_BINARY;
  }
  if (features->has_div) {
    flags |= IR_OPT_FEATURE_DIV;
  }
  return flags;
}

void ir_collect_function_features(const IRFunction *function,
                                         IROptFunctionFeatures *features) {
  if (!features) {
    return;
  }

  memset(features, 0, sizeof(*features));
  if (!function) {
    return;
  }

  unsigned seen = 0;
  for (size_t i = 0; i < function->instruction_count; i++) {
    const IRInstruction *instruction = &function->instructions[i];
    if (seen == IR_OPT_FEATURE_ALL) {
      break;
    }
    switch (instruction->op) {
    case IR_OP_LABEL:
      features->has_label = 1;
      seen |= IR_OPT_FEATURE_LABEL;
      if (instruction->text &&
          strncmp(instruction->text, "ir_while_", 9) == 0) {
        features->has_while_label = 1;
        seen |= IR_OPT_FEATURE_WHILE_LABEL;
      }
      break;
    case IR_OP_JUMP:
      features->has_jump = 1;
      seen |= IR_OPT_FEATURE_JUMP;
      break;
    case IR_OP_BRANCH_ZERO:
      features->has_branch_zero = 1;
      seen |= IR_OPT_FEATURE_BRANCH_ZERO;
      break;
    case IR_OP_BRANCH_EQ:
      features->has_branch_eq = 1;
      seen |= IR_OPT_FEATURE_BRANCH_EQ;
      break;
    case IR_OP_CALL:
    case IR_OP_CALL_INDIRECT:
      features->has_call = 1;
      seen |= IR_OPT_FEATURE_CALL;
      break;
    case IR_OP_LOAD:
      features->has_load = 1;
      seen |= IR_OPT_FEATURE_LOAD;
      break;
    case IR_OP_ASSIGN:
      features->has_assign = 1;
      seen |= IR_OPT_FEATURE_ASSIGN;
      break;
    case IR_OP_BINARY:
      features->has_binary = 1;
      seen |= IR_OPT_FEATURE_BINARY;
      if (instruction->text && strcmp(instruction->text, "/") == 0) {
        features->has_div = 1;
        seen |= IR_OPT_FEATURE_DIV;
      }
      break;
    default:
      break;
    }

    if (!(seen & IR_OPT_FEATURE_TEMP_WRITE) &&
        ir_instruction_writes_temp(instruction)) {
      features->has_temp_write = 1;
      seen |= IR_OPT_FEATURE_TEMP_WRITE;
    }
  }
}

static const IRInstruction *ir_scan_temp_producer_before(
    const IRFunction *function, size_t before_index, const char *temp_name) {
  for (size_t i = before_index; i > 0;) {
    i--;
    const IRInstruction *instruction = &function->instructions[i];
    if (instruction->op == IR_OP_NOP) {
      continue;
    }
    if (instruction->dest.kind == IR_OPERAND_TEMP && instruction->dest.name &&
        strcmp(instruction->dest.name, temp_name) == 0) {
      return instruction;
    }
    if (instruction->op == IR_OP_LABEL) {
      break;
    }
  }
  return NULL;
}

static int ir_producer_index_mode(void) {
  static int cached = -1;
  if (cached < 0) {
    const char *setting = getenv("METTLE_PRODUCER_INDEX");
    if (!setting || !*setting) {
      cached = 0;
    } else if (strcmp(setting, "index") == 0) {
      cached = 1;
    } else if (strcmp(setting, "verify") == 0) {
      cached = 2;
    } else {
      cached = 1;
    }
  }
  return cached;
}

const IRInstruction *ir_find_temp_producer_before(const IRFunction *function,
                                                  size_t before_index,
                                                  const char *temp_name) {
  if (!function || !temp_name) {
    return NULL;
  }

  const int mode = ir_producer_index_mode();
  if (mode == 0) {
    return ir_scan_temp_producer_before(function, before_index, temp_name);
  }

  int usable = 0;
  const IRInstruction *fast =
      ir_function_temp_producer_before(function, before_index, temp_name,
                                       &usable);
  if (!usable) {
    return ir_scan_temp_producer_before(function, before_index, temp_name);
  }

  if (mode == 2) {
    const IRInstruction *slow =
        ir_scan_temp_producer_before(function, before_index, temp_name);
    if (slow != fast) {
      fprintf(stderr,
              "mettle: producer index disagrees for '%s' before %zu in '%s'\n",
              temp_name, before_index,
              function->name ? function->name : "<unnamed>");
      return slow;
    }
  }
  return fast;
}

