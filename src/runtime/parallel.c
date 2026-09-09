#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define METTLE_PARALLEL_MAX_THREADS 16u

typedef void (*MettleParallelBody)(void *ctx, long long lo, long long hi);

typedef struct {
  MettleParallelBody body;
  void *ctx;
  long long lo;
  long long hi;
} MettleParallelChunk;

#if defined(_WIN32) || defined(_WIN64)

typedef struct {
  unsigned long dwOemId;
  unsigned long dwPageSize;
  void *lpMinimumApplicationAddress;
  void *lpMaximumApplicationAddress;
  unsigned long long dwActiveProcessorMask;
  unsigned long dwNumberOfProcessors;
  unsigned long dwProcessorType;
  unsigned long dwAllocationGranularity;
  unsigned short wProcessorLevel;
  unsigned short wProcessorRevision;
} MettleSystemInfo;

__declspec(dllimport) void __stdcall GetSystemInfo(MettleSystemInfo *info);
__declspec(dllimport) void *__stdcall CreateThread(
    void *attributes, size_t stack_size,
    unsigned long(__stdcall *start)(void *), void *argument,
    unsigned long flags, unsigned long *thread_id);
__declspec(dllimport) unsigned long __stdcall WaitForSingleObject(
    void *handle, unsigned long milliseconds);
__declspec(dllimport) int __stdcall CloseHandle(void *handle);

static unsigned long __stdcall mettle_parallel_trampoline(void *argument) {
  MettleParallelChunk *chunk = (MettleParallelChunk *)argument;
  chunk->body(chunk->ctx, chunk->lo, chunk->hi);
  return 0;
}

static unsigned mettle_parallel_hardware_threads(void) {
  MettleSystemInfo info;
  info.dwNumberOfProcessors = 0;
  GetSystemInfo(&info);
  return info.dwNumberOfProcessors ? (unsigned)info.dwNumberOfProcessors : 1u;
}

#else

extern int pthread_create(long long *thread, const void *attributes,
                          unsigned long long (*start)(void *), void *argument);
extern int pthread_join(long long thread, void **result);

static unsigned long long mettle_parallel_trampoline(void *argument) {
  MettleParallelChunk *chunk = (MettleParallelChunk *)argument;
  chunk->body(chunk->ctx, chunk->lo, chunk->hi);
  return 0;
}

static unsigned mettle_parallel_hardware_threads(void) { return 4u; }

#endif

static unsigned mettle_parallel_threads(void) {
  static int cached = -1;
  if (cached < 0) {
    const char *spec = getenv("METTLE_PARALLEL_THREADS");
    long requested = spec && spec[0] ? atol(spec) : 0;
    if (requested <= 0) {
      requested = (long)mettle_parallel_hardware_threads();
    }
    if (requested < 1) {
      requested = 1;
    }
    if (requested > (long)METTLE_PARALLEL_MAX_THREADS) {
      requested = (long)METTLE_PARALLEL_MAX_THREADS;
    }
    cached = (int)requested;
  }
  return (unsigned)cached;
}

void mettle_parallel_range(MettleParallelBody body, void *ctx, long long lo,
                           long long hi, long long min_chunk) {
  MettleParallelChunk chunks[METTLE_PARALLEL_MAX_THREADS];
  long long total = hi - lo;
  long long span;
  long long cursor;
  unsigned wanted;
  unsigned spawned = 0;
#if defined(_WIN32) || defined(_WIN64)
  void *handles[METTLE_PARALLEL_MAX_THREADS];
#else
  long long handles[METTLE_PARALLEL_MAX_THREADS];
#endif

  if (!body || total <= 0) {
    return;
  }
  if (min_chunk < 1) {
    min_chunk = 1;
  }
  wanted = mettle_parallel_threads();
  if ((long long)wanted > total / min_chunk) {
    wanted = (unsigned)(total / min_chunk);
  }
  if (wanted < 2u) {
    body(ctx, lo, hi);
    return;
  }

  span = (total + (long long)wanted - 1) / (long long)wanted;
  cursor = lo;
  for (unsigned i = 0; i + 1u < wanted && cursor < hi; i++) {
    long long end = cursor + span;
    if (end > hi) {
      end = hi;
    }
    chunks[spawned].body = body;
    chunks[spawned].ctx = ctx;
    chunks[spawned].lo = cursor;
    chunks[spawned].hi = end;
#if defined(_WIN32) || defined(_WIN64)
    handles[spawned] = CreateThread(NULL, 0, mettle_parallel_trampoline,
                                    &chunks[spawned], 0, NULL);
    if (!handles[spawned]) {
      break;
    }
#else
    if (pthread_create(&handles[spawned], NULL, mettle_parallel_trampoline,
                       &chunks[spawned]) != 0) {
      break;
    }
#endif
    spawned++;
    cursor = end;
  }

  if (cursor < hi) {
    body(ctx, cursor, hi);
  }

  for (unsigned i = 0; i < spawned; i++) {
#if defined(_WIN32) || defined(_WIN64)
    (void)WaitForSingleObject(handles[i], 0xFFFFFFFFu);
    (void)CloseHandle(handles[i]);
#else
    (void)pthread_join(handles[i], NULL);
#endif
  }
}

typedef struct {
  unsigned char *destination;
  const unsigned char *source;
  long long value;
  int element_size;
} MettleParallelMemArgs;

static void mettle_parallel_fill_body(void *ctx, long long lo, long long hi) {
  MettleParallelMemArgs *args = (MettleParallelMemArgs *)ctx;
  unsigned char *destination = args->destination;
  int size = args->element_size;
  long long value = args->value;

  if (size == 1) {
    unsigned char byte = (unsigned char)value;
    for (long long i = lo; i < hi; i++) {
      destination[i] = byte;
    }
    return;
  }
  if (size == 2) {
    for (long long i = lo; i < hi; i++) {
      ((uint16_t *)destination)[i] = (uint16_t)value;
    }
    return;
  }
  if (size == 4) {
    for (long long i = lo; i < hi; i++) {
      ((uint32_t *)destination)[i] = (uint32_t)value;
    }
    return;
  }
  for (long long i = lo; i < hi; i++) {
    ((uint64_t *)destination)[i] = (uint64_t)value;
  }
}

static void mettle_parallel_copy_body(void *ctx, long long lo, long long hi) {
  MettleParallelMemArgs *args = (MettleParallelMemArgs *)ctx;
  unsigned char *destination = args->destination + lo;
  const unsigned char *source = args->source + lo;
  long long count = hi - lo;
  for (long long i = 0; i < count; i++) {
    destination[i] = source[i];
  }
}

void mettle_parallel_fill(void *destination, long long count, long long value,
                          int element_size) {
  MettleParallelMemArgs args;
  if (!destination || count <= 0) {
    return;
  }
  if (element_size != 1 && element_size != 2 && element_size != 4 &&
      element_size != 8) {
    element_size = 1;
  }
  args.destination = (unsigned char *)destination;
  args.source = NULL;
  args.value = value;
  args.element_size = element_size;
  mettle_parallel_range(mettle_parallel_fill_body, &args, 0, count,
                        262144 / element_size);
}

void mettle_parallel_copy_bytes(void *destination, const void *source,
                                long long bytes) {
  MettleParallelMemArgs args;
  unsigned char *d = (unsigned char *)destination;
  const unsigned char *s = (const unsigned char *)source;
  if (!destination || !source || bytes <= 0) {
    return;
  }
  if (d < s + bytes && s < d + bytes) {
    if (d < s) {
      for (long long i = 0; i < bytes; i++) {
        d[i] = s[i];
      }
    } else if (d > s) {
      for (long long i = bytes; i > 0; i--) {
        d[i - 1] = s[i - 1];
      }
    }
    return;
  }
  args.destination = d;
  args.source = s;
  args.value = 0;
  args.element_size = 1;
  mettle_parallel_range(mettle_parallel_copy_body, &args, 0, bytes, 262144);
}
