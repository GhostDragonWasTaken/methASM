#include "ast.h"
#include <stdlib.h>
#include <string.h>

ASTNode* ast_create_node(ASTNodeType type, SourceLocation location) {
    ASTNode* node = malloc(sizeof(ASTNode));
    if (!node) return NULL;
    
    node->type = type;
    node->location = location;
    node->children = NULL;
    node->child_count = 0;
    node->data = NULL;
    
    return node;
}

void ast_destroy_node(ASTNode* node) {
    if (!node) return;
    
    // Free children
    for (size_t i = 0; i < node->child_count; i++) {
        ast_destroy_node(node->children[i]);
    }
    free(node->children);
    
    // Free node-specific data
    switch (node->type) {
        case AST_PROGRAM: {
            Program* program = (Program*)node->data;
            if (program) {
                free(program->declarations);
                free(program);
            }
            break;
        }
        case AST_VAR_DECLARATION: {
            VarDeclaration* var_decl = (VarDeclaration*)node->data;
            if (var_decl) {
                free(var_decl->name);
                free(var_decl->type_name);
                free(var_decl);
            }
            break;
        }
        case AST_FUNCTION_DECLARATION: {
            FunctionDeclaration* func_decl = (FunctionDeclaration*)node->data;
            if (func_decl) {
                free(func_decl->name);
                free(func_decl->return_type);
                for (size_t i = 0; i < func_decl->parameter_count; i++) {
                    free(func_decl->parameter_names[i]);
                    free(func_decl->parameter_types[i]);
                }
                free(func_decl->parameter_names);
                free(func_decl->parameter_types);
                free(func_decl);
            }
            break;
        }
        case AST_STRUCT_DECLARATION: {
            StructDeclaration* struct_decl = (StructDeclaration*)node->data;
            if (struct_decl) {
                free(struct_decl->name);
                for (size_t i = 0; i < struct_decl->field_count; i++) {
                    free(struct_decl->field_names[i]);
                    free(struct_decl->field_types[i]);
                }
                free(struct_decl->field_names);
                free(struct_decl->field_types);
                free(struct_decl->methods);
                free(struct_decl);
            }
            break;
        }
        case AST_FUNCTION_CALL: {
            CallExpression* call_expr = (CallExpression*)node->data;
            if (call_expr) {
                free(call_expr->function_name);
                free(call_expr->arguments);
                free(call_expr);
            }
            break;
        }
        case AST_ASSIGNMENT: {
            Assignment* assignment = (Assignment*)node->data;
            if (assignment) {
                free(assignment->variable_name);
                free(assignment);
            }
            break;
        }
        case AST_INLINE_ASM: {
            InlineAsm* inline_asm = (InlineAsm*)node->data;
            if (inline_asm) {
                free(inline_asm->assembly_code);
                free(inline_asm);
            }
            break;
        }
        case AST_IDENTIFIER: {
            Identifier* identifier = (Identifier*)node->data;
            if (identifier) {
                free(identifier->name);
                free(identifier);
            }
            break;
        }
        case AST_STRING_LITERAL: {
            StringLiteral* string_literal = (StringLiteral*)node->data;
            if (string_literal) {
                free(string_literal->value);
                free(string_literal);
            }
            break;
        }
        case AST_BINARY_EXPRESSION: {
            BinaryExpression* binary_expr = (BinaryExpression*)node->data;
            if (binary_expr) {
                free(binary_expr->operator);
                free(binary_expr);
            }
            break;
        }
        case AST_UNARY_EXPRESSION: {
            UnaryExpression* unary_expr = (UnaryExpression*)node->data;
            if (unary_expr) {
                free(unary_expr->operator);
                free(unary_expr);
            }
            break;
        }
        case AST_MEMBER_ACCESS: {
            MemberAccess* member_access = (MemberAccess*)node->data;
            if (member_access) {
                free(member_access->member);
                free(member_access);
            }
            break;
        }
        default:
            free(node->data);
            break;
    }
    
    free(node);
}

void ast_add_child(ASTNode* parent, ASTNode* child) {
    if (!parent || !child) return;
    
    parent->children = realloc(parent->children, (parent->child_count + 1) * sizeof(ASTNode*));
    if (!parent->children) return;
    
    parent->children[parent->child_count] = child;
    parent->child_count++;
}

// Specific node creation functions
ASTNode* ast_create_program() {
    SourceLocation location = {0, 0};
    ASTNode* node = ast_create_node(AST_PROGRAM, location);
    if (!node) return NULL;
    
    Program* program = malloc(sizeof(Program));
    if (!program) {
        free(node);
        return NULL;
    }
    
    program->declarations = NULL;
    program->declaration_count = 0;
    node->data = program;
    
    return node;
}

ASTNode* ast_create_var_declaration(const char* name, const char* type_name, ASTNode* initializer, SourceLocation location) {
    ASTNode* node = ast_create_node(AST_VAR_DECLARATION, location);
    if (!node) return NULL;
    
    VarDeclaration* var_decl = malloc(sizeof(VarDeclaration));
    if (!var_decl) {
        free(node);
        return NULL;
    }
    
    var_decl->name = name ? strdup(name) : NULL;
    var_decl->type_name = type_name ? strdup(type_name) : NULL;
    var_decl->initializer = initializer;
    node->data = var_decl;
    
    if (initializer) {
        ast_add_child(node, initializer);
    }
    
    return node;
}

ASTNode* ast_create_function_declaration(const char* name, char** param_names, char** param_types, size_t param_count, const char* return_type, ASTNode* body, SourceLocation location) {
    ASTNode* node = ast_create_node(AST_FUNCTION_DECLARATION, location);
    if (!node) return NULL;
    
    FunctionDeclaration* func_decl = malloc(sizeof(FunctionDeclaration));
    if (!func_decl) {
        free(node);
        return NULL;
    }
    
    func_decl->name = name ? strdup(name) : NULL;
    func_decl->return_type = return_type ? strdup(return_type) : NULL;
    func_decl->parameter_count = param_count;
    func_decl->body = body;
    
    if (param_count > 0) {
        func_decl->parameter_names = malloc(param_count * sizeof(char*));
        func_decl->parameter_types = malloc(param_count * sizeof(char*));
        
        for (size_t i = 0; i < param_count; i++) {
            func_decl->parameter_names[i] = param_names[i] ? strdup(param_names[i]) : NULL;
            func_decl->parameter_types[i] = param_types[i] ? strdup(param_types[i]) : NULL;
        }
    } else {
        func_decl->parameter_names = NULL;
        func_decl->parameter_types = NULL;
    }
    
    node->data = func_decl;
    
    if (body) {
        ast_add_child(node, body);
    }
    
    return node;
}

ASTNode* ast_create_struct_declaration(const char* name, char** field_names, char** field_types, size_t field_count, ASTNode** methods, size_t method_count, SourceLocation location) {
    ASTNode* node = ast_create_node(AST_STRUCT_DECLARATION, location);
    if (!node) return NULL;
    
    StructDeclaration* struct_decl = malloc(sizeof(StructDeclaration));
    if (!struct_decl) {
        free(node);
        return NULL;
    }
    
    struct_decl->name = name ? strdup(name) : NULL;
    struct_decl->field_count = field_count;
    struct_decl->method_count = method_count;
    
    if (field_count > 0) {
        struct_decl->field_names = malloc(field_count * sizeof(char*));
        struct_decl->field_types = malloc(field_count * sizeof(char*));
        
        for (size_t i = 0; i < field_count; i++) {
            struct_decl->field_names[i] = field_names[i] ? strdup(field_names[i]) : NULL;
            struct_decl->field_types[i] = field_types[i] ? strdup(field_types[i]) : NULL;
        }
    } else {
        struct_decl->field_names = NULL;
        struct_decl->field_types = NULL;
    }
    
    if (method_count > 0) {
        struct_decl->methods = malloc(method_count * sizeof(ASTNode*));
        for (size_t i = 0; i < method_count; i++) {
            struct_decl->methods[i] = methods[i];
            if (methods[i]) {
                ast_add_child(node, methods[i]);
            }
        }
    } else {
        struct_decl->methods = NULL;
    }
    
    node->data = struct_decl;
    
    return node;
}

ASTNode* ast_create_call_expression(const char* function_name, ASTNode** arguments, size_t argument_count, SourceLocation location) {
    ASTNode* node = ast_create_node(AST_FUNCTION_CALL, location);
    if (!node) return NULL;
    
    CallExpression* call_expr = malloc(sizeof(CallExpression));
    if (!call_expr) {
        free(node);
        return NULL;
    }
    
    call_expr->function_name = function_name ? strdup(function_name) : NULL;
    call_expr->argument_count = argument_count;
    
    if (argument_count > 0) {
        call_expr->arguments = malloc(argument_count * sizeof(ASTNode*));
        for (size_t i = 0; i < argument_count; i++) {
            call_expr->arguments[i] = arguments[i];
            if (arguments[i]) {
                ast_add_child(node, arguments[i]);
            }
        }
    } else {
        call_expr->arguments = NULL;
    }
    
    node->data = call_expr;
    
    return node;
}

ASTNode* ast_create_assignment(const char* variable_name, ASTNode* value, SourceLocation location) {
    ASTNode* node = ast_create_node(AST_ASSIGNMENT, location);
    if (!node) return NULL;
    
    Assignment* assignment = malloc(sizeof(Assignment));
    if (!assignment) {
        free(node);
        return NULL;
    }
    
    assignment->variable_name = variable_name ? strdup(variable_name) : NULL;
    assignment->value = value;
    node->data = assignment;
    
    if (value) {
        ast_add_child(node, value);
    }
    
    return node;
}

ASTNode* ast_create_inline_asm(const char* assembly_code, SourceLocation location) {
    ASTNode* node = ast_create_node(AST_INLINE_ASM, location);
    if (!node) return NULL;
    
    InlineAsm* inline_asm = malloc(sizeof(InlineAsm));
    if (!inline_asm) {
        free(node);
        return NULL;
    }
    
    inline_asm->assembly_code = assembly_code ? strdup(assembly_code) : NULL;
    node->data = inline_asm;
    
    return node;
}

ASTNode* ast_create_identifier(const char* name, SourceLocation location) {
    ASTNode* node = ast_create_node(AST_IDENTIFIER, location);
    if (!node) return NULL;
    
    Identifier* identifier = malloc(sizeof(Identifier));
    if (!identifier) {
        free(node);
        return NULL;
    }
    
    identifier->name = name ? strdup(name) : NULL;
    node->data = identifier;
    
    return node;
}

ASTNode* ast_create_number_literal(long long int_value, SourceLocation location) {
    ASTNode* node = ast_create_node(AST_NUMBER_LITERAL, location);
    if (!node) return NULL;
    
    NumberLiteral* number_literal = malloc(sizeof(NumberLiteral));
    if (!number_literal) {
        free(node);
        return NULL;
    }
    
    number_literal->int_value = int_value;
    number_literal->is_float = 0;
    node->data = number_literal;
    
    return node;
}

ASTNode* ast_create_float_literal(double float_value, SourceLocation location) {
    ASTNode* node = ast_create_node(AST_NUMBER_LITERAL, location);
    if (!node) return NULL;
    
    NumberLiteral* number_literal = malloc(sizeof(NumberLiteral));
    if (!number_literal) {
        free(node);
        return NULL;
    }
    
    number_literal->float_value = float_value;
    number_literal->is_float = 1;
    node->data = number_literal;
    
    return node;
}

ASTNode* ast_create_string_literal(const char* value, SourceLocation location) {
    ASTNode* node = ast_create_node(AST_STRING_LITERAL, location);
    if (!node) return NULL;
    
    StringLiteral* string_literal = malloc(sizeof(StringLiteral));
    if (!string_literal) {
        free(node);
        return NULL;
    }
    
    string_literal->value = value ? strdup(value) : NULL;
    node->data = string_literal;
    
    return node;
}

ASTNode* ast_create_binary_expression(ASTNode* left, const char* operator, ASTNode* right, SourceLocation location) {
    ASTNode* node = ast_create_node(AST_BINARY_EXPRESSION, location);
    if (!node) return NULL;
    
    BinaryExpression* binary_expr = malloc(sizeof(BinaryExpression));
    if (!binary_expr) {
        free(node);
        return NULL;
    }
    
    binary_expr->left = left;
    binary_expr->right = right;
    binary_expr->operator = operator ? strdup(operator) : NULL;
    node->data = binary_expr;
    
    if (left) {
        ast_add_child(node, left);
    }
    if (right) {
        ast_add_child(node, right);
    }
    
    return node;
}

ASTNode* ast_create_unary_expression(const char* operator, ASTNode* operand, SourceLocation location) {
    ASTNode* node = ast_create_node(AST_UNARY_EXPRESSION, location);
    if (!node) return NULL;
    
    UnaryExpression* unary_expr = malloc(sizeof(UnaryExpression));
    if (!unary_expr) {
        free(node);
        return NULL;
    }
    
    unary_expr->operator = operator ? strdup(operator) : NULL;
    unary_expr->operand = operand;
    node->data = unary_expr;
    
    if (operand) {
        ast_add_child(node, operand);
    }
    
    return node;
}

ASTNode* ast_create_member_access(ASTNode* object, const char* member, SourceLocation location) {
    ASTNode* node = ast_create_node(AST_MEMBER_ACCESS, location);
    if (!node) return NULL;
    
    MemberAccess* member_access = malloc(sizeof(MemberAccess));
    if (!member_access) {
        free(node);
        return NULL;
    }
    
    member_access->object = object;
    member_access->member = member ? strdup(member) : NULL;
    node->data = member_access;
    
    if (object) {
        ast_add_child(node, object);
    }
    
    return node;
}