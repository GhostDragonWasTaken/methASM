#include "ir_analysis.h"

#include "ir.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void ir_dom_destroy(IRDomTree *dom) {
  if (!dom) {
    return;
  }
  free(dom->idom);
  free(dom->rpo_index);
  free(dom->order);
  free(dom->child_head);
  free(dom->child_next);
  free(dom->enter);
  free(dom->leave);
  free(dom->frontier);
  free(dom->frontier_start);
  free(dom->frontier_count);
  memset(dom, 0, sizeof(*dom));
}

static void ir_use_defs_destroy(IRUseDefs *ud) {
  if (!ud) {
    return;
  }
  free(ud->def_first);
  free(ud->def_count);
  free(ud->defs);
  free(ud->uses);
  free(ud->use_start);
  free(ud->use_count);
  memset(ud, 0, sizeof(*ud));
}

static void ir_analysis_destroy(IRAnalysis *analysis) {
  if (!analysis) {
    return;
  }
  ir_dom_destroy(&analysis->dom);
  ir_use_defs_destroy(&analysis->ud);
  free(analysis->instruction_block);
  analysis->instruction_block = NULL;
  analysis->instruction_count = 0;
  analysis->valid = 0;
}

void ir_function_release_analysis(IRFunction *function) {
  if (!function || !function->analysis) {
    return;
  }
  ir_analysis_destroy((IRAnalysis *)function->analysis);
  free(function->analysis);
  function->analysis = NULL;
}

static int ir_dom_build(IRDomTree *dom, const IRBasicBlock *blocks,
                        size_t block_count, size_t entry) {
  memset(dom, 0, sizeof(*dom));
  if (block_count == 0 || entry >= block_count) {
    return 0;
  }

  dom->block_count = block_count;
  dom->entry = entry;
  dom->idom = (size_t *)malloc(block_count * sizeof(size_t));
  dom->rpo_index = (size_t *)malloc(block_count * sizeof(size_t));
  dom->order = (size_t *)malloc(block_count * sizeof(size_t));
  dom->child_head = (size_t *)malloc(block_count * sizeof(size_t));
  dom->child_next = (size_t *)malloc(block_count * sizeof(size_t));
  dom->enter = (size_t *)malloc(block_count * sizeof(size_t));
  dom->leave = (size_t *)malloc(block_count * sizeof(size_t));
  size_t *stack = (size_t *)malloc(block_count * sizeof(size_t));
  size_t *next_succ = (size_t *)malloc(block_count * sizeof(size_t));
  unsigned char *seen = (unsigned char *)calloc(block_count, 1);
  size_t *postorder = (size_t *)malloc(block_count * sizeof(size_t));

  if (!dom->idom || !dom->rpo_index || !dom->order || !dom->child_head ||
      !dom->child_next || !dom->enter || !dom->leave || !stack || !next_succ ||
      !seen || !postorder) {
    free(stack);
    free(next_succ);
    free(seen);
    free(postorder);
    ir_dom_destroy(dom);
    return 0;
  }

  for (size_t i = 0; i < block_count; i++) {
    dom->idom[i] = IR_BLOCK_NONE;
    dom->rpo_index[i] = IR_BLOCK_NONE;
    dom->child_head[i] = IR_BLOCK_NONE;
    dom->child_next[i] = IR_BLOCK_NONE;
    dom->enter[i] = 0;
    dom->leave[i] = 0;
  }

  size_t post_count = 0;
  size_t depth = 0;
  stack[0] = entry;
  next_succ[0] = 0;
  seen[entry] = 1;
  depth = 1;
  while (depth > 0) {
    const size_t block = stack[depth - 1];
    if (next_succ[depth - 1] < blocks[block].successor_count) {
      const size_t successor = blocks[block].successors[next_succ[depth - 1]++];
      if (successor < block_count && !seen[successor]) {
        seen[successor] = 1;
        stack[depth] = successor;
        next_succ[depth] = 0;
        depth++;
      }
      continue;
    }
    postorder[post_count++] = block;
    depth--;
  }

  dom->order_count = post_count;
  for (size_t i = 0; i < post_count; i++) {
    dom->order[i] = postorder[post_count - 1 - i];
    dom->rpo_index[dom->order[i]] = i;
  }
  free(postorder);
  free(next_succ);
  free(seen);

  dom->idom[entry] = entry;
  int changed = 1;
  while (changed) {
    changed = 0;
    for (size_t k = 0; k < dom->order_count; k++) {
      const size_t block = dom->order[k];
      if (block == entry) {
        continue;
      }
      size_t candidate = IR_BLOCK_NONE;
      for (size_t p = 0; p < blocks[block].predecessor_count; p++) {
        const size_t pred = blocks[block].predecessors[p];
        if (pred >= block_count || dom->idom[pred] == IR_BLOCK_NONE) {
          continue;
        }
        if (candidate == IR_BLOCK_NONE) {
          candidate = pred;
          continue;
        }
        size_t a = pred;
        size_t b = candidate;
        while (a != b) {
          while (dom->rpo_index[a] > dom->rpo_index[b]) {
            a = dom->idom[a];
          }
          while (dom->rpo_index[b] > dom->rpo_index[a]) {
            b = dom->idom[b];
          }
        }
        candidate = a;
      }
      if (candidate != IR_BLOCK_NONE && dom->idom[block] != candidate) {
        dom->idom[block] = candidate;
        changed = 1;
      }
    }
  }

  for (size_t k = dom->order_count; k-- > 0;) {
    const size_t block = dom->order[k];
    if (block == entry || dom->idom[block] == IR_BLOCK_NONE) {
      continue;
    }
    const size_t parent = dom->idom[block];
    dom->child_next[block] = dom->child_head[parent];
    dom->child_head[parent] = block;
  }

  size_t clock = 0;
  size_t sp = 0;
  stack[sp] = entry;
  size_t *child_cursor = (size_t *)malloc(block_count * sizeof(size_t));
  if (!child_cursor) {
    free(stack);
    ir_dom_destroy(dom);
    return 0;
  }
  for (size_t i = 0; i < block_count; i++) {
    child_cursor[i] = dom->child_head[i];
  }
  dom->enter[entry] = clock++;
  sp = 1;
  while (sp > 0) {
    const size_t block = stack[sp - 1];
    if (child_cursor[block] != IR_BLOCK_NONE) {
      const size_t child = child_cursor[block];
      child_cursor[block] = dom->child_next[child];
      dom->enter[child] = clock++;
      stack[sp++] = child;
      continue;
    }
    dom->leave[block] = clock++;
    sp--;
  }
  free(child_cursor);
  free(stack);

  size_t *counts = (size_t *)calloc(block_count, sizeof(size_t));
  if (!counts) {
    ir_dom_destroy(dom);
    return 0;
  }
  for (size_t block = 0; block < block_count; block++) {
    if (blocks[block].predecessor_count < 2) {
      continue;
    }
    const size_t stop = dom->idom[block];
    for (size_t p = 0; p < blocks[block].predecessor_count; p++) {
      size_t runner = blocks[block].predecessors[p];
      if (runner >= block_count || dom->idom[runner] == IR_BLOCK_NONE) {
        continue;
      }
      while (runner != stop && runner != IR_BLOCK_NONE) {
        counts[runner]++;
        if (runner == dom->idom[runner]) {
          break;
        }
        runner = dom->idom[runner];
      }
    }
  }

  dom->frontier_start = (size_t *)malloc(block_count * sizeof(size_t));
  dom->frontier_count = (size_t *)calloc(block_count, sizeof(size_t));
  if (!dom->frontier_start || !dom->frontier_count) {
    free(counts);
    ir_dom_destroy(dom);
    return 0;
  }
  size_t total = 0;
  for (size_t i = 0; i < block_count; i++) {
    dom->frontier_start[i] = total;
    total += counts[i];
  }
  dom->frontier_total = total;
  dom->frontier = total ? (size_t *)malloc(total * sizeof(size_t)) : NULL;
  if (total && !dom->frontier) {
    free(counts);
    ir_dom_destroy(dom);
    return 0;
  }

  for (size_t block = 0; block < block_count; block++) {
    if (blocks[block].predecessor_count < 2) {
      continue;
    }
    const size_t stop = dom->idom[block];
    for (size_t p = 0; p < blocks[block].predecessor_count; p++) {
      size_t runner = blocks[block].predecessors[p];
      if (runner >= block_count || dom->idom[runner] == IR_BLOCK_NONE) {
        continue;
      }
      while (runner != stop && runner != IR_BLOCK_NONE) {
        int present = 0;
        const size_t start = dom->frontier_start[runner];
        for (size_t k = 0; k < dom->frontier_count[runner]; k++) {
          if (dom->frontier[start + k] == block) {
            present = 1;
            break;
          }
        }
        if (!present) {
          dom->frontier[start + dom->frontier_count[runner]] = block;
          dom->frontier_count[runner]++;
        }
        if (runner == dom->idom[runner]) {
          break;
        }
        runner = dom->idom[runner];
      }
    }
  }

  free(counts);
  dom->built = 1;
  return 1;
}

static const IROperand *ir_analysis_operand_at(const IRInstruction *instruction,
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

static int ir_use_defs_build(IRUseDefs *ud, const IRFunction *function) {
  memset(ud, 0, sizeof(*ud));
  const size_t value_count = ir_value_table_count(&function->values) + 1;
  ud->value_count = value_count;
  ud->def_first = (uint32_t *)malloc(value_count * sizeof(uint32_t));
  ud->def_count = (size_t *)calloc(value_count, sizeof(size_t));
  ud->use_start = (size_t *)calloc(value_count, sizeof(size_t));
  ud->use_count = (size_t *)calloc(value_count, sizeof(size_t));
  if (!ud->def_first || !ud->def_count || !ud->use_start || !ud->use_count) {
    ir_use_defs_destroy(ud);
    return 0;
  }
  for (size_t i = 0; i < value_count; i++) {
    ud->def_first[i] = IR_INSTRUCTION_NONE;
  }

  size_t def_total = 0;
  size_t use_total = 0;
  for (size_t i = 0; i < function->instruction_count; i++) {
    const IRInstruction *instruction = &function->instructions[i];
    const int writes = ir_instruction_writes_destination(instruction);
    if (writes && ir_operand_is_value(&instruction->dest) &&
        instruction->dest.value_id != IR_VALUE_ID_NONE &&
        instruction->dest.value_id < value_count) {
      ud->def_count[instruction->dest.value_id]++;
      def_total++;
    }
    const size_t operands = 3 + instruction->argument_count;
    for (size_t j = writes ? 1 : 0; j < operands; j++) {
      const IROperand *operand = ir_analysis_operand_at(instruction, j);
      if (!operand || !ir_operand_is_value(operand) ||
          operand->value_id == IR_VALUE_ID_NONE ||
          operand->value_id >= value_count) {
        continue;
      }
      ud->use_count[operand->value_id]++;
      use_total++;
    }
  }

  ud->def_total = def_total;
  ud->use_total = use_total;
  ud->defs = def_total ? (uint32_t *)malloc(def_total * sizeof(uint32_t)) : NULL;
  ud->uses = use_total ? (IRValueUse *)malloc(use_total * sizeof(IRValueUse))
                       : NULL;
  if ((def_total && !ud->defs) || (use_total && !ud->uses)) {
    ir_use_defs_destroy(ud);
    return 0;
  }

  size_t *def_start = (size_t *)calloc(value_count, sizeof(size_t));
  if (!def_start) {
    ir_use_defs_destroy(ud);
    return 0;
  }
  size_t running = 0;
  for (size_t i = 0; i < value_count; i++) {
    def_start[i] = running;
    running += ud->def_count[i];
  }
  running = 0;
  for (size_t i = 0; i < value_count; i++) {
    ud->use_start[i] = running;
    running += ud->use_count[i];
  }

  size_t *def_fill = (size_t *)calloc(value_count, sizeof(size_t));
  size_t *use_fill = (size_t *)calloc(value_count, sizeof(size_t));
  if (!def_fill || !use_fill) {
    free(def_start);
    free(def_fill);
    free(use_fill);
    ir_use_defs_destroy(ud);
    return 0;
  }

  for (size_t i = 0; i < function->instruction_count; i++) {
    const IRInstruction *instruction = &function->instructions[i];
    const int writes = ir_instruction_writes_destination(instruction);
    if (writes && ir_operand_is_value(&instruction->dest) &&
        instruction->dest.value_id != IR_VALUE_ID_NONE &&
        instruction->dest.value_id < value_count) {
      const uint32_t id = instruction->dest.value_id;
      ud->defs[def_start[id] + def_fill[id]] = (uint32_t)i;
      if (def_fill[id] == 0) {
        ud->def_first[id] = (uint32_t)i;
      }
      def_fill[id]++;
    }
    const size_t operands = 3 + instruction->argument_count;
    for (size_t j = writes ? 1 : 0; j < operands; j++) {
      const IROperand *operand = ir_analysis_operand_at(instruction, j);
      if (!operand || !ir_operand_is_value(operand) ||
          operand->value_id == IR_VALUE_ID_NONE ||
          operand->value_id >= value_count) {
        continue;
      }
      const uint32_t id = operand->value_id;
      IRValueUse *slot = &ud->uses[ud->use_start[id] + use_fill[id]];
      slot->instruction = (uint32_t)i;
      slot->operand = (uint32_t)j;
      use_fill[id]++;
    }
  }

  for (size_t i = 0; i < value_count; i++) {
    ud->def_count[i] = def_fill[i];
  }
  free(def_start);
  free(def_fill);
  free(use_fill);
  ud->built = 1;
  return 1;
}

const IRAnalysis *ir_function_analysis(IRFunction *function) {
  if (!function) {
    return NULL;
  }

  size_t block_count = 0;
  const IRBasicBlock *blocks = ir_function_blocks(function, &block_count);

  IRAnalysis *analysis = (IRAnalysis *)function->analysis;
  if (analysis && analysis->valid &&
      analysis->generation == function->generation &&
      analysis->instruction_count == function->instruction_count &&
      analysis->dom.block_count == block_count) {
    return analysis;
  }

  if (!analysis) {
    analysis = (IRAnalysis *)calloc(1, sizeof(IRAnalysis));
    if (!analysis) {
      return NULL;
    }
    function->analysis = analysis;
  } else {
    ir_analysis_destroy(analysis);
  }

  if (!blocks || block_count == 0) {
    analysis->generation = function->generation;
    analysis->instruction_count = function->instruction_count;
    analysis->valid = 1;
    return analysis;
  }

  analysis->instruction_block =
      (size_t *)malloc(function->instruction_count * sizeof(size_t));
  if (analysis->instruction_block) {
    for (size_t i = 0; i < function->instruction_count; i++) {
      analysis->instruction_block[i] = IR_BLOCK_NONE;
    }
    for (size_t b = 0; b < block_count; b++) {
      const size_t start = blocks[b].first_instruction;
      for (size_t k = 0; k < blocks[b].instruction_count; k++) {
        if (start + k < function->instruction_count) {
          analysis->instruction_block[start + k] = b;
        }
      }
    }
  }

  ir_dom_build(&analysis->dom, blocks, block_count, function->entry_block);
  ir_use_defs_build(&analysis->ud, function);

  analysis->generation = function->generation;
  analysis->instruction_count = function->instruction_count;
  analysis->valid = 1;
  return analysis;
}

size_t ir_function_instruction_block(IRFunction *function, size_t index) {
  const IRAnalysis *analysis = ir_function_analysis(function);
  if (!analysis || !analysis->instruction_block ||
      index >= analysis->instruction_count) {
    return IR_BLOCK_NONE;
  }
  return analysis->instruction_block[index];
}

int ir_block_dominates(const IRAnalysis *analysis, size_t a, size_t b) {
  if (!analysis || !analysis->dom.built) {
    return 0;
  }
  const IRDomTree *dom = &analysis->dom;
  if (a >= dom->block_count || b >= dom->block_count) {
    return 0;
  }
  if (dom->rpo_index[a] == IR_BLOCK_NONE || dom->rpo_index[b] == IR_BLOCK_NONE) {
    return 0;
  }
  return dom->enter[a] <= dom->enter[b] && dom->leave[b] <= dom->leave[a];
}

int ir_function_block_dominates(IRFunction *function, size_t a, size_t b) {
  return ir_block_dominates(ir_function_analysis(function), a, b);
}

int ir_function_instruction_dominates(IRFunction *function, size_t a,
                                      size_t b) {
  const IRAnalysis *analysis = ir_function_analysis(function);
  if (!analysis || !analysis->instruction_block) {
    return 0;
  }
  if (a >= analysis->instruction_count || b >= analysis->instruction_count) {
    return 0;
  }
  const size_t block_a = analysis->instruction_block[a];
  const size_t block_b = analysis->instruction_block[b];
  if (block_a == IR_BLOCK_NONE || block_b == IR_BLOCK_NONE) {
    return 0;
  }
  if (block_a == block_b) {
    return a <= b;
  }
  return ir_block_dominates(analysis, block_a, block_b);
}

uint32_t ir_function_value_single_def(IRFunction *function, uint32_t id) {
  const IRAnalysis *analysis = ir_function_analysis(function);
  if (!analysis || !analysis->ud.built || id == IR_VALUE_ID_NONE ||
      id >= analysis->ud.value_count) {
    return IR_INSTRUCTION_NONE;
  }
  if (analysis->ud.def_count[id] != 1) {
    return IR_INSTRUCTION_NONE;
  }
  return analysis->ud.def_first[id];
}

const IRValueUse *ir_function_value_uses(IRFunction *function, uint32_t id,
                                         size_t *count_out) {
  const IRAnalysis *analysis = ir_function_analysis(function);
  if (count_out) {
    *count_out = 0;
  }
  if (!analysis || !analysis->ud.built || id == IR_VALUE_ID_NONE ||
      id >= analysis->ud.value_count) {
    return NULL;
  }
  if (count_out) {
    *count_out = analysis->ud.use_count[id];
  }
  if (analysis->ud.use_count[id] == 0 || !analysis->ud.uses) {
    return NULL;
  }
  return &analysis->ud.uses[analysis->ud.use_start[id]];
}

size_t ir_function_value_def_count(IRFunction *function, uint32_t id) {
  const IRAnalysis *analysis = ir_function_analysis(function);
  if (!analysis || !analysis->ud.built || id == IR_VALUE_ID_NONE ||
      id >= analysis->ud.value_count) {
    return 0;
  }
  return analysis->ud.def_count[id];
}

static const size_t *ir_function_dominance_frontier_fwd(IRFunction *function,
                                                        size_t block,
                                                        size_t *count_out);

size_t ir_analysis_self_check(IRFunction *function) {
  const char *setting = getenv("METTLE_DOM_CROSSCHECK");
  if (!setting || !*setting) {
    return 0;
  }
  const IRAnalysis *analysis = ir_function_analysis(function);
  if (!analysis || !analysis->dom.built) {
    return 0;
  }
  const IRDomTree *dom = &analysis->dom;
  size_t mismatches = 0;

  for (size_t b = 0; b < dom->block_count; b++) {
    if (dom->rpo_index[b] == IR_BLOCK_NONE) {
      continue;
    }
    for (size_t a = 0; a < dom->block_count; a++) {
      if (dom->rpo_index[a] == IR_BLOCK_NONE) {
        continue;
      }
      int walked = 0;
      size_t runner = b;
      while (1) {
        if (runner == a) {
          walked = 1;
          break;
        }
        if (dom->idom[runner] == IR_BLOCK_NONE ||
            dom->idom[runner] == runner) {
          break;
        }
        runner = dom->idom[runner];
      }
      if (walked != ir_block_dominates(analysis, a, b)) {
        mismatches++;
        if (mismatches <= 4) {
          fprintf(stderr,
                  "mettle: dominance self-check says block %zu %s block %zu in "
                  "'%s' while the parent walk disagrees\n",
                  a, ir_block_dominates(analysis, a, b) ? "dominates" : "misses",
                  b, function->name ? function->name : "<unnamed>");
        }
      }
    }
  }

  for (size_t b = 0; b < dom->block_count; b++) {
    size_t count = 0;
    const size_t *frontier =
        ir_function_dominance_frontier_fwd(function, b, &count);
    for (size_t k = 0; k < count; k++) {
      const size_t f = frontier[k];
      if (f == b) {
        continue;
      }
      if (ir_block_dominates(analysis, b, f) && dom->idom[f] != b) {
        mismatches++;
        fprintf(stderr,
                "mettle: dominance self-check has block %zu strictly "
                "dominating its own frontier member %zu in '%s'\n",
                b, f, function->name ? function->name : "<unnamed>");
      }
    }
  }

  if (mismatches == 0 && strcmp(setting, "verbose") == 0) {
    fprintf(stderr,
            "mettle: dominance self-check agrees on %zu blocks in '%s'\n",
            dom->block_count, function->name ? function->name : "<unnamed>");
  }
  return mismatches;
}

static const size_t *ir_function_dominance_frontier_fwd(IRFunction *function,
                                                        size_t block,
                                                        size_t *count_out) {
  return ir_function_dominance_frontier(function, block, count_out);
}

const size_t *ir_function_dominance_frontier(IRFunction *function, size_t block,
                                             size_t *count_out) {
  const IRAnalysis *analysis = ir_function_analysis(function);
  if (count_out) {
    *count_out = 0;
  }
  if (!analysis || !analysis->dom.built || block >= analysis->dom.block_count ||
      !analysis->dom.frontier) {
    return NULL;
  }
  if (count_out) {
    *count_out = analysis->dom.frontier_count[block];
  }
  if (analysis->dom.frontier_count[block] == 0) {
    return NULL;
  }
  return &analysis->dom.frontier[analysis->dom.frontier_start[block]];
}
