#ifndef METTLE_MACHINE_DESC_H
#define METTLE_MACHINE_DESC_H

#include "parser/ast.h"

#define MACHINE_MAX_INSNS 128
#define MACHINE_MAX_BYTES 32
#define MACHINE_OPERANDS 3

typedef struct {
  char name[64];
  char semantics[64];
  char reads[64];
  char writes[64];
  long long operands;
  unsigned char bytes[MACHINE_MAX_BYTES];
  int slot[MACHINE_MAX_BYTES];
  size_t length;
  size_t prefix;
  SourceLocation location;
} MachineInsn;

typedef struct {
  char name[64];
  MachineInsn insns[MACHINE_MAX_INSNS];
  size_t count;
  SourceLocation location;
} MachineDesc;

int machine_desc_read(ASTNode *program, MachineDesc *out, char *error,
                      size_t error_size, SourceLocation *error_at);

const MachineInsn *machine_desc_find(const MachineDesc *desc,
                                     const char *name);

int machine_assemble(const MachineDesc *desc, const char *line,
                     unsigned char *out, size_t capacity, size_t *written,
                     char *error, size_t error_size);

int machine_decode(const MachineDesc *desc, const unsigned char *bytes,
                   size_t length, size_t at, const MachineInsn **out,
                   long long *operands, size_t *consumed);

void machine_desc_print(FILE *out, const MachineDesc *desc);

#endif
