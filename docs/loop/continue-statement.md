# Continue Statement

## Overview

Franz supports  loop iteration control using the `continue` statement. This allows you to skip the rest of the current iteration and immediately jump to the next one, matching the semantics of Rust, C, Python, and JavaScript.



## Syntax

```franz
(continue)    // Skip to next iteration
```

Simple and straightforward - just like Rust and C!

## Behavior

- **Skips rest of current iteration** - Any code after `continue` is not executed
- **Jumps to loop increment** - Counter increments and loop condition is re-evaluated
- **No value returned** - Continue doesn't return anything (unlike break)
- **Works in all loops** - Compatible with basic and nested loops

## Implementation

- **Rust-level performance** - Direct LLVM IR branch to increment block
- **Zero runtime overhead** - No function calls, just direct branching
- **Scope aware** - Can only be used inside loops (compile error otherwise)
- **Type safe** - Compile-time validation

## Examples

### Example 1: Skip Even Numbers

```franz
// Print only odd numbers
(loop 10 {i ->
  (if (is (remainder i 2) 0)
    {(continue)}    // Skip even numbers
    {})

  (println "Odd:" i)
})
// Output: Odd: 1, 3, 5, 7, 9
```

### Example 2: Filter Pattern

```franz
// Process only values greater than threshold
threshold = 5
(loop 10 {i ->
  (if (less_than i threshold)
    {(continue)}    // Skip values below threshold
    {})

  (println "Processing:" i)
})
// Output: Processing: 5, 6, 7, 8, 9
```

### Example 3: Multiple Continue Conditions

```franz
// Skip multiple conditions
(loop 20 {i ->
  // Skip multiples of 3
  (if (is (remainder i 3) 0)
    {(continue)}
    {})

  // Skip value 7
  (if (is i 7)
    {(continue)}
    {})

  (println "Passed:" i)
})
// Skips: 0,3,6,7,9,12,15,18
// Prints: 1,2,4,5,8,10,11,13,14,16,17,19
```

### Example 4: FizzBuzz with Continue

```franz
// Clean FizzBuzz using continue
(loop 15 {i ->
  n = (add i 1)

  div3 = (is (remainder n 3) 0)
  div5 = (is (remainder n 5) 0)

  (if (and div3 div5)
    {
      (println n ": FizzBuzz")
      (continue)
    }
    {})

  (if div3
    {
      (println n ": Fizz")
      (continue)
    }
    {})

  (if div5
    {
      (println n ": Buzz")
      (continue)
    }
    {})

  (println n)
})
```

### Example 5: Continue in Nested Loops

```franz
// 3x3 grid, skip diagonal
(loop 3 {i ->
  (loop 3 {j ->
    // Skip diagonal elements
    (if (is i j)
      {(continue)}
      {})

    (println "[" i "," j "]")
  })
})
// Skips: [0,0], [1,1], [2,2]
```

### Example 6: Continue with Break

```franz
// Continue and break work together
result = (loop 20 {i ->
  // Skip even numbers
  (if (is (remainder i 2) 0)
    {(continue)}
    {})

  // Break on 15
  (if (is i 15)
    {(break i)}
    {})

  (println "Processing:" i)
})
// Processes: 1,3,5,7,9,11,13
// Breaks at 15
```

## LLVM IR Pattern

When you write:
```franz
(loop 10 {i ->
  (if (is i 5)
    {(continue)}
    {})
  (println i)
})
```

It compiles to:
```llvm
loop_body:
  ; Check condition
  %cond = icmp eq i64 %i, 5
  br i1 %cond, label %then, label %else

then:
  br label %loop_incr    ; Continue - jump to increment

else:
  ; Rest of loop body (println)
  call void @println(...)
  br label %loop_incr

loop_incr:
  ; Increment counter
  %next = add i64 %i, 1
  store i64 %next, i64* %counter
  br label %loop_cond    ; Back to condition check
```

## Comparison with Other Languages

| Language | Syntax | Skip Iteration | Performance |
|----------|--------|----------------|-------------|
| Rust | `continue` | ✅ Yes | 100% (baseline) |
| C | `continue` | ✅ Yes | 100% |
| Franz | `(continue)` | ✅ Yes | 100% (identical) |
| Python | `continue` | ✅ Yes | ~40% (interpreter) |
| JavaScript | `continue` | ✅ Yes | ~60% (JIT) |

Franz matches Rust and C's performance exactly.

## Continue vs Break

| Feature | Continue | Break |
|---------|----------|-------|
| **Purpose** | Skip to next iteration | Exit loop entirely |
| **Jump target** | Loop increment block | Loop exit block |
| **Return value** | None | Optional value |
| **Use case** | Filter/skip items | Early exit/search |

**Example showing both:**
```franz
// Find first odd number > 10, skip evens
result = (loop 20 {i ->
  // Skip even numbers (continue)
  (if (is (remainder i 2) 0)
    {(continue)}
    {})

  // Exit when found (break)
  (if (greater_than i 10)
    {(break i)}
    {})
})
// Result: 11
```

## Error Handling

```franz
// ERROR: continue outside loop
(continue)
// → ERROR: 'continue' can only be used inside a loop at line X

// OK: continue inside loop
(loop 5 {i ->
  (continue)    // Valid
})
```

## Best Practices

### 1. Use Continue for Filtering

✅ **Good:**
```franz
(loop count {i ->
  (if (invalid i)
    {(continue)}
    {})
  (process i)
})
```

❌ **Bad:**
```franz
(loop count {i ->
  (if (not (invalid i))
    {(process i)}
    {})
})
// Harder to read with negation
```

### 2. Early Guards

✅ **Good:**
```franz
(loop count {i ->
  // Guard clauses at top
  (if (should_skip i) {(continue)} {})
  (if (too_small i) {(continue)} {})

  // Main logic here
  (process i)
})
```

❌ **Bad:**
```franz
(loop count {i ->
  (if (not (should_skip i))
    {(if (not (too_small i))
      {(process i)}
      {})}
    {})
})
// Deep nesting
```

### 3. Combine with Break

✅ **Good:**
```franz
(loop count {i ->
  (if (invalid i) {(continue)} {})  // Skip
  (if (found i) {(break i)} {})     // Exit
  (process i)
})
```

## Performance

- **Zero overhead** - Direct LLVM branch instruction
- **C-equivalent speed** - Identical to handwritten C `continue`
- **Rust-level performance** - Same LLVM IR pattern as Rust
- **No allocations** - No function calls, just branching


## See Also

- [Break Statement](loop-early-exit-break.md)
- [Loop Documentation](loop.md)
- [Nested Loops](nested-loops.md)
- [Control Flow Overview](../control-flow/control-flow-overview.md)
