#include <stddef.h>
#include <stdint.h>

extern char *getenv(const char *name);

#define METTLE_PARALLEL_MAX_THREADS 16u

typedef void (*MettleParallelBody)(void *ctx, long long lo, long long hi);
typedef void (*MettleParallelSlotBody)(void *ctx, long long lo, long long hi,
                                       long long slot);

typedef struct {
  MettleParallelBody body;
  MettleParallelSlotBody slot_body;
  void *ctx;
  long long lo;
  long long hi;
  long long span;
  unsigned wanted;
} MettleParallelJob;

static MettleParallelJob g_job;
static volatile unsigned g_generation;
static volatile int g_remaining;
static volatile int g_pool_state;
static volatile int g_reentered;
static unsigned g_pool_workers;

static void mettle_parallel_worker(long index);

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
__declspec(dllimport) int __stdcall CloseHandle(void *handle);
__declspec(dllimport) int __stdcall SwitchToThread(void);
__declspec(dllimport) void __stdcall Sleep(unsigned long milliseconds);

static unsigned long __stdcall mettle_parallel_entry(void *argument) {
  mettle_parallel_worker((long)(size_t)argument);
  return 0;
}

static unsigned mettle_parallel_hardware_threads(void) {
  MettleSystemInfo info;
  info.dwNumberOfProcessors = 0;
  GetSystemInfo(&info);
  return info.dwNumberOfProcessors ? (unsigned)info.dwNumberOfProcessors : 1u;
}

static int mettle_parallel_spawn(long index) {
  void *handle = CreateThread(NULL, 0, mettle_parallel_entry,
                              (void *)(size_t)index, 0, NULL);
  if (!handle) {
    return 0;
  }
  (void)CloseHandle(handle);
  return 1;
}

static void mettle_parallel_yield(void) { (void)SwitchToThread(); }

static void mettle_parallel_nap(void) { Sleep(1); }

#else

struct mettle_parallel_timespec {
  long long seconds;
  long long nanoseconds;
};

extern int pthread_create(unsigned long *thread, const void *attributes,
                          void *(*start)(void *), void *argument);
extern int pthread_detach(unsigned long thread);

static long mettle_parallel_syscall(long number, long first, long second,
                                    long third) {
#if defined(__x86_64__)
  long result;
  __asm__ __volatile__("syscall"
                       : "=a"(result)
                       : "a"(number), "D"(first), "S"(second), "d"(third)
                       : "rcx", "r11", "memory");
  return result;
#else
  (void)number;
  (void)first;
  (void)second;
  (void)third;
  return -1;
#endif
}

static void *mettle_parallel_entry(void *argument) {
  mettle_parallel_worker((long)(size_t)argument);
  return NULL;
}

static unsigned mettle_parallel_hardware_threads(void) {
#if defined(__x86_64__)
  unsigned long long mask[16];
  long result;
  unsigned count = 0;
  for (unsigned i = 0; i < 16u; i++) {
    mask[i] = 0;
  }
  result = mettle_parallel_syscall(204L, 0L, (long)sizeof(mask),
                                   (long)(size_t)mask);
  if (result > 0) {
    for (unsigned i = 0; i < (unsigned)result / 8u && i < 16u; i++) {
      unsigned long long word = mask[i];
      while (word) {
        word &= word - 1ull;
        count++;
      }
    }
  }
  return count ? count : 1u;
#else
  return 4u;
#endif
}

static int mettle_parallel_spawn(long index) {
  unsigned long thread = 0;
  if (pthread_create(&thread, NULL, mettle_parallel_entry,
                     (void *)(size_t)index) != 0) {
    return 0;
  }
  (void)pthread_detach(thread);
  return 1;
}

static void mettle_parallel_yield(void) {
  (void)mettle_parallel_syscall(24L, 0L, 0L, 0L);
}

static void mettle_parallel_nap(void) {
  struct mettle_parallel_timespec request;
  request.seconds = 0;
  request.nanoseconds = 1000000;
  (void)mettle_parallel_syscall(35L, (long)(size_t)&request, 0L, 0L);
}

#endif

static void mettle_parallel_pause(void) {
#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64)
  __builtin_ia32_pause();
#elif defined(__aarch64__)
  __asm__ __volatile__("yield");
#endif
}

static void mettle_parallel_backoff(unsigned *spins) {
  unsigned count = *spins;
  if (count < 4096u) {
    mettle_parallel_pause();
  } else if (count < 8192u) {
    mettle_parallel_yield();
  } else {
    mettle_parallel_nap();
  }
  *spins = count + 1u;
}

static void mettle_parallel_run_chunk(unsigned slot) {
  long long start = g_job.lo + g_job.span * (long long)slot;
  long long end = start + g_job.span;
  if (end > g_job.hi) {
    end = g_job.hi;
  }
  if (start >= end) {
    return;
  }
  if (g_job.slot_body) {
    g_job.slot_body(g_job.ctx, start, end, (long long)slot);
  } else {
    g_job.body(g_job.ctx, start, end);
  }
}

static void mettle_parallel_worker(long index) {
  unsigned seen = 0;
  for (;;) {
    unsigned spins = 0;
    unsigned generation;
    while ((generation = __atomic_load_n(&g_generation, __ATOMIC_ACQUIRE)) ==
           seen) {
      mettle_parallel_backoff(&spins);
    }
    seen = generation;
    if ((unsigned)index + 1u < g_job.wanted) {
      mettle_parallel_run_chunk((unsigned)index);
    }
    __atomic_sub_fetch(&g_remaining, 1, __ATOMIC_ACQ_REL);
  }
}

static long mettle_parallel_parse_count(const char *text) {
  long value = 0;
  if (!text) {
    return 0;
  }
  while (*text == ' ' || *text == '\t') {
    text++;
  }
  if (*text < '0' || *text > '9') {
    return 0;
  }
  while (*text >= '0' && *text <= '9') {
    value = value * 10 + (*text - '0');
    if (value > 4096) {
      return 4096;
    }
    text++;
  }
  return value;
}

static unsigned mettle_parallel_threads(void) {
  static int cached = -1;
  if (cached < 0) {
    const char *spec = getenv("METTLE_PARALLEL_THREADS");
    long requested = mettle_parallel_parse_count(spec);
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

static int mettle_parallel_pool_ready(unsigned wanted) {
  int state = __atomic_load_n(&g_pool_state, __ATOMIC_ACQUIRE);
  unsigned index;

  if (state == 2) {
    return (int)(g_pool_workers + 1u >= wanted);
  }
  if (state != 0) {
    return 0;
  }
  if (!__atomic_compare_exchange_n(&g_pool_state, &state, 1, 0,
                                   __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
    return 0;
  }

  for (index = 1u; index < mettle_parallel_threads(); index++) {
    if (!mettle_parallel_spawn((long)(index - 1u))) {
      break;
    }
    g_pool_workers = index;
  }
  __atomic_store_n(&g_pool_state, 2, __ATOMIC_RELEASE);
  return (int)(g_pool_workers + 1u >= wanted);
}

static void mettle_parallel_dispatch(MettleParallelBody body,
                                     MettleParallelSlotBody slot_body,
                                     void *ctx, long long lo, long long hi,
                                     long long min_chunk) {
  long long total = hi - lo;
  unsigned wanted;
  int expected = 0;

  if ((!body && !slot_body) || total <= 0) {
    return;
  }
  if (min_chunk < 1) {
    min_chunk = 1;
  }
  wanted = mettle_parallel_threads();
  if (wanted > METTLE_PARALLEL_MAX_THREADS) {
    wanted = METTLE_PARALLEL_MAX_THREADS;
  }
  if ((long long)wanted > total / min_chunk) {
    wanted = (unsigned)(total / min_chunk);
  }
  if (wanted < 2u) {
    if (slot_body) {
      slot_body(ctx, lo, hi, 0);
    } else {
      body(ctx, lo, hi);
    }
    return;
  }
  if (!__atomic_compare_exchange_n(&g_reentered, &expected, 1, 0,
                                   __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
    if (slot_body) {
      slot_body(ctx, lo, hi, 0);
    } else {
      body(ctx, lo, hi);
    }
    return;
  }
  if (!mettle_parallel_pool_ready(wanted)) {
    if (g_pool_workers + 1u < wanted) {
      wanted = g_pool_workers + 1u;
    }
    if (wanted < 2u) {
      __atomic_store_n(&g_reentered, 0, __ATOMIC_RELEASE);
      if (slot_body) {
        slot_body(ctx, lo, hi, 0);
      } else {
        body(ctx, lo, hi);
      }
      return;
    }
  }

  g_job.body = body;
  g_job.slot_body = slot_body;
  g_job.ctx = ctx;
  g_job.lo = lo;
  g_job.hi = hi;
  g_job.span = (total + (long long)wanted - 1) / (long long)wanted;
  g_job.wanted = wanted;

  __atomic_store_n(&g_remaining, (int)g_pool_workers, __ATOMIC_RELEASE);
  __atomic_add_fetch(&g_generation, 1u, __ATOMIC_ACQ_REL);

  mettle_parallel_run_chunk(wanted - 1u);

  {
    unsigned spins = 0;
    while (__atomic_load_n(&g_remaining, __ATOMIC_ACQUIRE) != 0) {
      mettle_parallel_backoff(&spins);
    }
  }
  __atomic_store_n(&g_reentered, 0, __ATOMIC_RELEASE);
}

void mettle_parallel_range(MettleParallelBody body, void *ctx, long long lo,
                           long long hi, long long min_chunk) {
  mettle_parallel_dispatch(body, NULL, ctx, lo, hi, min_chunk);
}

void mettle_parallel_range_slots(MettleParallelSlotBody body, void *ctx,
                                 long long lo, long long hi,
                                 long long min_chunk) {
  mettle_parallel_dispatch(NULL, body, ctx, lo, hi, min_chunk);
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
