#ifndef SPIRV_EMITTER_H
#define SPIRV_EMITTER_H

#include "code_generator.h"
#include "ir/ir.h"
#include <stdio.h>

int spirv_emit_program(IRProgram *program, CodeGenerator *generator, FILE *out,
                       char **error);

#endif
