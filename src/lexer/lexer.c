#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

Lexer* lexer_create(const char* source) {
    Lexer* lexer = malloc(sizeof(Lexer));
    if (!lexer) return NULL;
    
    lexer->source = source;
    lexer->position = 0;
    lexer->line = 1;
    lexer->column = 1;
    lexer->length = strlen(source);
    
    return lexer;
}

void lexer_destroy(Lexer* lexer) {
    if (lexer) {
        free(lexer);
    }
}

Token lexer_next_token(Lexer* lexer) {
    Token token = {TOKEN_EOF, NULL, lexer->line, lexer->column};
    
    // Skip whitespace
    while (lexer->position < lexer->length && isspace(lexer->source[lexer->position])) {
        if (lexer->source[lexer->position] == '\n') {
            lexer->line++;
            lexer->column = 1;
        } else {
            lexer->column++;
        }
        lexer->position++;
    }
    
    if (lexer->position >= lexer->length) {
        return token; // EOF
    }
    
    char current = lexer->source[lexer->position];
    token.line = lexer->line;
    token.column = lexer->column;
    
    // Single character tokens
    switch (current) {
        case ':': token.type = TOKEN_COLON; break;
        case ';': token.type = TOKEN_SEMICOLON; break;
        case ',': token.type = TOKEN_COMMA; break;
        case '=': token.type = TOKEN_EQUALS; break;
        case '(': token.type = TOKEN_LPAREN; break;
        case ')': token.type = TOKEN_RPAREN; break;
        case '{': token.type = TOKEN_LBRACE; break;
        case '}': token.type = TOKEN_RBRACE; break;
        case '[': token.type = TOKEN_LBRACKET; break;
        case ']': token.type = TOKEN_RBRACKET; break;
        case '+': token.type = TOKEN_PLUS; break;
        case '*': token.type = TOKEN_MULTIPLY; break;
        case '/': token.type = TOKEN_DIVIDE; break;
        case '.': token.type = TOKEN_DOT; break;
        default:
            token.type = TOKEN_ERROR;
    }
    
    if (token.type != TOKEN_ERROR) {
        token.value = malloc(2);
        token.value[0] = current;
        token.value[1] = '\0';
        lexer->position++;
        lexer->column++;
        return token;
    }
    
    // Handle arrow operator
    if (current == '-' && lexer->position + 1 < lexer->length && lexer->source[lexer->position + 1] == '>') {
        token.type = TOKEN_ARROW;
        token.value = malloc(3);
        strcpy(token.value, "->");
        lexer->position += 2;
        lexer->column += 2;
        return token;
    }
    
    if (current == '-') {
        token.type = TOKEN_MINUS;
        token.value = malloc(2);
        token.value[0] = current;
        token.value[1] = '\0';
        lexer->position++;
        lexer->column++;
        return token;
    }
    
    // Numbers
    if (isdigit(current)) {
        size_t start = lexer->position;
        while (lexer->position < lexer->length && (isdigit(lexer->source[lexer->position]) || lexer->source[lexer->position] == '.')) {
            lexer->position++;
            lexer->column++;
        }
        
        size_t length = lexer->position - start;
        token.type = TOKEN_NUMBER;
        token.value = malloc(length + 1);
        strncpy(token.value, &lexer->source[start], length);
        token.value[length] = '\0';
        return token;
    }
    
    // Identifiers and keywords
    if (isalpha(current) || current == '_') {
        size_t start = lexer->position;
        while (lexer->position < lexer->length && (isalnum(lexer->source[lexer->position]) || lexer->source[lexer->position] == '_')) {
            lexer->position++;
            lexer->column++;
        }
        
        size_t length = lexer->position - start;
        token.value = malloc(length + 1);
        strncpy(token.value, &lexer->source[start], length);
        token.value[length] = '\0';
        
        // Check for keywords
        if (strcmp(token.value, "var") == 0) token.type = TOKEN_VAR;
        else if (strcmp(token.value, "function") == 0) token.type = TOKEN_FUNCTION;
        else if (strcmp(token.value, "struct") == 0) token.type = TOKEN_STRUCT;
        else if (strcmp(token.value, "method") == 0) token.type = TOKEN_METHOD;
        else if (strcmp(token.value, "return") == 0) token.type = TOKEN_RETURN;
        else if (strcmp(token.value, "if") == 0) token.type = TOKEN_IF;
        else if (strcmp(token.value, "while") == 0) token.type = TOKEN_WHILE;
        else if (strcmp(token.value, "asm") == 0) token.type = TOKEN_ASM;
        else if (strcmp(token.value, "this") == 0) token.type = TOKEN_THIS;
        else token.type = TOKEN_IDENTIFIER;
        
        return token;
    }
    
    // String literals
    if (current == '"') {
        lexer->position++; // Skip opening quote
        lexer->column++;
        size_t start = lexer->position;
        
        while (lexer->position < lexer->length && lexer->source[lexer->position] != '"') {
            if (lexer->source[lexer->position] == '\\') {
                lexer->position++; // Skip escape character
                lexer->column++;
            }
            lexer->position++;
            lexer->column++;
        }
        
        if (lexer->position >= lexer->length) {
            token.type = TOKEN_ERROR;
            token.value = strdup("Unterminated string literal");
            return token;
        }
        
        size_t length = lexer->position - start;
        token.type = TOKEN_STRING;
        token.value = malloc(length + 1);
        strncpy(token.value, &lexer->source[start], length);
        token.value[length] = '\0';
        
        lexer->position++; // Skip closing quote
        lexer->column++;
        return token;
    }
    
    // Unknown character
    token.type = TOKEN_ERROR;
    token.value = malloc(20);
    snprintf(token.value, 20, "Unknown character: %c", current);
    lexer->position++;
    lexer->column++;
    
    return token;
}

Token lexer_peek_token(Lexer* lexer) {
    size_t saved_position = lexer->position;
    size_t saved_line = lexer->line;
    size_t saved_column = lexer->column;
    
    Token token = lexer_next_token(lexer);
    
    lexer->position = saved_position;
    lexer->line = saved_line;
    lexer->column = saved_column;
    
    return token;
}

void token_destroy(Token* token) {
    if (token && token->value) {
        free(token->value);
        token->value = NULL;
    }
}