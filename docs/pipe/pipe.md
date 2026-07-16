# Pipe Function Documentation


## Overview

The `pipe` function provides a clean, readable way to chain function applications in Franz. Instead of nesting function calls or creating intermediate variables, `pipe` allows you to express data transformations as a linear, left-to-right flow.



---

## Quick Reference

```franz
// Basic syntax
result = (pipe initial_value fn1 fn2 fn3 ... fnN)

// Example: Transform number through multiple operations
result = (pipe 5
  {x -> <- (add x 3)}      // 5 + 3 = 8
  {x -> <- (multiply x 2)}  // 8 * 2 = 16
  {x -> <- (subtract x 4)}) // 16 - 4 = 12
// result = 12
```

---

## Function Signature

```
(pipe value function1 function2 ... functionN) -> result
```

**Parameters:**
- `value` - The initial value to transform (any type)
- `function1, function2, ..., functionN` - Functions to apply in sequence (at least one required)
  - Each function must accept exactly one argument
  - Each function must return a value (the input for the next function)

**Returns:** The result of applying all functions in sequence

**Type Signature (conceptual):**
```
pipe : a -> (a -> b) -> (b -> c) -> ... -> (y -> z) -> z
```

---

## How It Works

The `pipe` function works by:

1. Starting with the initial value
2. Passing it to the first function
3. Taking that result and passing it to the second function
4. Continuing this process for all functions
5. Returning the final result

This creates a **left-to-right data flow**, which mirrors how we naturally read code.

**Memory Management:**
- Intermediate results are automatically freed when no longer needed
- Uses Franz's reference counting system
- No memory leaks even with long pipelines

---

## Basic Usage

### Example 1: Simple Arithmetic Chain

```franz
// Without pipe - nested and hard to read
result = (subtract (multiply (add 5 3) 2) 4)

// With pipe - clear left-to-right flow
result = (pipe 5
  {x -> <- (add x 3)}
  {x -> <- (multiply x 2)}
  {x -> <- (subtract x 4)})

(println result)  // 12
```

### Example 2: Named Functions

```franz
double = {x -> <- (multiply x 2)}
add_ten = {x -> <- (add x 10)}
halve = {x -> <- (divide x 2)}

result = (pipe 20 double add_ten halve)
(println result)  // 25
```

### Example 3: Type Conversions

```franz
result = (pipe 10
  {x -> <- (float x)}
  {x -> <- (divide x 3)}
  {x -> <- (multiply x 2)})
(println result)  // 6.666667
```

---

## Advanced Usage

### Working with Lists

#### Sum of Squares

```franz
numbers = (list 1 2 3 4 5)

result = (pipe numbers
  {lst -> <- (map lst {x i -> <- (multiply x x)})}
  {lst -> <- (reduce lst {acc x i -> <- (add acc x)} 0)})

(println result)  // 55 (1+4+9+16+25)
```

**Important:** Franz's `map` callbacks require **two parameters**: `{item index -> ...}`

#### Extracting and Transforming

```franz
my_list = (list 100 200 300)

result = (pipe my_list
  {lst -> <- (get lst 0)}
  {x -> <- (add x 50)})

(println result)  // 150
```

### String Operations

```franz
result = (pipe "hello"
  {s -> <- (join s " world")}
  {s -> <- (join s "!")})

(println result)  // "hello world!"
```

---

## Common Patterns

### Pattern 1: Data Validation Pipeline

```franz
parse_number = {s -> <- (integer s)}
validate_positive = {n ->
  result = (if (greater_than n 0) {<- n} {<- 0})
  <- result
}
clamp_max = {n ->
  result = (if (greater_than n 100) {<- 100} {<- n})
  <- result
}

validated = (pipe "75" parse_number validate_positive clamp_max)
(println validated)  // 75
```

### Pattern 2: Mathematical Transformations

```franz
// Calculate area of circle from radius
radius = 5
area = (pipe radius
  {x -> <- (float x)}
  {x -> <- (multiply x x)}
  {x -> <- (multiply x 3.14159)})

(println area)  // 78.539750
```

### Pattern 3: List Aggregation

```franz
// Calculate average
data = (list 10 20 30 40 50)

total = (pipe data
  {lst -> <- (reduce lst {acc x i -> <- (add acc x)} 0)})

count = (pipe data {lst -> <- (length lst)})

average = (divide total count)
(println average)  // 30.0
```

---

## Error Handling

### Error 1: No Functions Provided

```franz
result = (pipe 42)  // ❌ Error!
```

**Error Message:**
```
Runtime Error @ Line N: pipe requires at least a value and one function.
```

**Fix:** Provide at least one function:
```franz
result = (pipe 42 {x -> <- x})  // ✓ OK
```

### Error 2: Non-Function Argument

```franz
result = (pipe 5 42 {x -> <- (add x 1)})  // ❌ Error!
```

**Error Message:**
```
Runtime Error @ Line N: pipe argument 2 must be a function.
```

**Fix:** Ensure all arguments after the value are functions:
```franz
result = (pipe 5 {x -> <- (add x 42)} {x -> <- (add x 1)})  // ✓ OK
```

### Error 3: Function Arity Mismatch

```franz
// Franz's map requires callbacks with 2 args (item, index)
result = (pipe (list 1 2 3)
  {lst -> <- (map lst {x -> <- (multiply x 2)})})  // ❌ Error!
```

**Error Message:**
```
Runtime Error @ Line N: Supplied more arguments than required to function.
```

**Fix:** Use correct arity for Franz callbacks:
```franz
result = (pipe (list 1 2 3)
  {lst -> <- (map lst {x i -> <- (multiply x 2)})})  // ✓ OK
```

---

## Performance Considerations

### Memory Efficiency

The `pipe` function is memory-efficient:
- Intermediate results are freed automatically
- Uses Franz's built-in reference counting
- No accumulation of temporary values

### Long Pipelines

Pipelines of any length are supported:

```franz
result = (pipe 2
  {x -> <- (add x 1)}
  {x -> <- (multiply x 2)}
  {x -> <- (add x 5)}
  {x -> <- (multiply x 3)}
  {x -> <- (subtract x 10)})
// Works perfectly: 2 -> 3 -> 6 -> 11 -> 33 -> 23
```

**Recursion Limit:** Franz has a recursion limit of 20,000 calls. Pipe itself doesn't use recursion, so this isn't a concern for pipelines.

---

## Comparison with Alternative Approaches

### Nested Function Calls

**Without pipe:**
```franz
result = (subtract (multiply (add 5 3) 2) 4)
```
- Hard to read (right-to-left execution, left-to-right reading)
- Difficult to modify or debug
- Lots of parentheses

**With pipe:**
```franz
result = (pipe 5
  {x -> <- (add x 3)}
  {x -> <- (multiply x 2)}
  {x -> <- (subtract x 4)})
```
- Easy to read (left-to-right flow)
- Easy to add/remove steps
- Clear data transformation sequence

### Intermediate Variables

**Without pipe:**
```franz
step1 = (add 5 3)
step2 = (multiply step1 2)
result = (subtract step2 4)
```
- Clutters namespace with temporary variables
- Variables must be uniquely named
- Hard to see the overall flow

**With pipe:**
```franz
result = (pipe 5
  {x -> <- (add x 3)}
  {x -> <- (multiply x 2)}
  {x -> <- (subtract x 4)})
```
- No intermediate variables
- Clear single expression
- Easier to refactor

---

## Examples

### Complete Working Examples

Franz provides three comprehensive example files:

1. **[simple-arithmetic.franz](../../examples/pipe/working/simple-arithmetic.franz)**
   - Basic arithmetic chains
   - Named functions
   - Temperature conversions
   - Float operations

2. **[list-operations.franz](../../examples/pipe/working/list-operations.franz)**
   - Sum of squares
   - Average calculation
   - List products
   - Extract and transform

3. **[data-processing.franz](../../examples/pipe/working/data-processing.franz)**
   - User input validation
   - List processing pipelines
   - Mathematical transformations

**Run examples:**
```bash
./franz examples/pipe/working/simple-arithmetic.franz
./franz examples/pipe/working/list-operations.franz
./franz examples/pipe/working/data-processing.franz
```

---

## Testing

### Test Suite

Franz includes a comprehensive test suite with 12 test cases covering:

- ✅ Basic arithmetic pipelines
- ✅ Multi-step transformations
- ✅ String operations
- ✅ List operations (map, reduce)
- ✅ Type conversions
- ✅ Identity functions
- ✅ Single-function pipes
- ✅ Long pipelines (5+ steps)
- ✅ Built-in function integration
- ✅ Edge cases and boundary conditions

**Test files:**
- [test/pipe/pipe-test.franz](../../test/pipe/pipe-test.franz) - Main comprehensive test suite (12 tests)

**Run tests:**
```bash
./franz test/pipe/pipe-test.franz
```

**Expected output:**
```
=== Pipe Function Test Suite ===
Test 1: Basic pipe with arithmetic
PASS: (pipe 5 add_one square) = 36 (got 36 )
...
All 12 tests passed!
```

---


## Best Practices

### ✅ DO

- Use `pipe` for sequences of 2+ transformations
- Name complex functions for reusability
- Keep each step focused on a single transformation
- Use descriptive variable names for clarity

```franz
// Good
square = {x -> <- (multiply x x)}
add_one = {x -> <- (add x 1)}
result = (pipe 5 add_one square)
```

### ❌ DON'T

- Don't use `pipe` for single function calls
  ```franz
  // Bad - pipe is unnecessary
  result = (pipe 5 double)

  // Good - call directly
  result = (double 5)
  ```

- Don't create overly long anonymous functions inside pipes
  ```franz
  // Bad - hard to read
  result = (pipe data
    {x -> <- (if (greater_than x 0) {<- (multiply x 2)} {<- 0})})

  // Good - extract to named function
  double_if_positive = {x ->
    result = (if (greater_than x 0) {<- (multiply x 2)} {<- 0})
    <- result
  }
  result = (pipe data double_if_positive)
  ```

---

## Troubleshooting

### Issue 1: "Supplied more arguments than required"

**Problem:** Franz callbacks (map, reduce) require specific arity

**Solution:** Check Franz's callback signatures:
- `map`: `{item index -> ...}`
- `reduce`: `{accumulator item index -> ...}`

### Issue 2: Type mismatches in pipeline

**Problem:** A function in the pipeline expects a different type than it receives

**Solution:** Add type conversion functions:
```franz
result = (pipe "42"
  {s -> <- (integer s)}      // Convert string to int
  {n -> <- (add n 10)})      // Now we can do math
```

### Issue 3: Pipeline returns unexpected value

**Problem:** Missing return statement (`<-`) in function

**Solution:** Ensure all functions return values:
```franz
// Bad
my_fn = {x -> (add x 1)}  // Missing <-

// Good
my_fn = {x -> <- (add x 1)}  // Returns the result
```

---

*Last Updated: 2025-10-02*
*Franz Version: 1.0*
