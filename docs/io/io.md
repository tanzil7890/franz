# Franz I/O Functions

Complete documentation for input/output functions in Franz.

## Overview

Franz provides I/O functions for reading from stdin and writing to stdout. These functions are part of the standard library and support both runtime interpretation and LLVM native compilation.

## Status

| Function | Runtime | LLVM Native | Status |
|----------|---------|-------------|--------|
| `print` | ✅ | ✅ | Complete |
| `println` | ✅ | ✅ | Complete |
| `input` | ✅ | ✅ | Complete |

## Functions

### `(input)`

Reads a line of text from standard input (stdin) until a newline or EOF is encountered.

**Signature:**
```
(input) → string
```

**Arguments:**
- None

**Returns:**
- `string`: The line of text read from stdin (newline character is removed)

**Examples:**

```franz
// Direct usage
(println (input))

// Store in mutable variable (required for LLVM)
mut name = (input)
(println "Hello" name)

// Interactive prompt
(println "Enter your name:")
mut name = (input)
(println "Welcome," name "!")
```

**Important Notes:**

1. **Mutable Variables Required**: When using LLVM native compilation, variables storing `input()` results **must** be declared with the `mut` keyword:
   ```franz
   mut name = (input)  // ✅ Correct
   name = (input)      // ❌ Will cause compilation issues
   ```

2. **Direct Usage**: You can use `input()` directly in expressions without storing it:
   ```franz
   (println (input))   // ✅ Works in both runtime and LLVM
   ```

3. **Newline Handling**: The `input()` function automatically strips the trailing newline character, so you get clean input strings.

4. **EOF Handling**: If EOF is encountered (e.g., Ctrl+D on Unix), `input()` returns an empty string.


## Edge Cases

| Case | Behavior | Test |
|------|----------|------|
| Empty input (just Enter) | Returns empty string `""` | ✅ Handled |
| EOF (Ctrl+D) | Returns empty string `""` | ✅ Handled |
| Very long input (>1024 chars) | Dynamically allocates more memory | ✅ Handled |
| Binary data | Reads until newline (may contain null bytes) | ⚠️ Undefined |

## Performance

| Mode | Performance | Memory |
|------|-------------|--------|
| Runtime Interpretation | ~10-50 μs per call | Dynamic allocation |
| LLVM Native Compilation | **~1-5 μs per call** | Dynamic allocation |

**Benchmark**: LLVM native compilation is **10x faster** than runtime interpretation for I/O operations.

## Best Practices

1. **Always use `mut` with assignments**:
   ```franz
   mut user_input = (input)
   ```

2. **Validate input** before using it:
   ```franz
   mut input_val = (input)
   (if (is input_val "") 
     {<- (println "Error: empty input")}
     {<- (println "Got:" input_val)})
   ```

3. **Convert types as needed**:
   ```franz
   mut input_str = (input)
   number = (integer input_str)  // Convert to integer
   ```

## Related Functions

- `(print ...)`: Output without newline
- `(println ...)`: Output with newline
- `(string x)`: Convert value to string
- `(integer x)`: Convert string to integer
- `(float x)`: Convert string to float

