# Franz If Statement - LLVM Native Compilation


---

## Overview

Franz's `if` statement provides conditional branching compiled directly to LLVM IR using basic blocks, branch instructions, and PHI nodes. This achieves C-level performance with zero runtime overhead.

**Key Features**:
- ✅ **LLVM branch instructions** - Direct CPU conditional jumps
- ✅ **PHI nodes** - Type-safe result merging from both branches
- ✅ **Automatic type promotion** - INT ↔ FLOAT conversions handled transparently
- ✅ **Truthy value normalization** - Any non-zero value treated as true
- ✅ **Nested if support** - Arbitrary nesting depth
- ✅ **Zero runtime overhead** - Direct LLVM IR → machine code

---

## Syntax

```franz
(if condition then-expr else-expr)
```

**Parameters**:
- `condition` - Expression evaluated as boolean (0 = false, non-zero = true)
- `then-expr` - Expression evaluated if condition is true
- `else-expr` - Expression evaluated if condition is false

**Returns**: Value from the executed branch (then or else)

---

## Basic Examples

### Example 1: Simple True/False Condition

```franz
result = (if 1 10 20)
(println result)  // Output: 10

result = (if 0 10 20)
(println result)  // Output: 20
```

### Example 2: Truthy Values (Non-Zero)

```franz
(if 5 "yes" "no")     // → "yes" (5 is truthy)
(if -1 "yes" "no")    // → "yes" (-1 is truthy)
(if 0 "yes" "no")     // → "no" (0 is falsy)
```

### Example 3: With Comparisons

```franz
age = 25
is_adult = (if (greater_than age 17) 1 0)
(println "Is adult:" is_adult)  // Output: Is adult: 1
```

### Example 4: Max of Two Numbers

```franz
x = 42
y = 17
max = (if (greater_than x y) x y)
(println "Max:" max)  // Output: Max: 42
```

### Example 5: Absolute Value

```franz
num = -5
abs_val = (if (less_than num 0) (multiply num -1) num)
(println "Abs:" abs_val)  // Output: Abs: 5
```

---

## Advanced Examples

### Example 6: Nested If Statements

```franz
score = 85
grade = (if (greater_than score 89)
  "A"
  (if (greater_than score 79)
    "B"
    (if (greater_than score 69)
      "C"
      "F")))
(println "Grade:" grade)  // Output: Grade: B
```

### Example 7: Sign Function (-1, 0, 1)

```franz
value = 10
sign = (if (greater_than value 0)
  1
  (if (less_than value 0) -1 0))
(println "Sign:" sign)  // Output: Sign: 1
```

### Example 8: Arithmetic in Branches

```franz
flag = 1
result = (if flag
  (multiply (add 2 3) 4)    // (2+3) * 4 = 20
  (divide 100 5))           // 100/5 = 20
(println result)  // Output: 20
```

### Example 9: Logical Operators with If

```franz
age = 25
has_license = 1
can_drive = (if (and (greater_than age 17) has_license) 1 0)
(println "Can drive:" can_drive)  // Output: Can drive: 1
```

### Example 10: Mixed Types (Int/Float)

```franz
use_float = 1
result = (if use_float 3.14 42)
(println result)  // Output: 3.14

// Type promotion happens automatically
result = (if 1 5 3.14)      // INT in then, FLOAT in else
(println result)  // Output: 5.000000 (promoted to float)
```

---

## Real-World Use Cases

### Use Case 1: Age-Based Ticket Pricing

```franz
visitor_age = 25
base_price = 100
ticket_price = (if (less_than visitor_age 18)
  (multiply base_price 0.5)        // 50% discount for children
  (if (greater_than visitor_age 64)
    (multiply base_price 0.7)      // 30% discount for seniors
    base_price))                   // Full price for adults
(println "Ticket price:" ticket_price)  // Output: 100.0
```

### Use Case 2: Conditional Discount

```franz
order_total = 150
discount_price = (if (greater_than order_total 100)
  (multiply order_total 0.9)       // 10% discount if >$100
  (multiply order_total 0.95))     // 5% discount otherwise
(println "Final price:" discount_price)  // Output: 135.0
```

### Use Case 3: BMI Health Categories

```franz
bmi = 24
category = (if (less_than bmi 18)
  "Underweight"
  (if (less_than bmi 25)
    "Normal"
    (if (less_than bmi 30)
      "Overweight"
      "Obese")))
(println "BMI Category:" category)  // Output: Normal
```

### Use Case 4: Loan Approval Logic

```franz
credit_score = 720
income = 60000
approved = (if (and (greater_than credit_score 650) (greater_than income 40000))
  1
  0)
status = (if approved "APPROVED" "DENIED")
(println "Loan status:" status)  // Output: APPROVED
```

---

## Type Handling

### Automatic Type Promotion

Franz automatically promotes INT to FLOAT when branches return mixed types:

```franz
// INT (then) and FLOAT (else)
result = (if 1 42 3.14)
// Result: 42.000000 (INT promoted to FLOAT)

// FLOAT (then) and INT (else)
result = (if 0 3.14 42)
// Result: 42.000000 (INT promoted to FLOAT)
```

**LLVM Implementation**:
```llvm
; Insert SIToFP instruction before branch terminator
%then_promoted = sitofp i64 %then_val to double
%result = phi double [ %then_promoted, %then ], [ %else_val, %else ]
```

### String Results

```franz
has_access = 1
message = (if has_access "Access granted" "Access denied")
(println message)  // Output: Access granted
```

---


### Running Tests

```bash
# Compile Franz (if not already compiled)
find src -name "*.c" -not -name "check.c" | xargs gcc \
  -I/opt/homebrew/Cellar/llvm@17/17.0.6/include \
  -L/opt/homebrew/Cellar/llvm@17/17.0.6/lib \
  -lLLVM-17 -Wall -lm -o franz

# Run comprehensive test suite
./franz test/llvm-control-flow/if-comprehensive-test.franz

# Expected output:
# === All 45 Tests Complete ===
# Review results above to verify correctness
```

### Working Examples

**Location**: [examples/llvm-control-flow/working/](../../examples/llvm-control-flow/working/)

1. **[if-basic.franz](../../examples/llvm-control-flow/working/if-basic.franz)** - Basic if patterns
2. **[if-advanced.franz](../../examples/llvm-control-flow/working/if-advanced.franz)** - Nested and complex patterns
3. **[if-real-world.franz](../../examples/llvm-control-flow/working/if-real-world.franz)** - Practical use cases

```bash
# Run working examples
./franz examples/llvm-control-flow/working/if-basic.franz
./franz examples/llvm-control-flow/working/if-advanced.franz
./franz examples/llvm-control-flow/working/if-real-world.franz
```

---

## Memory Safety

Franz's if statement implementation is memory-safe with proper cleanup:

```bash
# Check for memory leaks (macOS)
leaks -atExit -- ./franz test/llvm-control-flow/if-comprehensive-test.franz

# Expected: No leaks detected
```

**Verification**:
- ✅ No memory leaks in 45 comprehensive tests
- ✅ Proper cleanup of temporary values
- ✅ PHI nodes correctly managed
- ✅ Type conversion values freed

---

## Technical Implementation

### File Structure

```
src/llvm-control-flow/
├── llvm_control_flow.h      # Function prototypes
└── llvm_control_flow.c      # If statement implementation

src/llvm-codegen/
└── llvm_ir_gen.c            # Dispatcher routing (lines 988-989)
```

### Key Functions

**`LLVMCodeGen_compileIf(LLVMCodeGen *gen, AstNode *node)`**
- Validates 3 arguments (condition, then, else)
- Compiles condition and converts to i1 boolean
- Creates three basic blocks (then, else, merge)
- Emits conditional branch instruction
- Compiles both branches
- Creates PHI node for result merging
- Handles automatic type promotion

### Dispatcher Integration

```c
// src/llvm-codegen/llvm_ir_gen.c (lines 988-989)
} else if (strcmp(funcName, "if") == 0) {
  return LLVMCodeGen_compileIf(gen, &argNode);
```

---

## Comparison with Other Languages

| Language | Syntax | Performance | Type Handling |
|----------|--------|-------------|---------------|
| **Franz** | `(if cond then else)` | C-level (LLVM) | Auto promotion |
| C | `cond ? then : else` | Native | Strict types |
| Python | `then if cond else else` | Interpreted | Dynamic |
| JavaScript | `cond ? then : else` | JIT compiled | Coercion |
| Rust | `if cond { then } else { else }` | Native (LLVM) | Strict types |

Franz matches **C and Rust performance** while providing **dynamic type handling** similar to Python/JavaScript.

---

## Troubleshooting

### Issue 1: "Attempt to call non-function value"

**Cause**: Using block syntax `{<- value}` instead of direct values

```franz
// ❌ WRONG - Blocks are treated as functions
result = (if 1 {<- 10} {<- 20})

// ✅ CORRECT - Use direct values
result = (if 1 10 20)
```

### Issue 2: Type mismatch errors

**Cause**: Incompatible types in branches (e.g., string and number)

```franz
// ❌ WRONG - String and INT are incompatible
result = (if 1 "hello" 42)

// ✅ CORRECT - Same type in both branches
result = (if 1 "hello" "world")
result = (if 1 42 100)
```

### Issue 3: Condition always true

**Cause**: Using truthy values instead of comparisons

```franz
// ❌ WRONG - This always returns "yes" (5 is truthy)
result = (if 5 "yes" "no")

// ✅ CORRECT - Use explicit comparison
x = 5
result = (if (greater_than x 0) "yes" "no")
```

---
