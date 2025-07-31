#include "parser.h"
#include <stdlib.h>
#include <string.h>

Parser* parser_create(Lexer* lexer) {
    Parser* parser = malloc(sizeof(Parser));
    if (!parser) return NULL;
    
    parser->lexer = lexer;
    parser->current_token = lexer_next_token(lexer);
    parser->peek_token = lexer_peek_token(lexer);
    parser->has_error = 0;
    parser->error_message = NULL;
    
    return parser;
}

void parser_destroy(Parser* parser) {
    if (parser) {
        token_destroy(&parser->current_token);
        token_destroy(&parser->peek_token);
        free(parser->error_message);
        free(parser);
    }
}

ASTNode* parser_parse_program(Parser* parser) {
    SourceLocation location = {parser->current_token.line, parser->current_token.column};
    ASTNode* program = ast_create_node(AST_PROGRAM, location);
    
    while (parser->current_token.type != TOKEN_EOF && !parser->has_error) {
        ASTNode* declaration = parser_parse_declaration(parser);
        if (declaration) {
            ast_add_child(program, declaration);
        }
    }
    
    return program;
}

ASTNode* parser_parse_declaration(Parser* parser) {
    // Stub implementation - just consume one token and return NULL
    token_destroy(&parser->current_token);
    parser->current_token = lexer_next_token(parser->lexer);
    return NULL;
}

ASTNode* parser_parse_statement(Parser* parser) {
    // Stub implementation
    (void)parser; // Suppress unused parameter warning
    return NULL;
}

ASTNode* parser_parse_expression(Parser* parser) {
    // Stub implementation
    (void)parser; // Suppress unused parameter warning
    return NULL;
}