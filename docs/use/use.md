# Use - File-based Module System

## Overview

The `use` function provides a file-based module system for Franz, allowing code organization across multiple files. It loads Franz source files into a new scope and executes a callback function within that scope, enabling modular programming and code reuse.



## Syntax

```franz
(use path1 path2 ... pathN callback)
```

## Parameters

- `path1, path2, ..., pathN` - **String(s)**: File path(s) to load
- `callback` - **Function**: Function to execute with loaded modules in scope

## Return Value

Returns the value returned by the callback function.

## How It Works

1. **New Scope Creation**: Creates a fresh scope inheriting from parent scope
2. **File Loading**: Loads and executes each file in order within the new scope
3. **Callback Execution**: Executes callback function with access to all loaded symbols
4. **Scope Isolation**: Loaded symbols only available within the callback

## Key Features

### ✅ Multiple File Loading
Load multiple modules in a single `use` call:

```franz
(use "math.franz" "strings.franz" "utils.franz" {
  // All three modules available here
  <- (some_function)
})
```

### ✅ Scoped Imports
Module symbols only available within the callback scope:

```franz
// Module: helpers.franz
helper_fn = {x -> <- (multiply x 2)}

// Main program
(use "helpers.franz" {
  result = (helper_fn 5)  // ✓ Works inside use block
  <- result
})

// helper_fn not available here
```

### ✅ Access to Outer Scope
Modules can access variables from outer scopes:

```franz
base_value = 10

(use "multiplier.franz" {
  result = (multiply_by base_value 5)  // Uses outer scope variable
  <- result
})
```

### ✅ Return Values
Use blocks return the callback's return value:

```franz
result = (use "calculator.franz" {
  <- (calculate 10 20)
})
(println result)  // Prints calculation result
```

## Examples

### Example 1: Math Module

**File: math.franz**
```franz
square = {x -> <- (multiply x x)}
cube = {x -> <- (multiply x (multiply x x))}
```

**Usage:**
```franz
(use "math.franz" {
  (println "5 squared:" (square 5))      // 25
  (println "3 cubed:" (cube 3))          // 27
})
```

### Example 2: Multiple Modules

**File: geometry.franz**
```franz
area_circle = {r ->
  pi = 3.14159
  <- (multiply pi (multiply r r))
}
```

**File: physics.franz**
```franz
kinetic_energy = {mass velocity ->
  <- (multiply 0.5 (multiply mass (multiply velocity velocity)))
}
```

**Usage:**
```franz
(use "geometry.franz" "physics.franz" {
  area = (area_circle 5)
  energy = (kinetic_energy 10 20)
  (println "Circle area:" area)
  (println "Kinetic energy:" energy)
})
```

### Example 3: Higher-Order Functions

```franz
// File: transformers.franz
double = {x -> <- (multiply x 2)}
square = {x -> <- (multiply x x)}

(use "transformers.franz" {
  numbers = (list 1 2 3 4 5)

  // Use module functions with map
  doubled = (map numbers {n _ -> <- (double n)})
  squared = (map numbers {n _ -> <- (square n)})

  (println "Doubled:" doubled)  // [2, 4, 6, 8, 10]
  (println "Squared:" squared)  // [1, 4, 9, 16, 25]
})
```

### Example 4: Localized I18N

**File: francaise.franz**
```franz
afficher = print
répéter = loop
```

**Usage:**
```franz
(use "francaise.franz" {
  (répéter 3 {_ ->
    (afficher "Bonjour monde!\n")
  })
})
```

## Advanced Usage

### Recursive Functions in Modules

```franz
// File: recursion.franz
factorial = {n ->
  <- (if (less_than n 2)
    {<- 1}
    {<- (multiply n (factorial (subtract n 1)))})
}

(use "recursion.franz" {
  (println "5! =" (factorial 5))  // 120
})
```

### Module Composition

```franz
(use "math.franz" {
  // Define new functions using module functions
  area_square = {side -> <- (square side)}
  volume_cube = {side -> <- (cube side)}

  (println "Area of 5x5 square:" (area_square 5))
  (println "Volume of 5x5x5 cube:" (volume_cube 5))
})
```

### Nested Use Calls

```franz
(use "outer.franz" {
  outer_value = 10

  (use "inner.franz" {
    // Both outer.franz and inner.franz symbols available
    combined = (outer_fn (inner_fn outer_value))
    <- combined
  })
})
```

## Implementation Details

### Scope Isolation

- Each `use` creates a new scope with parent scope as fallback
- Loaded files execute in this new scope
- Callback executes in the same scope
- Scope destroyed after callback returns

### File Resolution

- Paths are relative to current working directory
- Files loaded in order specified
- Later files can override earlier definitions
- File must exist or runtime error occurs

### Performance

- Files are read and parsed at runtime
- No caching mechanism (files re-parsed on each `use`)
- Scope creation is O(1)
- File parsing is O(n) where n is file size

## Comparison with OCaml Modules

| Feature | OCaml | Franz `use` | Status |
|---------|-------|-------------|--------|
| File as module | ✅ Implicit | ✅ Explicit via `use` | ✅ |
| Namespace isolation | ✅ Automatic | ✅ Via callback scope | ✅ |
| Multiple files | ✅ `open A; open B` | ✅ `(use "a" "b" {...})` | ✅ |
| Module signatures | ✅ `.mli` files | ❌ No type interfaces | ❌ |
| Nested modules | ✅ `module M = struct...` | ❌ No module keyword | ❌ |
| Module functors | ✅ Parameterized modules | ❌ Not supported | ❌ |

## Error Handling

### File Not Found
```franz
(use "missing.franz" {
  <- "never runs"
})
// Runtime Error @ Line X: Cannot read file "missing.franz".
```

### Invalid Arguments
```franz
// Wrong: callback must be last argument
(use {<- 1} "file.franz")
// Runtime Error: use first argument must be a partial function

// Wrong: paths must be strings
(use 123 {<- 1})
// Runtime Error: use function requires string type for argument #1
```

