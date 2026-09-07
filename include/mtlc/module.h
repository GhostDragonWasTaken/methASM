
#ifndef MTLC_MODULE_H
#define MTLC_MODULE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MtlcModule MtlcModule;

MtlcModule *mtlc_module_adopt_ir(void *ir_program);

void *mtlc_module_ir(MtlcModule *module);

size_t mtlc_module_function_count(const MtlcModule *module);

void mtlc_module_destroy(MtlcModule *module);

#ifdef __cplusplus
}
#endif

#endif
