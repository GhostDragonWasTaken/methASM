#include "type_checker.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

TypeChecker* type_checker_create(SymbolTable* symbol_table) {
    TypeChecker* checker = malloc(sizeof(TypeChecker));
    if (!checker) return NULL;
    
    checker->symbol_table = symbol_table;
    checker->has_error = 0;
    checker->error_message = NULL;
    
    // Initialize built-in type pointers to NULL
    checker->builtin_int8 = NULL;
    checker->builtin_int16 = NULL;
    checker->builtin_int32 = NULL;
    checker->builtin_int64 = NULL;
    checker->builtin_uint8 = NULL;
    checker->builtin_uint16 = NULL;
    checker->builtin_uint32 = NULL;
    checker->builtin_uint64 = NULL;
    checker->builtin_float32 = NULL;
    checker->builtin_float64 = NULL;
    checker->builtin_string = NULL;
    checker->builtin_void = NULL;
    
    // Initialize built-in types
    type_checker_init_builtin_types(checker);
    
    return checker;
}

void type_checker_destroy(TypeChecker* checker) {
    if (checker) {
        // Clean up built-in types
        type_destroy(checker->builtin_int8);
        type_destroy(checker->builtin_int16);
        type_destroy(checker->builtin_int32);
        type_destroy(checker->builtin_int64);
        type_destroy(checker->builtin_uint8);
        type_destroy(checker->builtin_uint16);
        type_destroy(checker->builtin_uint32);
        type_destroy(checker->builtin_uint64);
        type_destroy(checker->builtin_float32);
        type_destroy(checker->builtin_float64);
        type_destroy(checker->builtin_string);
        type_destroy(checker->builtin_void);
        
        free(checker->error_message);
        free(checker);
    }
}

int type_checker_check_program(TypeChecker* checker, ASTNode* program) {
    if (!checker || !program || program->type != AST_PROGRAM) {
        return 0;
    }
    
    Program* prog = (Program*)program->data;
    if (!prog) return 0;
    
    // First pass: Process struct declarations to register struct types
    for (size_t i = 0; i < prog->declaration_count; i++) {
        ASTNode* decl = prog->declarations[i];
        if (decl && decl->type == AST_STRUCT_DECLARATION) {
            if (!type_checker_process_struct_declaration(checker, decl)) {
                return 0; // Error in struct declaration
            }
        }
    }
    
    // Second pass: Process other declarations and type check them
    for (size_t i = 0; i < prog->declaration_count; i++) {
        ASTNode* decl = prog->declarations[i];
        if (decl && decl->type != AST_STRUCT_DECLARATION) {
            if (!type_checker_process_declaration(checker, decl)) {
                return 0; // Error in declaration
            }
        }
    }
    
    return 1; // Success
}

Type* type_checker_infer_type(TypeChecker* checker, ASTNode* expression) {
    if (!checker || !expression) return NULL;
    
    switch (expression->type) {
        case AST_NUMBER_LITERAL: {
            NumberLiteral* literal = (NumberLiteral*)expression->data;
            if (literal->is_float) {
                // Floating literals default to float64
                return checker->builtin_float64;
            } else {
                // Integer literals default to int32
                return checker->builtin_int32;
            }
        }
        
        case AST_STRING_LITERAL:
            // String literals are string type
            return checker->builtin_string;
            
        case AST_IDENTIFIER: {
            Identifier* id = (Identifier*)expression->data;
            Symbol* symbol = symbol_table_lookup(checker->symbol_table, id->name);
            return symbol ? symbol->type : NULL;
        }
        
        case AST_BINARY_EXPRESSION: {
            BinaryExpression* binop = (BinaryExpression*)expression->data;
            Type* left_type = type_checker_infer_type(checker, binop->left);
            Type* right_type = type_checker_infer_type(checker, binop->right);
            
            // Apply type promotion rules
            return type_checker_promote_types(checker, left_type, right_type, binop->operator);
        }
        
        case AST_UNARY_EXPRESSION: {
            UnaryExpression* unop = (UnaryExpression*)expression->data;
            return type_checker_infer_type(checker, unop->operand);
        }
        
        case AST_FUNCTION_CALL: {
            CallExpression* call = (CallExpression*)expression->data;
            Symbol* func_symbol = symbol_table_lookup(checker->symbol_table, call->function_name);
            if (func_symbol && func_symbol->kind == SYMBOL_FUNCTION) {
                // Validate function call with proper type checking
                if (!type_checker_validate_function_call(checker, call, func_symbol)) {
                    return NULL; // Error already set by validation function
                }
                return func_symbol->data.function.return_type;
            }
            
            // Check if this might be a method call (struct.method_name syntax)
            // Method calls would typically be parsed as member access of a function
            // For now, we'll handle this as a regular function call
            type_checker_set_error(checker, "Undefined function '%s'", call->function_name);
            return NULL;
        }
        
        case AST_MEMBER_ACCESS: {
            MemberAccess* member = (MemberAccess*)expression->data;
            Type* object_type = type_checker_infer_type(checker, member->object);
            if (object_type && object_type->kind == TYPE_STRUCT) {
                // Look up the field type in the struct
                Type* field_type = type_get_field_type(object_type, member->member);
                if (field_type) {
                    return field_type;
                } else {
                    // Field not found in struct - this is an error
                    checker->has_error = 1;
                    if (checker->error_message) free(checker->error_message);
                    size_t msg_len = strlen(object_type->name) + strlen(member->member) + 50;
                    checker->error_message = malloc(msg_len);
                    snprintf(checker->error_message, msg_len, 
                             "Field '%s' not found in struct '%s'", 
                             member->member, object_type->name);
                    return NULL;
                }
            } else if (object_type) {
                // Trying to access member on non-struct type
                checker->has_error = 1;
                if (checker->error_message) free(checker->error_message);
                size_t msg_len = strlen(object_type->name) + 50;
                checker->error_message = malloc(msg_len);
                snprintf(checker->error_message, msg_len, 
                         "Cannot access field on non-struct type '%s'", 
                         object_type->name);
                return NULL;
            }
            return NULL;
        }
        
        default:
            return NULL;
    }
}

int type_checker_are_compatible(Type* type1, Type* type2) {
    if (!type1 || !type2) return 0;
    
    // Exact type match
    if (type1->kind == type2->kind) {
        return 1;
    }
    
    // Check for implicit numeric conversions
    return type_checker_is_implicitly_convertible(type1, type2) ||
           type_checker_is_implicitly_convertible(type2, type1);
}

// Built-in type system functions implementation

void type_checker_init_builtin_types(TypeChecker* checker) {
    if (!checker) return;
    
    // Create built-in integer types
    checker->builtin_int8 = type_create(TYPE_INT8, "int8");
    checker->builtin_int16 = type_create(TYPE_INT16, "int16");
    checker->builtin_int32 = type_create(TYPE_INT32, "int32");
    checker->builtin_int64 = type_create(TYPE_INT64, "int64");
    
    // Create built-in unsigned integer types
    checker->builtin_uint8 = type_create(TYPE_UINT8, "uint8");
    checker->builtin_uint16 = type_create(TYPE_UINT16, "uint16");
    checker->builtin_uint32 = type_create(TYPE_UINT32, "uint32");
    checker->builtin_uint64 = type_create(TYPE_UINT64, "uint64");
    
    // Create built-in floating-point types
    checker->builtin_float32 = type_create(TYPE_FLOAT32, "float32");
    checker->builtin_float64 = type_create(TYPE_FLOAT64, "float64");
    
    // Create built-in string type
    checker->builtin_string = type_create(TYPE_STRING, "string");
    if (checker->builtin_string) {
        // String is essentially a pointer to char, but we treat it specially
        checker->builtin_string->size = sizeof(void*); // Pointer size
        checker->builtin_string->alignment = sizeof(void*);
    }
    
    // Create built-in void type
    checker->builtin_void = type_create(TYPE_VOID, "void");
    if (checker->builtin_void) {
        checker->builtin_void->size = 0;
        checker->builtin_void->alignment = 1;
    }
}

Type* type_checker_get_builtin_type(TypeChecker* checker, TypeKind kind) {
    if (!checker) return NULL;
    
    switch (kind) {
        case TYPE_INT8:
            return checker->builtin_int8;
        case TYPE_INT16:
            return checker->builtin_int16;
        case TYPE_INT32:
            return checker->builtin_int32;
        case TYPE_INT64:
            return checker->builtin_int64;
        case TYPE_UINT8:
            return checker->builtin_uint8;
        case TYPE_UINT16:
            return checker->builtin_uint16;
        case TYPE_UINT32:
            return checker->builtin_uint32;
        case TYPE_UINT64:
            return checker->builtin_uint64;
        case TYPE_FLOAT32:
            return checker->builtin_float32;
        case TYPE_FLOAT64:
            return checker->builtin_float64;
        case TYPE_STRING:
            return checker->builtin_string;
        case TYPE_VOID:
            return checker->builtin_void;
        default:
            return NULL;
    }
}

Type* type_checker_get_type_by_name(TypeChecker* checker, const char* name) {
    if (!checker || !name) return NULL;
    
    // Check built-in types by name
    if (strcmp(name, "int8") == 0) return checker->builtin_int8;
    if (strcmp(name, "int16") == 0) return checker->builtin_int16;
    if (strcmp(name, "int32") == 0) return checker->builtin_int32;
    if (strcmp(name, "int64") == 0) return checker->builtin_int64;
    if (strcmp(name, "uint8") == 0) return checker->builtin_uint8;
    if (strcmp(name, "uint16") == 0) return checker->builtin_uint16;
    if (strcmp(name, "uint32") == 0) return checker->builtin_uint32;
    if (strcmp(name, "uint64") == 0) return checker->builtin_uint64;
    if (strcmp(name, "float32") == 0) return checker->builtin_float32;
    if (strcmp(name, "float64") == 0) return checker->builtin_float64;
    if (strcmp(name, "string") == 0) return checker->builtin_string;
    if (strcmp(name, "void") == 0) return checker->builtin_void;
    
    // Check for user-defined struct types in symbol table
    Symbol* struct_symbol = symbol_table_lookup(checker->symbol_table, name);
    if (struct_symbol && struct_symbol->kind == SYMBOL_STRUCT) {
        return struct_symbol->type;
    }
    
    return NULL;
}

int type_checker_is_integer_type(Type* type) {
    if (!type) return 0;
    
    switch (type->kind) {
        case TYPE_INT8:
        case TYPE_INT16:
        case TYPE_INT32:
        case TYPE_INT64:
        case TYPE_UINT8:
        case TYPE_UINT16:
        case TYPE_UINT32:
        case TYPE_UINT64:
            return 1;
        default:
            return 0;
    }
}

int type_checker_is_floating_type(Type* type) {
    if (!type) return 0;
    
    switch (type->kind) {
        case TYPE_FLOAT32:
        case TYPE_FLOAT64:
            return 1;
        default:
            return 0;
    }
}

int type_checker_is_numeric_type(Type* type) {
    return type_checker_is_integer_type(type) || type_checker_is_floating_type(type);
}

size_t type_checker_get_type_size(Type* type) {
    if (!type) return 0;
    return type->size;
}

size_t type_checker_get_type_alignment(Type* type) {
    if (!type) return 1;
    return type->alignment;
}

// Type inference and promotion functions implementation

Type* type_checker_promote_types(TypeChecker* checker, Type* left, Type* right, const char* operator) {
    if (!checker || !left || !right || !operator) return NULL;
    
    // For comparison operators, result is always int32 (boolean represented as int)
    if (strcmp(operator, "==") == 0 || strcmp(operator, "!=") == 0 ||
        strcmp(operator, "<") == 0 || strcmp(operator, "<=") == 0 ||
        strcmp(operator, ">") == 0 || strcmp(operator, ">=") == 0) {
        return checker->builtin_int32;
    }
    
    // For arithmetic operators, promote to larger type
    if (strcmp(operator, "+") == 0 || strcmp(operator, "-") == 0 ||
        strcmp(operator, "*") == 0 || strcmp(operator, "/") == 0 ||
        strcmp(operator, "%") == 0) {
        
        // If either operand is floating-point, result is floating-point
        if (type_checker_is_floating_type(left) || type_checker_is_floating_type(right)) {
            return type_checker_get_larger_type(checker, left, right);
        }
        
        // Both are integers, promote to larger integer type
        if (type_checker_is_integer_type(left) && type_checker_is_integer_type(right)) {
            return type_checker_get_larger_type(checker, left, right);
        }
    }
    
    // For logical operators, result is int32 (boolean)
    if (strcmp(operator, "&&") == 0 || strcmp(operator, "||") == 0) {
        return checker->builtin_int32;
    }
    
    // Default: return left type
    return left;
}

Type* type_checker_get_larger_type(TypeChecker* checker, Type* type1, Type* type2) {
    if (!checker || !type1 || !type2) return NULL;
    
    int rank1 = type_checker_get_type_rank(type1);
    int rank2 = type_checker_get_type_rank(type2);
    
    // Return the type with higher rank
    return (rank1 >= rank2) ? type1 : type2;
}

int type_checker_get_type_rank(Type* type) {
    if (!type) return -1;
    
    // Type promotion ranking (higher number = higher rank)
    switch (type->kind) {
        case TYPE_INT8:
        case TYPE_UINT8:
            return 1;
        case TYPE_INT16:
        case TYPE_UINT16:
            return 2;
        case TYPE_INT32:
        case TYPE_UINT32:
            return 3;
        case TYPE_FLOAT32:
            return 4;
        case TYPE_INT64:
        case TYPE_UINT64:
            return 5;
        case TYPE_FLOAT64:
            return 6;
        case TYPE_STRING:
            return 10; // Special case - strings don't promote with numbers
        default:
            return 0;
    }
}

Type* type_checker_infer_variable_type(TypeChecker* checker, ASTNode* initializer) {
    if (!checker || !initializer) return NULL;
    
    // Use the general type inference function
    return type_checker_infer_type(checker, initializer);
}

// Type compatibility and conversion functions implementation

int type_checker_is_assignable(TypeChecker* checker, Type* dest_type, Type* src_type) {
    if (!checker || !dest_type || !src_type) return 0;
    
    // Exact type match is always assignable
    if (dest_type->kind == src_type->kind) {
        return 1;
    }
    
    // Check for safe implicit conversions
    return type_checker_is_implicitly_convertible(src_type, dest_type);
}

int type_checker_is_implicitly_convertible(Type* from_type, Type* to_type) {
    if (!from_type || !to_type) return 0;
    
    // Same type is always convertible
    if (from_type->kind == to_type->kind) {
        return 1;
    }
    
    // Integer to integer conversions (with size constraints for safety)
    if (type_checker_is_integer_type(from_type) && type_checker_is_integer_type(to_type)) {
        // Allow conversions to larger or equal size types
        // Also allow signed to unsigned of same size (common in systems programming)
        int from_rank = type_checker_get_type_rank(from_type);
        int to_rank = type_checker_get_type_rank(to_type);
        return from_rank <= to_rank;
    }
    
    // Integer to floating point conversions
    if (type_checker_is_integer_type(from_type) && type_checker_is_floating_type(to_type)) {
        return 1; // Generally safe
    }
    
    // Floating point to floating point conversions (to larger precision)
    if (type_checker_is_floating_type(from_type) && type_checker_is_floating_type(to_type)) {
        int from_rank = type_checker_get_type_rank(from_type);
        int to_rank = type_checker_get_type_rank(to_type);
        return from_rank <= to_rank;
    }
    
    // No other implicit conversions are allowed
    return 0;
}

int type_checker_validate_function_call(TypeChecker* checker, CallExpression* call, Symbol* func_symbol) {
    if (!checker || !call || !func_symbol) return 0;
    
    // Check argument count
    if (call->argument_count != func_symbol->data.function.parameter_count) {
        type_checker_set_error(checker, 
            "Function '%s' expects %zu arguments, got %zu",
            call->function_name,
            func_symbol->data.function.parameter_count,
            call->argument_count);
        return 0;
    }
    
    // Check each argument type
    for (size_t i = 0; i < call->argument_count; i++) {
        Type* arg_type = type_checker_infer_type(checker, call->arguments[i]);
        if (!arg_type) {
            type_checker_set_error(checker,
                "Cannot infer type for argument %zu in call to '%s'",
                i + 1, call->function_name);
            return 0;
        }
        
        Type* param_type = func_symbol->data.function.parameter_types[i];
        if (!type_checker_is_assignable(checker, param_type, arg_type)) {
            type_checker_set_error(checker,
                "Argument %zu in call to '%s': cannot convert from '%s' to '%s'",
                i + 1, call->function_name,
                arg_type->name ? arg_type->name : "unknown",
                param_type->name ? param_type->name : "unknown");
            return 0;
        }
    }
    
    return 1;
}

int type_checker_validate_assignment(TypeChecker* checker, Type* dest_type, ASTNode* src_expr) {
    if (!checker || !dest_type || !src_expr) return 0;
    
    Type* src_type = type_checker_infer_type(checker, src_expr);
    if (!src_type) {
        type_checker_set_error(checker, "Cannot infer type of assignment value");
        return 0;
    }
    
    if (!type_checker_is_assignable(checker, dest_type, src_type)) {
        type_checker_set_error(checker,
            "Cannot assign value of type '%s' to variable of type '%s'",
            src_type->name ? src_type->name : "unknown",
            dest_type->name ? dest_type->name : "unknown");
        return 0;
    }
    
    return 1;
}

void type_checker_set_error(TypeChecker* checker, const char* format, ...) {
    if (!checker || !format) return;
    
    // Free previous error message
    free(checker->error_message);
    
    // Calculate required buffer size
    va_list args1, args2;
    va_start(args1, format);
    va_copy(args2, args1);
    
    int size = vsnprintf(NULL, 0, format, args1);
    va_end(args1);
    
    if (size < 0) {
        checker->error_message = NULL;
        checker->has_error = 1;
        va_end(args2);
        return;
    }
    
    // Allocate and format the message
    checker->error_message = malloc(size + 1);
    if (checker->error_message) {
        vsnprintf(checker->error_message, size + 1, format, args2);
    }
    
    va_end(args2);
    checker->has_error = 1;
}

// Struct type processing functions

int type_checker_process_struct_declaration(TypeChecker* checker, ASTNode* struct_decl) {
    if (!checker || !struct_decl || struct_decl->type != AST_STRUCT_DECLARATION) {
        return 0;
    }
    
    StructDeclaration* decl = (StructDeclaration*)struct_decl->data;
    if (!decl || !decl->name) {
        return 0;
    }
    
    // Check if struct already exists
    Symbol* existing = symbol_table_lookup_current_scope(checker->symbol_table, decl->name);
    if (existing) {
        checker->has_error = 1;
        if (checker->error_message) free(checker->error_message);
        size_t msg_len = strlen(decl->name) + 50;
        checker->error_message = malloc(msg_len);
        snprintf(checker->error_message, msg_len, 
                 "Struct '%s' is already declared", decl->name);
        return 0;
    }
    
    // Resolve field types
    Type** field_types = malloc(decl->field_count * sizeof(Type*));
    if (!field_types) return 0;
    
    for (size_t i = 0; i < decl->field_count; i++) {
        field_types[i] = type_checker_get_type_by_name(checker, decl->field_types[i]);
        if (!field_types[i]) {
            checker->has_error = 1;
            if (checker->error_message) free(checker->error_message);
            size_t msg_len = strlen(decl->field_types[i]) + 50;
            checker->error_message = malloc(msg_len);
            snprintf(checker->error_message, msg_len, 
                     "Unknown type '%s' in struct field", decl->field_types[i]);
            free(field_types);
            return 0;
        }
    }
    
    // Create struct type
    Type* struct_type = type_create_struct(decl->name, decl->field_names, 
                                           field_types, decl->field_count);
    if (!struct_type) {
        free(field_types);
        return 0;
    }
    
    // Create and register struct symbol
    Symbol* struct_symbol = symbol_create(decl->name, SYMBOL_STRUCT, struct_type);
    if (!struct_symbol) {
        type_destroy(struct_type);
        free(field_types);
        return 0;
    }
    
    if (!symbol_table_declare(checker->symbol_table, struct_symbol)) {
        symbol_destroy(struct_symbol);
        free(field_types);
        return 0;
    }
    
    free(field_types);
    return 1;
}

int type_checker_process_declaration(TypeChecker* checker, ASTNode* declaration) {
    if (!checker || !declaration) {
        return 0;
    }
    
    switch (declaration->type) {
        case AST_VAR_DECLARATION: {
            VarDeclaration* var_decl = (VarDeclaration*)declaration->data;
            if (!var_decl || !var_decl->name) {
                type_checker_set_error(checker, "Invalid variable declaration");
                return 0;
            }
            
            Type* var_type = NULL;
            
            // If type is explicitly specified, resolve it
            if (var_decl->type_name) {
                var_type = type_checker_get_type_by_name(checker, var_decl->type_name);
                if (!var_type) {
                    type_checker_set_error(checker, "Unknown type '%s' in variable declaration", var_decl->type_name);
                    return 0;
                }
            }
            
            // If there's an initializer, validate it
            if (var_decl->initializer) {
                Type* init_type = type_checker_infer_type(checker, var_decl->initializer);
                if (!init_type) {
                    type_checker_set_error(checker, "Cannot infer type of initializer for variable '%s'", var_decl->name);
                    return 0;
                }
                
                if (var_type) {
                    // Type specified: validate assignment compatibility
                    if (!type_checker_validate_assignment(checker, var_type, var_decl->initializer)) {
                        return 0; // Error already set
                    }
                } else {
                    // Type inference: use initializer type
                    var_type = init_type;
                }
            } else if (!var_type) {
                type_checker_set_error(checker, "Variable '%s' must have either a type annotation or an initializer", var_decl->name);
                return 0;
            }
            
            // Create and declare the symbol
            Symbol* var_symbol = symbol_create(var_decl->name, SYMBOL_VARIABLE, var_type);
            if (!var_symbol) {
                type_checker_set_error(checker, "Failed to create symbol for variable '%s'", var_decl->name);
                return 0;
            }
            
            if (!symbol_table_declare(checker->symbol_table, var_symbol)) {
                type_checker_set_error(checker, "Failed to declare variable '%s' - may already be declared", var_decl->name);
                symbol_destroy(var_symbol);
                return 0;
            }
            
            return 1;
        }
            
        case AST_FUNCTION_DECLARATION: {
            FunctionDeclaration* func_decl = (FunctionDeclaration*)declaration->data;
            if (!func_decl || !func_decl->name) {
                type_checker_set_error(checker, "Invalid function declaration");
                return 0;
            }
            
            // Resolve return type
            Type* return_type = NULL;
            if (func_decl->return_type) {
                return_type = type_checker_get_type_by_name(checker, func_decl->return_type);
                if (!return_type) {
                    type_checker_set_error(checker, "Unknown return type '%s' in function '%s'", 
                                         func_decl->return_type, func_decl->name);
                    return 0;
                }
            } else {
                return_type = checker->builtin_void;
            }
            
            // Resolve parameter types
            Type** param_types = NULL;
            if (func_decl->parameter_count > 0) {
                param_types = malloc(func_decl->parameter_count * sizeof(Type*));
                if (!param_types) {
                    type_checker_set_error(checker, "Memory allocation failed for function parameters");
                    return 0;
                }
                
                for (size_t i = 0; i < func_decl->parameter_count; i++) {
                    param_types[i] = type_checker_get_type_by_name(checker, func_decl->parameter_types[i]);
                    if (!param_types[i]) {
                        type_checker_set_error(checker, "Unknown parameter type '%s' in function '%s'", 
                                             func_decl->parameter_types[i], func_decl->name);
                        free(param_types);
                        return 0;
                    }
                }
            }
            
            // Create function symbol
            Symbol* func_symbol = symbol_create(func_decl->name, SYMBOL_FUNCTION, return_type);
            if (!func_symbol) {
                type_checker_set_error(checker, "Failed to create symbol for function '%s'", func_decl->name);
                free(param_types);
                return 0;
            }
            
            // Set function-specific data
            func_symbol->data.function.parameter_count = func_decl->parameter_count;
            func_symbol->data.function.parameter_names = func_decl->parameter_names;
            func_symbol->data.function.parameter_types = param_types;
            func_symbol->data.function.return_type = return_type;
            
            if (!symbol_table_declare(checker->symbol_table, func_symbol)) {
                type_checker_set_error(checker, "Failed to declare function '%s' - may already be declared", func_decl->name);
                symbol_destroy(func_symbol);
                return 0;
            }
            
            return 1;
        }
            
        case AST_METHOD_DECLARATION:
            // Method declarations are handled within struct processing
            // This case shouldn't normally be reached during standalone processing
            return 1;
            
        case AST_ASSIGNMENT: {
            Assignment* assignment = (Assignment*)declaration->data;
            if (!assignment || !assignment->variable_name || !assignment->value) {
                type_checker_set_error(checker, "Invalid assignment statement");
                return 0;
            }
            
            // Look up the variable
            Symbol* var_symbol = symbol_table_lookup(checker->symbol_table, assignment->variable_name);
            if (!var_symbol) {
                type_checker_set_error(checker, "Undefined variable '%s' in assignment", assignment->variable_name);
                return 0;
            }
            
            if (var_symbol->kind != SYMBOL_VARIABLE) {
                type_checker_set_error(checker, "'%s' is not a variable and cannot be assigned to", assignment->variable_name);
                return 0;
            }
            
            // Validate assignment compatibility
            if (!type_checker_validate_assignment(checker, var_symbol->type, assignment->value)) {
                return 0; // Error already set
            }
            
            return 1;
        }
            
        default:
            // Unknown declaration type - just accept it for now
            return 1;
    }
}