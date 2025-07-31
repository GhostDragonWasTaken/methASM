#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <stddef.h>

typedef enum {
    TYPE_INT8,
    TYPE_INT16,
    TYPE_INT32,
    TYPE_INT64,
    TYPE_UINT8,
    TYPE_UINT16,
    TYPE_UINT32,
    TYPE_UINT64,
    TYPE_FLOAT32,
    TYPE_FLOAT64,
    TYPE_STRING,
    TYPE_POINTER,
    TYPE_ARRAY,
    TYPE_STRUCT,
    TYPE_VOID
} TypeKind;

typedef struct Type {
    TypeKind kind;
    char* name;
    size_t size;
    size_t alignment;
    struct Type* base_type; // For pointers and arrays
    size_t array_size;      // For arrays
} Type;

typedef enum {
    SCOPE_GLOBAL,
    SCOPE_FUNCTION,
    SCOPE_BLOCK
} ScopeType;

typedef struct Scope {
    ScopeType type;
    struct Scope* parent;
    struct Symbol** symbols;
    size_t symbol_count;
    size_t symbol_capacity;
} Scope;

typedef enum {
    SYMBOL_VARIABLE,
    SYMBOL_FUNCTION,
    SYMBOL_STRUCT,
    SYMBOL_PARAMETER
} SymbolKind;

typedef struct Symbol {
    char* name;
    SymbolKind kind;
    Type* type;
    Scope* scope;
    int is_initialized;
    union {
        struct {
            int register_id;
            int memory_offset;
            int is_in_register;
        } variable;
        struct {
            char** parameter_names;
            Type** parameter_types;
            size_t parameter_count;
            Type* return_type;
        } function;
    } data;
} Symbol;

typedef struct {
    Scope* current_scope;
    Scope* global_scope;
} SymbolTable;

// Function declarations
SymbolTable* symbol_table_create(void);
void symbol_table_destroy(SymbolTable* table);
void symbol_table_enter_scope(SymbolTable* table, ScopeType type);
void symbol_table_exit_scope(SymbolTable* table);
int symbol_table_declare(SymbolTable* table, Symbol* symbol);
Symbol* symbol_table_lookup(SymbolTable* table, const char* name);
Type* type_create(TypeKind kind, const char* name);
void type_destroy(Type* type);

#endif // SYMBOL_TABLE_H