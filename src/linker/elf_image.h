#ifndef METTLE_ELF_IMAGE_H
#define METTLE_ELF_IMAGE_H

#include "linker/symbol_resolve.h"

#include <stdint.h>

typedef struct {
  uint64_t image_base;
  uint64_t page_size;
  int strip_symbols;
  const char *interpreter;
  int produce_shared_library;
  int export_dynamic;
  const char *soname;
  const char *const *runpaths;
  size_t runpath_count;
} ElfImageOptions;

int elf_image_emit_executable(LinkResolution *resolution,
                              const char *output_path,
                              const ElfImageOptions *options,
                              char **error_message_out);

#endif
