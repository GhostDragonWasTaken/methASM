#include "gc.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct GCAllocation {
  size_t size;
  int marked;
  struct GCAllocation *next;
  // Payload follows immediately after this struct
} GCAllocation;

typedef struct GCRoot {
  void **slot;
  struct GCRoot *next;
} GCRoot;

typedef struct GCMarkStack {
  GCAllocation **items;
  size_t count;
  size_t capacity;
} GCMarkStack;

static void *g_stack_base = NULL;
static GCAllocation *g_allocations = NULL;
static GCRoot *g_roots = NULL;
static size_t g_allocation_count = 0;
static size_t g_allocated_bytes = 0;
static uintptr_t g_heap_min = UINTPTR_MAX;
static uintptr_t g_heap_max = 0;
static size_t g_collection_threshold = 1024 * 1024; // 1 MiB default
static const size_t GC_MIN_COLLECTION_THRESHOLD = 4096;
static int g_is_collecting = 0;

static void gc_mark_stack_destroy(GCMarkStack *stack) {
  if (!stack) {
    return;
  }
  free(stack->items);
  stack->items = NULL;
  stack->count = 0;
  stack->capacity = 0;
}

static int gc_mark_stack_push(GCMarkStack *stack, GCAllocation *allocation) {
  if (!stack || !allocation) {
    return 0;
  }
  if (stack->count == stack->capacity) {
    size_t new_capacity = (stack->capacity == 0) ? 64 : stack->capacity * 2;
    GCAllocation **new_items = (GCAllocation **)realloc(
        stack->items, new_capacity * sizeof(GCAllocation *));
    if (!new_items) {
      return 0;
    }
    stack->items = new_items;
    stack->capacity = new_capacity;
  }

  stack->items[stack->count++] = allocation;
  return 1;
}

static GCAllocation *gc_mark_stack_pop(GCMarkStack *stack) {
  if (!stack || stack->count == 0) {
    return NULL;
  }
  return stack->items[--stack->count];
}

static GCAllocation *gc_find_allocation_containing(uintptr_t value) {
  GCAllocation *current = g_allocations;
  while (current) {
    uintptr_t payload_start = (uintptr_t)(current + 1);
    uintptr_t payload_end = payload_start + current->size;
    if (value >= payload_start && value < payload_end) {
      return current;
    }
    current = current->next;
  }
  return NULL;
}

static void gc_recompute_heap_bounds(void) {
  uintptr_t min_addr = UINTPTR_MAX;
  uintptr_t max_addr = 0;
  GCAllocation *current = g_allocations;

  while (current) {
    uintptr_t start = (uintptr_t)(current + 1);
    uintptr_t end = start + current->size;
    if (start < min_addr) {
      min_addr = start;
    }
    if (end > max_addr) {
      max_addr = end;
    }
    current = current->next;
  }

  g_heap_min = min_addr;
  g_heap_max = max_addr;
}

static void gc_maybe_collect_before_alloc(void) {
  if (!g_stack_base) {
    return;
  }
  if (g_is_collecting) {
    return;
  }
  if (g_allocated_bytes < g_collection_threshold) {
    return;
  }

  uintptr_t sp_marker = 0;
  gc_collect((void *)&sp_marker);

  // Adapt threshold to reduce repeated collections when heap is still large.
  size_t next_threshold = g_allocated_bytes * 2;
  if (next_threshold < GC_MIN_COLLECTION_THRESHOLD) {
    next_threshold = GC_MIN_COLLECTION_THRESHOLD;
  }
  g_collection_threshold = next_threshold;
}

void gc_register_root(void **root_slot) {
  if (!root_slot) {
    return;
  }

  GCRoot *current = g_roots;
  while (current) {
    if (current->slot == root_slot) {
      return; // Already registered
    }
    current = current->next;
  }

  GCRoot *root = (GCRoot *)malloc(sizeof(GCRoot));
  if (!root) {
    fprintf(stderr, "Fatal error: Out of memory during gc_register_root\n");
    exit(1);
  }

  root->slot = root_slot;
  root->next = g_roots;
  g_roots = root;
}

void gc_unregister_root(void **root_slot) {
  if (!root_slot) {
    return;
  }

  GCRoot **prev = &g_roots;
  GCRoot *current = g_roots;
  while (current) {
    if (current->slot == root_slot) {
      *prev = current->next;
      free(current);
      return;
    }
    prev = &current->next;
    current = current->next;
  }
}

void gc_init(void *stack_base) {
  g_stack_base = stack_base;
  if (g_collection_threshold < GC_MIN_COLLECTION_THRESHOLD) {
    g_collection_threshold = GC_MIN_COLLECTION_THRESHOLD;
  }
}

void *gc_alloc(size_t size) {
  if (size == 0)
    return NULL;

  gc_maybe_collect_before_alloc();

  size_t total_size = sizeof(GCAllocation) + size;
  GCAllocation *alloc = (GCAllocation *)malloc(total_size);
  if (!alloc) {
    fprintf(stderr, "Fatal error: Out of memory during gc_alloc\n");
    exit(1);
  }

  alloc->size = size;
  alloc->marked = 0;

  // Prepend to tracking list
  alloc->next = g_allocations;
  g_allocations = alloc;

  g_allocation_count++;
  g_allocated_bytes += size;

  uintptr_t payload_start = (uintptr_t)(alloc + 1);
  uintptr_t payload_end = payload_start + size;
  if (payload_start < g_heap_min) {
    g_heap_min = payload_start;
  }
  if (payload_end > g_heap_max) {
    g_heap_max = payload_end;
  }

  // Managed allocations start zeroed so uninitialized bytes do not retain
  // random objects during conservative scanning.
  memset((void *)(alloc + 1), 0, size);

  // The caller gets the payload memory, immediately after the header
  return (void *)(alloc + 1);
}

// Mark a single pointer if it falls within a known allocation block.
static void mark_pointer(void *ptr, GCMarkStack *stack) {
  if (!ptr)
    return;

  uintptr_t value = (uintptr_t)ptr;
  if (g_heap_min == UINTPTR_MAX || value < g_heap_min || value >= g_heap_max) {
    return;
  }

  GCAllocation *allocation = gc_find_allocation_containing(value);
  if (!allocation || allocation->marked) {
    return;
  }

  allocation->marked = 1;
  if (!gc_mark_stack_push(stack, allocation)) {
    fprintf(stderr, "Fatal error: Out of memory during GC mark phase\n");
    exit(1);
  }
}

void gc_collect(void *current_rsp) {
  if (!g_stack_base || !current_rsp)
    return;
  if (g_is_collecting)
    return;

  g_is_collecting = 1;

  // Ensure stack direction is correct (stack grows downwards on x86-64)
  uintptr_t stack_start = (uintptr_t)current_rsp;
  uintptr_t stack_end = (uintptr_t)g_stack_base;

  if (stack_start > stack_end) {
    uintptr_t temp = stack_start;
    stack_start = stack_end;
    stack_end = temp;
  }

  // 1. CLEAR MARKS
  GCAllocation *current = g_allocations;
  while (current) {
    current->marked = 0;
    current = current->next;
  }

  GCMarkStack mark_stack = {0};

  // 2. MARK PHASE (Conservative stack scanning)
  // Mark registered explicit roots first (globals, external runtime roots).
  GCRoot *root = g_roots;
  while (root) {
    if (root->slot) {
      mark_pointer(*root->slot, &mark_stack);
    }
    root = root->next;
  }

  // Conservative stack scanning.
  // Scan every aligned pointer-sized word on the stack
  uintptr_t aligned_start =
      (stack_start + (sizeof(void *) - 1)) & ~(uintptr_t)(sizeof(void *) - 1);
  void **scan_ptr = (void **)aligned_start;
  while ((uintptr_t)scan_ptr < stack_end) {
    mark_pointer(*scan_ptr, &mark_stack);
    scan_ptr++;
  }

  // Traverse object graph from marked roots.
  GCAllocation *marked_allocation = NULL;
  while ((marked_allocation = gc_mark_stack_pop(&mark_stack)) != NULL) {
    void **payload_ptrs = (void **)((void *)(marked_allocation + 1));
    size_t pointer_slots = marked_allocation->size / sizeof(void *);
    for (size_t i = 0; i < pointer_slots; i++) {
      mark_pointer(payload_ptrs[i], &mark_stack);
    }
  }

  gc_mark_stack_destroy(&mark_stack);

  // 3. SWEEP PHASE
  GCAllocation **prev_ptr = &g_allocations;
  current = g_allocations;

  while (current) {
    if (!current->marked) {
      GCAllocation *unreached = current;
      current = current->next;

      *prev_ptr = current; // Remove from list

      g_allocation_count--;
      g_allocated_bytes -= unreached->size;

      free(unreached);
    } else {
      prev_ptr = &current->next;
      current = current->next;
    }
  }

  gc_recompute_heap_bounds();
  g_is_collecting = 0;
}

void gc_collect_now(void) {
  uintptr_t sp_marker = 0;
  gc_collect((void *)&sp_marker);
}

void gc_set_collection_threshold(size_t bytes) {
  if (bytes < GC_MIN_COLLECTION_THRESHOLD) {
    g_collection_threshold = GC_MIN_COLLECTION_THRESHOLD;
  } else {
    g_collection_threshold = bytes;
  }
}

size_t gc_get_collection_threshold(void) { return g_collection_threshold; }

size_t gc_get_allocation_count(void) { return g_allocation_count; }

size_t gc_get_allocated_bytes(void) { return g_allocated_bytes; }

void gc_shutdown(void) {
  GCAllocation *current = g_allocations;
  while (current) {
    GCAllocation *next = current->next;
    free(current);
    current = next;
  }

  g_allocations = NULL;

  GCRoot *root = g_roots;
  while (root) {
    GCRoot *next = root->next;
    free(root);
    root = next;
  }
  g_roots = NULL;

  g_allocation_count = 0;
  g_allocated_bytes = 0;
  g_heap_min = UINTPTR_MAX;
  g_heap_max = 0;
  g_is_collecting = 0;
  g_stack_base = NULL;
  g_collection_threshold = 1024 * 1024;
}
