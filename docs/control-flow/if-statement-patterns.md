# Franz If Statement Patterns


---

## Overview

The Franz `if` statement has specific patterns for returning values vs. performing side effects. Understanding these patterns is **critical** for writing correct Franz code.

---

## Key Principles

### 1. **If statements ALWAYS return a value**
- The if statement evaluates to the result of the executed branch
- This value can be assigned to a variable or used in expressions
- Both branches (true/false) must return compatible types

### 2. **Return values vs. Side effects**
- **Return value**: Use `{<- value}` syntax to explicitly return a value
- **Side effect**: Use `{action}` syntax for operations like `println`
- **NEVER assign side-effect-only if to a variable** - causes "Attempt to call non-function value" error

---

## Pattern 1: If with Return Values (Correct ✅)

### Basic Pattern
```franz
// Correct: Using explicit return syntax
result = (if condition {<- value1} {<- value2})
```

### Examples
```franz
// Integer return
max = (if (greater_than a b) {<- a} {<- b})

// String return
grade = (if (greater_than score 90) {<- "A"} {<- "B"})

// Float return
abs_val = (if (less_than x 0) {<- (multiply x -1)} {<- x})

// List return
default_list = (if (empty? items) {<- (list 1 2 3)} {<- items})

// Function return
operation = (if use_add {<- add} {<- multiply})

// Expression return
result = (if (is x 0) {<- "zero"} {<- (add x 1)})
```

### Multi-clause If (3-argument form)
```franz
// Correct: All branches return explicit values
result = (if (is 1 1) {<- "yes"} {<- "no"})

result = (if test9_cond {<- "true branch"} {<- "false branch"})

// Can also return without blocks for simple values
t12_result = (if t12_cond 100 200)
```

---

## Pattern 2: If with Side Effects Only (Correct ✅)

### Basic Pattern
```franz
// Correct: No assignment, just execute side effects
(if condition
  {
    (println "True branch")
    (some_action)
  }
  {
    (println "False branch")
    (other_action)
  }
)
```

### Examples
```franz
// Example 1: Conditional printing
(if (is coin 0)
  {
    (println "Result: Heads")
  }
  {
    (println "Result: Tails")
  }
)

// Example 2: Conditional file operations
(if file_exists
  {
    (println "Processing file...")
    (process_file filename)
  }
  {
    (println "File not found")
  }
)

// Example 3: Multiple side effects per branch
(if (greater_than score 90)
  {
    (println "Excellent!")
    (println "Grade: A")
    (write_file "grade.txt" "A")
  }
  {
    (println "Keep trying")
    (println "Grade: B")
  }
)
```

---

## Pattern 3: If with Mixed Return and Side Effects (Correct ✅)

### Basic Pattern
```franz
// Correct: Side effects in blocks, then explicit return
result = (if condition
  {
    (println "Debug: true branch")
    <- value1
  }
  {
    (println "Debug: false branch")
    <- value2
  }
)
```

### Examples
```franz
// Example 1: Debug logging with return
max = (if (greater_than a b)
  {
    (println "a is greater")
    <- a
  }
  {
    (println "b is greater or equal")
    <- b
  }
)

// Example 2: Validation with logging
validated = (if (greater_than n 0)
  {
    (println "Valid input")
    <- n
  }
  {
    (println "Invalid input, using default")
    <- 0
  }
)
```

---

## ✅ Multi-Statement Blocks (B - IMPLEMENTED!)

### Now Fully Supported in Bytecode!

The bytecode compiler **NOW SUPPORTS** multi-statement blocks with mixed side effects and returns:

```franz
// ✅ NOW WORKS: Multi-statement blocks with side effects + return
result = (if condition
  {
    (println "Debug message")
    temp = (compute_value)
    <- temp
  }
  {
    <- other_value
  }
)
// Works perfectly in bytecode mode!
```

### All Patterns Now Supported

✅ **Parameterless blocks** (multiple statements, no parameters):
```franz
result = (if cond
  {
    (println "Statement 1")
    (println "Statement 2")
    <- 42
  }
  {<- 0}
)
```

✅ **Functions with parameters and multiple statements:**
```franz
square = {x ->
  temp = (multiply x x)
  (println "Computed: " temp)
  <- temp
}
```

✅ **Empty blocks:**
```franz
(if cond {} {(println "else branch")})  // Empty block returns void
```

✅ **Long statement sequences:**
```franz
result = (if 1
  {
    v1 = 1
    v2 = (add v1 1)
    v3 = (add v2 1)
    v4 = (add v3 1)
    <- v4
  }
  {<- 0}
)
```

✅ **Nested multi-statement blocks:**
```franz
outer = (if 1
  {
    (println "Outer")
    inner = (if 1
      {
        (println "Inner")
        <- 99
      }
      {<- 0}
    )
    <- inner
  }
  {<- 0}
)
```

### Implementation Details

**How It Works:**
- Bytecode compiler detects parameterless blocks by checking if first child is an identifier
- If first child is `OP_STATEMENT/OP_APPLICATION/OP_RETURN`, it's a parameterless block
- All children are compiled as body statements
- Last statement's value is returned automatically
- Intermediate values are popped from stack (BC_POP)


---

## ❌ ANTI-PATTERNS (Common Mistakes)

### ❌ WRONG: Assigning side-effect-only if to variable
```franz
// ERROR: println returns void, which can't be assigned
flip_result = (if (is coin 0)
  (println "Result: Heads")
  (println "Result: Tails")
)
// Runtime Error: Attempt to call non-function value
```

**Why this fails:**
- `println` returns `TYPE_VOID`
- Assigning void to `flip_result` creates a void value
- Franz tries to call the void value in some contexts → error

**Fix:**
```franz
// Option 1: Don't assign, just execute
(if (is coin 0)
  {
    (println "Result: Heads")
  }
  {
    (println "Result: Tails")
  }
)

// Option 2: Return explicit values
flip_result = (if (is coin 0) {<- "Heads"} {<- "Tails"})
(println "Result: " flip_result)
```

### ❌ WRONG: Inconsistent return types
```franz
// ERROR: One branch returns value, other doesn't
result = (if condition {<- 42} (println "no"))
```

**Fix:**
```franz
// Both branches must return values
result = (if condition {<- 42} {<- 0})
```

### ❌ WRONG: Missing braces for side effects
```franz
// ERROR: Multiple statements without braces
(if condition
  (println "Line 1")
  (println "Line 2")  // This is treated as false branch!
)
```

**Fix:**
```franz
// Use braces for multiple statements
(if condition
  {
    (println "Line 1")
    (println "Line 2")
  }
  {
    (println "False branch")
  }
)
```

---

## Pattern Decision Tree

Use this decision tree when writing `if` statements:

```
Do you need to store the result?
│
├─ YES → Use return value pattern
│   │
│   └─ result = (if cond {<- val1} {<- val2})
│
└─ NO → Are you just performing side effects?
    │
    ├─ YES → Use side-effect pattern (no assignment)
    │   │
    │   └─ (if cond {(println ...)} {(println ...)})
    │
    └─ Mixed (side effects + return)?
        │
        └─ Use mixed pattern
            │
            └─ result = (if cond {(println ...) <- val} {...})
```

---

## Common Use Cases

### Use Case 1: Ternary-like expressions
```franz
// Choose between two values
min = (if (less_than a b) {<- a} {<- b})
max = (if (greater_than a b) {<- a} {<- b})
sign = (if (less_than x 0) {<- -1} {<- 1})
```

### Use Case 2: Conditional execution
```franz
// Execute different actions based on condition
(if (is mode "debug")
  {
    (println "Debug mode enabled")
    (print_debug_info)
  }
  {
    (println "Production mode")
  }
)
```

### Use Case 3: Default values
```franz
// Provide default if condition fails
value = (if (greater_than input 0) {<- input} {<- 1})
```

### Use Case 4: Guard clauses
```franz
// Validate and return or default
result = (if (and (greater_than n 0) (less_than n 100))
  {<- n}
  {<- 50}  // Default to 50 if out of range
)
```

---

## Testing Patterns

### Pattern for Test Assertions
```franz
// Correct: Use explicit return for test results
test1_pass = (if (is result expected) {<- 1} {<- 0})
(println "Test 1: " test1_pass)

// Correct: Multiple test checks
test2_pass = (if (and check1 check2 check3) {<- 1} {<- 0})
```

### Pattern for Conditional Test Output
```franz
// Correct: Side effects for debugging, return for pass/fail
test_result = (if condition
  {
    (println "PASS: Test description")
    <- 1
  }
  {
    (println "FAIL: Test description")
    <- 0
  }
)
```

---

## Integration with Other Constructs

### With map/reduce/filter
```franz
// If inside higher-order functions
(map {item -> (if (greater_than item 0) {<- item} {<- 0})} lst)

// Cleaner: Define helper function
positive_or_zero = {n -> (if (greater_than n 0) {<- n} {<- 0})}
(map positive_or_zero lst)
```

### With loop/recursion
```franz
// If in recursive functions
factorial = {n ->
  (if (is n 0)
    {<- 1}
    {<- (multiply n (factorial (subtract n 1)))}
  )
}
```

### With pipe/thread-first
```franz
// If in pipelines
result = (pipe
  input
  {x -> (if (greater_than x 0) {<- x} {<- 0})}
  {x -> (multiply x 2)}
)
```

---

## Comparison with Other Languages

| Language   | Ternary/If Expression | Side Effect If |
|------------|----------------------|----------------|
| Franz      | `(if c {<- a} {<- b})`| `(if c {(println ...)} {})` |
| Python     | `a if c else b`      | `if c: print(...)` |
| JavaScript | `c ? a : b`          | `if (c) { console.log(...) }` |
| Lisp       | `(if c a b)`         | `(when c (print ...))` |
| Rust       | `if c { a } else { b }` | `if c { println!(...) }` |

**Franz Note:**
- Always use `{<- value}` for explicit returns in assigned if statements
- Use bare `(if c {...} {...})` for side effects only (no assignment)

---

## Quick Reference Card

```franz
✅ CORRECT PATTERNS:

1. Return value (assigned):
   result = (if cond {<- val1} {<- val2})

2. Side effects (not assigned):
   (if cond {(println "yes")} {(println "no")})

3. Mixed (assigned with side effects):
   result = (if cond {(println "debug") <- val1} {<- val2})

4. Simple return (no blocks needed for simple values):
   result = (if cond 100 200)

❌ WRONG PATTERNS:

1. Assigning void return:
   result = (if cond (println "yes") (println "no"))  // ERROR!

2. Inconsistent returns:
   result = (if cond {<- val} (println "no"))  // ERROR!

3. Missing braces for multiple statements:
   (if cond (println "1") (println "2"))  // "2" is false branch!
```

---

## File Naming Conventions for Tests/Examples

When creating test or example files with if statements:

- ✅ **test-if-return.franz** - Tests with return value patterns
- ✅ **test-if-side-effects.franz** - Tests with side effect patterns
- ✅ **example-if-ternary.franz** - Examples using if like ternary operator
- ❌ **test-if-broken.franz** - Avoid names that don't indicate pattern

---

