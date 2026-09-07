
#ifndef MTLC_PIPELINE_H
#define MTLC_PIPELINE_H

#include "context.h"
#include "module.h"
#include "target.h"

#ifdef __cplusplus
extern "C" {
#endif

int mtlc_optimize(MtlcContext *ctx, MtlcModule *module);

int mtlc_optimize_for(MtlcContext *ctx, MtlcModule *module, MtlcArch arch);

typedef struct {
  int proposals;
  int validated;
  int proven;
  int rejected;
  int skipped;
} MtlcMlOptStats;

int mtlc_apply_ml_opt(MtlcContext *ctx, MtlcModule *module,
                      MtlcMlOptStats *stats);

int mtlc_emit_object(MtlcContext *ctx, MtlcModule *module, const char *path);

int mtlc_emit(MtlcContext *ctx, MtlcModule *module, MtlcArch arch,
              const char *path);

int mtlc_build_executable(MtlcContext *ctx, MtlcModule *module,
                          const char *output_path);

#ifdef __cplusplus
}
#endif

#endif
