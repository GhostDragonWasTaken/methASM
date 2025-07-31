#include "register_allocator.h"
#include <stdlib.h>
#include <string.h>

RegisterAllocator* register_allocator_create(void) {
    RegisterAllocator* allocator = malloc(sizeof(RegisterAllocator));
    if (!allocator) return NULL;
    
    allocator->allocations = NULL;
    allocator->allocation_count = 0;
    allocator->allocation_capacity = 0;
    
    // Initialize register usage tracking
    memset(allocator->register_usage, 0, sizeof(allocator->register_usage));
    
    return allocator;
}

void register_allocator_destroy(RegisterAllocator* allocator) {
    if (allocator) {
        free(allocator->allocations);
        free(allocator);
    }
}

int register_allocator_allocate_function(RegisterAllocator* allocator, ASTNode* function) {
    // Stub implementation - always succeeds for now
    (void)allocator; // Suppress unused parameter warning
    (void)function;  // Suppress unused parameter warning
    return 1;
}

x86Register register_allocator_get_register(RegisterAllocator* allocator, const char* variable) {
    // Stub implementation
    (void)allocator; // Suppress unused parameter warning
    (void)variable;  // Suppress unused parameter warning
    return REG_RAX;
}

int register_allocator_get_memory_offset(RegisterAllocator* allocator, const char* variable) {
    // Stub implementation
    (void)allocator; // Suppress unused parameter warning
    (void)variable;  // Suppress unused parameter warning
    return -8; // Default stack offset
}

void register_allocator_spill_variable(RegisterAllocator* allocator, const char* variable) {
    // Stub implementation
    (void)allocator; // Suppress unused parameter warning
    (void)variable;  // Suppress unused parameter warning
}