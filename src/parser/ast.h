#ifndef AST_H
#define AST_H

#include <stddef.h>

typedef enum {
    AST_PROGRAM,
    AST_VAR_DECLARATION,
    AST_FUNCTION_DECLARATION,
    AST_STRUCT_DECLARATION,
    AST_METHOD_DECLARATION,
    AST_ASSIGNMENT,
    AST_FUNCTION_CALL,
    AST_RETURN_STATEMENT,
    AST_IF_STATEMENT,
    AST_WHILE_STATEMENT,
    AST_INLINE_ASM,
    AST_IDENTIFIER,
    AST_NUMBER_LITERAL,
    AST_STRING_LITERAL,
    AST_BINARY_EXPRESSION,
    AST_UNARY_EXPRESSION,
    AST_MEMBER_ACCESS
} ASTNodeType;

typedef struct {
    size_t line;
    size_t column;
} SourceLocation;

typedef struct ASTNode {
    ASTNodeType type;
    SourceLocation location;
    struct ASTNode** children;
    size_t child_count;
    void* data; // Node-specific data
} ASTNode;

typedef struct {
    char* name;
    char* type_name;
    ASTNode* initializer;
} VarDeclaration;

typedef struct {
    char* name;
    char** parameter_names;
    char** parameter_types;
    size_t parameter_count;
    char* return_type;
    ASTNode* body;
} FunctionDeclaration;

typedef struct {
    char* name;
    char** field_names;
    char** field_types;
    size_t field_count;
    ASTNode** methods;
    size_t method_count;
} StructDeclaration;

typedef struct {
    char* assembly_code;
} InlineAsm;

typedef struct {
    ASTNode** declarations;
    size_t declaration_count;
} Program;

typedef struct {
    char* function_name;
    ASTNode** arguments;
    size_t argument_count;
} CallExpression;

typedef struct {
    char* variable_name;
    ASTNode* value;
} Assignment;

typedef struct {
    char* name;
} Identifier;

typedef struct {
    union {
        long long int_value;
        double float_value;
    };
    int is_float;
} NumberLiteral;

typedef struct {
    char* value;
} StringLiteral;

typedef struct {
    ASTNode* left;
    ASTNode* right;
    char* operator;
} BinaryExpression;

typedef struct {
    ASTNode* operand;
    char* operator;
} UnaryExpression;

typedef struct {
    ASTNode* object;
    char* member;
} MemberAccess;

typedef struct {
    ASTNode* condition;
    ASTNode* then_branch;
    ASTNode* else_branch;
} IfStatement;

typedef struct {
    ASTNode* condition;
    ASTNode* body;
} WhileStatement;

typedef struct {
    ASTNode* value;
} ReturnStatement;

// Function declarations
ASTNode* ast_create_node(ASTNodeType type, SourceLocation location);
void ast_destroy_node(ASTNode* node);
void ast_add_child(ASTNode* parent, ASTNode* child);

// Specific node creation functions
ASTNode* ast_create_program();
ASTNode* ast_create_var_declaration(const char* name, const char* type_name, ASTNode* initializer, SourceLocation location);
ASTNode* ast_create_function_declaration(const char* name, char** param_names, char** param_types, size_t param_count, const char* return_type, ASTNode* body, SourceLocation location);
ASTNode* ast_create_struct_declaration(const char* name, char** field_names, char** field_types, size_t field_count, ASTNode** methods, size_t method_count, SourceLocation location);
ASTNode* ast_create_call_expression(const char* function_name, ASTNode** arguments, size_t argument_count, SourceLocation location);
ASTNode* ast_create_assignment(const char* variable_name, ASTNode* value, SourceLocation location);
ASTNode* ast_create_inline_asm(const char* assembly_code, SourceLocation location);
ASTNode* ast_create_identifier(const char* name, SourceLocation location);
ASTNode* ast_create_number_literal(long long int_value, SourceLocation location);
ASTNode* ast_create_float_literal(double float_value, SourceLocation location);
ASTNode* ast_create_string_literal(const char* value, SourceLocation location);
ASTNode* ast_create_binary_expression(ASTNode* left, const char* operator, ASTNode* right, SourceLocation location);
ASTNode* ast_create_unary_expression(const char* operator, ASTNode* operand, SourceLocation location);
ASTNode* ast_create_member_access(ASTNode* object, const char* member, SourceLocation location);

#endif // AST_H