#ifndef IR_VALUES_H
#define IR_VALUES_H

#include <stddef.h>
#include <stdint.h>

#define IR_VALUE_ID_NONE 0u

typedef struct {
  char *name;
  unsigned char kind;
  uint32_t version;
  uint32_t base_id;
} IRValueEntry;

typedef struct {
  IRValueEntry *entries;
  size_t count;
  size_t capacity;
  uint32_t *buckets;
  size_t bucket_count;
} IRValueTable;

void ir_value_table_init(IRValueTable *table);
void ir_value_table_clear(IRValueTable *table);

uint32_t ir_value_table_intern(IRValueTable *table, unsigned char kind,
                               const char *name);
uint32_t ir_value_table_lookup(const IRValueTable *table, unsigned char kind,
                               const char *name);
uint32_t ir_value_table_intern_version(IRValueTable *table, uint32_t base_id,
                                       const char *name);

const char *ir_value_table_name(const IRValueTable *table, uint32_t id);
unsigned char ir_value_table_kind(const IRValueTable *table, uint32_t id);
uint32_t ir_value_table_version(const IRValueTable *table, uint32_t id);
uint32_t ir_value_table_base(const IRValueTable *table, uint32_t id);
size_t ir_value_table_count(const IRValueTable *table);

#endif
