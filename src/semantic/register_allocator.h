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

// Calling conventions
typedef enum {
    CALLING_CONV_SYSV,    // System V ABI (Linux, Unix)
    CALLING_CONV_MS_X64   // Microsoft x64 (Windows)
} CallingConvention;

// Register classifications for calling conventions
typedef enum {
    REG_CLASS_CALLER_SAVED,  // Caller must save before function calls
    REG_CLASS_CALLEE_SAVED,  // Callee must save if used
    REG_CLASS_PARAMETER,     // Used for parameter passing
    REG_CLASS_RETURN,        // Used for return values
    REG_CLASS_RESERVED       // Reserved (RSP, RBP, etc.)
} RegisterClass;

// Calling convention specification
typedef struct {
    CallingConvention convention;
    
    // Parameter passing registers
    x86Register* int_param_registers;
    size_t int_param_count;
    x86Register* float_param_registers; 
    size_t float_param_count;
    
    // Return value registers
    x86Register int_return_register;
    x86Register float_return_register;
    
    // Caller-saved registers (volatile)
    x86Register* caller_saved_registers;
    size_t caller_saved_count;
    
    // Callee-saved registers (non-volatile)
    x86Register* callee_saved_registers;
    size_t callee_saved_count;
    
    // Stack alignment requirement
    int stack_alignment;
    
    // Shadow space requirement (Windows x64)
    int shadow_space_size;
    
} CallingConventionSpec;

// Enhanced live interval for linear scan algorithm
typedef struct UsePosition {
    int position;
    int is_def; // 1 if definition, 0 if use
    struct UsePosition* next;
} UsePosition;

typedef struct LiveInterval {
    char* variable_name;
    int start_position;
    int end_position;
    int loop_depth;        // Nesting level for spill cost calculation
    float spill_cost;      // Weighted spill cost
    Type* type;            // Reference to variable type
    UsePosition* use_positions; // Linked list of use positions
    x86Register assigned_register; // REG_NONE if not assigned
    int is_split;          // 1 if this interval was split
    struct LiveInterval* parent; // Parent interval if split
} LiveInterval;

// Active interval list for linear scan
typedef struct ActiveInterval {
    LiveInterval* interval;
    struct ActiveInterval* next;
} ActiveInterval;

// Legacy typedef for compatibility
typedef LiveInterval LiveRange;

typedef struct {
    char* variable_name;
    x86Register register_id;
    int memory_offset;
    int spill_location;
    int is_in_register;
    LiveInterval* live_interval;
} RegisterAllocation;

typedef struct {
    RegisterAllocation* allocations;
    size_t allocation_count;
    size_t allocation_capacity;
    
    LiveInterval* intervals;
    size_t interval_count;
    size_t interval_capacity;
    
    ActiveInterval* active_intervals; // Active list for linear scan
    
    int register_usage[64]; // Track which registers are in use
    int current_position;   // Position counter for live range analysis
    int current_loop_depth; // Current loop nesting depth
    int stack_offset;       // Current stack offset for spilled variables
    
    // Linear scan working data
    LiveInterval** sorted_intervals; // Sorted by start position
    size_t unhandled_count;         // Number of unhandled intervals
    
    // Calling convention support
    CallingConventionSpec* calling_convention;
    int* saved_registers;           // Registers that need to be saved/restored
    size_t saved_register_count;
    int function_call_in_progress;  // Flag for register preservation
} RegisterAllocator;

// Function declarations
RegisterAllocator* register_allocator_create(void);
void register_allocator_destroy(RegisterAllocator* allocator);

// Core allocation functions
int register_allocator_allocate_function(RegisterAllocator* allocator, ASTNode* function, SymbolTable* symbol_table);
x86Register register_allocator_get_register(RegisterAllocator* allocator, const char* variable);
int register_allocator_get_memory_offset(RegisterAllocator* allocator, const char* variable);

// Live interval analysis (enhanced)
void register_allocator_analyze_live_intervals(RegisterAllocator* allocator, ASTNode* node, SymbolTable* symbol_table);
LiveInterval* register_allocator_find_interval(RegisterAllocator* allocator, const char* variable);
void register_allocator_add_interval(RegisterAllocator* allocator, const char* variable, Type* type);
void register_allocator_add_use_position(LiveInterval* interval, int position, int is_def);
void register_allocator_extend_interval(RegisterAllocator* allocator, const char* variable);

// Linear scan register allocation
int register_allocator_linear_scan(RegisterAllocator* allocator, SymbolTable* symbol_table);
void register_allocator_sort_intervals(RegisterAllocator* allocator);
void register_allocator_expire_old_intervals(RegisterAllocator* allocator, int current_position);
x86Register register_allocator_try_allocate_free_register(RegisterAllocator* allocator, LiveInterval* interval);
void register_allocator_allocate_blocked_register(RegisterAllocator* allocator, LiveInterval* interval, SymbolTable* symbol_table);

// Active interval list management
void register_allocator_add_to_active(RegisterAllocator* allocator, LiveInterval* interval);
void register_allocator_remove_from_active(RegisterAllocator* allocator, LiveInterval* interval);
LiveInterval* register_allocator_find_spill_candidate(RegisterAllocator* allocator, Type* type);

// Interval splitting and spilling
LiveInterval* register_allocator_split_interval(LiveInterval* interval, int position);
void register_allocator_spill_interval(RegisterAllocator* allocator, LiveInterval* interval, SymbolTable* symbol_table);
float register_allocator_calculate_spill_cost(LiveInterval* interval);

// Legacy compatibility functions  
void register_allocator_analyze_live_ranges(RegisterAllocator* allocator, ASTNode* node, SymbolTable* symbol_table);
LiveRange* register_allocator_find_live_range(RegisterAllocator* allocator, const char* variable);
void register_allocator_add_live_range(RegisterAllocator* allocator, const char* variable, Type* type);
void register_allocator_extend_live_range(RegisterAllocator* allocator, const char* variable);
int register_allocator_assign_registers(RegisterAllocator* allocator, SymbolTable* symbol_table);
int register_allocator_has_conflict(RegisterAllocator* allocator, LiveRange* range1, LiveRange* range2);
x86Register register_allocator_find_free_register(RegisterAllocator* allocator, Type* type, int position);
void register_allocator_spill_variable(RegisterAllocator* allocator, const char* variable, SymbolTable* symbol_table);
LiveRange* register_allocator_select_spill_candidate(RegisterAllocator* allocator, Type* type);

// Calling convention management
void register_allocator_set_calling_convention(RegisterAllocator* allocator, CallingConvention convention);
CallingConventionSpec* register_allocator_get_calling_convention_spec(CallingConvention convention);
RegisterClass register_allocator_get_register_class(CallingConventionSpec* spec, x86Register reg);

// Function call support
void register_allocator_prepare_function_call(RegisterAllocator* allocator, const char* function_name);
void register_allocator_complete_function_call(RegisterAllocator* allocator);
x86Register register_allocator_get_parameter_register(RegisterAllocator* allocator, int param_index, Type* param_type);
x86Register register_allocator_get_return_register(RegisterAllocator* allocator, Type* return_type);

// Register preservation
void register_allocator_save_caller_saved_registers(RegisterAllocator* allocator);
void register_allocator_restore_caller_saved_registers(RegisterAllocator* allocator);
void register_allocator_mark_callee_saved_registers(RegisterAllocator* allocator);

// Utility functions
int register_allocator_is_floating_point_type(Type* type);
const char* register_allocator_register_name(x86Register reg);
int register_allocator_is_caller_saved(CallingConventionSpec* spec, x86Register reg);
int register_allocator_is_callee_saved(CallingConventionSpec* spec, x86Register reg);

// Helper functions (internal)
int register_allocator_add_allocation(RegisterAllocator* allocator, const char* variable_name, 
                                    x86Register register_id, int memory_offset, int is_in_register, LiveInterval* live_interval);
void register_allocator_destroy_use_positions(UsePosition* use_pos);
void register_allocator_destroy_intervals(RegisterAllocator* allocator);
void register_allocator_destroy_calling_convention_spec(CallingConventionSpec* spec);

#endif // REGISTER_ALLOCATOR_H