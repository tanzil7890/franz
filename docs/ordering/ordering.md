# Ordering Comparison Functions (`less_than`, `greater_than`)

## Overview

Franz provides two ordering comparison functions for numeric values: `less_than` and `greater_than`. These functions support both integers and floats with automatic numeric coercion, enabling flexible numeric comparisons.


---

## API Reference

### `less_than`

**Syntax:**
```franz
(less_than a b)
```

**Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | integer or float | First value to compare |
| `b` | integer or float | Second value to compare |

**Returns:**
- **Type:** `integer`
- **Value:** `1` if `a < b`, `0` otherwise

---

### `greater_than`

**Syntax:**
```franz
(greater_than a b)
```

**Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `a` | integer or float | First value to compare |
| `b` | integer or float | Second value to compare |

**Returns:**
- **Type:** `integer`
- **Value:** `1` if `a > b`, `0` otherwise

---

## Comparison Rules

### 1. Integer Comparison

**Standard numeric ordering:**

```franz
(less_than 3 5)     // → 1 (3 < 5)
(less_than 5 3)     // → 0 (5 is not < 3)
(less_than 5 5)     // → 0 (5 is not < 5, equal values)

(greater_than 10 3) // → 1 (10 > 3)
(greater_than 3 10) // → 0 (3 is not > 10)
(greater_than 5 5)  // → 0 (5 is not > 5, equal values)
```

### 2. Float Comparison

**Exact bit-level float comparison:**

```franz
(less_than 3.5 5.2)     // → 1 (3.5 < 5.2)
(less_than 5.2 3.5)     // → 0 (5.2 is not < 3.5)

(greater_than 10.5 3.2) // → 1 (10.5 > 3.2)
(greater_than 3.2 10.5) // → 0 (3.2 is not > 10.5)
```

### 3. Mixed Integer/Float Comparison (Numeric Coercion)

**Automatic type coercion** - integers and floats compared numerically:

```franz
(less_than 3 5.5)       // → 1 (3 < 5.5, int promoted to float)
(less_than 2.5 10)      // → 1 (2.5 < 10, int promoted to float)

(greater_than 10 3.5)   // → 1 (10 > 3.5, numeric coercion)
(greater_than 10.5 3)   // → 1 (10.5 > 3, numeric coercion)

(less_than 5 5.0)       // → 0 (5 == 5.0 via coercion, not less than)
(greater_than 1 1.0)    // → 0 (1 == 1.0 via coercion, not greater than)
```

**Implementation:** (src/stdlib.c:744-747)
```c
*p_res = (
  (a->type == TYPE_FLOAT ? *((double *) a->p_val) : *((int *) a->p_val))
  < (b->type == TYPE_FLOAT ? *((double *) b->p_val) : *((int *) b->p_val))
);
```

### 4. Negative Numbers

**Standard mathematical ordering:**

```franz
(less_than -10 -5)      // → 1 (-10 < -5)
(greater_than -5 -10)   // → 1 (-5 > -10)
(less_than -5 5)        // → 1 (-5 < 5, negative < positive)
(greater_than 5 -5)     // → 1 (5 > -5, positive > negative)
```

### 5. Zero Comparisons

**Zero equals negative zero:**

```franz
(less_than 0 5)         // → 1 (0 < 5)
(greater_than 5 0)      // → 1 (5 > 0)
(less_than 0.0 -0.0)    // → 0 (0.0 == -0.0, not less than)
(greater_than 0.0 -0.0) // → 0 (0.0 == -0.0, not greater than)
```

### 6. Self-Comparison

**Always false for ordering:**

```franz
x = 42
(less_than x x)     // → 0 (x is not < x)
(greater_than x x)  // → 0 (x is not > x)
```

---

## Usage Patterns

### Pattern 1: Find Maximum of Two Numbers

```franz
find_max = {x y ->
  <- (if (greater_than x y) {<- x} {<- y})
}

max_val = (find_max 42 27)  // → 42
(println "Maximum: " max_val)
```

### Pattern 2: Find Minimum of Two Numbers

```franz
find_min = {x y ->
  <- (if (less_than x y) {<- x} {<- y})
}

min_val = (find_min 15 8)  // → 8
(println "Minimum: " min_val)
```

### Pattern 3: Range Check

```franz
in_range = {val min_r max_r ->
  <- (and (greater_than val min_r) (less_than val max_r))
}

result = (in_range 50 0 100)  // → 1 (true, 50 is in [0, 100])
(println "Is 50 in range? " (if result {<- "Yes"} {<- "No"}))
```

### Pattern 4: Clamp Value to Range

```franz
clamp = {val min_v max_v ->
  <- (if (less_than val min_v)
    {<- min_v}
    {(if (greater_than val max_v)
      {<- max_v}
      {<- val}
    )}
  )
}

clamped = (clamp 150 0 100)  // → 100 (limited to max)
(println "Clamped value: " clamped)
```

### Pattern 5: Multi-Way Comparison (Grade Classification)

```franz
classify_grade = {score ->
  <- (if (greater_than score 90)
    {<- "A"}
    {(if (greater_than score 80)
      {<- "B"}
      {(if (greater_than score 70)
        {<- "C"}
        {(if (greater_than score 60)
          {<- "D"}
          {<- "F"}
        )}
      )}
    )}
  )
}

grade = (classify_grade 85)  // → "B"
(println "Grade: " grade)
```

### Pattern 6: Comparison Chaining (Check if Sorted)

```franz
is_sorted_three = {a b c ->
  <- (and (less_than a b) (less_than b c))
}

sorted = (is_sorted_three 1 5 10)      // → 1 (sorted ascending)
not_sorted = (is_sorted_three 10 5 1)  // → 0 (not sorted)

(println "Is [1,5,10] sorted? " (if sorted {<- "Yes"} {<- "No"}))
```

### Pattern 7: Sorting Two Values

```franz
sort_two = {x y ->
  <- (if (less_than x y)
    {<- (list x y)}
    {<- (list y x)}
  )
}

sorted = (sort_two 30 10)  // → [List: 10, 30]
(println "Sorted: " sorted)
```

### Pattern 8: Binary Search Direction

```franz
binary_search_direction = {target mid ->
  <- (if (less_than target mid)
    {<- "left"}
    {(if (greater_than target mid)
      {<- "right"}
      {<- "found"}
    )}
  )
}

direction = (binary_search_direction 42 50)  // → "left"
(println "Search direction: " direction)
```

### Pattern 9: Min/Max of Three Values

```franz
min_of_three = {a b c ->
  temp = (if (less_than a b) {<- a} {<- b})
  <- (if (less_than temp c) {<- temp} {<- c})
}

max_of_three = {a b c ->
  temp = (if (greater_than a b) {<- a} {<- b})
  <- (if (greater_than temp c) {<- temp} {<- c})
}

min3 = (min_of_three 15 8 23)  // → 8
max3 = (max_of_three 15 8 23)  // → 23
```

### Pattern 10: Age Validation with Range

```franz
validate_age = {age ->
  too_young = (less_than age 18)
  too_old = (greater_than age 65)
  <- (not (or too_young too_old))
}

valid = (validate_age 25)   // → 1 (valid, between 18-65)
invalid = (validate_age 70) // → 0 (invalid, > 65)
```

---

## Edge Cases

### Large Numbers

```franz
(less_than 999999 1000000)      // → 1 (works with large integers)
(less_than -1000000 -999999)    // → 1 (works with large negatives)
```

### Float Precision

```franz
(less_than 0.0001 0.001)             // → 1 (small floats)
(greater_than -0.0001 -0.001)        // → 1 (small negative floats)
(less_than 1.0000001 1.0000002)      // → 1 (high precision)
```

### Zero and Negative Zero

```franz
(less_than 0.0 -0.0)    // → 0 (0.0 == -0.0, equal)
(greater_than 0.0 -0.0) // → 0 (0.0 == -0.0, equal)
(less_than 0 0)         // → 0 (0 == 0, equal)
```

### Numeric Coercion Edge Cases

```franz
(less_than 1 1.0)       // → 0 (1 == 1.0, equal via coercion)
(greater_than 1 1.0)    // → 0 (1 == 1.0, equal via coercion)
(less_than 0 0.0)       // → 0 (0 == 0.0, equal)
(less_than 4.999 5)     // → 1 (4.999 < 5)
(less_than -5 -4.5)     // → 1 (-5 < -4.5)
```

### Transitivity

**Mathematical property holds:**

```franz
// If a < b and b < c, then a < c
a = 3
b = 7
c = 15

ab = (less_than a b)  // → 1
bc = (less_than b c)  // → 1
ac = (less_than a c)  // → 1 (transitivity holds)
```

---

## Examples

### Basic Examples

See: [examples/ordering/working/basic-ordering.franz](../../examples/ordering/working/basic-ordering.franz)

- Simple integer comparison
- Finding maximum of two numbers
- Finding minimum of two numbers
- Range check
- Sorting two values
- Comparison with floats
- Mixed int/float comparison

**Run:**
```bash
./franz examples/ordering/working/basic-ordering.franz
```

### Advanced Examples

See: [examples/ordering/working/advanced-ordering.franz](../../examples/ordering/working/advanced-ordering.franz)

- Bubble sort comparison function
- Three-way comparison (min/max of three)
- Grade classification
- Clamp function
- Binary search direction
- Price comparison with tolerance
- Sorting three numbers
- Validation chain
- Comparison chaining (sorted check)

**Run:**
```bash
./franz examples/ordering/working/advanced-ordering.franz
```

---

## Performance Analysis

### Time Complexity

| Operation | Complexity | Notes |
|-----------|------------|-------|
| `less_than` | O(1) | Direct numeric comparison |
| `greater_than` | O(1) | Direct numeric comparison |
| Type coercion | O(1) | Ternary operator, no conversion overhead |

### Memory Usage

- **Zero allocation** for comparison logic
- **Result allocation:** Single `int` (4 bytes) for return value
- **No temporary variables** or intermediate allocations

### Comparison Cost

| Type Combination | Cost |
|------------------|------|
| int vs int | 1 integer comparison |
| float vs float | 1 double comparison |
| int vs float | 1 int→float cast + 1 double comparison |
| float vs int | 1 int→float cast + 1 double comparison |

---

## Language Comparison

| Language | Less Than | Greater Than | Numeric Coercion | Strict Typing |
|----------|-----------|--------------|------------------|---------------|
| **Franz** | `(less_than a b)` | `(greater_than a b)` | ✅ Yes (int/float) | ❌ No (dynamic) |
| Python | `a < b` | `a > b` | ✅ Yes | ❌ No (dynamic) |
| JavaScript | `a < b` | `a > b` | ✅ Yes | ❌ No (dynamic) |
| Scheme/Lisp | `(< a b)` | `(> a b)` | ✅ Yes | ❌ No (dynamic) |
| OCaml | `a < b` | `a > b` | ❌ No (strict types) | ✅ Yes (static) |
| Rust | `a < b` | `a > b` | ❌ No (strict types) | ✅ Yes (static) |
| C | `a < b` | `a > b` | ✅ Yes (implicit) | ⚠️ Partial (weak) |

**Franz's Approach:**
- **Pragmatic numeric coercion** like Python/JavaScript
- **S-expression syntax** like Scheme/Lisp
- **Simple comparison operators** with automatic type handling

---

## Troubleshooting

### Issue: "Function expects 2 arguments but got 1"

**Cause:** Missing second argument

**Fix:**
```franz
// ❌ Wrong: Only one argument
(less_than x)

// ✅ Correct: Two arguments
(less_than x y)
```

### Issue: "Type error: Expected int or float, got string"

**Cause:** Trying to compare non-numeric values

**Expected behavior:** Only integers and floats can be compared

```franz
// ❌ Wrong: Comparing strings
(less_than "5" "10")  // Type error!

// ✅ Correct: Compare numbers
(less_than 5 10)      // → 1
```

### Issue: Float precision causing unexpected results

**Diagnosis:** Exact bit-level float comparison used

**Workaround:** Add tolerance-based comparison:
```franz
close_enough_less = {a b tolerance ->
  diff = (subtract b a)
  <- (greater_than diff tolerance)
}

(close_enough_less 1.0 1.0000001 0.001)  // → 0 (too close)
(close_enough_less 1.0 1.1 0.001)        // → 1 (far enough)
```

### Issue: Confusion between `less_than` and `is`

**Clarification:**
- `less_than` / `greater_than` → **Ordering** (numeric only)
- `is` → **Equality** (all types, with numeric coercion)

```franz
(less_than 5 10)   // → 1 (5 < 10)
(greater_than 5 10) // → 0 (5 is not > 10)
(is 5 10)          // → 0 (5 != 10)

(less_than 5 5)    // → 0 (5 is not < 5)
(greater_than 5 5) // → 0 (5 is not > 5)
(is 5 5)           // → 1 (5 == 5)
```

---

## Best Practices

1. **Use for numeric comparisons only** - Strings/lists not supported
2. **Numeric coercion is automatic** - No need to convert int/float manually
3. **Equal values return 0** - Neither less than nor greater than
4. **Chain comparisons with `and`** - `(and (less_than a b) (less_than b c))` for sorting checks
5. **Use with `if` for conditional logic** - Return value is 1 (true) or 0 (false)
6. **Combine with `min`/`max` stdlib** - For finding extremes in lists
7. **Transitivity holds** - Standard mathematical properties apply

---

## Related Functions

- `(is a b)` - Equality comparison (all types, numeric coercion)
- `(not value)` - Logical negation
- `(and a b ...)` - Logical AND (for chaining comparisons)
- `(or a b ...)` - Logical OR
- `(min a b ...)` - Minimum value (variadic)
- `(max a b ...)` - Maximum value (variadic)

---

