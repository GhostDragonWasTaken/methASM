#ifndef METTLE_RUNTIME_PARALLEL_H
#define METTLE_RUNTIME_PARALLEL_H

#define METTLE_PARALLEL_MAX_THREADS 64u

typedef void (*MettleParallelBody)(void *ctx, long long lo, long long hi);
typedef void (*MettleParallelSlotBody)(void *ctx, long long lo, long long hi,
                                       long long thread_slot);

void mettle_parallel_range(MettleParallelBody body, void *ctx, long long lo,
                           long long hi, long long min_chunk);
void mettle_parallel_range_slots(MettleParallelSlotBody body, void *ctx,
                                 long long lo, long long hi,
                                 long long min_chunk);
long long mettle_parallel_threads(void);
void mettle_parallel_fill(void *destination, long long count, long long value,
                          int element_size);
void mettle_parallel_copy_bytes(void *destination, const void *source,
                                long long bytes);

#endif
