#ifndef METTLE_RUNTIME_VERIFY_OWNED_H
#define METTLE_RUNTIME_VERIFY_OWNED_H

#include <stddef.h>

int mettle_verify_owned_executable(const char *path, char *reason,
                                   size_t reason_size);

int mettle_verify_owned_dynamic_executable(const char *path, char *reason,
                                           size_t reason_size);

int mettle_verify_owned_image(const unsigned char *data, size_t size,
                              char *reason, size_t reason_size);

int mettle_link_argument_uses_forbidden_runtime(const char *argument);

#endif
