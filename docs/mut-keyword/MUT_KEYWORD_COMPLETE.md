#  Mutable Variables with `mut` Keyword 

## Overview

Franz now supports **explicit mutability** with the `mut` keyword, following Rust-style variable declaration patterns. Variables are immutable by default, providing safety and predictability, while mutable variables enable controlled state changes when needed.



## Syntax

### Immutable Variables (Default)
```franz
x = 10
(println x)  // → 10

// This will error:
// x = 20
// ERROR: Cannot reassign immutable variable 'x'
```

### Mutable Variables
```franz
mut x = 10
(println x)  // → 10

x = 20  // ✅ Allowed
(println x)  // → 20

x = 30  // ✅ Allowed
(println x)  // → 30
```

## Key Features

### 1. Immutability by Default
- All variables declared without `mut` are immutable
- Attempting to reassign causes clear compile-time error
- Promotes functional programming patterns

### 2. Explicit Mutability
- Use `mut` keyword to declare mutable variables
- Enables controlled state changes
- Required for accumulator patterns in loops

### 3. LLVM Native Compilation
- **Immutable variables**: SSA-style direct values (zero overhead)
- **Mutable variables**: Alloca + store/load pattern (stack-allocated)
- **Type inference**: Automatic type tracking for both patterns
- **Performance**: C-equivalent speed for both immutable and mutable variables


## LLVM IR Patterns

### Immutable Variable (SSA Style)
```llvm
%x = add i64 5, 10     ; Direct SSA value
%result = mul i64 %x, 2
```

### Mutable Variable (Alloca Pattern)
```llvm
%x = alloca i64, align 8        ; Stack allocation
store i64 10, ptr %x, align 4   ; Store initial value

%x_val = load i64, ptr %x, align 4   ; Load current value
%new_val = add i64 %x_val, 5
store i64 %new_val, ptr %x, align 4  ; Store new value
```

## Examples

### Basic Mutability
```franz
// Immutable
x = 10
y = (add x 5)
(println y)  // → 15

// Mutable
mut counter = 0
counter = (add counter 1)
counter = (add counter 1)
(println counter)  // → 2
```

### Loop with Accumulation
```franz
mut sum = 0
(loop 10 {i ->
  sum = (add sum i)
})
(println "Sum:" sum)  // → Sum: 45
```

### Factorial Calculation
```franz
mut factorial = 1
(loop 5 {i ->
  n = (add i 1)
  factorial = (multiply factorial n)
})
(println "5! =" factorial)  // → 5! = 120
```

### Multiple Mutable Variables
```franz
mut x = 0
mut y = 0

(loop 5 {i ->
  x = (add x i)
  y = (multiply i 2)
})

(println "x:" x "y:" y)  // → x: 10 y: 8
```

## Error Handling

### Immutable Reassignment Error
```franz
x = 10
x = 20  // ERROR

// Output:
// ERROR: Cannot reassign immutable variable 'x' at line 2
// Hint: Declare the variable with 'mut' to make it mutable: mut x = ...
```

## Performance

### Benchmarks

**Immutable variables:**
- **Overhead:** Zero (SSA-style direct values)
- **Memory:** No allocation (register-only or inlined)
- **Speed:** Identical to handwritten C

**Mutable variables:**
- **Overhead:** One stack allocation (alloca)
- **Memory:** Stack-allocated (very fast)
- **Speed:** C-equivalent (LLVM optimizes load/store patterns)

### Comparison with Other Languages

| Language | Mutability Model | Performance |
|----------|------------------|-------------|
| **Franz** | Immutable by default, `mut` keyword | C-equivalent |
| **Rust** | Immutable by default, `mut` keyword | C-equivalent |
| **OCaml** | Immutable by default, `ref` for mutability | Similar to Franz |
| **JavaScript** | Mutable by default, `const` for immutability | V8-optimized |
| **Python** | Mutable by default | Interpreted (slower) |

## Integration with Other Features

### ✅ Loop Support
Mutable variables work seamlessly with loops:
```franz
mut sum = 0
(loop 100 {i -> sum = (add sum i)})
```

### ✅ Type System
Type inference works with both immutable and mutable variables:
```franz
mut x = 5       // Inferred as int
mut y = 3.14    // Inferred as float
```

### ✅ Control Flow
Mutable variables work with if/when/unless/cond:
```franz
mut score = 0
(when (greater_than age 18)
  {score = (add score 10)}
)
```

## Conclusion

The `mut` keyword brings **Rust-style explicit mutability** to Franz, providing:
- ✅ **Safety:** Immutability by default prevents accidental mutations
- ✅ **Clarity:** `mut` explicitly marks mutable state
- ✅ **Performance:** Zero overhead for immutable, C-equivalent for mutable
- ✅ **Flexibility:** Enables accumulator patterns and stateful algorithms
- ✅ **Modern:** Follows industry best practices (Rust, OCaml patterns)

 **** and fully integrated with Franz's LLVM native compilation pipeline.

---
