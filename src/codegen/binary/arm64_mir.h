#ifndef CODEGEN_BINARY_ARM64_MIR_H
#define CODEGEN_BINARY_ARM64_MIR_H

#include "codegen/binary/arm64_emit.h"
#include "codegen/binary/mir.h"

Arm64Cond arm64_cond_from_x86_cc(unsigned char x86_cc);

int arm64_mir_encode_seq(Arm64Emit *e, const MirInst *insns, size_t count);

int arm64_mir_encode_vregs(Arm64Emit *e, const MirInst *insns, size_t count,
                           int nvregs, int nparams);

#endif
