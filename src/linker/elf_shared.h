#ifndef METTLE_ELF_SHARED_H
#define METTLE_ELF_SHARED_H

#include <stddef.h>
#include <stdint.h>

#define ELF_SHARED_TYPE_NOTYPE 0u
#define ELF_SHARED_TYPE_OBJECT 1u
#define ELF_SHARED_TYPE_FUNC 2u
#define ELF_SHARED_TYPE_TLS 6u
#define ELF_SHARED_TYPE_IFUNC 10u

typedef struct {
  char *name;
  char *version;
  uint64_t size;
  uint64_t alignment;
  uint8_t type;
  int is_weak;
} ElfSharedSymbol;

typedef struct {
  char *path;
  char *soname;
  ElfSharedSymbol *symbols;
  size_t symbol_count;
  size_t symbol_capacity;
  size_t *buckets;
  size_t bucket_count;
} ElfSharedLibrary;

int elf_shared_library_read(const char *path, ElfSharedLibrary **library_out,
                            char **error_message_out);
void elf_shared_library_destroy(ElfSharedLibrary *library);
const ElfSharedSymbol *elf_shared_library_find(const ElfSharedLibrary *library,
                                               const char *name);

char *elf_shared_library_locate(const char *library_name,
                                const char *const *directories,
                                size_t directory_count,
                                char **error_message_out);

int elf_path_is_shared_library(const char *path);

#endif
