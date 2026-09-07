#ifndef ML_OBS_H
#define ML_OBS_H

#include <stdint.h>

#define ML_OBS_NPROBE 8
#define ML_OBS_NBITS (ML_OBS_NPROBE * 64)
#define ML_OBS_NPROJ 32
#define ML_OBS_NSEM 4
#define ML_OBS_NOBS (ML_OBS_NPROJ + ML_OBS_NSEM)

typedef struct {
  uint64_t v[ML_OBS_NPROBE];
  int valid;
} MlObsFp;

int ml_obs_fingerprints(char **texts, int n, MlObsFp *fps,
                        MlObsFp **leaves, int *nleaves);

void ml_obs_features(char **texts, int n, float *out);

int ml_obs_semantic_edges(char **texts, int n, int *src, int *dst);

int ml_obs_edge_eligible(const MlObsFp *f);

uint64_t ml_obs_splitmix64(uint64_t x);
uint64_t ml_obs_fnv1a64(const char *s);
void ml_obs_projection_row(int r, uint64_t out[ML_OBS_NPROBE]);

int ml_obs_selftest(const char *golden_path);

#endif
