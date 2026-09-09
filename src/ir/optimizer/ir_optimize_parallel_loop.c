#include "../ir.h"
#include "ir_optimize_internal.h"
#include "../../common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IR_PARALLEL_WORK_TARGET 4096
#define IR_PARALLEL_MAX_SLOTS 16u
#define IR_PARALLEL_RANGE_SYMBOL "mettle_parallel_range"
#define IR_PARALLEL_SLOTS_SYMBOL "mettle_parallel_range_slots"

typedef struct {
  char **names;
  unsigned char *defined;
  unsigned char *used;
  unsigned char *addressed;
  unsigned char *other_use;
  unsigned char *private_ok;
  size_t count;
  size_t capacity;
} IRParNames;

typedef struct {
  const char *name;
  const char *type_name;
  int bytes;
  int is_float;
  int float_bits;
  int is_unsigned;
  int take_address;
  int is_temp;
} IRParCapture;

typedef struct {
  const char *name;
  const char *op;
  IRParCapture shape;
} IRParReduction;

typedef struct {
  IRProgram *program;
  IRFunction *function;
  size_t marker;
  size_t header;
  size_t compare;
  size_t branch;
  size_t latch;
  size_t after;
  const char *start_label;
  const char *end_label;
  const char *iv;
  const char *iv_type;
  IROperand bound;
  int bound_inclusive;
  IRParNames names;
  IRParCapture *captures;
  size_t capture_count;
  IRParReduction *reductions;
  size_t reduction_count;
  SourceLocation location;
} IRParLoop;

static void ir_par_reject(const IRParLoop *loop, const char *reason,
                          const char *detail) {
  const SourceLocation *at = &loop->location;
  const char *file = at->filename ? at->filename : "<input>";
  if (detail) {
    fprintf(stderr, "%s:%zu:%zu: error: '@parallel' cannot run this loop "
                    "across threads: %s `%s`\n",
            file, at->line, at->column, reason, detail);
  } else {
    fprintf(stderr, "%s:%zu:%zu: error: '@parallel' cannot run this loop "
                    "across threads: %s\n",
            file, at->line, at->column, reason);
  }
  ir_optimize_note_user_error();
}

static void ir_par_names_free(IRParNames *set) {
  free(set->names);
  free(set->defined);
  free(set->used);
  free(set->addressed);
  free(set->other_use);
  free(set->private_ok);
  memset(set, 0, sizeof(*set));
}

static long ir_par_names_find(const IRParNames *set, const char *name) {
  for (size_t i = 0; i < set->count; i++) {
    if (strcmp(set->names[i], name) == 0) {
      return (long)i;
    }
  }
  return -1;
}

static long ir_par_names_add(IRParNames *set, const char *name) {
  long found = ir_par_names_find(set, name);
  if (found >= 0) {
    return found;
  }
  if (set->count == set->capacity) {
    size_t grown = set->capacity ? set->capacity * 2u : 32u;
    char **names = realloc(set->names, grown * sizeof(*names));
    unsigned char *defined = realloc(set->defined, grown);
    unsigned char *used = realloc(set->used, grown);
    unsigned char *addressed = realloc(set->addressed, grown);
    unsigned char *other = realloc(set->other_use, grown);
    unsigned char *ok = realloc(set->private_ok, grown);
    if (names) set->names = names;
    if (defined) set->defined = defined;
    if (used) set->used = used;
    if (addressed) set->addressed = addressed;
    if (other) set->other_use = other;
    if (ok) set->private_ok = ok;
    if (!names || !defined || !used || !addressed || !other || !ok) {
      return -1;
    }
    set->capacity = grown;
  }
  set->names[set->count] = (char *)name;
  set->defined[set->count] = 0;
  set->used[set->count] = 0;
  set->addressed[set->count] = 0;
  set->other_use[set->count] = 0;
  set->private_ok[set->count] = 0;
  set->count++;
  return (long)(set->count - 1u);
}

static int ir_par_operand_is_value(const IROperand *operand) {
  return operand && operand->name &&
         (operand->kind == IR_OPERAND_TEMP ||
          operand->kind == IR_OPERAND_SYMBOL);
}

static int ir_par_is_trap_call(const IRInstruction *in) {
  return in->op == IR_OP_CALL && in->text &&
         strncmp(in->text, "mettle_crash_trap", 17) == 0 &&
         !ir_par_operand_is_value(&in->dest);
}

static int ir_par_opcode_supported(IROpcode op) {
  switch (op) {
  case IR_OP_NOP:
  case IR_OP_LABEL:
  case IR_OP_JUMP:
  case IR_OP_BRANCH_ZERO:
  case IR_OP_BRANCH_EQ:
  case IR_OP_DECLARE_LOCAL:
  case IR_OP_ASSIGN:
  case IR_OP_ADDRESS_OF:
  case IR_OP_LOAD:
  case IR_OP_STORE:
  case IR_OP_BINARY:
  case IR_OP_UNARY:
  case IR_OP_ROTATE_ADD:
  case IR_OP_CAST:
  case IR_OP_SELECT:
  case IR_OP_PREFETCH:
    return 1;
  default:
    return 0;
  }
}

static const IROperand *ir_par_definition(const IRInstruction *in) {
  switch (in->op) {
  case IR_OP_ASSIGN:
  case IR_OP_ADDRESS_OF:
  case IR_OP_LOAD:
  case IR_OP_BINARY:
  case IR_OP_UNARY:
  case IR_OP_ROTATE_ADD:
  case IR_OP_CAST:
  case IR_OP_SELECT:
    return ir_par_operand_is_value(&in->dest) ? &in->dest : NULL;
  default:
    return NULL;
  }
}

typedef void (*IRParUseVisitor)(void *state, const IROperand *operand);

static void ir_par_visit_uses(const IRInstruction *in, void *state,
                              IRParUseVisitor visit) {
  switch (in->op) {
  case IR_OP_ASSIGN:
  case IR_OP_LOAD:
  case IR_OP_UNARY:
  case IR_OP_CAST:
  case IR_OP_BRANCH_ZERO:
  case IR_OP_PREFETCH:
    visit(state, &in->lhs);
    break;
  case IR_OP_BINARY:
  case IR_OP_ROTATE_ADD:
  case IR_OP_BRANCH_EQ:
    visit(state, &in->lhs);
    visit(state, &in->rhs);
    break;
  case IR_OP_STORE:
    visit(state, &in->dest);
    visit(state, &in->lhs);
    break;
  case IR_OP_CALL:
    for (size_t i = 0; i < in->argument_count; i++) {
      visit(state, &in->arguments[i]);
    }
    break;
  case IR_OP_SELECT:
    visit(state, &in->lhs);
    visit(state, &in->rhs);
    for (size_t i = 0; i < in->argument_count; i++) {
      visit(state, &in->arguments[i]);
    }
    break;
  default:
    break;
  }
}

typedef struct {
  IRParNames *set;
  int outside;
  int failed;
} IRParCollector;

static void ir_par_note_use(void *state, const IROperand *operand) {
  IRParCollector *collector = (IRParCollector *)state;
  long index;
  if (!ir_par_operand_is_value(operand)) {
    return;
  }
  index = ir_par_names_add(collector->set, operand->name);
  if (index < 0) {
    collector->failed = 1;
    return;
  }
  if (collector->outside) {
    collector->set->other_use[index] = 1;
  } else {
    collector->set->used[index] = 1;
  }
}

static int ir_par_type_shape(const char *type_name, IRParCapture *out) {
  size_t length;
  if (!type_name) {
    return 0;
  }
  length = strlen(type_name);
  out->type_name = type_name;
  out->is_float = 0;
  out->float_bits = 0;
  out->is_unsigned = 0;
  if (length > 0 && type_name[length - 1u] == '*') {
    out->bytes = 8;
    return 1;
  }
  if (strcmp(type_name, "rawptr") == 0) {
    out->bytes = 8;
    return 1;
  }
  if (strcmp(type_name, "float64") == 0) {
    out->bytes = 8;
    out->is_float = 1;
    out->float_bits = 64;
    return 1;
  }
  if (strcmp(type_name, "float32") == 0) {
    out->bytes = 4;
    out->is_float = 1;
    out->float_bits = 32;
    return 1;
  }
  if (strncmp(type_name, "uint", 4) == 0) {
    out->is_unsigned = 1;
  } else if (strncmp(type_name, "int", 3) != 0 &&
             strcmp(type_name, "bool") != 0 && strcmp(type_name, "char") != 0) {
    return 0;
  }
  if (strcmp(type_name, "bool") == 0 || strcmp(type_name, "char") == 0 ||
      strcmp(type_name, "int8") == 0 || strcmp(type_name, "uint8") == 0) {
    out->bytes = 1;
    return 1;
  }
  if (strcmp(type_name, "int16") == 0 || strcmp(type_name, "uint16") == 0) {
    out->bytes = 2;
    return 1;
  }
  if (strcmp(type_name, "int32") == 0 || strcmp(type_name, "uint32") == 0) {
    out->bytes = 4;
    return 1;
  }
  if (strcmp(type_name, "int64") == 0 || strcmp(type_name, "uint64") == 0) {
    out->bytes = 8;
    return 1;
  }
  return 0;
}

static const IRInstruction *ir_par_local_declaration(const IRFunction *function,
                                                     const char *name) {
  for (size_t i = 0; i < function->instruction_count; i++) {
    const IRInstruction *in = &function->instructions[i];
    if (in->op == IR_OP_DECLARE_LOCAL && in->dest.kind == IR_OPERAND_SYMBOL &&
        in->dest.name && strcmp(in->dest.name, name) == 0) {
      return in;
    }
  }
  return NULL;
}

static const char *ir_par_parameter_type(const IRFunction *function,
                                         const char *name) {
  for (size_t i = 0; i < function->parameter_count; i++) {
    if (function->parameter_names[i] &&
        strcmp(function->parameter_names[i], name) == 0) {
      return function->parameter_types ? function->parameter_types[i] : NULL;
    }
  }
  return NULL;
}

static const char *ir_par_name_type(const IRFunction *function,
                                    const char *name, int *is_local) {
  const IRInstruction *declaration = ir_par_local_declaration(function, name);
  if (declaration) {
    *is_local = 1;
    return declaration->text;
  }
  *is_local = 0;
  return ir_par_parameter_type(function, name);
}

static int ir_par_label_index(const IRFunction *function, const char *label,
                              size_t *out) {
  for (size_t i = 0; i < function->instruction_count; i++) {
    const IRInstruction *in = &function->instructions[i];
    if (in->op == IR_OP_LABEL && in->text && strcmp(in->text, label) == 0) {
      *out = i;
      return 1;
    }
  }
  return 0;
}

static int ir_par_label_index(const IRFunction *function, const char *label,
                              size_t *out);

static int ir_par_is_back_edge_target(const IRFunction *function, size_t at) {
  const char *label = function->instructions[at].text;
  if (!label) {
    return 0;
  }
  for (size_t i = at + 1u; i < function->instruction_count; i++) {
    const IRInstruction *in = &function->instructions[i];
    if ((in->op == IR_OP_JUMP || in->op == IR_OP_BRANCH_ZERO ||
         in->op == IR_OP_BRANCH_EQ) &&
        in->text && strcmp(in->text, label) == 0) {
      return 1;
    }
  }
  return 0;
}

static size_t ir_par_next_loop_header(const IRFunction *function,
                                      size_t marker) {
  for (size_t i = marker + 1u; i < function->instruction_count; i++) {
    if (function->instructions[i].op == IR_OP_LABEL &&
        ir_par_is_back_edge_target(function, i)) {
      return i;
    }
  }
  return function->instruction_count;
}

static int ir_par_locate_header(IRParLoop *loop) {
  IRFunction *function = loop->function;
  const char *wanted = function->instructions[loop->marker].text +
                       strlen(IR_PARALLEL_MARKER_PREFIX);
  size_t i;

  while (*wanted && *wanted != ':') {
    wanted++;
  }
  if (*wanted != ':' ||
      !ir_par_label_index(function, wanted + 1, &i) || i <= loop->marker) {
    i = ir_par_next_loop_header(function, loop->marker);
  }
  if (i >= function->instruction_count || i + 2u >= function->instruction_count ||
      function->instructions[i].op != IR_OP_LABEL) {
    ir_par_reject(loop, "the loop this marks is no longer here", NULL);
    return 0;
  }
  loop->header = i;
  loop->start_label = function->instructions[i].text;
  loop->compare = i + 1u;
  loop->branch = i + 2u;
  return 1;
}

static int ir_par_read_header_test(IRParLoop *loop) {
  IRFunction *function = loop->function;
  const IRInstruction *compare;
  const IRInstruction *branch;

  compare = &function->instructions[loop->compare];
  branch = &function->instructions[loop->branch];
  if (compare->op != IR_OP_BINARY || !compare->text ||
      (strcmp(compare->text, "<") != 0 && strcmp(compare->text, "<=") != 0) ||
      compare->is_float || compare->lhs.kind != IR_OPERAND_SYMBOL ||
      !compare->lhs.name) {
    ir_par_reject(loop,
                  "the header must compare one counter against a bound with "
                  "'<' or '<='",
                  NULL);
    return 0;
  }
  if (branch->op != IR_OP_BRANCH_ZERO || !branch->text ||
      !ir_par_operand_is_value(&branch->lhs) || !compare->dest.name ||
      strcmp(branch->lhs.name, compare->dest.name) != 0) {
    ir_par_reject(loop, "the header test does not guard the loop", NULL);
    return 0;
  }
  loop->iv = compare->lhs.name;
  loop->bound = compare->rhs;
  loop->bound_inclusive = strcmp(compare->text, "<=") == 0;
  loop->end_label = branch->text;

  if (!ir_par_label_index(function, loop->end_label, &loop->after)) {
    ir_par_reject(loop, "the loop exit label is missing", loop->end_label);
    return 0;
  }
  if (loop->after <= loop->branch) {
    ir_par_reject(loop, "the loop exit runs backwards", NULL);
    return 0;
  }
  loop->latch = loop->after - 1u;
  if (function->instructions[loop->latch].op != IR_OP_JUMP ||
      !function->instructions[loop->latch].text ||
      strcmp(function->instructions[loop->latch].text, loop->start_label) !=
          0) {
    ir_par_reject(loop, "the loop body does not end at its back edge", NULL);
    return 0;
  }
  return 1;
}

static int ir_par_scan_body(IRParLoop *loop) {
  IRFunction *function = loop->function;
  size_t i;
  size_t back_edges = 0;

  for (i = loop->branch + 1u; i <= loop->latch; i++) {
    const IRInstruction *in = &function->instructions[i];
    const char *target = NULL;
    if (in->op == IR_OP_JUMP || in->op == IR_OP_BRANCH_ZERO ||
        in->op == IR_OP_BRANCH_EQ) {
      target = in->text;
    }
    if (target && strcmp(target, loop->start_label) == 0) {
      back_edges++;
    }
    if (in->op == IR_OP_RETURN) {
      ir_par_reject(loop, "the body returns out of the loop", NULL);
      return 0;
    }
    if (in->op == IR_OP_NOP && in->text &&
        strncmp(in->text, IR_PARALLEL_MARKER_PREFIX,
                strlen(IR_PARALLEL_MARKER_PREFIX)) == 0) {
      ir_par_reject(loop, "a loop inside this one is marked as well, and the "
                          "threads are already spread across the outer one",
                    NULL);
      return 0;
    }
    if (!ir_par_opcode_supported(in->op) && !ir_par_is_trap_call(in)) {
      ir_par_reject(loop, "the body contains work this pass will not move",
                    in->op == IR_OP_CALL && in->text ? in->text
                                                     : ir_opcode_name(in->op));
      return 0;
    }
    if (target && strcmp(target, loop->start_label) != 0) {
      size_t at = 0;
      if (!ir_par_label_index(function, target, &at) || at <= loop->header ||
          at > loop->latch) {
        ir_par_reject(loop, "the body branches out of the loop", target);
        return 0;
      }
    }
  }
  if (back_edges != 1u) {
    ir_par_reject(loop, "the loop has more than one back edge", NULL);
    return 0;
  }
  return 1;
}

static int ir_par_find_loop(IRParLoop *loop) {
  return ir_par_locate_header(loop) && ir_par_read_header_test(loop) &&
         ir_par_scan_body(loop);
}

static int ir_par_step_is_increment(const IRFunction *function, size_t at,
                                    const char *iv) {
  const IRInstruction *in = &function->instructions[at];
  if (in->op == IR_OP_BINARY) {
    return in->text && strcmp(in->text, "+") == 0 &&
           in->lhs.kind == IR_OPERAND_SYMBOL && in->lhs.name &&
           strcmp(in->lhs.name, iv) == 0 && in->rhs.kind == IR_OPERAND_INT &&
           in->rhs.int_value == 1;
  }
  if (in->op == IR_OP_ASSIGN && in->lhs.kind == IR_OPERAND_TEMP &&
      in->lhs.name) {
    size_t k = at;
    while (k > 0u) {
      const IROperand *def;
      k--;
      def = ir_par_definition(&function->instructions[k]);
      if (def && def->name && strcmp(def->name, in->lhs.name) == 0) {
        return ir_par_step_is_increment(function, k, iv);
      }
    }
  }
  return 0;
}

static int ir_par_check_step(IRParLoop *loop) {
  IRFunction *function = loop->function;
  size_t updates = 0;
  for (size_t i = loop->branch + 1u; i <= loop->latch; i++) {
    const IRInstruction *in = &function->instructions[i];
    const IROperand *def = ir_par_definition(in);
    if (!def || def->kind != IR_OPERAND_SYMBOL ||
        strcmp(def->name, loop->iv) != 0) {
      continue;
    }
    updates++;
    if (!ir_par_step_is_increment(function, i, loop->iv)) {
      ir_par_reject(loop, "the counter must step by one", loop->iv);
      return 0;
    }
  }
  if (updates != 1u) {
    ir_par_reject(loop, "the counter is not stepped exactly once", loop->iv);
    return 0;
  }
  return 1;
}

typedef struct {
  IRParNames *set;
  unsigned char *live;
  int failed;
} IRParCoverage;

static void ir_par_coverage_use(void *state, const IROperand *operand) {
  IRParCoverage *coverage = (IRParCoverage *)state;
  long index;
  if (!ir_par_operand_is_value(operand)) {
    return;
  }
  index = ir_par_names_find(coverage->set, operand->name);
  if (index >= 0 && !coverage->live[index]) {
    coverage->set->private_ok[index] = 0;
  }
}

static int ir_par_classify(IRParLoop *loop) {
  IRFunction *function = loop->function;
  size_t block_count = 0;
  size_t names = loop->names.count;
  const IRBasicBlock *blocks;
  unsigned char *defs = NULL;
  unsigned char *state = NULL;
  unsigned char *seed = NULL;
  unsigned char *live = NULL;
  unsigned char *inside = NULL;
  IRParCoverage coverage;
  int settled = 0;
  int rounds = 0;

  if (!ir_function_rebuild_cfg(function)) {
    return 0;
  }
  blocks = ir_function_blocks(function, &block_count);
  if (!blocks || block_count == 0u || names == 0u) {
    return 1;
  }

  defs = calloc(block_count * names, 1);
  state = calloc(block_count * names, 1);
  seed = calloc(names, 1);
  live = calloc(names, 1);
  inside = calloc(block_count, 1);
  if (!defs || !state || !seed || !live || !inside) {
    free(defs);
    free(state);
    free(seed);
    free(live);
    free(inside);
    return 0;
  }

  for (size_t b = 0; b < block_count; b++) {
    const IRBasicBlock *block = &blocks[b];
    size_t lo = block->first_instruction;
    size_t hi = lo + block->instruction_count;
    if (block->instruction_count == 0u || lo < loop->header ||
        hi > loop->latch + 1u) {
      continue;
    }
    inside[b] = 1;
    if (lo != loop->header) {
      memset(state + b * names, 1, names);
    }
    for (size_t k = lo; k < hi; k++) {
      const IROperand *def = ir_par_definition(&function->instructions[k]);
      long index = def ? ir_par_names_find(&loop->names, def->name) : -1;
      if (index >= 0) {
        defs[b * names + (size_t)index] = 1;
      }
    }
  }

  while (!settled && rounds < 32) {
    settled = 1;
    rounds++;
    for (size_t b = 0; b < block_count; b++) {
      const IRBasicBlock *block = &blocks[b];
      unsigned char *row = state + b * names;
      int first = 1;
      if (!inside[b]) {
        continue;
      }
      memset(seed, 0, names);
      if (block->first_instruction != loop->header) {
        for (size_t p = 0; p < block->predecessor_count; p++) {
          size_t pred = block->predecessors[p];
          const unsigned char *prow;
          const unsigned char *pdef;
          if (pred >= block_count || !inside[pred]) {
            continue;
          }
          prow = state + pred * names;
          pdef = defs + pred * names;
          for (size_t n = 0; n < names; n++) {
            unsigned char out = (unsigned char)(prow[n] || pdef[n]);
            seed[n] = first ? out : (unsigned char)(seed[n] && out);
          }
          first = 0;
        }
      }
      if (first) {
        memset(seed, 0, names);
      }
      for (size_t n = 0; n < names; n++) {
        if (row[n] != seed[n]) {
          row[n] = seed[n];
          settled = 0;
        }
      }
    }
  }

  for (size_t i = 0; i < names; i++) {
    loop->names.private_ok[i] = loop->names.defined[i];
  }
  coverage.set = &loop->names;
  coverage.live = live;
  coverage.failed = 0;
  for (size_t b = 0; b < block_count; b++) {
    const IRBasicBlock *block = &blocks[b];
    size_t lo = block->first_instruction;
    size_t hi = lo + block->instruction_count;
    if (!inside[b]) {
      continue;
    }
    memcpy(live, state + b * names, names);
    for (size_t k = lo; k < hi; k++) {
      const IRInstruction *in = &function->instructions[k];
      const IROperand *def;
      if (k != loop->compare) {
        ir_par_visit_uses(in, &coverage, ir_par_coverage_use);
      } else if (ir_par_operand_is_value(&in->lhs)) {
        ir_par_coverage_use(&coverage, &in->lhs);
      }
      def = ir_par_definition(in);
      if (def) {
        long index = ir_par_names_find(&loop->names, def->name);
        if (index >= 0) {
          live[index] = 1;
        }
      }
    }
  }

  free(defs);
  free(state);
  free(seed);
  free(live);
  free(inside);
  return 1;
}

static const char *ir_par_reduction_operator(const char *text) {
  static const char *kOps[] = {"+", "^", "|", "&", "*"};
  if (!text) {
    return NULL;
  }
  for (size_t i = 0; i < sizeof(kOps) / sizeof(kOps[0]); i++) {
    if (strcmp(text, kOps[i]) == 0) {
      return kOps[i];
    }
  }
  return NULL;
}

static long long ir_par_reduction_identity(const char *op) {
  if (strcmp(op, "*") == 0) {
    return 1;
  }
  if (strcmp(op, "&") == 0) {
    return -1;
  }
  return 0;
}

static size_t ir_par_single_use(const IRFunction *function, size_t lo,
                                size_t hi, const char *name);

static int ir_par_combine_site(const IRParLoop *loop, size_t at,
                               size_t *combine) {
  const IRFunction *function = loop->function;
  const IRInstruction *in = &function->instructions[at];
  if (in->op == IR_OP_BINARY) {
    *combine = at;
    return 1;
  }
  if (in->op == IR_OP_ASSIGN && in->lhs.kind == IR_OPERAND_TEMP &&
      in->lhs.name) {
    size_t k = at;
    if (ir_par_single_use(function, loop->header, loop->latch, in->lhs.name) !=
        1u) {
      return 0;
    }
    while (k > loop->header) {
      const IROperand *def;
      k--;
      def = ir_par_definition(&function->instructions[k]);
      if (def && def->name && strcmp(def->name, in->lhs.name) == 0) {
        if (function->instructions[k].op != IR_OP_BINARY) {
          return 0;
        }
        *combine = k;
        return 1;
      }
    }
  }
  return 0;
}

typedef struct {
  const char *name;
  size_t count;
} IRParUseCounter;

static void ir_par_count_use(void *state, const IROperand *operand) {
  IRParUseCounter *counter = (IRParUseCounter *)state;
  if (ir_par_operand_is_value(operand) &&
      strcmp(operand->name, counter->name) == 0) {
    counter->count++;
  }
}

static size_t ir_par_single_use(const IRFunction *function, size_t lo,
                                size_t hi, const char *name) {
  IRParUseCounter counter;
  counter.name = name;
  counter.count = 0;
  for (size_t i = lo; i <= hi; i++) {
    ir_par_visit_uses(&function->instructions[i], &counter, ir_par_count_use);
  }
  return counter.count;
}

static int ir_par_recognize_reduction(IRParLoop *loop, const char *name,
                                      const char *type_name,
                                      IRParReduction *out) {
  IRFunction *function = loop->function;
  size_t sites[32];
  size_t site_count = 0;
  const char *op = NULL;
  IRParCapture shape;

  memset(&shape, 0, sizeof(shape));
  if (!ir_par_type_shape(type_name, &shape) || shape.is_float) {
    return 0;
  }
  for (size_t i = loop->branch + 1u; i <= loop->latch; i++) {
    const IRInstruction *in = &function->instructions[i];
    const IROperand *def = ir_par_definition(in);
    const IRInstruction *combine;
    const char *found;
    size_t at = 0;
    int uses_lhs;
    int uses_rhs;
    if (!def || def->kind != IR_OPERAND_SYMBOL ||
        strcmp(def->name, name) != 0) {
      continue;
    }
    if (site_count >= sizeof(sites) / sizeof(sites[0]) ||
        !ir_par_combine_site(loop, i, &at)) {
      return 0;
    }
    combine = &function->instructions[at];
    found = ir_par_reduction_operator(combine->text);
    if (!found || combine->is_float) {
      return 0;
    }
    if (op && strcmp(op, found) != 0) {
      return 0;
    }
    op = found;
    uses_lhs = combine->lhs.kind == IR_OPERAND_SYMBOL && combine->lhs.name &&
               strcmp(combine->lhs.name, name) == 0;
    uses_rhs = combine->rhs.kind == IR_OPERAND_SYMBOL && combine->rhs.name &&
               strcmp(combine->rhs.name, name) == 0;
    if (uses_lhs == uses_rhs) {
      return 0;
    }
    sites[site_count++] = at;
  }
  if (!site_count || !op) {
    return 0;
  }
  if (ir_par_single_use(function, loop->header, loop->latch, name) !=
      site_count) {
    return 0;
  }
  out->name = name;
  out->op = op;
  out->shape = shape;
  out->shape.type_name = type_name;
  return 1;
}

static int ir_par_reduction_add(IRParLoop *loop, const IRParReduction *found) {
  IRParReduction *grown = realloc(
      loop->reductions, (loop->reduction_count + 1u) * sizeof(*grown));
  if (!grown) {
    return 0;
  }
  loop->reductions = grown;
  loop->reductions[loop->reduction_count++] = *found;
  return 1;
}

static int ir_par_capture_add(IRParLoop *loop, const char *name,
                              const IRParCapture *shape, int take_address,
                              int is_temp) {
  IRParCapture *grown =
      realloc(loop->captures, (loop->capture_count + 1u) * sizeof(*grown));
  if (!grown) {
    return 0;
  }
  loop->captures = grown;
  loop->captures[loop->capture_count] = *shape;
  loop->captures[loop->capture_count].name = name;
  loop->captures[loop->capture_count].take_address = take_address;
  loop->captures[loop->capture_count].is_temp = is_temp;
  loop->capture_count++;
  return 1;
}

static int ir_par_temp_holds_address(const IRFunction *function, size_t before,
                                     const char *name) {
  size_t k = before;
  while (k > 0u) {
    const IROperand *def;
    k--;
    def = ir_par_definition(&function->instructions[k]);
    if (def && def->name && strcmp(def->name, name) == 0) {
      return function->instructions[k].op == IR_OP_ADDRESS_OF;
    }
  }
  return 0;
}

static char *ir_par_pointer_type_name(const char *array_type) {
  size_t length = 0;
  char *made;
  const char *bracket = strchr(array_type, '[');
  length = bracket ? (size_t)(bracket - array_type) : strlen(array_type);
  made = malloc(length + 2u);
  if (!made) {
    return NULL;
  }
  memcpy(made, array_type, length);
  made[length] = '*';
  made[length + 1u] = 0;
  return made;
}

static int ir_par_build_captures(IRParLoop *loop) {
  IRFunction *function = loop->function;
  for (size_t i = 0; i < loop->names.count; i++) {
    const char *name = loop->names.names[i];
    const char *type_name;
    IRParCapture shape;
    int is_local = 0;
    if (!loop->names.used[i] || loop->names.private_ok[i]) {
      continue;
    }
    if (strcmp(name, loop->iv) == 0) {
      continue;
    }
    type_name = ir_par_name_type(function, name, &is_local);
    if (loop->names.defined[i]) {
      IRParReduction found;
      memset(&found, 0, sizeof(found));
      if (is_local && type_name &&
          ir_par_recognize_reduction(loop, name, type_name, &found)) {
        if (!ir_par_reduction_add(loop, &found)) {
          return 0;
        }
        continue;
      }
      ir_par_reject(loop, "a value carries from one iteration to the next",
                    name);
      return 0;
    }
    if (!type_name) {
      if (ir_program_lookup_symbol(loop->program, name)) {
        continue;
      }
      if (ir_par_temp_holds_address(function, loop->header, name)) {
        memset(&shape, 0, sizeof(shape));
        shape.type_name = "rawptr";
        shape.bytes = 8;
        if (!ir_par_capture_add(loop, name, &shape, 0, 1)) {
          return 0;
        }
        continue;
      }
      ir_par_reject(loop,
                    "a value the optimizer hoisted out of the loop cannot be "
                    "carried into it",
                    name);
      return 0;
    }
    if (loop->names.addressed[i]) {
      char *pointer = ir_par_pointer_type_name(type_name);
      if (!pointer) {
        return 0;
      }
      memset(&shape, 0, sizeof(shape));
      shape.type_name = pointer;
      shape.bytes = 8;
      if (!ir_par_capture_add(loop, name, &shape, 1, 0)) {
        free(pointer);
        return 0;
      }
      continue;
    }
    memset(&shape, 0, sizeof(shape));
    if (!ir_par_type_shape(type_name, &shape)) {
      ir_par_reject(loop, "a value enters the loop with a type this pass "
                          "cannot carry",
                    name);
      return 0;
    }
    if (!ir_par_capture_add(loop, name, &shape, 0, 0)) {
      return 0;
    }
  }
  return 1;
}

static int ir_par_is_reduction(const IRParLoop *loop, const char *name) {
  for (size_t i = 0; i < loop->reduction_count; i++) {
    if (strcmp(loop->reductions[i].name, name) == 0) {
      return 1;
    }
  }
  return 0;
}

static int ir_par_check_escapes(IRParLoop *loop) {
  for (size_t i = 0; i < loop->names.count; i++) {
    if (!loop->names.defined[i] || !loop->names.other_use[i]) {
      continue;
    }
    if (strcmp(loop->names.names[i], loop->iv) == 0 ||
        ir_par_is_reduction(loop, loop->names.names[i])) {
      continue;
    }
    ir_par_reject(loop, "a value written in the loop is read after it",
                  loop->names.names[i]);
    return 0;
  }
  return 1;
}

static int ir_par_emit(IRFunction *out, const IRInstruction *in) {
  return ir_function_append_instruction(out, in);
}

static int ir_par_emit_declare(IRParLoop *loop, IRFunction *out,
                               const char *name, const char *type_name) {
  IRInstruction declaration = {0};
  int ok;
  declaration.op = IR_OP_DECLARE_LOCAL;
  declaration.location = loop->location;
  declaration.dest = ir_operand_symbol(name);
  declaration.text = (char *)type_name;
  declaration.value_type =
      ir_program_lookup_type(loop->program, type_name);
  if (!declaration.dest.name) {
    return 0;
  }
  ok = ir_par_emit(out, &declaration);
  ir_operand_destroy(&declaration.dest);
  return ok;
}

static int ir_par_emit_binary(IRParLoop *loop, IRFunction *out,
                              const char *dest, const char *op,
                              const IROperand *lhs, const IROperand *rhs) {
  IRInstruction in = {0};
  int ok;
  in.op = IR_OP_BINARY;
  in.location = loop->location;
  in.dest = ir_operand_temp(dest);
  in.lhs = *lhs;
  in.rhs = *rhs;
  in.text = (char *)op;
  if (!in.dest.name) {
    return 0;
  }
  ok = ir_par_emit(out, &in);
  ir_operand_destroy(&in.dest);
  return ok;
}

static int ir_par_emit_load(IRParLoop *loop, IRFunction *out,
                            const IROperand *dest, const char *address,
                            const IRParCapture *shape) {
  IRInstruction in = {0};
  int ok;
  in.op = IR_OP_LOAD;
  in.location = loop->location;
  in.dest = *dest;
  in.lhs = ir_operand_temp(address);
  in.rhs = ir_operand_int(shape->bytes);
  in.is_float = shape->is_float;
  in.float_bits = shape->float_bits;
  in.is_unsigned = shape->is_unsigned;
  if (!in.lhs.name) {
    return 0;
  }
  ok = ir_par_emit(out, &in);
  ir_operand_destroy(&in.lhs);
  return ok;
}

static int ir_par_emit_store(IRParLoop *loop, IRFunction *out,
                             const char *address, const IROperand *value,
                             const IRParCapture *shape) {
  IRInstruction in = {0};
  int ok;
  in.op = IR_OP_STORE;
  in.location = loop->location;
  in.dest = ir_operand_temp(address);
  in.lhs = *value;
  in.rhs = ir_operand_int(shape->bytes);
  in.is_float = shape->is_float;
  in.float_bits = shape->float_bits;
  in.is_unsigned = shape->is_unsigned;
  if (!in.dest.name) {
    return 0;
  }
  ok = ir_par_emit(out, &in);
  ir_operand_destroy(&in.dest);
  return ok;
}

static int ir_par_body_size(const IRParLoop *loop) {
  const IRFunction *function = loop->function;
  size_t weight = 0;
  size_t depth = 0;
  for (size_t i = loop->header; i <= loop->latch; i++) {
    const IRInstruction *in = &function->instructions[i];
    size_t step = 1u;
    if (in->op == IR_OP_LABEL && i > loop->header &&
        ir_par_is_back_edge_target(function, i)) {
      depth++;
    }
    if (in->op == IR_OP_JUMP && in->text && i < loop->latch) {
      size_t at = 0;
      if (ir_par_label_index(function, in->text, &at) && at < i && depth > 0u) {
        depth--;
      }
    }
    for (size_t d = 0; d < depth && d < 3u; d++) {
      step *= 8u;
    }
    weight += step;
  }
  if (weight > 100000u) {
    weight = 100000u;
  }
  return weight < 1u ? 1 : (int)weight;
}

static IRFunction *ir_par_build_worker(IRParLoop *loop, size_t id) {
  IRFunction *function = loop->function;
  IRFunction *worker;
  char name[192];
  const char *parameter_names[4] = {"__par_ctx", "__par_lo", "__par_hi",
                                    "__par_slot"};
  const char *parameter_types[4] = {"rawptr", "int64", "int64", "int64"};
  char slot[64];
  char address[64];
  IRInstruction cast = {0};
  IRInstruction assign = {0};
  IRInstruction ret = {0};

  snprintf(name, sizeof(name), "__mtl_par_%s_%zu",
           function->name ? function->name : "fn", id);
  worker = ir_function_create(name);
  if (!worker) {
    return NULL;
  }
  if (!ir_function_set_parameters(worker, parameter_names, parameter_types,
                                  loop->reduction_count ? 4u : 3u)) {
    ir_function_destroy(worker);
    return NULL;
  }
  worker->return_type_name = mettle_strdup("int32");
  worker->location = loop->location;
  worker->is_noinline = 1;

  for (size_t i = 0; i < loop->capture_count; i++) {
    const IRParCapture *capture = &loop->captures[i];
    IROperand destination = capture->is_temp
                                ? ir_operand_temp(capture->name)
                                : ir_operand_symbol(capture->name);
    IROperand base = ir_operand_symbol("__par_ctx");
    IROperand offset = ir_operand_int((long long)(i * 8u));
    int ok;
    snprintf(slot, sizeof(slot), ".__par_slot%zu", i);
    if (!destination.name || !base.name ||
        (!capture->is_temp &&
         !ir_par_emit_declare(loop, worker, capture->name,
                              capture->type_name)) ||
        !ir_par_emit_binary(loop, worker, slot, "+", &base, &offset)) {
      ir_operand_destroy(&destination);
      ir_operand_destroy(&base);
      ir_function_destroy(worker);
      return NULL;
    }
    ok = ir_par_emit_load(loop, worker, &destination, slot, capture);
    ir_operand_destroy(&destination);
    ir_operand_destroy(&base);
    if (!ok) {
      ir_function_destroy(worker);
      return NULL;
    }
  }

  for (size_t i = 0; i < loop->names.count; i++) {
    const IRInstruction *declaration;
    if (!loop->names.used[i] && !loop->names.defined[i]) {
      continue;
    }
    if (!loop->names.private_ok[i] && strcmp(loop->names.names[i], loop->iv)) {
      continue;
    }
    declaration = ir_par_local_declaration(function, loop->names.names[i]);
    if (!declaration) {
      continue;
    }
    if (!ir_par_emit(worker, declaration)) {
      ir_function_destroy(worker);
      return NULL;
    }
  }

  for (size_t i = 0; i < loop->reduction_count; i++) {
    const IRParReduction *reduction = &loop->reductions[i];
    IRInstruction seed = {0};
    int ok;
    if (!ir_par_emit_declare(loop, worker, reduction->name,
                             reduction->shape.type_name)) {
      ir_function_destroy(worker);
      return NULL;
    }
    seed.op = IR_OP_ASSIGN;
    seed.location = loop->location;
    seed.dest = ir_operand_symbol(reduction->name);
    seed.lhs = ir_operand_int(ir_par_reduction_identity(reduction->op));
    ok = seed.dest.name && ir_par_emit(worker, &seed);
    ir_operand_destroy(&seed.dest);
    if (!ok) {
      ir_function_destroy(worker);
      return NULL;
    }
  }

  if (!ir_par_emit_declare(loop, worker, "__par_stop", loop->iv_type)) {
    ir_function_destroy(worker);
    return NULL;
  }

  snprintf(address, sizeof(address), ".__par_lo_cast");
  cast.op = IR_OP_CAST;
  cast.location = loop->location;
  cast.dest = ir_operand_temp(address);
  cast.lhs = ir_operand_symbol("__par_lo");
  cast.text = (char *)loop->iv_type;
  if (!cast.dest.name || !cast.lhs.name || !ir_par_emit(worker, &cast)) {
    ir_operand_destroy(&cast.dest);
    ir_operand_destroy(&cast.lhs);
    ir_function_destroy(worker);
    return NULL;
  }
  ir_operand_destroy(&cast.dest);
  ir_operand_destroy(&cast.lhs);

  assign.op = IR_OP_ASSIGN;
  assign.location = loop->location;
  assign.dest = ir_operand_symbol(loop->iv);
  assign.lhs = ir_operand_temp(address);
  if (!assign.dest.name || !assign.lhs.name || !ir_par_emit(worker, &assign)) {
    ir_operand_destroy(&assign.dest);
    ir_operand_destroy(&assign.lhs);
    ir_function_destroy(worker);
    return NULL;
  }
  ir_operand_destroy(&assign.dest);
  ir_operand_destroy(&assign.lhs);

  memset(&cast, 0, sizeof(cast));
  cast.op = IR_OP_CAST;
  cast.location = loop->location;
  cast.dest = ir_operand_temp(".__par_hi_cast");
  cast.lhs = ir_operand_symbol("__par_hi");
  cast.text = (char *)loop->iv_type;
  if (!cast.dest.name || !cast.lhs.name || !ir_par_emit(worker, &cast)) {
    ir_operand_destroy(&cast.dest);
    ir_operand_destroy(&cast.lhs);
    ir_function_destroy(worker);
    return NULL;
  }
  ir_operand_destroy(&cast.dest);
  ir_operand_destroy(&cast.lhs);

  memset(&assign, 0, sizeof(assign));
  assign.op = IR_OP_ASSIGN;
  assign.location = loop->location;
  assign.dest = ir_operand_symbol("__par_stop");
  assign.lhs = ir_operand_temp(".__par_hi_cast");
  if (!assign.dest.name || !assign.lhs.name || !ir_par_emit(worker, &assign)) {
    ir_operand_destroy(&assign.dest);
    ir_operand_destroy(&assign.lhs);
    ir_function_destroy(worker);
    return NULL;
  }
  ir_operand_destroy(&assign.dest);
  ir_operand_destroy(&assign.lhs);

  for (size_t i = loop->header; i <= loop->latch; i++) {
    IRInstruction copy = function->instructions[i];
    IROperand replacement = {0};
    int replaced = 0;
    int ok;
    if (i == loop->compare) {
      replacement = ir_operand_symbol("__par_stop");
      if (!replacement.name) {
        ir_function_destroy(worker);
        return NULL;
      }
      copy.rhs = replacement;
      copy.text = (char *)"<";
      replaced = 1;
    }
    if (copy.op == IR_OP_ADDRESS_OF && copy.lhs.kind == IR_OPERAND_SYMBOL &&
        copy.lhs.name) {
      long index = ir_par_names_find(&loop->names, copy.lhs.name);
      if (index >= 0 && loop->names.addressed[index] &&
          !loop->names.private_ok[index]) {
        copy.op = IR_OP_ASSIGN;
      }
    }
    ok = ir_par_emit(worker, &copy);
    if (replaced) {
      ir_operand_destroy(&replacement);
    }
    if (!ok) {
      ir_function_destroy(worker);
      return NULL;
    }
  }

  {
    IRInstruction label = {0};
    label.op = IR_OP_LABEL;
    label.location = loop->location;
    label.text = (char *)loop->end_label;
    if (!ir_par_emit(worker, &label)) {
      ir_function_destroy(worker);
      return NULL;
    }
  }

  if (loop->reduction_count) {
    IROperand slot = ir_operand_symbol("__par_slot");
    IROperand stride = ir_operand_int((long long)(loop->reduction_count * 8u));
    IROperand base = ir_operand_symbol("__par_ctx");
    IROperand offset = ir_operand_temp(".__par_out_off");
    int ok = slot.name && base.name && offset.name;
    if (ok) {
      ok = ir_par_emit_binary(loop, worker, ".__par_out_off", "*", &slot,
                              &stride);
    }
    if (ok) {
      ok = ir_par_emit_binary(loop, worker, ".__par_out_base", "+", &base,
                              &offset);
    }
    ir_operand_destroy(&slot);
    ir_operand_destroy(&base);
    ir_operand_destroy(&offset);
    if (!ok) {
      ir_function_destroy(worker);
      return NULL;
    }
    for (size_t i = 0; i < loop->reduction_count; i++) {
      const IRParReduction *reduction = &loop->reductions[i];
      IROperand out_base = ir_operand_temp(".__par_out_base");
      IROperand fixed = ir_operand_int(
          (long long)((loop->capture_count + i) * 8u));
      IROperand value = ir_operand_symbol(reduction->name);
      char address[64];
      snprintf(address, sizeof(address), ".__par_out%zu", i);
      if (!out_base.name || !value.name ||
          !ir_par_emit_binary(loop, worker, address, "+", &out_base, &fixed) ||
          !ir_par_emit_store(loop, worker, address, &value,
                             &reduction->shape)) {
        ir_operand_destroy(&out_base);
        ir_operand_destroy(&value);
        ir_function_destroy(worker);
        return NULL;
      }
      ir_operand_destroy(&out_base);
      ir_operand_destroy(&value);
    }
  }

  ret.op = IR_OP_RETURN;
  ret.location = loop->location;
  ret.lhs = ir_operand_int(0);
  if (!ir_par_emit(worker, &ret)) {
    ir_function_destroy(worker);
    return NULL;
  }
  return worker;
}

static int ir_par_emit_dispatch(IRParLoop *loop, IRFunction *out,
                                const char *worker_name, size_t id) {
  char context_name[96];
  char base_name[96];
  char slot_name[96];
  char address_name[96];
  char lo_name[96];
  char hi_name[96];
  char stop_name[96];
  char pointer_name[96];
  char array_type[64];
  IROperand base;
  IRInstruction in = {0};
  IROperand call_args[5] = {0};
  int min_chunk = IR_PARALLEL_WORK_TARGET / ir_par_body_size(loop);

  if (min_chunk < 1) {
    min_chunk = 1;
  }
  snprintf(context_name, sizeof(context_name), "__par_ctx_%zu", id);
  snprintf(base_name, sizeof(base_name), ".__par_base_%zu", id);
  snprintf(lo_name, sizeof(lo_name), ".__par_lo_%zu", id);
  snprintf(hi_name, sizeof(hi_name), ".__par_hi_%zu", id);
  snprintf(stop_name, sizeof(stop_name), ".__par_stop_%zu", id);
  snprintf(pointer_name, sizeof(pointer_name), ".__par_fn_%zu", id);
  {
    size_t slots = loop->capture_count +
                   loop->reduction_count * IR_PARALLEL_MAX_SLOTS;
    if (!slots) {
      slots = 1u;
    }
    snprintf(array_type, sizeof(array_type), "int64[%zu]", slots);
    if (!ir_program_int64_array_type(loop->program, slots) ||
        !ir_par_emit_declare(loop, out, context_name, array_type)) {
      return 0;
    }
  }

  in.op = IR_OP_ADDRESS_OF;
  in.location = loop->location;
  in.dest = ir_operand_temp(base_name);
  in.lhs = ir_operand_symbol(context_name);
  if (!in.dest.name || !in.lhs.name || !ir_par_emit(out, &in)) {
    ir_operand_destroy(&in.dest);
    ir_operand_destroy(&in.lhs);
    return 0;
  }
  ir_operand_destroy(&in.dest);
  ir_operand_destroy(&in.lhs);

  base = ir_operand_temp(base_name);
  if (!base.name) {
    return 0;
  }
  for (size_t i = 0; i < loop->capture_count; i++) {
    const IRParCapture *capture = &loop->captures[i];
    IROperand offset = ir_operand_int((long long)(i * 8u));
    IROperand value;
    int ok;
    snprintf(slot_name, sizeof(slot_name), ".__par_put%zu_%zu", id, i);
    if (!ir_par_emit_binary(loop, out, slot_name, "+", &base, &offset)) {
      ir_operand_destroy(&base);
      return 0;
    }
    if (capture->take_address) {
      IRInstruction take = {0};
      snprintf(address_name, sizeof(address_name), ".__par_addr%zu_%zu", id, i);
      take.op = IR_OP_ADDRESS_OF;
      take.location = loop->location;
      take.dest = ir_operand_temp(address_name);
      take.lhs = ir_operand_symbol(capture->name);
      if (!take.dest.name || !take.lhs.name || !ir_par_emit(out, &take)) {
        ir_operand_destroy(&take.dest);
        ir_operand_destroy(&take.lhs);
        ir_operand_destroy(&base);
        return 0;
      }
      ir_operand_destroy(&take.dest);
      ir_operand_destroy(&take.lhs);
      value = ir_operand_temp(address_name);
    } else if (capture->is_temp) {
      value = ir_operand_temp(capture->name);
    } else {
      value = ir_operand_symbol(capture->name);
    }
    if (!value.name) {
      ir_operand_destroy(&base);
      return 0;
    }
    ok = ir_par_emit_store(loop, out, slot_name, &value, capture);
    ir_operand_destroy(&value);
    if (!ok) {
      ir_operand_destroy(&base);
      return 0;
    }
  }

  for (size_t k = 0; k < IR_PARALLEL_MAX_SLOTS; k++) {
    for (size_t r = 0; r < loop->reduction_count; r++) {
      const IRParReduction *reduction = &loop->reductions[r];
      IROperand offset = ir_operand_int((long long)(
          (loop->capture_count + k * loop->reduction_count + r) * 8u));
      IROperand identity =
          ir_operand_int(ir_par_reduction_identity(reduction->op));
      char seed_name[96];
      snprintf(seed_name, sizeof(seed_name), ".__par_seed%zu_%zu_%zu", id, k,
               r);
      if (!ir_par_emit_binary(loop, out, seed_name, "+", &base, &offset) ||
          !ir_par_emit_store(loop, out, seed_name, &identity,
                             &reduction->shape)) {
        ir_operand_destroy(&base);
        return 0;
      }
    }
  }

  memset(&in, 0, sizeof(in));
  in.op = IR_OP_ADDRESS_OF;
  in.location = loop->location;
  in.dest = ir_operand_temp(pointer_name);
  in.lhs = ir_operand_symbol(worker_name);
  if (!in.dest.name || !in.lhs.name || !ir_par_emit(out, &in)) {
    ir_operand_destroy(&in.dest);
    ir_operand_destroy(&in.lhs);
    ir_operand_destroy(&base);
    return 0;
  }
  ir_operand_destroy(&in.dest);
  ir_operand_destroy(&in.lhs);

  memset(&in, 0, sizeof(in));
  in.op = IR_OP_CAST;
  in.location = loop->location;
  in.dest = ir_operand_temp(lo_name);
  in.lhs = ir_operand_symbol(loop->iv);
  in.text = (char *)"int64";
  if (!in.dest.name || !in.lhs.name || !ir_par_emit(out, &in)) {
    ir_operand_destroy(&in.dest);
    ir_operand_destroy(&in.lhs);
    ir_operand_destroy(&base);
    return 0;
  }
  ir_operand_destroy(&in.dest);
  ir_operand_destroy(&in.lhs);

  memset(&in, 0, sizeof(in));
  in.op = IR_OP_CAST;
  in.location = loop->location;
  in.dest = ir_operand_temp(hi_name);
  in.lhs = loop->bound;
  in.text = (char *)"int64";
  if (!in.dest.name || !ir_par_emit(out, &in)) {
    ir_operand_destroy(&in.dest);
    ir_operand_destroy(&base);
    return 0;
  }
  ir_operand_destroy(&in.dest);

  if (loop->bound_inclusive) {
    IROperand lhs = ir_operand_temp(hi_name);
    IROperand one = ir_operand_int(1);
    int ok;
    if (!lhs.name) {
      ir_operand_destroy(&base);
      return 0;
    }
    ok = ir_par_emit_binary(loop, out, stop_name, "+", &lhs, &one);
    ir_operand_destroy(&lhs);
    if (!ok) {
      ir_operand_destroy(&base);
      return 0;
    }
  }

  memset(&in, 0, sizeof(in));
  call_args[0] = ir_operand_temp(pointer_name);
  call_args[1] = ir_operand_temp(base_name);
  call_args[2] = ir_operand_temp(lo_name);
  call_args[3] = ir_operand_temp(loop->bound_inclusive ? stop_name : hi_name);
  call_args[4] = ir_operand_int(min_chunk);
  in.op = IR_OP_CALL;
  in.location = loop->location;
  in.text = (char *)(loop->reduction_count ? IR_PARALLEL_SLOTS_SYMBOL
                                           : IR_PARALLEL_RANGE_SYMBOL);
  in.arguments = call_args;
  in.argument_count = 5u;
  if (!call_args[0].name || !call_args[1].name || !call_args[2].name ||
      !call_args[3].name || !ir_par_emit(out, &in)) {
    for (size_t i = 0; i < 4u; i++) {
      ir_operand_destroy(&call_args[i]);
    }
    ir_operand_destroy(&base);
    return 0;
  }
  for (size_t i = 0; i < 4u; i++) {
    ir_operand_destroy(&call_args[i]);
  }
  ir_operand_destroy(&base);

  {
    const char *stop = loop->bound_inclusive ? stop_name : hi_name;
    char done_label[96];
    char guard_name[96];
    char settle_name[96];
    IROperand lo_value = ir_operand_temp(lo_name);
    IROperand stop_value = ir_operand_temp(stop);
    IRInstruction step = {0};
    int ok;
    snprintf(done_label, sizeof(done_label), "__par_done_%zu", id);
    snprintf(guard_name, sizeof(guard_name), ".__par_ran_%zu", id);
    snprintf(settle_name, sizeof(settle_name), ".__par_end_%zu", id);
    if (!lo_value.name || !stop_value.name) {
      ir_operand_destroy(&lo_value);
      ir_operand_destroy(&stop_value);
      return 0;
    }
    ok = ir_par_emit_binary(loop, out, guard_name, "<", &lo_value,
                            &stop_value);
    ir_operand_destroy(&lo_value);
    ir_operand_destroy(&stop_value);
    if (!ok) {
      return 0;
    }

    step.op = IR_OP_BRANCH_ZERO;
    step.location = loop->location;
    step.lhs = ir_operand_temp(guard_name);
    step.text = done_label;
    if (!step.lhs.name || !ir_par_emit(out, &step)) {
      ir_operand_destroy(&step.lhs);
      return 0;
    }
    ir_operand_destroy(&step.lhs);

    memset(&step, 0, sizeof(step));
    step.op = IR_OP_CAST;
    step.location = loop->location;
    step.dest = ir_operand_temp(settle_name);
    step.lhs = ir_operand_temp(stop);
    step.text = (char *)loop->iv_type;
    if (!step.dest.name || !step.lhs.name || !ir_par_emit(out, &step)) {
      ir_operand_destroy(&step.dest);
      ir_operand_destroy(&step.lhs);
      return 0;
    }
    ir_operand_destroy(&step.dest);
    ir_operand_destroy(&step.lhs);

    memset(&step, 0, sizeof(step));
    step.op = IR_OP_ASSIGN;
    step.location = loop->location;
    step.dest = ir_operand_symbol(loop->iv);
    step.lhs = ir_operand_temp(settle_name);
    if (!step.dest.name || !step.lhs.name || !ir_par_emit(out, &step)) {
      ir_operand_destroy(&step.dest);
      ir_operand_destroy(&step.lhs);
      return 0;
    }
    ir_operand_destroy(&step.dest);
    ir_operand_destroy(&step.lhs);

    memset(&step, 0, sizeof(step));
    step.op = IR_OP_LABEL;
    step.location = loop->location;
    step.text = done_label;
    if (!ir_par_emit(out, &step)) {
      return 0;
    }
  }

  for (size_t k = 0; k < IR_PARALLEL_MAX_SLOTS; k++) {
    for (size_t r = 0; r < loop->reduction_count; r++) {
      const IRParReduction *reduction = &loop->reductions[r];
      IROperand ctx_base = ir_operand_temp(base_name);
      IROperand offset = ir_operand_int((long long)(
          (loop->capture_count + k * loop->reduction_count + r) * 8u));
      IROperand loaded;
      IROperand accumulator;
      IRInstruction merge = {0};
      char address[96];
      char value_name[96];
      int ok;
      snprintf(address, sizeof(address), ".__par_get%zu_%zu_%zu", id, k, r);
      snprintf(value_name, sizeof(value_name), ".__par_val%zu_%zu_%zu", id, k,
               r);
      if (!ctx_base.name ||
          !ir_par_emit_binary(loop, out, address, "+", &ctx_base, &offset)) {
        ir_operand_destroy(&ctx_base);
        return 0;
      }
      ir_operand_destroy(&ctx_base);
      loaded = ir_operand_temp(value_name);
      if (!loaded.name ||
          !ir_par_emit_load(loop, out, &loaded, address, &reduction->shape)) {
        ir_operand_destroy(&loaded);
        return 0;
      }
      ir_operand_destroy(&loaded);
      accumulator = ir_operand_symbol(reduction->name);
      merge.op = IR_OP_BINARY;
      merge.location = loop->location;
      merge.dest = ir_operand_symbol(reduction->name);
      merge.lhs = accumulator;
      merge.rhs = ir_operand_temp(value_name);
      merge.text = (char *)reduction->op;
      merge.is_unsigned = reduction->shape.is_unsigned;
      ok = merge.dest.name && merge.lhs.name && merge.rhs.name &&
           ir_par_emit(out, &merge);
      ir_operand_destroy(&merge.dest);
      ir_operand_destroy(&merge.lhs);
      ir_operand_destroy(&merge.rhs);
      if (!ok) {
        return 0;
      }
    }
  }
  return 1;
}

static void ir_par_free_instructions(IRFunction *scratch) {
  for (size_t i = 0; i < scratch->instruction_count; i++) {
    ir_instruction_destroy_storage(&scratch->instructions[i]);
  }
  free(scratch->instructions);
  scratch->instructions = NULL;
  scratch->instruction_count = 0;
  scratch->instruction_capacity = 0;
}

static void ir_par_free_captures(IRParLoop *loop) {
  for (size_t i = 0; i < loop->capture_count; i++) {
    if (loop->captures[i].take_address) {
      free((char *)loop->captures[i].type_name);
    }
  }
  free(loop->captures);
  loop->captures = NULL;
  loop->capture_count = 0;
  free(loop->reductions);
  loop->reductions = NULL;
  loop->reduction_count = 0;
}

static int ir_par_rewrite(IRParLoop *loop, const char *worker_name,
                          size_t id) {
  IRFunction *function = loop->function;
  IRFunction rebuilt = {0};
  for (size_t i = 0; i < function->instruction_count; i++) {
    if (i == loop->marker) {
      continue;
    }
    if (i == loop->header) {
      if (!ir_par_emit_dispatch(loop, &rebuilt, worker_name, id)) {
        ir_par_free_instructions(&rebuilt);
        return 0;
      }
      i = loop->latch;
      continue;
    }
    if (!ir_function_append_instruction(&rebuilt, &function->instructions[i])) {
      ir_par_free_instructions(&rebuilt);
      return 0;
    }
  }
  ir_function_clear_cfg(function);
  for (size_t i = 0; i < function->instruction_count; i++) {
    ir_instruction_destroy_storage(&function->instructions[i]);
  }
  free(function->instructions);
  function->instructions = rebuilt.instructions;
  function->instruction_count = rebuilt.instruction_count;
  function->instruction_capacity = rebuilt.instruction_capacity;
  function->cfg_valid = 0;
  return 1;
}

static int ir_par_gather(IRParLoop *loop) {
  IRFunction *function = loop->function;
  IRParCollector collector;
  collector.set = &loop->names;
  collector.failed = 0;

  for (size_t i = 0; i < function->instruction_count; i++) {
    const IRInstruction *in = &function->instructions[i];
    const IROperand *def;
    int inside = i >= loop->header && i <= loop->latch;
    collector.outside = !inside;
    if (i != loop->compare) {
      ir_par_visit_uses(in, &collector, ir_par_note_use);
    } else if (ir_par_operand_is_value(&in->lhs)) {
      ir_par_note_use(&collector, &in->lhs);
    }
    if (in->op == IR_OP_ADDRESS_OF && in->lhs.kind == IR_OPERAND_SYMBOL &&
        in->lhs.name && inside) {
      long index = ir_par_names_add(&loop->names, in->lhs.name);
      if (index < 0) {
        return 0;
      }
      loop->names.addressed[index] = 1;
      loop->names.used[index] = 1;
    }
    def = ir_par_definition(in);
    if (def) {
      long index = ir_par_names_add(&loop->names, def->name);
      if (index < 0) {
        return 0;
      }
      if (inside) {
        loop->names.defined[index] = 1;
      }
    }
    if (collector.failed) {
      return 0;
    }
  }
  return 1;
}

static int ir_par_check_globals(IRParLoop *loop) {
  IRFunction *function = loop->function;
  for (size_t i = loop->header; i <= loop->latch; i++) {
    const IRInstruction *in = &function->instructions[i];
    const IROperand *def = ir_par_definition(in);
    int is_local = 0;
    if (!def || def->kind != IR_OPERAND_SYMBOL) {
      continue;
    }
    if (!ir_par_name_type(function, def->name, &is_local) || !is_local) {
      ir_par_reject(loop, "the loop writes a name that is not a local",
                    def->name);
      return 0;
    }
  }
  return 1;
}

static int ir_par_run_one(IRProgram *program, IRFunction *function,
                          size_t marker, size_t id, int *changed) {
  IRParLoop loop;
  IRFunction *worker;
  const IRInstruction *iv_declaration;
  char *worker_name;
  int ok = 0;

  memset(&loop, 0, sizeof(loop));
  loop.program = program;
  loop.function = function;
  loop.marker = marker;
  loop.location = function->instructions[marker].location;

  if (!ir_par_find_loop(&loop) || !ir_par_check_step(&loop)) {
    ir_par_names_free(&loop.names);
    return 0;
  }
  iv_declaration = ir_par_local_declaration(function, loop.iv);
  if (!iv_declaration || !iv_declaration->text) {
    ir_par_reject(&loop, "the counter has no declared type", loop.iv);
    ir_par_names_free(&loop.names);
    return 0;
  }
  loop.iv_type = iv_declaration->text;

  if (!ir_par_gather(&loop) || !ir_par_classify(&loop)) {
    ir_par_names_free(&loop.names);
    return 0;
  }
  if (!ir_par_check_globals(&loop) || !ir_par_build_captures(&loop) ||
      !ir_par_check_escapes(&loop)) {
    ir_par_free_captures(&loop);
    ir_par_names_free(&loop.names);
    return 0;
  }

  worker = ir_par_build_worker(&loop, id);
  if (!worker) {
    ir_par_free_captures(&loop);
    ir_par_names_free(&loop.names);
    return 0;
  }
  worker_name = mettle_strdup(worker->name);
  if (!worker_name || !ir_program_add_function(program, worker)) {
    free(worker_name);
    ir_function_destroy(worker);
    ir_par_free_captures(&loop);
    ir_par_names_free(&loop.names);
    return 0;
  }
  ok = ir_par_rewrite(&loop, worker_name, id);
  free(worker_name);
  ir_par_free_captures(&loop);
  ir_par_names_free(&loop.names);
  if (ok) {
    *changed = 1;
  }
  return ok;
}

int ir_parallelize_marked_loops_pass(IRProgram *program, int *changed) {
  size_t id = 0;
  size_t original;
  if (!program) {
    return 0;
  }
  original = program->function_count;
  for (size_t f = 0; f < original; f++) {
    IRFunction *function = program->functions[f];
    size_t i = 0;
    if (!function) {
      continue;
    }
    while (i < function->instruction_count) {
      const IRInstruction *in = &function->instructions[i];
      if (in->op != IR_OP_NOP || !in->text ||
          strncmp(in->text, IR_PARALLEL_MARKER_PREFIX,
                  strlen(IR_PARALLEL_MARKER_PREFIX)) != 0) {
        i++;
        continue;
      }
      if (!ir_par_run_one(program, function, i, id++, changed)) {
        return 1;
      }
      i = 0;
    }
  }
  return 1;
}

void ir_note_parallel_loops_unverified(IRProgram *program) {
  int marked = 0;
  if (!program) {
    return;
  }
  for (size_t f = 0; f < program->function_count; f++) {
    IRFunction *function = program->functions[f];
    if (!function) {
      continue;
    }
    for (size_t i = 0; i < function->instruction_count; i++) {
      IRInstruction *in = &function->instructions[i];
      if (in->op == IR_OP_NOP && in->text &&
          strncmp(in->text, IR_PARALLEL_MARKER_PREFIX,
                  strlen(IR_PARALLEL_MARKER_PREFIX)) == 0) {
        in->text = NULL;
        marked++;
      }
    }
  }
  if (marked > 0) {
    fprintf(stderr,
            "note: %d `@parallel` loop%s left running on one thread; the "
            "outliner only runs with -O/--release\n",
            marked, marked == 1 ? "" : "s");
  }
}
