#ifndef CODEGEN_BINARY_ARM64_EMIT_H
#define CODEGEN_BINARY_ARM64_EMIT_H

#include "codegen/binary/arm64.h"

#include <stddef.h>

typedef struct {
  unsigned char *data;
  size_t len;
  size_t cap;
} Arm64Buf;

typedef enum {
  ARM64_FIX_B26 = 0,
  ARM64_FIX_IMM19 = 1,
  ARM64_FIX_ABS64_MOV = 2
} Arm64FixKind;

typedef struct {
  size_t at;
  int label;
  Arm64FixKind kind;
} Arm64Fixup;

typedef struct {
  Arm64Buf code;
  size_t *label_off;
  int *label_bound;
  int label_count;
  int label_cap;
  Arm64Fixup *fixups;
  int fixup_count;
  int fixup_cap;
  int error;
  char reason[192];
  uint64_t code_vaddr;
} Arm64Emit;

void arm64_emit_init(Arm64Emit *e);
void arm64_emit_free(Arm64Emit *e);

void arm64_fail(Arm64Emit *e, const char *fmt, ...);
const char *arm64_error_reason(const Arm64Emit *e);
size_t arm64_here(const Arm64Emit *e);
int arm64_emit_word(Arm64Emit *e, uint32_t word);
int arm64_emit_bytes(Arm64Emit *e, const void *data, size_t len);

int arm64_emit_mov(Arm64Emit *e, int is64, Arm64Reg rd, Arm64Reg rn);

int arm64_new_label(Arm64Emit *e);
void arm64_bind_label(Arm64Emit *e, int label);

int arm64_emit_b(Arm64Emit *e, int label);
int arm64_emit_bl(Arm64Emit *e, int label);
int arm64_emit_bcond(Arm64Emit *e, Arm64Cond cond, int label);
int arm64_emit_cbz(Arm64Emit *e, int is64, Arm64Reg rt, int label);
int arm64_emit_cbnz(Arm64Emit *e, int is64, Arm64Reg rt, int label);

int arm64_emit_label_address(Arm64Emit *e, Arm64Reg rd, int label);

int arm64_emit_finalize(Arm64Emit *e);

int arm64_emit_prologue(Arm64Emit *e, int frame_bytes, const Arm64Reg *saved,
                        int n_saved);
int arm64_emit_epilogue(Arm64Emit *e, int frame_bytes, const Arm64Reg *saved,
                        int n_saved);

#endif
