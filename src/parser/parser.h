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

#endif // PARSER_H