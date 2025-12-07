# Boolean Logic Functions (`not`, `and`, `or`)

## Overview

Franz provides three boolean logic operators for working with binary values (0 and 1): `not`, `and`, and `or`. These functions implement standard propositional logic with support for variadic arguments (and/or).


**Location:**
- Implementation: [src/stdlib.c](../../src/stdlib.c#L777-L837)
- Registration: [src/stdlib.c](../../src/stdlib.c#L3216-L3218)

---

## API Reference

### `not`

**Syntax:**
```franz
(not value)
```

**Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | integer (0 or 1) | Boolean value to negate |

**Returns:**
- **Type:** `integer`
- **Value:** `1` if value is `0`, `0` if value is `1`

**Truth Table:**
| Input | Output |
|-------|--------|
| 0 | 1 |
| 1 | 0 |

---

### `and`

**Syntax:**
```franz
(and a b ...)
```

**Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `a, b, ...` | integer (0 or 1) | 2 or more boolean values |

**Returns:**
- **Type:** `integer`
- **Value:** `1` if **all** values are `1`, `0` otherwise

**Truth Table:**
| A | B | A AND B |
|---|---|---------|
| 0 | 0 | 0 |
| 0 | 1 | 0 |
| 1 | 0 | 0 |
| 1 | 1 | 1 |

---

### `or`

**Syntax:**
```franz
(or a b ...)
```

**Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `a, b, ...` | integer (0 or 1) | 2 or more boolean values |

**Returns:**
- **Type:** `integer`
- **Value:** `1` if **at least one** value is `1`, `0` if all are `0`

**Truth Table:**
| A | B | A OR B |
|---|---|--------|
| 0 | 0 | 0 |
| 0 | 1 | 1 |
| 1 | 0 | 1 |
| 1 | 1 | 1 |

---

## Logical Operations

### 1. Negation (NOT)

**Simple negation:**
```franz
(not 0)  // → 1
(not 1)  // → 0
```

**Double negation (identity):**
```franz
(not (not 0))  // → 0
(not (not 1))  // → 1
```

**Implementation:** (src/stdlib.c:785-786)
```c
int *p_res = (int *) malloc(sizeof(int));
*p_res = 1 - *((int *) args[0]->p_val);  // Flip 0↔1
```

---

### 2. Conjunction (AND)

**All must be true:**
```franz
(and 1 1 1)  // → 1 (all true)
(and 1 0 1)  // → 0 (one false)
(and 0 0 0)  // → 0 (all false)
```

**Variadic (2+ arguments):**
```franz
(and 1 1)          // → 1 (two args)
(and 1 1 1 1 1)    // → 1 (five args)
(and 1 1 0 1 1)    // → 0 (one false among many)
```

**Implementation:** (src/stdlib.c:808-810)
```c
for (int i = 0; i < length; i++) {
  *p_res = *p_res && *((int *) args[i]->p_val);  // Logical AND
}
```

---

### 3. Disjunction (OR)

**At least one must be true:**
```franz
(or 0 0 1)  // → 1 (one true)
(or 0 0 0)  // → 0 (all false)
(or 1 1 1)  // → 1 (all true)
```

**Variadic (2+ arguments):**
```franz
(or 1 0)           // → 1 (two args)
(or 0 0 0 1 0)     // → 1 (one true among many)
(or 0 0 0 0 0)     // → 0 (all false)
```

**Implementation:** (src/stdlib.c:832-834)
```c
for (int i = 0; i < length; i++) {
  *p_res = *p_res || *((int *) args[i]->p_val);  // Logical OR
}
```

---

## Usage Patterns

### Pattern 1: Simple Condition Negation

```franz
is_ready = 0
not_ready = (not is_ready)
(if not_ready
  {(println "Not ready yet")}
  {(println "Ready!")}
)
```

### Pattern 2: Multiple Conditions (AND)

```franz
age = 25
has_license = 1
can_drive = (and (greater_than age 18) has_license)

(if can_drive
  {(println "Can drive")}
  {(println "Cannot drive")}
)
```

### Pattern 3: Alternative Conditions (OR)

```franz
is_weekend = 0
is_holiday = 1
can_relax = (or is_weekend is_holiday)

(if can_relax
  {(println "Time to relax!")}
  {(println "Work day")}
)
```

### Pattern 4: Range Validation (AND)

```franz
temperature = 72
is_comfortable = (and
  (greater_than temperature 60)
  (less_than temperature 80)
)

(println "Temperature comfortable? " (if is_comfortable {<- "Yes"} {<- "No"}))
```

### Pattern 5: XOR Implementation (Exclusive OR)

```franz
xor = {a b ->
  <- (and (or a b) (not (and a b)))
}

result = (xor 1 0)  // → 1 (different values)
(println "XOR(1, 0) = " result)
```

### Pattern 6: NAND Implementation

```franz
nand = {a b ->
  <- (not (and a b))
}

result = (nand 1 1)  // → 0 (opposite of AND)
(println "NAND(1, 1) = " result)
```

### Pattern 7: NOR Implementation

```franz
nor = {a b ->
  <- (not (or a b))
}

result = (nor 0 0)  // → 1 (opposite of OR)
(println "NOR(0, 0) = " result)
```

### Pattern 8: Implies (Material Conditional)

```franz
implies = {a b ->
  <- (or (not a) b)
}

result = (implies 1 1)  // → 1 (true → true)
(println "1 implies 1? " result)
```

### Pattern 9: Complex Validation

```franz
validate_user = {age is_verified has_email ->
  age_ok = (greater_than age 13)
  identity_ok = (or is_verified has_email)
  <- (and age_ok identity_ok)
}

valid = (validate_user 25 1 0)  // → 1 (age > 13, verified)
(println "User valid? " (if valid {<- "Yes"} {<- "No"}))
```

### Pattern 10: Access Control Logic

```franz
check_access = {is_admin is_moderator is_owner is_premium ->
  admin_or_mod = (or is_admin is_moderator)
  owner_and_premium = (and is_owner is_premium)
  <- (or admin_or_mod owner_and_premium)
}

has_access = (check_access 0 1 0 0)  // → 1 (moderator)
(println "Access? " (if has_access {<- "Granted"} {<- "Denied"}))
```

---

## Boolean Algebra Laws

### Identity Laws

```franz
x = 1

// AND identity: (and 1 x) == x
(is (and 1 x) x)  // → 1 (true)

// OR identity: (or 0 x) == x
(is (or 0 x) x)   // → 1 (true)
```

### De Morgan's Laws

**Law 1:** `not(a AND b) = (not a) OR (not b)`

```franz
a = 1
b = 0

left = (not (and a b))
right = (or (not a) (not b))

(is left right)  // → 1 (laws hold)
```

**Law 2:** `not(a OR b) = (not a) AND (not b)`

```franz
a = 1
b = 0

left = (not (or a b))
right = (and (not a) (not b))

(is left right)  // → 1 (laws hold)
```

### Distributive Law

**AND distributes over OR:** `p AND (q OR r) = (p AND q) OR (p AND r)`

```franz
p = 1
q = 0
r = 1

left = (and p (or q r))
right = (or (and p q) (and p r))

(is left right)  // → 1 (distributive law holds)
```

### Absorption Law

**AND-OR absorption:** `x AND (x OR y) = x`

```franz
x = 1
y = 0

result = (and x (or x y))
(is result x)  // → 1 (absorption holds)
```

---

## Edge Cases

### Multiple Negations

```franz
(not (not (not 1)))         // → 0 (triple negation)
(not (not (not (not 1))))   // → 1 (quadruple negation)
```

### Nested AND/OR

```franz
(and (or 1 0) 1)   // → 1 (nested: or→1, and→1)
(or (and 0 1) 1)   // → 1 (nested: and→0, or→1)
(and (or 0 0) 1)   // → 0 (nested: or→0, and→0)
```

### Variadic Arguments

```franz
(and 1 1 1 1 1 1 1)  // → 1 (many arguments, all true)
(and 0 0 0 0 0 0 0)  // → 0 (many arguments, all false)
(or 1 1 1 1 1 1 1)   // → 1 (many arguments, at least one true)
(or 0 0 0 0 0 0 0)   // → 0 (many arguments, all false)
```

### Comparison Results

```franz
cmp1 = (less_than 3 5)       // → 1
cmp2 = (greater_than 10 3)   // → 1

(and cmp1 cmp2)  // → 1 (both comparisons true)
```

### Complex Nested Logic

```franz
(not (and (or 0 1) (or 0 0)))  // → 1
// Step 1: (or 0 1) → 1
// Step 2: (or 0 0) → 0
// Step 3: (and 1 0) → 0
// Step 4: (not 0) → 1
```

---

## Examples

### Basic Examples

See: [examples/boolean-logic/working/basic-boolean.franz](../../examples/boolean-logic/working/basic-boolean.franz)

- Simple NOT operation
- AND with two conditions (driving check)
- OR for multiple valid options (weekend/holiday)
- Range validation with AND (temperature)
- Multiple OR conditions (color validation)
- Negating a comparison (fail check)
- Complex condition (AND + OR qualification)

**Run:**
```bash
./franz examples/boolean-logic/working/basic-boolean.franz
```

### Advanced Examples

See: [examples/boolean-logic/working/advanced-boolean.franz](../../examples/boolean-logic/working/advanced-boolean.franz)

- XOR implementation (exclusive OR)
- NAND implementation (not AND)
- NOR implementation (not OR)
- Implies implementation (material conditional)
- De Morgan's Laws verification
- Three-way AND condition checker
- Any true checker (variadic OR)
- Validation with multiple AND/OR
- Boolean algebra - Distributive law
- Access control with complex logic

**Run:**
```bash
./franz examples/boolean-logic/working/advanced-boolean.franz
```

---



**Edge Cases Tested:**
- Triple/quadruple negation
- AND/OR with comparison results
- Nested AND/OR combinations
- NOT with AND/OR results
- De Morgan's Laws (both)
- Variable references
- Chained comparisons
- Complex nested logic
- Many arguments (7+ values)
- Identity laws (AND 1 x, OR 0 x)
- Absorption law

---


## Performance Analysis

### Time Complexity

| Operation | Complexity | Notes |
|-----------|------------|-------|
| `not` | O(1) | Single subtraction (1 - value) |
| `and` | O(n) | Linear scan, n = number of arguments |
| `or` | O(n) | Linear scan, n = number of arguments |

### Memory Usage

- **NOT:** Single `int` allocation (4 bytes)
- **AND/OR:** Single `int` allocation (4 bytes), regardless of argument count
- **No temporary allocations** for intermediate results

### Short-Circuit Evaluation

**Note:** Franz boolean operators do **NOT** short-circuit. All arguments are evaluated before the function call.

```franz
// Both expressions evaluated even if first is false
(and 0 (some_expensive_function))  // some_expensive_function() still runs

// Workaround: Use nested if for short-circuit behavior
(if 0
  {<- (some_expensive_function)}
  {<- 0}
)
```

---

## Language Comparison

| Language | NOT | AND | OR | Variadic AND/OR | Short-Circuit |
|----------|-----|-----|-----|-----------------|---------------|
| **Franz** | `(not x)` | `(and a b ...)` | `(or a b ...)` | ✅ Yes | ❌ No |
| Python | `not x` | `a and b` | `a or b` | ❌ No (binary only) | ✅ Yes |
| JavaScript | `!x` | `a && b` | `a \|\| b` | ❌ No (binary only) | ✅ Yes |
| Scheme/Lisp | `(not x)` | `(and a b ...)` | `(or a b ...)` | ✅ Yes | ✅ Yes (macros) |
| OCaml | `not x` | `a && b` | `a \|\| b` | ❌ No (binary only) | ✅ Yes |
| C | `!x` | `a && b` | `a \|\| b` | ❌ No (binary only) | ✅ Yes |

**Franz's Approach:**
- **Variadic AND/OR** like Scheme/Lisp (accept 2+ arguments)
- **No short-circuit** - all arguments evaluated (unlike most languages)
- **S-expression syntax** - function call semantics
- **Binary validation** - strict 0/1 values required

---

## Troubleshooting

### Issue: "Function expects 1 arguments but got 0"

**Cause:** Missing argument to `not`

**Fix:**
```franz
// ❌ Wrong: No argument
(not)

// ✅ Correct: One argument
(not 0)
```

### Issue: "Type error: Expected int, got string"

**Cause:** Passing non-integer values

**Expected behavior:** Only 0 or 1 allowed

```franz
// ❌ Wrong: String value
(not "false")

// ✅ Correct: Integer 0 or 1
(not 0)
```

### Issue: "Value must be 0 or 1 (binary)"

**Cause:** Integer value outside 0/1 range

**Fix:**
```franz
// ❌ Wrong: Non-binary integer
(not 5)  // Error: must be 0 or 1

// ✅ Correct: Binary values only
(not 0)  // → 1
(not 1)  // → 0
```

### Issue: No short-circuit evaluation

**Problem:** Expensive function still executes even when result is known

**Workaround:** Use nested `if` statements

```franz
// ❌ Problem: Both expressions evaluated
result = (and 0 (expensive_computation))  // expensive_computation() runs

// ✅ Solution: Manual short-circuit with if
result = (if 0
  {<- (expensive_computation)}
  {<- 0}
)  // expensive_computation() NOT run
```

### Issue: "Function expects at least 2 arguments"

**Cause:** AND/OR called with only 1 argument

**Fix:**
```franz
// ❌ Wrong: Only one argument
(and 1)

// ✅ Correct: Two or more arguments
(and 1 1)
(and 1 1 1)
```

---

## Best Practices

1. **Use 0 and 1 explicitly** - Avoid other integers (will error)
2. **Comparison results are valid** - `less_than`, `greater_than`, `is` return 0/1
3. **Chain with AND/OR** - Combine multiple conditions naturally
4. **Implement derived operators** - Build XOR, NAND, NOR, IMPLIES from primitives
5. **Boolean algebra applies** - Identity, De Morgan's, distributive laws all hold
6. **No short-circuit** - Consider performance if using expensive functions
7. **Variadic AND/OR** - Use for checking multiple conditions at once

---

## Related Functions

- `(is a b)` - Equality comparison (returns 0 or 1)
- `(less_than a b)` - Less than comparison (returns 0 or 1)
- `(greater_than a b)` - Greater than comparison (returns 0 or 1)
- `(if cond then else)` - Conditional execution
- `(type value)` - Type checking

---

## References

- **Implementation:** [src/stdlib.c](../../src/stdlib.c#L777-L837)
- **Tests:** [test/boolean-logic/](../../test/boolean-logic/)
- **Examples:** [examples/boolean-logic/working/](../../examples/boolean-logic/working/)
- **Bytecode Checklist:** [docs/bytecode/BYTECODE_TEST_CHECKLIST.md](../bytecode/BYTECODE_TEST_CHECKLIST.md)
- **Comparison Functions:** [docs/ordering/ordering.md](../ordering/ordering.md)
- **Equality Function:** [docs/equality/equality.md](../equality/equality.md)
