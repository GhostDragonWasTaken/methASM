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

// Function declarations
ASTNode* ast_create_node(ASTNodeType type, SourceLocation location);
void ast_destroy_node(ASTNode* node);
void ast_add_child(ASTNode* parent, ASTNode* child);

#endif // AST_H