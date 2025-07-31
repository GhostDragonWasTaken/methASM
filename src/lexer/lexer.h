#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

typedef enum {
    TOKEN_EOF,
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_VAR,
    TOKEN_FUNCTION,
    TOKEN_STRUCT,
    TOKEN_METHOD,
    TOKEN_RETURN,
    TOKEN_IF,
    TOKEN_WHILE,
    TOKEN_ASM,
    TOKEN_THIS,
    TOKEN_COLON,
    TOKEN_SEMICOLON,
    TOKEN_COMMA,
    TOKEN_EQUALS,
    TOKEN_ARROW,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_MULTIPLY,
    TOKEN_DIVIDE,
    TOKEN_DOT,
    TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char* value;
    size_t line;
    size_t column;
} Token;

typedef struct {
    const char* source;
    size_t position;
    size_t line;
    size_t column;
    size_t length;
} Lexer;

// Function declarations
Lexer* lexer_create(const char* source);
void lexer_destroy(Lexer* lexer);
Token lexer_next_token(Lexer* lexer);
Token lexer_peek_token(Lexer* lexer);
void token_destroy(Token* token);

#endif // LEXER_H