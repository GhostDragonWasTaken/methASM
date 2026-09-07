#ifndef METTLE_ATOMICS_H
#define METTLE_ATOMICS_H

#include <stdint.h>

int32_t mettle_atomic_compare_exchange_i32(int32_t *target, int32_t exchange,
                                         int32_t comparand);
int32_t mettle_atomic_exchange_i32(int32_t *target, int32_t value);
int32_t mettle_atomic_inc_i32(int32_t *target);
int32_t mettle_atomic_dec_i32(int32_t *target);

#endif
