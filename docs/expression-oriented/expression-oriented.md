# Expression-Oriented Programming in Franz

## Overview

Franz is a fully expression-oriented language where **everything is an expression** that evaluates to a value. There are no statements - only expressions that produce results.

## Core Concept

In expression-oriented programming:
- Every piece of code evaluates to a value
- Control flow (if, loop) are expressions that return values
- Function definitions are expressions
- Even assignments evaluate to a value (void)

## Basic Expressions

### Literals

All literals are expressions:

```franz
42          // Evaluates to: 42 (integer)
3.14        // Evaluates to: 3.14 (float)
"hello"     // Evaluates to: "hello" (string)
```

### Arithmetic

Arithmetic operations are expressions:

```franz
(add 1 2)              // Evaluates to: 3
(multiply 5 6)         // Evaluates to: 30
(divide 10 2)          // Evaluates to: 5
```

### Function Calls

Function applications are expressions:

```franz
(print "hello")        // Evaluates to: void (after side effect)
(length (list 1 2 3))  // Evaluates to: 3
```

## Control Flow as Expressions

### `if` Returns a Value

Unlike many languages where `if` is a statement, Franz's `if` is an expression:

```franz
// if evaluates to the chosen branch
result = (if (greater_than x 10) {
  <- "big"
} {
  <- "small"
})

(print result "\n")

// Can be used directly in expressions
(print (if (is x 0) {<- "zero"} {<- "nonzero"}) "\n")
```

### Multi-way Conditionals

```franz
category = (if
  (less_than age 13) {<- "child"}
  (less_than age 20) {<- "teenager"}
  (less_than age 65) {<- "adult"}
  {<- "senior"}
)
```

### `loop` Returns the Last Value

```franz
// loop evaluates to the last iteration's value
result = (loop 5 {i ->
  <- (multiply i 2)
})
// result = 8 (last iteration: 4 * 2)
```

### `until` Returns Final State

```franz
// until evaluates to the final state
final = (until 10 {state n ->
  <- (add state 1)
})
// final = 10
```

## Functions as Expressions

### Function Definitions

Function definitions are expressions that evaluate to function values:

```franz
// The function definition evaluates to a function value
add_one = {x -> <- (add x 1)}

// Can be used inline
result = ({x -> <- (multiply x 2)} 5)  // result = 10
```

### Anonymous Functions

```franz
// Anonymous function as an expression
(map (list 1 2 3) {x i -> <- (multiply x 2)})
// Evaluates to: (list 2 4 6)
```

## Assignment is an Expression

Assignments evaluate to void but can be used in sequences:

```franz
// Multiple assignments in sequence
x = 5
y = 10
z = (add x y)

// Inside function bodies
calculate = {a b ->
  sum = (add a b)
  product = (multiply a b)
  <- (divide product sum)
}
```

## Expression Sequences

Use functions to create expression sequences:

```franz
process = {->
  step1 = (print "Processing...\n")
  step2 = (print "Done!\n")
  <- "result"
}

result = (process)  // result = "result"
```

## Practical Examples

### Example 1: Factorial

```franz
factorial = {n ->
  <- (if (is n 0) {<- 1} {
    <- (multiply n (factorial (subtract n 1)))
  })
}

(print (factorial 5) "\n")  // The whole thing is an expression tree
```

### Example 2: FizzBuzz

```franz
(loop 100 {i ->
  i = (add i 1)

  // if is an expression, result is used by print
  (print (if
    (is (remainder i 15) 0) {<- "fizzbuzz\n"}
    (is (remainder i 3) 0) {<- "fizz\n"}
    (is (remainder i 5) 0) {<- "buzz\n"}
    {<- (join (string i) "\n")}
  ))
})
```

### Example 3: Nested Expressions

```franz
// Everything composes as expressions
result = (multiply
  (add 1 2)
  (subtract 10 5)
)
// result = 3 * 5 = 15

// More complex nesting
output = (join
  "Result: "
  (string (add
    (multiply 2 3)
    (divide 10 2)
  ))
)
// output = "Result: 11"
```

### Example 4: Map as Expression

```franz
// Transform and use result immediately
(print
  (reduce
    (map (list 1 2 3 4 5) {x i -> <- (multiply x x)})
    {acc x i -> <- (add acc x)}
    0
  )
  "\n"
)
// Prints: 55 (sum of squares)
```

## Return Values

### Explicit Returns

Use `<-` to return values from functions:

```franz
max = {a b ->
  <- (if (greater_than a b) {<- a} {<- b})
}
```

### Implicit Returns

The last expression in a function body is not automatically returned - use `<-`:

```franz
// WRONG - no return
wrong = {x ->
  (add x 1)  // Result discarded
}

// CORRECT - explicit return
correct = {x ->
  <- (add x 1)
}
```

## Benefits

### Composability

Expressions compose naturally:

```franz
// Everything can be nested
result = (add
  (multiply 2 3)
  (if (is x 0) {<- 10} {<- 20})
)
```

### No Special Cases

No distinction between statements and expressions:

```franz
// Everything follows the same rules
x = 5                    // Expression (evaluates to void)
(add 1 2)               // Expression (evaluates to 3)
{x -> <- (add x 1)}     // Expression (evaluates to function)
(if true {<- 1} {<- 2}) // Expression (evaluates to 1 or 2)
```

### Functional Style

Natural functional programming:

```franz
// Chain transformations
result = (reduce
  (filter
    (map (range 100) {x i -> <- (multiply x 2)})
    {x i -> <- (is (remainder x 3) 0)}
  )
  {acc x i -> <- (add acc x)}
  0
)
```

## Common Patterns

### Pattern 1: Guard Expressions

```franz
validate = {x ->
  <- (if
    (less_than x 0) {<- void}
    (greater_than x 100) {<- void}
    {<- x}
  )
}
```

### Pattern 2: Expression Builder

```franz
build_message = {name age ->
  greeting = (join "Hello, " name "!")
  info = (join "You are " (string age) " years old.")
  <- (join greeting " " info)
}
```

### Pattern 3: Computation Pipeline

```franz
process_numbers = {numbers ->
  <- (reduce
    (map numbers {x i -> <- (multiply x x)})
    {acc x i -> <- (add acc x)}
    0
  )
}
```


The evaluator always returns a value for every expression type.

## Comparison with Other Languages

| Language | Style | Example |
|----------|-------|---------|
| Franz | Expression | `x = (if cond {<- 1} {<- 2})` |
| OCaml | Expression | `let x = if cond then 1 else 2` |
| Rust | Expression | `let x = if cond { 1 } else { 2 };` |
| Python | Statement | `if cond: x = 1\nelse: x = 2` |
| JavaScript | Mixed | `let x = cond ? 1 : 2` |

## Related Documentation

- [Type System Guide](../type-system/type-system.md)
- [First-Class Functions Guide](../first-class-functions/first-class-functions.md)
- [Scoping Guide](../scoping/scoping.md)
