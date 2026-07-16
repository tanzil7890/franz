# Mutable References in Franz



---

## Overview

Franz now supports **mutable references** - a controlled way to introduce mutation in a functional language. This feature enables practical patterns like counters, accumulators, and stateful computations while maintaining Franz's functional-first philosophy.

**Industry Precedent**: OCaml `ref`, Rust `RefCell`, Scheme `box`

---

## API

### `(ref value)`

Creates a new mutable reference containing `value`.

**Signature**: `(a -> (ref a))`

**Returns**: A mutable reference wrapping the value

**Example**:
```franz
counter = (ref 0)
```

---

### `(deref ref)`

Retrieves the current value from a mutable reference.

**Signature**: `((ref a) -> a)`

**Returns**: A copy of the value (safe from mutation)

**Example**:
```franz
counter = (ref 42)
value = (deref counter)  // value = 42
```

---

### `(set! ref new-value)`

Updates a mutable reference with a new value.

**Signature**: `((ref a) a -> void)`

**Returns**: `void`

**Side Effect**: Modifies the reference in-place

**Example**:
```franz
counter = (ref 0)
(set! counter 10)
(deref counter)  // Returns 10
```

---

## Usage Patterns

### Pattern 1: Counter

```franz
// Create mutable counter
counter = (ref 0)

// Increment function
increment = {->
  current = (deref counter)
  (set! counter (add current 1))
  <- (deref counter)
}

(increment)  // Returns 1
(increment)  // Returns 2
(increment)  // Returns 3
```

### Pattern 2: Accumulator

```franz
// Sum list using accumulator
nums = (list 1 2 3 4 5)
sum = (ref 0)

(map nums {x i ->
  current = (deref sum)
  (set! sum (add current x))
  <- x
})

(println "Sum: " (deref sum))  // Sum: 15
```

### Pattern 3: Test Counter

```franz
// Track test results
passed = (ref 0)
failed = (ref 0)

run_test = {name condition ->
  (if condition
    {->
      p = (deref passed)
      (set! passed (add p 1))
      (println "✓ PASS: " name)
    }
    {->
      f = (deref failed)
      (set! failed (add f 1))
      (println "✗ FAIL: " name)
    }
  )
}

(run_test "addition" (is (add 2 2) 4))
(run_test "subtraction" (is (subtract 5 3) 2))

(println "Passed: " (deref passed))
(println "Failed: " (deref failed))
```

### Pattern 4: Memoization Cache

```franz
// Memoized fibonacci
cache = (ref (dict))

fib = {n ->
  cached = (dict-get (deref cache) (string n))
  (if (is cached void)
    {->
      // Compute fib(n)
      result = (if (less_than n 2)
        {-> <- n}
        {-> <- (add (fib (subtract n 1)) (fib (subtract n 2)))}
      )
      // Store in cache
      current_cache = (deref cache)
      new_cache = (dict-set current_cache (string n) result)
      (set! cache new_cache)
      <- result
    }
    {-> <- cached}
  )
}

(println "fib(10) = " (fib 10))
(println "fib(10) again (cached) = " (fib 10))
```

---

## Type System Integration

### Type Representation

Mutable references have type `(ref T)` where `T` is the type of the contained value.

**Examples**:
```franz
(sig counter (ref int))
counter = (ref 0)

(sig name (ref string))
name = (ref "Franz")

(sig items (ref (list int)))
items = (ref (list 1 2 3))
```

### Type Rules

```
Γ ⊢ e : T
-------------------  (T-Ref)
Γ ⊢ (ref e) : (ref T)


Γ ⊢ r : (ref T)
-------------------  (T-Deref)
Γ ⊢ (deref r) : T


Γ ⊢ r : (ref T)    Γ ⊢ e : T
--------------------------------  (T-Set!)
Γ ⊢ (set! r e) : void
```

---

## Memory Management

### Reference Counting

Mutable references use **reference counting** for garbage collection:

```c
typedef struct Ref {
  Generic *value;      // The mutable value
  int refCount;        // Reference count
} Ref;
```

**Lifecycle**:
1. `(ref value)` → Creates `Ref` with `refCount = 1`
2. Assignment/copy → Increments `refCount`
3. Scope exit → Decrements `refCount`
4. `refCount == 0` → Frees `Ref` and contained value

### Memory Safety

- **No dangling pointers**: Ref-counted cleanup prevents use-after-free
- **No memory leaks**: Refs freed when `refCount` reaches 0
- **Value safety**: `deref` returns a **copy**, not a pointer

**Example**:
```franz
create_ref = {->
  local_ref = (ref 42)
  <- local_ref
}

r = (create_ref)  // Safe! Ref survives scope exit
(println (deref r))  // Works: 42
```

---

## Performance Characteristics

| Operation | Time Complexity | Space Complexity |
|-----------|----------------|------------------|
| `(ref value)` | O(1) | O(1) |
| `(deref ref)` | O(c) where c = cost of copying value | O(c) |
| `(set! ref new)` | O(1) for ref update + O(c) for cleanup | O(1) |

**Notes**:
- `deref` returns a **copy** of the value (safe but potentially expensive for large data)
- `set!` frees old value and stores new value
- Refs add minimal overhead (~16 bytes per ref)

---

## Comparison with Other Languages

### OCaml

| OCaml | Franz | Notes |
|-------|-------|-------|
| `let x = ref 0` | `x = (ref 0)` | Create ref |
| `!x` | `(deref x)` | Dereference |
| `x := 1` | `(set! x 1)` | Update ref |

**Example**:
```ocaml
(* OCaml *)
let counter = ref 0 in
counter := !counter + 1;
!counter  (* Returns 1 *)
```

```franz
; Franz
counter = (ref 0)
(set! counter (add (deref counter) 1))
(deref counter)  // Returns 1
```

### Rust

| Rust | Franz | Notes |
|------|-------|-------|
| `let mut x = 0` | `x = (ref 0)` | Mutable binding |
| `x` | `(deref x)` | Read value |
| `x = 1` | `(set! x 1)` | Update |

### Scheme

| Scheme | Franz | Notes |
|--------|-------|-------|
| `(define x (box 0))` | `x = (ref 0)` | Create box |
| `(unbox x)` | `(deref x)` | Unbox |
| `(set-box! x 1)` | `(set! x 1)` | Update box |

---

## Best Practices

### ✅ DO

1. **Use refs for local state**
   ```franz
   accumulator = (ref 0)
   ```

2. **Use refs for counters**
   ```franz
   test_count = (ref 0)
   ```

3. **Use refs for caches**
   ```franz
   memo_cache = (ref (dict))
   ```

4. **Keep refs lexically scoped**
   ```franz
   process_items = {items ->
     count = (ref 0)
     // Use count locally
     <- (deref count)
   }
   ```

### ❌ DON'T

1. **Don't use refs for everything**
   ```franz
   // BAD: Unnecessary mutation
   x = (ref 10)
   y = (ref 20)
   
   // GOOD: Use immutable values
   x = 10
   y = 20
   ```

2. **Don't return refs from functions** (unless intentional shared state)
   ```franz
   // BAD: Leaks mutable state
   make_counter = {-> <- (ref 0)}
   
   // GOOD: Return values
   make_counter = {start -> <- start}
   ```

3. **Don't use refs when return values work**
   ```franz
   // BAD: Unnecessary ref
   sum_list = {lst ->
     total = (ref 0)
     (map lst {x i -> (set! total (add (deref total) x))})
     <- (deref total)
   }
   
   // GOOD: Use reduce
   sum_list = {lst ->
     <- (reduce lst {acc x i -> <- (add acc x)} 0)
   }
   ```

---

## Guidelines

### When to Use Mutable References

✅ **Use refs when**:
- You need **local stateful computation** (counters, accumulators)
- You're tracking **side effects** (test counts, progress bars)
- You need **performance-critical mutation** (caching, memoization)
- You want **imperative patterns** in a functional context

❌ **Avoid refs when**:
- **Return values** work just as well
- **Higher-order functions** (map/reduce/filter) solve the problem
- You can use **recursion** instead

### Immutable by Default, Mutable by Choice

Franz remains **functional-first**:
- Regular variables are **immutable**
- Refs are **explicit opt-in** to mutation
- Clear distinction: `x` vs `(ref x)`

---
