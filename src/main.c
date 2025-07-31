#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    CompilerOptions options = {0};
    options.input_filename = argv[1];
    options.output_filename = "output.s";
    options.debug_mode = 0;
    options.optimize = 0;

    // Parse command line arguments
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            options.output_filename = argv[++i];
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--debug") == 0) {
            options.debug_mode = 1;
        } else if (strcmp(argv[i], "-O") == 0 || strcmp(argv[i], "--optimize") == 0) {
            options.optimize = 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    return compile_file(options.input_filename, options.output_filename, &options);
}

int compile_file(const char* input_filename, const char* output_filename, CompilerOptions* options) {
    // Read input file
    char* source = read_file(input_filename);
    if (!source) {
        fprintf(stderr, "Error: Could not read file '%s'\n", input_filename);
        return 1;
    }

    // Initialize compiler components
    Lexer* lexer = lexer_create(source);
    Parser* parser = parser_create(lexer);
    SymbolTable* symbol_table = symbol_table_create();
    TypeChecker* type_checker = type_checker_create(symbol_table);
    RegisterAllocator* register_allocator = register_allocator_create();
    CodeGenerator* code_generator = code_generator_create(symbol_table, register_allocator);

    int result = 0;

    // Parse the source code
    ASTNode* program = parser_parse_program(parser);
    if (!program || parser->has_error) {
        fprintf(stderr, "Parse error: %s\n", parser->error_message ? parser->error_message : "Unknown error");
        result = 1;
        goto cleanup;
    }

    // Type checking
    if (!type_checker_check_program(type_checker, program)) {
        fprintf(stderr, "Type error: %s\n", type_checker->error_message ? type_checker->error_message : "Unknown error");
        result = 1;
        goto cleanup;
    }

    // Generate code
    if (!code_generator_generate_program(code_generator, program)) {
        fprintf(stderr, "Code generation error\n");
        result = 1;
        goto cleanup;
    }

    // Write output file
    FILE* output_file = fopen(output_filename, "w");
    if (!output_file) {
        fprintf(stderr, "Error: Could not create output file '%s'\n", output_filename);
        result = 1;
        goto cleanup;
    }

    char* generated_code = code_generator_get_output(code_generator);
    fprintf(output_file, "%s", generated_code);
    fclose(output_file);

    if (options->debug_mode) {
        printf("Successfully compiled '%s' to '%s'\n", input_filename, output_filename);
    }

cleanup:
    // Clean up resources
    if (program) ast_destroy_node(program);
    code_generator_destroy(code_generator);
    register_allocator_destroy(register_allocator);
    type_checker_destroy(type_checker);
    symbol_table_destroy(symbol_table);
    parser_destroy(parser);
    lexer_destroy(lexer);
    free(source);

    return result;
}

void print_usage(const char* program_name) {
    printf("Usage: %s <input.masm> [options]\n", program_name);
    printf("Options:\n");
    printf("  -o <file>     Output file (default: output.s)\n");
    printf("  -d, --debug   Enable debug output\n");
    printf("  -O, --optimize Enable optimizations\n");
    printf("  -h, --help    Show this help message\n");
}

char* read_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        return NULL;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Allocate buffer and read file
    char* buffer = malloc(size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    size_t bytes_read = fread(buffer, 1, size, file);
    buffer[bytes_read] = '\0';

    fclose(file);
    return buffer;
}