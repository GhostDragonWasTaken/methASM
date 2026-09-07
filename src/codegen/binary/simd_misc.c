#include "codegen/binary/internal.h"
#include "codegen/binary/simd_internal.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int code_generator_binary_emit_count_word_starts(
    CodeGenerator *generator, BinaryFunctionContext *context,
    const IRInstruction *instruction) {
  if (!generator || !context || !instruction ||
      instruction->dest.kind != IR_OPERAND_SYMBOL ||
      instruction->lhs.kind != IR_OPERAND_SYMBOL ||
      instruction->rhs.kind != IR_OPERAND_SYMBOL) {
    code_generator_set_error(generator, "Malformed count_word_starts in '%s'",
                             context ? context->function_name : "?");
    return 0;
  }

  BinaryCodeBuffer *b = &context->code;

  if (!code_generator_binary_emit_operand_load(generator, context,
                                               &instruction->lhs,
                                               BINARY_GP_RCX) ||
      !code_generator_binary_emit_operand_load(generator, context,
                                               &instruction->rhs,
                                               BINARY_GP_RDX) ||
      !code_generator_binary_emit_operand_load(generator, context,
                                               &instruction->dest,
                                               BINARY_GP_RAX)) {
    return 0;
  }
  if (!wcs_xor_self32(b, BINARY_GP_R8)) return 0;

  static const struct { unsigned int pat; int xmm; } CONSTS[4] = {
      {0x20202020u, 1}, {0x09090909u, 2}, {0x0A0A0A0Au, 3}, {0x0D0D0D0Du, 4}};
  for (int i = 0; i < 4; i++) {
    if (!wcs_mov_reg_imm32(b, BINARY_GP_R9, CONSTS[i].pat) ||
        !wcs_movd_xmm_reg(b, CONSTS[i].xmm, BINARY_GP_R9) ||
        !wcs_pshufd(b, CONSTS[i].xmm, CONSTS[i].xmm, 0x00)) {
      return 0;
    }
  }

  size_t loop_top = b->size;
  if (!wcs_cmp_reg_imm8(b, BINARY_GP_RDX, 16)) return 0;
  size_t j_to_tail;
  if (!wcs_jcc(b, 0x82 , &j_to_tail)) return 0;

  if (!wcs_movdqu_xmm_rcx(b, 0) ||
      !wcs_sse_66(b, 0x6F, 5, 0) ||
      !wcs_sse_66(b, 0x74, 0, 1) ||
      !wcs_sse_66(b, 0x6F, 6, 5) ||
      !wcs_sse_66(b, 0x74, 6, 2) ||
      !wcs_sse_66(b, 0xEB, 0, 6) ||
      !wcs_sse_66(b, 0x6F, 6, 5) ||
      !wcs_sse_66(b, 0x74, 6, 3) ||
      !wcs_sse_66(b, 0xEB, 0, 6) ||
      !wcs_sse_66(b, 0x6F, 6, 5) ||
      !wcs_sse_66(b, 0x74, 6, 4) ||
      !wcs_sse_66(b, 0xEB, 0, 6)) {
    return 0;
  }
  if (!wcs_pmovmskb(b, BINARY_GP_R9, 0) ||
      !wcs_mov_reg_reg32(b, BINARY_GP_R10, BINARY_GP_R9) ||
      !wcs_not_reg(b, BINARY_GP_R10) ||
      !binary_emit_and_reg_imm32(b, BINARY_GP_R10, 0xFFFF)) {
    return 0;
  }
  if (!wcs_mov_reg_reg32(b, BINARY_GP_R11, BINARY_GP_R10) ||
      !wcs_shift_reg_imm(b, BINARY_GP_R11, 0, 1) ||
      !wcs_or_reg_reg(b, BINARY_GP_R11, BINARY_GP_R8)) {
    return 0;
  }
  if (!wcs_mov_reg_reg32(b, BINARY_GP_R8, BINARY_GP_R10) ||
      !wcs_shift_reg_imm(b, BINARY_GP_R8, 1, 15) ||
      !binary_emit_and_reg_imm32(b, BINARY_GP_R8, 1)) {
    return 0;
  }
  if (!wcs_not_reg(b, BINARY_GP_R11) ||
      !wcs_and_reg_reg(b, BINARY_GP_R11, BINARY_GP_R10) ||
      !binary_emit_and_reg_imm32(b, BINARY_GP_R11, 0xFFFF) ||
      !wcs_popcnt(b, BINARY_GP_R11, BINARY_GP_R11) ||
      !wcs_add_reg_reg64(b, BINARY_GP_RAX, BINARY_GP_R11)) {
    return 0;
  }
  if (!wcs_addsub_reg_imm8(b, BINARY_GP_RCX, 0, 16) ||
      !wcs_addsub_reg_imm8(b, BINARY_GP_RDX, 1, 16)) {
    return 0;
  }
  size_t j_back;
  if (!wcs_jcc(b, 0, &j_back)) return 0;
  if (!wcs_patch_to(b, j_back, loop_top)) {
    code_generator_set_error(generator, "wcs back-jump out of range");
    return 0;
  }

  if (!wcs_patch_here(b, j_to_tail)) return 0;
  if (!wcs_cmp_reg_imm8(b, BINARY_GP_RDX, 0)) return 0;
  size_t j_done_early;
  if (!wcs_jcc(b, 0x84 , &j_done_early)) return 0;

  size_t tail_top = b->size;
  if (!wcs_movzx_reg_byte_rcx(b, BINARY_GP_R9)) return 0;
  size_t j_ws[4];
  static const unsigned int WS[4] = {32u, 9u, 10u, 13u};
  for (int i = 0; i < 4; i++) {
    if (!wcs_cmp_reg_imm32(b, BINARY_GP_R9, WS[i]) ||
        !wcs_jcc(b, 0x84 , &j_ws[i])) {
      return 0;
    }
  }
  if (!wcs_test_reg_reg32(b, BINARY_GP_R8)) return 0;
  size_t j_skip_inc;
  if (!wcs_jcc(b, 0x85 , &j_skip_inc)) return 0;
  if (!wcs_addsub_reg_imm8(b, BINARY_GP_RAX, 0, 1)) return 0;
  if (!wcs_patch_here(b, j_skip_inc)) return 0;
  if (!wcs_mov_reg_imm32(b, BINARY_GP_R8, 1)) return 0;
  size_t j_after_class;
  if (!wcs_jcc(b, 0, &j_after_class)) return 0;
  for (int i = 0; i < 4; i++) {
    if (!wcs_patch_here(b, j_ws[i])) return 0;
  }
  if (!wcs_xor_self32(b, BINARY_GP_R8)) return 0;
  if (!wcs_patch_here(b, j_after_class)) return 0;
  if (!wcs_addsub_reg_imm8(b, BINARY_GP_RCX, 0, 1) ||
      !wcs_addsub_reg_imm8(b, BINARY_GP_RDX, 1, 1)) {
    return 0;
  }
  if (!wcs_cmp_reg_imm8(b, BINARY_GP_RDX, 0)) return 0;
  size_t j_tail_back;
  if (!wcs_jcc(b, 0x85 , &j_tail_back)) return 0;
  if (!wcs_patch_to(b, j_tail_back, tail_top)) {
    code_generator_set_error(generator, "wcs tail-jump out of range");
    return 0;
  }

  if (!wcs_patch_here(b, j_done_early)) return 0;

  if (!code_generator_binary_emit_destination_store(generator, context,
                                                    &instruction->dest,
                                                    BINARY_GP_RAX)) {
    return 0;
  }
  return 1;
}
