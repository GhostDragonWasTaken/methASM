#ifndef MTLC_SOURCE_LOCATION_H
#define MTLC_SOURCE_LOCATION_H

#include <stddef.h>

typedef struct {
  size_t line;
  size_t column;
  const char *filename;
} SourceLocation;

#endif
