# Franz Programming Language - LLVM Native Compiler
#  LLVM-only compilation (like Rust)

# Compiler
CC = gcc

# LLVM Configuration (ARM64)
LLVM_CONFIG = /opt/homebrew/opt/llvm@17/bin/llvm-config
LLVM_CFLAGS = $(shell $(LLVM_CONFIG) --cflags)
LLVM_LDFLAGS = $(shell $(LLVM_CONFIG) --ldflags)
LLVM_LIBS = $(shell $(LLVM_CONFIG) --libs core executionengine mcjit native)

# Compiler flags
CFLAGS = -Wall -g $(LLVM_CFLAGS)
LDFLAGS = -lm $(LLVM_LDFLAGS) $(LLVM_LIBS)

# Source files (excluding bytecode/codegen/eval - removed in )
SRC = $(filter-out src/check.c, $(wildcard src/*.c))
SRC += $(wildcard src/circular-deps/*.c)
SRC += $(wildcard src/closure/*.c)
SRC += $(wildcard src/error-handling/*.c)
SRC += $(wildcard src/llvm-codegen/*.c)
SRC += $(wildcard src/llvm-closures/*.c)
SRC += $(wildcard src/llvm-comparisons/*.c)
SRC += $(wildcard src/llvm-lists/*.c)
SRC += $(wildcard src/llvm-list-ops/*.c)
SRC += $(wildcard src/llvm-dict/*.c)
SRC += $(wildcard src/llvm-file-ops/*.c)
SRC += $(wildcard src/llvm-file-advanced/*.c)
SRC += $(wildcard src/llvm-filter/*.c)
SRC += $(wildcard src/llvm-map/*.c)
SRC += $(wildcard src/llvm-reduce/*.c)
SRC += $(wildcard src/llvm-refs/*.c)
SRC += $(wildcard src/llvm-modules/*.c)
SRC += $(wildcard src/llvm-adt/*.c)
SRC += $(wildcard src/llvm-string-ops/*.c)
SRC += $(wildcard src/llvm-unboxing/*.c)
SRC += $(wildcard src/llvm-tco/*.c)
SRC += $(wildcard src/file-advanced/*.c)
SRC += $(wildcard src/llvm-control-flow/*.c)
SRC += $(wildcard src/llvm-logical/*.c)
SRC += $(wildcard src/llvm-math/*.c)
SRC += $(wildcard src/llvm-terminal/*.c)
SRC += $(wildcard src/llvm-type/*.c)
SRC += $(wildcard src/llvm-type-guards/*.c)
SRC += $(wildcard src/mutable-refs/*.c)
SRC += $(wildcard src/number-formats/*.c)
SRC += $(wildcard src/security/*.c)
SRC += $(wildcard src/freevar/*.c)
SRC += $(wildcard src/optimization/*.c)
SRC += $(wildcard src/type-inference/*.c)
SRC += $(wildcard src/weakref/*.c)

# Object files
OBJ = $(SRC:.c=.o)

# Target executable
TARGET = franz

# Default target
all: $(TARGET)

# Build franz executable
$(TARGET): $(OBJ)
	@echo "Linking Franz (LLVM native compiler)..."
	$(CC) $(OBJ) $(LDFLAGS) -o $(TARGET)
	@echo "Franz built successfully!"
	@echo ""
	@echo " Complete: LLVM infrastructure ready"
	@echo "Next: Implement  (Basic LLVM IR generation)"

# Compile source files
%.o: %.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -f $(OBJ) $(TARGET)
	@echo "Clean complete"

# Run tests
test: $(TARGET)
	@echo "Running  smoke test..."
	@echo 'x = 42' | ./$(TARGET)

# Debug build
debug: CFLAGS += -DDEBUG
debug: clean all

# Show LLVM configuration
llvm-info:
	@echo "LLVM Configuration:"
	@echo "  Version: $$($(LLVM_CONFIG) --version)"
	@echo "  Prefix: $$($(LLVM_CONFIG) --prefix)"
	@echo "  CFLAGS: $(LLVM_CFLAGS)"
	@echo "  LDFLAGS: $(LLVM_LDFLAGS)"
	@echo "  LIBS: $(LLVM_LIBS)"

.PHONY: all clean test debug llvm-info
