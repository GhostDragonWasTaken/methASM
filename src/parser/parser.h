#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "../lexer/lexer.h"

typedef struct {
    Lexer* lexer;
    Token current_token;
    Token peek_token;
    int has_error;
    char* error_message;
} Parser;

// Function declarations
Parser* parser_create(Lexer* lexer);
void parser_destroy(Parser* parser);
ASTNode* parser_parse_program(Parser* parser);
ASTNode* parser_parse_declaration(Parser* parser);
ASTNode* parser_parse_statement(Parser* parser);
ASTNode* parser_parse_expression(Parser* parser);

// Expression parsing with precedence
ASTNode* parser_parse_primary_expression(Parser* parser);
ASTNode* parser_parse_unary_expression(Parser* parser);
ASTNode* parser_parse_binary_expression(Parser* parser, int min_precedence);
ASTNode* parser_parse_postfix_expression(Parser* parser);

// Specific parsing functions
ASTNode* parser_parse_var_declaration(Parser* parser);
ASTNode* parser_parse_function_declaration(Parser* parser);
ASTNode* parser_parse_struct_declaration(Parser* parser);
ASTNode* parser_parse_method_declaration(Parser* parser);
ASTNode* parser_parse_inline_asm(Parser* parser);
ASTNode* parser_parse_assignment(Parser* parser);
ASTNode* parser_parse_return_statement(Parser* parser);
ASTNode* parser_parse_if_statement(Parser* parser);
ASTNode* parser_parse_while_statement(Parser* parser);
ASTNode* parser_parse_block(Parser* parser);

// Utility functions
void parser_advance(Parser* parser);
int parser_match(Parser* parser, TokenType type);
int parser_expect(Parser* parser, TokenType type);
void parser_set_error(Parser* parser, const char* message);
void parser_recover_from_error(Parser* parser);
int parser_get_operator_precedence(TokenType type);
int parser_is_binary_operator(TokenType type);
int parser_is_unary_operator(TokenType type);
int parser_is_identifier_like(TokenType type);
int parser_is_type_keyword(TokenType type);

#endif // PARSER_H