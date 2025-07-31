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
        case AST_INLINE_ASM: {
            InlineAsm* inline_asm = (InlineAsm*)node->data;
            if (inline_asm) {
                free(inline_asm->assembly_code);
                free(inline_asm);
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