#include "parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

Parser* parser_create(Lexer* lexer) {
    Parser* parser = malloc(sizeof(Parser));
    if (!parser) return NULL;
    
    parser->lexer = lexer;
    parser->current_token = lexer_next_token(lexer);
    parser->peek_token = lexer_next_token(lexer);
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

void parser_advance(Parser* parser) {
    if (!parser || parser->current_token.type == TOKEN_EOF) return;
    
    token_destroy(&parser->current_token);
    parser->current_token = parser->peek_token;
    
    // Clear peek_token to avoid double-free
    parser->peek_token.type = TOKEN_EOF;
    parser->peek_token.value = NULL;
    parser->peek_token.line = 0;
    parser->peek_token.column = 0;
    
    // Get new peek token
    parser->peek_token = lexer_next_token(parser->lexer);
}

int parser_match(Parser* parser, TokenType type) {
    if (!parser) return 0;
    return parser->current_token.type == type;
}

int parser_expect(Parser* parser, TokenType type) {
    if (!parser) return 0;
    
    if (parser->current_token.type == type) {
        parser_advance(parser);
        return 1;
    }
    
    // Set error message
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg), "Expected token type %d, got %d at line %zu, column %zu", 
             type, parser->current_token.type, parser->current_token.line, parser->current_token.column);
    parser_set_error(parser, error_msg);
    return 0;
}

void parser_set_error(Parser* parser, const char* message) {
    if (!parser || !message) return;
    
    parser->has_error = 1;
    free(parser->error_message);
    parser->error_message = strdup(message);
}

void parser_recover_from_error(Parser* parser) {
    if (!parser) return;
    
    // Simple error recovery: skip tokens until we find a semicolon, brace, or EOF
    while (parser->current_token.type != TOKEN_EOF &&
           parser->current_token.type != TOKEN_SEMICOLON &&
           parser->current_token.type != TOKEN_RBRACE &&
           parser->current_token.type != TOKEN_LBRACE) {
        parser_advance(parser);
    }
    
    if (parser->current_token.type == TOKEN_SEMICOLON) {
        parser_advance(parser);
    }
    
    // Clear error state to continue parsing
    parser->has_error = 0;
    free(parser->error_message);
    parser->error_message = NULL;
}

int parser_get_operator_precedence(TokenType type) {
    switch (type) {
        case TOKEN_DOT:
            return 9;  // Member access (highest precedence)
        case TOKEN_MULTIPLY:
        case TOKEN_DIVIDE:
            return 7;  // Multiplicative
        case TOKEN_PLUS:
        case TOKEN_MINUS:
            return 6;  // Additive
        case TOKEN_EQUALS:
            return 2;  // Assignment (lowest precedence)
        default:
            return 0;  // Not a binary operator
    }
}

int parser_is_binary_operator(TokenType type) {
    switch (type) {
        case TOKEN_PLUS:
        case TOKEN_MINUS:
        case TOKEN_MULTIPLY:
        case TOKEN_DIVIDE:
        case TOKEN_EQUALS:
        case TOKEN_DOT:
            return 1;
        default:
            return 0;
    }
}

int parser_is_unary_operator(TokenType type) {
    switch (type) {
        case TOKEN_MINUS:
        case TOKEN_PLUS:
            return 1;
        default:
            return 0;
    }
}

ASTNode* parser_parse_program(Parser* parser) {
    if (!parser) return NULL;
    
    ASTNode* program = ast_create_program();
    if (!program) return NULL;
    
    Program* prog_data = (Program*)program->data;
    
    while (parser->current_token.type != TOKEN_EOF && !parser->has_error) {
        ASTNode* declaration = parser_parse_declaration(parser);
        if (declaration) {
            // Add to program's declarations array
            prog_data->declarations = realloc(prog_data->declarations, 
                                            (prog_data->declaration_count + 1) * sizeof(ASTNode*));
            if (prog_data->declarations) {
                prog_data->declarations[prog_data->declaration_count] = declaration;
                prog_data->declaration_count++;
                ast_add_child(program, declaration);
            }
        } else if (parser->has_error) {
            // Try to recover from error
            parser_recover_from_error(parser);
        }
    }
    
    return program;
}

ASTNode* parser_parse_declaration(Parser* parser) {
    if (!parser) return NULL;
    
    SourceLocation location = {parser->current_token.line, parser->current_token.column};
    
    switch (parser->current_token.type) {
        case TOKEN_VAR:
            return parser_parse_var_declaration(parser);
        case TOKEN_FUNCTION:
            return parser_parse_function_declaration(parser);
        case TOKEN_STRUCT:
            return parser_parse_struct_declaration(parser);
        default:
            // Try to parse as a statement instead
            return parser_parse_statement(parser);
    }
}

ASTNode* parser_parse_statement(Parser* parser) {
    if (!parser) return NULL;
    
    SourceLocation location = {parser->current_token.line, parser->current_token.column};
    
    switch (parser->current_token.type) {
        case TOKEN_RETURN:
            return parser_parse_return_statement(parser);
        case TOKEN_IF:
            return parser_parse_if_statement(parser);
        case TOKEN_WHILE:
            return parser_parse_while_statement(parser);
        case TOKEN_ASM:
            return parser_parse_inline_asm(parser);
        case TOKEN_IDENTIFIER:
            // Could be assignment or function call
            if (parser->peek_token.type == TOKEN_EQUALS) {
                return parser_parse_assignment(parser);
            } else if (parser->peek_token.type == TOKEN_LPAREN) {
                return parser_parse_expression(parser); // Function call
            }
            break;
        case TOKEN_LBRACE:
            return parser_parse_block(parser);
        default:
            // Try to parse as expression statement
            return parser_parse_expression(parser);
    }
    
    parser_set_error(parser, "Unexpected token in statement");
    return NULL;
}

ASTNode* parser_parse_expression(Parser* parser) {
    if (!parser) return NULL;
    
    return parser_parse_binary_expression(parser, 0);
}

int parser_is_identifier_like(TokenType type) {
    // Check if token can be used as an identifier in expression context
    return type == TOKEN_IDENTIFIER ||
           // x86 mnemonics can be used as function names
           (type >= TOKEN_MOV && type <= TOKEN_SYSCALL) ||
           // x86 registers can be used as identifiers in high-level context
           (type >= TOKEN_EAX && type <= TOKEN_R15);
}

int parser_is_type_keyword(TokenType type) {
    // Check if token is a built-in type keyword
    return (type >= TOKEN_INT8 && type <= TOKEN_STRING_TYPE);
}

ASTNode* parser_parse_primary_expression(Parser* parser) {
    if (!parser) return NULL;
    
    SourceLocation location = {parser->current_token.line, parser->current_token.column};
    
    if (parser_is_identifier_like(parser->current_token.type)) {
        char* name = strdup(parser->current_token.value);
        parser_advance(parser);
        return ast_create_identifier(name, location);
    }
    
    switch (parser->current_token.type) {
        case TOKEN_NUMBER: {
            // Check if it's a float or integer
            char* value = parser->current_token.value;
            ASTNode* result;
            
            if (strchr(value, '.')) {
                double float_val = atof(value);
                result = ast_create_float_literal(float_val, location);
            } else {
                long long int_val = atoll(value);
                result = ast_create_number_literal(int_val, location);
            }
            
            parser_advance(parser);
            return result;
        }
        case TOKEN_STRING: {
            char* value = strdup(parser->current_token.value);
            parser_advance(parser);
            return ast_create_string_literal(value, location);
        }
        case TOKEN_LPAREN: {
            parser_advance(parser); // consume '('
            ASTNode* expr = parser_parse_expression(parser);
            if (!parser_expect(parser, TOKEN_RPAREN)) {
                ast_destroy_node(expr);
                return NULL;
            }
            return expr;
        }
        case TOKEN_THIS: {
            parser_advance(parser);
            return ast_create_identifier("this", location);
        }
        default:
            parser_set_error(parser, "Expected primary expression");
            return NULL;
    }
}

ASTNode* parser_parse_unary_expression(Parser* parser) {
    if (!parser) return NULL;
    
    SourceLocation location = {parser->current_token.line, parser->current_token.column};
    
    if (parser_is_unary_operator(parser->current_token.type)) {
        char* operator = strdup(parser->current_token.value);
        parser_advance(parser);
        
        ASTNode* operand = parser_parse_unary_expression(parser);
        if (!operand) {
            free(operator);
            return NULL;
        }
        
        return ast_create_unary_expression(operator, operand, location);
    }
    
    return parser_parse_postfix_expression(parser);
}

ASTNode* parser_parse_postfix_expression(Parser* parser) {
    if (!parser) return NULL;
    
    ASTNode* expr = parser_parse_primary_expression(parser);
    if (!expr) return NULL;
    
    while (1) {
        SourceLocation location = {parser->current_token.line, parser->current_token.column};
        
        if (parser->current_token.type == TOKEN_LPAREN) {
            // Function call
            parser_advance(parser); // consume '('
            
            // Get function name from identifier
            if (expr->type != AST_IDENTIFIER) {
                parser_set_error(parser, "Invalid function call");
                ast_destroy_node(expr);
                return NULL;
            }
            
            Identifier* id_data = (Identifier*)expr->data;
            char* func_name = strdup(id_data->name);
            
            // Parse arguments
            ASTNode** arguments = NULL;
            size_t arg_count = 0;
            
            if (parser->current_token.type != TOKEN_RPAREN) {
                do {
                    ASTNode* arg = parser_parse_expression(parser);
                    if (!arg) break;
                    
                    arguments = realloc(arguments, (arg_count + 1) * sizeof(ASTNode*));
                    arguments[arg_count] = arg;
                    arg_count++;
                    
                    if (parser->current_token.type == TOKEN_COMMA) {
                        parser_advance(parser);
                        // Continue parsing next argument
                    } else if (parser->current_token.type == TOKEN_RPAREN) {
                        // End of argument list
                        break;
                    } else {
                        // Unexpected token
                        parser_set_error(parser, "Expected ',' or ')' in argument list");
                        break;
                    }
                } while (1);
            }
            
            if (!parser_expect(parser, TOKEN_RPAREN)) {
                // Clean up on error
                for (size_t i = 0; i < arg_count; i++) {
                    ast_destroy_node(arguments[i]);
                }
                free(arguments);
                free(func_name);
                ast_destroy_node(expr);
                return NULL;
            }
            
            ast_destroy_node(expr);
            expr = ast_create_call_expression(func_name, arguments, arg_count, location);
            free(func_name);
            
        } else if (parser->current_token.type == TOKEN_DOT) {
            // Member access
            parser_advance(parser); // consume '.'
            
            if (parser->current_token.type != TOKEN_IDENTIFIER) {
                parser_set_error(parser, "Expected member name after '.'");
                ast_destroy_node(expr);
                return NULL;
            }
            
            char* member = strdup(parser->current_token.value);
            parser_advance(parser);
            
            expr = ast_create_member_access(expr, member, location);
            free(member);
            
        } else {
            break;
        }
    }
    
    return expr;
}

ASTNode* parser_parse_binary_expression(Parser* parser, int min_precedence) {
    if (!parser) return NULL;
    
    ASTNode* left = parser_parse_unary_expression(parser);
    if (!left) return NULL;
    
    while (parser_is_binary_operator(parser->current_token.type)) {
        int precedence = parser_get_operator_precedence(parser->current_token.type);
        if (precedence < min_precedence) break;
        
        SourceLocation location = {parser->current_token.line, parser->current_token.column};
        char* operator = strdup(parser->current_token.value);
        parser_advance(parser);
        
        ASTNode* right = parser_parse_binary_expression(parser, precedence + 1);
        if (!right) {
            free(operator);
            ast_destroy_node(left);
            return NULL;
        }
        
        left = ast_create_binary_expression(left, operator, right, location);
        free(operator);
    }
    
    return left;
}

// Placeholder implementations for specific parsing functions
// These will be implemented in subsequent tasks

ASTNode* parser_parse_var_declaration(Parser* parser) {
    if (!parser) return NULL;
    
    SourceLocation location = {parser->current_token.line, parser->current_token.column};
    
    // Expect 'var' keyword
    if (!parser_expect(parser, TOKEN_VAR)) {
        return NULL;
    }
    
    // Expect identifier
    if (!parser_is_identifier_like(parser->current_token.type)) {
        parser_set_error(parser, "Expected identifier after 'var'");
        return NULL;
    }
    
    char* var_name = strdup(parser->current_token.value);
    parser_advance(parser);
    
    char* type_name = NULL;
    ASTNode* initializer = NULL;
    
    // Optional type annotation: ': type'
    if (parser->current_token.type == TOKEN_COLON) {
        parser_advance(parser); // consume ':'
        
        if (!parser_is_type_keyword(parser->current_token.type) && 
            !parser_is_identifier_like(parser->current_token.type)) {
            parser_set_error(parser, "Expected type after ':'");
            free(var_name);
            return NULL;
        }
        
        type_name = strdup(parser->current_token.value);
        parser_advance(parser);
    }
    
    // Optional initializer: '= expression'
    if (parser->current_token.type == TOKEN_EQUALS) {
        parser_advance(parser); // consume '='
        
        initializer = parser_parse_expression(parser);
        if (!initializer) {
            free(var_name);
            free(type_name);
            return NULL;
        }
    }
    
    // For type inference, if no type is specified but there's an initializer,
    // we'll leave type_name as NULL and let the semantic analyzer infer it
    if (!type_name && !initializer) {
        parser_set_error(parser, "Variable declaration must have either a type annotation or an initializer");
        free(var_name);
        return NULL;
    }
    
    // Expect semicolon (optional for now, but good practice)
    if (parser->current_token.type == TOKEN_SEMICOLON) {
        parser_advance(parser);
    }
    
    ASTNode* var_decl = ast_create_var_declaration(var_name, type_name, initializer, location);
    
    free(var_name);
    free(type_name);
    
    return var_decl;
}

ASTNode* parser_parse_function_declaration(Parser* parser) {
    if (!parser) return NULL;
    
    SourceLocation location = {parser->current_token.line, parser->current_token.column};
    
    // Expect 'function' keyword
    if (!parser_expect(parser, TOKEN_FUNCTION)) {
        return NULL;
    }
    
    // Expect function name
    if (!parser_is_identifier_like(parser->current_token.type)) {
        parser_set_error(parser, "Expected function name after 'function'");
        return NULL;
    }
    
    char* func_name = strdup(parser->current_token.value);
    parser_advance(parser);
    
    // Expect '('
    if (!parser_expect(parser, TOKEN_LPAREN)) {
        free(func_name);
        return NULL;
    }
    
    // Parse parameter list
    char** param_names = NULL;
    char** param_types = NULL;
    size_t param_count = 0;
    
    if (parser->current_token.type != TOKEN_RPAREN) {
        do {
            // Parse parameter: name : type
            if (!parser_is_identifier_like(parser->current_token.type)) {
                parser_set_error(parser, "Expected parameter name");
                // Clean up
                for (size_t i = 0; i < param_count; i++) {
                    free(param_names[i]);
                    free(param_types[i]);
                }
                free(param_names);
                free(param_types);
                free(func_name);
                return NULL;
            }
            
            // Reallocate arrays
            param_names = realloc(param_names, (param_count + 1) * sizeof(char*));
            param_types = realloc(param_types, (param_count + 1) * sizeof(char*));
            
            param_names[param_count] = strdup(parser->current_token.value);
            parser_advance(parser);
            
            // Expect ':'
            if (!parser_expect(parser, TOKEN_COLON)) {
                // Clean up
                for (size_t i = 0; i <= param_count; i++) {
                    free(param_names[i]);
                    if (i < param_count) free(param_types[i]);
                }
                free(param_names);
                free(param_types);
                free(func_name);
                return NULL;
            }
            
            // Parse parameter type
            if (!parser_is_type_keyword(parser->current_token.type) && 
                !parser_is_identifier_like(parser->current_token.type)) {
                parser_set_error(parser, "Expected parameter type");
                // Clean up
                for (size_t i = 0; i <= param_count; i++) {
                    free(param_names[i]);
                    if (i < param_count) free(param_types[i]);
                }
                free(param_names);
                free(param_types);
                free(func_name);
                return NULL;
            }
            
            param_types[param_count] = strdup(parser->current_token.value);
            parser_advance(parser);
            param_count++;
            
            // Check for comma (more parameters) or end of parameter list
            if (parser->current_token.type == TOKEN_COMMA) {
                parser_advance(parser);
            } else if (parser->current_token.type == TOKEN_RPAREN) {
                break;
            } else {
                parser_set_error(parser, "Expected ',' or ')' in parameter list");
                // Clean up
                for (size_t i = 0; i < param_count; i++) {
                    free(param_names[i]);
                    free(param_types[i]);
                }
                free(param_names);
                free(param_types);
                free(func_name);
                return NULL;
            }
        } while (1);
    }
    
    // Expect ')'
    if (!parser_expect(parser, TOKEN_RPAREN)) {
        // Clean up
        for (size_t i = 0; i < param_count; i++) {
            free(param_names[i]);
            free(param_types[i]);
        }
        free(param_names);
        free(param_types);
        free(func_name);
        return NULL;
    }
    
    // Optional return type: '-> type'
    char* return_type = NULL;
    if (parser->current_token.type == TOKEN_ARROW) {
        parser_advance(parser); // consume '->'
        
        if (!parser_is_type_keyword(parser->current_token.type) && 
            !parser_is_identifier_like(parser->current_token.type)) {
            parser_set_error(parser, "Expected return type after '->'");
            // Clean up
            for (size_t i = 0; i < param_count; i++) {
                free(param_names[i]);
                free(param_types[i]);
            }
            free(param_names);
            free(param_types);
            free(func_name);
            return NULL;
        }
        
        return_type = strdup(parser->current_token.value);
        parser_advance(parser);
    }
    
    // Parse function body (block)
    ASTNode* body = NULL;
    if (parser->current_token.type == TOKEN_LBRACE) {
        body = parser_parse_block(parser);
        if (!body && parser->has_error) {
            // Clean up
            for (size_t i = 0; i < param_count; i++) {
                free(param_names[i]);
                free(param_types[i]);
            }
            free(param_names);
            free(param_types);
            free(func_name);
            free(return_type);
            return NULL;
        }
    } else {
        parser_set_error(parser, "Expected function body ('{')");
        // Clean up
        for (size_t i = 0; i < param_count; i++) {
            free(param_names[i]);
            free(param_types[i]);
        }
        free(param_names);
        free(param_types);
        free(func_name);
        free(return_type);
        return NULL;
    }
    
    ASTNode* func_decl = ast_create_function_declaration(func_name, param_names, param_types, 
                                                        param_count, return_type, body, location);
    
    // Clean up temporary strings
    free(func_name);
    free(return_type);
    for (size_t i = 0; i < param_count; i++) {
        free(param_names[i]);
        free(param_types[i]);
    }
    free(param_names);
    free(param_types);
    
    return func_decl;
}

ASTNode* parser_parse_struct_declaration(Parser* parser) {
    if (!parser) return NULL;
    
    SourceLocation location = {parser->current_token.line, parser->current_token.column};
    
    // Expect 'struct' keyword
    if (!parser_expect(parser, TOKEN_STRUCT)) {
        return NULL;
    }
    
    // Expect struct name
    if (!parser_is_identifier_like(parser->current_token.type)) {
        parser_set_error(parser, "Expected struct name after 'struct'");
        return NULL;
    }
    
    char* struct_name = strdup(parser->current_token.value);
    parser_advance(parser);
    
    // Expect '{'
    if (!parser_expect(parser, TOKEN_LBRACE)) {
        free(struct_name);
        return NULL;
    }
    
    // Parse fields and methods
    char** field_names = NULL;
    char** field_types = NULL;
    size_t field_count = 0;
    ASTNode** methods = NULL;
    size_t method_count = 0;
    
    while (parser->current_token.type != TOKEN_RBRACE && 
           parser->current_token.type != TOKEN_EOF && 
           !parser->has_error) {
        
        if (parser->current_token.type == TOKEN_METHOD) {
            // Parse method declaration
            ASTNode* method = parser_parse_method_declaration(parser);
            if (method) {
                methods = realloc(methods, (method_count + 1) * sizeof(ASTNode*));
                methods[method_count] = method;
                method_count++;
            } else if (parser->has_error) {
                // Clean up and return
                for (size_t i = 0; i < field_count; i++) {
                    free(field_names[i]);
                    free(field_types[i]);
                }
                free(field_names);
                free(field_types);
                for (size_t i = 0; i < method_count; i++) {
                    ast_destroy_node(methods[i]);
                }
                free(methods);
                free(struct_name);
                return NULL;
            }
        } else {
            // Parse field declaration: name: type;
            if (!parser_is_identifier_like(parser->current_token.type)) {
                parser_set_error(parser, "Expected field name or method declaration");
                // Clean up
                for (size_t i = 0; i < field_count; i++) {
                    free(field_names[i]);
                    free(field_types[i]);
                }
                free(field_names);
                free(field_types);
                for (size_t i = 0; i < method_count; i++) {
                    ast_destroy_node(methods[i]);
                }
                free(methods);
                free(struct_name);
                return NULL;
            }
            
            // Reallocate field arrays
            field_names = realloc(field_names, (field_count + 1) * sizeof(char*));
            field_types = realloc(field_types, (field_count + 1) * sizeof(char*));
            
            field_names[field_count] = strdup(parser->current_token.value);
            parser_advance(parser);
            
            // Expect ':'
            if (!parser_expect(parser, TOKEN_COLON)) {
                // Clean up
                for (size_t i = 0; i <= field_count; i++) {
                    free(field_names[i]);
                    if (i < field_count) free(field_types[i]);
                }
                free(field_names);
                free(field_types);
                for (size_t i = 0; i < method_count; i++) {
                    ast_destroy_node(methods[i]);
                }
                free(methods);
                free(struct_name);
                return NULL;
            }
            
            // Parse field type
            if (!parser_is_type_keyword(parser->current_token.type) && 
                !parser_is_identifier_like(parser->current_token.type)) {
                parser_set_error(parser, "Expected field type");
                // Clean up
                for (size_t i = 0; i <= field_count; i++) {
                    free(field_names[i]);
                    if (i < field_count) free(field_types[i]);
                }
                free(field_names);
                free(field_types);
                for (size_t i = 0; i < method_count; i++) {
                    ast_destroy_node(methods[i]);
                }
                free(methods);
                free(struct_name);
                return NULL;
            }
            
            field_types[field_count] = strdup(parser->current_token.value);
            parser_advance(parser);
            field_count++;
            
            // Expect ';'
            if (parser->current_token.type == TOKEN_SEMICOLON) {
                parser_advance(parser);
            }
        }
    }
    
    // Expect '}'
    if (!parser_expect(parser, TOKEN_RBRACE)) {
        // Clean up
        for (size_t i = 0; i < field_count; i++) {
            free(field_names[i]);
            free(field_types[i]);
        }
        free(field_names);
        free(field_types);
        for (size_t i = 0; i < method_count; i++) {
            ast_destroy_node(methods[i]);
        }
        free(methods);
        free(struct_name);
        return NULL;
    }
    
    ASTNode* struct_decl = ast_create_struct_declaration(struct_name, field_names, field_types, 
                                                        field_count, methods, method_count, location);
    
    // Clean up temporary strings
    free(struct_name);
    for (size_t i = 0; i < field_count; i++) {
        free(field_names[i]);
        free(field_types[i]);
    }
    free(field_names);
    free(field_types);
    // Note: methods array is now owned by the AST node, don't free it
    
    return struct_decl;
}

ASTNode* parser_parse_inline_asm(Parser* parser) {
    if (!parser) return NULL;
    
    SourceLocation location = {parser->current_token.line, parser->current_token.column};
    
    // Expect 'asm' keyword
    if (!parser_expect(parser, TOKEN_ASM)) {
        return NULL;
    }
    
    // Expect '{'
    if (!parser_expect(parser, TOKEN_LBRACE)) {
        return NULL;
    }
    
    // Collect all tokens until we hit the closing '}'
    // We need to preserve the exact assembly code as a string
    char* assembly_code = malloc(1024); // Start with 1KB buffer
    size_t buffer_size = 1024;
    size_t code_length = 0;
    
    if (!assembly_code) {
        parser_set_error(parser, "Memory allocation failed");
        return NULL;
    }
    
    assembly_code[0] = '\0';
    
    while (parser->current_token.type != TOKEN_RBRACE && 
           parser->current_token.type != TOKEN_EOF && 
           !parser->has_error) {
        
        // Add the token value to our assembly code string
        if (parser->current_token.value) {
            size_t token_len = strlen(parser->current_token.value);
            
            // Check if we need to expand the buffer
            if (code_length + token_len + 2 >= buffer_size) { // +2 for space and null terminator
                buffer_size *= 2;
                assembly_code = realloc(assembly_code, buffer_size);
                if (!assembly_code) {
                    parser_set_error(parser, "Memory allocation failed");
                    return NULL;
                }
            }
            
            // Add a space before the token if we already have content
            if (code_length > 0) {
                assembly_code[code_length] = ' ';
                code_length++;
            }
            
            // Copy the token value
            strcpy(assembly_code + code_length, parser->current_token.value);
            code_length += token_len;
        }
        
        parser_advance(parser);
    }
    
    // Expect '}'
    if (!parser_expect(parser, TOKEN_RBRACE)) {
        free(assembly_code);
        return NULL;
    }
    
    // Create inline assembly node
    ASTNode* inline_asm = ast_create_inline_asm(assembly_code, location);
    
    free(assembly_code);
    return inline_asm;
}

ASTNode* parser_parse_assignment(Parser* parser) {
    if (!parser) return NULL;
    
    SourceLocation location = {parser->current_token.line, parser->current_token.column};
    
    if (parser->current_token.type != TOKEN_IDENTIFIER) {
        parser_set_error(parser, "Expected identifier in assignment");
        return NULL;
    }
    
    char* var_name = strdup(parser->current_token.value);
    parser_advance(parser);
    
    if (!parser_expect(parser, TOKEN_EQUALS)) {
        free(var_name);
        return NULL;
    }
    
    ASTNode* value = parser_parse_expression(parser);
    if (!value) {
        free(var_name);
        return NULL;
    }
    
    ASTNode* assignment = ast_create_assignment(var_name, value, location);
    free(var_name);
    return assignment;
}

ASTNode* parser_parse_return_statement(Parser* parser) {
    if (!parser) return NULL;
    
    SourceLocation location = {parser->current_token.line, parser->current_token.column};
    parser_advance(parser); // consume 'return'
    
    ASTNode* value = NULL;
    if (parser->current_token.type != TOKEN_SEMICOLON) {
        value = parser_parse_expression(parser);
    }
    
    ASTNode* return_stmt = ast_create_node(AST_RETURN_STATEMENT, location);
    if (return_stmt && value) {
        ReturnStatement* ret_data = malloc(sizeof(ReturnStatement));
        ret_data->value = value;
        return_stmt->data = ret_data;
        ast_add_child(return_stmt, value);
    }
    
    return return_stmt;
}

ASTNode* parser_parse_if_statement(Parser* parser) {
    // Placeholder implementation
    parser_set_error(parser, "If statement parsing not yet implemented");
    return NULL;
}

ASTNode* parser_parse_while_statement(Parser* parser) {
    // Placeholder implementation
    parser_set_error(parser, "While statement parsing not yet implemented");
    return NULL;
}

ASTNode* parser_parse_block(Parser* parser) {
    if (!parser) return NULL;
    
    SourceLocation location = {parser->current_token.line, parser->current_token.column};
    
    // Expect '{'
    if (!parser_expect(parser, TOKEN_LBRACE)) {
        return NULL;
    }
    
    // Create a block node (we'll use a program node to hold statements)
    ASTNode* block = ast_create_program();
    if (!block) return NULL;
    
    Program* block_data = (Program*)block->data;
    
    // Parse statements until we hit '}'
    while (parser->current_token.type != TOKEN_RBRACE && 
           parser->current_token.type != TOKEN_EOF && 
           !parser->has_error) {
        
        ASTNode* stmt = parser_parse_statement(parser);
        if (stmt) {
            // Add to block's statements array
            block_data->declarations = realloc(block_data->declarations, 
                                             (block_data->declaration_count + 1) * sizeof(ASTNode*));
            if (block_data->declarations) {
                block_data->declarations[block_data->declaration_count] = stmt;
                block_data->declaration_count++;
                ast_add_child(block, stmt);
            }
        } else if (parser->has_error) {
            // Try to recover from error
            parser_recover_from_error(parser);
        }
    }
    
    // Expect '}'
    if (!parser_expect(parser, TOKEN_RBRACE)) {
        ast_destroy_node(block);
        return NULL;
    }
    
    return block;
}

ASTNode* parser_parse_method_declaration(Parser* parser) {
    if (!parser) return NULL;
    
    SourceLocation location = {parser->current_token.line, parser->current_token.column};
    
    // Expect 'method' keyword
    if (!parser_expect(parser, TOKEN_METHOD)) {
        return NULL;
    }
    
    // Expect method name
    if (!parser_is_identifier_like(parser->current_token.type)) {
        parser_set_error(parser, "Expected method name after 'method'");
        return NULL;
    }
    
    char* method_name = strdup(parser->current_token.value);
    parser_advance(parser);
    
    // Expect '('
    if (!parser_expect(parser, TOKEN_LPAREN)) {
        free(method_name);
        return NULL;
    }
    
    // Parse parameter list (similar to function declaration)
    char** param_names = NULL;
    char** param_types = NULL;
    size_t param_count = 0;
    
    if (parser->current_token.type != TOKEN_RPAREN) {
        do {
            // Parse parameter: name : type
            if (!parser_is_identifier_like(parser->current_token.type)) {
                parser_set_error(parser, "Expected parameter name");
                // Clean up
                for (size_t i = 0; i < param_count; i++) {
                    free(param_names[i]);
                    free(param_types[i]);
                }
                free(param_names);
                free(param_types);
                free(method_name);
                return NULL;
            }
            
            // Reallocate arrays
            param_names = realloc(param_names, (param_count + 1) * sizeof(char*));
            param_types = realloc(param_types, (param_count + 1) * sizeof(char*));
            
            param_names[param_count] = strdup(parser->current_token.value);
            parser_advance(parser);
            
            // Expect ':'
            if (!parser_expect(parser, TOKEN_COLON)) {
                // Clean up
                for (size_t i = 0; i <= param_count; i++) {
                    free(param_names[i]);
                    if (i < param_count) free(param_types[i]);
                }
                free(param_names);
                free(param_types);
                free(method_name);
                return NULL;
            }
            
            // Parse parameter type
            if (!parser_is_type_keyword(parser->current_token.type) && 
                !parser_is_identifier_like(parser->current_token.type)) {
                parser_set_error(parser, "Expected parameter type");
                // Clean up
                for (size_t i = 0; i <= param_count; i++) {
                    free(param_names[i]);
                    if (i < param_count) free(param_types[i]);
                }
                free(param_names);
                free(param_types);
                free(method_name);
                return NULL;
            }
            
            param_types[param_count] = strdup(parser->current_token.value);
            parser_advance(parser);
            param_count++;
            
            // Check for comma (more parameters) or end of parameter list
            if (parser->current_token.type == TOKEN_COMMA) {
                parser_advance(parser);
            } else if (parser->current_token.type == TOKEN_RPAREN) {
                break;
            } else {
                parser_set_error(parser, "Expected ',' or ')' in parameter list");
                // Clean up
                for (size_t i = 0; i < param_count; i++) {
                    free(param_names[i]);
                    free(param_types[i]);
                }
                free(param_names);
                free(param_types);
                free(method_name);
                return NULL;
            }
        } while (1);
    }
    
    // Expect ')'
    if (!parser_expect(parser, TOKEN_RPAREN)) {
        // Clean up
        for (size_t i = 0; i < param_count; i++) {
            free(param_names[i]);
            free(param_types[i]);
        }
        free(param_names);
        free(param_types);
        free(method_name);
        return NULL;
    }
    
    // Optional return type: '-> type'
    char* return_type = NULL;
    if (parser->current_token.type == TOKEN_ARROW) {
        parser_advance(parser); // consume '->'
        
        if (!parser_is_type_keyword(parser->current_token.type) && 
            !parser_is_identifier_like(parser->current_token.type)) {
            parser_set_error(parser, "Expected return type after '->'");
            // Clean up
            for (size_t i = 0; i < param_count; i++) {
                free(param_names[i]);
                free(param_types[i]);
            }
            free(param_names);
            free(param_types);
            free(method_name);
            return NULL;
        }
        
        return_type = strdup(parser->current_token.value);
        parser_advance(parser);
    }
    
    // Parse method body (block)
    ASTNode* body = NULL;
    if (parser->current_token.type == TOKEN_LBRACE) {
        body = parser_parse_block(parser);
        if (!body && parser->has_error) {
            // Clean up
            for (size_t i = 0; i < param_count; i++) {
                free(param_names[i]);
                free(param_types[i]);
            }
            free(param_names);
            free(param_types);
            free(method_name);
            free(return_type);
            return NULL;
        }
    } else {
        parser_set_error(parser, "Expected method body ('{')");
        // Clean up
        for (size_t i = 0; i < param_count; i++) {
            free(param_names[i]);
            free(param_types[i]);
        }
        free(param_names);
        free(param_types);
        free(method_name);
        free(return_type);
        return NULL;
    }
    
    // Create method declaration node (we'll use AST_METHOD_DECLARATION type)
    ASTNode* method_decl = ast_create_node(AST_METHOD_DECLARATION, location);
    if (!method_decl) {
        // Clean up
        for (size_t i = 0; i < param_count; i++) {
            free(param_names[i]);
            free(param_types[i]);
        }
        free(param_names);
        free(param_types);
        free(method_name);
        free(return_type);
        if (body) ast_destroy_node(body);
        return NULL;
    }
    
    // Create method declaration data (reuse FunctionDeclaration structure)
    FunctionDeclaration* method_data = malloc(sizeof(FunctionDeclaration));
    if (!method_data) {
        // Clean up
        for (size_t i = 0; i < param_count; i++) {
            free(param_names[i]);
            free(param_types[i]);
        }
        free(param_names);
        free(param_types);
        free(method_name);
        free(return_type);
        if (body) ast_destroy_node(body);
        ast_destroy_node(method_decl);
        return NULL;
    }
    
    method_data->name = strdup(method_name);
    method_data->return_type = return_type ? strdup(return_type) : NULL;
    method_data->parameter_count = param_count;
    method_data->body = body;
    
    if (param_count > 0) {
        method_data->parameter_names = malloc(param_count * sizeof(char*));
        method_data->parameter_types = malloc(param_count * sizeof(char*));
        
        for (size_t i = 0; i < param_count; i++) {
            method_data->parameter_names[i] = strdup(param_names[i]);
            method_data->parameter_types[i] = strdup(param_types[i]);
        }
    } else {
        method_data->parameter_names = NULL;
        method_data->parameter_types = NULL;
    }
    
    method_decl->data = method_data;
    
    if (body) {
        ast_add_child(method_decl, body);
    }
    
    // Clean up temporary strings
    free(method_name);
    free(return_type);
    for (size_t i = 0; i < param_count; i++) {
        free(param_names[i]);
        free(param_types[i]);
    }
    free(param_names);
    free(param_types);
    
    return method_decl;
}