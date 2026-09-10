#ifndef CODEGEN_BINARY_MIR_COLOR_H
#define CODEGEN_BINARY_MIR_COLOR_H

#include "codegen/binary/mir.h"
#include "codegen/binary/mir_cfg.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
  MirCfg cfg;
  int valid;
  uint32_t *clobbered;
  unsigned char *a_dies;
  unsigned char *b_dies;
  unsigned char *undef_live;
  int max_live_gp;
  int max_live_xmm;
  size_t max_live_at;
} MirRaFacts;

typedef struct {
  MirFunction *fn;
  const MirRaFacts *facts;
  size_t count;
  size_t words;
  uint64_t *inter;
  uint32_t *mask;
  int *degree;
  int *cost;
  int *colorable;
  int *removed;
  int *reg_count;
  long long *metric;
  MirVregId *stack;
  MirVregId *narrow_src;
  unsigned char *use_depth;
  MirVregId *rep;
  int *phys_hint;
  MirVregId *copy_partner;
  size_t merged_copies;
} MirColorState;

#define MIR_INTER_FOR_EACH(st, a, bvar)                                        \
  for (size_t w_ = 0; w_ < (st)->words; w_++)                                  \
    for (uint64_t bits_ = (st)->inter[(size_t)(a) * (st)->words + w_], bvar;   \
         bits_ && ((bvar = w_ * 64 + (size_t)__builtin_ctzll(bits_)), 1);      \
         bits_ &= bits_ - 1)

static inline int mir_inter_get(const MirColorState *st, size_t a, size_t b) {
  return (int)((st->inter[a * st->words + (b >> 6)] >> (b & 63)) & 1u);
}

MirVregId mir_color_find(const MirColorState *st, MirVregId v);
int mir_color_coalesce(MirColorState *st);
void mir_color_note_phys_hints(MirColorState *st);
void mir_color_mirror_members(MirColorState *st);

#endif
