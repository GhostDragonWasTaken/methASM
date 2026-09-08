#include "codegen/binary/mir.h"
#include "codegen/binary/mir_machine.h"
#include "../../common.h"
#include "internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int mir_home_bytes_for(const MirVreg *vr, int *base) {
  int home = vr->home_bytes > 0 ? vr->home_bytes : 8;
  if (vr->home_granule) {
    home = (home + BINARY_SAFETY_GRANULE - 1) / BINARY_SAFETY_GRANULE *
           BINARY_SAFETY_GRANULE;
    *base = (*base + BINARY_SAFETY_GRANULE - 1) / BINARY_SAFETY_GRANULE *
            BINARY_SAFETY_GRANULE;
  }
  return home;
}

static const BinaryGpRegister MIR_GP_POOL[] = {
    BINARY_GP_RBX, BINARY_GP_R12, BINARY_GP_R13, BINARY_GP_R14, BINARY_GP_R15};
#define MIR_GP_POOL_COUNT (sizeof(MIR_GP_POOL) / sizeof(MIR_GP_POOL[0]))
static const BinaryGpRegister MIR_GP_EXTRA[] = {
    BINARY_GP_RAX, BINARY_GP_RCX, BINARY_GP_RDX, BINARY_GP_RSI,
    BINARY_GP_RDI, BINARY_GP_R8,  BINARY_GP_R9};
#define MIR_GP_EXTRA_COUNT (sizeof(MIR_GP_EXTRA) / sizeof(MIR_GP_EXTRA[0]))
#define MIR_GP_LEAF_POOL_MAX (MIR_GP_POOL_COUNT + MIR_GP_EXTRA_COUNT)

static int mir_op_is_call_barrier(MirOpcode op) {
  return mir_op_has(op, MIR_OPF_CALL_BARRIER);
}

static int mir_fn_has_calls(const MirFunction *fn) {
  for (size_t i = 0; i < fn->insn_count; i++) {
    if (mir_op_is_call_barrier(fn->insns[i].op)) {
      return 1;
    }
  }
  return 0;
}

static void mir_mark_crosses_call(MirFunction *fn) {
  for (size_t v = 0; v < fn->vreg_count; v++) {
    fn->vregs[v].crosses_call = 0;
    fn->vregs[v].crosses_preserving_only = 1;
    fn->vregs[v].crosses_xmm_preserving_only = 1;
  }
  for (size_t i = 0; i < fn->insn_count; i++) {
    if (!mir_op_is_call_barrier(fn->insns[i].op)) {
      continue;
    }
    int is_call = fn->insns[i].op == MIR_CALL;
    int keeps_rax = is_call && fn->insns[i].preserves_rax;
    int keeps_xmm = is_call && fn->insns[i].preserves_xmm;
    int c = (int)i;
    for (size_t v = 0; v < fn->vreg_count; v++) {
      MirVreg *vr = &fn->vregs[v];
      if (vr->live_start != MIR_LIVE_NONE && vr->live_start < c &&
          vr->live_end > c) {
        vr->crosses_call = 1;
        if (!keeps_rax) {
          vr->crosses_preserving_only = 0;
        }
        if (!keeps_xmm) {
          vr->crosses_xmm_preserving_only = 0;
        }
      }
    }
  }
}

static void mir_drop_unused_preserves(MirFunction *fn) {
  int need_gp = 0;
  int need_xmm = 0;
  for (size_t v = 0; v < fn->vreg_count; v++) {
    const MirVreg *vr = &fn->vregs[v];
    if (!vr->in_register || !vr->crosses_call) {
      continue;
    }
    if (vr->rclass == MIR_RC_GP && vr->phys == (int)BINARY_GP_RAX) {
      need_gp = 1;
    } else if (vr->rclass == MIR_RC_XMM) {
      for (size_t i = 0; i < MIR_XMM_POOL_COUNT; i++) {
        if (vr->phys == (int)MIR_XMM_POOL[i]) {
          need_xmm = 1;
        }
      }
    }
  }
  if (!need_gp) {
    fn->preserve_slot = 0;
  }
  if (!need_xmm) {
    fn->preserve_xmm_slot = 0;
  }
}

static size_t mir_cross_pool_for(const MirVreg *vr,
                                 const BinaryGpRegister *base, size_t base_n,
                                 BinaryGpRegister *buffer) {
  size_t n = 0;
  for (; n < base_n; n++) {
    buffer[n] = base[n];
  }
  if (vr->crosses_preserving_only) {
    buffer[n++] = BINARY_GP_RAX;
  }
  return n;
}

static int mir_fn_has_preserving_call(const MirFunction *fn, int xmm) {
  for (size_t i = 0; i < fn->insn_count; i++) {
    if (fn->insns[i].op != MIR_CALL) {
      continue;
    }
    if (xmm ? fn->insns[i].preserves_xmm : fn->insns[i].preserves_rax) {
      return 1;
    }
  }
  return 0;
}

static int mir_fn_has_real_calls(const MirFunction *fn) {
  for (size_t i = 0; i < fn->insn_count; i++) {
    if (mir_op_has(fn->insns[i].op, MIR_OPF_REAL_CALL)) {
      return 1;
    }
  }
  return 0;
}

static int mir_fn_uses_slp(const MirFunction *fn) {
  for (size_t i = 0; i < fn->insn_count; i++) {
    if (mir_op_is_inline_kernel(fn->insns[i].op)) {
      return 1;
    }
  }
  return 0;
}

static int mir_reg_arg_index(BinaryGpRegister reg) {
  const BinaryAbi *abi = code_generator_binary_active_abi();
  if (abi && abi->int_param_registers) {
    for (size_t i = 0; i < abi->int_param_count; i++) {
      if (abi->int_param_registers[i] == reg) {
        return (int)i;
      }
    }
  }
  return -1;
}

static int mir_reg_poolable(BinaryGpRegister reg, size_t param_count,
                            int is_leaf) {
  (void)is_leaf;
  (void)param_count;
  (void)reg;
  return 1;
}

static size_t mir_build_gp_leaf_pool(BinaryGpRegister *out, size_t param_count,
                                     int is_leaf) {
  size_t n = 0;
  for (size_t i = 0; i < MIR_GP_POOL_COUNT; i++) {
    out[n++] = MIR_GP_POOL[i];
  }
  for (size_t i = 0; i < MIR_GP_EXTRA_COUNT; i++) {
    if (mir_reg_poolable(MIR_GP_EXTRA[i], param_count, is_leaf)) {
      out[n++] = MIR_GP_EXTRA[i];
    }
  }
  return n;
}

#define MIR_GP_CROSSCALL_POOL_MAX 7
#define MIR_GP_CROSSCALL_POOL_EXT (MIR_GP_CROSSCALL_POOL_MAX + 1)

static size_t mir_build_gp_crosscall_pool(BinaryGpRegister *out) {
  size_t n = 0;
  const BinaryAbi *abi = code_generator_binary_active_abi();
  int sysv = abi && abi->counts_classes_separately;
  out[n++] = BINARY_GP_RBX;
  if (!sysv) {
    out[n++] = BINARY_GP_RSI;
    out[n++] = BINARY_GP_RDI;
  }
  out[n++] = BINARY_GP_R12;
  out[n++] = BINARY_GP_R13;
  out[n++] = BINARY_GP_R14;
  out[n++] = BINARY_GP_R15;
  return n;
}

const BinaryXmmRegister MIR_XMM_POOL[MIR_XMM_POOL_COUNT] = {
    BINARY_XMM0, BINARY_XMM1, BINARY_XMM2, BINARY_XMM3};

static const BinaryXmmRegister MIR_XMM_NONVOL_POOL[] = {
    BINARY_XMM8,  BINARY_XMM9,  BINARY_XMM10, BINARY_XMM11,
    BINARY_XMM12, BINARY_XMM13, BINARY_XMM14, BINARY_XMM15};
#define MIR_XMM_NONVOL_POOL_COUNT \
  (sizeof(MIR_XMM_NONVOL_POOL) / sizeof(MIR_XMM_NONVOL_POOL[0]))

static int mir_gp_is_nonvolatile(BinaryGpRegister reg) {
  return code_generator_binary_gp_register_is_win64_nonvolatile(reg);
}

static void mir_note_operand_liveness(MirFunction *fn, const MirOperand *op,
                                      int index) {
  if (!op) {
    return;
  }
  MirVregId ids[2] = {MIR_VREG_NONE, MIR_VREG_NONE};
  if (op->kind == MIR_OPK_VREG) {
    ids[0] = op->vreg;
  } else if (op->kind == MIR_OPK_MEM) {
    ids[0] = op->mem.base;
    ids[1] = op->mem.index;
  }
  for (int k = 0; k < 2; k++) {
    MirVregId v = ids[k];
    if (v < 0 || (size_t)v >= fn->vreg_count) {
      continue;
    }
    MirVreg *vr = &fn->vregs[v];
    if (vr->live_start == MIR_LIVE_NONE || index < vr->live_start) {
      vr->live_start = index;
    }
    if (vr->live_end == MIR_LIVE_NONE || index > vr->live_end) {
      vr->live_end = index;
    }
  }
}

static int mir_find_label(const MirFunction *fn, const char *name) {
  if (!name) {
    return -1;
  }
  for (size_t i = 0; i < fn->insn_count; i++) {
    const MirInst *in = &fn->insns[i];
    if (in->op == MIR_LABEL && in->dst.kind == MIR_OPK_LABEL && in->dst.sym &&
        strcmp(in->dst.sym, name) == 0) {
      return (int)i;
    }
  }
  return -1;
}

static int mir_inst_is_branch(const MirInst *in) {
  return in->op == MIR_JMP || in->op == MIR_JCC || in->op == MIR_CMPBR ||
         in->op == MIR_FCMPBR;
}

typedef struct {
  int l;
  int b;
} MirBackEdge;

static int mir_collect_back_edges(const MirFunction *fn,
                                  MirBackEdge **edges_out, size_t *count_out) {
  size_t label_count = 0;
  size_t branch_count = 0;
  *edges_out = NULL;
  *count_out = 0;

  for (size_t i = 0; i < fn->insn_count; i++) {
    const MirInst *in = &fn->insns[i];
    if (in->op == MIR_LABEL && in->dst.kind == MIR_OPK_LABEL && in->dst.sym) {
      label_count++;
    } else if (mir_inst_is_branch(in) && in->dst.kind == MIR_OPK_LABEL) {
      branch_count++;
    }
  }
  if (branch_count == 0) {
    return 1;
  }

  size_t slot_count = 16;
  while (slot_count < label_count * 2) {
    slot_count *= 2;
  }
  size_t *slots = calloc(slot_count, sizeof(*slots));
  MirBackEdge *edges = malloc(branch_count * sizeof(*edges));
  if (!slots || !edges) {
    free(slots);
    free(edges);
    return 0;
  }

  size_t mask = slot_count - 1;
  for (size_t i = 0; i < fn->insn_count; i++) {
    const MirInst *in = &fn->insns[i];
    if (in->op != MIR_LABEL || in->dst.kind != MIR_OPK_LABEL || !in->dst.sym) {
      continue;
    }
    size_t h = mettle_fnv1a_hash(in->dst.sym) & mask;
    while (slots[h]) {
      if (strcmp(fn->insns[slots[h] - 1].dst.sym, in->dst.sym) == 0) {
        if (getenv("METTLE_MIR_DUPLABEL")) {
          fprintf(stderr, "MIR-DUPLABEL %s at %zu (first at %zu)\n",
                  in->dst.sym, i, slots[h] - 1);
        }
        h = SIZE_MAX;
        break;
      }
      h = (h + 1) & mask;
    }
    if (h != SIZE_MAX) {
      slots[h] = i + 1;
    }
  }

  size_t n = 0;
  for (size_t i = 0; i < fn->insn_count; i++) {
    const MirInst *in = &fn->insns[i];
    if (!mir_inst_is_branch(in) || in->dst.kind != MIR_OPK_LABEL ||
        !in->dst.sym) {
      continue;
    }
    int l = -1;
    size_t h = mettle_fnv1a_hash(in->dst.sym) & mask;
    while (slots[h]) {
      if (strcmp(fn->insns[slots[h] - 1].dst.sym, in->dst.sym) == 0) {
        l = (int)(slots[h] - 1);
        break;
      }
      h = (h + 1) & mask;
    }
    if (l < 0 || l >= (int)i) {
      continue;
    }
    edges[n].l = l;
    edges[n].b = (int)i;
    n++;
  }

  free(slots);
  if (n == 0) {
    free(edges);
    return 1;
  }
  *edges_out = edges;
  *count_out = n;
  return 1;
}

#define MIR_LIVE_CFG_MAX_WORK 8000000u

typedef struct {
  size_t *slots;
  size_t mask;
} MirLabelMap;

typedef struct {
  int *block_of;
  int *block_start;
  unsigned long long *live_in;
  unsigned long long *live_out;
  size_t block_count;
  size_t words;
} MirLiveCfg;

static int mir_label_map_build(const MirFunction *fn, MirLabelMap *map) {
  size_t label_count = 0;
  size_t slot_count = 16;
  map->slots = NULL;
  map->mask = 0;
  for (size_t i = 0; i < fn->insn_count; i++) {
    const MirInst *in = &fn->insns[i];
    if (in->op == MIR_LABEL && in->dst.kind == MIR_OPK_LABEL && in->dst.sym) {
      label_count++;
    }
  }
  while (slot_count < label_count * 2) {
    slot_count *= 2;
  }
  map->slots = (size_t *)calloc(slot_count, sizeof(*map->slots));
  if (!map->slots) {
    return 0;
  }
  map->mask = slot_count - 1;
  for (size_t i = 0; i < fn->insn_count; i++) {
    const MirInst *in = &fn->insns[i];
    size_t h;
    int duplicate = 0;
    if (in->op != MIR_LABEL || in->dst.kind != MIR_OPK_LABEL || !in->dst.sym) {
      continue;
    }
    h = mettle_fnv1a_hash(in->dst.sym) & map->mask;
    while (map->slots[h]) {
      if (strcmp(fn->insns[map->slots[h] - 1].dst.sym, in->dst.sym) == 0) {
        duplicate = 1;
        break;
      }
      h = (h + 1) & map->mask;
    }
    if (!duplicate) {
      map->slots[h] = i + 1;
    }
  }
  return 1;
}

static int mir_label_map_find(const MirFunction *fn, const MirLabelMap *map,
                              const char *name) {
  size_t h;
  if (!name || !map->slots) {
    return -1;
  }
  h = mettle_fnv1a_hash(name) & map->mask;
  while (map->slots[h]) {
    if (strcmp(fn->insns[map->slots[h] - 1].dst.sym, name) == 0) {
      return (int)(map->slots[h] - 1);
    }
    h = (h + 1) & map->mask;
  }
  return -1;
}

static int mir_inst_ends_block(const MirInst *in) {
  return in->op == MIR_JMP || in->op == MIR_JCC || in->op == MIR_CMPBR ||
         in->op == MIR_FCMPBR || in->op == MIR_JMP_TABLE || in->op == MIR_RET ||
         in->op == MIR_TRAP;
}

static void mir_live_bit_set(unsigned long long *set, size_t v) {
  set[v >> 6] |= 1ull << (v & 63);
}

static int mir_live_bit_get(const unsigned long long *set, size_t v) {
  return (set[v >> 6] & (1ull << (v & 63))) != 0;
}

static void mir_live_note_uses(const MirFunction *fn, const MirOperand *op,
                               unsigned long long *use_set,
                               const unsigned long long *def_set,
                               int skip_plain_vreg) {
  MirVregId ids[2] = {MIR_VREG_NONE, MIR_VREG_NONE};
  if (op->kind == MIR_OPK_VREG) {
    if (skip_plain_vreg) {
      return;
    }
    ids[0] = op->vreg;
  } else if (op->kind == MIR_OPK_MEM) {
    ids[0] = op->mem.base;
    ids[1] = op->mem.index;
  } else {
    return;
  }
  for (int k = 0; k < 2; k++) {
    MirVregId v = ids[k];
    if (v < 0 || (size_t)v >= fn->vreg_count) {
      continue;
    }
    if (!mir_live_bit_get(def_set, (size_t)v)) {
      mir_live_bit_set(use_set, (size_t)v);
    }
  }
}

static void mir_live_add_operand(const MirFunction *fn, const MirOperand *op,
                                 unsigned long long *live,
                                 int skip_plain_vreg) {
  MirVregId ids[2] = {MIR_VREG_NONE, MIR_VREG_NONE};
  if (op->kind == MIR_OPK_VREG) {
    if (skip_plain_vreg) {
      return;
    }
    ids[0] = op->vreg;
  } else if (op->kind == MIR_OPK_MEM) {
    ids[0] = op->mem.base;
    ids[1] = op->mem.index;
  } else {
    return;
  }
  for (int k = 0; k < 2; k++) {
    MirVregId v = ids[k];
    if (v >= 0 && (size_t)v < fn->vreg_count) {
      mir_live_bit_set(live, (size_t)v);
    }
  }
}

static int mir_live_cfg_build(const MirFunction *fn, MirLiveCfg *cfg,
                              int precise_defs) {
  MirLabelMap map;
  unsigned char *leader = NULL;
  int *block_start = NULL;
  int *succ_head = NULL;
  int *succ_next = NULL;
  int *succ_block = NULL;
  unsigned long long *use_set = NULL;
  unsigned long long *def_set = NULL;
  unsigned char *killable = NULL;
  int *first_dst = NULL;
  int *first_any = NULL;
  size_t words = (fn->vreg_count + 63) / 64;
  size_t nblocks = 0;
  size_t succ_capacity = 0;
  size_t succ_count = 0;
  int ok = 0;
  int changed = 1;

  cfg->block_of = NULL;
  cfg->block_start = NULL;
  cfg->live_in = NULL;
  cfg->live_out = NULL;
  cfg->block_count = 0;
  cfg->words = words;
  if (fn->insn_count == 0 || fn->vreg_count == 0 || words == 0) {
    return 0;
  }
  if ((unsigned long long)fn->insn_count * (unsigned long long)words >
      MIR_LIVE_CFG_MAX_WORK) {
    return 0;
  }
  if (!mir_label_map_build(fn, &map)) {
    return 0;
  }

  leader = (unsigned char *)calloc(fn->insn_count, sizeof(*leader));
  cfg->block_of = (int *)malloc(fn->insn_count * sizeof(*cfg->block_of));
  if (!precise_defs) {
    killable = (unsigned char *)calloc(fn->vreg_count, sizeof(*killable));
    first_dst = (int *)malloc(fn->vreg_count * sizeof(*first_dst));
    first_any = (int *)malloc(fn->vreg_count * sizeof(*first_any));
  }
  if (!leader || !cfg->block_of ||
      (!precise_defs && (!killable || !first_dst || !first_any))) {
    goto done;
  }
  if (!precise_defs) {
    for (size_t v = 0; v < fn->vreg_count; v++) {
      first_dst[v] = -1;
      first_any[v] = -1;
    }
  }

  leader[0] = 1;
  for (size_t i = 0; i < fn->insn_count; i++) {
    const MirInst *in = &fn->insns[i];
    if (in->op == MIR_LABEL) {
      leader[i] = 1;
    }
    if (mir_inst_ends_block(in) && i + 1 < fn->insn_count) {
      leader[i + 1] = 1;
    }
  }
  for (size_t i = 0; i < fn->insn_count; i++) {
    if (leader[i]) {
      nblocks++;
    }
    cfg->block_of[i] = (int)nblocks - 1;
  }
  block_start = (int *)malloc(nblocks * sizeof(*block_start));
  succ_head = (int *)malloc(nblocks * sizeof(*succ_head));
  if (!block_start || !succ_head) {
    goto done;
  }
  {
    size_t b = 0;
    for (size_t i = 0; i < fn->insn_count; i++) {
      if (leader[i]) {
        block_start[b++] = (int)i;
      }
    }
  }
  for (size_t b = 0; b < nblocks; b++) {
    succ_head[b] = -1;
  }

  succ_capacity = nblocks * 2 + 8;
  succ_next = (int *)malloc(succ_capacity * sizeof(*succ_next));
  succ_block = (int *)malloc(succ_capacity * sizeof(*succ_block));
  if (!succ_next || !succ_block) {
    goto done;
  }
  for (size_t b = 0; b < nblocks; b++) {
    size_t last = (b + 1 < nblocks) ? (size_t)block_start[b + 1] - 1
                                    : fn->insn_count - 1;
    const MirInst *in = &fn->insns[last];
    int targets[2];
    int target_count = 0;
    int fall_through = 1;
    const MirJumpTable *table = NULL;
    if (in->op == MIR_JMP) {
      fall_through = 0;
      targets[target_count++] = mir_label_map_find(fn, &map, in->dst.sym);
    } else if (in->op == MIR_JCC || in->op == MIR_CMPBR ||
               in->op == MIR_FCMPBR) {
      targets[target_count++] = mir_label_map_find(fn, &map, in->dst.sym);
    } else if (in->op == MIR_JMP_TABLE) {
      fall_through = 0;
      table = (const MirJumpTable *)in->aux;
      if (!table) {
        goto done;
      }
    } else if (in->op == MIR_RET || in->op == MIR_TRAP) {
      fall_through = 0;
    }
    for (int t = 0; t < target_count; t++) {
      if (targets[t] < 0) {
        goto done;
      }
    }
    if (fall_through && b + 1 >= nblocks) {
      fall_through = 0;
    }
    {
      size_t need = succ_count + (size_t)target_count + (fall_through ? 1u : 0u);
      if (table) {
        need += table->count;
      }
      if (need > succ_capacity) {
        int *n1;
        int *n2;
        size_t grown = succ_capacity * 2 + need;
        n1 = (int *)realloc(succ_next, grown * sizeof(*succ_next));
        if (!n1) {
          goto done;
        }
        succ_next = n1;
        n2 = (int *)realloc(succ_block, grown * sizeof(*succ_block));
        if (!n2) {
          goto done;
        }
        succ_block = n2;
        succ_capacity = grown;
      }
    }
    for (int t = 0; t < target_count; t++) {
      succ_block[succ_count] = cfg->block_of[targets[t]];
      succ_next[succ_count] = succ_head[b];
      succ_head[b] = (int)succ_count;
      succ_count++;
    }
    if (table) {
      for (size_t t = 0; t < table->count; t++) {
        int target = mir_label_map_find(fn, &map, table->labels[t]);
        if (target < 0) {
          goto done;
        }
        succ_block[succ_count] = cfg->block_of[target];
        succ_next[succ_count] = succ_head[b];
        succ_head[b] = (int)succ_count;
        succ_count++;
      }
    }
    if (fall_through) {
      succ_block[succ_count] = (int)b + 1;
      succ_next[succ_count] = succ_head[b];
      succ_head[b] = (int)succ_count;
      succ_count++;
    }
  }

  if (!precise_defs) {
    for (size_t i = 0; i < fn->insn_count; i++) {
      const MirInst *in = &fn->insns[i];
      const MirOperand *ops[3] = {&in->dst, &in->a, &in->b};
      for (int k = 0; k < 3; k++) {
        MirVregId ids[2] = {MIR_VREG_NONE, MIR_VREG_NONE};
        if (ops[k]->kind == MIR_OPK_VREG) {
          ids[0] = ops[k]->vreg;
        } else if (ops[k]->kind == MIR_OPK_MEM) {
          ids[0] = ops[k]->mem.base;
          ids[1] = ops[k]->mem.index;
        }
        for (int j = 0; j < 2; j++) {
          MirVregId v = ids[j];
          if (v < 0 || (size_t)v >= fn->vreg_count) {
            continue;
          }
          if (first_any[v] < 0) {
            first_any[v] = (int)i;
          }
          if (k == 0 && ops[k]->kind == MIR_OPK_VREG && first_dst[v] < 0) {
            first_dst[v] = (int)i;
          }
        }
      }
    }
    for (size_t v = 0; v < fn->vreg_count; v++) {
      killable[v] = (first_dst[v] >= 0 && first_dst[v] == first_any[v] &&
                     !fn->vregs[v].entry_live)
                        ? 1
                        : 0;
    }
  }

  use_set = (unsigned long long *)calloc(nblocks * words, sizeof(*use_set));
  def_set = (unsigned long long *)calloc(nblocks * words, sizeof(*def_set));
  cfg->live_in =
      (unsigned long long *)calloc(nblocks * words, sizeof(*cfg->live_in));
  if (!use_set || !def_set || !cfg->live_in) {
    goto done;
  }
  for (size_t b = 0; b < nblocks; b++) {
    size_t lo = (size_t)block_start[b];
    size_t hi = (b + 1 < nblocks) ? (size_t)block_start[b + 1] : fn->insn_count;
    unsigned long long *u = use_set + b * words;
    unsigned long long *d = def_set + b * words;
    for (size_t i = lo; i < hi; i++) {
      const MirInst *in = &fn->insns[i];
      mir_live_note_uses(fn, &in->dst, u, d, 1);
      mir_live_note_uses(fn, &in->a, u, d, 0);
      mir_live_note_uses(fn, &in->b, u, d, 0);
      if (in->dst.kind == MIR_OPK_VREG) {
        MirVregId v = in->dst.vreg;
        if (v >= 0 && (size_t)v < fn->vreg_count) {
          if (precise_defs) {
            mir_live_bit_set(d, (size_t)v);
          } else if (killable[v] && first_dst[v] == (int)i) {
            mir_live_bit_set(d, (size_t)v);
          } else if (!mir_live_bit_get(d, (size_t)v)) {
            mir_live_bit_set(u, (size_t)v);
          }
        }
      }
    }
  }

  while (changed) {
    changed = 0;
    for (size_t bi = nblocks; bi > 0; bi--) {
      size_t b = bi - 1;
      const unsigned long long *u = use_set + b * words;
      const unsigned long long *d = def_set + b * words;
      unsigned long long *in_set = cfg->live_in + b * words;
      for (int e = succ_head[b]; e >= 0; e = succ_next[e]) {
        const unsigned long long *s =
            cfg->live_in + (size_t)succ_block[e] * words;
        for (size_t w = 0; w < words; w++) {
          unsigned long long next = in_set[w] | (s[w] & ~d[w]);
          if (next != in_set[w]) {
            in_set[w] = next;
            changed = 1;
          }
        }
      }
      for (size_t w = 0; w < words; w++) {
        unsigned long long next = in_set[w] | u[w];
        if (next != in_set[w]) {
          in_set[w] = next;
          changed = 1;
        }
      }
    }
  }

  if (precise_defs) {
    cfg->live_out =
        (unsigned long long *)calloc(nblocks * words, sizeof(*cfg->live_out));
    if (!cfg->live_out) {
      goto done;
    }
    for (size_t b = 0; b < nblocks; b++) {
      unsigned long long *out_set = cfg->live_out + b * words;
      for (int e = succ_head[b]; e >= 0; e = succ_next[e]) {
        const unsigned long long *s =
            cfg->live_in + (size_t)succ_block[e] * words;
        for (size_t w = 0; w < words; w++) {
          out_set[w] |= s[w];
        }
      }
    }

    cfg->block_start = block_start;
    block_start = NULL;
  }
  cfg->block_count = nblocks;
  ok = 1;

done:
  free(map.slots);
  free(leader);
  free(block_start);
  free(succ_head);
  free(succ_next);
  free(succ_block);
  free(use_set);
  free(def_set);
  free(killable);
  free(first_dst);
  free(first_any);
  if (!ok) {
    free(cfg->block_of);
    free(cfg->block_start);
    free(cfg->live_in);
    free(cfg->live_out);
    cfg->block_of = NULL;
    cfg->block_start = NULL;
    cfg->live_in = NULL;
    cfg->live_out = NULL;
    cfg->block_count = 0;
  }
  return ok;
}

static void mir_live_cfg_free(MirLiveCfg *cfg) {
  free(cfg->block_of);
  free(cfg->block_start);
  free(cfg->live_in);
  free(cfg->live_out);
  cfg->block_of = NULL;
  cfg->block_start = NULL;
  cfg->live_in = NULL;
  cfg->live_out = NULL;
  cfg->block_count = 0;
}

static void mir_extend_across_edge(MirFunction *fn, int l, int b,
                                   const unsigned long long *header_live,
                                   int *changed) {
  for (size_t v = 0; v < fn->vreg_count; v++) {
    MirVreg *vr = &fn->vregs[v];
    if (vr->live_start == MIR_LIVE_NONE) {
      continue;
    }
    if (header_live && !mir_live_bit_get(header_live, v)) {
      continue;
    }
    if (vr->live_end < l || vr->live_start > b) {
      continue;
    }
    int crosses = (vr->live_start < l) || (vr->live_end > b) ||
                  (vr->entry_live && l == 0);
    if (!crosses) {
      continue;
    }
    vr->loop_carried = 1;
    if (vr->live_start > l) {
      vr->live_start = l;
      *changed = 1;
    }
    if (vr->live_end < b) {
      vr->live_end = b;
      *changed = 1;
    }
  }
}

static void mir_compute_liveness(MirFunction *fn) {
  for (size_t i = 0; i < fn->vreg_count; i++) {
    fn->vregs[i].live_start = MIR_LIVE_NONE;
    fn->vregs[i].live_end = MIR_LIVE_NONE;
    fn->vregs[i].loop_carried = 0;
    fn->vregs[i].entry_live = 0;
  }
  for (size_t i = 0; i < fn->insn_count; i++) {
    const MirInst *in = &fn->insns[i];
    mir_note_operand_liveness(fn, &in->dst, (int)i);
    mir_note_operand_liveness(fn, &in->a, (int)i);
    mir_note_operand_liveness(fn, &in->b, (int)i);
  }

  for (size_t i = 0; i < fn->param_count; i++) {
    MirVreg *pv = &fn->vregs[fn->params[i].vreg];
    if (pv->live_end != MIR_LIVE_NONE) {
      pv->live_start = 0;
      pv->entry_live = 1;
    }
  }
  if (fn->returns_indirect && fn->indirect_return_vreg != MIR_VREG_NONE) {
    MirVreg *rv = &fn->vregs[fn->indirect_return_vreg];
    if (rv->live_end != MIR_LIVE_NONE) {
      rv->live_start = 0;
      rv->entry_live = 1;
    }
  }

  MirBackEdge *edges = NULL;
  size_t edge_count = 0;
  int changed = 1;
  if (mir_collect_back_edges(fn, &edges, &edge_count)) {
    MirLiveCfg cfg;
    int have_cfg = edge_count > 0 && mir_live_cfg_build(fn, &cfg, 0);
    while (changed) {
      changed = 0;
      for (size_t e = 0; e < edge_count; e++) {
        const unsigned long long *header_live = NULL;
        if (have_cfg) {
          header_live =
              cfg.live_in + (size_t)cfg.block_of[edges[e].l] * cfg.words;
        }
        mir_extend_across_edge(fn, edges[e].l, edges[e].b, header_live,
                               &changed);
      }
    }
    if (have_cfg) {
      mir_live_cfg_free(&cfg);
    }
    free(edges);
    return;
  }

  while (changed) {
    changed = 0;
    for (size_t i = 0; i < fn->insn_count; i++) {
      const MirInst *in = &fn->insns[i];
      if (!mir_inst_is_branch(in) || in->dst.kind != MIR_OPK_LABEL) {
        continue;
      }
      int l = mir_find_label(fn, in->dst.sym);
      int b = (int)i;
      if (l < 0 || l >= b) {
        continue;
      }
      mir_extend_across_edge(fn, l, b, NULL, &changed);
    }
  }
}

static MirVregId *mir_order_by_start(MirFunction *fn, size_t *count_out) {
  size_t live = 0;
  for (size_t i = 0; i < fn->vreg_count; i++) {
    if (fn->vregs[i].live_start != MIR_LIVE_NONE) {
      live++;
    }
  }
  *count_out = live;
  if (live == 0) {
    return NULL;
  }
  MirVregId *order = (MirVregId *)malloc(live * sizeof(MirVregId));
  if (!order) {
    fn->has_error = 1;
    return NULL;
  }
  size_t n = 0;
  for (size_t i = 0; i < fn->vreg_count; i++) {
    if (fn->vregs[i].live_start != MIR_LIVE_NONE) {
      order[n++] = (MirVregId)i;
    }
  }
  for (size_t i = 1; i < live; i++) {
    MirVregId key = order[i];
    int ks = fn->vregs[key].live_start;
    size_t j = i;
    while (j > 0) {
      int prev_s = fn->vregs[order[j - 1]].live_start;
      if (prev_s < ks || (prev_s == ks && order[j - 1] <= key)) {
        break;
      }
      order[j] = order[j - 1];
      j--;
    }
    order[j] = key;
  }
  return order;
}

static void mir_compute_coalesce_hints(MirFunction *fn) {
  for (size_t v = 0; v < fn->vreg_count; v++) {
    fn->vregs[v].coalesce_hint = MIR_VREG_NONE;
  }
  for (size_t i = 0; i < fn->insn_count; i++) {
    const MirInst *in = &fn->insns[i];
    int commutative;
    if (!mir_op_has(in->op, MIR_OPF_COALESCE_CANDIDATE)) {
      continue;
    }
    commutative = mir_op_has(in->op, MIR_OPF_COMMUTATIVE);
    if (in->dst.kind != MIR_OPK_VREG) {
      continue;
    }
    MirRegClass dcls = fn->vregs[in->dst.vreg].rclass;
    if (in->op == MIR_IMUL && in->b.kind == MIR_OPK_IMM) {
      continue;
    }
    if ((in->op == MIR_SHL || in->op == MIR_SHR || in->op == MIR_SAR) &&
        in->b.kind != MIR_OPK_IMM) {
      continue;
    }
    MirVregId d = in->dst.vreg;
    MirVregId cand = MIR_VREG_NONE;
    if (in->a.kind == MIR_OPK_VREG && in->a.vreg != d &&
        fn->vregs[in->a.vreg].rclass == dcls &&
        fn->vregs[in->a.vreg].live_end == (int)i) {
      cand = in->a.vreg;
    } else if (commutative && in->b.kind == MIR_OPK_VREG && in->b.vreg != d &&
               fn->vregs[in->b.vreg].rclass == dcls &&
               fn->vregs[in->b.vreg].live_end == (int)i) {
      cand = in->b.vreg;
    }
    fn->vregs[d].coalesce_hint = cand;
  }
}

typedef struct {
  int *pos;
  size_t count;
  size_t cap;
} MirClobberList;

typedef struct {
  const MirFunction *fn;
  const MirInst *insns;
  size_t insn_count;
  int valid;
  MirClobberList explicit_fixed[16];
  MirClobberList rax_implicit;
  MirClobberList rcx_implicit;
  MirClobberList rdx_implicit;
} MirClobberIndex;

static MirClobberIndex g_mir_clobber_index = {0};

static void mir_clobber_list_free(MirClobberList *l) {
  free(l->pos);
  l->pos = NULL;
  l->count = 0;
  l->cap = 0;
}

static int mir_clobber_list_push(MirClobberList *l, int k) {
  if (l->count == l->cap) {
    size_t cap = l->cap ? l->cap * 2 : 16;
    int *pos = realloc(l->pos, cap * sizeof(*pos));
    if (!pos) {
      return 0;
    }
    l->pos = pos;
    l->cap = cap;
  }
  l->pos[l->count++] = k;
  return 1;
}

static int mir_clobber_list_hit(const MirClobberList *l, int s, int e) {
  size_t lo = 0;
  size_t hi = l->count;
  while (lo < hi) {
    size_t mid = lo + (hi - lo) / 2;
    if (l->pos[mid] <= s) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }
  return lo < l->count && l->pos[lo] < e;
}

static void mir_clobber_index_reset(void) {
  for (size_t r = 0; r < 16; r++) {
    mir_clobber_list_free(&g_mir_clobber_index.explicit_fixed[r]);
  }
  mir_clobber_list_free(&g_mir_clobber_index.rax_implicit);
  mir_clobber_list_free(&g_mir_clobber_index.rcx_implicit);
  mir_clobber_list_free(&g_mir_clobber_index.rdx_implicit);
  g_mir_clobber_index.fn = NULL;
  g_mir_clobber_index.insns = NULL;
  g_mir_clobber_index.insn_count = 0;
  g_mir_clobber_index.valid = 0;
}

static int mir_clobber_index_ensure(const MirFunction *fn) {
  MirClobberIndex *ix = &g_mir_clobber_index;
  if (ix->fn == fn && ix->insns == fn->insns &&
      ix->insn_count == fn->insn_count) {
    return ix->valid;
  }

  mir_clobber_index_reset();
  ix->fn = fn;
  ix->insns = fn->insns;
  ix->insn_count = fn->insn_count;
  ix->valid = 1;

  for (size_t k = 0; k < fn->insn_count; k++) {
    const MirInst *in = &fn->insns[k];
    int ok = 1;
    const MirOperand *fixed[3] = {&in->dst, &in->a, &in->b};
    for (int f = 0; ok && f < 3; f++) {
      if (fixed[f]->kind == MIR_OPK_PHYS && fixed[f]->rclass == MIR_RC_GP &&
          fixed[f]->phys >= 0 && fixed[f]->phys < 16) {
        ok = mir_clobber_list_push(&ix->explicit_fixed[fixed[f]->phys], (int)k);
      }
    }
    if (ok) {
      unsigned implicit = mir_inst_fixed_gp(in);
      if (implicit & (1u << (unsigned)BINARY_GP_RAX)) {
        ok = mir_clobber_list_push(&ix->rax_implicit, (int)k);
      }
      if (ok && (implicit & (1u << (unsigned)BINARY_GP_RDX))) {
        ok = mir_clobber_list_push(&ix->rdx_implicit, (int)k);
      }
      if (ok && (implicit & (1u << (unsigned)BINARY_GP_RCX))) {
        ok = mir_clobber_list_push(&ix->rcx_implicit, (int)k);
      }
      if (ok && mir_op_has(in->op, MIR_OPF_CLOBBERS_LISTED_BY_KERNEL)) {
        const MirKernelAux *ka = (const MirKernelAux *)in->aux;
        const MirIrKernel *kern = ka ? mir_ir_kernel_at(ka->kernel_index) : NULL;
        unsigned clobbers = kern ? kern->gp_clobbers : 0u;
        for (int r = 0; clobbers && ok; r++, clobbers >>= 1) {
          if (clobbers & 1u) {
            ok = mir_clobber_list_push(&ix->explicit_fixed[r], (int)k);
          }
        }
      }
      if (ok && mir_op_has(in->op, MIR_OPF_CLOBBERS_EVERY_GP)) {
        for (int r = 0; ok && r < mir_machine()->gp_register_count; r++) {
          ok = mir_clobber_list_push(&ix->explicit_fixed[r], (int)k);
        }
      }
    }
    if (!ok) {
      mir_clobber_index_reset();
      ix->fn = fn;
      ix->insns = fn->insns;
      ix->insn_count = fn->insn_count;
      ix->valid = 0;
      return 0;
    }
  }

  return 1;
}

static int mir_reg_is_pinned(BinaryGpRegister reg) {
  return mir_machine_gp_is_pinned((int)reg);
}

static int mir_reg_clobbered_by_index(const MirFunction *fn,
                                      BinaryGpRegister reg, int s, int e,
                                      int *answered) {
  const MirClobberIndex *ix = &g_mir_clobber_index;

  *answered = 0;
  if ((int)reg < 0 || (int)reg >= 16 || !mir_clobber_index_ensure(fn)) {
    return 0;
  }
  *answered = 1;
  if (mir_clobber_list_hit(&ix->explicit_fixed[reg], s, e)) {
    return 1;
  }
  if (reg == BINARY_GP_RAX) {
    return mir_clobber_list_hit(&ix->rax_implicit, s, e);
  }
  if (reg == BINARY_GP_RCX) {
    return mir_clobber_list_hit(&ix->rcx_implicit, s, e);
  }
  if (reg == BINARY_GP_RDX) {
    return mir_clobber_list_hit(&ix->rdx_implicit, s, e);
  }
  return 0;
}

static int mir_inst_pins_reg(const MirInst *in, BinaryGpRegister reg) {
  return mir_inst_pins_gp(in, (int)reg);
}

static int mir_inst_clobbers_reg(const MirInst *in, BinaryGpRegister reg) {
  const MirOperand *fixed[3] = {&in->dst, &in->a, &in->b};

  for (int f = 0; f < 3; f++) {
    if (fixed[f]->kind == MIR_OPK_PHYS && fixed[f]->rclass == MIR_RC_GP &&
        fixed[f]->phys == (int)reg) {
      return 1;
    }
  }
  if (in->op == MIR_IR_KERNEL && (int)reg >= 0 && (int)reg < 16) {
    const MirKernelAux *aux = (const MirKernelAux *)in->aux;
    const MirIrKernel *kernel = aux ? mir_ir_kernel_at(aux->kernel_index)
                                    : NULL;
    if (kernel && (kernel->gp_clobbers & (1u << (unsigned)reg))) {
      return 1;
    }
  }
  if (in->op == MIR_INLINE_ASM) {
    return 1;
  }
  return mir_reg_is_pinned(reg) && mir_inst_pins_reg(in, reg);
}

static int mir_reg_clobbered_in_range(const MirFunction *fn,
                                      BinaryGpRegister reg, int s, int e) {
  int answered = 0;
  int indexed = mir_reg_clobbered_by_index(fn, reg, s, e, &answered);

  if (answered) {
    return indexed;
  }
  for (int k = s + 1; k < e; k++) {
    if (mir_inst_clobbers_reg(&fn->insns[k], reg)) {
      return 1;
    }
  }
  return 0;
}

static int mir_vreg_is_param_in_reg(const MirFunction *fn, MirVregId v,
                                    BinaryGpRegister reg) {
  int ai = mir_reg_arg_index(reg);
  if (ai < 0) {
    return 0;
  }
  /* Only where the function calls nothing. A parameter that dies AT a call
     does not cross it, so nothing else stops it sitting in an argument
     register -- and then marshalling the outgoing arguments overwrites it
     while it is still live. `invoke(cb, v)` placed v in RCX over cb and
     called v. */
  if (mir_fn_has_real_calls(fn)) {
    return 0;
  }
  for (size_t i = 0; i < fn->param_count; i++) {
    if (fn->params[i].vreg == v && !fn->params[i].is_float &&
        fn->params[i].arg_index == ai) {
      return 1;
    }
  }
  return 0;
}

static uint32_t mir_color_reg_mask(const MirFunction *fn, MirVregId v,
                                   const BinaryGpRegister *gp_leaf_pool,
                                   size_t gp_leaf_n,
                                   const BinaryGpRegister *gp_cross_pool,
                                   size_t gp_cross_n, int allow_rbp) {
  const MirVreg *vr = &fn->vregs[v];
  uint32_t m = 0;
  BinaryGpRegister cross_ext[MIR_GP_CROSSCALL_POOL_EXT];
  if (vr->rclass == MIR_RC_GP) {
    if (vr->crosses_call) {
      gp_cross_n = mir_cross_pool_for(vr, gp_cross_pool, gp_cross_n, cross_ext);
      gp_cross_pool = cross_ext;
    }
    const BinaryGpRegister *pool =
        vr->crosses_call ? gp_cross_pool : gp_leaf_pool;
    size_t n = vr->crosses_call ? gp_cross_n : gp_leaf_n;
    size_t incoming = fn->incoming_arg_slots
                          ? fn->incoming_arg_slots
                          : fn->param_count + (fn->returns_indirect ? 1 : 0);
    for (size_t i = 0; i < n; i++) {
      BinaryGpRegister reg = pool[i];
      if (fn->reserve_rbx && reg == BINARY_GP_RBX) {
        continue;
      }
      if (vr->entry_live) {
        int ai = mir_reg_arg_index(reg);
        /* An incoming argument register is barred from holding a value that
           is live at entry, because the prologue would have to shuffle the
           arguments past each other to place it. The one register that needs
           no shuffle is the one this parameter already arrives in: leaving it
           there costs nothing and spares a callee-saved register, which is a
           store in the prologue and a load at every exit on every call. */
        if (ai >= 0 && (size_t)ai < incoming &&
            !mir_vreg_is_param_in_reg(fn, v, reg)) {
          continue;
        }
      }
      if (!mir_reg_clobbered_in_range(fn, reg, vr->live_start, vr->live_end)) {
        m |= 1u << reg;
      }
    }
    if (allow_rbp &&
        !mir_reg_clobbered_in_range(fn, BINARY_GP_RBP, vr->live_start,
                                    vr->live_end)) {
      m |= 1u << BINARY_GP_RBP;
    }
  } else if (vr->rclass == MIR_RC_XMM &&
             (!vr->crosses_call || vr->crosses_xmm_preserving_only)) {
    if (!fn->has_xmm_arg_call) {
      for (size_t i = 0; i < MIR_XMM_POOL_COUNT; i++) {
        m |= 1u << MIR_XMM_POOL[i];
      }
    }
    if (!vr->crosses_call) {
      for (size_t i = 0; i < MIR_XMM_NONVOL_POOL_COUNT; i++) {
        if (mir_xmm_is_encoder_scratch(MIR_XMM_NONVOL_POOL[i])) {
          continue;
        }
        m |= 1u << MIR_XMM_NONVOL_POOL[i];
      }
    }
  }
  return m;
}

static int mir_color_interferes(const MirVreg *a, const MirVreg *b) {
  if (a->rclass != b->rclass) {
    return 0;
  }
  if (a->entry_live && b->entry_live) {
    return 1;
  }
  return a->live_start < b->live_end && b->live_start < a->live_end;
}

static MirVregId *mir_build_narrowing_extend_map(const MirFunction *fn) {
  if (fn->vreg_count == 0) {
    return NULL;
  }
  MirVregId *map = (MirVregId *)malloc(fn->vreg_count * sizeof(MirVregId));
  if (!map) {
    return NULL;
  }
  for (size_t v = 0; v < fn->vreg_count; v++) {
    map[v] = MIR_VREG_NONE;
  }
  for (size_t i = 0; i < fn->insn_count; i++) {
    const MirInst *in = &fn->insns[i];
    if ((in->op != MIR_MOVZX && in->op != MIR_MOVSX) || in->width >= 8 ||
        in->dst.kind != MIR_OPK_VREG || in->a.kind != MIR_OPK_VREG ||
        in->dst.vreg == in->a.vreg) {
      continue;
    }
    map[in->dst.vreg] = in->a.vreg;
  }
  return map;
}

static int mir_narrowing_avoid_reg(const MirFunction *fn, const MirVregId *map,
                                   MirVregId v) {
  if (!map || map[v] == MIR_VREG_NONE) {
    return -1;
  }
  const MirVreg *sv = &fn->vregs[map[v]];
  return sv->in_register ? (int)sv->phys : -1;
}

#define MIR_MAX_WEIGHTED_DEPTH 3
#define MIR_MAX_SPILL_COST (1 << 24)

static unsigned char *mir_build_loop_depths(const MirFunction *fn) {
  MirBackEdge *edges = NULL;
  size_t edge_count = 0;
  unsigned char *depth;
  if (fn->insn_count == 0) {
    return NULL;
  }
  depth = (unsigned char *)calloc(fn->insn_count, sizeof(*depth));
  if (!depth) {
    return NULL;
  }
  if (!mir_collect_back_edges(fn, &edges, &edge_count) || edge_count == 0) {
    free(edges);
    return depth;
  }
  for (size_t e = 0; e < edge_count; e++) {
    int l = edges[e].l;
    int b = edges[e].b;
    int seen = 0;
    if (l < 0 || b < l || (size_t)b >= fn->insn_count) {
      continue;
    }
    for (size_t k = 0; k < e; k++) {
      if (edges[k].l == l) {
        seen = 1;
        break;
      }
    }
    if (seen) {
      continue;
    }
    for (size_t k = e + 1; k < edge_count; k++) {
      if (edges[k].l == l && edges[k].b > b && (size_t)edges[k].b < fn->insn_count) {
        b = edges[k].b;
      }
    }
    for (int i = l; i <= b; i++) {
      if (depth[i] < MIR_MAX_WEIGHTED_DEPTH) {
        depth[i]++;
      }
    }
  }
  free(edges);
  return depth;
}

static int mir_scale_cost(int cost, int factor) {
  long long scaled = (long long)cost * factor;
  return scaled > MIR_MAX_SPILL_COST ? MIR_MAX_SPILL_COST : (int)scaled;
}

static void mir_note_operand_depth(const MirOperand *op, unsigned char depth,
                                   unsigned char *use_depth, size_t n) {
  MirVregId ids[2] = {MIR_VREG_NONE, MIR_VREG_NONE};
  if (op->kind == MIR_OPK_VREG) {
    ids[0] = op->vreg;
  } else if (op->kind == MIR_OPK_MEM) {
    ids[0] = op->mem.base;
    ids[1] = op->mem.index;
  }
  for (int j = 0; j < 2; j++) {
    if (ids[j] >= 0 && (size_t)ids[j] < n && use_depth[ids[j]] < depth) {
      use_depth[ids[j]] = depth;
    }
  }
}

static int mir_spill_rank(const MirFunction *fn, const unsigned char *use_depth,
                          MirVregId v) {
  (void)fn;
  return (int)use_depth[v];
}

const char *g_mir_ra_trace_name = NULL;

static int mir_env_regalloc_trace(void) {
  static int cached = -1;
  if (cached < 0) {
    cached = getenv("METTLE_REGALLOC_TRACE") ? 1 : 0;
  }
  return cached;
}

static const char *mir_ra_trace_name(void) {
  return g_mir_ra_trace_name ? g_mir_ra_trace_name : "?";
}

static void mir_color_spill_costs(const MirFunction *fn,
                                  const int *colorable, int *cost,
                                  unsigned char *use_depth, size_t count) {
  unsigned char *loop_depth = mir_build_loop_depths(fn);

  for (size_t i = 0; i < fn->insn_count; i++) {
    const MirInst *in = &fn->insns[i];
    const MirOperand *ops[3] = {&in->dst, &in->a, &in->b};
    unsigned char depth = loop_depth ? loop_depth[i] : 0;
    int weight = 1;

    for (int k = 0; k < depth; k++) {
      weight *= 10;
    }
    for (int k = 0; k < 3; k++) {
      const MirOperand *op = ops[k];
      MirVregId ids[2] = {MIR_VREG_NONE, MIR_VREG_NONE};

      mir_note_operand_depth(op, depth, use_depth, count);
      if (op->kind == MIR_OPK_VREG) {
        ids[0] = op->vreg;
      } else if (op->kind == MIR_OPK_MEM) {
        ids[0] = op->mem.base;
        ids[1] = op->mem.index;
      }
      for (int j = 0; j < 2; j++) {
        MirVregId id = ids[j];
        if (id >= 0 && (size_t)id < count && colorable[id]) {
          cost[id] = mir_scale_cost(cost[id] + weight, 1);
        }
      }
    }
  }
  free(loop_depth);
  for (size_t i = 0; i < fn->iconst_count; i++) {
    MirVregId v = fn->iconsts[i].vreg;
    if (v >= 0 && (size_t)v < count && colorable[v]) {
      cost[v] = mir_scale_cost(cost[v], 64);
    }
  }
  for (size_t i = 0; i < fn->fconst_count; i++) {
    MirVregId v = fn->fconsts[i].vreg;
    if (v >= 0 && (size_t)v < count && colorable[v]) {
      cost[v] = mir_scale_cost(cost[v], 32);
    }
  }
  for (size_t i = 0; i < fn->insn_count; i++) {
    const MirInst *in = &fn->insns[i];
    if (in->op == MIR_LEA_FUNC && in->dst.kind == MIR_OPK_VREG) {
      MirVregId v = in->dst.vreg;
      if (v >= 0 && (size_t)v < count && colorable[v]) {
        cost[v] = mir_scale_cost(cost[v], 128);
      }
    }
  }
}

static int mir_color_graph(MirFunction *fn, const BinaryGpRegister *gp_leaf_pool,
                           size_t gp_leaf_n,
                           const BinaryGpRegister *gp_cross_pool,
                           size_t gp_cross_n, int *next_spill, int allow_rbp) {
  size_t N = fn->vreg_count;
  if (N == 0) {
    return 1;
  }
  size_t words = (N + 63) / 64;
  uint64_t *inter = (uint64_t *)calloc(N * words, sizeof(uint64_t));
  uint32_t *mask = (uint32_t *)calloc(N, sizeof(uint32_t));
  int *degree = (int *)calloc(N, sizeof(int));
  int *cost = (int *)calloc(N, sizeof(int));
  int *colorable = (int *)calloc(N, sizeof(int));
  int *removed = (int *)calloc(N, sizeof(int));
  int *reg_count = (int *)calloc(N, sizeof(int));
  long long *metric = (long long *)calloc(N, sizeof(long long));
  MirVregId *stack = (MirVregId *)malloc(N * sizeof(MirVregId));
  MirVregId *narrow_src = mir_build_narrowing_extend_map(fn);
  if (!inter || !mask || !degree || !cost || !colorable || !removed ||
      !reg_count || !metric || !stack) {
    free(inter); free(mask); free(degree); free(cost); free(colorable);
    free(removed); free(reg_count); free(metric); free(stack);
    free(narrow_src);
    return 0;
  }
#define MIR_METRIC(v) ((long long)cost[v] * 1000 / (degree[v] + 1))
#define MIR_INTER_SET(a, b)                                                    \
  (inter[(size_t)(a) * words + ((size_t)(b) >> 6)] |= (uint64_t)1                \
                                                       << ((size_t)(b) & 63))
#define MIR_INTER_GET(a, b)                                                    \
  ((inter[(size_t)(a) * words + ((size_t)(b) >> 6)] >>                          \
    ((size_t)(b) & 63)) & 1u)
#define MIR_INTER_ADD(a, b)                                                     \
  do {                                                                          \
    if ((a) != (b) && !MIR_INTER_GET((a), (b))) {                               \
      MIR_INTER_SET((a), (b));                                                  \
      MIR_INTER_SET((b), (a));                                                  \
      degree[(a)]++;                                                            \
      degree[(b)]++;                                                            \
    }                                                                           \
  } while (0)
#define MIR_INTER_FOR_EACH(a, bvar)                                            \
  for (size_t w_ = 0; w_ < words; w_++)                                        \
    for (uint64_t bits_ = inter[(size_t)(a) * words + w_], bvar;               \
         bits_ && ((bvar = w_ * 64 + (size_t)__builtin_ctzll(bits_)), 1);       \
         bits_ &= bits_ - 1)

  for (size_t v = 0; v < N; v++) {
    MirVreg *vr = &fn->vregs[v];
    if (vr->live_start == MIR_LIVE_NONE || vr->address_taken) {
      continue;
    }
    colorable[v] = 1;
    mask[v] = mir_color_reg_mask(fn, (MirVregId)v, gp_leaf_pool, gp_leaf_n,
                                 gp_cross_pool, gp_cross_n, allow_rbp);
    reg_count[v] = __builtin_popcount(mask[v]);
    cost[v] = 1;
  }
  unsigned char *use_depth = (unsigned char *)calloc(N, sizeof(*use_depth));
  if (!use_depth) {
    free(inter); free(mask); free(degree); free(cost); free(colorable);
    free(removed); free(reg_count); free(metric); free(stack);
    free(narrow_src);
    return 0;
  }
  mir_color_spill_costs(fn, colorable, cost, use_depth, N);

  {
    MirLiveCfg cfg;
    size_t branch_count = 0;
    for (size_t i = 0; i < fn->insn_count; i++) {
      if (mir_inst_ends_block(&fn->insns[i])) {
        branch_count++;
      }
    }
    static int interval_only = -1;
    if (interval_only < 0) {
      interval_only = getenv("METTLE_INTERVAL_INTERFERENCE") ? 1 : 0;
    }
    int use_cfg = !interval_only && fn->insn_count >= 8 && branch_count >= 1 &&
                  (unsigned long long)fn->insn_count *
                          (unsigned long long)words <=
                      MIR_LIVE_CFG_MAX_WORK;
    if (mir_env_regalloc_trace()) {
      fprintf(stderr, "RA\t%s\tvregs=%zu\tinsns=%zu\tblocks=%zu\tcfg=%d\n",
              mir_ra_trace_name(), N, fn->insn_count, branch_count, use_cfg);
    }
    int have_cfg = use_cfg && mir_live_cfg_build(fn, &cfg, 1);
    unsigned long long *live =
        have_cfg ? (unsigned long long *)malloc(words * sizeof(*live)) : NULL;
    int exact_graph = have_cfg && live;
    if (exact_graph) {
      for (size_t block = 0; block < cfg.block_count; block++) {
        size_t lo = (size_t)cfg.block_start[block];
        size_t hi = block + 1 < cfg.block_count
                        ? (size_t)cfg.block_start[block + 1]
                        : fn->insn_count;
        memcpy(live, cfg.live_out + block * words,
               words * sizeof(*live));
        for (size_t at = hi; at-- > lo;) {
          const MirInst *in = &fn->insns[at];
          if (in->dst.kind == MIR_OPK_VREG) {
            MirVregId d = in->dst.vreg;
            if (d >= 0 && (size_t)d < N) {
              if (colorable[d]) {
                for (size_t w = 0; w < words; w++) {
                  uint64_t bits = live[w];
                  while (bits) {
                    size_t v = w * 64 + (size_t)__builtin_ctzll(bits);
                    bits &= bits - 1;
                    if (v < N && colorable[v] &&
                        fn->vregs[d].rclass == fn->vregs[v].rclass) {
                      MIR_INTER_ADD((size_t)d, v);
                    }
                  }
                }
              }
              live[(size_t)d >> 6] &= ~(1ull << ((size_t)d & 63));
            }
          }
          mir_live_add_operand(fn, &in->dst, live, 1);
          mir_live_add_operand(fn, &in->a, live, 0);
          mir_live_add_operand(fn, &in->b, live, 0);
        }
      }

      for (size_t a = 0; a < N; a++) {
        if (!colorable[a] || !mir_live_bit_get(cfg.live_in, a)) {
          continue;
        }
        for (size_t b = a + 1; b < N; b++) {
          if (colorable[b] && mir_live_bit_get(cfg.live_in, b) &&
              fn->vregs[a].rclass == fn->vregs[b].rclass) {
            MIR_INTER_ADD(a, b);
          }
        }
      }
    } else {
      for (size_t a = 0; a < N; a++) {
        if (!colorable[a]) {
          continue;
        }
        for (size_t b = a + 1; b < N; b++) {
          if (colorable[b] &&
              mir_color_interferes(&fn->vregs[a], &fn->vregs[b])) {
            MIR_INTER_ADD(a, b);
          }
        }
      }
    }
    int exact_pressure = 0;
    int interval_pressure = 0;
    if (exact_graph) {
      uint64_t *interval_inter =
          (uint64_t *)calloc(N * words, sizeof(*interval_inter));
      int *interval_degree = (int *)calloc(N, sizeof(*interval_degree));
      for (size_t a = 0; a < N; a++) {
        if (!colorable[a]) {
          continue;
        }
        if (degree[a] >= reg_count[a]) {
          exact_pressure++;
        }
        if (interval_inter && interval_degree) {
          for (size_t b = a + 1; b < N; b++) {
            if (colorable[b] &&
                mir_color_interferes(&fn->vregs[a], &fn->vregs[b])) {
              interval_inter[a * words + (b >> 6)] |= 1ull << (b & 63);
              interval_inter[b * words + (a >> 6)] |= 1ull << (a & 63);
              interval_degree[a]++;
              interval_degree[b]++;
            }
          }
        }
      }
      if (interval_inter && interval_degree) {
        for (size_t a = 0; a < N; a++) {
          if (colorable[a] && interval_degree[a] >= reg_count[a]) {
            interval_pressure++;
          }
        }
        if (mir_env_regalloc_trace()) {
          fprintf(stderr, "RA-PRESSURE\t%s\tinterval=%d\texact=%d\n",
                  mir_ra_trace_name(), interval_pressure, exact_pressure);
        }
        if (interval_pressure - exact_pressure <
            (int)MIR_GP_LEAF_POOL_MAX) {
          free(inter);
          free(degree);
          inter = interval_inter;
          degree = interval_degree;
          interval_inter = NULL;
          interval_degree = NULL;
        }
      }
      free(interval_inter);
      free(interval_degree);
    }
    free(live);
    if (have_cfg) {
      mir_live_cfg_free(&cfg);
    }
  }

  if (narrow_src) {
    for (size_t v = 0; v < N; v++) {
      MirVregId s = narrow_src[v];
      if (!colorable[v] || s == MIR_VREG_NONE || (size_t)s >= N ||
          !colorable[s] || (size_t)s == v || MIR_INTER_GET(v, s)) {
        continue;
      }
      MIR_INTER_SET(v, s);
      MIR_INTER_SET(s, v);
      degree[v]++;
      degree[s]++;
    }
  }

  size_t sp = 0;
  size_t remaining = 0;
  for (size_t v = 0; v < N; v++) {
    if (colorable[v]) {
      remaining++;
      metric[v] = MIR_METRIC(v);
    }
  }
  while (remaining > 0) {
    MirVregId pick = MIR_VREG_NONE;
    long long best_simplify = -1;
    int best_simplify_rank = MIR_MAX_WEIGHTED_DEPTH + 1;
    for (size_t v = 0; v < N; v++) {
      if (colorable[v] && !removed[v] && degree[v] < reg_count[v]) {
        int rank = mir_spill_rank(fn, use_depth, (MirVregId)v);
        int better;
        if (pick == MIR_VREG_NONE) {
          better = 1;
        } else if (rank != best_simplify_rank) {
          better = (rank < best_simplify_rank);
        } else {
          better = (metric[v] < best_simplify);
        }
        if (better) {
          best_simplify = metric[v];
          best_simplify_rank = rank;
          pick = (MirVregId)v;
        }
      }
    }
    if (pick == MIR_VREG_NONE) {
      long long best = -1;
      int best_rank = MIR_MAX_WEIGHTED_DEPTH + 1;
      for (size_t v = 0; v < N; v++) {
        int rank;
        int better;
        if (!colorable[v] || removed[v]) {
          continue;
        }
        rank = mir_spill_rank(fn, use_depth, (MirVregId)v);
        if (pick == MIR_VREG_NONE) {
          better = 1;
        } else if (rank != best_rank) {
          better = (rank < best_rank);
        } else {
          better = (metric[v] < best);
        }
        if (better) {
          best = metric[v];
          best_rank = rank;
          pick = (MirVregId)v;
        }
      }
    }
    removed[pick] = 1;
    stack[sp++] = pick;
    remaining--;
    MIR_INTER_FOR_EACH(pick, b) {
      if (!removed[b]) {
        degree[b]--;
        metric[b] = MIR_METRIC(b);
      }
    }
  }

  while (sp > 0) {
    MirVregId v = stack[--sp];
    MirVreg *vr = &fn->vregs[v];
    uint32_t used = 0;
    MIR_INTER_FOR_EACH(v, b) {
      if (fn->vregs[b].in_register) {
        used |= 1u << fn->vregs[b].phys;
      }
    }
    uint32_t avail = mask[v] & ~used;
    if (avail == 0) {
      *next_spill += vr->width > 8 ? 16 : 8;
      vr->assigned = 1;
      vr->in_register = 0;
      vr->spill_offset = *next_spill;
      continue;
    }
    uint32_t preferred = avail;
    int avoid = mir_narrowing_avoid_reg(fn, narrow_src, v);
    if (avoid >= 0 && (preferred & ~(1u << (unsigned)avoid)) != 0) {
      preferred &= ~(1u << (unsigned)avoid);
    }
    int chosen = -1;
    if (vr->coalesce_hint != MIR_VREG_NONE) {
      MirVreg *hv = &fn->vregs[vr->coalesce_hint];
      if (hv->in_register && (preferred & (1u << hv->phys))) {
        chosen = hv->phys;
      }
    }
    /* A callee-saved register costs a store in the prologue and a load at
       every exit, paid on each call. A value that outlives no call has no
       need of one, so take a volatile register while any is free and
       leave the saved set empty. Scanning by encoding number alone
       reaches RBX, encoding 3, before RSI, RDI, R8 and R9. */
    for (int pass = 0; pass < 2 && chosen < 0; pass++) {
      for (int r = 0; r < 16; r++) {
        if (!(preferred & (1u << r))) {
          continue;
        }
        int nonvol = mir_gp_is_nonvolatile((BinaryGpRegister)r) ||
                     r == BINARY_GP_RBP;
        if (nonvol == pass) {
          chosen = r;
          break;
        }
      }
    }
    vr->assigned = 1;
    vr->in_register = 1;
    vr->phys = chosen;
  }

  int coalesced = 1;
  int coalesce_rounds = 0;
  while (coalesced && coalesce_rounds++ < 16) {
    coalesced = 0;
    for (size_t i = 0; i < fn->insn_count; i++) {
      const MirInst *in = &fn->insns[i];
      if (in->op != MIR_MOV || in->dst.kind != MIR_OPK_VREG ||
          in->a.kind != MIR_OPK_VREG) {
        continue;
      }
      MirVregId d = in->dst.vreg;
      MirVregId s = in->a.vreg;
      if (d < 0 || s < 0 || (size_t)d >= N || (size_t)s >= N || d == s ||
          !colorable[d] || !colorable[s]) {
        continue;
      }
      MirVreg *dv = &fn->vregs[d];
      MirVreg *sv = &fn->vregs[s];
      if (!dv->in_register || !sv->in_register || dv->rclass != sv->rclass ||
          dv->phys == sv->phys || sv->live_end != (int)i ||
          MIR_INTER_GET(d, s) || !(mask[d] & (1u << sv->phys))) {
        continue;
      }
      uint32_t used = 0;
      MIR_INTER_FOR_EACH(d, b) {
        if (fn->vregs[b].in_register) {
          used |= 1u << fn->vregs[b].phys;
        }
      }
      if (used & (1u << sv->phys)) {
        continue;
      }
      dv->phys = sv->phys;
      coalesced = 1;
    }
  }

#undef MIR_METRIC
#undef MIR_INTER_SET
#undef MIR_INTER_GET
#undef MIR_INTER_ADD
  free(inter); free(mask); free(degree); free(cost); free(colorable);
  free(removed); free(reg_count); free(metric); free(stack);
  free(narrow_src); free(use_depth);
  return 1;
}

static int mir_regalloc_report_saved(MirFunction *fn) {
  if (!fn->context) {
    return 1;
  }
  int used_nonvol[16];
  memset(used_nonvol, 0, sizeof(used_nonvol));
  for (size_t i = 0; i < fn->vreg_count; i++) {
    MirVreg *vr = &fn->vregs[i];
    if (vr->in_register && vr->rclass == MIR_RC_GP &&
        (mir_gp_is_nonvolatile((BinaryGpRegister)vr->phys) ||
         vr->phys == BINARY_GP_RBP)) {
      used_nonvol[vr->phys] = 1;
    }
  }
  for (size_t i = 0; i < fn->insn_count; i++) {
    if (fn->insns[i].op == MIR_INLINE_ASM) {
      for (int reg = 0; reg < 16; reg++) {
        if (mir_gp_is_nonvolatile((BinaryGpRegister)reg)) {
          used_nonvol[reg] = 1;
        }
      }
      continue;
    }
    if (fn->insns[i].op != MIR_IR_KERNEL) {
      continue;
    }
    const MirKernelAux *ka = (const MirKernelAux *)fn->insns[i].aux;
    const MirIrKernel *kern = ka ? mir_ir_kernel_at(ka->kernel_index) : NULL;
    unsigned clobbers = kern ? kern->gp_clobbers : 0u;
    for (int reg = 0; clobbers; reg++, clobbers >>= 1) {
      if ((clobbers & 1u) && (mir_gp_is_nonvolatile((BinaryGpRegister)reg) ||
                              reg == BINARY_GP_RBP)) {
        used_nonvol[reg] = 1;
      }
    }
  }
  for (int reg = 0; reg < 16; reg++) {
    if (used_nonvol[reg] && !code_generator_binary_context_add_saved_register(
                                fn->context, (BinaryGpRegister)reg)) {
      return 0;
    }
  }
  int used_xmm[16];
  memset(used_xmm, 0, sizeof(used_xmm));
  for (size_t i = 0; i < fn->vreg_count; i++) {
    MirVreg *vr = &fn->vregs[i];
    if (vr->in_register && vr->rclass == MIR_RC_XMM && vr->phys >= 8) {
      used_xmm[vr->phys] = 1;
    }
  }
  for (int reg = 8; reg < 16; reg++) {
    if (used_xmm[reg] && !code_generator_binary_context_add_saved_xmm_register(
                             fn->context, (BinaryXmmRegister)reg)) {
      return 0;
    }
  }
  return 1;
}

static int mir_regalloc_color(MirFunction *fn) {
  mir_compute_liveness(fn);
  mir_compute_coalesce_hints(fn);
  mir_mark_crosses_call(fn);

  int next_spill = fn->context ? fn->context->raw_frame_size : 0;
  fn->preserve_slot = 0;
  fn->preserve_xmm_slot = 0;
  if (mir_fn_has_preserving_call(fn, 0)) {
    next_spill += 8;
    fn->preserve_slot = next_spill;
  }
  if (mir_fn_has_preserving_call(fn, 1)) {
    next_spill += (int)MIR_XMM_POOL_COUNT * 8;
    fn->preserve_xmm_slot = next_spill;
  }
  for (size_t v = 0; v < fn->vreg_count; v++) {
    MirVreg *vr = &fn->vregs[v];
    if (vr->address_taken) {
      int home = mir_home_bytes_for(vr, &next_spill);
      next_spill += home;
      vr->assigned = 1;
      vr->in_register = 0;
      vr->spill_offset = next_spill;
    }
  }

  BinaryGpRegister gp_leaf_pool[MIR_GP_LEAF_POOL_MAX];
  size_t gp_leaf_n = mir_build_gp_leaf_pool(
      gp_leaf_pool, fn->param_count + (fn->returns_indirect ? 1 : 0),
      !mir_fn_has_real_calls(fn));
  BinaryGpRegister gp_cross_pool[MIR_GP_CROSSCALL_POOL_MAX];
  size_t gp_cross_n = mir_build_gp_crosscall_pool(gp_cross_pool);

  int allow_rbp = fn->context && fn->context->omit_frame_pointer &&
                  !mir_fn_uses_slp(fn);
  if (!mir_color_graph(fn, gp_leaf_pool, gp_leaf_n, gp_cross_pool, gp_cross_n,
                       &next_spill, allow_rbp)) {
    fn->has_error = 1;
    return 0;
  }
  mir_drop_unused_preserves(fn);
  if (mir_env_regalloc_trace()) {
    size_t spilled = 0, kept = 0;
    for (size_t v = 0; v < fn->vreg_count; v++) {
      const MirVreg *vr = &fn->vregs[v];
      if (vr->live_start == MIR_LIVE_NONE || vr->address_taken) {
        continue;
      }
      if (vr->assigned && vr->in_register) {
        kept++;
      } else if (vr->assigned) {
        spilled++;
      }
    }
    fprintf(stderr, "RA-DONE\t%s\tkept=%zu\tspilled=%zu\n",
            mir_ra_trace_name(), kept, spilled);
  }
  fn->spill_bytes = next_spill - (fn->context ? fn->context->raw_frame_size : 0);
  if (!mir_regalloc_report_saved(fn)) {
    fn->has_error = 1;
    return 0;
  }
  return 1;
}

static int mir_op_pure_def(MirOpcode op) {
  return mir_op_has(op, MIR_OPF_PURE_DEF);
}

static void mir_dce_add_read(MirVregId v, int *reads, size_t n) {
  if (v >= 0 && (size_t)v < n) {
    reads[v]++;
  }
}

static void mir_dce_count_operand(const MirOperand *op, int *reads, size_t n) {
  if (op->kind == MIR_OPK_VREG) {
    mir_dce_add_read(op->vreg, reads, n);
  } else if (op->kind == MIR_OPK_MEM) {
    mir_dce_add_read(op->mem.base, reads, n);
    mir_dce_add_read(op->mem.index, reads, n);
  }
}

static void mir_dce(MirFunction *fn) {
  if (fn->vreg_count == 0 || fn->insn_count == 0) {
    return;
  }
  int *reads = (int *)malloc(fn->vreg_count * sizeof(int));
  if (!reads) {
    return;
  }
  int changed = 1;
  while (changed) {
    changed = 0;
    memset(reads, 0, fn->vreg_count * sizeof(int));
    for (size_t i = 0; i < fn->insn_count; i++) {
      const MirInst *in = &fn->insns[i];
      if (in->op == MIR_NOP) {
        continue;
      }
      mir_dce_count_operand(&in->a, reads, fn->vreg_count);
      mir_dce_count_operand(&in->b, reads, fn->vreg_count);
      if (in->dst.kind == MIR_OPK_MEM) {
        mir_dce_add_read(in->dst.mem.base, reads, fn->vreg_count);
        mir_dce_add_read(in->dst.mem.index, reads, fn->vreg_count);
      }
    }
    for (size_t i = 0; i < fn->insn_count; i++) {
      MirInst *in = &fn->insns[i];
      if (in->op == MIR_NOP || !mir_op_pure_def(in->op)) {
        continue;
      }
      if (in->dst.kind != MIR_OPK_VREG) {
        continue;
      }
      if (in->op == MIR_MOV && in->a.kind == MIR_OPK_MEM) {
        continue;
      }
      MirVregId d = in->dst.vreg;
      if (d < 0 || (size_t)d >= fn->vreg_count ||
          d == fn->indirect_return_vreg) {
        continue;
      }
      if (reads[d] == 0) {
        in->op = MIR_NOP;
        changed = 1;
      }
    }
  }
  free(reads);
}

int mir_regalloc(MirFunction *fn) {
  if (!fn) {
    return 0;
  }
  if (fn->vreg_count == 0) {
    return 1;
  }

  mir_clobber_index_reset();

  mir_dce(fn);

  if (fn->context && fn->context->omit_frame_pointer && mir_fn_has_calls(fn)) {
    fn->context->omit_frame_pointer = 0;
  }

  {
    static int linear = -1;
    if (linear < 0) {
      linear = getenv("METTLE_LINEAR_ALLOC") ? 1 : 0;
    }
    if (!linear) {
      return mir_regalloc_color(fn);
    }
  }

  mir_compute_liveness(fn);
  mir_compute_coalesce_hints(fn);

  mir_mark_crosses_call(fn);

  size_t order_count = 0;
  MirVregId *order = mir_order_by_start(fn, &order_count);
  if (fn->has_error) {
    free(order);
    return 0;
  }
  MirVregId *narrow_src = mir_build_narrowing_extend_map(fn);

  int gp_held_by[16];
  int xmm_held_by[16];
  for (int i = 0; i < 16; i++) {
    gp_held_by[i] = -1;
    xmm_held_by[i] = -1;
  }
  xmm_held_by[BINARY_XMM4] = -2;
  xmm_held_by[BINARY_XMM5] = -2;
  BinaryGpRegister gp_leaf_pool[MIR_GP_LEAF_POOL_MAX];
  size_t gp_leaf_pool_count = mir_build_gp_leaf_pool(
      gp_leaf_pool, fn->param_count + (fn->returns_indirect ? 1 : 0),
      !mir_fn_has_real_calls(fn));
  BinaryGpRegister gp_cross_pool[MIR_GP_CROSSCALL_POOL_MAX];
  size_t gp_cross_pool_count = mir_build_gp_crosscall_pool(gp_cross_pool);
  for (int r = 0; r < 16; r++) {
    gp_held_by[r] = -2;
  }
  for (size_t i = 0; i < gp_leaf_pool_count; i++) {
    gp_held_by[gp_leaf_pool[i]] = -1;
  }

  int next_spill_offset = fn->context ? fn->context->raw_frame_size : 0;
  fn->preserve_slot = 0;
  fn->preserve_xmm_slot = 0;
  if (mir_fn_has_preserving_call(fn, 0)) {
    next_spill_offset += 8;
    fn->preserve_slot = next_spill_offset;
  }
  if (mir_fn_has_preserving_call(fn, 1)) {
    next_spill_offset += (int)MIR_XMM_POOL_COUNT * 8;
    fn->preserve_xmm_slot = next_spill_offset;
  }

  for (size_t v = 0; v < fn->vreg_count; v++) {
    MirVreg *vr = &fn->vregs[v];
    if (vr->address_taken) {
      int home = mir_home_bytes_for(vr, &next_spill_offset);
      next_spill_offset += home;
      vr->assigned = 1;
      vr->in_register = 0;
      vr->spill_offset = next_spill_offset;
    }
  }

  MirVregId *active = (MirVregId *)malloc(order_count * sizeof(MirVregId));
  if (!active && order_count > 0) {
    free(order);
    free(narrow_src);
    fn->has_error = 1;
    return 0;
  }
  size_t active_count = 0;

  for (size_t oi = 0; oi < order_count; oi++) {
    MirVregId cur = order[oi];
    MirVreg *cv = &fn->vregs[cur];
    int point = cv->live_start;

    size_t w = 0;
    for (size_t r = 0; r < active_count; r++) {
      MirVregId a = active[r];
      MirVreg *av = &fn->vregs[a];
      if (av->live_end < point) {
        if (av->in_register) {
          if (av->rclass == MIR_RC_XMM) {
            xmm_held_by[av->phys] = -1;
          } else {
            gp_held_by[av->phys] = -1;
          }
        }
      } else {
        active[w++] = a;
      }
    }
    active_count = w;

    if (cv->address_taken) {
      continue;
    }

    int got_reg = 0;
    if (cv->rclass == MIR_RC_GP && !cv->crosses_call &&
        cv->coalesce_hint != MIR_VREG_NONE) {
      MirVreg *hv = &fn->vregs[cv->coalesce_hint];
      if (hv->in_register && hv->rclass == MIR_RC_GP &&
          hv->live_end == point && gp_held_by[hv->phys] == cv->coalesce_hint &&
          !mir_reg_clobbered_in_range(fn, (BinaryGpRegister)hv->phys,
                                      cv->live_start, cv->live_end)) {
        cv->phys = hv->phys;
        cv->assigned = 1;
        cv->in_register = 1;
        gp_held_by[hv->phys] = cur;
        for (size_t r = 0; r < active_count; r++) {
          if (active[r] == cv->coalesce_hint) {
            active[r] = active[--active_count];
            break;
          }
        }
        got_reg = 1;
      }
    }
    if (!got_reg && cv->rclass == MIR_RC_XMM) {
      if (!cv->crosses_call || cv->crosses_xmm_preserving_only) {
        for (size_t p = 0; !fn->has_xmm_arg_call && p < MIR_XMM_POOL_COUNT; p++) {
          BinaryXmmRegister reg = MIR_XMM_POOL[p];
          if (xmm_held_by[reg] == -1) {
            xmm_held_by[reg] = cur;
            cv->assigned = 1;
            cv->in_register = 1;
            cv->phys = reg;
            got_reg = 1;
            break;
          }
        }
        for (size_t p = 0;
             !got_reg && !cv->crosses_call && p < MIR_XMM_NONVOL_POOL_COUNT;
             p++) {
          BinaryXmmRegister reg = MIR_XMM_NONVOL_POOL[p];
          if (xmm_held_by[reg] == -1) {
            xmm_held_by[reg] = cur;
            cv->assigned = 1;
            cv->in_register = 1;
            cv->phys = reg;
            got_reg = 1;
            break;
          }
        }
      }
    } else if (!got_reg) {
      BinaryGpRegister cross_ext[MIR_GP_CROSSCALL_POOL_EXT];
      size_t cross_ext_n =
          mir_cross_pool_for(cv, gp_cross_pool, gp_cross_pool_count, cross_ext);
      const BinaryGpRegister *pool =
          cv->crosses_call ? cross_ext : gp_leaf_pool;
      size_t pool_n = cv->crosses_call ? cross_ext_n : gp_leaf_pool_count;
      int avoid = mir_narrowing_avoid_reg(fn, narrow_src, cur);
      for (int relax = 0; !got_reg && relax < 2; relax++) {
        for (size_t p = 0; p < pool_n; p++) {
          BinaryGpRegister reg = pool[p];
          if (relax == 0 && avoid >= 0 && (int)reg == avoid) {
            continue;
          }
          if (gp_held_by[reg] == -1 &&
              !mir_reg_clobbered_in_range(fn, reg, cv->live_start,
                                          cv->live_end)) {
            gp_held_by[reg] = cur;
            cv->assigned = 1;
            cv->in_register = 1;
            cv->phys = reg;
            got_reg = 1;
            break;
          }
        }
        if (avoid < 0) {
          break;
        }
      }
    }

    if (got_reg) {
      active[active_count++] = cur;
      continue;
    }

    if (cv->crosses_call) {
      next_spill_offset += cv->width > 8 ? 16 : 8;
      cv->assigned = 1;
      cv->in_register = 0;
      cv->spill_offset = next_spill_offset;
      continue;
    }

    MirVregId spill_victim = MIR_VREG_NONE;
    int victim_end = -1;
    int victim_lc = 1;
    for (size_t r = 0; r < active_count; r++) {
      MirVregId a = active[r];
      MirVreg *av = &fn->vregs[a];
      if (av->rclass != cv->rclass || !av->in_register) {
        continue;
      }
      if (av->rclass == MIR_RC_GP &&
          mir_reg_clobbered_in_range(fn, (BinaryGpRegister)av->phys,
                                     cv->live_start, cv->live_end)) {
        continue;
      }
      int better;
      if (spill_victim == MIR_VREG_NONE) {
        better = 1;
      } else if (av->loop_carried != victim_lc) {
        better = (av->loop_carried < victim_lc);
      } else {
        better = (av->live_end > victim_end);
      }
      if (better) {
        victim_end = av->live_end;
        victim_lc = av->loop_carried;
        spill_victim = a;
      }
    }

    int prefer_victim = 0;
    if (spill_victim != MIR_VREG_NONE) {
      if (victim_lc != cv->loop_carried) {
        prefer_victim = (cv->loop_carried && !victim_lc);
      } else {
        prefer_victim = fn->vregs[spill_victim].live_end > cv->live_end;
      }
    }
    if (prefer_victim) {
      MirVreg *vv = &fn->vregs[spill_victim];
      int reg = vv->phys;
      next_spill_offset += vv->width > 8 ? 16 : 8;
      vv->in_register = 0;
      vv->assigned = 1;
      vv->spill_offset = next_spill_offset;
      cv->assigned = 1;
      cv->in_register = 1;
      cv->phys = reg;
      if (cv->rclass == MIR_RC_XMM) {
        xmm_held_by[reg] = cur;
      } else {
        gp_held_by[reg] = cur;
      }
      for (size_t r = 0; r < active_count; r++) {
        if (active[r] == spill_victim) {
          active[r] = cur;
          break;
        }
      }
    } else {
      next_spill_offset += cv->width > 8 ? 16 : 8;
      cv->assigned = 1;
      cv->in_register = 0;
      cv->spill_offset = next_spill_offset;
    }
  }

  mir_drop_unused_preserves(fn);
  fn->spill_bytes =
      next_spill_offset - (fn->context ? fn->context->raw_frame_size : 0);

  if (fn->context) {
    int used_nonvol[16];
    memset(used_nonvol, 0, sizeof(used_nonvol));
    for (size_t i = 0; i < fn->vreg_count; i++) {
      MirVreg *vr = &fn->vregs[i];
      if (vr->in_register && vr->rclass == MIR_RC_GP &&
          mir_gp_is_nonvolatile((BinaryGpRegister)vr->phys)) {
        used_nonvol[vr->phys] = 1;
      }
    }
    for (int reg = 0; reg < 16; reg++) {
      if (used_nonvol[reg] &&
          !code_generator_binary_context_add_saved_register(
              fn->context, (BinaryGpRegister)reg)) {
        free(order);
        free(active);
        free(narrow_src);
        fn->has_error = 1;
        return 0;
      }
    }

    int used_xmm[16];
    memset(used_xmm, 0, sizeof(used_xmm));
    for (size_t i = 0; i < fn->vreg_count; i++) {
      MirVreg *vr = &fn->vregs[i];
      if (vr->in_register && vr->rclass == MIR_RC_XMM && vr->phys >= 8) {
        used_xmm[vr->phys] = 1;
      }
    }
    for (int reg = 8; reg < 16; reg++) {
      if (used_xmm[reg] &&
          !code_generator_binary_context_add_saved_xmm_register(
              fn->context, (BinaryXmmRegister)reg)) {
        free(order);
        free(active);
        free(narrow_src);
        fn->has_error = 1;
        return 0;
      }
    }
  }

  free(order);
  free(active);
  free(narrow_src);
  return 1;
}
