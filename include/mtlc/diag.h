
#ifndef MTLC_DIAG_H
#define MTLC_DIAG_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  MTLC_DIAG_ERROR,
  MTLC_DIAG_WARNING,
  MTLC_DIAG_NOTE
} MtlcDiagSeverity;

typedef void (*MtlcDiagHandler)(void *user_data, MtlcDiagSeverity severity,
                                const char *message);

const char *mtlc_diag_severity_name(MtlcDiagSeverity severity);

#ifdef __cplusplus
}
#endif

#endif
