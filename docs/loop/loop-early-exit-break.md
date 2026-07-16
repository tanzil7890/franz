# Loop Early Exit with `break` Statement

## Overview

Franz supports Rust-style early loop exit using the `break` statement. This allows you to exit a loop before it completes all iterations, optionally returning a value.



## Syntax

Franz supports **two forms** of the break statement:

```franz
(break)           // Exit loop, return 0 (no value)
(break value)     // Exit loop, return specified value
```

**Both forms are fully supported** - you can use `(break)` alone or with a value.

## Behavior

- **`(break)`** - Exits the loop immediately and returns 0 (void/no value)
- **`(break value)`** - Exits the loop immediately and returns the specified value
- **No break** - Loop completes all iterations and returns 0

**Key Point:** The value is optional - `(break)` alone is perfectly valid!

## Implementation

- **Rust-level performance** - Direct LLVM IR branch to loop exit block
- **Type safe** - Break value type must match loop context
- **Scope aware** - Can only be used inside loops
- **Zero runtime overhead** - No function calls, just direct branching

## Examples

### Example 1: Break Without Value

```franz
// Exit loop when condition met, don't need a return value
(loop 10 {i ->
  (println "Processing:" i)
  (if (is i 5)
    {(break)}       // Exit with 0, no value needed
    {})
})
// Prints: Processing: 0, 1, 2, 3, 4, 5
```

### Example 2: Break With Value

```franz
// Find first even number and return it
result = (loop 10 {i ->
  (if (is (remainder i 2) 0)
    {(break i)}     // Exit with value i
    {})
})
(println "First even:" result)  // → First even: 0
```

### Example 3: Both Forms in One Program

```franz
// Demonstrate both forms working together

// Form 1: Break without value (just exit)
(println "Countdown:")
(loop 10 {i ->
  countdown = (subtract 9 i)
  (println countdown)
  (if (is countdown 5)
    {(break)}       // Just exit, no value needed
    {})
})

// Form 2: Break with value (return result)
(println "")
(println "Search for target:")
target = 7
found = (loop 20 {i ->
  (if (is i target)
    {(break i)}     // Return the found value
    {})
})
(println "Found:" found)  // → Found: 7
```

### Example 3: Early Exit with Computation

```franz
// Exit when product exceeds 50
result = (loop 10 {i ->
  product = (multiply i i)
  (if (greater_than product 50)
    {(break product)}
    {})
})
(println "First product > 50:" result)  // → First product > 50: 64
```

### Example 4: Nested Loops with Break

```franz
// Find first pair where i * j > 10
outer_result = (loop 5 {i ->
  inner_result = (loop 5 {j ->
    product = (multiply i j)
    (if (greater_than product 10)
      {(break product)}
      {})
  })

  (if (greater_than inner_result 0)
    {(break inner_result)}
    {})
})
(println "Result:" outer_result)  // → Result: 12 (3*4)
```

### Example 5: Conditional Processing with Break

```franz
// Process until condition met
count = 0
result = (loop 100 {i ->
  count = (add count 1)

  // Check multiple conditions
  too_high = (greater_than count 50)
  found_target = (is i 25)

  (if (or too_high found_target)
    {(break i)}
    {})
})
(println "Stopped at:" result)  // → Stopped at: 25
```

## Comparison with Other Languages

### Rust
```rust
// Rust
let result = loop {
    if condition {
        break value;  // Exit with value
    }
};
```

### Franz
```franz
// Franz (equivalent)
result = (loop count {i ->
  (if condition
    {(break value)}
    {})
})
```

### JavaScript
```javascript
// JavaScript (no direct equivalent - uses for loops)
let result = null;
for (let i = 0; i < count; i++) {
    if (condition) {
        result = value;
        break;
    }
}
```

### Python
```python
# Python (no direct equivalent - uses for loops)
result = None
for i in range(count):
    if condition:
        result = value
        break
```

## LLVM IR Pattern

When you write:
```franz
(loop 10 {i ->
  (if (is i 5)
    {(break i)}
    {})
})
```

It compiles to:
```llvm
loop_body:
  ; ... compute condition ...
  br i1 %cond, label %then, label %else

then:
  ; Store break value
  store i64 %i, i64* %loop_return
  br label %loop_exit    ; Jump to exit

else:
  br label %loop_incr    ; Continue to next iteration

loop_incr:
  ; counter++
  %next = add i64 %i, 1
  store i64 %next, i64* %counter
  br label %loop_cond

loop_exit:
  ; Load and return result
  %result = load i64, i64* %loop_return
  ret i64 %result
```

## Error Handling

```franz
// ERROR: break outside loop
(break 42)
// → ERROR: 'break' can only be used inside a loop at line X

// OK: break inside loop
result = (loop 10 {i -> (break 42)})
// → result = 42
```

## Best Practices

### 1. Use break for Early Exit Logic

✅ **Good:**
```franz
result = (loop 1000 {i ->
  (if (found_what_we_need i)
    {(break i)}
    {})
})
```

❌ **Bad:**
```franz
// Don't use loop for single iteration
result = (loop 1 {i -> i})  // Just use direct value instead
```

### 2. Return Meaningful Values

✅ **Good:**
```franz
result = (loop 100 {i ->
  (if (condition i)
    {(break i)}           // Return the actual value
    {})
})
```

❌ **Bad:**
```franz
result = (loop 100 {i ->
  (if (condition i)
    {(break 1)}           // Returns meaningless 1
    {})
})
// Better: Check if result > 0 to know if found
```

### 3. Prefer break Over Complex Return Logic

✅ **Good:**
```franz
result = (loop count {i ->
  (if (is i target)
    {(break i)}
    {})
})
```

❌ **Bad:**
```franz
// Using mutation and flags
mut found = 0
(loop count {i ->
  (if (is i target)
    {found = i}
    {})
})
// found now has result, but harder to reason about
```

## Performance

- **Zero overhead** - Direct LLVM branch instruction
- **C-equivalent speed** - Same performance as hand-written C `break`
- **Rust-level** - Identical to Rust's `break value` pattern
- **No allocations** - Break value stored in stack-allocated pointer


## See Also

- [Loop Documentation](loop.md)
- [Nested Loops](nested-loops.md)
- [Control Flow Overview](../control-flow/control-flow-overview.md)
