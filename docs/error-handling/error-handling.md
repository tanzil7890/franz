# Error Handling in Franz

## Overview

Franz provides industry-standard error handling through three built-in functions: `try`, `catch`, and `error`. This system allows you to gracefully handle errors without crashing your program, similar to try/catch in JavaScript or try/except in Python.


## Features

- **Graceful error recovery**: Catch errors and provide fallback values
- **Custom error messages**: Throw errors with descriptive messages
- **Nested try blocks**: Support for complex error handling scenarios
- **No process termination**: Errors inside try blocks don't crash the program
- **Error message access**: Error handlers receive the error message as an argument

## API Reference

### `try`

Executes a block of code and catches any errors that occur.

**Syntax:**
```franz
(try {try-block} {error-param -> handler-block})
```

**Parameters:**
- `try-block`: Function containing code that might throw an error
- `error-param -> handler-block`: Error handler function that receives the error message

**Returns:** Value from try-block if successful, or value from handler-block if error occurs

**Example:**
```franz
result = (try {
  (error "Something went wrong!")
} {err ->
  <- "Error was caught!"
})
(println result)  // Output: Error was caught!
```

### `catch`

Similar to `try`, but uses a fallback value instead of an error handler function.

**Syntax:**
```franz
(catch {try-block} {fallback})
```

**Parameters:**
- `try-block`: Function containing code that might throw an error
- `fallback`: Value or function to use if error occurs

**Returns:** Value from try-block if successful, or fallback value if error occurs

**Example:**
```franz
result = (catch {
  (error "Failed!")
} {
  <- 42
})
(println result)  // Output: 42
```

### `error`

Throws an error with a custom message.

**Syntax:**
```franz
(error "message")
```

**Parameters:**
- `message`: String describing the error

**Behavior:**
- If inside a try/catch block: Sets error state, handler will be called
- If outside try/catch: Prints error and exits program

**Example:**
```franz
(error "Invalid input!")  // Program exits with: Error @ Line X: Invalid input!
```

## Usage Examples

### Basic Error Handling

```franz
// Catch an error and provide fallback
result = (try {
  (error "Something failed")
} {err ->
  <- "Recovered successfully"
})
(println result)  // Output: Recovered successfully
```

### Access Error Message

```franz
// Use the error message in handler
result = (try {
  (error "Database connection failed")
} {err ->
  <- err  // Return the error message
})
(println result)  // Output: Database connection failed
```

### Nested Try Blocks

```franz
outer = (try {
  inner = (try {
    (error "Inner error")
  } {err ->
    <- "Inner caught"
  })
  <- inner
} {err ->
  <- "Outer caught"
})
(println outer)  // Output: Inner caught
```

### Try vs Catch

```franz
// try - with error handler function
x = (try { <- 10 } {e -> <- 0})  // x = 10

// catch - with fallback value
y = (catch { (error "fail") } { <- 20 })  // y = 20
```

### Consecutive Error Handling

```franz
r1 = (try { (error "first") } {e -> <- 1})
r2 = (try { (error "second") } {e -> <- 2})
(println r1 r2)  // Output: 1 2
```

### Complex Error Recovery

```franz
result = (try {
  x = (add 10 20)
  y = (multiply x 2)
  (error "Midway error!")
  z = (add x y)  // Never executed
  <- z
} {err ->
  <- 999  // Fallback value
})
(println result)  // Output: 999
```

### Safe Division

```franz
safe_divide = {a b ->
  <- (catch {
    (if (is b 0) {
      (error "Division by zero")
    } {
      <- (divide a b)
    })
  } {
    <- 0  // Fallback to 0 on error
  })
}

(println (safe_divide 10 2))  // Output: 5.0
(println (safe_divide 10 0))  // Output: 0
```

## Error Types

Franz supports multiple error types internally:
- `ERROR_RUNTIME` - Runtime errors
- `ERROR_SYNTAX` - Syntax errors
- `ERROR_TYPE` - Type errors
- `ERROR_CUSTOM` - User-defined errors (via `error` function)
- `ERROR_FILE` - File operation errors
- `ERROR_IMPORT` - Import/module errors
- `ERROR_MEMORY` - Memory allocation errors

## Implementation Details

### Flag-Based Error Propagation

Franz uses a flag-based error handling system (not setjmp/longjmp) for:
- **Reliability**: No stack corruption or undefined behavior
- **Portability**: Works across all platforms
- **Simplicity**: Easy to understand and debug
- **Industry standard**: Similar to Python, JavaScript, Java

### Try Depth Tracking

The error system tracks nesting level with `try_depth`:
- Incremented when entering a try block
- Decremented when exiting a try block
- If `try_depth == 0`, errors cause program exit
- If `try_depth > 0`, errors are caught by innermost try block

### Memory Management

Error messages are:
- Allocated with `strdup` and stored in global error state
- Freed automatically when new error occurs or error is cleared
- Passed to error handlers as Franz string objects
- Managed by Franz's garbage collector (no manual free needed)

## Best Practices

1. **Use descriptive error messages**: Help users understand what went wrong
   ```franz
   (error "Invalid input: expected positive number")
   ```

2. **Prefer `catch` for simple fallbacks**: When you just need a default value
   ```franz
   value = (catch { (risky_operation) } { <- 0 })
   ```

3. **Use `try` when you need the error message**: For logging or custom handling
   ```franz
   result = (try {
     (operation)
   } {err ->
     (println "Error occurred:" err)
     <- fallback_value
   })
   ```

4. **Don't nest too deeply**: Keep error handling simple and readable

5. **Clean up resources in error handlers**: If you allocate resources, free them in the error handler

## Comparison with Other Languages

### JavaScript
```javascript
// JavaScript
try {
  throw new Error("Something failed");
} catch (err) {
  console.log(err.message);
}
```

### Franz
```franz
// Franz
(try {
  (error "Something failed")
} {err ->
  (println err)
})
```

### Python
```python
# Python
try:
    raise Exception("Something failed")
except Exception as err:
    print(err)
```

## Testing

Comprehensive test suite: [test/error-handling/comprehensive-test.franz](../../test/error-handling/comprehensive-test.franz)

Tests cover:
- ✅ Basic try/catch with custom errors
- ✅ Try block succeeds (no error)
- ✅ Catch with fallback value
- ✅ Nested try blocks
- ✅ Error message propagation
- ✅ Consecutive try blocks
- ✅ Try/catch mixing
- ✅ Errors in complex expressions
- ✅ Multiple nested catches

## Files

- Implementation: [src/error-handling/error_handler_v2.c](../../src/error-handling/error_handler_v2.c)
- Header: [src/error-handling/error_handler_v2.h](../../src/error-handling/error_handler_v2.h)
- Standard Library: [src/stdlib.c](../../src/stdlib.c) (lines 1219-1322)
- Tests: [test/error-handling/](../../test/error-handling/)
- Examples: See test files for usage examples

## Troubleshooting

### Error not being caught
- Ensure you're using the correct syntax: `(try {...} {err -> ...})`
- Check that error handler is a function, not a value

### Program still exits on error
- Verify error occurs inside try block, not outside
- Check try block is properly formed

### Error message is "[Void]"
- This happens with nested catches that don't explicitly return a value
- Add `<- value` to return explicit value from catch block
