#ifndef IR_ANALYSIS_H
#define IR_ANALYSIS_H

#include "ir_values.h"

#include <stddef.h>
#include <stdint.h>

#define IR_BLOCK_NONE ((size_t)-1)
#define IR_INSTRUCTION_NONE ((uint32_t)-1)

typedef struct {
  uint32_t instruction;
  uint32_t operand;
} IRValueUse;

typedef struct {
  size_t *idom;
  size_t *rpo_index;
  size_t *order;
  size_t order_count;
  size_t *child_head;
  size_t *child_next;
  size_t *enter;
  size_t *leave;
  size_t *frontier;
  size_t *frontier_start;
  size_t *frontier_count;
  size_t frontier_total;
  size_t block_count;
  size_t entry;
  int built;
} IRDomTree;

typedef struct {
  size_t value_count;
  uint32_t *def_first;
  size_t *def_count;
  uint32_t *defs;
  size_t def_total;
  IRValueUse *uses;
  size_t *use_start;
  size_t *use_count;
  size_t use_total;
  int built;
} IRUseDefs;

typedef struct {
  uint32_t *starts;
  uint32_t *counts;
  uint32_t *targets;
  size_t total;
  size_t label_count;
  int built;
} IRJumpIndex;

typedef struct {
  uint64_t generation;
  size_t instruction_count;
  size_t *instruction_block;
  IRDomTree dom;
  IRUseDefs ud;
  IRJumpIndex jumps;
  IRValueTable *labels;
  int valid;
} IRAnalysis;

#endif
