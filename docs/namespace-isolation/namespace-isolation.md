# Namespace Isolation in Franz


## Overview

Namespace isolation prevents name collisions when using multiple modules. Without namespaces, if two libraries both define a `map` function, the second one overwrites the first—creating silent bugs.

Franz's namespace system provides:
- ✅ **Isolated scopes** - Each module's symbols stay separate
- ✅ **Explicit access** - Use dot notation: `math.square`
- ✅ **No pollution** - Namespaces don't leak into global scope
- ✅ **Zero overhead** - Namespace lookup is a simple scope access

## Problem Solved



### With Namespaces :
```franz
lodash = (use_as "lodash.franz")
underscore = (use_as "underscore.franz")

(lodash.map data fn1)      // ✅ lodash's map
(underscore.map data fn2)  // ✅ underscore's map
```

## Syntax

### Creating a Namespace
```franz
namespace_var = (use_as "path/to/module.franz")
```

**Arguments:**
- `path` (string) - Path to the Franz module file

**Returns:**
- Namespace object containing all module symbols

### Accessing Namespace Members
```franz
namespace_var.member_name
```

**Syntax:**
- Namespace variable name
- Dot (`.`)
- Member identifier

**Result:** The value of `member_name` from the namespace

## Examples

### Example 1: Basic Namespace Usage
```franz
// math.franz
square = {x -> <- (multiply x x)}
cube = {x -> <- (multiply x (multiply x x))}

// main.franz
math = (use_as "math.franz")

(println (math.square 5))  // → 25
(println (math.cube 3))    // → 27
```

### Example 2: Multiple Namespaces Without Collision
```franz
// math.franz
double = {x -> <- (multiply x 2)}

// string.franz
double = {str -> <- (join str str)}

// main.franz
math = (use_as "math.franz")
str = (use_as "string.franz")

(println (math.double 5))        // → 10 (number)
(println (str.double "Hello"))   // → "HelloHello" (string)
// ✅ No collision! Both 'double' functions coexist
```

### Example 3: Chaining Namespace Calls
```franz
math = (use_as "math.franz")

result = (math.double (math.square 5))
(println result)  // → 50  (square(5)=25, double(25)=50)
```

### Example 4: Namespaces in Higher-Order Functions
```franz
math = (use_as "math.franz")
numbers = (list 1 2 3 4 5)

// Use namespace function in map
squares = (map numbers {item index -> <- (math.square item)})
(println squares)  // → [1, 4, 9, 16, 25]
```

### Example 5: Nested Operations
```franz
math = (use_as "math.franz")

// Combine namespace calls with stdlib
result = (add (math.square 3) (math.cube 2))
(println result)  // → 17  (9 + 8)
```

### Example 6: Multiple Uses of Same Module
```franz
// Each use_as creates a separate namespace instance
math1 = (use_as "math.franz")
math2 = (use_as "math.franz")

(math1.square 4)  // → 16
(math2.square 4)  // → 16
// Both work independently
```

## API Reference

### `use_as`
**Function**: `(use_as path) → namespace`

**Description**: Loads a Franz module and returns an isolated namespace containing all the module's symbols.

**Parameters:**
- `path` (string) - File path to the Franz module

**Returns:**
- `namespace` object - Isolated scope containing module exports

**Performance:**
- Uses module cache (118k+ cached loads/second)
- First load: parses and caches AST
- Subsequent loads: reuses cached AST

**Example:**
```franz
math = (use_as "lib/math.franz")
(math.square 7)  // → 49
```

**Error Handling:**
```franz
// File not found
math = (use_as "nonexistent.franz")
// Runtime Error @ Line N: Cannot read file "nonexistent.franz".

// Invalid type
(use_as 123)
// Runtime Error @ Line N: use_as function requires string type...
```

### Dot Notation (`.`)

**Syntax**: `namespace.member`

**Description**: Accesses a member from a namespace. The namespace must be of type `namespace`.

**Example:**
```franz
math = (use_as "math.franz")
square_fn = math.square  // Get the function
result = (math.square 5)  // Call the function
```

**Error Handling:**
```franz
// Not a namespace
x = 5
(x.something)
// Runtime Error @ Line N: "x" is not a namespace, it is a integer.

// Member doesn't exist
math = (use_as "math.franz")
(math.nonexistent)
// Runtime Error @ Line N: Identifier 'nonexistent' not found in scope.
```

## Implementation Details

### Architecture

**Namespace Structure:**
```
TYPE_NAMESPACE Generic
  ↓
  p_val → Scope*
    ↓
    Isolated scope containing:
      - All module symbols
      - Parent: global scope (for stdlib access)
      - NOT parent: current scope (isolation!)
```

**Dot Notation Parsing:**
```
Source:   math.square
Tokens:   [IDENTIFIER:math] [DOT] [IDENTIFIER:square]
AST:      OP_QUALIFIED("math.square")
Eval:     1. Get 'math' from scope → namespace object
          2. Extract scope from namespace
          3. Get 'square' from namespace scope
          4. Return the value
```

### Performance

| Operation | Performance |
|-----------|-------------|
| First `use_as` (cache miss) | Parse + cache (~1ms) |
| Cached `use_as` | 118k+ loads/second |
| Dot notation access | O(1) scope lookup |
| Namespace creation | O(module size) |

### Memory Management

**Namespace Lifecycle:**
1. `use_as` creates `Scope*` with module symbols
2. Wraps `Scope*` in `TYPE_NAMESPACE` Generic
3. Returns Generic to user
4. When Generic is freed, `Scope_free()` is called automatically

**Reference Counting:**
```franz
math = (use_as "math.franz")  // refCount = 0 (assigned to var)
fn = math.square               // Scope lookup, no ref change
// When 'math' goes out of scope, namespace is freed
```

## Comparison with Traditional `use`

### Old Way: `use` (Still Supported)
```franz
(use "math.franz" {
  // math.franz symbols merged into this scope
  result = (square 5)
  <- result
})
// square is NOT available here (scoped to callback)
```

**Use cases:**
- Quick scripts
- Single module imports
- Temporary scope needed

### New Way: `use_as` (Namespace Isolation)
```franz
math = (use_as "math.franz")
// math.franz symbols isolated in 'math' namespace

result = (math.square 5)
// 'math' persists in current scope
```

**Use cases:**
- Multiple libraries
- Name collision prevention
- Long-lived module references
- Professional codebases

**Both work! Use whichever fits your needs.**

## Best Practices

### ✅ Do

**1. Use namespaces for multiple libraries:**
```franz
json = (use_as "lib/json.franz")
http = (use_as "lib/http.franz")
db = (use_as "lib/database.franz")
```

**2. Use short, clear namespace names:**
```franz
math = (use_as "libraries/mathematics.franz")  // ✅ Good
m = (use_as "libraries/mathematics.franz")     // ❌ Too short
mathematics = (use_as "libraries/mathematics.franz")  // ❌ Too long
```

**3. Document namespace usage:**
```franz
// HTTP client library for API calls
http = (use_as "lib/http.franz")

// Math utilities for calculations
math = (use_as "lib/math.franz")
```

### ❌ Don't

**1. Don't use overly generic namespace names:**
```franz
utils = (use_as "lib/utils.franz")  // ❌ What kind of utils?
helpers = (use_as "lib/helpers.franz")  // ❌ Too vague
```

**2. Don't mix `use` and `use_as` unnecessarily:**
```franz
// ❌ Confusing - mixing styles
(use "math.franz" {
  math2 = (use_as "string.franz")
  // Now have 'square' (from math) and 'math2.greet' (from string)
})

// ✅ Consistent - all namespaced
math = (use_as "math.franz")
str = (use_as "string.franz")
```

**3. Don't reassign namespace variables:**
```franz
math = (use_as "math.franz")
math = 5  // ❌ Now 'math' is a number, namespace is lost
```

## Comparison with Other Languages

### JavaScript (ES6 Modules)
```javascript
import * as math from './math.js';
math.square(5);  // → 25
```

### Python
```python
import math
math.sqrt(16)  # → 4.0
```

### OCaml
```ocaml
module Math = struct
  let square x = x * x
end

Math.square 5  (* → 25 *)
```

### Franz
```franz
math = (use_as "math.franz")
(math.square 5)  // → 25
```

**Franz's approach:**
- ✅ Simple syntax (dot notation)
- ✅ No `import` keyword needed
- ✅ Dynamic loading (runtime)
- ✅ First-class namespace objects
