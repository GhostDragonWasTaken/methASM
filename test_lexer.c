#include "src/lexer/lexer.h"
#include <stdio.h>
#include <stdlib.h>

const char* token_type_to_string(TokenType type) {
    switch (type) {
        case TOKEN_EOF: return "EOF";
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TOKEN_NUMBER: return "NUMBER";
        case TOKEN_STRING: return "STRING";
        case TOKEN_VAR: return "VAR";
        case TOKEN_FUNCTION: return "FUNCTION";
        case TOKEN_INT32: return "INT32";
        case TOKEN_FLOAT64: return "FLOAT64";
        case TOKEN_COLON: return "COLON";
        case TOKEN_SEMICOLON: return "SEMICOLON";
        case TOKEN_COMMA: return "COMMA";
        case TOKEN_EQUALS: return "EQUALS";
        case TOKEN_ARROW: return "ARROW";
        case TOKEN_RETURN: return "RETURN";
        case TOKEN_LPAREN: return "LPAREN";
        case TOKEN_RPAREN: return "RPAREN";
        case TOKEN_LBRACE: return "LBRACE";
        case TOKEN_RBRACE: return "RBRACE";
        case TOKEN_MOV: return "MOV";
        case TOKEN_ADD: return "ADD";
        case TOKEN_EAX: return "EAX";
        case TOKEN_EBX: return "EBX";
        default: return "UNKNOWN";
    }
}

int main() {
    const char* test_code = "var counter: int32 = 42;\nvar hex_val = 0xFF;\nvar bin_val = 0b1010;\nvar message = \"Hello\\nWorld\\t!\";\nfunction add(a: int32, b: int32) -> int32 {\n    mov eax, a\n    add eax, b\n    return eax;\n}";
    
    printf("Testing enhanced lexer with code:\n%s\n\n", test_code);
    
    Lexer* lexer = lexer_create(test_code);
    if (!lexer) {
        printf("Failed to create lexer\n");
        return 1;
    }
    
    size_t token_count;
    Token* tokens = lexer_tokenize(lexer, &token_count);
    
    if (!tokens) {
        printf("Failed to tokenize\n");
        lexer_destroy(lexer);
        return 1;
    }
    
    printf("Found %zu tokens:\n", token_count);
    for (size_t i = 0; i < token_count; i++) {
        printf("%zu: %s", i + 1, token_type_to_string(tokens[i].type));
        if (tokens[i].value) {
            printf(" ('%s')", tokens[i].value);
        }
        printf(" at line %zu, column %zu\n", tokens[i].line, tokens[i].column);
    }
    
    tokens_destroy(tokens, token_count);
    lexer_destroy(lexer);
    
    return 0;
}