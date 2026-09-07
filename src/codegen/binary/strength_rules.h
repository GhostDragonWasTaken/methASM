#ifndef CODEGEN_BINARY_STRENGTH_RULES_H
#define CODEGEN_BINARY_STRENGTH_RULES_H

#include <stdint.h>

typedef enum {
  CG_SR_NONE = 0,
  CG_SR_MUL_SHL,
  CG_SR_MUL_SHL_ADD,
  CG_SR_MUL_SHL_SUB,
  CG_SR_UDIV_SHR,
  CG_SR_UREM_AND,
  CG_SR_SDIV_POW2,
  CG_SR_SREM_POW2,
  CG_SR_DIV_MAGIC,
  CG_SR_REM_MAGIC
} CgStrengthKind;

typedef struct {
  CgStrengthKind kind;
  int shift;
  long long mask;
  long long magic;
  int magic_add;
} CgStrengthRewrite;

int cg_strength_classify(char op, long long c, int is_unsigned,
                         CgStrengthRewrite *out);

void cg_magic_s64(int64_t d, int64_t *magic_out, int *shift_out);

void cg_magic_u64(uint64_t d, uint64_t *magic_out, int *shift_out,
                  int *add_out);

#endif
