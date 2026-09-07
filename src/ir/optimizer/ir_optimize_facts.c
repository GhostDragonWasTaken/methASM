#include "ir_optimize_internal.h"
#include "../../common.h"

#include <string.h>

int ir_facts_build(IRFacts *facts, const IRFunction *function) {
  if (!facts || !function) {
    return 0;
  }
  memset(facts, 0, sizeof(*facts));
  if (!ir_name_index_init(&facts->symbol_defs, function->instruction_count) ||
      !ir_name_index_init(&facts->symbol_first_def,
                          function->instruction_count) ||
      !ir_name_index_init(&facts->temp_defs, function->instruction_count) ||
      !ir_name_index_init(&facts->temp_first_def,
                          function->instruction_count)) {
    ir_facts_destroy(facts);
    return 0;
  }
  for (size_t i = 0; i < function->instruction_count; i++) {
    const IRInstruction *ins = &function->instructions[i];
    const IROperand *dest = &ins->dest;
    IRNameIndex *counts;
    IRNameIndex *first;
    size_t seen = 0;
    if (ins->op == IR_OP_DECLARE_LOCAL || !ir_instruction_writes_destination(ins) ||
        !dest->name) {
      continue;
    }
    if (dest->kind == IR_OPERAND_SYMBOL) {
      counts = &facts->symbol_defs;
      first = &facts->symbol_first_def;
    } else if (dest->kind == IR_OPERAND_TEMP) {
      counts = &facts->temp_defs;
      first = &facts->temp_first_def;
    } else {
      continue;
    }
    ir_name_index_add(counts, dest->name, 1);
    if (!ir_name_index_find(first, dest->name, &seen)) {
      ir_name_index_insert(first, dest->name, i);
    }
  }
  facts->instruction_count = function->instruction_count;
  facts->valid = 1;
  return 1;
}

void ir_facts_destroy(IRFacts *facts) {
  if (!facts) {
    return;
  }
  ir_name_index_destroy(&facts->symbol_defs);
  ir_name_index_destroy(&facts->symbol_first_def);
  ir_name_index_destroy(&facts->temp_defs);
  ir_name_index_destroy(&facts->temp_first_def);
  facts->valid = 0;
  facts->instruction_count = 0;
}

size_t ir_facts_symbol_def_count(const IRFacts *facts, const char *name) {
  size_t count = 0;
  if (!facts || !facts->valid || !name) {
    return 0;
  }
  return ir_name_index_find(&facts->symbol_defs, name, &count) ? count : 0;
}

size_t ir_facts_temp_def_count(const IRFacts *facts, const char *name) {
  size_t count = 0;
  if (!facts || !facts->valid || !name) {
    return 0;
  }
  return ir_name_index_find(&facts->temp_defs, name, &count) ? count : 0;
}

int ir_facts_symbol_single_def(const IRFacts *facts, const char *name,
                               size_t *at) {
  size_t where = 0;
  if (ir_facts_symbol_def_count(facts, name) != 1) {
    return 0;
  }
  if (!ir_name_index_find(&facts->symbol_first_def, name, &where)) {
    return 0;
  }
  if (at) {
    *at = where;
  }
  return 1;
}

int ir_facts_temp_single_def(const IRFacts *facts, const char *name,
                             size_t *at) {
  size_t where = 0;
  if (ir_facts_temp_def_count(facts, name) != 1) {
    return 0;
  }
  if (!ir_name_index_find(&facts->temp_first_def, name, &where)) {
    return 0;
  }
  if (at) {
    *at = where;
  }
  return 1;
}

int ir_facts_matches(const IRFacts *facts, const IRFunction *function) {
  return facts && function && facts->valid &&
         facts->instruction_count == function->instruction_count;
}

static IRFacts g_facts;
static const IRFunction *g_facts_function;
static unsigned long long g_facts_generation;
static unsigned long long g_facts_built_generation;

void ir_facts_invalidate(void) { g_facts_generation++; }

void ir_facts_release(void) {
  ir_facts_destroy(&g_facts);
  g_facts_function = NULL;
}

const IRFacts *ir_facts_of(const IRFunction *function) {
  if (!function) {
    return NULL;
  }
  if (g_facts_function == function &&
      g_facts_built_generation == g_facts_generation &&
      ir_facts_matches(&g_facts, function)) {
    return &g_facts;
  }
  ir_facts_destroy(&g_facts);
  if (!ir_facts_build(&g_facts, function)) {
    g_facts_function = NULL;
    return NULL;
  }
  g_facts_function = function;
  g_facts_built_generation = g_facts_generation;
  return &g_facts;
}
