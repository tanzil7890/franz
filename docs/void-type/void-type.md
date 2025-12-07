# Void Type



## Overview

Franz provides a `void` type to represent the absence of a value. Void is used for functions that perform side effects without returning meaningful values, and as a sentinel/placeholder value.

### Key Characteristics

- **Type Name:** `"void"`
- **Literal:** `void` keyword
- **Purpose:** Represent absence of value, side-effect-only operations
- **Comparison:** All void values are equal (`(is void void)` → 1)
- **Memory:** Null pointer (no allocated value)

---

## API Reference

### Void Literal

**Syntax:** `void`

**Description:** A built-in identifier that evaluates to a void value

**Example:**
```franz
x = void
(println (type x))  // → "void"
```

---

### Functions Returning Void

Functions that don't have an explicit return statement automatically return void.

**Pattern 1: No Return Statement**
```franz
log = {message -> (println "LOG: " message)}
result = (log "Hello")
(type result)  // → "void"
```

**Pattern 2: Side Effects Only**
```franz
increment = {->
  (println "Counter incremented")
}
status = (increment)
(type status)  // → "void"
```

**Pattern 3: Explicit Void Return**
```franz
do_nothing = {-> <- void}
result = (do_nothing)
(type result)  // → "void"
```

---

## Built-in Functions Returning Void

Many Franz built-in functions return void:

| Function | Returns Void | Example |
|----------|--------------|---------|
| `println` | ✅ Yes | `(println "text")` → void |
| `print` | ✅ Yes | `(print "text")` → void |
| `write_file` | ✅ Yes | `(write_file "file.txt" "content")` → void |

**Example:**
```franz
result = (println "Hello, World!")
(type result)  // → "void"
```

---

## Usage Patterns

### Pattern 1: Logging Functions

```franz
log = {level msg ->
  (println "[" level "] " msg)
}

(log "INFO" "App started")
(log "ERROR" "Failed to connect")
```

**Use Case:** Functions that output information without returning data

---

### Pattern 2: Initialization/Setup

```franz
config = void
(println "Config status: " (type config))  // → "void"

// Later: load actual config
config = "loaded"
(println "Config: " config)  // → "loaded"
```

**Use Case:** Placeholder before actual value is available

---

### Pattern 3: Error Handling

```franz
safe_divide = {a b ->
  (if (is b 0)
    {
      (println "Error: Division by zero")
      <- void
    }
    {<- (divide a b)}
  )
}

result = (safe_divide 10 0)
(type result)  // → "void" (error case)
```

**Use Case:** Return void to signal error conditions

---

### Pattern 4: Sentinel Values

```franz
find = {value list ->
  <- (if (greater_than (length list) 0)
    {<- (get list 0)}
    {<- void}  // Not found
  )
}

result = (find 5 (list))
(is (type result) "void")  // → 1 (not found)
```

**Use Case:** Void indicates "nothing found" or "no result"

---

### Pattern 5: Type Guards

```franz
is_void = {value ->
  <- (is (type value) "void")
}

(is_void void)     // → 1
(is_void 42)       // → 0
(is_void "hello")  // → 0
```

**Use Case:** Check if a value is void

---

### Pattern 6: Optional Values

```franz
get_or_default = {list index default ->
  <- (if (less_than index (length list))
    {<- (get list index)}
    {<- default}
  )
}

data = (list 10 20 30)
val = (get_or_default data 10 void)
(type val)  // → "void" (out of bounds)
```

**Use Case:** Return void as default for missing values

---

## Type Checking

### Checking for Void

```franz
value = void
type_str = (type value)
is_void = (is type_str "void")  // → 1
```

### Void Comparison

```franz
v1 = void
v2 = void
(is v1 v2)  // → 1 (all voids are equal)
```

### Void vs Other Types

```franz
(is void 0)       // → 0 (void ≠ 0)
(is void "")      // → 0 (void ≠ empty string)
(is void (list))  // → 0 (void ≠ empty list)
```

---

## Void in Data Structures

### Void in Lists

```franz
mixed = (list 1 "hello" void 3.14)
third = (get mixed 2)
(type third)  // → "void"
```

### Lists of Void

```franz
empties = (list void void void)
first = (get empties 0)
(type first)  // → "void"
```

---


## Performance

**Memory Overhead:** Zero (NULL pointer, no allocated value)
**Comparison:** O(1) (pointer comparison)
**Type Check:** O(1) (enum comparison)

---

## Comparison with Other Languages

| Language | Void Representation | Usage |
|----------|---------------------|-------|
| Franz | `void` literal | Side effects, placeholders |
| JavaScript | `undefined`, `null` | Uninitialized, absent values |
| Python | `None` | Absent values, no return |
| OCaml | `unit` type `()` | Side effects, no meaningful return |
| Haskell | `()` (unit type) | IO actions, void-like |
| C | `void` type | Function return type |
| Rust | `()` (unit type) | Functions with no return |

Franz's `void` is most similar to OCaml's `unit` and Rust's `()` - a distinct type representing "no value".

---

## Troubleshooting

### Issue: Void Causes "Non-Function" Error

**Symptom:**
```franz
result = (if cond {<- void} {<- void})
(println result)  // Error if trying to call void
```

**Explanation:**
Void is not callable. Don't try to use void as a function.

**Solution:**
Check type before calling:
```franz
(if (is (type result) "void")
  {(println "Result is void")}
  {(result)}  // Safe to call
)
```

---

### Issue: Void vs Empty String

**Symptom:**
```franz
(is void "")  // → 0 (not equal!)
```

**Explanation:**
Void and empty string are different types.

**Solution:**
Use explicit type checking:
```franz
is_empty = {value ->
  <- (or (is (type value) "void") (is value ""))
}
```

---

### Issue: Void in Arithmetic

**Symptom:**
```franz
(add 1 void)  // Type error
```

**Explanation:**
Void is not a number and can't be used in arithmetic.

**Solution:**
Provide default values:
```franz
safe_add = {a b ->
  <- (if (is (type b) "void")
    {<- a}
    {<- (add a b)}
  )
}
```

---

## Related Functions

- `type(value)` - Returns `"void"` for void values
- `is(a, b)` - Compare two void values (always equal)

---
