# Scoping in Franz

## Overview

Franz uses **lexical scoping** (also called static scoping) where variable bindings are determined by the structure of the code. Functions create new scopes that can access variables from their parent scopes.

## Global Scope

Variables defined at the top level belong to the global scope:

```franz
// Global variables
name = "Franz"
version = 0.0.4
pi = 3.14159

// Global functions
greet = {-> (print "Hello from Franz!\n") <- void}

// Accessible anywhere
(print name "\n")
(greet)
```

### Pre-defined Global Variables

Franz provides built-in globals:

```franz
// Command-line arguments (list)
(print arguments "\n")

// Void value
result = void
```

**Implementation:** `src/stdlib.c:1595` (arguments), `src/stdlib.c:1604` (void)

## Local Scope

Functions create new local scopes:

```franz
// Function parameters are local
add_and_print = {a b ->
  // These bindings are local to the function
  sum = (add a b)
  message = (join "Sum: " (string sum))

  (print message "\n")
  <- sum
}

// a, b, sum, message are not accessible here
(add_and_print 5 10)
```

### Nested Scopes

Scopes can be nested:

```franz
outer = {x ->
  y = (multiply x 2)

  inner = {z ->
    // Can access x, y from outer scope
    // z is local to inner
    <- (add x (add y z))
  }

  <- (inner 5)
}

(print (outer 10) "\n")  // Prints: 35 (10 + 20 + 5)
```

## Variable Shadowing

Inner scopes can shadow (hide) outer variables:

```franz
x = 10  // Global x

test = {->
  x = 20  // Local x shadows global
  (print x "\n")  // Prints: 20

  inner = {->
    x = 30  // Inner x shadows outer x
    (print x "\n")  // Prints: 30
    <- void
  }

  (inner)
  (print x "\n")  // Prints: 20 (local x unchanged)
  <- void
}

(test)
(print x "\n")  // Prints: 10 (global x unchanged)
```

## Function Parameters

Function parameters create local bindings:

```franz
process = {x y z ->
  // x, y, z are local parameters
  result = (add (multiply x y) z)
  <- result
}

// x, y, z don't exist outside function
(print (process 2 3 4) "\n")  // Prints: 10
```

## Closures

Functions capture variables from their surrounding scope:

```franz
// Closure example
make_counter = {->
  count = 0

  // This function captures 'count'
  increment = {->
    count = (add count 1)
    <- count
  }

  <- increment
}

counter1 = (make_counter)
(print (counter1) "\n")  // Prints: 1
(print (counter1) "\n")  // Prints: 2
(print (counter1) "\n")  // Prints: 3

counter2 = (make_counter)
(print (counter2) "\n")  // Prints: 1 (separate closure)
```

### Closure Captures

Closures capture variables, not values:

```franz
make_adder = {n ->
  // Returns function that captures 'n'
  <- {x -> <- (add x n)}
}

add_five = (make_adder 5)
add_ten = (make_adder 10)

(print (add_five 3) "\n")   // Prints: 8
(print (add_ten 3) "\n")    // Prints: 13
```

## Scope Chain Lookup

Variables are resolved by walking up the scope chain:

```franz
a = 1     // Global

outer = {->
  b = 2   // Outer local

  middle = {->
    c = 3  // Middle local

    inner = {->
      d = 4  // Inner local

      // Can access all: d, c, b, a
      (print a " " b " " c " " d "\n")
      <- void
    }

    <- (inner)
  }

  <- (middle)
}

(outer)  // Prints: 1 2 3 4
```

**Implementation:** `src/scope.c:74`

```c
// Walks up parent chain until variable found
Generic *Scope_get(Scope *p_target, char *key, int lineNumber) {
  // Search current scope
  ScopeItem *p_curr = p_target->p_head;
  while (p_curr != NULL && strcmp(p_curr->key, key) != 0)
    p_curr = p_curr->p_next;

  if (p_curr == NULL) {
    // Not found, search parent scope
    if (p_target->p_parent == NULL) {
      printf("Runtime Error: %s is not defined.\n", key);
      exit(0);
    } else {
      return Scope_get(p_target->p_parent, key, lineNumber);
    }
  }
  return p_curr->p_val;
}
```

## Variable Assignment

### Creating vs Updating

Assignments always create new bindings in the current scope:

```franz
x = 10  // Global

test = {->
  x = 20  // Creates NEW local x (doesn't modify global)
  <- void
}

(test)
(print x "\n")  // Prints: 10 (global unchanged)
```

Franz does not distinguish between declaration and assignment - all assignments create bindings in the current scope.

### Mutation Pattern

To update outer scope variables, return and reassign:

```franz
count = 0

increment = {c ->
  <- (add c 1)
}

// Update by reassignment
count = (increment count)
count = (increment count)
(print count "\n")  // Prints: 2
```

## The `use` Function and Scope

The `use` function creates an isolated scope for loading files:

```franz
// lib.franz contains:
// helper = {x -> <- (multiply x 2)}

// Load and use
(use "lib.franz" {->
  // helper is accessible here
  result = (helper 5)
  (print result "\n")
  <- void
})

// helper is NOT accessible here
```

**Implementation:** `src/stdlib.c:350`

Files are evaluated in a new scope, and the callback runs in that scope.

## Practical Examples

### Example 1: Configuration Pattern

```franz
// Global config
config = (list
  (list "host" "localhost")
  (list "port" 8080)
)

get_config = {key ->
  <- (find config {item i -> <- (is (get item 0) key)})
}

host = (get (get_config "host") 1)
(print "Host: " host "\n")
```

### Example 2: Private Helper Functions

```franz
// Public API function
calculate = {x ->
  // Private helper (local to calculate)
  helper = {n -> <- (multiply n n)}

  result = (add (helper x) 10)
  <- result
}

// helper is not accessible outside
(print (calculate 5) "\n")  // Prints: 35
```

### Example 3: Factory with State

```franz
// Create object with private state
make_person = {name age ->
  // Private state
  internal_age = age

  // Public methods
  get_name = {-> <- name}
  get_age = {-> <- internal_age}
  birthday = {->
    internal_age = (add internal_age 1)
    <- void
  }

  // Return "object" (list of functions)
  <- (list get_name get_age birthday)
}

person = (make_person "Alice" 25)

get_name = (get person 0)
get_age = (get person 1)
birthday = (get person 2)

(print (get_name) " is " (string (get_age)) " years old\n")
(birthday)
(print "After birthday: " (string (get_age)) "\n")
```

### Example 4: Currying with Closures

```franz
// Curry a two-argument function
curry = {fn ->
  <- {a ->
    <- {b ->
      <- (fn a b)
    }
  }
}

add_nums = {a b -> <- (add a b)}
curried_add = (curry add_nums)

// Partial application
add_five = (curried_add 5)

(print (add_five 10) "\n")  // Prints: 15
(print (add_five 20) "\n")  // Prints: 25
```

### Example 5: Module Pattern

```franz
// Create module with public/private members
math_module = {->
  // Private constants
  pi = 3.14159

  // Private helper
  square = {x -> <- (multiply x x)}

  // Public functions
  circle_area = {r -> <- (multiply pi (square r))}
  circle_circumference = {r -> <- (multiply 2 (multiply pi r))}

  // Export public API
  <- (list circle_area circle_circumference)
}

math = (math_module)
area_fn = (get math 0)
circ_fn = (get math 1)

(print "Area: " (string (area_fn 5)) "\n")
(print "Circumference: " (string (circ_fn 5)) "\n")
```

## Scope Implementation

### Scope Structure

**File:** `src/scope.h:15`

```c
typedef struct Scope {
  struct Scope *p_parent;  // Parent scope (lexical chaining)
  ScopeItem *p_head;       // Linked list of bindings
} Scope;
```

### Scope Operations

| Operation | Function | Description |
|-----------|----------|-------------|
| Create | `Scope_new(parent)` | New scope with parent |
| Set | `Scope_set(scope, key, val)` | Add/update binding |
| Get | `Scope_get(scope, key)` | Lookup variable |
| Free | `Scope_free(scope)` | Destroy scope |

**File:** `src/scope.c:1-120`

## Common Patterns

### Pattern 1: Let-style Bindings

```franz
// Simulate let...in using functions
compute = {->
  x = 10
  y = 20
  <- (add x y)
}

result = (compute)
// x and y not accessible here
```

### Pattern 2: Namespace Pattern

```franz
// Group related functions
string_utils = {->
  uppercase = {s -> <- s}  // Simplified
  lowercase = {s -> <- s}  // Simplified

  <- (list uppercase lowercase)
}

utils = (string_utils)
// Access through list
```

### Pattern 3: Recursive Closures

```franz
make_factorial = {->
  fact = {n ->
    <- (if (is n 0) {<- 1} {
      <- (multiply n (fact (subtract n 1)))
    })
  }
  <- fact
}

factorial = (make_factorial)
(print (factorial 5) "\n")  // Prints: 120
```

## Scope vs Dynamic Scoping

Franz uses **lexical scoping**, not dynamic scoping:

```franz
x = 10

get_x = {-> <- x}  // Captures lexical x

test = {->
  x = 20  // Different x
  <- (get_x)  // Returns 10, not 20
}

(print (test) "\n")  // Prints: 10
```

In dynamic scoping, `get_x` would return 20 (the most recent binding). Franz's lexical scoping means variables are resolved based on where the function was defined, not where it's called.

## Best Practices

### 1. Minimize Global Variables

```franz
// Avoid too many globals
// Good - encapsulate in function
main = {->
  config = load_config
  data = load_data
  <- (process config data)
}

(main)
```

### 2. Use Closures for Encapsulation

```franz
// Good - private state via closure
make_counter = {->
  count = 0
  <- {-> count = (add count 1) <- count}
}
```

### 3. Clear Function Signatures

```franz
// Good - clear parameters
calculate = {width height depth ->
  <- (multiply width (multiply height depth))
}

// Avoid - unclear what x, y, z mean
calc = {x y z -> <- (multiply x (multiply y z))}
```
