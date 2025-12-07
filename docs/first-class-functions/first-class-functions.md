# First-Class Functions in Franz

## Overview

In Franz, **functions are first-class values**, meaning they can be:
- Stored in variables
- Passed as arguments to other functions
- Returned from functions
- Created anonymously

## Function Definition Syntax

### Basic Function Definition

```franz
// Store function in a variable
add_one = {x -> <- (add x 1)}

// Call the function
(print (add_one 5) "\n")  // Prints: 6
```

### Multi-Parameter Functions

```franz
// Multiple parameters
add_numbers = {a b -> <- (add a b)}

(print (add_numbers 3 7) "\n")  // Prints: 10

// Three parameters
sum_three = {a b c ->
  <- (add (add a b) c)
}

(print (sum_three 1 2 3) "\n")  // Prints: 6
```

### Zero-Parameter Functions

```franz
// No parameters - use empty parameter list
greet = {->
  (print "Hello, World!\n")
  <- void
}

(greet)  // Calls the function
```

## Functions as Values

### Storing Functions

```franz
// Functions are just values
square = {x -> <- (multiply x x)}
cube = {x -> <- (multiply x (multiply x x))}

// Store in a variable
operation = square

(print (operation 5) "\n")  // Prints: 25

// Reassign to different function
operation = cube
(print (operation 5) "\n")  // Prints: 125
```

### Functions in Lists

```franz
// List of functions
operations = (list
  {x -> <- (add x 1)}
  {x -> <- (multiply x 2)}
  {x -> <- (subtract x 1)}
)

// Apply first function
first_op = (get operations 0)
(print (first_op 10) "\n")  // Prints: 11

// Apply all operations
(map operations {fn i ->
  (print "Result: " (string (fn 10)) "\n")
  <- void
})
```

## Passing Functions as Arguments

### Function Parameters

```franz
// Function that takes a function as parameter
apply_twice = {f x ->
  <- (f (f x))
}

double = {x -> <- (multiply x 2)}

(print (apply_twice double 3) "\n")  // Prints: 12 (3 * 2 * 2)
```

### Inline Anonymous Functions

```franz
// Pass function directly without naming it
(apply_twice {x -> <- (add x 5)} 10)  // Returns: 20
```

## Returning Functions

### Function Factories

```franz
// Function that returns a function
make_adder = {n ->
  <- {x -> <- (add x n)}
}

// Create specialized functions
add_five = (make_adder 5)
add_ten = (make_adder 10)

(print (add_five 3) "\n")   // Prints: 8
(print (add_ten 3) "\n")    // Prints: 13
```

### Closures

Functions capture variables from their surrounding scope:

```franz
// Closure captures 'multiplier'
make_multiplier = {multiplier ->
  <- {x -> <- (multiply x multiplier)}
}

double = (make_multiplier 2)
triple = (make_multiplier 3)

(print (double 7) "\n")  // Prints: 14
(print (triple 7) "\n")  // Prints: 21
```

## Anonymous Functions

### Lambda Expressions

```franz
// Anonymous function - no name needed
result = ({x -> <- (multiply x x)} 5)
(print result "\n")  // Prints: 25

// With map
squares = (map (list 1 2 3 4) {x i -> <- (multiply x x)})
// squares = (list 1 4 9 16)
```

### Inline Usage

```franz
// Use anonymous functions directly
(print
  (reduce (list 1 2 3 4 5) {acc x i -> <- (add acc x)} 0)
  "\n"
)
// Prints: 15
```

## Practical Examples

### Example 1: Function Composition

```franz
// Compose two functions
compose = {f g ->
  <- {x -> <- (f (g x))}
}

add_one = {x -> <- (add x 1)}
square = {x -> <- (multiply x x)}

// Compose: square after add_one
square_after_add = (compose square add_one)

(print (square_after_add 3) "\n")  // Prints: 16 ((3 + 1)²)
```

### Example 2: Conditional Function Selection

```franz
// Choose function based on condition
choose_operation = {op_name ->
  <- (if
    (is op_name "add") {<- {a b -> <- (add a b)}}
    (is op_name "multiply") {<- {a b -> <- (multiply a b)}}
    {<- {a b -> <- (subtract a b)}}
  )
}

operation = (choose_operation "multiply")
(print (operation 6 7) "\n")  // Prints: 42
```

### Example 3: Callback Pattern

```franz
// Process data with callback
process_list = {data callback ->
  <- (map data callback)
}

numbers = (list 1 2 3 4 5)

// Pass different callbacks
doubled = (process_list numbers {x i -> <- (multiply x 2)})
squared = (process_list numbers {x i -> <- (multiply x x)})

(print doubled "\n")  // Prints: (list 2 4 6 8 10)
(print squared "\n")  // Prints: (list 1 4 9 16 25)
```

### Example 4: Partial Application (Manual)

```franz
// Simulate partial application
partial = {fn arg1 ->
  <- {arg2 -> <- (fn arg1 arg2)}
}

add_nums = {a b -> <- (add a b)}

// Partially apply first argument
add_five = (partial add_nums 5)

(print (add_five 10) "\n")  // Prints: 15
(print (add_five 20) "\n")  // Prints: 25
```

### Example 5: Strategy Pattern

```franz
// Different strategies as functions
aggressive = {x -> <- (multiply x 3)}
moderate = {x -> <- (multiply x 2)}
conservative = {x -> <- (add x 1)}

apply_strategy = {strategy value ->
  <- (strategy value)
}

(print (apply_strategy aggressive 10) "\n")     // Prints: 30
(print (apply_strategy moderate 10) "\n")       // Prints: 20
(print (apply_strategy conservative 10) "\n")   // Prints: 11
```

## Recursive Functions

Functions can call themselves:

```franz
// Recursive factorial
factorial = {n ->
  <- (if (is n 0) {<- 1} {
    <- (multiply n (factorial (subtract n 1)))
  })
}

(print (factorial 5) "\n")  // Prints: 120
```

### Mutual Recursion

```franz
// Define functions that call each other
is_even = {n ->
  <- (if (is n 0) {<- 1} {
    <- (is_odd (subtract n 1))
  })
}

is_odd = {n ->
  <- (if (is n 0) {<- 0} {
    <- (is_even (subtract n 1))
  })
}

(print (is_even 4) "\n")  // Prints: 1
(print (is_odd 4) "\n")   // Prints: 0
```

## Function Arity

Franz functions can accept variable numbers of arguments:

```franz
// Functions check argument count at runtime
add_nums = {a b -> <- (add a b)}

(add_nums 1 2)      // Works: 2 arguments
(add_nums 1)        // Runtime Error: missing argument
(add_nums 1 2 3)    // Runtime Error: too many arguments
```

## Built-in vs User Functions

### User-Defined Functions

```franz
// Your functions - TYPE_FUNCTION
my_func = {x -> <- (add x 1)}
```

### Native Functions

```franz
// Built-in functions - TYPE_NATIVEFUNCTION
// Examples: print, add, map, reduce, etc.
(print "hello\n")
(add 1 2)
```

Both can be passed around as first-class values:

```franz
// Store built-in function
my_add = add
(print (my_add 3 7) "\n")  // Prints: 10
```

## Implementation Details

**File:** `src/eval.c:79-82`

Function definitions create `Generic` values with `TYPE_FUNCTION`:

```c
else if (p_head->opcode == OP_FUNCTION) {
  return Generic_new(TYPE_FUNCTION, AstNode_copy(p_head, 0), 0);
}
```



Function application evaluates the function and applies arguments in a new scope.

## Comparison with Other Languages

| Language | First-Class Functions | Syntax |
|----------|----------------------|--------|
| Franz | ✅ Yes | `{x -> <- (add x 1)}` |
| OCaml | ✅ Yes | `fun x -> x + 1` |
| JavaScript | ✅ Yes | `x => x + 1` |
| Python | ✅ Yes | `lambda x: x + 1` |
| C | ❌ No | Function pointers only |

## Best Practices

### 1. Use Descriptive Names

```franz
// Good
calculate_total = {items -> ...}

// Avoid
f = {x -> ...}
```

### 2. Keep Functions Small

```franz
// Good - single responsibility
double = {x -> <- (multiply x 2)}

// Avoid large, complex functions
```

### 3. Compose Functions

```franz
// Build complex behavior from simple functions
process = {data ->
  filtered = (filter data {x i -> <- (greater_than x 0)})
  mapped = (map filtered {x i -> <- (multiply x 2)})
  <- (reduce mapped {acc x i -> <- (add acc x)} 0)
}
```

## Related Documentation

- [Higher-Order Functions Guide](../higher-order-functions/higher-order-functions.md)
- [Expression-Oriented Guide](../expression-oriented/expression-oriented.md)
- [Scoping Guide](../scoping/scoping.md)
- [Lists Guide](../lists/lists.md)
