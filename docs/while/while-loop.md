# While Loop - Condition-Based Iteration

** LLVM Native Compilation**

## Overview

The `while` statement provides condition-based iteration with full support for `break` and `continue` statements. While loops execute their body repeatedly as long as the condition remains true (non-zero).



## Syntax

```franz
(while condition body)
```

**Parameters:**
- `condition` - Expression evaluated before each iteration (0 = false, non-zero = true)
- `body` - Block or expression to execute while condition is true

**Returns:**
- Value from `break` statement, or
- `0` if loop completes normally

## Key Features

- ✅ **Condition-based iteration** - Loop continues while condition is true
- ✅ **Break support** - `(break)` or `(break value)` to exit early with result
- ✅ **Continue support** - `(continue)` to skip to next iteration
- ✅ **Nested loops** - Full support for nested while loops
- ✅ **Zero overhead** - Direct LLVM IR compilation, Rust-level performance
- ✅ **Mutable variables** - Use `mut` keyword for loop counters

## Basic Usage

### Simple While Loop

```franz
mut counter = 0
(while (less_than counter 5) {
  (println "Count:" counter)
  counter = (add counter 1)
})
// Prints: Count: 0, Count: 1, Count: 2, Count: 3, Count: 4
```

### Countdown

```franz
mut count = 5
(while (greater_than count 0) {
  (println count)
  count = (subtract count 1)
})
(println "Blast off!")
```

### Loop Until Condition

```franz
mut value = 1
(while (less_than value 100) {
  (println value)
  value = (multiply value 2)
})
// Prints: 1, 2, 4, 8, 16, 32, 64
```

## With Break Statement

### Search Pattern

```franz
// Find first number divisible by 7
mut n = 1
result = (while 1 {
  (if (is (remainder n 7) 0)
    {(break n)}
    {})
  n = (add n 1)
})
(println "Found:" result)  // → Found: 7
```

### Binary Search

```franz
mut low = 0
mut high = 100
target = 42
found = (while (less_than low (add high 1)) {
  mid = (divide (add low high) 2)

  (if (is mid target)
    {(break mid)}
    {})

  (if (less_than mid target)
    {low = (add mid 1)}
    {high = (subtract mid 1)})
})
(println "Found:" found)  // → Found: 42
```

## With Continue Statement

### Filter Pattern

```franz
// Print odd numbers from 1-10
mut i = 0
(while (less_than i 10) {
  i = (add i 1)
  (if (is (remainder i 2) 0)
    {(continue)}   // Skip even numbers
    {})
  (println "Odd:" i)
})
// Prints: Odd: 1, Odd: 3, Odd: 5, Odd: 7, Odd: 9
```

### Skip Invalid Data

```franz
mut n = 0
mut count = 0
(while (less_than n 10) {
  n = (add n 1)
  (if (is (remainder n 3) 0)
    {(continue)}   // Skip multiples of 3
    {})
  count = (add count 1)
})
```

## Advanced Examples

### Collatz Conjecture

```franz
mut n = 10
mut steps = 0
(while (not (is n 1)) {
  (if (is (remainder n 2) 0)
    {n = (divide n 2)}
    {n = (add (multiply n 3) 1)})
  steps = (add steps 1)
})
(println "Steps:" steps)
```

### Fibonacci Sequence

```franz
mut a = 0
mut b = 1
(while (less_than a 100) {
  (println a)
  temp = (add a b)
  a = b
  b = temp
})
// Prints: 0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89
```

### Prime Number Finder

```franz
mut candidate = 2
mut primes_found = 0
(while (less_than primes_found 5) {
  mut divisor = 2
  mut is_prime = 1

  (while (less_than divisor candidate) {
    (if (is (remainder candidate divisor) 0)
      {
        is_prime = 0
        (break 0)
      }
      {})
    divisor = (add divisor 1)
  })

  (if (is is_prime 1)
    {primes_found = (add primes_found 1)}
    {})

  candidate = (add candidate 1)
})
```

## Nested While Loops

```franz
mut outer = 0
(while (less_than outer 3) {
  mut inner = 0
  (while (less_than inner 2) {
    (println "outer=" outer "inner=" inner)
    inner = (add inner 1)
  })
  outer = (add outer 1)
})
```

## LLVM IR Pattern

```llvm
; Allocate return value storage
%while_return = alloca i64
store i64 0, i64* %while_return
br label %while_cond

while_cond:
  %cond_val = ...              ; Evaluate condition
  %cond = icmp ne i64 %cond_val, 0
  br i1 %cond, label %while_body, label %while_exit

while_body:
  ; Execute body statements
  ; Can use (break value) or (continue)
  br label %while_incr

while_incr:
  ; Continue jumps here
  br label %while_cond

while_exit:
  ; Break jumps here
  %result = load i64, i64* %while_return
```

## Comparison with Other Languages

| Language | Syntax | Break | Continue | Return Value |
|----------|--------|-------|----------|--------------|
| **Franz** | `(while cond body)` | ✅ `(break val)` | ✅ `(continue)` | Break value or 0 |
| **Rust** | `while cond { }` | ✅ `break val` | ✅ `continue` | Break value or `()` |
| **C** | `while (cond) { }` | ✅ `break` | ✅ `continue` | No return value |
| **Python** | `while cond:` | ✅ `break` | ✅ `continue` | No return value |
| **JavaScript** | `while (cond) { }` | ✅ `break` | ✅ `continue` | No return value |

**Franz's Advantage:**
- Rust-equivalent semantics with break values
- LLVM native compilation (C-level performance)
- Functional style with expression-based returns

## While vs Loop

| Feature | `while` | `loop` |
|---------|---------|--------|
| **Condition** | Evaluated each iteration | Fixed count |
| **Index** | Manual tracking | Automatic `i` parameter |
| **Use Case** | Unknown iteration count | Known iteration count |
| **Performance** | Same (both compiled to LLVM) | Same |

**When to use while:**
- Condition-based iteration (until threshold, until found, etc.)
- Unknown number of iterations
- Complex exit conditions

**When to use loop:**
- Fixed number of iterations
- Need index variable automatically
- Simpler for counted loops

## Best Practices

### 1. Always Use `mut` for Loop Variables

```franz
// ✅ CORRECT
mut counter = 0
(while (less_than counter 10) {
  counter = (add counter 1)
})

// ❌ WRONG - Immutability error
counter = 0
(while (less_than counter 10) {
  counter = (add counter 1)  // ERROR: Cannot reassign immutable variable
})
```

### 2. Prefer `break` with Value for Search Patterns

```franz
// ✅ GOOD - Returns found value
result = (while 1 {
  (if condition {(break value)} {})
})

// ❌ OKAY - But requires external variable
mut result = 0
(while 1 {
  (if condition {result = value} {})
})
```

### 3. Use `continue` for Filtering

```franz
// ✅ GOOD - Clean filtering with continue
(while condition {
  (if (not valid) {(continue)} {})
  (process)
})

// ❌ OKAY - But more nested
(while condition {
  (if valid {(process)} {})
})
```

### 4. Avoid Infinite Loops

```franz
// ✅ GOOD - Has exit condition
mut i = 0
(while (less_than i 10) {
  i = (add i 1)
})

// ⚠️ WARNING - Infinite loop (needs break)
(while 1 {
  // Must have (break) somewhere or will never exit
})
```

## Performance

- **Zero overhead**: Direct LLVM IR compilation
- **C-equivalent speed**: Same performance as hand-written C while loops
- **Rust-level performance**: Identical LLVM IR patterns as Rust
- **No runtime cost**: Condition, break, and continue are direct branches

## Troubleshooting

### Immutability Errors

**Problem**: `ERROR: Cannot reassign immutable variable`

**Solution**: Use `mut` keyword for variables modified in loop:
```franz
mut counter = 0  // ← Add 'mut'
(while (less_than counter 5) {
  counter = (add counter 1)
})
```

### Infinite Loops

**Problem**: Loop never exits

**Solution**: Ensure condition becomes false or use `break`:
```franz
mut i = 0
(while (less_than i 10) {  // ← Condition will become false
  i = (add i 1)            // ← Must modify condition variable
})
```

### Using Break Outside Loop

**Problem**: `ERROR: 'break' can only be used inside a loop`

**Solution**: Only use `break` inside `while` or `loop`:
```franz
(while condition {
  (if test {(break)} {})  // ← OK: Inside while
})
```



## Implementation

**File**: [`src/llvm-control-flow/llvm_control_flow_loop.c`](../../src/llvm-control-flow/llvm_control_flow_loop.c#L320-L409)

The while loop implementation:
- Creates 4 basic blocks: condition, body, increment, exit
- Evaluates condition before each iteration
- Branches to body if true, exit if false
- Increment block handles continue (jumps back to condition)
- Exit block handles break and normal completion
- Returns break value or 0

**Key Components**:
- Loop context tracking for nested loops
- Break/continue integration
- Block-as-statements compilation for body
- PHI node handling for return values
