#include "codegen/binary/internal.h"
#include "codegen/binary/simd_internal.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int code_generator_binary_emit_simd_sum_f64(
    CodeGenerator *generator, BinaryFunctionContext *context,
    const IRInstruction *instruction) {
  BinaryCodeBuffer *b = NULL;
  size_t loop_top = 0;
  size_t j_done = 0;
  size_t j_vec2 = 0;
  size_t j_vec = 0;
  size_t j_scalar = 0;

  if (!generator || !context || !instruction) {
    return 0;
  }
  b = &context->code;

  if (!code_generator_binary_emit_operand_load(generator, context,
                                               &instruction->dest,
                                               BINARY_GP_RAX) ||
      !binary_emit_movq_xmm_reg(b, BINARY_XMM3, BINARY_GP_RAX) ||
      !code_generator_binary_emit_operand_load(generator, context,
                                               &instruction->lhs,
                                               BINARY_GP_RCX) ||
      !code_generator_binary_emit_operand_load(generator, context,
                                               &instruction->rhs,
                                               BINARY_GP_R8) ||
      !wcs_mov_reg_reg32(b, BINARY_GP_R9, BINARY_GP_R8) ||
      !wcs_shift_reg_imm(b, BINARY_GP_R9, 0, 3) ||
      !wcs_add_reg_reg64(b, BINARY_GP_R9, BINARY_GP_RCX) ||
      !wcs_avx_vpxor_ymm(b, 2, 2, 2) ||
      !wcs_avx_vpxor_ymm(b, 4, 4, 4)) {
    return 0;
  }

  loop_top = b->size;
  if (!binary_emit_cmp_reg_reg(b, BINARY_GP_RCX, BINARY_GP_R9) ||
      !wcs_jcc(b, 0x83 , &j_done) ||
      !binary_emit_mov_reg_reg(b, BINARY_GP_R10, BINARY_GP_R9) ||
      !binary_emit_alu_reg_reg(b, 0x29, BINARY_GP_R10, BINARY_GP_RCX) ||
      !wcs_cmp_reg_imm32(b, BINARY_GP_R10, 64) ||
      !wcs_jcc(b, 0x83 , &j_vec2) ||
      !wcs_cmp_reg_imm32(b, BINARY_GP_R10, 32) ||
      !wcs_jcc(b, 0x83 , &j_vec) ||
      !wcs_jcc(b, 0, &j_scalar)) {
    return 0;
  }

  if (!wcs_patch_here(b, j_vec2) ||
      !wcs_avx_vmovups_ymm_mem(b, 0, BINARY_GP_RCX, 0) ||
      !wcs_avx_vmovups_ymm_mem(b, 1, BINARY_GP_RCX, 32) ||
      !wcs_avx_vaddpd_ymm(b, 2, 2, 0) ||
      !wcs_avx_vaddpd_ymm(b, 4, 4, 1) ||
      !wcs_addsub_reg_imm8(b, BINARY_GP_RCX, 0, 64)) {
    return 0;
  }
  {
    size_t j_back = 0;
    if (!wcs_jcc(b, 0, &j_back) || !wcs_patch_to(b, j_back, loop_top)) {
      return 0;
    }
  }

  if (!wcs_patch_here(b, j_vec) ||
      !wcs_avx_vmovups_ymm_mem(b, 0, BINARY_GP_RCX, 0) ||
      !wcs_avx_vaddpd_ymm(b, 2, 2, 0) ||
      !wcs_addsub_reg_imm8(b, BINARY_GP_RCX, 0, 32)) {
    return 0;
  }
  {
    size_t j_back = 0;
    if (!wcs_jcc(b, 0, &j_back) || !wcs_patch_to(b, j_back, loop_top)) {
      return 0;
    }
  }

  if (!wcs_patch_here(b, j_scalar) ||
      !wcs_movsd_xmm_mem(b, 0, BINARY_GP_RCX, 0) ||
      !binary_emit_addsd_xmm_xmm(b, BINARY_XMM3, BINARY_XMM0) ||
      !wcs_addsub_reg_imm8(b, BINARY_GP_RCX, 0, 8)) {
    return 0;
  }
  {
    size_t j_back = 0;
    if (!wcs_jcc(b, 0, &j_back) || !wcs_patch_to(b, j_back, loop_top)) {
      return 0;
    }
  }

  if (!wcs_patch_here(b, j_done) ||
      !wcs_avx_vaddpd_ymm(b, 2, 2, 4) ||
      !wcs_reduce_pd_acc_to_rax(b)) {
    return 0;
  }
  return code_generator_binary_emit_destination_store(generator, context,
                                                      &instruction->dest,
                                                      BINARY_GP_RAX);
}

int code_generator_binary_emit_simd_sum_f32(
    CodeGenerator *generator, BinaryFunctionContext *context,
    const IRInstruction *instruction) {
  BinaryCodeBuffer *b = NULL;
  size_t loop_top = 0;
  size_t j_done = 0;
  size_t j_vec2 = 0;
  size_t j_vec = 0;
  size_t j_scalar = 0;

  if (!generator || !context || !instruction) {
    return 0;
  }
  b = &context->code;

  if (!code_generator_binary_emit_operand_load(generator, context,
                                               &instruction->dest,
                                               BINARY_GP_RAX) ||
      !binary_emit_movd_xmm_reg(b, BINARY_XMM3, BINARY_GP_RAX) ||
      !code_generator_binary_emit_operand_load(generator, context,
                                               &instruction->lhs,
                                               BINARY_GP_RCX) ||
      !code_generator_binary_emit_operand_load(generator, context,
                                               &instruction->rhs,
                                               BINARY_GP_R8) ||
      !wcs_mov_reg_reg32(b, BINARY_GP_R9, BINARY_GP_R8) ||
      !wcs_shift_reg_imm(b, BINARY_GP_R9, 0, 2) ||
      !wcs_add_reg_reg64(b, BINARY_GP_R9, BINARY_GP_RCX) ||
      !wcs_avx_vpxor_ymm(b, 2, 2, 2) ||
      !wcs_avx_vpxor_ymm(b, 4, 4, 4)) {
    return 0;
  }

  loop_top = b->size;
  if (!binary_emit_cmp_reg_reg(b, BINARY_GP_RCX, BINARY_GP_R9) ||
      !wcs_jcc(b, 0x83 , &j_done) ||
      !binary_emit_mov_reg_reg(b, BINARY_GP_R10, BINARY_GP_R9) ||
      !binary_emit_alu_reg_reg(b, 0x29, BINARY_GP_R10, BINARY_GP_RCX) ||
      !wcs_cmp_reg_imm32(b, BINARY_GP_R10, 64) ||
      !wcs_jcc(b, 0x83 , &j_vec2) ||
      !wcs_cmp_reg_imm32(b, BINARY_GP_R10, 32) ||
      !wcs_jcc(b, 0x83 , &j_vec) ||
      !wcs_jcc(b, 0, &j_scalar)) {
    return 0;
  }

  if (!wcs_patch_here(b, j_vec2) ||
      !wcs_avx_vmovups_ymm_mem(b, 0, BINARY_GP_RCX, 0) ||
      !wcs_avx_vmovups_ymm_mem(b, 1, BINARY_GP_RCX, 32) ||
      !wcs_avx_vaddps_ymm(b, 2, 2, 0) ||
      !wcs_avx_vaddps_ymm(b, 4, 4, 1) ||
      !wcs_addsub_reg_imm8(b, BINARY_GP_RCX, 0, 64)) {
    return 0;
  }
  {
    size_t j_back = 0;
    if (!wcs_jcc(b, 0, &j_back) || !wcs_patch_to(b, j_back, loop_top)) {
      return 0;
    }
  }

  if (!wcs_patch_here(b, j_vec) ||
      !wcs_avx_vmovups_ymm_mem(b, 0, BINARY_GP_RCX, 0) ||
      !wcs_avx_vaddps_ymm(b, 2, 2, 0) ||
      !wcs_addsub_reg_imm8(b, BINARY_GP_RCX, 0, 32)) {
    return 0;
  }
  {
    size_t j_back = 0;
    if (!wcs_jcc(b, 0, &j_back) || !wcs_patch_to(b, j_back, loop_top)) {
      return 0;
    }
  }

  if (!wcs_patch_here(b, j_scalar) ||
      !wcs_movss_xmm_mem(b, 0, BINARY_GP_RCX, 0) ||
      !binary_emit_addss_xmm_xmm(b, BINARY_XMM3, BINARY_XMM0) ||
      !wcs_addsub_reg_imm8(b, BINARY_GP_RCX, 0, 4)) {
    return 0;
  }
  {
    size_t j_back = 0;
    if (!wcs_jcc(b, 0, &j_back) || !wcs_patch_to(b, j_back, loop_top)) {
      return 0;
    }
  }

  if (!wcs_patch_here(b, j_done) ||
      !wcs_avx_vaddps_ymm(b, 2, 2, 4) ||
      !wcs_reduce_ps_acc_to_rax(b)) {
    return 0;
  }
  return code_generator_binary_emit_destination_store(generator, context,
                                                      &instruction->dest,
                                                      BINARY_GP_RAX);
}

int code_generator_binary_emit_simd_dot_f64(
    CodeGenerator *generator, BinaryFunctionContext *context,
    const IRInstruction *instruction) {
  BinaryCodeBuffer *b = NULL;
  size_t loop_top = 0;
  size_t j_done = 0;
  size_t j_vec2 = 0;
  size_t j_vec = 0;
  size_t j_scalar = 0;

  if (!generator || !context || !instruction ||
      instruction->argument_count < 1 || !instruction->arguments) {
    code_generator_set_error(generator, "Malformed simd_dot_f64");
    return 0;
  }
  b = &context->code;

  if (!code_generator_binary_emit_operand_load(generator, context,
                                               &instruction->dest,
                                               BINARY_GP_RAX) ||
      !binary_emit_movq_xmm_reg(b, BINARY_XMM3, BINARY_GP_RAX) ||
      !code_generator_binary_emit_operand_load(generator, context,
                                               &instruction->lhs,
                                               BINARY_GP_RCX) ||
      !code_generator_binary_emit_operand_load(generator, context,
                                               &instruction->rhs,
                                               BINARY_GP_RDX) ||
      !code_generator_binary_emit_operand_load(generator, context,
                                               &instruction->arguments[0],
                                               BINARY_GP_R8) ||
      !wcs_mov_reg_reg32(b, BINARY_GP_R9, BINARY_GP_R8) ||
      !wcs_shift_reg_imm(b, BINARY_GP_R9, 0, 3) ||
      !wcs_add_reg_reg64(b, BINARY_GP_R9, BINARY_GP_RCX) ||
      !wcs_avx_vpxor_ymm(b, 2, 2, 2) ||
      !wcs_avx_vpxor_ymm(b, 4, 4, 4)) {
    return 0;
  }

  loop_top = b->size;
  if (!binary_emit_cmp_reg_reg(b, BINARY_GP_RCX, BINARY_GP_R9) ||
      !wcs_jcc(b, 0x83 , &j_done) ||
      !binary_emit_mov_reg_reg(b, BINARY_GP_R10, BINARY_GP_R9) ||
      !binary_emit_alu_reg_reg(b, 0x29, BINARY_GP_R10, BINARY_GP_RCX) ||
      !wcs_cmp_reg_imm32(b, BINARY_GP_R10, 64) ||
      !wcs_jcc(b, 0x83 , &j_vec2) ||
      !wcs_cmp_reg_imm32(b, BINARY_GP_R10, 32) ||
      !wcs_jcc(b, 0x83 , &j_vec) ||
      !wcs_jcc(b, 0, &j_scalar)) {
    return 0;
  }

  if (!wcs_patch_here(b, j_vec2) ||
      !wcs_avx_vmovups_ymm_mem(b, 0, BINARY_GP_RCX, 0) ||
      !wcs_avx_vmovups_ymm_mem(b, 1, BINARY_GP_RDX, 0) ||
      !wcs_avx_vfmadd231pd_ymm(b, 2, 0, 1) ||
      !wcs_avx_vmovups_ymm_mem(b, 0, BINARY_GP_RCX, 32) ||
      !wcs_avx_vmovups_ymm_mem(b, 1, BINARY_GP_RDX, 32) ||
      !wcs_avx_vfmadd231pd_ymm(b, 4, 0, 1) ||
      !wcs_addsub_reg_imm8(b, BINARY_GP_RCX, 0, 64) ||
      !wcs_addsub_reg_imm8(b, BINARY_GP_RDX, 0, 64)) {
    return 0;
  }
  {
    size_t j_back = 0;
    if (!wcs_jcc(b, 0, &j_back) || !wcs_patch_to(b, j_back, loop_top)) {
      return 0;
    }
  }

  if (!wcs_patch_here(b, j_vec) ||
      !wcs_avx_vmovups_ymm_mem(b, 0, BINARY_GP_RCX, 0) ||
      !wcs_avx_vmovups_ymm_mem(b, 1, BINARY_GP_RDX, 0) ||
      !wcs_avx_vfmadd231pd_ymm(b, 2, 0, 1) ||
      !wcs_addsub_reg_imm8(b, BINARY_GP_RCX, 0, 32) ||
      !wcs_addsub_reg_imm8(b, BINARY_GP_RDX, 0, 32)) {
    return 0;
  }
  {
    size_t j_back = 0;
    if (!wcs_jcc(b, 0, &j_back) || !wcs_patch_to(b, j_back, loop_top)) {
      return 0;
    }
  }

  if (!wcs_patch_here(b, j_scalar) ||
      !wcs_movsd_xmm_mem(b, 0, BINARY_GP_RCX, 0) ||
      !wcs_movsd_xmm_mem(b, 1, BINARY_GP_RDX, 0) ||
      !wcs_fmadd231sd(b, 3, 0, 1) ||
      !wcs_addsub_reg_imm8(b, BINARY_GP_RCX, 0, 8) ||
      !wcs_addsub_reg_imm8(b, BINARY_GP_RDX, 0, 8)) {
    return 0;
  }
  {
    size_t j_back = 0;
    if (!wcs_jcc(b, 0, &j_back) || !wcs_patch_to(b, j_back, loop_top)) {
      return 0;
    }
  }

  if (!wcs_patch_here(b, j_done) ||
      !wcs_avx_vaddpd_ymm(b, 2, 2, 4) ||
      !wcs_reduce_pd_acc_to_rax(b)) {
    return 0;
  }
  return code_generator_binary_emit_destination_store(generator, context,
                                                      &instruction->dest,
                                                      BINARY_GP_RAX);
}

static int wcs_affine_f64_scalar_step(BinaryCodeBuffer *b, int b_is_one,
                                      int b_is_zero, int c_is_zero) {
  if (!wcs_movsd_xmm_mem(b, 0, BINARY_GP_RCX, 0) ||
      !wcs_movsd_xmm_mem(b, 1, BINARY_GP_RDX, 0)) {
    return 0;
  }
  if (b_is_one && c_is_zero) {
    if (!wcs_fmadd231sd(b, 1, 0, 4) ||
        !wcs_movsd_mem_xmm(b, BINARY_GP_RDX, 0, BINARY_XMM1)) {
      return 0;
    }
  } else {
    if (!binary_emit_mulsd_xmm_xmm(b, BINARY_XMM0, BINARY_XMM4)) {
      return 0;
    }
    if (!b_is_zero && !wcs_fmadd231sd(b, 0, 5, 1)) {
      return 0;
    }
    if (!c_is_zero && !binary_emit_addsd_xmm_xmm(b, BINARY_XMM0, BINARY_XMM3)) {
      return 0;
    }
    if (!wcs_movsd_mem_xmm(b, BINARY_GP_RDX, 0, BINARY_XMM0)) {
      return 0;
    }
  }
  return wcs_addsub_reg_imm8(b, BINARY_GP_RCX, 0, 8) &&
         wcs_addsub_reg_imm8(b, BINARY_GP_RDX, 0, 8);
}

static int wcs_affine_alias_guard(BinaryCodeBuffer *b, size_t *j_alias) {
  size_t j_same = 0, j_after = 0;
  if (!binary_emit_cmp_reg_reg(b, BINARY_GP_RDX, BINARY_GP_RCX) ||
      !wcs_jcc(b, 0x84 , &j_same) ||
      !binary_emit_cmp_reg_reg(b, BINARY_GP_RDX, BINARY_GP_R9) ||
      !wcs_jcc(b, 0x83 , &j_after)) {
    return 0;
  }
  if (!binary_emit_mov_reg_reg(b, BINARY_GP_R11, BINARY_GP_RDX) ||
      !wcs_add_reg_reg64(b, BINARY_GP_R11, BINARY_GP_R9) ||
      !binary_emit_alu_reg_reg(b, 0x29, BINARY_GP_R11, BINARY_GP_RCX) ||
      !binary_emit_cmp_reg_reg(b, BINARY_GP_RCX, BINARY_GP_R11) ||
      !wcs_jcc(b, 0x82 , j_alias)) {
    return 0;
  }
  return wcs_patch_here(b, j_same) && wcs_patch_here(b, j_after);
}

int code_generator_binary_emit_simd_affine_map_f64_loop(BinaryCodeBuffer *b,
                                                        int b_is_one,
                                                        int b_is_zero,
                                                        int c_is_zero) {
  size_t loop_top = 0, j_done = 0, j_scalar = 0;
  size_t j_alias = 0;
  if (!wcs_affine_alias_guard(b, &j_alias)) {
    return 0;
  }

  if (b_is_one && c_is_zero) {
    size_t j_skip64 = 0, top64 = 0;
    if (!binary_emit_mov_reg_reg(b, BINARY_GP_R11, BINARY_GP_R9) ||
        !binary_emit_alu_reg_reg(b, 0x29, BINARY_GP_R11, BINARY_GP_RCX) ||
        !wcs_shift_reg_imm(b, BINARY_GP_R11, 1 , 6) ||
        !wcs_shift_reg_imm(b, BINARY_GP_R11, 0 , 6) ||
        !wcs_add_reg_reg64(b, BINARY_GP_R11, BINARY_GP_RCX)) {
      return 0;
    }
    if (!binary_emit_cmp_reg_reg(b, BINARY_GP_RCX, BINARY_GP_R11) ||
        !wcs_jcc(b, 0x83 , &j_skip64)) {
      return 0;
    }
    top64 = b->size;
    if (!wcs_avx_vmovups_ymm_mem(b, 0, BINARY_GP_RCX, 0) ||
        !wcs_avx_vmovups_ymm_mem(b, 1, BINARY_GP_RDX, 0) ||
        !wcs_avx_vfmadd231pd_ymm(b, 1, 0, 4) ||
        !wcs_avx_vmovups_mem_ymm(b, BINARY_GP_RDX, 0, 1) ||
        !wcs_avx_vmovups_ymm_mem(b, 2, BINARY_GP_RCX, 32) ||
        !wcs_avx_vmovups_ymm_mem(b, 3, BINARY_GP_RDX, 32) ||
        !wcs_avx_vfmadd231pd_ymm(b, 3, 2, 4) ||
        !wcs_avx_vmovups_mem_ymm(b, BINARY_GP_RDX, 32, 3)) {
      return 0;
    }
    if (!wcs_addsub_reg_imm8(b, BINARY_GP_RCX, 0, 64) ||
        !wcs_addsub_reg_imm8(b, BINARY_GP_RDX, 0, 64) ||
        !binary_emit_cmp_reg_reg(b, BINARY_GP_RCX, BINARY_GP_R11)) {
      return 0;
    }
    {
      size_t j_back = 0;
      if (!wcs_jcc(b, 0x82 , &j_back) ||
          !wcs_patch_to(b, j_back, top64)) {
        return 0;
      }
    }
    if (!wcs_patch_here(b, j_skip64)) {
      return 0;
    }
  }

  if (!binary_emit_mov_reg_reg(b, BINARY_GP_R11, BINARY_GP_R9) ||
      !binary_emit_alu_reg_reg(b, 0x29, BINARY_GP_R11, BINARY_GP_RCX) ||
      !wcs_shift_reg_imm(b, BINARY_GP_R11, 1 , 5) ||
      !wcs_shift_reg_imm(b, BINARY_GP_R11, 0 , 5) ||
      !wcs_add_reg_reg64(b, BINARY_GP_R11, BINARY_GP_RCX)) {
    return 0;
  }

  if (!binary_emit_cmp_reg_reg(b, BINARY_GP_RCX, BINARY_GP_R11) ||
      !wcs_jcc(b, 0x83 , &j_scalar)) {
    return 0;
  }
  loop_top = b->size;
  if (!wcs_avx_vmovups_ymm_mem(b, 0, BINARY_GP_RCX, 0) ||
      !wcs_avx_vmovups_ymm_mem(b, 1, BINARY_GP_RDX, 0)) {
    return 0;
  }
  if (b_is_one && c_is_zero) {
    if (!wcs_avx_vfmadd231pd_ymm(b, 1, 0, 4) ||
        !wcs_avx_vmovups_mem_ymm(b, BINARY_GP_RDX, 0, 1)) {
      return 0;
    }
  } else {
    if (!wcs_avx_vmulpd_ymm(b, 0, 0, 4)) {
      return 0;
    }
    if (!b_is_zero && !wcs_avx_vfmadd231pd_ymm(b, 0, 5, 1)) {
      return 0;
    }
    if (!c_is_zero && !wcs_avx_vaddpd_ymm(b, 0, 0, 3)) {
      return 0;
    }
    if (!wcs_avx_vmovups_mem_ymm(b, BINARY_GP_RDX, 0, 0)) {
      return 0;
    }
  }
  if (!wcs_addsub_reg_imm8(b, BINARY_GP_RCX, 0, 32) ||
      !wcs_addsub_reg_imm8(b, BINARY_GP_RDX, 0, 32) ||
      !binary_emit_cmp_reg_reg(b, BINARY_GP_RCX, BINARY_GP_R11)) {
    return 0;
  }
  {
    size_t j_back = 0;
    if (!wcs_jcc(b, 0x82 , &j_back) ||
        !wcs_patch_to(b, j_back, loop_top)) {
      return 0;
    }
  }

  if (!wcs_patch_here(b, j_scalar) || !wcs_patch_here(b, j_alias) ||
      !binary_emit_cmp_reg_reg(b, BINARY_GP_RCX, BINARY_GP_R9) ||
      !wcs_jcc(b, 0x83 , &j_done)) {
    return 0;
  }
  {
    size_t scalar_top = b->size;
    if (!wcs_affine_f64_scalar_step(b, b_is_one, b_is_zero, c_is_zero) ||
        !binary_emit_cmp_reg_reg(b, BINARY_GP_RCX, BINARY_GP_R9)) {
      return 0;
    }
    {
      size_t j_back = 0;
      if (!wcs_jcc(b, 0x82 , &j_back) ||
          !wcs_patch_to(b, j_back, scalar_top)) {
        return 0;
      }
    }
  }

  return wcs_patch_here(b, j_done) && wcs_avx_vzeroupper(b);
}

int code_generator_binary_emit_simd_affine_map_f64_inline(
    BinaryCodeBuffer *b, unsigned long long a_bits, unsigned long long b_bits,
    unsigned long long c_bits, int b_is_one, int b_is_zero, int c_is_zero,
    int a_runtime) {
  if (a_runtime) {
    if (!wcs_avx_vbroadcastsd_ymm_xmm(b, 4, 4)) {
      return 0;
    }
  } else if (!binary_emit_mov_reg_imm64(b, BINARY_GP_RAX, (long long)a_bits) ||
             !binary_emit_movq_xmm_reg(b, BINARY_XMM4, BINARY_GP_RAX) ||
             !wcs_avx_vbroadcastsd_ymm_xmm(b, 4, 4)) {
    return 0;
  }
  if (!binary_emit_mov_reg_imm64(b, BINARY_GP_RAX, (long long)b_bits) ||
      !binary_emit_movq_xmm_reg(b, BINARY_XMM5, BINARY_GP_RAX) ||
      !wcs_avx_vbroadcastsd_ymm_xmm(b, 5, 5) ||
      !binary_emit_mov_reg_imm64(b, BINARY_GP_RAX, (long long)c_bits) ||
      !binary_emit_movq_xmm_reg(b, BINARY_XMM3, BINARY_GP_RAX) ||
      !wcs_avx_vbroadcastsd_ymm_xmm(b, 3, 3) ||
      !wcs_mov_reg_reg32(b, BINARY_GP_R9, BINARY_GP_R8) ||
      !wcs_shift_reg_imm(b, BINARY_GP_R9, 0, 3) ||
      !wcs_add_reg_reg64(b, BINARY_GP_R9, BINARY_GP_RCX)) {
    return 0;
  }
  return code_generator_binary_emit_simd_affine_map_f64_loop(b, b_is_one,
                                                             b_is_zero,
                                                             c_is_zero);
}

int code_generator_binary_emit_simd_affine_map_f64(
    CodeGenerator *generator, BinaryFunctionContext *context,
    const IRInstruction *instruction) {
  BinaryCodeBuffer *b = NULL;

  if (!generator || !context || !instruction ||
      instruction->argument_count < 4 || !instruction->arguments) {
    code_generator_set_error(generator, "Malformed simd_affine_map_f64");
    return 0;
  }
  b = &context->code;

  int b_is_one = instruction->arguments[2].kind == IR_OPERAND_FLOAT &&
                 instruction->arguments[2].float_value == 1.0;
  int b_is_zero = instruction->arguments[2].kind == IR_OPERAND_FLOAT &&
                  instruction->arguments[2].float_value == 0.0;
  int c_is_zero = instruction->arguments[3].kind == IR_OPERAND_FLOAT &&
                  instruction->arguments[3].float_value == 0.0;

  if (!code_generator_binary_emit_operand_load(generator, context,
                                               &instruction->lhs,
                                               BINARY_GP_RCX) ||
      !code_generator_binary_emit_operand_load(generator, context,
                                               &instruction->rhs,
                                               BINARY_GP_RDX) ||
      !code_generator_binary_emit_operand_load(generator, context,
                                               &instruction->arguments[0],
                                               BINARY_GP_R8) ||
      !code_generator_binary_emit_operand_load(generator, context,
                                               &instruction->arguments[1],
                                               BINARY_GP_RAX) ||
      !binary_emit_movq_xmm_reg(b, BINARY_XMM4, BINARY_GP_RAX) ||
      !wcs_avx_vbroadcastsd_ymm_xmm(b, 4, 4) ||
      !code_generator_binary_emit_operand_load(generator, context,
                                               &instruction->arguments[2],
                                               BINARY_GP_RAX) ||
      !binary_emit_movq_xmm_reg(b, BINARY_XMM5, BINARY_GP_RAX) ||
      !wcs_avx_vbroadcastsd_ymm_xmm(b, 5, 5) ||
      !code_generator_binary_emit_operand_load(generator, context,
                                               &instruction->arguments[3],
                                               BINARY_GP_RAX) ||
      !binary_emit_movq_xmm_reg(b, BINARY_XMM3, BINARY_GP_RAX) ||
      !wcs_avx_vbroadcastsd_ymm_xmm(b, 3, 3) ||
      !wcs_mov_reg_reg32(b, BINARY_GP_R9, BINARY_GP_R8) ||
      !wcs_shift_reg_imm(b, BINARY_GP_R9, 0, 3) ||
      !wcs_add_reg_reg64(b, BINARY_GP_R9, BINARY_GP_RCX)) {
    return 0;
  }

  return code_generator_binary_emit_simd_affine_map_f64_loop(b, b_is_one,
                                                             b_is_zero,
                                                             c_is_zero);
}

int code_generator_binary_emit_simd_affine_map_f32_loop(BinaryCodeBuffer *b,
                                                        int b_is_one,
                                                        int b_is_zero,
                                                        int c_is_zero) {
  size_t loop_top = 0, j_done = 0, j_scalar = 0;
  size_t j_alias = 0;
  if (!wcs_affine_alias_guard(b, &j_alias)) {
    return 0;
  }

  if (!binary_emit_mov_reg_reg(b, BINARY_GP_R11, BINARY_GP_R9) ||
      !binary_emit_alu_reg_reg(b, 0x29, BINARY_GP_R11, BINARY_GP_RCX) ||
      !wcs_shift_reg_imm(b, BINARY_GP_R11, 1 , 5) ||
      !wcs_shift_reg_imm(b, BINARY_GP_R11, 0 , 5) ||
      !wcs_add_reg_reg64(b, BINARY_GP_R11, BINARY_GP_RCX)) {
    return 0;
  }

  if (!binary_emit_cmp_reg_reg(b, BINARY_GP_RCX, BINARY_GP_R11) ||
      !wcs_jcc(b, 0x83 , &j_scalar)) {
    return 0;
  }
  loop_top = b->size;
  if (!wcs_avx_vmovups_ymm_mem(b, 0, BINARY_GP_RCX, 0) ||
      !wcs_avx_vmovups_ymm_mem(b, 1, BINARY_GP_RDX, 0)) {
    return 0;
  }
  if (b_is_one && c_is_zero) {
    if (!wcs_avx_vfmadd231ps_ymm(b, 1, 0, 4) ||
        !wcs_avx_vmovups_mem_ymm(b, BINARY_GP_RDX, 0, 1)) {
      return 0;
    }
  } else {
    if (!wcs_avx_vmulps_ymm(b, 0, 0, 4)) {
      return 0;
    }
    if (!b_is_zero && !wcs_avx_vfmadd231ps_ymm(b, 0, 5, 1)) {
      return 0;
    }
    if (!c_is_zero && !wcs_avx_vaddps_ymm(b, 0, 0, 3)) {
      return 0;
    }
    if (!wcs_avx_vmovups_mem_ymm(b, BINARY_GP_RDX, 0, 0)) {
      return 0;
    }
  }
  if (!wcs_addsub_reg_imm8(b, BINARY_GP_RCX, 0, 32) ||
      !wcs_addsub_reg_imm8(b, BINARY_GP_RDX, 0, 32) ||
      !binary_emit_cmp_reg_reg(b, BINARY_GP_RCX, BINARY_GP_R11)) {
    return 0;
  }
  {
    size_t j_back = 0;
    if (!wcs_jcc(b, 0x82 , &j_back) ||
        !wcs_patch_to(b, j_back, loop_top)) {
      return 0;
    }
  }

  if (!wcs_patch_here(b, j_scalar) || !wcs_patch_here(b, j_alias) ||
      !binary_emit_cmp_reg_reg(b, BINARY_GP_RCX, BINARY_GP_R9) ||
      !wcs_jcc(b, 0x83 , &j_done)) {
    return 0;
  }
  {
    size_t scalar_top = b->size;
    if (!wcs_movss_xmm_mem(b, 0, BINARY_GP_RCX, 0) ||
        !wcs_movss_xmm_mem(b, 1, BINARY_GP_RDX, 0)) {
      return 0;
    }
    if (b_is_one && c_is_zero) {
      if (!wcs_fmadd231ss(b, 1, 0, 4) ||
          !wcs_movss_mem_xmm(b, BINARY_GP_RDX, 0, BINARY_XMM1)) {
        return 0;
      }
    } else if (!binary_emit_mulss_xmm_xmm(b, BINARY_XMM0, BINARY_XMM4) ||
               (!b_is_zero && !wcs_fmadd231ss(b, 0, 5, 1)) ||
               (!c_is_zero &&
                !binary_emit_addss_xmm_xmm(b, BINARY_XMM0, BINARY_XMM3)) ||
               !wcs_movss_mem_xmm(b, BINARY_GP_RDX, 0, BINARY_XMM0)) {
      return 0;
    }
    if (!wcs_addsub_reg_imm8(b, BINARY_GP_RCX, 0, 4) ||
        !wcs_addsub_reg_imm8(b, BINARY_GP_RDX, 0, 4) ||
        !binary_emit_cmp_reg_reg(b, BINARY_GP_RCX, BINARY_GP_R9)) {
      return 0;
    }
    {
      size_t j_back = 0;
      if (!wcs_jcc(b, 0x82 , &j_back) ||
          !wcs_patch_to(b, j_back, scalar_top)) {
        return 0;
      }
    }
  }
  return wcs_patch_here(b, j_done) && wcs_avx_vzeroupper(b);
}

int code_generator_binary_emit_simd_affine_map_f32_inline(
    BinaryCodeBuffer *b, unsigned a_bits, unsigned b_bits, unsigned c_bits,
    int b_is_one, int b_is_zero, int c_is_zero, int a_runtime) {
  if (a_runtime) {
    if (!wcs_avx_vpbroadcastd_ymm(b, 4, 4)) {
      return 0;
    }
  } else if (!binary_emit_mov_reg_imm64(b, BINARY_GP_RAX, a_bits) ||
             !wcs_movd_xmm_reg(b, 4, BINARY_GP_RAX) ||
             !wcs_avx_vpbroadcastd_ymm(b, 4, 4)) {
    return 0;
  }
  if (!binary_emit_mov_reg_imm64(b, BINARY_GP_RAX, b_bits) ||
      !wcs_movd_xmm_reg(b, 5, BINARY_GP_RAX) ||
      !wcs_avx_vpbroadcastd_ymm(b, 5, 5) ||
      !binary_emit_mov_reg_imm64(b, BINARY_GP_RAX, c_bits) ||
      !wcs_movd_xmm_reg(b, 3, BINARY_GP_RAX) ||
      !wcs_avx_vpbroadcastd_ymm(b, 3, 3) ||
      !wcs_mov_reg_reg32(b, BINARY_GP_R9, BINARY_GP_R8) ||
      !wcs_shift_reg_imm(b, BINARY_GP_R9, 0, 2) ||
      !wcs_add_reg_reg64(b, BINARY_GP_R9, BINARY_GP_RCX)) {
    return 0;
  }
  return code_generator_binary_emit_simd_affine_map_f32_loop(b, b_is_one,
                                                             b_is_zero,
                                                             c_is_zero);
}

int code_generator_binary_emit_simd_affine_map_f32(
    CodeGenerator *generator, BinaryFunctionContext *context,
    const IRInstruction *instruction) {
  BinaryCodeBuffer *b = NULL;

  if (!generator || !context || !instruction ||
      instruction->argument_count < 4 || !instruction->arguments) {
    code_generator_set_error(generator, "Malformed simd_affine_map_f32");
    return 0;
  }
  b = &context->code;

  int b_is_one = instruction->arguments[2].kind == IR_OPERAND_FLOAT &&
                 instruction->arguments[2].float_value == 1.0;
  int b_is_zero = instruction->arguments[2].kind == IR_OPERAND_FLOAT &&
                  instruction->arguments[2].float_value == 0.0;
  int c_is_zero = instruction->arguments[3].kind == IR_OPERAND_FLOAT &&
                  instruction->arguments[3].float_value == 0.0;

  if (!code_generator_binary_emit_operand_load(generator, context,
                                               &instruction->lhs,
                                               BINARY_GP_RCX) ||
      !code_generator_binary_emit_operand_load(generator, context,
                                               &instruction->rhs,
                                               BINARY_GP_RDX) ||
      !code_generator_binary_emit_operand_load(generator, context,
                                               &instruction->arguments[0],
                                               BINARY_GP_R8) ||
      !code_generator_binary_emit_operand_load(generator, context,
                                               &instruction->arguments[1],
                                               BINARY_GP_RAX) ||
      !wcs_movd_xmm_reg(b, 4, BINARY_GP_RAX) ||
      !wcs_avx_vpbroadcastd_ymm(b, 4, 4) ||
      !code_generator_binary_emit_operand_load(generator, context,
                                               &instruction->arguments[2],
                                               BINARY_GP_RAX) ||
      !wcs_movd_xmm_reg(b, 5, BINARY_GP_RAX) ||
      !wcs_avx_vpbroadcastd_ymm(b, 5, 5) ||
      !code_generator_binary_emit_operand_load(generator, context,
                                               &instruction->arguments[3],
                                               BINARY_GP_RAX) ||
      !wcs_movd_xmm_reg(b, 3, BINARY_GP_RAX) ||
      !wcs_avx_vpbroadcastd_ymm(b, 3, 3) ||
      !wcs_mov_reg_reg32(b, BINARY_GP_R9, BINARY_GP_R8) ||
      !wcs_shift_reg_imm(b, BINARY_GP_R9, 0, 2) ||
      !wcs_add_reg_reg64(b, BINARY_GP_R9, BINARY_GP_RCX)) {
    return 0;
  }

  return code_generator_binary_emit_simd_affine_map_f32_loop(b, b_is_one,
                                                             b_is_zero,
                                                             c_is_zero);
}

#define VLOOP_K_LOAD 0
#define VLOOP_K_IOTA 1
#define VLOOP_K_CONST 2
#define VLOOP_K_ADD 3
#define VLOOP_K_SUB 4
#define VLOOP_K_MUL 5
#define VLOOP_K_DIV 6
#define VLOOP_K_SCALAR 7
#define VLOOP_K_AND 8
#define VLOOP_K_OR 9
#define VLOOP_K_XOR 10
#define VLOOP_K_SHL 11
#define VLOOP_K_SAR 12
#define VLOOP_K_SHR 13
#define VLOOP_K_MIN 14
#define VLOOP_K_MAX 15
#define VLOOP_K_CMPGT 16
#define VLOOP_K_PAIR 17
#define VLOOP_K_SELECT 18
#define VLOOP_K_CMPEQ 19
#define VLOOP_KERNEL_REGS 4
#define VLOOP_KERNEL_MAX_NODES 48
#define VLOOP_KERNEL_MAX_BASES 4
#define VLOOP_KERNEL_POOL_MAX 6

int code_generator_vloop_pool_size(int elem8, int has_iota) {
  return 4 + (elem8 ? 0 : 1) + (has_iota ? 0 : 1);
}

static int vloop_kernel_tag_is_leaf(int tag) {
  return tag == VLOOP_K_LOAD || tag == VLOOP_K_IOTA || tag == VLOOP_K_CONST ||
         tag == VLOOP_K_SCALAR;
}

int code_generator_vloop_collect_dist(const IRInstruction *in, int is_reduce,
                                      const char *names[4],
                                      const IROperand *srcs[4], int *n_out) {
  int n_arrays = (int)in->arguments[1].int_value;
  int n = 0;
  if (!is_reduce) {
    names[0] = in->dest.name;
    srcs[0] = &in->dest;
    n = 1;
  }
  for (int k = 0; k < n_arrays; k++) {
    const IROperand *as = &in->arguments[7 + k];
    const char *nm = as->name;
    int found = 0;
    for (int j = 0; j < n; j++)
      if (names[j] && nm && strcmp(names[j], nm) == 0) { found = 1; break; }
    if (!found) {
      if (n >= VLOOP_KERNEL_MAX_BASES) return -1;
      names[n] = nm;
      srcs[n] = as;
      n++;
    }
  }
  *n_out = n;
  return 0;
}

int code_generator_binary_emit_simd_vloop_unmarshaled(
    CodeGenerator *generator, BinaryFunctionContext *context,
    const IRInstruction *instruction) {
  return code_generator_binary_emit_simd_vloop_f64(generator, context,
                                                   instruction, 0);
}

int code_generator_binary_emit_simd_vloop_f64(
    CodeGenerator *generator, BinaryFunctionContext *context,
    const IRInstruction *instruction, int operands_marshaled) {
  BinaryCodeBuffer *b = NULL;
  static const int kPoolReduce[3] = {0, 1, 4};
  static const BinaryGpRegister kGp[VLOOP_KERNEL_MAX_BASES] = {
      BINARY_GP_RCX, BINARY_GP_RDX, BINARY_GP_R8, BINARY_GP_R9};
  const int IOTA_CONST = 5;
  const int BYTE_MASK = 3;

  if (!generator || !context || !instruction || instruction->argument_count < 7 ||
      !instruction->arguments || instruction->dest.kind != IR_OPERAND_SYMBOL) {
    code_generator_set_error(generator, "Malformed simd_vloop");
    return 0;
  }
  const int i32 = (instruction->op == IR_OP_SIMD_VLOOP_I32);
  if (!i32 && instruction->float_bits != 32 && instruction->float_bits != 64) {
    code_generator_set_error(generator, "simd_vloop bad float width");
    return 0;
  }
  const int elem8 = i32 && instruction->float_bits == 8;
  const int elem8_unsigned = elem8 && instruction->is_unsigned;
  if (i32 && instruction->float_bits != 32 && instruction->float_bits != 8) {
    code_generator_set_error(generator, "simd_vloop bad int element width");
    return 0;
  }
  b = &context->code;
  const int f32 = i32 || (instruction->float_bits == 32);
  const int lanes = f32 ? 8 : 4;
  const int elem_bytes = elem8 ? 1 : (f32 ? 4 : 8);
  const int vec_stride = elem_bytes * lanes;

  const IROperand *args = instruction->arguments;
  long long reduce_op = args[0].int_value;
  int n_arrays = (int)args[1].int_value;
  int n_nodes = (int)args[2].int_value;
  int root = (int)args[3].int_value;
  int n_consts = (int)args[4].int_value;
  int n_scalars = (int)args[5].int_value;
  int depth = (int)args[6].int_value;
  const int is_minmax = (reduce_op == 2 || reduce_op == 3);
  const int is_max = (reduce_op == 2);
  int is_reduce = (reduce_op == 1) || is_minmax;
  size_t expect =
      (size_t)(7 + n_arrays + n_scalars + 3 * n_nodes + n_consts);
  if ((reduce_op < 0 || reduce_op > 3) || n_arrays < 0 ||
      n_arrays > VLOOP_KERNEL_MAX_BASES || n_nodes <= 0 ||
      n_nodes > VLOOP_KERNEL_MAX_NODES || n_consts < 0 || n_scalars < 0 ||
      root < 0 || root >= n_nodes ||
      instruction->argument_count != expect) {
    code_generator_set_error(generator, "Bad simd_vloop encoding");
    return 0;
  }

  size_t scalars_off = (size_t)(7 + n_arrays);
  size_t nodes_off = scalars_off + (size_t)n_scalars;
  size_t consts_off = nodes_off + (size_t)(3 * n_nodes);
  int n_slots = n_consts + n_scalars;

  const char *dist_name[VLOOP_KERNEL_MAX_BASES];
  const IROperand *dist_src[VLOOP_KERNEL_MAX_BASES];
  int n_dist = 0;
  if (code_generator_vloop_collect_dist(instruction, is_reduce, dist_name,
                                        dist_src, &n_dist) < 0) {
    code_generator_set_error(generator, "simd_vloop_f64 too many bases");
    return 0;
  }
  int has_iota = 0;
  for (int i = 0; i < n_nodes; i++) {
    if ((int)args[nodes_off + 3 * i].int_value == VLOOP_K_IOTA) {
      has_iota = 1;
    }
  }
  int map_pool[VLOOP_KERNEL_POOL_MAX] = {0, 1, 2, 4, 0, 0};
  int map_pool_n = 4;
  if (!elem8) {
    map_pool[map_pool_n++] = BYTE_MASK;
  }
  if (!has_iota) {
    map_pool[map_pool_n++] = IOTA_CONST;
  }
  const int *kPool = is_reduce ? kPoolReduce : map_pool;
  int pool_n = is_reduce ? 3 : map_pool_n;
  if (pool_n != (is_reduce ? 3 : code_generator_vloop_pool_size(elem8,
                                                                has_iota)) ||
      depth > pool_n) {
    code_generator_set_error(generator, "simd_vloop depth over pool");
    return 0;
  }
  int arr_reg[VLOOP_KERNEL_MAX_BASES];
  for (int k = 0; k < n_arrays; k++) {
    const char *nm = args[7 + k].name;
    int found = 0;
    for (int j = 0; j < n_dist; j++) {
      if (dist_name[j] && nm && strcmp(dist_name[j], nm) == 0) {
        found = j;
        break;
      }
    }
    arr_reg[k] = kGp[found];
  }
  int dst_reg = kGp[0];

  uint32_t cbytes = (uint32_t)(32 * n_slots);
  if (cbytes && !binary_emit_sub_rsp_imm32(b, cbytes)) {
    return 0;
  }

  if (operands_marshaled) {
    if (!binary_emit_mov_reg_reg(b, BINARY_GP_R10, kGp[n_dist])) {
      return 0;
    }
  } else {
    for (int j = 0; j < n_dist; j++) {
      if (!code_generator_binary_emit_operand_load(generator, context,
                                                   dist_src[j], kGp[j])) {
        return 0;
      }
    }
    if (!code_generator_binary_emit_operand_load(generator, context,
                                                 &instruction->lhs,
                                                 BINARY_GP_R10)) {
      return 0;
    }
  }
  if (has_iota && !binary_emit_mov_reg_imm64(b, BINARY_GP_R11, 0)) {
    return 0;
  }

  for (int c = 0; c < n_consts; c++) {
    uint64_t bits = 0;
    if (i32) {
      uint32_t iv = (uint32_t)(uint64_t)args[consts_off + c].int_value;
      bits = (uint64_t)iv | ((uint64_t)iv << 32);
    } else if (f32) {
      float fv = (float)args[consts_off + c].float_value;
      uint32_t fb = 0;
      memcpy(&fb, &fv, sizeof(fb));
      bits = (uint64_t)fb | ((uint64_t)fb << 32);
    } else {
      double dv = args[consts_off + c].float_value;
      memcpy(&bits, &dv, sizeof(bits));
    }
    if (!binary_emit_mov_reg_imm64(b, BINARY_GP_RAX, bits) ||
        !binary_emit_movq_xmm_reg(b, BINARY_XMM0, BINARY_GP_RAX) ||
        !wcs_avx_vbroadcastsd_ymm_xmm(b, 0, 0) ||
        !wcs_avx_vmovups_mem_ymm(b, BINARY_GP_RSP, 32 * c, 0)) {
      return 0;
    }
  }
  for (int s = 0; s < n_scalars; s++) {
    if (!code_generator_binary_emit_operand_load(generator, context,
                                                 &args[scalars_off + s],
                                                 BINARY_GP_RAX)) {
      return 0;
    }
    if (f32) {
      if (!wcs_broadcast_i32_to_ymm(b, 0, BINARY_GP_RAX)) {
        return 0;
      }
    } else if (!binary_emit_movq_xmm_reg(b, BINARY_XMM0, BINARY_GP_RAX) ||
               !wcs_avx_vbroadcastsd_ymm_xmm(b, 0, 0)) {
      return 0;
    }
    if (!wcs_avx_vmovups_mem_ymm(b, BINARY_GP_RSP, 32 * (n_consts + s), 0)) {
      return 0;
    }
  }
  if (has_iota) {
    if (f32) {
      if (!binary_emit_mov_reg_imm64(b, BINARY_GP_RAX, 0x0000000100000000ULL) ||
          !binary_emit_movq_xmm_reg(b, BINARY_XMM0, BINARY_GP_RAX) ||
          !binary_emit_mov_reg_imm64(b, BINARY_GP_RAX, 0x0000000300000002ULL) ||
          !binary_emit_movq_xmm_reg(b, BINARY_XMM2, BINARY_GP_RAX) ||
          !wcs_avx_vpunpcklqdq_xmm(b, 0, 0, 2) ||
          !binary_emit_mov_reg_imm64(b, BINARY_GP_RAX, 0x0000000500000004ULL) ||
          !binary_emit_movq_xmm_reg(b, BINARY_XMM1, BINARY_GP_RAX) ||
          !binary_emit_mov_reg_imm64(b, BINARY_GP_RAX, 0x0000000700000006ULL) ||
          !binary_emit_movq_xmm_reg(b, BINARY_XMM2, BINARY_GP_RAX) ||
          !wcs_avx_vpunpcklqdq_xmm(b, 1, 1, 2) ||
          !wcs_avx_vperm2i128(b, IOTA_CONST, 0, 1, 0x20)) {
        return 0;
      }
    } else if (!binary_emit_mov_reg_imm64(b, BINARY_GP_RAX,
                                          0x0000000100000000ULL) ||
               !binary_emit_movq_xmm_reg(b, BINARY_XMM0, BINARY_GP_RAX) ||
               !binary_emit_mov_reg_imm64(b, BINARY_GP_RAX,
                                          0x0000000300000002ULL) ||
               !binary_emit_movq_xmm_reg(b, BINARY_XMM1, BINARY_GP_RAX) ||
               !wcs_avx_vpunpcklqdq_xmm(b, IOTA_CONST, 0, 1)) {
      return 0;
    }
  }

  if (elem8 && !is_reduce &&
      (!binary_emit_mov_reg_imm64(b, BINARY_GP_RAX, 0x000000FF000000FFULL) ||
       !binary_emit_movq_xmm_reg(b, BINARY_XMM0, BINARY_GP_RAX) ||
       !wcs_avx_vbroadcastsd_ymm_xmm(b, BYTE_MASK, 0))) {
    return 0;
  }
  if (is_minmax) {
    if (!code_generator_binary_emit_operand_load(generator, context,
                                                 &instruction->dest,
                                                 BINARY_GP_RAX)) {
      return 0;
    }
    if (f32) {
      if (!wcs_broadcast_i32_to_ymm(b, 2, BINARY_GP_RAX)) {
        return 0;
      }
    } else if (!binary_emit_movq_xmm_reg(b, BINARY_XMM2, BINARY_GP_RAX) ||
               !wcs_avx_vbroadcastsd_ymm_xmm(b, 2, 2)) {
      return 0;
    }
  } else if (is_reduce) {
    if (i32) {
      if (!wcs_avx_vpxor_ymm(b, 2, 2, 2)) {
        return 0;
      }
    } else if (!code_generator_binary_emit_operand_load(generator, context,
                                                        &instruction->dest,
                                                        BINARY_GP_RAX) ||
               !(f32 ? binary_emit_movd_xmm_reg(b, BINARY_XMM3, BINARY_GP_RAX)
                     : binary_emit_movq_xmm_reg(b, BINARY_XMM3, BINARY_GP_RAX)) ||
               !wcs_avx_vpxor_ymm(b, 2, 2, 2)) {
      return 0;
    }
  }

  size_t j_overlap[2 * VLOOP_KERNEL_MAX_BASES];
  int n_overlap = 0;
  if (!is_reduce) {
    int shift = 0;
    while ((1 << shift) < elem_bytes) {
      shift++;
    }
    for (int j = 1; j < n_dist; j++) {
      for (int dir = 0; dir < 2; dir++) {
        int lo = dir ? (int)kGp[j] : (int)dst_reg;
        int hi = dir ? (int)dst_reg : (int)kGp[j];
        if (!binary_emit_mov_reg_reg(b, BINARY_GP_RAX, (BinaryGpRegister)hi) ||
            !wcs_sub_reg_reg64(b, BINARY_GP_RAX, lo)) {
          return 0;
        }
        if (shift && !wcs_shift_reg_imm(b, BINARY_GP_RAX, 1,
                                        (unsigned char)shift)) {
          return 0;
        }
        if (!wcs_cmp_reg_reg64(b, BINARY_GP_RAX, BINARY_GP_R10) ||
            !wcs_jcc(b, 0x82 , &j_overlap[n_overlap++])) {
          return 0;
        }
      }
    }
  }

  size_t vec_top = b->size;
  size_t j_tail = 0;
  if (!wcs_cmp_reg_imm8(b, BINARY_GP_R10, lanes) ||
      !wcs_jcc(b, 0x82 , &j_tail)) {
    return 0;
  }
  {
    int pool[VLOOP_KERNEL_POOL_MAX];
    int nfree = pool_n;
    for (int i = 0; i < pool_n; i++) {
      pool[i] = kPool[i];
    }
    int vstk[VLOOP_KERNEL_MAX_NODES];
    int nv = 0;
    for (int i = 0; i < n_nodes; i++) {
      int tag = (int)args[nodes_off + 3 * i].int_value;
      int op0 = (int)args[nodes_off + 3 * i + 1].int_value;
      int op1 = (int)args[nodes_off + 3 * i + 2].int_value;
      if (vloop_kernel_tag_is_leaf(tag)) {
        if (nfree <= 0) {
          code_generator_set_error(generator, "vloop reg budget");
          return 0;
        }
        int R = pool[--nfree];
        int ok = 0;
        if (tag == VLOOP_K_LOAD) {
          ok = elem8 ? (elem8_unsigned
                            ? wcs_avx_vpmovzxbd_ymm_mem(b, R, arr_reg[op0], 0)
                            : wcs_avx_vpmovsxbd_ymm_mem(b, R, arr_reg[op0], 0))
                     : wcs_avx_vmovups_ymm_mem(b, R, arr_reg[op0], 0);
        } else if (tag == VLOOP_K_CONST) {
          ok = wcs_avx_vmovups_ymm_mem(b, R, BINARY_GP_RSP, 32 * op0);
        } else if (tag == VLOOP_K_SCALAR) {
          ok = wcs_avx_vmovups_ymm_mem(b, R, BINARY_GP_RSP,
                                       32 * (n_consts + op0));
        } else {
          ok = wcs_broadcast_i32_to_ymm(b, R, BINARY_GP_R11) &&
               wcs_avx_vpaddd_ymm(b, R, R, IOTA_CONST) &&
               (i32 ? 1
                    : (f32 ? wcs_avx_vcvtdq2ps_ymm(b, R, R)
                           : wcs_avx_vcvtdq2pd_ymm_xmm(b, R, R)));
        }
        if (!ok) {
          return 0;
        }
        vstk[nv++] = R;
      } else if (tag == VLOOP_K_SHL || tag == VLOOP_K_SAR ||
                 tag == VLOOP_K_SHR) {
        if (nv < 1 || !i32) {
          code_generator_set_error(generator, "vloop shift");
          return 0;
        }
        int ra = vstk[nv - 1];
        if (!(tag == VLOOP_K_SHL
                  ? wcs_avx_vpslld_ymm_imm(b, ra, ra, (unsigned char)op1)
                  : tag == VLOOP_K_SAR
                        ? wcs_avx_vpsrad_ymm_imm(b, ra, ra, (unsigned char)op1)
                        : wcs_avx_vpsrld_ymm_imm(b, ra, ra,
                                                 (unsigned char)op1))) {
          return 0;
        }
      } else if (tag == VLOOP_K_PAIR) {
        if (nv < 2) {
          code_generator_set_error(generator, "vloop pair");
          return 0;
        }
      } else if (tag == VLOOP_K_SELECT) {
        if (nv < 3 || !i32) {
          code_generator_set_error(generator, "vloop select");
          return 0;
        }
        int relse = vstk[--nv];
        int rthen = vstk[--nv];
        int rmask = vstk[--nv];
        if (!wcs_avx_vpxor_ymm(b, rthen, rthen, relse) ||
            !wcs_avx_vpand_ymm(b, rthen, rthen, rmask) ||
            !wcs_avx_vpxor_ymm(b, rthen, rthen, relse)) {
          return 0;
        }
        pool[nfree++] = relse;
        pool[nfree++] = rmask;
        vstk[nv++] = rthen;
      } else {
        if (nv < 2) {
          code_generator_set_error(generator, "vloop stack");
          return 0;
        }
        int rb = vstk[--nv];
        int ra = vstk[--nv];
        int ok = 0;
        switch (tag) {
        case VLOOP_K_ADD:
          ok = i32 ? wcs_avx_vpaddd_ymm(b, ra, ra, rb)
                   : (f32 ? wcs_avx_vaddps_ymm(b, ra, ra, rb)
                          : wcs_avx_vaddpd_ymm(b, ra, ra, rb));
          break;
        case VLOOP_K_SUB:
          ok = i32 ? wcs_avx_vpsubd_ymm(b, ra, ra, rb)
                   : (f32 ? wcs_avx_vsubps_ymm(b, ra, ra, rb)
                          : wcs_avx_vsubpd_ymm(b, ra, ra, rb));
          break;
        case VLOOP_K_MUL:
          ok = i32 ? wcs_avx_vpmulld_ymm(b, ra, ra, rb)
                   : (f32 ? wcs_avx_vmulps_ymm(b, ra, ra, rb)
                          : wcs_avx_vmulpd_ymm(b, ra, ra, rb));
          break;
        case VLOOP_K_DIV:
          if (i32) {
            code_generator_set_error(generator, "vloop int div");
            return 0;
          }
          ok = f32 ? wcs_avx_vdivps_ymm(b, ra, ra, rb)
                   : wcs_avx_vdivpd_ymm(b, ra, ra, rb);
          break;
        case VLOOP_K_AND:
        case VLOOP_K_OR:
        case VLOOP_K_XOR:
          if (!i32) {
            code_generator_set_error(generator, "vloop float bitop");
            return 0;
          }
          ok = tag == VLOOP_K_AND
                   ? wcs_avx_vpand_ymm(b, ra, ra, rb)
                   : (tag == VLOOP_K_OR ? wcs_avx_vpor_ymm(b, ra, ra, rb)
                                        : wcs_avx_vpxor_ymm(b, ra, ra, rb));
          break;
        case VLOOP_K_MIN:
        case VLOOP_K_MAX:
        case VLOOP_K_CMPGT:
        case VLOOP_K_CMPEQ:
          if (!i32) {
            code_generator_set_error(generator, "vloop float compare");
            return 0;
          }
          ok = tag == VLOOP_K_MIN
                   ? wcs_avx_vpminsd_ymm(b, ra, ra, rb)
                   : tag == VLOOP_K_MAX
                         ? wcs_avx_vpmaxsd_ymm(b, ra, ra, rb)
                         : tag == VLOOP_K_CMPGT
                               ? wcs_avx_vpcmpgtd_ymm(b, ra, ra, rb)
                               : wcs_avx_vpcmpeqd_ymm(b, ra, ra, rb);
          break;
        default: code_generator_set_error(generator, "vloop op"); return 0;
        }
        if (!ok) {
          return 0;
        }
        pool[nfree++] = rb;
        vstk[nv++] = ra;
        (void)op0;
      }
    }
    if (nv != 1) {
      code_generator_set_error(generator, "vloop root");
      return 0;
    }
    if (is_minmax) {
      if (!(i32 ? (is_max ? wcs_avx_vpmaxsd_ymm(b, 2, vstk[0], 2)
                          : wcs_avx_vpminsd_ymm(b, 2, vstk[0], 2))
                : (f32 ? (is_max ? wcs_avx_vmaxps_ymm(b, 2, vstk[0], 2)
                                 : wcs_avx_vminps_ymm(b, 2, vstk[0], 2))
                       : (is_max ? wcs_avx_vmaxpd_ymm(b, 2, vstk[0], 2)
                                 : wcs_avx_vminpd_ymm(b, 2, vstk[0], 2))))) {
        return 0;
      }
    } else if (is_reduce) {
      if (!(i32 ? wcs_avx_vpaddd_ymm(b, 2, 2, vstk[0])
                : (f32 ? wcs_avx_vaddps_ymm(b, 2, 2, vstk[0])
                       : wcs_avx_vaddpd_ymm(b, 2, 2, vstk[0])))) {
        return 0;
      }
    } else if (elem8) {
      int R = vstk[0];
      if (!wcs_avx_vpand_ymm(b, R, R, BYTE_MASK) ||
          !wcs_avx_vpackusdw_ymm(b, R, R, R) ||
          !wcs_avx_vpermq_ymm(b, R, R, 0x08) ||
          !wcs_avx_vpackuswb_ymm(b, R, R, R) ||
          !wcs_movsd_mem_xmm(b, dst_reg, 0, R)) {
        return 0;
      }
    } else if (!wcs_avx_vmovups_mem_ymm(b, dst_reg, 0, vstk[0])) {
      return 0;
    }
  }
  for (int j = 0; j < n_dist; j++) {
    if (!wcs_addsub_reg_imm8(b, kGp[j], 0, vec_stride)) {
      return 0;
    }
  }
  if (has_iota && !wcs_addsub_reg_imm8(b, BINARY_GP_R11, 0, lanes)) {
    return 0;
  }
  if (!wcs_addsub_reg_imm8(b, BINARY_GP_R10, 1, lanes)) {
    return 0;
  }
  {
    size_t j_back = 0;
    if (!wcs_jcc(b, 0, &j_back) || !wcs_patch_to(b, j_back, vec_top)) {
      return 0;
    }
  }

  if (!wcs_patch_here(b, j_tail)) {
    return 0;
  }
  for (int j = 0; j < n_overlap; j++) {
    if (!wcs_patch_here(b, j_overlap[j])) {
      return 0;
    }
  }
  size_t tail_top = b->size;
  size_t j_done = 0;
  if (!wcs_cmp_reg_imm8(b, BINARY_GP_R10, 0) || !wcs_jcc(b, 0x84 , &j_done)) {
    return 0;
  }
  {
    int pool[VLOOP_KERNEL_POOL_MAX];
    int nfree = pool_n;
    for (int i = 0; i < pool_n; i++) {
      pool[i] = kPool[i];
    }
    int vstk[VLOOP_KERNEL_MAX_NODES];
    int nv = 0;
    for (int i = 0; i < n_nodes; i++) {
      int tag = (int)args[nodes_off + 3 * i].int_value;
      int op0 = (int)args[nodes_off + 3 * i + 1].int_value;
      int op1 = (int)args[nodes_off + 3 * i + 2].int_value;
      if (vloop_kernel_tag_is_leaf(tag)) {
        int R = pool[--nfree];
        int ok = 0;
        if (tag == VLOOP_K_LOAD && elem8) {
          ok = (elem8_unsigned
                    ? binary_emit_movzx_reg_mem8(b, BINARY_GP_RAX,
                                                 (BinaryGpRegister)arr_reg[op0], 0)
                    : binary_emit_movsx_reg_mem8(b, BINARY_GP_RAX,
                                                 (BinaryGpRegister)arr_reg[op0], 0)) &&
               wcs_avx_vmovd_xmm_reg(b, R, BINARY_GP_RAX);
        } else if (tag == VLOOP_K_LOAD) {
          ok = i32 ? wcs_avx_vmovd_xmm_mem(b, R, arr_reg[op0], 0)
                   : (f32 ? wcs_movss_xmm_mem(b, R, arr_reg[op0], 0)
                          : wcs_movsd_xmm_mem(b, R, arr_reg[op0], 0));
        } else if (tag == VLOOP_K_CONST || tag == VLOOP_K_SCALAR) {
          int disp = 32 * (tag == VLOOP_K_CONST ? op0 : n_consts + op0);
          ok = i32 ? wcs_avx_vmovd_xmm_mem(b, R, BINARY_GP_RSP, disp)
                   : (f32 ? wcs_movss_xmm_mem(b, R, BINARY_GP_RSP, disp)
                          : wcs_movsd_xmm_mem(b, R, BINARY_GP_RSP, disp));
        } else {
          ok = i32 ? wcs_avx_vmovd_xmm_reg(b, R, BINARY_GP_R11)
                   : (f32 ? binary_emit_cvtsi2ss_xmm_reg(b, (BinaryXmmRegister)R,
                                                         BINARY_GP_R11)
                          : binary_emit_cvtsi2sd_xmm_reg(b, (BinaryXmmRegister)R,
                                                         BINARY_GP_R11));
        }
        if (!ok) {
          return 0;
        }
        vstk[nv++] = R;
      } else if (tag == VLOOP_K_SHL || tag == VLOOP_K_SAR ||
                 tag == VLOOP_K_SHR) {
        int ra = vstk[nv - 1];
        if (!i32 ||
            !(tag == VLOOP_K_SHL
                  ? wcs_avx_vpslld_ymm_imm(b, ra, ra, (unsigned char)op1)
                  : tag == VLOOP_K_SAR
                        ? wcs_avx_vpsrad_ymm_imm(b, ra, ra, (unsigned char)op1)
                        : wcs_avx_vpsrld_ymm_imm(b, ra, ra,
                                                 (unsigned char)op1))) {
          return 0;
        }
      } else if (tag == VLOOP_K_PAIR) {
        if (nv < 2) {
          return 0;
        }
      } else if (tag == VLOOP_K_SELECT) {
        int relse, rthen, rmask;
        if (nv < 3 || !i32) {
          return 0;
        }
        relse = vstk[--nv];
        rthen = vstk[--nv];
        rmask = vstk[--nv];
        if (!wcs_avx_vpxor_ymm(b, rthen, rthen, relse) ||
            !wcs_avx_vpand_ymm(b, rthen, rthen, rmask) ||
            !wcs_avx_vpxor_ymm(b, rthen, rthen, relse)) {
          return 0;
        }
        pool[nfree++] = relse;
        pool[nfree++] = rmask;
        vstk[nv++] = rthen;
      } else if (i32) {
        int rb = vstk[--nv];
        int ra = vstk[--nv];
        int ok = 0;
        switch (tag) {
        case VLOOP_K_ADD: ok = wcs_avx_vpaddd_ymm(b, ra, ra, rb); break;
        case VLOOP_K_SUB: ok = wcs_avx_vpsubd_ymm(b, ra, ra, rb); break;
        case VLOOP_K_MUL: ok = wcs_avx_vpmulld_ymm(b, ra, ra, rb); break;
        case VLOOP_K_AND: ok = wcs_avx_vpand_ymm(b, ra, ra, rb); break;
        case VLOOP_K_OR: ok = wcs_avx_vpor_ymm(b, ra, ra, rb); break;
        case VLOOP_K_XOR: ok = wcs_avx_vpxor_ymm(b, ra, ra, rb); break;
        case VLOOP_K_MIN: ok = wcs_avx_vpminsd_ymm(b, ra, ra, rb); break;
        case VLOOP_K_MAX: ok = wcs_avx_vpmaxsd_ymm(b, ra, ra, rb); break;
        case VLOOP_K_CMPGT: ok = wcs_avx_vpcmpgtd_ymm(b, ra, ra, rb); break;
        case VLOOP_K_CMPEQ: ok = wcs_avx_vpcmpeqd_ymm(b, ra, ra, rb); break;
        default: return 0;
        }
        if (!ok) {
          return 0;
        }
        pool[nfree++] = rb;
        vstk[nv++] = ra;
      } else {
        int rb = vstk[--nv];
        int ra = vstk[--nv];
        int ok = 0;
        BinaryXmmRegister A = (BinaryXmmRegister)ra;
        BinaryXmmRegister B = (BinaryXmmRegister)rb;
        switch (tag) {
        case VLOOP_K_ADD:
          ok = f32 ? binary_emit_addss_xmm_xmm(b, A, B)
                   : binary_emit_addsd_xmm_xmm(b, A, B);
          break;
        case VLOOP_K_SUB:
          ok = f32 ? binary_emit_subss_xmm_xmm(b, A, B)
                   : binary_emit_subsd_xmm_xmm(b, A, B);
          break;
        case VLOOP_K_MUL:
          ok = f32 ? binary_emit_mulss_xmm_xmm(b, A, B)
                   : binary_emit_mulsd_xmm_xmm(b, A, B);
          break;
        case VLOOP_K_DIV:
          ok = f32 ? binary_emit_divss_xmm_xmm(b, A, B)
                   : binary_emit_divsd_xmm_xmm(b, A, B);
          break;
        default: return 0;
        }
        if (!ok) {
          return 0;
        }
        pool[nfree++] = rb;
        vstk[nv++] = ra;
      }
    }
    if (is_minmax) {
      int R = vstk[0];
      if (!(f32 ? wcs_avx_vpbroadcastd_ymm(b, R, R)
                : wcs_avx_vbroadcastsd_ymm_xmm(b, R, R))) {
        return 0;
      }
      if (!(i32 ? (is_max ? wcs_avx_vpmaxsd_ymm(b, 2, R, 2)
                          : wcs_avx_vpminsd_ymm(b, 2, R, 2))
                : (f32 ? (is_max ? wcs_avx_vmaxps_ymm(b, 2, R, 2)
                                 : wcs_avx_vminps_ymm(b, 2, R, 2))
                       : (is_max ? wcs_avx_vmaxpd_ymm(b, 2, R, 2)
                                 : wcs_avx_vminpd_ymm(b, 2, R, 2))))) {
        return 0;
      }
    } else if (is_reduce) {
      if (i32) {
        if (!wcs_avx_vpaddd_ymm(b, 2, 2, vstk[0])) {
          return 0;
        }
      } else if (!(f32 ? binary_emit_addss_xmm_xmm(b, BINARY_XMM3,
                                                   (BinaryXmmRegister)vstk[0])
                       : binary_emit_addsd_xmm_xmm(b, BINARY_XMM3,
                                                   (BinaryXmmRegister)vstk[0]))) {
        return 0;
      }
    } else if (elem8) {
      if (!wcs_avx_vpextrb_mem_xmm(b, dst_reg, 0, vstk[0])) {
        return 0;
      }
    } else if (!(i32 ? wcs_avx_vmovd_mem_xmm(b, dst_reg, 0, vstk[0])
                     : (f32 ? wcs_movss_mem_xmm(b, dst_reg, 0, vstk[0])
                            : wcs_movsd_mem_xmm(b, dst_reg, 0, vstk[0])))) {
      return 0;
    }
  }
  for (int j = 0; j < n_dist; j++) {
    if (!wcs_addsub_reg_imm8(b, kGp[j], 0, elem_bytes)) {
      return 0;
    }
  }
  if (has_iota && !wcs_addsub_reg_imm8(b, BINARY_GP_R11, 0, 1)) {
    return 0;
  }
  if (!wcs_addsub_reg_imm8(b, BINARY_GP_R10, 1, 1)) {
    return 0;
  }
  {
    size_t j_back = 0;
    if (!wcs_jcc(b, 0, &j_back) || !wcs_patch_to(b, j_back, tail_top)) {
      return 0;
    }
  }

  if (!wcs_patch_here(b, j_done)) {
    return 0;
  }
  if (is_reduce) {
    if (!is_minmax && i32 &&
        !code_generator_binary_emit_operand_load(generator, context,
                                                 &instruction->dest,
                                                 BINARY_GP_RAX)) {
      return 0;
    }
    if (is_minmax) {
      if (!(i32 ? wcs_reduce_ymm_i32_minmax_to_rax(b, is_max)
                : (f32 ? wcs_reduce_ps_minmax_to_rax(b, is_max)
                       : wcs_reduce_pd_minmax_to_rax(b, is_max)))) {
        return 0;
      }
    } else if (!(i32 ? wcs_reduce_ymm_i32_sum_to_rax(b, 2)
                     : (f32 ? wcs_reduce_ps_acc_to_rax(b)
                            : wcs_reduce_pd_acc_to_rax(b)))) {
      return 0;
    }
    if (i32 &&
        !(instruction->is_unsigned
              ? wcs_mov_reg_reg32(b, BINARY_GP_RAX, BINARY_GP_RAX)
              : binary_emit_movsxd_reg_reg32(b, BINARY_GP_RAX, BINARY_GP_RAX))) {
      return 0;
    }
    if (cbytes && !binary_emit_add_rsp_imm32(b, cbytes)) {
      return 0;
    }
    return code_generator_binary_emit_destination_store(generator, context,
                                                        &instruction->dest,
                                                        BINARY_GP_RAX);
  }
  if (!wcs_avx_vzeroupper(b)) {
    return 0;
  }
  if (cbytes && !binary_emit_add_rsp_imm32(b, cbytes)) {
    return 0;
  }
  return 1;
}

#define OLK_U_AND 1
#define OLK_U_OR 2
#define OLK_U_XOR 3
#define OLK_U_ADD 4
#define OLK_U_SUB 5
#define OLK_U_MUL 6
#define OLK_U_SHL 7
#define OLK_U_SHR 8
#define OLK_U_CVT 9
#define OLK_U_FADD 10
#define OLK_U_FSUB 11
#define OLK_U_FMUL 12
#define OLK_U_FDIV 13
#define OLK_C_ADD 0
#define OLK_C_SUB 1
#define OLK_C_MUL 2
#define OLK_C_DIV 3

static int ol_emit_uniform_prog(CodeGenerator *gen, BinaryCodeBuffer *b,
                                const IROperand *args, size_t prog_off,
                                int n_micro, int n_fconst,
                                BinaryGpRegister val_gpr, BinaryGpRegister work_gpr,
                                int res_xmm, int ctmp_xmm) {
  if (!binary_emit_mov_reg_reg(b, work_gpr, val_gpr)) {
    return 0;
  }
  int in_float = 0;
  for (int m = 0; m < n_micro; m++) {
    int mop = (int)args[prog_off + 2 * m].int_value;
    long long mimm = args[prog_off + 2 * m + 1].int_value;
    int ok = 1;
    switch (mop) {
    case OLK_U_AND:
    case OLK_U_OR:
    case OLK_U_XOR:
    case OLK_U_ADD:
    case OLK_U_SUB:
    case OLK_U_MUL: {
      unsigned char alu = 0;
      if (mop == OLK_U_ADD) alu = 0x01;
      else if (mop == OLK_U_SUB) alu = 0x29;
      else if (mop == OLK_U_AND) alu = 0x21;
      else if (mop == OLK_U_OR) alu = 0x09;
      else if (mop == OLK_U_XOR) alu = 0x31;
      ok = binary_emit_mov_reg_imm64(b, BINARY_GP_RAX, (uint64_t)mimm);
      if (ok && mop == OLK_U_MUL) {
        ok = binary_emit_imul_reg_reg(b, work_gpr, BINARY_GP_RAX);
      } else if (ok) {
        ok = binary_emit_alu_reg_reg(b, alu, work_gpr, BINARY_GP_RAX);
      }
      break;
    }
    case OLK_U_SHL: ok = wcs_shift_reg_imm(b, work_gpr, 0, (unsigned char)mimm); break;
    case OLK_U_SHR: ok = wcs_shift_reg_imm(b, work_gpr, 1, (unsigned char)mimm); break;
    case OLK_U_CVT:
      ok = wcs_avx_vcvtsi2sd(b, res_xmm, res_xmm, work_gpr);
      in_float = 1;
      break;
    case OLK_U_FADD:
    case OLK_U_FSUB:
    case OLK_U_FMUL:
    case OLK_U_FDIV: {
      if (mimm < 0 || mimm >= n_fconst) {
        code_generator_set_error(gen, "vloop outer: unif fconst idx");
        return 0;
      }
      ok = wcs_avx_vmovsd_xmm_mem(b, ctmp_xmm, BINARY_GP_RSP, (int)(32 * mimm));
      if (ok) {
        if (mop == OLK_U_FADD) ok = wcs_avx_vaddsd(b, res_xmm, res_xmm, ctmp_xmm);
        else if (mop == OLK_U_FSUB) ok = wcs_avx_vsubsd(b, res_xmm, res_xmm, ctmp_xmm);
        else if (mop == OLK_U_FMUL) ok = wcs_avx_vmulsd(b, res_xmm, res_xmm, ctmp_xmm);
        else ok = wcs_avx_vdivsd(b, res_xmm, res_xmm, ctmp_xmm);
      }
      break;
    }
    default:
      code_generator_set_error(gen, "vloop outer: bad micro op");
      return 0;
    }
    if (!ok) {
      return 0;
    }
  }
  if (!in_float) {
    code_generator_set_error(gen, "vloop outer: uniform never cast");
    return 0;
  }
  return 1;
}

int code_generator_binary_emit_simd_outer_lane_f64(
    CodeGenerator *generator, BinaryFunctionContext *context,
    const IRInstruction *instruction) {
  BinaryCodeBuffer *b = NULL;
  if (!generator || !context || !instruction || instruction->argument_count < 8 ||
      !instruction->arguments || instruction->dest.kind != IR_OPERAND_SYMBOL) {
    code_generator_set_error(generator, "Malformed simd_outer_lane_f64");
    return 0;
  }
  b = &context->code;
  const IROperand *args = instruction->arguments;
  int inner_cmp = (int)args[0].int_value;
  long long istep = args[1].int_value;
  int n_chain = (int)args[2].int_value;
  int n_unif = (int)args[3].int_value;
  int n_fconst = (int)args[4].int_value;
  long long i0 = args[5].int_value;
  int init_mode = (int)args[6].int_value;
  double iacc_init = args[7].float_value;
  if (n_chain <= 0 || n_chain > 8 || n_unif < 0 || n_unif > 8 || n_fconst < 0 ||
      n_fconst > 16 || istep <= 0 || (init_mode != 0 && init_mode != 1)) {
    code_generator_set_error(generator, "Bad simd_outer_lane_f64 encoding");
    return 0;
  }

  const int K = 3;
  const int kAcc[3] = {0, 1, 2};
  const int GROUP = 4 * K;

  size_t chain_off = 8;
  size_t off = chain_off + (size_t)(4 * n_chain);
  size_t unif_start[8];
  int unif_nmicro[8];
  for (int u = 0; u < n_unif; u++) {
    if (off >= instruction->argument_count) {
      code_generator_set_error(generator, "vloop outer: uniform overrun");
      return 0;
    }
    int nm = (int)args[off].int_value;
    if (nm < 0 || nm > 16) {
      code_generator_set_error(generator, "vloop outer: bad micro count");
      return 0;
    }
    unif_nmicro[u] = nm;
    unif_start[u] = off + 1;
    off += 1 + (size_t)(2 * nm);
  }
  size_t init_start = 0;
  int init_nmicro = 0;
  if (init_mode == 1) {
    if (off >= instruction->argument_count) {
      code_generator_set_error(generator, "vloop outer: seed overrun");
      return 0;
    }
    init_nmicro = (int)args[off].int_value;
    if (init_nmicro < 0 || init_nmicro > 16) {
      code_generator_set_error(generator, "vloop outer: bad seed micro count");
      return 0;
    }
    init_start = off + 1;
    off += 1 + (size_t)(2 * init_nmicro);
  }
  size_t fconst_off = off;
  if (fconst_off + (size_t)n_fconst != instruction->argument_count) {
    code_generator_set_error(generator, "vloop outer: arg length mismatch");
    return 0;
  }

  uint64_t init_bits = 0;
  memcpy(&init_bits, &iacc_init, sizeof(init_bits));
  int total_off = 32 * n_fconst;
  int seedarr_off = total_off + 16;
  uint32_t cbytes =
      (uint32_t)(seedarr_off + (init_mode == 1 ? 32 * K : 0));
  if (!binary_emit_sub_rsp_imm32(b, cbytes)) {
    return 0;
  }
  if (!code_generator_binary_emit_operand_load(generator, context,
                                               &instruction->dest,
                                               BINARY_GP_RAX) ||
      !binary_emit_movq_xmm_reg(b, BINARY_XMM4, BINARY_GP_RAX) ||
      !wcs_movsd_mem_xmm(b, BINARY_GP_RSP, total_off, 4) ||
      !code_generator_binary_emit_operand_load(generator, context,
                                               &instruction->lhs,
                                               BINARY_GP_R9) ||
      !code_generator_binary_emit_operand_load(generator, context,
                                               &instruction->rhs,
                                               BINARY_GP_R8)) {
    return 0;
  }
  for (int c = 0; c < n_fconst; c++) {
    double dv = args[fconst_off + c].float_value;
    uint64_t bits = 0;
    memcpy(&bits, &dv, sizeof(bits));
    if (!binary_emit_mov_reg_imm64(b, BINARY_GP_RAX, bits) ||
        !binary_emit_movq_xmm_reg(b, BINARY_XMM0, BINARY_GP_RAX) ||
        !wcs_avx_vbroadcastsd_ymm_xmm(b, 0, 0) ||
        !wcs_avx_vmovups_mem_ymm(b, BINARY_GP_RSP, 32 * c, 0)) {
      return 0;
    }
  }
  if (!binary_emit_mov_reg_imm64(b, BINARY_GP_RDX, 0)) {
    return 0;
  }

  size_t outer_top = b->size;
  size_t j_outer_end = 0;
  if (!binary_emit_cmp_reg_reg(b, BINARY_GP_RDX, BINARY_GP_R9) ||
      !wcs_jcc(b, 0x8D , &j_outer_end)) {
    return 0;
  }
  if (init_mode == 0) {
    if (!binary_emit_mov_reg_imm64(b, BINARY_GP_RAX, init_bits) ||
        !binary_emit_movq_xmm_reg(b, BINARY_XMM4, BINARY_GP_RAX)) {
      return 0;
    }
    for (int k = 0; k < K; k++) {
      if (!wcs_avx_vbroadcastsd_ymm_xmm(b, kAcc[k], 4)) {
        return 0;
      }
    }
  } else {
    for (int g = 0; g < GROUP; g++) {
      if (!binary_emit_mov_reg_reg(b, BINARY_GP_R11, BINARY_GP_RDX) ||
          !wcs_addsub_reg_imm8(b, BINARY_GP_R11, 0, (unsigned char)g) ||
          !ol_emit_uniform_prog(generator, b, args, init_start, init_nmicro,
                                n_fconst, BINARY_GP_R11, BINARY_GP_R10, 4, 5) ||
          !wcs_avx_vmovsd_mem_xmm(b, BINARY_GP_RSP, seedarr_off + 8 * g, 4)) {
        return 0;
      }
    }
    for (int k = 0; k < K; k++) {
      if (!wcs_avx_vmovups_ymm_mem(b, kAcc[k], BINARY_GP_RSP,
                                   seedarr_off + 32 * k)) {
        return 0;
      }
    }
  }
  if (!binary_emit_mov_reg_imm64(b, BINARY_GP_RCX, (uint64_t)i0)) {
    return 0;
  }

  size_t inner_top = b->size;
  size_t j_inner_end = 0;
  if (!binary_emit_cmp_reg_reg(b, BINARY_GP_RCX, BINARY_GP_R8) ||
      !wcs_jcc(b, inner_cmp ? 0x8F  : 0x8D , &j_inner_end)) {
    return 0;
  }
  for (int s = 0; s < n_chain; s++) {
    int c_op = (int)args[chain_off + 4 * s].int_value;
    int side = (int)args[chain_off + 4 * s + 1].int_value;
    int kind = (int)args[chain_off + 4 * s + 2].int_value;
    int idx = (int)args[chain_off + 4 * s + 3].int_value;
    if (kind == 0) {
      if (idx < 0 || idx >= n_fconst ||
          !wcs_avx_vmovups_ymm_mem(b, 3, BINARY_GP_RSP, 32 * idx)) {
        code_generator_set_error(generator, "vloop outer: const idx");
        return 0;
      }
    } else {
      if (idx < 0 || idx >= n_unif ||
          !ol_emit_uniform_prog(generator, b, args, unif_start[idx],
                                unif_nmicro[idx], n_fconst, BINARY_GP_RCX,
                                BINARY_GP_R10, 4, 5) ||
          !wcs_avx_vbroadcastsd_ymm_xmm(b, 3, 4)) {
        if (idx < 0 || idx >= n_unif) {
          code_generator_set_error(generator, "vloop outer: unif idx");
        }
        return 0;
      }
    }
    for (int k = 0; k < K; k++) {
      int a = kAcc[k];
      int ok = 0;
      if (c_op == OLK_C_ADD) ok = wcs_avx_vaddpd_ymm(b, a, a, 3);
      else if (c_op == OLK_C_MUL) ok = wcs_avx_vmulpd_ymm(b, a, a, 3);
      else if (c_op == OLK_C_SUB)
        ok = side ? wcs_avx_vsubpd_ymm(b, a, 3, a) : wcs_avx_vsubpd_ymm(b, a, a, 3);
      else if (c_op == OLK_C_DIV)
        ok = side ? wcs_avx_vdivpd_ymm(b, a, 3, a) : wcs_avx_vdivpd_ymm(b, a, a, 3);
      else { code_generator_set_error(generator, "vloop outer: chain op"); return 0; }
      if (!ok) {
        return 0;
      }
    }
  }
  if (!wcs_addsub_reg_imm8(b, BINARY_GP_RCX, 0, (unsigned char)istep)) {
    return 0;
  }
  {
    size_t j_back = 0;
    if (!wcs_jcc(b, 0, &j_back) || !wcs_patch_to(b, j_back, inner_top)) {
      return 0;
    }
  }

  if (!wcs_patch_here(b, j_inner_end)) {
    return 0;
  }
  if (!wcs_avx_vmovsd_xmm_mem(b, 5, BINARY_GP_RSP, total_off)) {
    return 0;
  }
  if (init_mode == 0) {
    if (!binary_emit_mov_reg_reg(b, BINARY_GP_R11, BINARY_GP_R9) ||
        !binary_emit_alu_reg_reg(b, 0x29 , BINARY_GP_R11, BINARY_GP_RDX) ||
        !binary_emit_mov_reg_imm64(b, BINARY_GP_RAX, (uint64_t)GROUP) ||
        !wcs_cmp_reg_imm32(b, BINARY_GP_R11, (uint32_t)GROUP) ||
        !binary_emit_cmovcc_reg_reg(b, 0x4F , BINARY_GP_R11,
                                    BINARY_GP_RAX)) {
      return 0;
    }
    size_t add_top = b->size;
    if (!wcs_avx_vaddsd(b, 5, 5, 0) ||
        !wcs_addsub_reg_imm8(b, BINARY_GP_RDX, 0, 1) ||
        !wcs_addsub_reg_imm8(b, BINARY_GP_R11, 1, 1)) {
      return 0;
    }
    size_t j_add_back = 0;
    if (!wcs_jcc(b, 0x85 , &j_add_back) ||
        !wcs_patch_to(b, j_add_back, add_top)) {
      return 0;
    }
  } else {
    for (int g = 0; g < GROUP; g++) {
      int k = g / 4, j = g % 4;
      int acc = kAcc[k];
      size_t j_skip = 0;
      if (!binary_emit_mov_reg_reg(b, BINARY_GP_R11, BINARY_GP_RDX) ||
          !wcs_addsub_reg_imm8(b, BINARY_GP_R11, 0, (unsigned char)g) ||
          !binary_emit_cmp_reg_reg(b, BINARY_GP_R11, BINARY_GP_R9) ||
          !wcs_jcc(b, 0x8D , &j_skip)) {
        return 0;
      }
      int ok = 1;
      if (j == 0) {
        ok = wcs_avx_vaddsd(b, 5, 5, acc);
      } else if (j == 1) {
        ok = wcs_avx_vunpckhpd_xmm(b, 4, acc, acc) && wcs_avx_vaddsd(b, 5, 5, 4);
      } else if (j == 2) {
        ok = wcs_avx_vextractf128(b, 4, acc, 1) && wcs_avx_vaddsd(b, 5, 5, 4);
      } else {
        ok = wcs_avx_vextractf128(b, 4, acc, 1) &&
             wcs_avx_vunpckhpd_xmm(b, 4, 4, 4) && wcs_avx_vaddsd(b, 5, 5, 4);
      }
      if (!ok || !wcs_patch_here(b, j_skip)) {
        return 0;
      }
    }
    if (!wcs_addsub_reg_imm8(b, BINARY_GP_RDX, 0, (unsigned char)GROUP)) {
      return 0;
    }
  }
  if (!wcs_avx_vmovsd_mem_xmm(b, BINARY_GP_RSP, total_off, 5)) {
    return 0;
  }
  {
    size_t j_back = 0;
    if (!wcs_jcc(b, 0, &j_back) || !wcs_patch_to(b, j_back, outer_top)) {
      return 0;
    }
  }

  if (!wcs_patch_here(b, j_outer_end) || !wcs_avx_vzeroupper(b)) {
    return 0;
  }
  if (!wcs_movsd_xmm_mem(b, 0, BINARY_GP_RSP, total_off) ||
      !binary_emit_movq_reg_xmm(b, BINARY_GP_RAX, BINARY_XMM0)) {
    return 0;
  }
  if (!binary_emit_add_rsp_imm32(b, cbytes)) {
    return 0;
  }
  return code_generator_binary_emit_destination_store(generator, context,
                                                      &instruction->dest,
                                                      BINARY_GP_RAX);
}

int code_generator_binary_emit_simd_i2f_reduce_f64(
    CodeGenerator *generator, BinaryFunctionContext *context,
    const IRInstruction *instruction) {
  BinaryCodeBuffer *b = NULL;
  long long bound = 0;
  long long vec_end = 0;
  size_t nsteps = 0;
  size_t vec_top = 0;
  size_t rem_top = 0;
  size_t j_after_vec = 0;
  size_t j_done = 0;
  size_t back = 0;

  if (!generator || !context || !instruction || instruction->argument_count < 3 ||
      (instruction->argument_count % 2) == 0 || !instruction->arguments) {
    code_generator_set_error(generator, "Malformed simd_i2f_reduce_f64");
    return 0;
  }
  b = &context->code;
  bound = instruction->arguments[0].int_value;
  vec_end = bound & ~3LL;
  nsteps = (instruction->argument_count - 1) / 2;
  uint32_t const_bytes = (uint32_t)(32 * nsteps);

  if (!wcs_avx_vpxor_ymm(b, 2, 2, 2) || !wcs_avx_vpxor_ymm(b, 3, 3, 3) ||
      !binary_emit_mov_reg_imm64(b, BINARY_GP_RAX, 0x0000000100000000ULL) ||
      !binary_emit_movq_xmm_reg(b, BINARY_XMM0, BINARY_GP_RAX) ||
      !binary_emit_mov_reg_imm64(b, BINARY_GP_RAX, 0x0000000300000002ULL) ||
      !binary_emit_movq_xmm_reg(b, BINARY_XMM1, BINARY_GP_RAX) ||
      !wcs_avx_vpunpcklqdq_xmm(b, 5, 0, 1) ||
      !binary_emit_mov_reg_imm64(b, BINARY_GP_RCX, 0) ||
      !binary_emit_mov_reg_imm64(b, BINARY_GP_R8, (uint64_t)vec_end) ||
      !binary_emit_mov_reg_imm64(b, BINARY_GP_R9, (uint64_t)bound)) {
    return 0;
  }

  if (!binary_emit_sub_rsp_imm32(b, const_bytes) ||
      !binary_emit_mov_reg_reg(b, BINARY_GP_R11, BINARY_GP_RSP)) {
    return 0;
  }
  for (size_t s = 0; s < nsteps; s++) {
    double kd = instruction->arguments[2 + 2 * s].float_value;
    uint64_t kbits = 0;
    memcpy(&kbits, &kd, sizeof(kbits));
    if (!binary_emit_mov_reg_imm64(b, BINARY_GP_RAX, kbits) ||
        !binary_emit_movq_xmm_reg(b, BINARY_XMM1, BINARY_GP_RAX) ||
        !wcs_avx_vbroadcastsd_ymm_xmm(b, 1, 1) ||
        !wcs_avx_vmovups_mem_ymm(b, BINARY_GP_R11, (int)(32 * s), 1)) {
      return 0;
    }
  }

  vec_top = b->size;
  if (!binary_emit_cmp_reg_reg(b, BINARY_GP_RCX, BINARY_GP_R8) ||
      !wcs_jcc(b, 0x8D , &j_after_vec) ||
      !wcs_broadcast_i32_to_ymm(b, 1, BINARY_GP_RCX) ||
      !wcs_avx_vpaddd_ymm(b, 1, 1, 5) ||
      !wcs_avx_vcvtdq2pd_ymm_xmm(b, 0, 1)) {
    return 0;
  }
  for (size_t s = 0; s < nsteps; s++) {
    int op = (int)instruction->arguments[1 + 2 * s].int_value;
    if (!wcs_avx_vmovups_ymm_mem(b, 1, BINARY_GP_R11, (int)(32 * s))) {
      return 0;
    }
    int ok = 0;
    switch (op) {
    case 0: ok = wcs_avx_vmulpd_ymm(b, 0, 0, 1); break;
    case 1: ok = wcs_avx_vaddpd_ymm(b, 0, 0, 1); break;
    case 2: ok = wcs_avx_vsubpd_ymm(b, 0, 0, 1); break;
    case 3: ok = wcs_avx_vsubpd_ymm(b, 0, 1, 0); break;
    case 4: ok = wcs_avx_vdivpd_ymm(b, 0, 0, 1); break;
    default: code_generator_set_error(generator, "bad i2f step op"); return 0;
    }
    if (!ok) {
      return 0;
    }
  }
  if (!wcs_avx_vcvttpd2dq_xmm_ymm(b, 1, 0) ||
      !wcs_avx_vcvtdq2pd_ymm_xmm(b, 0, 1) || !wcs_avx_vaddpd_ymm(b, 2, 2, 0) ||
      !wcs_addsub_reg_imm8(b, BINARY_GP_RCX, 0, 4) ||
      !wcs_jcc(b, 0, &back) || !wcs_patch_to(b, back, vec_top)) {
    return 0;
  }

  if (!wcs_patch_here(b, j_after_vec)) {
    return 0;
  }
  rem_top = b->size;
  if (!binary_emit_cmp_reg_reg(b, BINARY_GP_RCX, BINARY_GP_R9) ||
      !wcs_jcc(b, 0x8D , &j_done) ||
      !binary_emit_cvtsi2sd_xmm_reg(b, BINARY_XMM0, BINARY_GP_RCX)) {
    return 0;
  }
  for (size_t s = 0; s < nsteps; s++) {
    int op = (int)instruction->arguments[1 + 2 * s].int_value;
    if (!wcs_movsd_xmm_mem(b, 1, BINARY_GP_R11, (int)(32 * s))) {
      return 0;
    }
    int ok = 0;
    switch (op) {
    case 0: ok = binary_emit_mulsd_xmm_xmm(b, BINARY_XMM0, BINARY_XMM1); break;
    case 1: ok = binary_emit_addsd_xmm_xmm(b, BINARY_XMM0, BINARY_XMM1); break;
    case 2: ok = binary_emit_subsd_xmm_xmm(b, BINARY_XMM0, BINARY_XMM1); break;
    case 3:
      ok = binary_emit_subsd_xmm_xmm(b, BINARY_XMM1, BINARY_XMM0) &&
           wcs_sse_f2(b, 0x10, 0, 1) ;
      break;
    case 4: ok = binary_emit_divsd_xmm_xmm(b, BINARY_XMM0, BINARY_XMM1); break;
    default: code_generator_set_error(generator, "bad i2f step op"); return 0;
    }
    if (!ok) {
      return 0;
    }
  }
  if (!binary_emit_cvttsd2si_reg_xmm(b, BINARY_GP_RAX, BINARY_XMM0) ||
      !binary_emit_cvtsi2sd_xmm_reg(b, BINARY_XMM0, BINARY_GP_RAX) ||
      !binary_emit_addsd_xmm_xmm(b, BINARY_XMM3, BINARY_XMM0) ||
      !wcs_addsub_reg_imm8(b, BINARY_GP_RCX, 0, 1) ||
      !wcs_jcc(b, 0, &back) || !wcs_patch_to(b, back, rem_top)) {
    return 0;
  }

  if (!wcs_patch_here(b, j_done) ||
      !binary_emit_add_rsp_imm32(b, const_bytes)  ||
      !wcs_reduce_pd_acc_to_rax(b) ||
      !binary_emit_movq_xmm_reg(b, BINARY_XMM0, BINARY_GP_RAX) ||
      !binary_emit_cvttsd2si_reg_xmm(b, BINARY_GP_R10, BINARY_XMM0) ||
      !code_generator_binary_emit_operand_load(generator, context,
                                               &instruction->dest,
                                               BINARY_GP_RAX) ||
      !wcs_add_reg_reg64(b, BINARY_GP_RAX, BINARY_GP_R10)) {
    return 0;
  }
  return code_generator_binary_emit_destination_store(generator, context,
                                                      &instruction->dest,
                                                      BINARY_GP_RAX);
}

int code_generator_binary_emit_simd_dot_f32(
    CodeGenerator *generator, BinaryFunctionContext *context,
    const IRInstruction *instruction) {
  BinaryCodeBuffer *b = NULL;
  size_t loop_top = 0;
  size_t j_done = 0;
  size_t j_vec2 = 0;
  size_t j_vec = 0;
  size_t j_scalar = 0;

  if (!generator || !context || !instruction ||
      instruction->argument_count < 1 || !instruction->arguments) {
    code_generator_set_error(generator, "Malformed simd_dot_f32");
    return 0;
  }
  b = &context->code;

  if (!code_generator_binary_emit_operand_load(generator, context,
                                               &instruction->dest,
                                               BINARY_GP_RAX) ||
      !binary_emit_movd_xmm_reg(b, BINARY_XMM3, BINARY_GP_RAX) ||
      !code_generator_binary_emit_operand_load(generator, context,
                                               &instruction->lhs,
                                               BINARY_GP_RCX) ||
      !code_generator_binary_emit_operand_load(generator, context,
                                               &instruction->rhs,
                                               BINARY_GP_RDX) ||
      !code_generator_binary_emit_operand_load(generator, context,
                                               &instruction->arguments[0],
                                               BINARY_GP_R8) ||
      !wcs_mov_reg_reg32(b, BINARY_GP_R9, BINARY_GP_R8) ||
      !wcs_shift_reg_imm(b, BINARY_GP_R9, 0, 2) ||
      !wcs_add_reg_reg64(b, BINARY_GP_R9, BINARY_GP_RCX) ||
      !wcs_avx_vpxor_ymm(b, 2, 2, 2) ||
      !wcs_avx_vpxor_ymm(b, 4, 4, 4)) {
    return 0;
  }

  loop_top = b->size;
  if (!binary_emit_cmp_reg_reg(b, BINARY_GP_RCX, BINARY_GP_R9) ||
      !wcs_jcc(b, 0x83 , &j_done) ||
      !binary_emit_mov_reg_reg(b, BINARY_GP_R10, BINARY_GP_R9) ||
      !binary_emit_alu_reg_reg(b, 0x29, BINARY_GP_R10, BINARY_GP_RCX) ||
      !wcs_cmp_reg_imm32(b, BINARY_GP_R10, 64) ||
      !wcs_jcc(b, 0x83 , &j_vec2) ||
      !wcs_cmp_reg_imm32(b, BINARY_GP_R10, 32) ||
      !wcs_jcc(b, 0x83 , &j_vec) ||
      !wcs_jcc(b, 0, &j_scalar)) {
    return 0;
  }

  if (!wcs_patch_here(b, j_vec2) ||
      !wcs_avx_vmovups_ymm_mem(b, 0, BINARY_GP_RCX, 0) ||
      !wcs_avx_vmovups_ymm_mem(b, 1, BINARY_GP_RDX, 0) ||
      !wcs_avx_vfmadd231ps_ymm(b, 2, 0, 1) ||
      !wcs_avx_vmovups_ymm_mem(b, 0, BINARY_GP_RCX, 32) ||
      !wcs_avx_vmovups_ymm_mem(b, 1, BINARY_GP_RDX, 32) ||
      !wcs_avx_vfmadd231ps_ymm(b, 4, 0, 1) ||
      !wcs_addsub_reg_imm8(b, BINARY_GP_RCX, 0, 64) ||
      !wcs_addsub_reg_imm8(b, BINARY_GP_RDX, 0, 64)) {
    return 0;
  }
  {
    size_t j_back = 0;
    if (!wcs_jcc(b, 0, &j_back) || !wcs_patch_to(b, j_back, loop_top)) {
      return 0;
    }
  }

  if (!wcs_patch_here(b, j_vec) ||
      !wcs_avx_vmovups_ymm_mem(b, 0, BINARY_GP_RCX, 0) ||
      !wcs_avx_vmovups_ymm_mem(b, 1, BINARY_GP_RDX, 0) ||
      !wcs_avx_vfmadd231ps_ymm(b, 2, 0, 1) ||
      !wcs_addsub_reg_imm8(b, BINARY_GP_RCX, 0, 32) ||
      !wcs_addsub_reg_imm8(b, BINARY_GP_RDX, 0, 32)) {
    return 0;
  }
  {
    size_t j_back = 0;
    if (!wcs_jcc(b, 0, &j_back) || !wcs_patch_to(b, j_back, loop_top)) {
      return 0;
    }
  }

  if (!wcs_patch_here(b, j_scalar) ||
      !wcs_movss_xmm_mem(b, 0, BINARY_GP_RCX, 0) ||
      !wcs_movss_xmm_mem(b, 1, BINARY_GP_RDX, 0) ||
      !wcs_fmadd231ss(b, 3, 0, 1) ||
      !wcs_addsub_reg_imm8(b, BINARY_GP_RCX, 0, 4) ||
      !wcs_addsub_reg_imm8(b, BINARY_GP_RDX, 0, 4)) {
    return 0;
  }
  {
    size_t j_back = 0;
    if (!wcs_jcc(b, 0, &j_back) || !wcs_patch_to(b, j_back, loop_top)) {
      return 0;
    }
  }

  if (!wcs_patch_here(b, j_done) ||
      !wcs_avx_vaddps_ymm(b, 2, 2, 4) ||
      !wcs_reduce_ps_acc_to_rax(b)) {
    return 0;
  }
  return code_generator_binary_emit_destination_store(generator, context,
                                                      &instruction->dest,
                                                      BINARY_GP_RAX);
}

static uint32_t exp_f32_bits(float f) {
  uint32_t u;
  memcpy(&u, &f, 4);
  return u;
}

#define EXP_POOL_OFF 32
#define EXP_C(i) (EXP_POOL_OFF + (i) * 4)
static const float exp_pool_consts[13] = {
    88.3762626647949f,    -88.3762626647949f,  1.44269504088896341f,
    0.5f,                 0.693359375f,        -2.12194440e-4f,
    1.9875691500E-4f,     1.3981999507E-3f,    8.3334519073E-3f,
    4.1665795894E-2f,     1.6666665459E-1f,    5.0000001201E-1f,
    1.0f};

static int exp_compute(BinaryCodeBuffer *b) {
  if (!wcs_avx_vbroadcastss_ymm_mem(b, 4, BINARY_GP_RSP, EXP_C(0)) ||
      !wcs_avx_vminps_ymm(b, 0, 0, 4) ||
      !wcs_avx_vbroadcastss_ymm_mem(b, 4, BINARY_GP_RSP, EXP_C(1)) ||
      !wcs_avx_vmaxps_ymm(b, 0, 0, 4)) {
    return 0;
  }
  if (!wcs_avx_vbroadcastss_ymm_mem(b, 1, BINARY_GP_RSP, EXP_C(2)) ||
      !wcs_avx_vmulps_ymm(b, 1, 0, 1) ||
      !wcs_avx_vbroadcastss_ymm_mem(b, 4, BINARY_GP_RSP, EXP_C(3)) ||
      !wcs_avx_vaddps_ymm(b, 1, 1, 4) ||
      !wcs_avx_vroundps_ymm(b, 1, 1, 1 )) {
    return 0;
  }
  if (!wcs_avx_vbroadcastss_ymm_mem(b, 4, BINARY_GP_RSP, EXP_C(4)) ||
      !wcs_avx_vmulps_ymm(b, 5, 1, 4) || !wcs_avx_vsubps_ymm(b, 0, 0, 5) ||
      !wcs_avx_vbroadcastss_ymm_mem(b, 4, BINARY_GP_RSP, EXP_C(5)) ||
      !wcs_avx_vmulps_ymm(b, 5, 1, 4) || !wcs_avx_vsubps_ymm(b, 0, 0, 5)) {
    return 0;
  }
  if (!wcs_avx_vbroadcastss_ymm_mem(b, 2, BINARY_GP_RSP, EXP_C(6)) ||
      !wcs_avx_vmulps_ymm(b, 2, 2, 0) ||
      !wcs_avx_vbroadcastss_ymm_mem(b, 4, BINARY_GP_RSP, EXP_C(7)) ||
      !wcs_avx_vaddps_ymm(b, 2, 2, 4)) {
    return 0;
  }
  for (int i = 8; i <= 11; i++) {
    if (!wcs_avx_vmulps_ymm(b, 2, 2, 0) ||
        !wcs_avx_vbroadcastss_ymm_mem(b, 4, BINARY_GP_RSP, EXP_C(i)) ||
        !wcs_avx_vaddps_ymm(b, 2, 2, 4)) {
      return 0;
    }
  }
  if (!wcs_avx_vmulps_ymm(b, 3, 0, 0) || !wcs_avx_vmulps_ymm(b, 2, 2, 3) ||
      !wcs_avx_vaddps_ymm(b, 2, 2, 0) ||
      !wcs_avx_vbroadcastss_ymm_mem(b, 4, BINARY_GP_RSP, EXP_C(12)) ||
      !wcs_avx_vaddps_ymm(b, 2, 2, 4)) {
    return 0;
  }
  if (!wcs_avx_vcvttps2dq_ymm(b, 1, 1) ||
      !wcs_avx_vbroadcastss_ymm_mem(b, 4, BINARY_GP_RSP, EXP_C(13)) ||
      !wcs_avx_vpaddd_ymm(b, 1, 1, 4) || !wcs_avx_vpslld_ymm_imm(b, 1, 1, 23) ||
      !wcs_avx_vmulps_ymm(b, 2, 2, 1)) {
    return 0;
  }
  return 1;
}

static int exp_fill_pool(BinaryCodeBuffer *b) {
  for (int i = 0; i < 14; i++) {
    uint32_t bits = (i == 13) ? 127u : exp_f32_bits(exp_pool_consts[i]);
    if (!binary_emit_mov_reg_imm32_zero_extend(b, BINARY_GP_RAX, bits) ||
        !binary_emit_mov_mem_reg32(b, BINARY_GP_RSP, EXP_C(i), BINARY_GP_RAX)) {
      return 0;
    }
  }
  return 1;
}

static int simd_tail_count_to_r8(BinaryCodeBuffer *b, size_t *tail) {
  return binary_emit_mov_reg_reg(b, BINARY_GP_R10, BINARY_GP_R9) &&
         wcs_addsub_reg_imm8(b, BINARY_GP_R10, 0 , 32) &&
         wcs_sub_reg_reg64(b, BINARY_GP_R10, BINARY_GP_RCX) &&
         binary_emit_shift_reg_imm8(b, 5 , BINARY_GP_R10, 2) &&
         binary_emit_mov_reg_reg(b, BINARY_GP_R8, BINARY_GP_R10) &&
         wcs_jcc(b, 0, tail);
}

static int simd_exp_step(BinaryCodeBuffer *b, size_t *done_main,
                         size_t *noclamp) {
  return wcs_avx_vmovups_ymm_mem(b, 0, BINARY_GP_RCX, 0) && exp_compute(b) &&
         wcs_avx_vmovups_mem_ymm(b, BINARY_GP_RCX, 0, 2) &&
         binary_emit_cmp_reg_reg(b, BINARY_GP_RCX, BINARY_GP_R9) &&
         wcs_jcc(b, 0x83 , done_main) &&
         wcs_addsub_reg_imm8(b, BINARY_GP_RCX, 0, 32) &&
         binary_emit_cmp_reg_reg(b, BINARY_GP_RCX, BINARY_GP_R9) &&
         wcs_jcc(b, 0x86 , noclamp);
}

int code_generator_binary_emit_simd_exp_f32(CodeGenerator *generator,
                                            BinaryFunctionContext *context,
                                            const IRInstruction *instruction) {
  BinaryCodeBuffer *b = NULL;
  size_t loop_top = 0, done_main = 0, small = 0, fin = 0, noclamp = 0;
  size_t tail = 0;
  if (!generator || !context || !instruction ||
      instruction->argument_count < 1 || !instruction->arguments) {
    code_generator_set_error(generator, "Malformed simd_exp_f32");
    return 0;
  }
  b = &context->code;

  if (!code_generator_binary_emit_operand_load(generator, context,
                                               &instruction->dest,
                                               BINARY_GP_RCX) ||
      !code_generator_binary_emit_operand_load(generator, context,
                                               &instruction->arguments[0],
                                               BINARY_GP_R8) ||
      !binary_emit_mov_reg_reg(b, BINARY_GP_R9, BINARY_GP_R8) ||
      !binary_emit_shift_reg_imm8(b, 4, BINARY_GP_R9, 2) ||
      !wcs_add_reg_reg64(b, BINARY_GP_R9, BINARY_GP_RCX)) {
    return 0;
  }
  if (!binary_emit_sub_rsp_imm32(b, 96) || !exp_fill_pool(b)) {
    return 0;
  }
  if (!wcs_cmp_reg_imm32(b, BINARY_GP_R8, 8) ||
      !wcs_jcc(b, 0x82 , &small)) {
    return 0;
  }
  if (!wcs_addsub_reg_imm8(b, BINARY_GP_R9, 1 , 32)) {
    return 0;
  }
  loop_top = b->size;
  if (!simd_exp_step(b, &done_main, &noclamp)) {
    return 0;
  }
  if (!simd_tail_count_to_r8(b, &tail)) {
    return 0;
  }
  if (!wcs_patch_here(b, noclamp)) {
    return 0;
  }
  {
    size_t jb = 0;
    if (!wcs_jcc(b, 0, &jb) || !wcs_patch_to(b, jb, loop_top)) {
      return 0;
    }
  }
  if (!wcs_patch_here(b, done_main) || !wcs_jcc(b, 0, &fin)) {
    return 0;
  }

  if (!wcs_patch_here(b, small) || !wcs_patch_here(b, tail) ||
      !binary_emit_mov_reg_reg(b, BINARY_GP_R9, BINARY_GP_RCX) ||
      !binary_emit_mov_reg_reg(b, BINARY_GP_R11, BINARY_GP_RSP) ||
      !binary_emit_mov_reg_reg(b, BINARY_GP_R10, BINARY_GP_R8)) {
    return 0;
  }
  {
    size_t ci = b->size, di = 0;
    if (!binary_emit_test_reg_reg(b, BINARY_GP_R10) ||
        !wcs_jcc(b, 0x84 , &di) ||
        !binary_emit_mov_reg_mem32(b, BINARY_GP_RAX, BINARY_GP_RCX, 0) ||
        !binary_emit_mov_mem_reg32(b, BINARY_GP_R11, 0, BINARY_GP_RAX) ||
        !wcs_addsub_reg_imm8(b, BINARY_GP_RCX, 0, 4) ||
        !wcs_addsub_reg_imm8(b, BINARY_GP_R11, 0, 4) ||
        !binary_emit_sub_reg_imm32(b, BINARY_GP_R10, 1)) {
      return 0;
    }
    size_t jb = 0;
    if (!wcs_jcc(b, 0, &jb) || !wcs_patch_to(b, jb, ci) ||
        !wcs_patch_here(b, di)) {
      return 0;
    }
  }
  if (!wcs_avx_vmovups_ymm_mem(b, 0, BINARY_GP_RSP, 0) || !exp_compute(b) ||
      !wcs_avx_vmovups_mem_ymm(b, BINARY_GP_RSP, 0, 2) ||
      !binary_emit_mov_reg_reg(b, BINARY_GP_R11, BINARY_GP_RSP) ||
      !binary_emit_mov_reg_reg(b, BINARY_GP_R10, BINARY_GP_R8)) {
    return 0;
  }
  {
    size_t co = b->size, dout = 0;
    if (!binary_emit_test_reg_reg(b, BINARY_GP_R10) ||
        !wcs_jcc(b, 0x84 , &dout) ||
        !binary_emit_mov_reg_mem32(b, BINARY_GP_RAX, BINARY_GP_R11, 0) ||
        !binary_emit_mov_mem_reg32(b, BINARY_GP_R9, 0, BINARY_GP_RAX) ||
        !wcs_addsub_reg_imm8(b, BINARY_GP_R9, 0, 4) ||
        !wcs_addsub_reg_imm8(b, BINARY_GP_R11, 0, 4) ||
        !binary_emit_sub_reg_imm32(b, BINARY_GP_R10, 1)) {
      return 0;
    }
    size_t jb = 0;
    if (!wcs_jcc(b, 0, &jb) || !wcs_patch_to(b, jb, co) ||
        !wcs_patch_here(b, dout)) {
      return 0;
    }
  }
  if (!wcs_patch_here(b, fin)) {
    return 0;
  }
  if (!binary_emit_add_rsp_imm32(b, 96)) {
    return 0;
  }
  return wcs_avx_vzeroupper(b);
}

#define SILU_USCRATCH 88

static int silu_vec(BinaryCodeBuffer *b, int has_mul) {
  if (!wcs_avx_vpxor_ymm(b, 0, 0, 0) ||
      !wcs_avx_vsubps_ymm(b, 0, 0, 6) ||
      !exp_compute(b) ||
      !wcs_avx_vbroadcastss_ymm_mem(b, 4, BINARY_GP_RSP, EXP_C(12)) ||
      !wcs_avx_vaddps_ymm(b, 2, 2, 4) ||
      !wcs_avx_vdivps_ymm(b, 6, 6, 2)) {
    return 0;
  }
  if (has_mul && !wcs_avx_vmulps_ymm(b, 6, 6, 7)) {
    return 0;
  }
  return 1;
}

int code_generator_binary_emit_simd_silu_f32_inline(BinaryCodeBuffer *b,
                                                    int has_mul) {
  size_t loop_top = 0, done_main = 0, small = 0, fin = 0, noclamp = 0;
  size_t tail = 0;
  if (!binary_emit_mov_reg_reg(b, BINARY_GP_R9, BINARY_GP_R8) ||
      !binary_emit_shift_reg_imm8(b, 4, BINARY_GP_R9, 2) ||
      !wcs_add_reg_reg64(b, BINARY_GP_R9, BINARY_GP_RCX)) {
    return 0;
  }
  if (!binary_emit_sub_rsp_imm32(b, 128) || !exp_fill_pool(b)) {
    return 0;
  }
  if (!wcs_cmp_reg_imm32(b, BINARY_GP_R8, 8) ||
      !wcs_jcc(b, 0x82 , &small)) {
    return 0;
  }
  if (!wcs_addsub_reg_imm8(b, BINARY_GP_R9, 1 , 32)) {
    return 0;
  }
  loop_top = b->size;
  if (!wcs_avx_vmovups_ymm_mem(b, 6, BINARY_GP_RCX, 0)) {
    return 0;
  }
  if (has_mul && !wcs_avx_vmovups_ymm_mem(b, 7, BINARY_GP_RDX, 0)) {
    return 0;
  }
  if (!silu_vec(b, has_mul) ||
      !wcs_avx_vmovups_mem_ymm(b, BINARY_GP_RCX, 0, 6) ||
      !binary_emit_cmp_reg_reg(b, BINARY_GP_RCX, BINARY_GP_R9) ||
      !wcs_jcc(b, 0x83 , &done_main) ||
      !wcs_addsub_reg_imm8(b, BINARY_GP_RCX, 0, 32)) {
    return 0;
  }
  if (has_mul && !wcs_addsub_reg_imm8(b, BINARY_GP_RDX, 0, 32)) {
    return 0;
  }
  if (!binary_emit_cmp_reg_reg(b, BINARY_GP_RCX, BINARY_GP_R9) ||
      !wcs_jcc(b, 0x86 , &noclamp)) {
    return 0;
  }
  if (!simd_tail_count_to_r8(b, &tail)) {
    return 0;
  }
  if (!wcs_patch_here(b, noclamp)) {
    return 0;
  }
  {
    size_t jb = 0;
    if (!wcs_jcc(b, 0, &jb) || !wcs_patch_to(b, jb, loop_top)) {
      return 0;
    }
  }
  if (!wcs_patch_here(b, done_main) || !wcs_jcc(b, 0, &fin)) {
    return 0;
  }

  if (!wcs_patch_here(b, small) || !wcs_patch_here(b, tail) ||
      !binary_emit_mov_reg_reg(b, BINARY_GP_R9, BINARY_GP_RCX)) {
    return 0;
  }
  if (!binary_emit_mov_reg_reg(b, BINARY_GP_R11, BINARY_GP_RSP) ||
      !binary_emit_mov_reg_reg(b, BINARY_GP_R10, BINARY_GP_R8)) {
    return 0;
  }
  {
    size_t ci = b->size, di = 0;
    if (!binary_emit_test_reg_reg(b, BINARY_GP_R10) ||
        !wcs_jcc(b, 0x84 , &di) ||
        !binary_emit_mov_reg_mem32(b, BINARY_GP_RAX, BINARY_GP_RCX, 0) ||
        !binary_emit_mov_mem_reg32(b, BINARY_GP_R11, 0, BINARY_GP_RAX)) {
      return 0;
    }
    if (has_mul &&
        (!binary_emit_mov_reg_mem32(b, BINARY_GP_RAX, BINARY_GP_RDX, 0) ||
         !binary_emit_mov_mem_reg32(b, BINARY_GP_R11, SILU_USCRATCH,
                                    BINARY_GP_RAX) ||
         !wcs_addsub_reg_imm8(b, BINARY_GP_RDX, 0, 4))) {
      return 0;
    }
    if (!wcs_addsub_reg_imm8(b, BINARY_GP_RCX, 0, 4) ||
        !wcs_addsub_reg_imm8(b, BINARY_GP_R11, 0, 4) ||
        !binary_emit_sub_reg_imm32(b, BINARY_GP_R10, 1)) {
      return 0;
    }
    size_t jb = 0;
    if (!wcs_jcc(b, 0, &jb) || !wcs_patch_to(b, jb, ci) ||
        !wcs_patch_here(b, di)) {
      return 0;
    }
  }
  if (!wcs_avx_vmovups_ymm_mem(b, 6, BINARY_GP_RSP, 0)) {
    return 0;
  }
  if (has_mul &&
      !wcs_avx_vmovups_ymm_mem(b, 7, BINARY_GP_RSP, SILU_USCRATCH)) {
    return 0;
  }
  if (!silu_vec(b, has_mul) ||
      !wcs_avx_vmovups_mem_ymm(b, BINARY_GP_RSP, 0, 6)) {
    return 0;
  }
  if (!binary_emit_mov_reg_reg(b, BINARY_GP_R11, BINARY_GP_RSP) ||
      !binary_emit_mov_reg_reg(b, BINARY_GP_R10, BINARY_GP_R8)) {
    return 0;
  }
  {
    size_t co = b->size, dout = 0;
    if (!binary_emit_test_reg_reg(b, BINARY_GP_R10) ||
        !wcs_jcc(b, 0x84 , &dout) ||
        !binary_emit_mov_reg_mem32(b, BINARY_GP_RAX, BINARY_GP_R11, 0) ||
        !binary_emit_mov_mem_reg32(b, BINARY_GP_R9, 0, BINARY_GP_RAX) ||
        !wcs_addsub_reg_imm8(b, BINARY_GP_R9, 0, 4) ||
        !wcs_addsub_reg_imm8(b, BINARY_GP_R11, 0, 4) ||
        !binary_emit_sub_reg_imm32(b, BINARY_GP_R10, 1)) {
      return 0;
    }
    size_t jb = 0;
    if (!wcs_jcc(b, 0, &jb) || !wcs_patch_to(b, jb, co) ||
        !wcs_patch_here(b, dout)) {
      return 0;
    }
  }
  if (!wcs_patch_here(b, fin) || !binary_emit_add_rsp_imm32(b, 128)) {
    return 0;
  }
  return wcs_avx_vzeroupper(b);
}
