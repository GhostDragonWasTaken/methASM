#ifndef REGISTER_ALLOCATOR_H
#define REGISTER_ALLOCATOR_H

#include "symbol_table.h"
#include "../parser/ast.h"

typedef enum {
    REG_RAX, REG_RBX, REG_RCX, REG_RDX,
    REG_RSI, REG_RDI, REG_RSP, REG_RBP,
    REG_R8,  REG_R9,  REG_R10, REG_R11,
    REG_R12, REG_R13, REG_R14, REG_R15,
    REG_XMM0, REG_XMM1, REG_XMM2, REG_XMM3,
    REG_XMM4, REG_XMM5, REG_XMM6, REG_XMM7,
    REG_XMM8, REG_XMM9, REG_XMM10, REG_XMM11,
    REG_XMM12, REG_XMM13, REG_XMM14, REG_XMM15,
    REG_NONE
} x86Register;

typedef struct {
    char* variable_name;
    x86Register register_id;
    int memory_offset;
    int spill_location;
    int is_in_register;
} RegisterAllocation;

typedef struct {
    RegisterAllocation* allocations;
    size_t allocation_count;
    size_t allocation_capacity;
    int register_usage[64]; // Track which registers are in use
} RegisterAllocator;

// Function declarations
RegisterAllocator* register_allocator_create(void);
void register_allocator_destroy(RegisterAllocator* allocator);
int register_allocator_allocate_function(RegisterAllocator* allocator, ASTNode* function);
x86Register register_allocator_get_register(RegisterAllocator* allocator, const char* variable);
int register_allocator_get_memory_offset(RegisterAllocator* allocator, const char* variable);
void register_allocator_spill_variable(RegisterAllocator* allocator, const char* variable);

#endif // REGISTER_ALLOCATOR_H