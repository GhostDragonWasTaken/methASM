#include "src/lexer/lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Test result tracking
int tests_run = 0;
int tests_passed = 0;

#define TEST(name) \
    do { \
        printf("Running test: %s... ", #name); \
        tests_run++; \
        if (test_##name()) { \
            printf("PASSED\n"); \
            tests_passed++; \
        } else { \
            printf("FAILED\n"); \
        } \
    } while(0)

// Helper function to compare tokens
int token_equals(Token* actual, TokenType expected_type, const char* expected_value) {
    if (actual->type != expected_type) {
        printf("Expected type %d, got %d\n", expected_type, actual->type);
        return 0;
    }
    
    if (expected_value && (!actual->value || strcmp(actual->value, expected_value) != 0)) {
        printf("Expected value '%s', got '%s'\n", expected_value, actual->value ? actual->value : "NULL");
        return 0;
    }
    
    if (!expected_value && actual->value) {
        printf("Expected NULL value, got '%s'\n", actual->value);
        return 0;
    }
    
    return 1;
}

// Test basic keyword recognition
int test_keywords() {
    const char* code = "var function struct method return if while asm this";
    Lexer* lexer = lexer_create(code);
    
    TokenType expected[] = {TOKEN_VAR, TOKEN_FUNCTION, TOKEN_STRUCT, TOKEN_METHOD, 
                           TOKEN_RETURN, TOKEN_IF, TOKEN_WHILE, TOKEN_ASM, TOKEN_THIS, TOKEN_EOF};
    
    for (int i = 0; i < 10; i++) {
        Token token = lexer_next_token(lexer);
        if (token.type != expected[i]) {
            token_destroy(&token);
            lexer_destroy(lexer);
            return 0;
        }
        token_destroy(&token);
    }
    
    lexer_destroy(lexer);
    return 1;
}

// Test type keywords
int test_type_keywords() {
    const char* code = "int32 float64 string uint8 int64";
    Lexer* lexer = lexer_create(code);
    
    TokenType expected[] = {TOKEN_INT32, TOKEN_FLOAT64, TOKEN_STRING_TYPE, TOKEN_UINT8, TOKEN_INT64, TOKEN_EOF};
    
    for (int i = 0; i < 6; i++) {
        Token token = lexer_next_token(lexer);
        if (token.type != expected[i]) {
            token_destroy(&token);
            lexer_destroy(lexer);
            return 0;
        }
        token_destroy(&token);
    }
    
    lexer_destroy(lexer);
    return 1;
}

// Test x86 mnemonics
int test_x86_mnemonics() {
    const char* code = "mov add sub mul div jmp call ret";
    Lexer* lexer = lexer_create(code);
    
    TokenType expected[] = {TOKEN_MOV, TOKEN_ADD, TOKEN_SUB, TOKEN_MUL, TOKEN_DIV, 
                           TOKEN_JMP, TOKEN_CALL, TOKEN_RET, TOKEN_EOF};
    
    for (int i = 0; i < 9; i++) {
        Token token = lexer_next_token(lexer);
        if (token.type != expected[i]) {
            token_destroy(&token);
            lexer_destroy(lexer);
            return 0;
        }
        token_destroy(&token);
    }
    
    lexer_destroy(lexer);
    return 1;
}

// Test x86 registers
int test_x86_registers() {
    const char* code = "eax ebx ecx edx rax rbx r8 r15";
    Lexer* lexer = lexer_create(code);
    
    TokenType expected[] = {TOKEN_EAX, TOKEN_EBX, TOKEN_ECX, TOKEN_EDX, 
                           TOKEN_RAX, TOKEN_RBX, TOKEN_R8, TOKEN_R15, TOKEN_EOF};
    
    for (int i = 0; i < 9; i++) {
        Token token = lexer_next_token(lexer);
        if (token.type != expected[i]) {
            token_destroy(&token);
            lexer_destroy(lexer);
            return 0;
        }
        token_destroy(&token);
    }
    
    lexer_destroy(lexer);
    return 1;
}

// Test numeric literals
int test_numeric_literals() {
    const char* code = "42 3.14 0xFF 0b1010";
    Lexer* lexer = lexer_create(code);
    
    const char* expected_values[] = {"42", "3.14", "0xFF", "0b1010"};
    
    for (int i = 0; i < 4; i++) {
        Token token = lexer_next_token(lexer);
        if (!token_equals(&token, TOKEN_NUMBER, expected_values[i])) {
            token_destroy(&token);
            lexer_destroy(lexer);
            return 0;
        }
        token_destroy(&token);
    }
    
    lexer_destroy(lexer);
    return 1;
}

// Test string literals with escape sequences
int test_string_literals() {
    const char* code = "\"Hello\" \"World\\nTest\" \"Tab\\tSeparated\" \"Quote\\\"Inside\"";
    Lexer* lexer = lexer_create(code);
    
    const char* expected_values[] = {"Hello", "World\nTest", "Tab\tSeparated", "Quote\"Inside"};
    
    for (int i = 0; i < 4; i++) {
        Token token = lexer_next_token(lexer);
        if (!token_equals(&token, TOKEN_STRING, expected_values[i])) {
            token_destroy(&token);
            lexer_destroy(lexer);
            return 0;
        }
        token_destroy(&token);
    }
    
    lexer_destroy(lexer);
    return 1;
}

// Test operators and punctuation
int test_operators() {
    const char* code = ": ; , = -> ( ) { } [ ] + - * / .";
    Lexer* lexer = lexer_create(code);
    
    TokenType expected[] = {TOKEN_COLON, TOKEN_SEMICOLON, TOKEN_COMMA, TOKEN_EQUALS, TOKEN_ARROW,
                           TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_LBRACE, TOKEN_RBRACE,
                           TOKEN_LBRACKET, TOKEN_RBRACKET, TOKEN_PLUS, TOKEN_MINUS,
                           TOKEN_MULTIPLY, TOKEN_DIVIDE, TOKEN_DOT, TOKEN_EOF};
    
    for (int i = 0; i < 17; i++) {
        Token token = lexer_next_token(lexer);
        if (token.type != expected[i]) {
            token_destroy(&token);
            lexer_destroy(lexer);
            return 0;
        }
        token_destroy(&token);
    }
    
    lexer_destroy(lexer);
    return 1;
}

// Test position tracking
int test_position_tracking() {
    const char* code = "var\ncounter\n:\nint32";
    Lexer* lexer = lexer_create(code);
    
    Token token1 = lexer_next_token(lexer);
    if (token1.line != 1 || token1.column != 1) {
        token_destroy(&token1);
        lexer_destroy(lexer);
        return 0;
    }
    token_destroy(&token1);
    
    Token token2 = lexer_next_token(lexer);
    if (token2.line != 2 || token2.column != 1) {
        token_destroy(&token2);
        lexer_destroy(lexer);
        return 0;
    }
    token_destroy(&token2);
    
    Token token3 = lexer_next_token(lexer);
    if (token3.line != 3 || token3.column != 1) {
        token_destroy(&token3);
        lexer_destroy(lexer);
        return 0;
    }
    token_destroy(&token3);
    
    Token token4 = lexer_next_token(lexer);
    if (token4.line != 4 || token4.column != 1) {
        token_destroy(&token4);
        lexer_destroy(lexer);
        return 0;
    }
    token_destroy(&token4);
    
    lexer_destroy(lexer);
    return 1;
}

// Test error handling
int test_error_handling() {
    const char* code = "\"unterminated string";
    Lexer* lexer = lexer_create(code);
    
    Token token = lexer_next_token(lexer);
    int result = (token.type == TOKEN_ERROR && token.value && 
                  strstr(token.value, "Unterminated") != NULL);
    
    token_destroy(&token);
    lexer_destroy(lexer);
    return result;
}

// Test tokenize function
int test_tokenize_function() {
    const char* code = "var x = 42;";
    Lexer* lexer = lexer_create(code);
    
    size_t token_count;
    Token* tokens = lexer_tokenize(lexer, &token_count);
    
    if (!tokens || token_count != 6) {
        if (tokens) tokens_destroy(tokens, token_count);
        lexer_destroy(lexer);
        return 0;
    }
    
    TokenType expected[] = {TOKEN_VAR, TOKEN_IDENTIFIER, TOKEN_EQUALS, TOKEN_NUMBER, TOKEN_SEMICOLON, TOKEN_EOF};
    
    for (size_t i = 0; i < token_count; i++) {
        if (tokens[i].type != expected[i]) {
            tokens_destroy(tokens, token_count);
            lexer_destroy(lexer);
            return 0;
        }
    }
    
    tokens_destroy(tokens, token_count);
    lexer_destroy(lexer);
    return 1;
}

int main() {
    printf("Running comprehensive lexer tests...\n\n");
    
    TEST(keywords);
    TEST(type_keywords);
    TEST(x86_mnemonics);
    TEST(x86_registers);
    TEST(numeric_literals);
    TEST(string_literals);
    TEST(operators);
    TEST(position_tracking);
    TEST(error_handling);
    TEST(tokenize_function);
    
    printf("\n=== Test Results ===\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);
    printf("Success rate: %.1f%%\n", (float)tests_passed / tests_run * 100);
    
    return (tests_passed == tests_run) ? 0 : 1;
}