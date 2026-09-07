#ifndef COMPTIME_VALUE_H
#define COMPTIME_VALUE_H

#include <stdint.h>

typedef enum {
  COMPTIME_NONE = 0,
  COMPTIME_INT,
  COMPTIME_FLOAT,
  COMPTIME_TYPE_REF,
  COMPTIME_FIELD_REF,
  COMPTIME_STRING,
  COMPTIME_SEQUENCE,
  COMPTIME_ROW
} ComptimeValueKind;

typedef struct {
  uint32_t type_index;
} TypeRef;

typedef struct {
  uint32_t type_index;
  uint32_t field_index;
} FieldRef;

typedef struct {
  const char *value;
} ComptimeString;

struct ComptimeValue;

typedef struct {
  const struct ComptimeValue *items;
  uint32_t count;
} ComptimeSequence;

typedef struct {
  const void *literal;
  uint32_t type_index;
  uint32_t index;
} ComptimeRow;

typedef struct ComptimeValue {
  ComptimeValueKind kind;
  union {
    long long int_value;
    double float_value;
    TypeRef type_ref;
    FieldRef field_ref;
    ComptimeString string;
    ComptimeSequence sequence;
    ComptimeRow row;
  } as;
} ComptimeValue;

ComptimeValue comptime_none(void);
ComptimeValue comptime_int(long long value);
ComptimeValue comptime_float(double value);
ComptimeValue comptime_type_ref(uint32_t type_index);
ComptimeValue comptime_field_ref(uint32_t type_index, uint32_t field_index);
ComptimeValue comptime_string(const char *value);
ComptimeValue comptime_sequence(const ComptimeValue *items, uint32_t count);
ComptimeValue comptime_row(const void *literal, uint32_t type_index,
                           uint32_t index);

int comptime_is_none(ComptimeValue value);
int comptime_is_reflection(ComptimeValue value);
const char *comptime_kind_name(ComptimeValueKind kind);

#endif
