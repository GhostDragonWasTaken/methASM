#define _GNU_SOURCE
#include "symbol_table.h"
#include <stdlib.h>
#include <string.h>

SymbolTable* symbol_table_create(void) {
    SymbolTable* table = malloc(sizeof(SymbolTable));
    if (!table) return NULL;
    
    table->global_scope = malloc(sizeof(Scope));
    if (!table->global_scope) {
        free(table);
        return NULL;
    }
    
    table->global_scope->type = SCOPE_GLOBAL;
    table->global_scope->parent = NULL;
    table->global_scope->symbols = NULL;
    table->global_scope->symbol_count = 0;
    table->global_scope->symbol_capacity = 0;
    
    table->current_scope = table->global_scope;
    
    return table;
}

static void scope_destroy(Scope* scope) {
    if (!scope) return;
    
    // Free all symbols in this scope
    for (size_t i = 0; i < scope->symbol_count; i++) {
        Symbol* symbol = scope->symbols[i];
        if (symbol) {
            free(symbol->name);
            if (symbol->kind == SYMBOL_FUNCTION) {
                // Free function parameter names and types
                for (size_t j = 0; j < symbol->data.function.parameter_count; j++) {
                    free(symbol->data.function.parameter_names[j]);
                    type_destroy(symbol->data.function.parameter_types[j]);
                }
                free(symbol->data.function.parameter_names);
                free(symbol->data.function.parameter_types);
                type_destroy(symbol->data.function.return_type);
            }
            type_destroy(symbol->type);
            free(symbol);
        }
    }
    free(scope->symbols);
    free(scope);
}

void symbol_table_destroy(SymbolTable* table) {
    if (!table) return;
    
    // Free all scopes starting from current and going up to global
    Scope* current = table->current_scope;
    while (current && current != table->global_scope) {
        Scope* parent = current->parent;
        scope_destroy(current);
        current = parent;
    }
    
    // Free global scope
    scope_destroy(table->global_scope);
    free(table);
}

void symbol_table_enter_scope(SymbolTable* table, ScopeType type) {
    if (!table) return;
    
    Scope* new_scope = malloc(sizeof(Scope));
    if (!new_scope) return;
    
    new_scope->type = type;
    new_scope->parent = table->current_scope;
    new_scope->symbols = NULL;
    new_scope->symbol_count = 0;
    new_scope->symbol_capacity = 0;
    
    table->current_scope = new_scope;
}

void symbol_table_exit_scope(SymbolTable* table) {
    if (!table || !table->current_scope || table->current_scope == table->global_scope) {
        return;
    }
    
    Scope* old_scope = table->current_scope;
    table->current_scope = old_scope->parent;
    
    // Properly free the old scope
    scope_destroy(old_scope);
}

int symbol_table_declare(SymbolTable* table, Symbol* symbol) {
    if (!table || !symbol || !table->current_scope) {
        return 0; // Failure
    }
    
    // Validate the declaration
    if (!symbol_table_validate_declaration(table, symbol)) {
        return 0; // Invalid declaration
    }
    
    // Check for duplicate declaration in current scope only
    for (size_t i = 0; i < table->current_scope->symbol_count; i++) {
        if (table->current_scope->symbols[i] && 
            strcmp(table->current_scope->symbols[i]->name, symbol->name) == 0) {
            // Allow forward declaration resolution for functions
            if (symbol->kind == SYMBOL_FUNCTION && 
                table->current_scope->symbols[i]->is_forward_declaration) {
                // Resolve the forward declaration
                table->current_scope->symbols[i]->is_forward_declaration = 0;
                table->current_scope->symbols[i]->is_initialized = 1;
                return 1; // Successfully resolved forward declaration
            }
            return 0; // Duplicate declaration
        }
    }
    
    // Resize symbols array if needed
    if (table->current_scope->symbol_count >= table->current_scope->symbol_capacity) {
        size_t new_capacity = table->current_scope->symbol_capacity == 0 ? 8 : table->current_scope->symbol_capacity * 2;
        Symbol** new_symbols = realloc(table->current_scope->symbols, new_capacity * sizeof(Symbol*));
        if (!new_symbols) {
            return 0; // Memory allocation failure
        }
        table->current_scope->symbols = new_symbols;
        table->current_scope->symbol_capacity = new_capacity;
    }
    
    // Set the symbol's scope
    symbol->scope = table->current_scope;
    
    // Add symbol to current scope
    table->current_scope->symbols[table->current_scope->symbol_count] = symbol;
    table->current_scope->symbol_count++;
    
    return 1; // Success
}

Symbol* symbol_table_lookup(SymbolTable* table, const char* name) {
    if (!table || !name) {
        return NULL;
    }
    
    // Search from current scope up to global scope
    Scope* current_scope = table->current_scope;
    while (current_scope) {
        // Search in current scope
        for (size_t i = 0; i < current_scope->symbol_count; i++) {
            if (current_scope->symbols[i] && 
                strcmp(current_scope->symbols[i]->name, name) == 0) {
                return current_scope->symbols[i];
            }
        }
        // Move to parent scope
        current_scope = current_scope->parent;
    }
    
    return NULL; // Symbol not found
}

Type* type_create(TypeKind kind, const char* name) {
    Type* type = malloc(sizeof(Type));
    if (!type) return NULL;
    
    type->kind = kind;
    type->name = name ? strdup(name) : NULL;
    type->size = 0;
    type->alignment = 0;
    type->base_type = NULL;
    type->array_size = 0;
    
    // Initialize struct-specific fields
    type->field_names = NULL;
    type->field_types = NULL;
    type->field_offsets = NULL;
    type->field_count = 0;
    
    // Set default sizes
    switch (kind) {
        case TYPE_INT8:
        case TYPE_UINT8:
            type->size = 1;
            type->alignment = 1;
            break;
        case TYPE_INT16:
        case TYPE_UINT16:
            type->size = 2;
            type->alignment = 2;
            break;
        case TYPE_INT32:
        case TYPE_UINT32:
        case TYPE_FLOAT32:
            type->size = 4;
            type->alignment = 4;
            break;
        case TYPE_INT64:
        case TYPE_UINT64:
        case TYPE_FLOAT64:
            type->size = 8;
            type->alignment = 8;
            break;
        default:
            break;
    }
    
    return type;
}

void type_destroy(Type* type) {
    if (type) {
        // Clean up struct-specific fields
        if (type->field_names) {
            for (size_t i = 0; i < type->field_count; i++) {
                free(type->field_names[i]);
            }
            free(type->field_names);
        }
        
        if (type->field_types) {
            // Note: Don't destroy field types as they might be shared/referenced elsewhere
            free(type->field_types);
        }
        
        if (type->field_offsets) {
            free(type->field_offsets);
        }
        
        free(type->name);
        free(type);
    }
}

Scope* symbol_table_get_current_scope(SymbolTable* table) {
    if (!table) return NULL;
    return table->current_scope;
}

Symbol* symbol_create(const char* name, SymbolKind kind, Type* type) {
    if (!name) return NULL;
    
    Symbol* symbol = malloc(sizeof(Symbol));
    if (!symbol) return NULL;
    
    symbol->name = strdup(name);
    if (!symbol->name) {
        free(symbol);
        return NULL;
    }
    
    symbol->kind = kind;
    symbol->type = type;
    symbol->scope = NULL; // Will be set when declared
    symbol->is_initialized = 0;
    symbol->is_forward_declaration = 0;
    
    // Initialize union data based on symbol kind
    switch (kind) {
        case SYMBOL_VARIABLE:
        case SYMBOL_PARAMETER:
            symbol->data.variable.register_id = -1;
            symbol->data.variable.memory_offset = 0;
            symbol->data.variable.is_in_register = 0;
            break;
        case SYMBOL_FUNCTION:
            symbol->data.function.parameter_names = NULL;
            symbol->data.function.parameter_types = NULL;
            symbol->data.function.parameter_count = 0;
            symbol->data.function.return_type = NULL;
            break;
        case SYMBOL_STRUCT:
            // No specific data for struct symbols
            break;
    }
    
    return symbol;
}

void symbol_destroy(Symbol* symbol) {
    if (!symbol) return;
    
    free(symbol->name);
    
    if (symbol->kind == SYMBOL_FUNCTION) {
        // Free function parameter names and types
        for (size_t i = 0; i < symbol->data.function.parameter_count; i++) {
            free(symbol->data.function.parameter_names[i]);
            type_destroy(symbol->data.function.parameter_types[i]);
        }
        free(symbol->data.function.parameter_names);
        free(symbol->data.function.parameter_types);
        type_destroy(symbol->data.function.return_type);
    }
    
    type_destroy(symbol->type);
    free(symbol);
}

Symbol* symbol_table_lookup_current_scope(SymbolTable* table, const char* name) {
    if (!table || !name || !table->current_scope) {
        return NULL;
    }
    
    // Search only in current scope
    for (size_t i = 0; i < table->current_scope->symbol_count; i++) {
        if (table->current_scope->symbols[i] && 
            strcmp(table->current_scope->symbols[i]->name, name) == 0) {
            return table->current_scope->symbols[i];
        }
    }
    
    return NULL; // Symbol not found in current scope
}

int symbol_table_declare_forward(SymbolTable* table, Symbol* symbol) {
    if (!table || !symbol || !table->current_scope) {
        return 0; // Failure
    }
    
    // Only functions can be forward declared
    if (symbol->kind != SYMBOL_FUNCTION) {
        return 0; // Only functions can be forward declared
    }
    
    // Check if symbol already exists in current scope
    Symbol* existing = symbol_table_lookup_current_scope(table, symbol->name);
    if (existing) {
        // If it's already a forward declaration, check compatibility
        if (existing->is_forward_declaration) {
            // TODO: Check function signature compatibility
            return 1; // Compatible forward declaration
        } else {
            return 0; // Already defined
        }
    }
    
    // Mark as forward declaration
    symbol->is_forward_declaration = 1;
    
    // Declare the symbol
    return symbol_table_declare(table, symbol);
}

int symbol_table_resolve_forward_declaration(SymbolTable* table, Symbol* symbol) {
    if (!table || !symbol || symbol->kind != SYMBOL_FUNCTION) {
        return 0; // Failure
    }
    
    // Look for existing forward declaration
    Symbol* existing = symbol_table_lookup_current_scope(table, symbol->name);
    if (existing && existing->is_forward_declaration) {
        // TODO: Check function signature compatibility
        // For now, just mark the existing symbol as no longer forward
        existing->is_forward_declaration = 0;
        existing->is_initialized = 1;
        return 1; // Successfully resolved
    }
    
    // No forward declaration found, declare normally
    symbol->is_forward_declaration = 0;
    symbol->is_initialized = 1;
    return symbol_table_declare(table, symbol);
}

int symbol_table_validate_declaration(SymbolTable* table, Symbol* symbol) {
    if (!table || !symbol) {
        return 0; // Invalid parameters
    }
    
    // Check if name is valid (not empty)
    if (!symbol->name || strlen(symbol->name) == 0) {
        return 0; // Invalid name
    }
    
    // Check if type is valid
    if (!symbol->type) {
        return 0; // Invalid type
    }
    
    // For functions, validate parameter information
    if (symbol->kind == SYMBOL_FUNCTION) {
        // If parameter count > 0, must have parameter arrays
        if (symbol->data.function.parameter_count > 0) {
            if (!symbol->data.function.parameter_names || 
                !symbol->data.function.parameter_types) {
                return 0; // Invalid function parameters
            }
            
            // Check each parameter
            for (size_t i = 0; i < symbol->data.function.parameter_count; i++) {
                if (!symbol->data.function.parameter_names[i] || 
                    !symbol->data.function.parameter_types[i]) {
                    return 0; // Invalid parameter
                }
            }
        }
    }
    
    // Check for duplicate declaration in current scope
    Symbol* existing = symbol_table_lookup_current_scope(table, symbol->name);
    if (existing) {
        // Allow forward declaration resolution for functions
        if (symbol->kind == SYMBOL_FUNCTION && existing->is_forward_declaration) {
            return 1; // Valid forward declaration resolution
        }
        return 0; // Duplicate declaration
    }
    
    return 1; // Valid declaration
}

// Struct type creation and manipulation functions

Type* type_create_struct(const char* name, char** field_names, Type** field_types, size_t field_count) {
    if (!name || field_count == 0 || !field_names || !field_types) {
        return NULL;
    }
    
    Type* struct_type = type_create(TYPE_STRUCT, name);
    if (!struct_type) {
        return NULL;
    }
    
    struct_type->field_count = field_count;
    
    // Allocate arrays for field information
    struct_type->field_names = malloc(field_count * sizeof(char*));
    struct_type->field_types = malloc(field_count * sizeof(Type*));
    struct_type->field_offsets = malloc(field_count * sizeof(size_t));
    
    if (!struct_type->field_names || !struct_type->field_types || !struct_type->field_offsets) {
        type_destroy(struct_type);
        return NULL;
    }
    
    // Copy field information and calculate offsets
    size_t current_offset = 0;
    size_t max_alignment = 1;
    
    for (size_t i = 0; i < field_count; i++) {
        // Copy field name
        struct_type->field_names[i] = strdup(field_names[i]);
        if (!struct_type->field_names[i]) {
            type_destroy(struct_type);
            return NULL;
        }
        
        // Reference field type (don't duplicate)
        struct_type->field_types[i] = field_types[i];
        
        // Calculate field alignment and offset
        size_t field_alignment = field_types[i]->alignment;
        if (field_alignment > max_alignment) {
            max_alignment = field_alignment;
        }
        
        // Align current offset to field alignment
        size_t padding = (field_alignment - (current_offset % field_alignment)) % field_alignment;
        current_offset += padding;
        
        struct_type->field_offsets[i] = current_offset;
        current_offset += field_types[i]->size;
    }
    
    // Calculate total struct size with final padding
    size_t final_padding = (max_alignment - (current_offset % max_alignment)) % max_alignment;
    struct_type->size = current_offset + final_padding;
    struct_type->alignment = max_alignment;
    
    return struct_type;
}

Type* type_get_field_type(Type* struct_type, const char* field_name) {
    if (!struct_type || struct_type->kind != TYPE_STRUCT || !field_name) {
        return NULL;
    }
    
    for (size_t i = 0; i < struct_type->field_count; i++) {
        if (strcmp(struct_type->field_names[i], field_name) == 0) {
            return struct_type->field_types[i];
        }
    }
    
    return NULL; // Field not found
}

size_t type_get_field_offset(Type* struct_type, const char* field_name) {
    if (!struct_type || struct_type->kind != TYPE_STRUCT || !field_name) {
        return 0;
    }
    
    for (size_t i = 0; i < struct_type->field_count; i++) {
        if (strcmp(struct_type->field_names[i], field_name) == 0) {
            return struct_type->field_offsets[i];
        }
    }
    
    return 0; // Field not found
}

int type_has_field(Type* struct_type, const char* field_name) {
    if (!struct_type || struct_type->kind != TYPE_STRUCT || !field_name) {
        return 0;
    }
    
    for (size_t i = 0; i < struct_type->field_count; i++) {
        if (strcmp(struct_type->field_names[i], field_name) == 0) {
            return 1; // Field found
        }
    }
    
    return 0; // Field not found
}