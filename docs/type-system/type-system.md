# Franz Type System Guide

## Overview

Franz features a **gradual type system** that combines dynamic typing with optional static type annotations. This allows you to write code quickly without types, and add type safety when needed.

## Core Type System

### Runtime Types

Franz has 7 built-in runtime types:

1. `integer` - Whole numbers
2. `float` - Decimal numbers
3. `string` - Text data
4. `list` - Collections
5. `function` - User-defined functions
6. `native_function` - Built-in functions
7. `void` - Empty/null value

### Dynamic Typing (Default)

By default, Franz is dynamically typed. Types are checked at runtime:

```franz
// No type annotations needed
x = 5
y = "hello"
z = (list 1 2 3)

// Type errors caught at runtime
(add "hello" 5)  // Runtime Error: add requires integer or float
```

## Type Checking Functions

### `type` - Get Runtime Type

Returns the type name of a value:

```franz
(print (type 42) "\n")        // "integer"
(print (type 3.14) "\n")      // "float"
(print (type "hello") "\n")   // "string"
(print (type (list 1 2)) "\n") // "list"
```

### Type Conversions

Convert between types explicitly:

```franz
// String to number
x = (integer "42")      // 42
y = (float "3.14")      // 3.14

// Number to string
s = (string 42)         // "42"
t = (string 3.14)       // "3.14"

// Number conversions
f = (float 5)           // 5.0
i = (integer 3.9)       // 3 (truncates)
```

## Gradual Typing (Optional Static Types) - 


**✅ What Works:**
- Type inference engine (Hindley-Milner)
- `franz-check` tool (permissive by default; `--strict` exits non‑zero on errors)
- Type checking without annotations (inference)
- `sig` syntax parsing; signatures are parsed and ignored at runtime
- Function types with arrows and type variables

### Type Signatures with `sig`

`sig` is parsed by the interpreter, ignored at runtime, and consumed by the type checker.

```franz
// ✅ THIS NOW WORKS! sig syntax fully implemented
(sig factorial (integer -> integer))
factorial = {n ->
  <- (if (is n 0) {<- 1} {
    <- (multiply n (factorial (subtract n 1)))
  })
}

// ✅ Multiple parameter functions
(sig add_numbers (integer integer -> integer))
add_numbers = {a b -> <- (add a b)}

// ✅ Higher-order functions with type variables
(sig apply_twice ((a -> a) a -> a))
apply_twice = {f x -> <- (f (f x))}

// ✅ Run normally
(print (factorial 5) "\n")  // Prints: 120

// ✅ Type check (permissive)
// $ franz-check examples/type-system/working/typed_factorial.franz
// ✓ Type check passed (permissive)

// ✅ Type check (strict)
// $ franz-check --strict examples/type-system/working/typed_factorial.franz
// ✓ Type check passed
```

### Type Syntax - 

**Basic Types:**
- `integer` - Integer type
- `float` - Float type
- `string` - String type
- `list` - List type
- `void` - Void type

**Function Types:**
- `(type1 -> type2)` - Single argument function
- `(type1 type2 -> type3)` - Multiple argument function
- `(type1 type2 type3 -> type4)` - Three arguments

**Type Variables (Polymorphism):**
- `a`, `b`, `c` - Type variables for generic functions
- `(a -> a)` - Identity function type
- `(a -> b)` - Transformation function type

## Type Checking Tool

### `franz-check` - 

A separate static type checker that infers types automatically:

```bash
# Check types without running (permissive)
./franz-check path/to/program.franz

# Check types and run if valid (strict)
./franz-check --strict path/to/program.franz && ./franz path/to/program.franz

# Or do both in one step (subject-reduction guard)
./franz --assert-types path/to/program.franz

# Show inferred types
./franz-check --show-types path/to/program.franz
```

**Example:**
```bash
$ ./franz-check --strict examples/type-system/working/typed_factorial.franz
✓ Type check passed
```

### Zero Runtime Overhead

Type checking has no runtime cost:
- Types checked separately by franz-check
- No performance penalty during execution
- Same runtime behavior whether checked or not

## Type Inference - 

The type checker uses Hindley-Milner type inference:

```franz
// Type automatically inferred
double = {x -> <- (multiply x 2)}
// Inferred: (or integer float) -> (or integer float)

// Works with both
(print (double 5) "\n")      // 10
(print (double 2.5) "\n")    // 5.0
```

## Subject Reduction (What, Why, How)

Subject reduction is a property of typed languages stating that if a program is well‑typed, each step of evaluation preserves its type. Informally: “once typed, always typed throughout execution.”

How it applies to Franz
- Franz is dynamically typed at runtime; the optional checker (`franz-check`) performs static validation ahead of time.
- The interpreter offers a guard to enforce this contract before running code:
  - `./franz --assert-types path/to/program.franz`
  - If the program fails the checker, Franz aborts before execution.

Why to use it
- Prevents executing ill‑typed programs (safety in local runs and CI).
- Guards against regressions when refactoring or upgrading stdlib.
- Works well with strict checker mode in pipelines.

Recommended flows
- Permissive check: `./franz-check program.franz`
- Strict check: `./franz-check --strict program.franz`
- Single‑step guard (subject‑reduction style): `./franz --assert-types program.franz`

Notes
- Dynamic features are modeled conservatively; examples adhere to real semantics (e.g., `map` callback arity `(a integer -> b)`).
- The guard checks prior to execution (it does not re‑type each runtime reduction), but ensures only well‑typed programs are executed.

## Examples

### Example 1: Fibonacci with Types

```franz
(sig fibonacci (integer -> integer))
fibonacci = {n ->
  <- (if
    (is n 0) {<- 0}
    (is n 1) {<- 1}
    {<- (add (fibonacci (subtract n 1)) (fibonacci (subtract n 2)))}
  )
}

(print (fibonacci 10) "\n")
```

### Example 2: Generic Map (using built-in map)

```franz
// map in Franz passes (item, index) to callbacks
// Signature: ((list a) (a integer -> b) -> (list b))

double = {x _ -> <- (multiply x 2)}
nums = (list 1 2 3 4 5)
result = (map nums double)
(print result "\n")
```

### Example 3: Mixed Numeric Types

```franz
// Accept int or float
(sig calculate ((or integer float) -> float))
calculate = {x ->
  result = (multiply x 2.5)
  <- (add result 10.0)
}

(print (calculate 5) "\n")      // 22.5
(print (calculate 3.5) "\n")    // 18.75
```

## Best Practices

### When to Add Types

✅ **Add types for:**
- Public API functions
- Complex algorithms
- Library code
- Long-term maintenance

❌ **Skip types for:**
- Quick scripts
- Prototypes
- Internal helpers
- One-off calculations

### Type-Driven Development

1. Write function signature first
2. Implement function body
3. Let type checker validate

```franz
// Step 1: Signature (S-expr style)
(sig process_data ((list string) -> (list integer)))

// Step 2: Implementation
process_data = {items ->
  <- (map items {item i -> <- (integer item)})
}

// Step 3: Check
// franz-check program.franz
```

