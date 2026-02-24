CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -O0 -D_GNU_SOURCE
SRCDIR = src
OBJDIR = obj
BINDIR = bin

# Source files
LEXER_SOURCES = $(SRCDIR)/lexer/lexer.c
PARSER_SOURCES = $(SRCDIR)/parser/parser.c $(SRCDIR)/parser/ast.c
SEMANTIC_SOURCES = $(SRCDIR)/semantic/symbol_table.c $(SRCDIR)/semantic/type_checker.c $(SRCDIR)/semantic/register_allocator.c
CODEGEN_SOURCES = $(SRCDIR)/codegen/code_generator.c
ERROR_SOURCES = $(SRCDIR)/error/error_reporter.c
DEBUG_SOURCES = $(SRCDIR)/debug/debug_info.c
MAIN_SOURCES = $(SRCDIR)/main.c

SOURCES = $(LEXER_SOURCES) $(PARSER_SOURCES) $(SEMANTIC_SOURCES) $(CODEGEN_SOURCES) $(ERROR_SOURCES) $(DEBUG_SOURCES) $(MAIN_SOURCES)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

TARGET = $(BINDIR)/methasm

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJECTS) | $(BINDIR)
	$(CC) $(OBJECTS) -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)/lexer $(OBJDIR)/parser $(OBJDIR)/semantic $(OBJDIR)/codegen $(OBJDIR)/error $(OBJDIR)/debug

$(BINDIR):
	mkdir -p $(BINDIR)

clean:
	rm -rf $(OBJDIR) $(BINDIR)

test: $(TARGET)
	@echo "Running tests..."
	@echo "Test framework not yet implemented"

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

.PHONY: debug
debug: CFLAGS += -DDEBUG
debug: $(TARGET)