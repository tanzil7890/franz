# Franz LLVM Logical Operators Documentation



## Overview

Franz  implements **three  logical operators** that work in both interpreter and LLVM native compilation modes:

- **not** - Logical NOT (unary inversion)
- **and** - Logical AND (variadic, 2+ arguments)
- **or** - Logical OR (variadic, 2+ arguments)

All operators work with **integers as booleans** (0 = false, non-zero = true) and return **0 or 1**.

---

## API Reference

### 1. NOT - Logical NOT

**Syntax:** `(not value)`

**Description:** Inverts a boolean value (0 → 1, non-zero → 0)

**Arguments:**
- `value` (integer) - Value to invert

**Returns:** Integer (0 or 1)

**Examples:**
```franz
(not 0)      // → 1 (false becomes true)
(not 1)      // → 0 (true becomes false)
(not 5)      // → 0 (truthy becomes false)
(not -1)     // → 0 (truthy becomes false)

// Double negation
(not (not 1))  // → 1 (inverts twice)
```

**LLVM IR Implementation:**
```llvm
; Step 1: Convert to boolean (0 or 1)
%is_nonzero = icmp ne i64 %value, 0
%bool_to_int = zext i1 %is_nonzero to i64

; Step 2: XOR with 1 to invert
%result = xor i64 %bool_to_int, 1
```

**Use Cases:**
- Flip boolean flags
- Negate conditions
- Implement "opposite" logic

---

### 2. AND - Logical AND

**Syntax:** `(and val1 val2 ...)`

**Description:** Returns 1 if ALL arguments are non-zero (truthy), 0 otherwise

**Arguments:**
- `val1, val2, ...` (integers) - Values to check (minimum 2)

**Returns:** Integer (0 or 1)

**Examples:**
```franz
// Basic AND
(and 1 1)          // → 1 (both true)
(and 1 0)          // → 0 (one false)
(and 0 0)          // → 0 (both false)

// Variadic AND
(and 1 1 1)        // → 1 (all true)
(and 1 1 0 1)      // → 0 (one false)
(and 5 3 1)        // → 1 (all truthy)

// With comparisons
(and (is 5 5) (greater_than 10 3))  // → 1 (both true)

// Complex conditions
age = 25
(and (greater_than age 17) (less_than age 65))  // → 1
```

**LLVM IR Implementation:**
```llvm
; Initialize result to 1 (true)
%result = i64 1

; For each argument:
%is_nonzero = icmp ne i64 %arg, 0
%bool_to_int = zext i1 %is_nonzero to i64

; AND with current result (using multiplication since values are 0/1)
%result = mul i64 %result, %bool_to_int
```

**Use Cases:**
- Validate multiple conditions
- Access control (all permissions required)
- Form validation (all fields must be valid)
- Multi-step verification

---

### 3. OR - Logical OR

**Syntax:** `(or val1 val2 ...)`

**Description:** Returns 1 if ANY argument is non-zero (truthy), 0 otherwise

**Arguments:**
- `val1, val2, ...` (integers) - Values to check (minimum 2)

**Returns:** Integer (0 or 1)

**Examples:**
```franz
// Basic OR
(or 0 0)           // → 0 (both false)
(or 0 1)           // → 1 (one true)
(or 1 1)           // → 1 (both true)

// Variadic OR
(or 0 0 0)         // → 0 (all false)
(or 0 0 1 0)       // → 1 (one true)
(or 5 0 0)         // → 1 (first is truthy)

// With comparisons
(or (is 5 3) (greater_than 10 3))  // → 1 (second is true)

// Complex conditions
is_admin = 0
is_owner = 1
(or is_admin is_owner)  // → 1 (one is true)
```

**LLVM IR Implementation:**
```llvm
; Initialize result to 0 (false)
%result = i64 0

; For each argument:
%is_nonzero = icmp ne i64 %arg, 0
%bool_to_int = zext i1 %is_nonzero to i64

; OR with current result (using LLVM OR instruction)
%result = or i64 %result, %bool_to_int
```

**Use Cases:**
- Alternative conditions
- Feature flags (any flag enables feature)
- Payment methods (any method works)
- Fallback logic

---

## Advanced Usage

### 1. De Morgan's Laws

**Law 1:** NOT (A AND B) = (NOT A) OR (NOT B)

```franz
a = 1
b = 0

left = (not (and a b))              // → 1
right = (or (not a) (not b))        // → 1
(is left right)                      // → 1 (equal)
```

**Law 2:** NOT (A OR B) = (NOT A) AND (NOT B)

```franz
a = 0
b = 0

left = (not (or a b))               // → 1
right = (and (not a) (not b))       // → 1
(is left right)                      // → 1 (equal)
```

---

### 2. Nested Boolean Logic

```franz
// Complex access control
has_password = 1
has_2fa = 1
is_verified = 1
is_device_trusted = 0

// Require password AND (2FA OR trusted device) AND verification
can_login = (and has_password (or has_2fa is_device_trusted) is_verified)
(println "Can login:" can_login)  // → 1
```

---

### 3. Range Validation

```franz
// Check if value is in range [10, 100]
value = 50
in_range = (and (greater_than value 9) (less_than value 101))
(println "In range:" in_range)  // → 1
```

---

### 4. Feature Flags

```franz
// Enable feature if any flag is on AND beta is enabled
flag1 = 0
flag2 = 1
flag3 = 0
beta_enabled = 1

feature_enabled = (and (or flag1 flag2 flag3) beta_enabled)
(println "Feature enabled:" feature_enabled)  // → 1
```

---

## Performance

### Interpreter Mode
- **Time Complexity:** O(1) for NOT, O(n) for AND/OR (n = argument count)
- **Implementation:** Direct C integer operations
- **Overhead:** Function call + type validation

### LLVM Native Mode
- **Time Complexity:** Same as interpreter, but constant-time is faster
- **Implementation:** Direct CPU instructions (xor, icmp, or)
- **Overhead:** Zero runtime overhead (compiled to machine code)
- **Performance:** **C-level speed** (same as handwritten C code)

---

# Example

```franz
(println "(not 0) =" (not 0))    // → 1
(println "(and 1 1) =" (and 1 1))  // → 1
(println "(or 0 1) =" (or 0 1))    // → 1
```

### Real-World Demo
**File:** [examples/llvm-logical/working/logical-demo.franz](../../examples/llvm-logical/working/logical-demo.franz)

9 practical examples including:
- Access control
- Age validation
- Feature flags
- Input validation
- De Morgan's laws

### Conditionals Demo
**File:** [examples/llvm-logical/working/conditionals-demo.franz](../../examples/llvm-logical/working/conditionals-demo.franz)

6 conditional logic patterns including:
- Login systems
- Range validation
- Multi-factor authentication

---

## Error Handling

### Argument Count Errors

```franz
// NOT requires exactly 1 argument
(not)        // ERROR: not requires exactly 1 argument
(not 1 2)    // ERROR: not requires exactly 1 argument

// AND requires at least 2 arguments
(and)        // ERROR: and requires at least 2 arguments
(and 1)      // ERROR: and requires at least 2 arguments

// OR requires at least 2 arguments
(or)         // ERROR: or requires at least 2 arguments
(or 1)       // ERROR: or requires at least 2 arguments
```

### Type Errors

```franz
// All operators require integer arguments
(not "hello")     // ERROR: not requires integer argument
(and 1 "test")    // ERROR: and requires integer arguments
(or 1 3.14)       // ERROR: or requires integer arguments
```

---

## Comparison with Other Languages

### C
```c
int not = !a;
int and = a && b;
int or = a || b;
```

### JavaScript
```javascript
let not = !a;
let and = a && b;
let or = a || b;
```

### Franz
```franz
not = (not a)
and = (and a b)
or = (or a b)
```

**Franz advantages:**
- Variadic AND/OR (unlimited arguments)
- Consistent S-expression syntax
- Works identically in interpreter and LLVM modes
- Zero performance penalty in LLVM mode

---

## Best Practices

### 1. Use Descriptive Variable Names
```franz
// Good
is_authenticated = (and has_password has_token)

// Bad
x = (and a b)
```

### 2. Break Complex Conditions Into Steps
```franz
// Good
is_adult = (greater_than age 17)
not_senior = (less_than age 65)
is_valid_age = (and is_adult not_senior)

// Bad (hard to read)
is_valid_age = (and (greater_than age 17) (less_than age 65))
```

### 3. Use Variadic AND/OR When Possible
```franz
// Good
all_flags = (and flag1 flag2 flag3 flag4)

// Bad (nested)
all_flags = (and (and (and flag1 flag2) flag3) flag4)
```

### 4. Leverage De Morgan's Laws
```franz
// These are equivalent:
(not (and a b))           // NOT (A AND B)
(or (not a) (not b))      // (NOT A) OR (NOT B)
```

---
