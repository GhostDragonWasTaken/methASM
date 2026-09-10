#ifndef CODEGEN_BINARY_MIR_CFG_H
#define CODEGEN_BINARY_MIR_CFG_H

#include "codegen/binary/mir.h"

#include <stddef.h>

typedef struct {
  int start;
  int end;
  int succ_head;
  int pred_head;
  int idom;
  int rpo;
  int loop_depth;
  int loop_header;
} MirCfgBlock;

typedef struct {
  const MirFunction *fn;
  MirCfgBlock *blocks;
  size_t block_count;
  int *block_of;
  int *edge_next;
  int *edge_to;
  size_t edge_count;
  size_t edge_capacity;
  int *rpo_order;
  size_t rpo_count;
  int *order;
  size_t words;
  unsigned long long *use;
  unsigned long long *def;
  unsigned long long *live_in;
  unsigned long long *live_out;
  unsigned long long *defd_in;
  unsigned long long *defd_out;
  unsigned char *insn_depth;
} MirCfg;

int mir_cfg_build(MirCfg *cfg, const MirFunction *fn);
void mir_cfg_free(MirCfg *cfg);

int mir_cfg_insn_uses(const MirInst *in, MirVregId *out);
MirVregId mir_cfg_insn_def(const MirInst *in);
int mir_cfg_insn_reads_dst(const MirInst *in);

typedef struct {
  const MirCfg *cfg;
  size_t block;
  size_t at;
  unsigned long long *live;
  unsigned long long *defd;
  int *count;
} MirCfgCursor;

int mir_cfg_cursor_init(MirCfgCursor *cur, const MirCfg *cfg);
void mir_cfg_cursor_free(MirCfgCursor *cur);
void mir_cfg_cursor_start_block(MirCfgCursor *cur, size_t block);
void mir_cfg_cursor_step_back(MirCfgCursor *cur);

static inline int mir_cfg_set_get(const unsigned long long *set, size_t v) {
  return (set[v >> 6] >> (v & 63)) & 1ull;
}

static inline void mir_cfg_set_add(unsigned long long *set, size_t v) {
  set[v >> 6] |= 1ull << (v & 63);
}

static inline void mir_cfg_set_clear(unsigned long long *set, size_t v) {
  set[v >> 6] &= ~(1ull << (v & 63));
}

#endif
