

#include "codegen/binary/arm64.h"
#include "codegen/binary/arm64_emit.h"
#include "codegen/binary/arm64_mir.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int g_fail = 0;

static uint32_t arm64_svc0(void) { return 0xD4000001u; }

#define NR_EXIT 93

#define ELF_BASE 0x400000u
#define ELF_HDRS 120u

static void put16(unsigned char *p, uint16_t v) { memcpy(p, &v, 2); }
static void put32(unsigned char *p, uint32_t v) { memcpy(p, &v, 4); }
static void put64(unsigned char *p, uint64_t v) { memcpy(p, &v, 8); }

static int write_elf(const char *path, const unsigned char *code,
                     size_t code_len) {
  unsigned char hdr[ELF_HDRS];
  memset(hdr, 0, sizeof(hdr));
  uint64_t total = ELF_HDRS + code_len;
  uint64_t entry = ELF_BASE + ELF_HDRS;

  hdr[0] = 0x7F; hdr[1] = 'E'; hdr[2] = 'L'; hdr[3] = 'F';
  hdr[4] = 2;
  hdr[5] = 1;
  hdr[6] = 1;
  put16(hdr + 16, 2);
  put16(hdr + 18, 183);
  put32(hdr + 20, 1);
  put64(hdr + 24, entry);
  put64(hdr + 32, 64);
  put64(hdr + 40, 0);
  put32(hdr + 48, 0);
  put16(hdr + 52, 64);
  put16(hdr + 54, 56);
  put16(hdr + 56, 1);
  put16(hdr + 58, 0);
  put16(hdr + 60, 0);
  put16(hdr + 62, 0);

  unsigned char *ph = hdr + 64;
  put32(ph + 0, 1);
  put32(ph + 4, 5);
  put64(ph + 8, 0);
  put64(ph + 16, ELF_BASE);
  put64(ph + 24, ELF_BASE);
  put64(ph + 32, total);
  put64(ph + 40, total);
  put64(ph + 48, 0x1000);

  FILE *f = fopen(path, "wb");
  if (!f) {
    return 0;
  }
  int ok = fwrite(hdr, 1, ELF_HDRS, f) == ELF_HDRS &&
           fwrite(code, 1, code_len, f) == code_len;
  fclose(f);
  return ok;
}

typedef void (*BodyFn)(Arm64Emit *);

static void body_add(Arm64Emit *e) {
  arm64_emit_prologue(e, 0, NULL, 0);
  arm64_emit_word(e, arm64_add_reg(1, ARM64_X0, ARM64_X0, ARM64_X1));
  arm64_emit_epilogue(e, 0, NULL, 0);
}

static void body_sum_to_n(Arm64Emit *e) {
  int cond = arm64_new_label(e), done = arm64_new_label(e);
  arm64_emit_prologue(e, 0, NULL, 0);
  arm64_emit_word(e, arm64_movz(1, ARM64_X9, 0, 0));
  arm64_emit_word(e, arm64_movz(1, ARM64_X10, 1, 0));
  arm64_bind_label(e, cond);
  arm64_emit_word(e, arm64_cmp_reg(1, ARM64_X10, ARM64_X0));
  arm64_emit_bcond(e, ARM64_GT, done);
  arm64_emit_word(e, arm64_add_reg(1, ARM64_X9, ARM64_X9, ARM64_X10));
  arm64_emit_word(e, arm64_add_imm(1, ARM64_X10, ARM64_X10, 1, 0));
  arm64_emit_b(e, cond);
  arm64_bind_label(e, done);
  arm64_emit_word(e, arm64_mov_reg(1, ARM64_X0, ARM64_X9));
  arm64_emit_epilogue(e, 0, NULL, 0);
}

static void body_fact(Arm64Emit *e) {
  int cond = arm64_new_label(e), done = arm64_new_label(e);
  arm64_emit_prologue(e, 0, NULL, 0);
  arm64_emit_word(e, arm64_movz(1, ARM64_X9, 1, 0));
  arm64_emit_word(e, arm64_movz(1, ARM64_X10, 1, 0));
  arm64_bind_label(e, cond);
  arm64_emit_word(e, arm64_cmp_reg(1, ARM64_X10, ARM64_X0));
  arm64_emit_bcond(e, ARM64_GT, done);
  arm64_emit_word(e, arm64_mul(1, ARM64_X9, ARM64_X9, ARM64_X10));
  arm64_emit_word(e, arm64_add_imm(1, ARM64_X10, ARM64_X10, 1, 0));
  arm64_emit_b(e, cond);
  arm64_bind_label(e, done);
  arm64_emit_word(e, arm64_mov_reg(1, ARM64_X0, ARM64_X9));
  arm64_emit_epilogue(e, 0, NULL, 0);
}

static void body_mod(Arm64Emit *e) {
  arm64_emit_prologue(e, 0, NULL, 0);
  arm64_emit_word(e, arm64_sdiv(1, ARM64_X9, ARM64_X0, ARM64_X1));
  arm64_emit_word(e, arm64_msub(1, ARM64_X0, ARM64_X9, ARM64_X1, ARM64_X0));
  arm64_emit_epilogue(e, 0, NULL, 0);
}

static void body_popcount(Arm64Emit *e) {
  int loop = arm64_new_label(e), done = arm64_new_label(e);
  arm64_emit_prologue(e, 0, NULL, 0);
  arm64_emit_word(e, arm64_movz(1, ARM64_X9, 0, 0));
  arm64_emit_word(e, arm64_movz(1, ARM64_X11, 1, 0));
  arm64_bind_label(e, loop);
  arm64_emit_cbz(e, 1, ARM64_X0, done);
  arm64_emit_word(e, arm64_and_reg(1, ARM64_X10, ARM64_X0, ARM64_X11));
  arm64_emit_word(e, arm64_add_reg(1, ARM64_X9, ARM64_X9, ARM64_X10));
  arm64_emit_word(e, arm64_lsr_imm(1, ARM64_X0, ARM64_X0, 1));
  arm64_emit_b(e, loop);
  arm64_bind_label(e, done);
  arm64_emit_word(e, arm64_mov_reg(1, ARM64_X0, ARM64_X9));
  arm64_emit_epilogue(e, 0, NULL, 0);
}

static void body_max(Arm64Emit *e) {
  arm64_emit_prologue(e, 0, NULL, 0);
  arm64_emit_word(e, arm64_cmp_reg(1, ARM64_X0, ARM64_X1));
  arm64_emit_word(e, arm64_csel(1, ARM64_X0, ARM64_X0, ARM64_X1, ARM64_GT));
  arm64_emit_epilogue(e, 0, NULL, 0);
}

static MirOperand P(int r) {
  MirOperand o;
  memset(&o, 0, sizeof o);
  o.kind = MIR_OPK_PHYS;
  o.phys = r;
  o.rclass = MIR_RC_GP;
  return o;
}
static MirOperand IMM(long long v) {
  MirOperand o;
  memset(&o, 0, sizeof o);
  o.kind = MIR_OPK_IMM;
  o.imm = v;
  return o;
}
static MirOperand V(int id) {
  MirOperand o;
  memset(&o, 0, sizeof o);
  o.kind = MIR_OPK_VREG;
  o.vreg = id;
  o.rclass = MIR_RC_GP;
  return o;
}
static MirOperand LBL(const char *s) {
  MirOperand o;
  memset(&o, 0, sizeof o);
  o.kind = MIR_OPK_LABEL;
  o.sym = s;
  return o;
}
static MirOperand NONE(void) {
  MirOperand o;
  memset(&o, 0, sizeof o);
  o.kind = MIR_OPK_NONE;
  return o;
}
static MirInst I(MirOpcode op, MirOperand dst, MirOperand a, MirOperand b) {
  MirInst in;
  memset(&in, 0, sizeof in);
  in.op = op;
  in.dst = dst;
  in.a = a;
  in.b = b;
  in.ir_index = -1;
  return in;
}

static void body_mir_add(Arm64Emit *e) {
  MirInst seq[] = {
      I(MIR_ADD, P(ARM64_X0), P(ARM64_X0), P(ARM64_X1)),
      I(MIR_RET, NONE(), NONE(), NONE()),
  };
  arm64_mir_encode_seq(e, seq, sizeof(seq) / sizeof(seq[0]));
}

static void body_mir_sum(Arm64Emit *e) {
  MirInst seq[] = {
      I(MIR_MOV, P(ARM64_X9), IMM(0), NONE()),
      I(MIR_MOV, P(ARM64_X10), IMM(1), NONE()),
      I(MIR_LABEL, LBL("Lcond"), NONE(), NONE()),
      I(MIR_CMPBR, LBL("Ldone"), P(ARM64_X10), P(ARM64_X0)),
      I(MIR_ADD, P(ARM64_X9), P(ARM64_X9), P(ARM64_X10)),
      I(MIR_ADD, P(ARM64_X10), P(ARM64_X10), IMM(1)),
      I(MIR_JMP, LBL("Lcond"), NONE(), NONE()),
      I(MIR_LABEL, LBL("Ldone"), NONE(), NONE()),
      I(MIR_MOV, P(ARM64_X0), P(ARM64_X9), NONE()),
      I(MIR_RET, NONE(), NONE(), NONE()),
  };
  seq[3].cc = 0x8F;
  arm64_mir_encode_seq(e, seq, sizeof(seq) / sizeof(seq[0]));
}

static void body_mir_isgt(Arm64Emit *e) {
  MirInst seq[] = {
      I(MIR_CMP, NONE(), P(ARM64_X0), P(ARM64_X1)),
      I(MIR_SETCC, P(ARM64_X0), NONE(), NONE()),
      I(MIR_RET, NONE(), NONE(), NONE()),
  };
  seq[1].cc = 0x9F;
  arm64_mir_encode_seq(e, seq, sizeof(seq) / sizeof(seq[0]));
}

static void body_mir_pack(Arm64Emit *e) {
  MirInst seq[] = {
      I(MIR_SHL, P(ARM64_X9), P(ARM64_X0), IMM(4)),
      I(MIR_OR, P(ARM64_X0), P(ARM64_X9), P(ARM64_X1)),
      I(MIR_RET, NONE(), NONE(), NONE()),
  };
  arm64_mir_encode_seq(e, seq, sizeof(seq) / sizeof(seq[0]));
}

static void body_mir_div(Arm64Emit *e) {
  MirInst seq[] = {
      I(MIR_IDIV, P(ARM64_X0), P(ARM64_X0), P(ARM64_X1)),
      I(MIR_RET, NONE(), NONE(), NONE()),
  };
  arm64_mir_encode_seq(e, seq, sizeof(seq) / sizeof(seq[0]));
}

static void body_mir_mod(Arm64Emit *e) {
  MirInst seq[] = {
      I(MIR_IDIV, P(ARM64_X0), P(ARM64_X0), P(ARM64_X1)),
      I(MIR_RET, NONE(), NONE(), NONE()),
  };
  seq[0].cc = 1;
  arm64_mir_encode_seq(e, seq, sizeof(seq) / sizeof(seq[0]));
}

static void body_mir_max(Arm64Emit *e) {
  MirInst seq[] = {
      I(MIR_CMP, NONE(), P(ARM64_X0), P(ARM64_X1)),
      I(MIR_CMOVCC, P(ARM64_X0), P(ARM64_X1), NONE()),
      I(MIR_RET, NONE(), NONE(), NONE()),
  };
  seq[1].cc = 0x8C;
  arm64_mir_encode_seq(e, seq, sizeof(seq) / sizeof(seq[0]));
}

static void body_mir_uxtb(Arm64Emit *e) {
  MirInst seq[] = {
      I(MIR_MOVZX, P(ARM64_X0), P(ARM64_X0), NONE()),
      I(MIR_RET, NONE(), NONE(), NONE()),
  };
  seq[0].width = 1;
  arm64_mir_encode_seq(e, seq, sizeof(seq) / sizeof(seq[0]));
}

static void body_mir_iseven(Arm64Emit *e) {
  MirInst seq[] = {
      I(MIR_MOV, P(ARM64_X9), IMM(1), NONE()),
      I(MIR_TEST, NONE(), P(ARM64_X0), P(ARM64_X9)),
      I(MIR_SETCC, P(ARM64_X0), NONE(), NONE()),
      I(MIR_RET, NONE(), NONE(), NONE()),
  };
  seq[2].cc = 0x94;
  arm64_mir_encode_seq(e, seq, sizeof(seq) / sizeof(seq[0]));
}

static void body_vmir_sum(Arm64Emit *e) {
  MirInst seq[] = {
      I(MIR_MOV, V(1), IMM(0), NONE()),
      I(MIR_MOV, V(2), IMM(1), NONE()),
      I(MIR_LABEL, LBL("Lc"), NONE(), NONE()),
      I(MIR_CMPBR, LBL("Ld"), V(2), V(0)),
      I(MIR_ADD, V(1), V(1), V(2)),
      I(MIR_ADD, V(2), V(2), IMM(1)),
      I(MIR_JMP, LBL("Lc"), NONE(), NONE()),
      I(MIR_LABEL, LBL("Ld"), NONE(), NONE()),
      I(MIR_RET, NONE(), V(1), NONE()),
  };
  seq[3].cc = 0x8F;
  arm64_mir_encode_vregs(e, seq, sizeof(seq) / sizeof(seq[0]), 3, 1);
}

static void body_vmir_poly(Arm64Emit *e) {
  MirInst seq[] = {
      I(MIR_IMUL, V(2), V(0), V(0)),
      I(MIR_ADD, V(2), V(2), V(1)),
      I(MIR_RET, NONE(), V(2), NONE()),
  };
  arm64_mir_encode_vregs(e, seq, sizeof(seq) / sizeof(seq[0]), 3, 2);
}

static int build_case(const char *out_dir, FILE *manifest, const char *name,
                      int expected, uint16_t a, uint16_t b, int nargs,
                      BodyFn body) {
  Arm64Emit e;
  arm64_emit_init(&e);
  int func = arm64_new_label(&e);

  arm64_emit_word(&e, arm64_movz(1, ARM64_X0, a, 0));
  if (nargs >= 2) {
    arm64_emit_word(&e, arm64_movz(1, ARM64_X1, b, 0));
  }
  arm64_emit_bl(&e, func);
  arm64_emit_word(&e, arm64_movz(1, ARM64_X8, NR_EXIT, 0));
  arm64_emit_word(&e, arm64_svc0());

  arm64_bind_label(&e, func);
  body(&e);

  if (!arm64_emit_finalize(&e)) {
    printf("  FAIL %-12s emit/finalize error\n", name);
    g_fail++;
    arm64_emit_free(&e);
    return 0;
  }

  int n_words = (int)(e.code.len / 4);
  int saw_ret = 0, saw_unknown = 0;
  for (int i = 0; i < n_words; i++) {
    uint32_t w;
    memcpy(&w, e.code.data + (size_t)i * 4, 4);
    Arm64Inst d = arm64_decode(w);
    if (d.op == ARM64_DIS_UNKNOWN && w != arm64_svc0()) {
      saw_unknown = 1;
    }
    if (d.op == ARM64_DIS_RET) {
      saw_ret = 1;
    }
  }
  if (saw_unknown || !saw_ret) {
    printf("  FAIL %-12s decode (unknown=%d ret=%d)\n", name, saw_unknown,
           saw_ret);
    g_fail++;
    arm64_emit_free(&e);
    return 0;
  }

  char path[1024];
  snprintf(path, sizeof(path), "%s/%s.elf", out_dir, name);
  if (!write_elf(path, e.code.data, e.code.len)) {
    printf("  FAIL %-12s write_elf %s\n", name, path);
    g_fail++;
    arm64_emit_free(&e);
    return 0;
  }

  if (manifest) {
    fprintf(manifest, "%s %d\n", name, expected);
  }
  printf("  EXEC %-12s expect %-4d %s\n", name, expected, path);
  arm64_emit_free(&e);
  return 1;
}

int main(int argc, char **argv) {
  const char *out_dir = (argc > 1) ? argv[1] : ".";
  printf("=== AArch64 emit + ELF self-test ===\n");

  char manifest_path[1024];
  snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.txt", out_dir);
  FILE *manifest = fopen(manifest_path, "wb");

  build_case(out_dir, manifest, "add", 12, 5, 7, 2, body_add);
  build_case(out_dir, manifest, "sum_to_n", 55, 10, 0, 1, body_sum_to_n);
  build_case(out_dir, manifest, "fact", 120, 5, 0, 1, body_fact);
  build_case(out_dir, manifest, "mod", 2, 17, 5, 2, body_mod);
  build_case(out_dir, manifest, "popcount", 3, 0xB, 0, 1, body_popcount);
  build_case(out_dir, manifest, "max", 20, 7, 20, 2, body_max);

  build_case(out_dir, manifest, "mir_add", 12, 5, 7, 2, body_mir_add);
  build_case(out_dir, manifest, "mir_sum", 55, 10, 0, 1, body_mir_sum);
  build_case(out_dir, manifest, "mir_isgt", 1, 20, 7, 2, body_mir_isgt);
  build_case(out_dir, manifest, "mir_pack", 35, 2, 3, 2, body_mir_pack);
  build_case(out_dir, manifest, "mir_div", 3, 17, 5, 2, body_mir_div);
  build_case(out_dir, manifest, "mir_mod", 2, 17, 5, 2, body_mir_mod);
  build_case(out_dir, manifest, "mir_max", 20, 7, 20, 2, body_mir_max);
  build_case(out_dir, manifest, "mir_uxtb", 255, 0x1FF, 0, 1, body_mir_uxtb);
  build_case(out_dir, manifest, "mir_iseven", 1, 6, 0, 1, body_mir_iseven);

  build_case(out_dir, manifest, "vmir_sum", 55, 10, 0, 1, body_vmir_sum);
  build_case(out_dir, manifest, "vmir_poly", 32, 5, 7, 2, body_vmir_poly);

  if (manifest) {
    fclose(manifest);
  }
  printf("\n%s\n", g_fail ? "RESULT: FAIL" : "RESULT: PASS");
  return g_fail ? 1 : 0;
}
