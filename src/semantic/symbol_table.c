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

void symbol_table_destroy(SymbolTable* table) {
    if (!table) return;
    
    // TODO: Properly free all scopes and symbols
    free(table->global_scope);
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
    
    // TODO: Properly free the old scope
    free(old_scope);
}

int symbol_table_declare(SymbolTable* table, Symbol* symbol) {
    // Stub implementation
    return 1;
}

Symbol* symbol_table_lookup(SymbolTable* table, const char* name) {
    // Stub implementation
    return NULL;
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
        free(type->name);
        free(type);
    }
}