#include "ir_verify_structure.h"

#include "../common.h"
#include "../compiler/compiler_crash.h"
#include "ir.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
  IR_STRUCT_MODE_UNSET = 0,
  IR_STRUCT_MODE_OFF,
  IR_STRUCT_MODE_CENSUS,
  IR_STRUCT_MODE_STRICT
} IRStructureMode;

static IRStructureMode g_structure_mode = IR_STRUCT_MODE_UNSET;
static size_t g_structure_violations = 0;
static size_t g_structure_regressions = 0;

static IRStructureMode ir_structure_mode(void) {
  if (g_structure_mode != IR_STRUCT_MODE_UNSET) {
    return g_structure_mode;
  }
  const char *setting = getenv("METTLE_IR_STRUCT");
  if (!setting || !*setting) {
    g_structure_mode = IR_STRUCT_MODE_OFF;
  } else if (strcmp(setting, "strict") == 0) {
    g_structure_mode = IR_STRUCT_MODE_STRICT;
  } else if (strcmp(setting, "census") == 0 || strcmp(setting, "1") == 0) {
    g_structure_mode = IR_STRUCT_MODE_CENSUS;
  } else {
    g_structure_mode = IR_STRUCT_MODE_OFF;
  }
  return g_structure_mode;
}

int ir_structure_enabled(void) {
  return ir_structure_mode() != IR_STRUCT_MODE_OFF;
}

size_t ir_structure_violation_count(void) { return g_structure_violations; }

size_t ir_structure_regression_count(void) { return g_structure_regressions; }

static int ir_structure_is_branch(const IRInstruction *instruction) {
  return instruction && (instruction->op == IR_OP_BRANCH_ZERO ||
                         instruction->op == IR_OP_BRANCH_EQ ||
                         instruction->op == IR_OP_JUMP);
}

static const IROperand *ir_structure_operand_at(const IRInstruction *instruction,
                                                size_t index) {
  switch (index) {
  case 0:
    return &instruction->dest;
  case 1:
    return &instruction->lhs;
  case 2:
    return &instruction->rhs;
  default:
    break;
  }
  const size_t argument = index - 3;
  if (!instruction->arguments || argument >= instruction->argument_count) {
    return NULL;
  }
  return &instruction->arguments[argument];
}

int ir_function_check_structure(const IRFunction *function,
                                IRStructureReport *report, char *why,
                                size_t why_capacity) {
  if (report) {
    memset(report, 0, sizeof(*report));
  }
  if (!function) {
    return 1;
  }
  if (why && why_capacity) {
    why[0] = '\0';
  }

  int ok = 1;
  IRValueTable labels;
  ir_value_table_init(&labels);

  for (size_t i = 0; i < function->instruction_count; i++) {
    const IRInstruction *instruction = &function->instructions[i];
    if (instruction->op != IR_OP_LABEL) {
      continue;
    }
    if (!instruction->text) {
      if (ok && why && why_capacity) {
        snprintf(why, why_capacity, "instruction %zu is a label with no name",
                 i);
      }
      ok = 0;
      if (report) {
        report->duplicate_labels++;
      }
      continue;
    }
    if (ir_value_table_lookup(&labels, (unsigned char)IR_OPERAND_LABEL,
                              instruction->text) != IR_VALUE_ID_NONE) {
      if (ok && why && why_capacity) {
        snprintf(why, why_capacity,
                 "label '%s' is defined more than once, again at instruction "
                 "%zu",
                 instruction->text, i);
      }
      ok = 0;
      if (report) {
        report->duplicate_labels++;
      }
      continue;
    }
    ir_value_table_intern(&labels, (unsigned char)IR_OPERAND_LABEL,
                          instruction->text);
  }

  const size_t value_capacity = ir_value_table_count(&function->values) + 1;
  size_t *defs = (size_t *)calloc(value_capacity, sizeof(size_t));

  for (size_t i = 0; i < function->instruction_count; i++) {
    const IRInstruction *instruction = &function->instructions[i];

    if (ir_structure_is_branch(instruction)) {
      if (!instruction->text) {
        if (ok && why && why_capacity) {
          snprintf(why, why_capacity, "instruction %zu branches to no target",
                   i);
        }
        ok = 0;
        if (report) {
          report->unresolved_targets++;
        }
      } else if (ir_value_table_lookup(&labels, (unsigned char)IR_OPERAND_LABEL,
                                       instruction->text) ==
                 IR_VALUE_ID_NONE) {
        if (ok && why && why_capacity) {
          snprintf(why, why_capacity,
                   "instruction %zu branches to '%s' which no label defines", i,
                   instruction->text);
        }
        ok = 0;
        if (report) {
          report->unresolved_targets++;
        }
      }
    }

    if (defs && ir_operand_is_value(&instruction->dest)) {
      const uint32_t id = instruction->dest.value_id;
      if (id != IR_VALUE_ID_NONE && id < value_capacity) {
        defs[id]++;
      }
    }

    if (report) {
      const size_t operands = 3 + instruction->argument_count;
      for (size_t j = 0; j < operands; j++) {
        const IROperand *operand = ir_structure_operand_at(instruction, j);
        if (operand && ir_operand_is_value(operand) &&
            operand->value_id == IR_VALUE_ID_NONE) {
          report->unnumbered_values++;
        }
      }
    }
  }

  if (report && defs) {
    for (uint32_t id = 1; id < (uint32_t)value_capacity; id++) {
      if (defs[id] == 0) {
        continue;
      }
      if (ir_value_table_kind(&function->values, id) == IR_OPERAND_TEMP) {
        report->temps++;
        if (defs[id] > 1) {
          report->temps_multi_def++;
        }
      } else {
        report->symbols++;
        if (defs[id] > 1) {
          report->symbols_multi_def++;
        }
      }
    }
  }

  free(defs);
  ir_value_table_clear(&labels);
  return ok;
}

static int g_structure_sabotage_fired = 0;

void ir_structure_maybe_sabotage(IRFunction *function, const char *pass_name) {
  if (g_structure_sabotage_fired || !function || !pass_name) {
    return;
  }
  const char *spec = getenv("METTLE_IR_STRUCT_BREAK");
  if (!spec || !*spec) {
    return;
  }
  const char *colon = strchr(spec, ':');
  const char *kind = spec;
  if (colon) {
    if (strncmp(spec, pass_name, (size_t)(colon - spec)) != 0 ||
        strlen(pass_name) != (size_t)(colon - spec)) {
      return;
    }
    kind = colon + 1;
  }

  if (strcmp(kind, "dangle") == 0) {
    for (size_t i = 0; i < function->instruction_count; i++) {
      IRInstruction *instruction = &function->instructions[i];
      if (!ir_structure_is_branch(instruction) || !instruction->text) {
        continue;
      }
      free(instruction->text);
      instruction->text = mettle_strdup("__structure_sabotage_target");
      g_structure_sabotage_fired = 1;
      return;
    }
    return;
  }

  if (strcmp(kind, "dup") == 0) {
    for (size_t i = 0; i < function->instruction_count; i++) {
      IRInstruction *instruction = &function->instructions[i];
      if (instruction->op != IR_OP_LABEL || !instruction->text) {
        continue;
      }
      for (size_t j = i + 1; j < function->instruction_count; j++) {
        IRInstruction *other = &function->instructions[j];
        if (other->op != IR_OP_LABEL || !other->text ||
            strcmp(other->text, instruction->text) == 0) {
          continue;
        }
        free(other->text);
        other->text = mettle_strdup(instruction->text);
        g_structure_sabotage_fired = 1;
        return;
      }
    }
    return;
  }

  if (strcmp(kind, "redef") == 0) {
    for (size_t i = 0; i + 1 < function->instruction_count; i++) {
      IRInstruction *instruction = &function->instructions[i];
      if (instruction->dest.kind != IR_OPERAND_TEMP || !instruction->dest.name) {
        continue;
      }
      for (size_t j = i + 1; j < function->instruction_count; j++) {
        IRInstruction *other = &function->instructions[j];
        if (other->dest.kind != IR_OPERAND_TEMP || !other->dest.name ||
            strcmp(other->dest.name, instruction->dest.name) == 0) {
          continue;
        }
        ir_operand_destroy(&other->dest);
        other->dest = ir_operand_temp(instruction->dest.name);
        other->dest.value_id = instruction->dest.value_id;
        g_structure_sabotage_fired = 1;
        return;
      }
    }
    return;
  }
}

size_t ir_structure_snapshot(const IRFunction *function) {
  if (!ir_structure_enabled() || !function) {
    return 0;
  }
  IRStructureReport report;
  ir_function_check_structure(function, &report, NULL, 0);
  return report.temps_multi_def + report.symbols_multi_def;
}

void ir_structure_check_after_pass(const IRFunction *function,
                                   const char *pass_name, size_t before) {
  const IRStructureMode mode = ir_structure_mode();
  if (mode == IR_STRUCT_MODE_OFF || !function) {
    return;
  }

  IRStructureReport report;
  char why[512];
  const int ok =
      ir_function_check_structure(function, &report, why, sizeof(why));
  const size_t after = report.temps_multi_def + report.symbols_multi_def;

  if (!ok) {
    g_structure_violations++;
    fprintf(stderr, "mettle: ir structure broken after pass '%s' in '%s': %s\n",
            pass_name ? pass_name : "<unnamed>",
            function->name ? function->name : "<unnamed>", why);
    if (mode == IR_STRUCT_MODE_STRICT) {
      char message[768];
      snprintf(message, sizeof(message),
               "optimization pass '%s' left '%s' structurally invalid: %s",
               pass_name ? pass_name : "<unnamed>",
               function->name ? function->name : "<unnamed>", why);
      mettle_compiler_ice(message);
    }
  }

  if (after > before) {
    g_structure_regressions++;
    fprintf(stderr,
            "mettle: pass '%s' in '%s' raised the values defined more than "
            "once from %zu to %zu\n",
            pass_name ? pass_name : "<unnamed>",
            function->name ? function->name : "<unnamed>", before, after);
    if (mode == IR_STRUCT_MODE_STRICT) {
      char message[768];
      snprintf(message, sizeof(message),
               "optimization pass '%s' raised the values defined more than "
               "once in '%s' from %zu to %zu",
               pass_name ? pass_name : "<unnamed>",
               function->name ? function->name : "<unnamed>", before, after);
      mettle_compiler_ice(message);
    }
  }
}
