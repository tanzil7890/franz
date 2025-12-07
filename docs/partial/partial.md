# Partial Application Documentation



## Overview

Partial application allows you to **fix some arguments of a function**, creating a **new specialized function** that remembers those arguments. This eliminates repetitive lambda boilerplate and enables building libraries of reusable function transformations.


---

## Quick Reference

```franz
// Create a partial function
add_five = (partial add 5)

// Use it with call
result = (call add_five 10)  // Returns 15 (same as: add 5 10)

// Reuse multiple times
(call add_five 1)   // 6
(call add_five 10)  // 15
(call add_five 100) // 105
```

---

## Function Signatures

### `partial`
```
(partial function fixed_arg1 fixed_arg2 ... fixed_argN) -> partial_function
```

**Parameters:**
- `function` - The function to partially apply (any Franz function)
- `fixed_arg1, fixed_arg2, ..., fixed_argN` - Arguments to fix (at least one required)

**Returns:** A partial function (internally a list) that can be used with `call`

---

### `call`
```
(call partial_function new_arg1 new_arg2 ... new_argN) -> result
```

**Parameters:**
- `partial_function` - A partial function created by `partial`
- `new_arg1, new_arg2, ..., new_argN` - Additional arguments to provide (zero or more)

**Returns:** Result of calling the original function with fixed args + new args

---

## How It Works

### Creating a Partial
```franz
double = (partial multiply 2)
```

This creates a list internally: `[multiply, 2]`

### Calling a Partial
```franz
result = (call double 21)
```

The `call` function:
1. Extracts the original function: `multiply`
2. Combines fixed args `[2]` with new args `[21]`
3. Calls `multiply` with `[2, 21]`
4. Returns `42`

### Memory Management
- Partial functions can be reused unlimited times
- Reference counting prevents premature freeing
- No memory leaks even with complex pipelines

---

## Basic Usage

### Example 1: Simple Math Operations

```franz
add_ten = (partial add 10)
double = (partial multiply 2)
half = (partial divide 2)

(println (call add_ten 5))   // 15
(println (call double 21))   // 42
(println (call half 100))    // 0.02 (note: 2/100, not 100/2)
```

### Example 2: Multiple Fixed Arguments

```franz
add_many = (partial add 10 20 30)

// Call with additional args
(println (call add_many 40))      // 100 (10+20+30+40)
(println (call add_many 5 15))    // 80  (10+20+30+5+15)
```

### Example 3: String Operations

```franz
prefix_hello = (partial join "Hello ")

(println (call prefix_hello "World"))  // "Hello World"
(println (call prefix_hello "Franz"))  // "Hello Franz"
```

### Example 4: Reusing Partials

```franz
add_hundred = (partial add 100)

// Same partial, different arguments
(println (call add_hundred 1))    // 101
(println (call add_hundred 50))   // 150
(println (call add_hundred 200))  // 300
```

---

## Advanced Usage

### Partial with User-Defined Functions

```franz
square = {x -> <- (multiply x x)}

// Create partial with custom function
square_then_add = {base ->
  squared = (square base)
  add_ten = (partial add 10)
  <- (call add_ten squared)
}

(println (square_then_add 5))  // 35 (5² + 10)
```

### Chaining Partials

```franz
add_hundred = (partial add 100)
times_ten = (partial multiply 10)

x = 50
step1 = (call add_hundred x)     // 150
step2 = (call times_ten step1)   // 1500
(println step2)
```

### Partial with Comparisons

```franz
// Note: (partial greater_than 100) = {x -> (greater_than 100 x)} = "is 100 > x?"
gt_100 = (partial greater_than 100)
lt_50 = (partial less_than 50)

(println (call gt_100 150))  // 0 (is 100 > 150? no)
(println (call gt_100 25))   // 1 (is 100 > 25? yes)
(println (call lt_50 75))    // 1 (is 50 < 75? yes)
```

---

## Integration with Pipe

Partial application works beautifully with the `pipe` function:

### Example: Clean Pipeline

```franz
add_ten = (partial add 10)
times_three = (partial multiply 3)

result = (pipe 5
  {x -> <- (call add_ten x)}
  {x -> <- (call times_three x)})

(println result)  // 45 ((5 + 10) * 3)
```

### Example: Data Processing

```franz
numbers = (list 1 2 3 4 5)
double_each = (partial map numbers)

doubled = (call double_each {x i -> <- (multiply x 2)})
(println doubled)  // [List: 2, 4, 6, 8, 10]
```

---

## Common Patterns

### Pattern 1: Function Libraries

```franz
// Mathematical operations
increment = (partial add 1)
decrement = (partial subtract 1)
double = (partial multiply 2)
square = {x -> <- (multiply x x)}

// Use in pipelines
process = {n ->
  <- (pipe n
    square
    {x -> <- (call increment x)}
    {x -> <- (call double x)})
}
```

### Pattern 2: Configuration Functions

```franz
// Configure with specific values
log_to_file = (partial write_file "/var/log/app.log")
seconds_to_hours = (partial divide 3600)

// Use configured functions
(call log_to_file "Error: Something went wrong")
hours = (call seconds_to_hours 7200)  // 2 hours
```

### Pattern 3: Validation Functions

```franz
min_age = (partial greater_than 18)
max_price = (partial less_than 1000)

validate_user = {age price ->
  age_ok = (is (call min_age age) 1)
  price_ok = (is (call max_price price) 0)
  <- (and age_ok price_ok)
}
```

---

## Error Handling

### Error 1: Missing Arguments

```franz
p = (partial add)  // ❌ Error!
```

**Error Message:**
```
Runtime Error @ Line N: partial requires a function and at least one argument.
```

**Fix:** Provide at least one fixed argument:
```franz
p = (partial add 5)  // ✓ OK
```

### Error 2: Non-Function First Argument

```franz
p = (partial 42 5)  // ❌ Error!
```

**Error Message:**
```
Runtime Error @ Line N: partial first argument must be a function.
```

**Fix:** First argument must be a function:
```franz
p = (partial add 5)  // ✓ OK
```

### Error 3: Invalid Partial Structure

```franz
not_a_partial = (list 1 2 3)
result = (call not_a_partial 5)  // ❌ Error!
```

**Error Message:**
```
Runtime Error @ Line N: Invalid partial function - must have [function, ...fixed_args].
```

**Fix:** Only use `call` with partials created by `partial`.

---

## Argument Order Semantics

**IMPORTANT:** Fixed arguments come FIRST, then new arguments.

### Example: Addition (Commutative)

```franz
add_five = (partial add 5)
(call add_five 10)  // (add 5 10) = 15
```

Order doesn't matter for commutative operations.

### Example: Subtraction (Non-Commutative)

```franz
subtract_from_10 = (partial subtract 10)
(call subtract_from_10 3)  // (subtract 10 3) = 7  (10 - 3)

// NOT the same as:
subtract_3 = (partial subtract 3)
(call subtract_3 10)  // (subtract 3 10) = -7  (3 - 10)
```

### Example: Division (Non-Commutative)

```franz
divide_100_by = (partial divide 100)
(call divide_100_by 10)  // (divide 100 10) = 10  (100 / 10)

// vs

divide_by_100 = (partial divide 10)
(call divide_by_100 1000)  // (divide 10 1000) = 0.01  (10 / 1000)
```

**Rule of Thumb:** `(partial fn A)` creates `{x -> (fn A x)}`, not `{x -> (fn x A)}`

---

## Examples

### Complete Working Examples

Franz provides two comprehensive example files:

1. **[basic-partial.franz](../../examples/partial/working/basic-partial.franz)**
   - Math functions
   - Multiple fixed arguments
   - Comparison functions
   - String operations
   - Reusing partials

2. **[partial-with-pipe.franz](../../examples/partial/working/partial-with-pipe.franz)**
   - Pipeline integration
   - Data processing
   - Function libraries
   - Validation pipelines

**Run examples:**
```bash
./franz examples/partial/working/basic-partial.franz
./franz examples/partial/working/partial-with-pipe.franz
```

---
