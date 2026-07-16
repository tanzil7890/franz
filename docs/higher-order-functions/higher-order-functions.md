# Higher-Order Functions in Franz

## Overview

**Higher-order functions** are functions that either:
1. Take one or more functions as arguments, OR
2. Return a function as their result

Franz has excellent support for higher-order functions with built-in functions like `map`, `reduce`, and `filter`, plus the ability to create custom higher-order functions.

## Built-in Higher-Order Functions

### `map` - Transform Elements

Transform each element in a list:

**Syntax:** `(map list function)`

```franz
// Double each number
numbers = (list 1 2 3 4 5)
doubled = (map numbers {x i -> <- (multiply x 2)})
// doubled = (list 2 4 6 8 10)

// Square each number
squared = (map numbers {x i -> <- (multiply x x)})
// squared = (list 1 4 9 16 25)

// Convert to strings
strings = (map numbers {x i -> <- (string x)})
// strings = (list "1" "2" "3" "4" "5")
```

The callback receives two arguments:
- `x` - current element
- `i` - current index

**Implementation:** `src/stdlib.c:1302`

### `reduce` - Aggregate Values

Reduce a list to a single value:

**Syntax:** `(reduce list function initial_value)`

```franz
// Sum all numbers
numbers = (list 1 2 3 4 5)
sum = (reduce numbers {acc x i -> <- (add acc x)} 0)
// sum = 15

// Product of numbers
product = (reduce numbers {acc x i -> <- (multiply acc x)} 1)
// product = 120

// Find maximum
max_val = (reduce numbers {acc x i ->
  <- (if (greater_than x acc) {<- x} {<- acc})
} 0)
// max_val = 5

// Build string
text = (reduce numbers {acc x i ->
  <- (join acc (string x) ",")
} "")
// text = "1,2,3,4,5,"
```

The callback receives three arguments:
- `acc` - accumulator (current aggregate value)
- `x` - current element
- `i` - current index

**Implementation:** `src/stdlib.c:1371`

### `filter` - Select Elements

Keep only elements that match a condition:

**Syntax:** `(filter list predicate)`

```franz
// Filter even numbers
numbers = (list 1 2 3 4 5 6 7 8 9 10)
evens = (filter numbers {x i -> <- (is (remainder x 2) 0)})
// evens = (list 2 4 6 8 10)

// Filter positive numbers
mixed = (list -3 -1 0 2 4 -5 7)
positive = (filter mixed {x i -> <- (greater_than x 0)})
// positive = (list 2 4 7)

// Filter by length
words = (list "a" "hi" "cat" "dog" "hello")
long_words = (filter words {w i -> <- (greater_than (length w) 2)})
// long_words = (list "cat" "dog" "hello")
```

The predicate receives two arguments:
- `x` - current element
- `i` - current index

Returns true (1) to keep element, false (0) to discard.

**Implementation:** `src/stdlib.c:1437`

### `loop` - Iterate N Times

Execute a function N times:

**Syntax:** `(loop count function)`

```franz
// Print numbers 0-9
(loop 10 {i ->
  (print i "\n")
  <- void
})

// Build list of squares
squares = (list)
(loop 5 {i ->
  squares = (insert squares i (multiply i i))
  <- void
})
// squares = (list 0 1 4 9 16)

// Countdown
(loop 5 {i ->
  (print (subtract 5 i) "...\n")
  <- void
})
```

The callback receives:
- `i` - current iteration (0-indexed)

**Implementation:** `src/stdlib.c:848`

### `until` - Loop Until Condition

Loop until a stop condition is met:

**Syntax:** `(until stop_value function)`

```franz
// Count up to 10
(until 10 {state n ->
  (print "Count: " (string state) "\n")
  <- (add state 1)
})

// Interactive input loop
(until "quit" {state n ->
  user_input = (input)
  (print "You entered: " user_input "\n")
  <- user_input
})

// Build Fibonacci sequence
(until 100 {state n ->
  (print state "\n")
  <- (add state n)
})
```

The callback receives:
- `state` - current state value
- `n` - previous state value

**Implementation:** `src/stdlib.c:883`

## Chaining Higher-Order Functions

Combine multiple operations:

```franz
// Filter, map, reduce chain
numbers = (list 1 2 3 4 5 6 7 8 9 10)

result = (reduce
  (map
    (filter numbers {x i -> <- (is (remainder x 2) 0)})
    {x i -> <- (multiply x x)}
  )
  {acc x i -> <- (add acc x)}
  0
)
// Filter evens -> square -> sum
// result = 220 (2² + 4² + 6² + 8² + 10²)
```

## Creating Custom Higher-Order Functions

### Example 1: Apply Twice

```franz
// Apply a function twice
apply_twice = {fn x ->
  <- (fn (fn x))
}

double = {x -> <- (multiply x 2)}
(print (apply_twice double 3) "\n")  // Prints: 12
```

### Example 2: Apply N Times

```franz
// Apply a function N times
apply_n = {fn x n ->
  result = x
  (loop n {i ->
    result = (fn result)
    <- void
  })
  <- result
}

increment = {x -> <- (add x 1)}
(print (apply_n increment 5 3) "\n")  // Prints: 8 (5+1+1+1)
```

### Example 3: Function Composition

```franz
// Compose two functions: f(g(x))
compose = {f g ->
  <- {x -> <- (f (g x))}
}

add_one = {x -> <- (add x 1)}
square = {x -> <- (multiply x x)}

// Compose: square then add one
square_then_add = (compose add_one square)
(print (square_then_add 5) "\n")  // Prints: 26 (5² + 1)
```

### Example 4: Partial Application

```franz
// Bind first argument
partial = {fn arg1 ->
  <- {arg2 -> <- (fn arg1 arg2)}
}

multiply_nums = {a b -> <- (multiply a b)}
double = (partial multiply_nums 2)
triple = (partial multiply_nums 3)

(print (double 7) "\n")  // Prints: 14
(print (triple 7) "\n")  // Prints: 21
```

### Example 5: Conditional Execution

```franz
// Run function only if condition is true
when = {condition fn ->
  <- (if condition {<- (fn)} {<- void})
}

is_positive = {x -> <- (greater_than x 0)}
print_value = {-> (print "Value is positive!\n") <- void}

x = 5
(when (is_positive x) print_value)
```

### Example 6: Memoization

```franz
// Simple memoization (with manual cache)
memoize = {fn ->
  cache = (list)
  <- {x ->
    cached = (find cache {item i -> <- (is (get item 0) x)})
    <- (if (is cached void) {
      result = (fn x)
      cache = (insert cache (length cache) (list x result))
      <- result
    } {
      <- (get cached 1)
    })
  }
}
```

## Practical Examples

### Example 1: Data Processing Pipeline

```franz
// Process list of numbers
numbers = (list 1 2 3 4 5 6 7 8 9 10)

process = {data ->
  // Step 1: Filter odds
  step1 = (filter data {x i -> <- (is (remainder x 2) 1)})

  // Step 2: Square each
  step2 = (map step1 {x i -> <- (multiply x x)})

  // Step 3: Sum all
  step3 = (reduce step2 {acc x i -> <- (add acc x)} 0)

  <- step3
}

(print (process numbers) "\n")  // Prints: 165
```

### Example 2: String Manipulation

```franz
// Process list of strings
words = (list "hello" "world" "franz" "language")

// Uppercase first letter (simplified)
capitalize = {str ->
  <- (join (string (get str 0)) (get str 1 (length str)))
}

capitalized = (map words {w i -> <- (capitalize w)})
// capitalized = ...
```

### Example 3: Nested Mapping

```franz
// Map over 2D list
matrix = (list
  (list 1 2 3)
  (list 4 5 6)
  (list 7 8 9)
)

// Double all values
doubled_matrix = (map matrix {row y ->
  <- (map row {val x -> <- (multiply val 2)})
})
```

### Example 4: Custom Filter with Context

```franz
// Filter with additional context
filter_with_context = {data predicate context ->
  <- (filter data {x i -> <- (predicate x i context)})
}

numbers = (list 1 2 3 4 5 6 7 8 9 10)
threshold = 5

greater_than_threshold = {x i ctx ->
  <- (greater_than x ctx)
}

result = (filter_with_context numbers greater_than_threshold threshold)
// result = (list 6 7 8 9 10)
```

### Example 5: Reducing to Dictionary (List of Pairs)

```franz
// Group by even/odd
numbers = (list 1 2 3 4 5 6)

grouped = (reduce numbers {acc x i ->
  <- (if (is (remainder x 2) 0) {
    // Even - add to first group
    even_group = (get acc 0)
    even_group = (insert even_group (length even_group) x)
    <- (set acc 0 even_group)
  } {
    // Odd - add to second group
    odd_group = (get acc 1)
    odd_group = (insert odd_group (length odd_group) x)
    <- (set acc 1 odd_group)
  })
} (list (list) (list)))

// grouped = (list (list 2 4 6) (list 1 3 5))
```

## Advanced Patterns

### Currying (Manual)

```franz
// Convert multi-argument function to curried form
curry = {fn ->
  <- {a -> <- {b -> <- (fn a b)}}
}

add_nums = {a b -> <- (add a b)}
curried_add = (curry add_nums)

add_five = (curried_add 5)
(print (add_five 10) "\n")  // Prints: 15
```

### Function Pipeline

```franz
// Create pipeline of functions
pipeline = {fns initial ->
  <- (reduce fns {acc fn i -> <- (fn acc)} initial)
}

// Pipeline: add 1 -> multiply by 2 -> subtract 3
result = (pipeline (list
  {x -> <- (add x 1)}
  {x -> <- (multiply x 2)}
  {x -> <- (subtract x 3)}
) 5)
// result = 9 ((5 + 1) * 2 - 3)
```

## Built-in HOFs Summary

| Function | Purpose | Arguments | Returns | Location |
|----------|---------|-----------|---------|----------|
| `map` | Transform elements | list, fn | list | `stdlib.c:1302` |
| `reduce` | Aggregate to single value | list, fn, init | any | `stdlib.c:1371` |
| `filter` | Select elements | list, predicate | list | `stdlib.c:1437` |
| `loop` | Fixed iteration | count, fn | last value | `stdlib.c:848` |
| `until` | Conditional loop | stop, fn | last value | `stdlib.c:883` |
| `if` | Conditional execution | cond, then, else, ... | chosen value | `stdlib.c:922` |
| `use` | Load files with callback | paths..., fn | result | `stdlib.c:350` |

## Comparison with Other Languages

| Language | map | reduce | filter | Custom HOFs |
|----------|-----|--------|--------|-------------|
| Franz | ✅ | ✅ | ✅ | ✅ |
| OCaml | ✅ `List.map` | ✅ `List.fold` | ✅ `List.filter` | ✅ |
| JavaScript | ✅ `.map()` | ✅ `.reduce()` | ✅ `.filter()` | ✅ |
| Python | ✅ `map()` | ✅ `reduce()` | ✅ `filter()` | ✅ |
| Haskell | ✅ `map` | ✅ `fold` | ✅ `filter` | ✅ |

## Best Practices

### 1. Chain Operations

```franz
// Good - clear pipeline
result = (reduce
  (filter
    (map data transform)
    predicate)
  aggregate
  init)

// Avoid - nested one-liners that are hard to read
```

### 2. Extract Callback Functions

```franz
// Good - named functions
is_even = {x i -> <- (is (remainder x 2) 0)}
square = {x i -> <- (multiply x x)}

evens = (filter numbers is_even)
squared = (map evens square)

// Avoid - too many inline lambdas
```

### 3. Use Descriptive Names

```franz
// Good
sum_of_squares = (reduce
  (map numbers {x i -> <- (multiply x x)})
  {acc x i -> <- (add acc x)}
  0)

// Avoid
x = (reduce (map n {a b -> <- (multiply a a)}) {c d e -> <- (add c d)} 0)
```

## Related Documentation

- [First-Class Functions Guide](../first-class-functions/first-class-functions.md)
- [Lists Guide](../lists/lists.md)
- [Expression-Oriented Guide](../expression-oriented/expression-oriented.md)
- [Scoping Guide](../scoping/scoping.md)
