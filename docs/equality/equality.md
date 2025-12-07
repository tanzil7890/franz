# Equality Function (`is`)

## Overview

The `is()` function provides value equality comparison in Franz, supporting all Franz types with intelligent numeric coercion and deep structural comparison for composite types.

---

## API Reference

### Syntax

```franz
(is value1 value2)
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `value1` | any | First value to compare |
| `value2` | any | Second value to compare |

### Returns

- **Type:** `integer`
- **Value:** `1` if values are equal, `0` otherwise

---

## Comparison Rules

### 1. Numeric Coercion

**Integers and floats are compared numerically** - type differences are ignored:

```franz
(is 5 5.0)      // → 1 (equal via numeric coercion)
(is 1.0 1)      // → 1 (equal via numeric coercion)
(is -42 -42.0)  // → 1 (equal via numeric coercion)
```

h (src/generic.c:158-166)
```c
// Numeric coercion: int and float compared as numbers
if ((a->type == TYPE_INT || a->type == TYPE_FLOAT) &&
    (b->type == TYPE_INT || b->type == TYPE_FLOAT)) {
  return (a->type == TYPE_FLOAT ? *((double *) a->p_val) : *((int *) a->p_val))
      == (b->type == TYPE_FLOAT ? *((double *) b->p_val) : *((int *) b->p_val));
}
```

### 2. String Comparison

**Case-sensitive lexicographic comparison:**

```franz
(is "hello" "hello")    // → 1 (identical)
(is "Hello" "hello")    // → 0 (case sensitive)
(is "" "")              // → 1 (empty strings equal)
```

h Uses `strcmp()` for C-string comparison

### 3. List Comparison

**Deep structural comparison** - recursively compares all elements:

```franz
(is (list 1 2 3) (list 1 2 3))           // → 1 (identical)
(is (list 1 2 3) (list 3 2 1))           // → 0 (different order)
(is (list (list 1 2)) (list (list 1 2))) // → 1 (nested lists)
(is (list) (list))                       // → 1 (empty lists)
```

h Delegates to `List_compare()` for element-by-element comparison

### 4. Function/Closure Comparison

**Reference equality** - only equal if same function object:

```franz
f = {x -> <- (add x 1)}
g = {x -> <- (add x 1)}

(is f f)  // → 1 (same reference)
(is f g)  // → 0 (different references, even if identical code)
```

### 5. Void Comparison

**All void values are equal:**

```franz
(is void void)    // → 1 (all voids equal)
v1 = void
v2 = void
(is v1 v2)        // → 1 (all voids equal)
```

Additional guarantees (LLVM + interpreter):

- `(is value void)` works for literal `void`, variables assigned to `void`, and closure parameters.
- `void` values never alias numeric zero — `(is 0 void)` always returns `0`.
- Example coverage: [`examples/equality/working/void-comparison.franz`](../../examples/equality/working/void-comparison.franz).

h `src/llvm-comparisons/llvm_comparisons.c` tracks runtime type tags so void operands bypass the numeric fast path and compare only against other voids.

### 6. Different Type Comparison

**Different types are never equal** (except int/float):

```franz
(is 5 "5")        // → 0 (int vs string)
(is 0 void)       // → 0 (int vs void — numeric zero is never treated as void)
(is (list) "")    // → 0 (list vs string)
```

---

## Usage Patterns

### Pattern 1: Type Checking

```franz
check_value = {val expected ->
  (if (is val expected)
    {(println "✓ Value matches expected: " expected)}
    {(println "✗ Value mismatch! Expected: " expected ", Got: " val)}
  )
}

(check_value 42 42)         // ✓ Value matches expected: 42
(check_value "hello" "hi")  // ✗ Value mismatch! Expected: hi, Got: hello
```

### Pattern 2: Search Function

```franz
find = {target list ->
  first = (get list 0)
  <- (if (is first target)
    {<- 1}
    {<- 0}
  )
}

numbers = (list 10 20 30)
found = (find 10 numbers)
(println (if (is found 1) {<- "Found!"} {<- "Not found"}))
```

### Pattern 3: Assertion Pattern

```franz
assert_equals = {actual expected message ->
  (if (is actual expected)
    {(println "  ✓ PASS: " message)}
    {(println "  ✗ FAIL: " message " (expected " expected ", got " actual ")")}
  )
}

(assert_equals (add 2 2) 4 "Addition test")
(assert_equals (multiply 3 4) 12 "Multiplication test")
```

### Pattern 4: Type-Safe Comparison

```franz
safe_compare = {a b ->
  type_a = (type a)
  type_b = (type b)
  <- (if (is type_a type_b)
    {<- (is a b)}
    {
      (println "  Warning: Comparing different types (" type_a " vs " type_b ")")
      <- 0
    }
  )
}

(safe_compare 5 5)      // → 1
(safe_compare 5 "5")    // Warning: different types → 0
```

### Pattern 5: Switch-Like Pattern

```franz
handle_command = {cmd ->
  (if (is cmd "start")
    {(println "  Starting application...")}
    {(if (is cmd "stop")
      {(println "  Stopping application...")}
      {(println "  Unknown command: " cmd)}
    )}
  )
}

(handle_command "start")   // Starting application...
(handle_command "status")  // Unknown command: status
```

### Pattern 6: Equality Chains

```franz
all_equal = {a b c ->
  <- (and (is a b) (is b c))
}

(all_equal 5 5 5)   // → 1 (all equal)
(all_equal 5 5 10)  // → 0 (not all equal)
```

---

## Edge Cases

### Large Numbers

```franz
(is 999999 999999)  // → 1 (works with large integers)
```

### Float Precision

```franz
(is 0.1 0.1)        // → 1 (exact float comparison)
(is 0.0 0)          // → 1 (zero float vs int)
(is -0 0)           // → 1 (negative zero equals zero)
```

### String Edge Cases

```franz
(is "" "")                    // → 1 (empty strings)
(is "hello\nworld" "hello\nworld")  // → 1 (escape sequences)
(is "Hello" "hello")          // → 0 (case sensitive)
```

### List Edge Cases

```franz
(is (list 1 2) (list 1 2 3))  // → 0 (different lengths)
(is (list 1 "hi") (list 1 "hi"))  // → 1 (mixed type lists)
(is (list void 1) (list void 1))  // → 1 (lists with void)
```

### Self-Comparison

```franz
x = 42
(is x x)  // → 1 (always equal to itself)
```

---

## Examples

### Basic Examples

See: [examples/equality/working/basic-equality.franz](../../examples/equality/working/basic-equality.franz)

- Integer comparison
- String comparison
- Numeric type coercion (5 == 5.0)
- List comparison
- Void comparison
- Type checking pattern

**Run:**
```bash
./franz examples/equality/working/basic-equality.franz
```

### Advanced Examples

See: [examples/equality/working/advanced-equality.franz](../../examples/equality/working/advanced-equality.franz)

- List search function
- Assertion pattern
- Type-safe comparison
- Deduplication check
- Switch-like pattern
- Result validation
- Conditional processing
- Equality chains

**Run:**
```bash
./franz examples/equality/working/advanced-equality.franz
```

---

## Testing

### Test Coverage: 40/40 Tests Passing (100%)

**Comprehensive Test Suite (20 tests):**
[test/equality/equality-comprehensive.franz](../../test/equality/equality-comprehensive.franz)

```bash
./franz test/equality/equality-comprehensive.franz
```

**Test Coverage:**
- Integer equality/inequality (same, different, zero, negative)
- Float equality/inequality (same, different)
- Mixed int/float (numeric coercion: 1.0 == 1, 5.0 == 5)
- String equality/inequality (same, different, empty, with spaces)
- List equality/inequality (same elements, different elements, empty, nested)
- Void equality (all voids equal)
- Different type comparison (int vs string, int vs void)
- Variable equality

**Edge Case Test Suite (20 tests):**
[test/equality/equality-edge-cases.franz](../../test/equality/equality-edge-cases.franz)

```bash
./franz test/equality/equality-edge-cases.franz
```

**Edge Cases Tested:**
- Large numbers (999999)
- Negative zero (-0 == 0)
- Float precision (0.1 == 0.1)
- Mixed negative numbers (-1.0 == -1)
- String case sensitivity ("Hello" != "hello")
- String with escape sequences
- List with mixed types
- List length differences
- List order sensitivity
- Lists with void elements
- Self-comparison (x == x)
- Function reference equality
- Different function instances
- Zero float vs int (0.0 == 0)
- String number vs actual number ("42" != 42)
- Empty vs non-empty strings
- Deeply nested lists
- Lists with strings
- Chain comparisons

---

## Performance Analysis

### Time Complexity

| Type | Comparison | Complexity |
|------|------------|------------|
| Integer | Numeric equality | O(1) |
| Float | Numeric equality | O(1) |
| String | strcmp | O(min(len1, len2)) |
| List | Recursive element comparison | O(n) |
| Dict | Key-value comparison | O(n) |
| Function/Closure | Pointer comparison | O(1) |
| Void | Always true | O(1) |

### Memory Usage

- **Zero allocation** for primitive types (int, float, void, function)
- **No temporary allocation** for string comparison (uses strcmp)
- **No temporary allocation** for list comparison (recursive traversal)
- **Result allocation:** Single `int` allocated for return value (4 bytes)

---

## Language Comparison

| Language | Equality Operator | Numeric Coercion | Deep List Compare |
|----------|-------------------|------------------|-------------------|
| **Franz** | `(is a b)` | ✅ Yes (1 == 1.0) | ✅ Yes (recursive) |
| Python | `a == b` | ✅ Yes | ✅ Yes |
| JavaScript | `a === b` (strict) | ❌ No | ❌ No (reference only) |
| JavaScript | `a == b` (loose) | ✅ Yes | ❌ No (reference only) |
| Scheme/Lisp | `(equal? a b)` | ❌ No (exact numbers) | ✅ Yes |
| OCaml | `a = b` | ❌ No (strict typing) | ✅ Yes |
| Rust | `a == b` | ❌ No (strict typing) | ✅ Yes (via PartialEq) |

**Franz's Approach:**
- **Pragmatic numeric coercion** like Python/JavaScript
- **Deep structural equality** like Scheme/OCaml
- **Simple syntax** via S-expression function call



---

## Best Practices

1. **Use `is` for value equality** - Deep comparison for all types
2. **Numeric coercion is automatic** - No need to convert int/float
3. **String comparison is case-sensitive** - Normalize if needed
4. **Function equality is by reference** - Store function in variable if reuse needed
5. **List comparison is deep** - Recursively compares all elements
6. **Type-check before comparing** - Use `(type x)` to validate types if needed

---

## Related Functions

- `(type value)` - Get type name as string
- `(less_than a b)` - Numeric less-than comparison
- `(greater_than a b)` - Numeric greater-than comparison
- `(not value)` - Logical negation (for inequality: `(not (is a b))`)
- `(and a b)` - Logical AND (for equality chains)

