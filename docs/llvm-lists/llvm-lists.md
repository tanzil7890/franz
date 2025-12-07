#  + 1.5.1: LLVM List Literals - Implementation


Franz now supports **heterogeneous lists** in LLVM native compilation mode, matching Rust's `Vec<Box<dyn Any>>` approach for maximum flexibility and performance.

**** adds **automatic unboxing** - list operation results can be used directly in arithmetic, comparisons, and printing without manual type extraction!

## Features

### List Creation
```franz
// Empty list
empty = []

// Integer list
nums = [1, 2, 3, 4, 5]

// Mixed types (heterogeneous)
mixed = [42, "hello", 3.14]

// Nested lists
matrix = [[1, 2], [3, 4, 5]]
```

### List Operations

All operations work with **Generic* pointers** (Rust-like trait objects):

| Operation | Syntax | Description | Returns |
|-----------|--------|-------------|---------|
| **is_list** | `(is_list value)` | Check if value is a list | i64 (0 or 1) |
| **empty?** | `(empty? list)` | Check if list is empty | i64 (0 or 1) |
| **length** | `(length list)` | Get list length | i64 |
| **head** | `(head list)` | Get first element | Generic* |
| **tail** | `(tail list)` | Get rest of list | Generic* (list) |
| **cons** | `(cons elem list)` | Prepend element | Generic* (list) |
| **nth** | `(nth list index)` | Get element at index | Generic* |

### Type Checking

```franz
nums = [1, 2, 3]
(is_list nums)          // → 1 (true)
(is_list 42)            // → 0 (false)
```

### Examples

**Basic Operations:**
```franz
numbers = [1, 2, 3, 4, 5]

// Length
len = (length numbers)      // → 5

// Head/Tail
first = (head numbers)      // → Generic*(1)
rest = (tail numbers)       // → Generic*([2,3,4,5])
rest_len = (length rest)    // → 4

// Cons
extended = (cons 0 numbers) // → [0,1,2,3,4,5]
new_len = (length extended) // → 6

// Nth access
third = (nth numbers 2)     // → Generic*(3)

// Empty check
empty = []
(empty? empty)              // → 1
```

**Nested Lists:**
```franz
matrix = [[1, 2], [3, 4, 5]]
first_row = (head matrix)       // → Generic*([1,2])
row_length = (length first_row) // → 2

// Deep nesting
deep = [[[1]]]
outer = (head deep)             // → Generic*([[1]])
inner = (head outer)            // → Generic*([1])
innermost_len = (length inner)  // → 1
```

##  Auto-Unboxing (NEW! ✅)

**Automatic conversion of Generic* → native values** for seamless arithmetic and printing!

### Direct Printing

List values and operation results now print naturally:

```franz
nums = [1, 2, 3]
(println nums)              // → [1, 2, 3]

first = (head nums)
(println first)             // → 1 (auto-unboxed!)

(println (head [10, 20]))   // → 10 (works inline!)
```

### Arithmetic with List Values

Use list operation results directly in calculations:

```franz
nums = [10, 5, 2]
a = (head nums)             // a = Generic*(10)
b = (nth nums 1)            // b = Generic*(5)
c = (nth nums 2)            // c = Generic*(2)

// Auto-unboxing in arithmetic
result1 = (add a b)         // → 15 (auto-unboxed!)
result2 = (subtract a b)    // → 5
result3 = (multiply b c)    // → 10
result4 = (divide a c)      // → 5

// Complex expressions work too!
result = (add (multiply a c) (subtract b c))  // → 23
```

### How Auto-Unboxing Works

1. **Detection**: When a Generic* value is used in arithmetic, the compiler detects it
2. **Unboxing**: Automatically inserts call to `franz_unbox_int()` or `franz_unbox_float()`
3. **Type Conversion**: Supports automatic int↔float conversion
4. **Zero Overhead**: Direct LLVM IR calls, no runtime penalty

### Unboxing Functions

Runtime helpers for manual unboxing if needed:

```c
// Runtime unboxing helpers (in src/stdlib.c)
int64_t franz_unbox_int(Generic *generic);    // Extract int (auto-converts from float)
double franz_unbox_float(Generic *generic);   // Extract float (auto-converts from int)
char *franz_unbox_string(Generic *generic);   // Extract string
```

**Note**: You typically don't need to call these manually - auto-unboxing handles it!


### Boxing Strategy

Native LLVM values are boxed into `Generic*` wrappers:

```c
// Boxing helpers
Generic *franz_box_int(int64_t value);
Generic *franz_box_float(double value);
Generic *franz_box_string(char *value);
Generic *franz_box_list(Generic *list);

// List creation
Generic *franz_list_new(Generic **elements, int length);

// List operations
Generic *franz_list_head(Generic *list);
Generic *franz_list_tail(Generic *list);
Generic *franz_list_cons(Generic *elem, Generic *list);
int64_t franz_list_is_empty(Generic *list);
int64_t franz_list_length(Generic *list);
Generic *franz_list_nth(Generic *list, int64_t index);
int64_t franz_is_list(Generic *value);
void franz_print_generic(Generic *value);
```

## Performance

- **C-level speed**: Direct LLVM IR → machine code
- **Zero runtime overhead**: Compiled, not interpreted
- **Rust-equivalent**: Same performance as Rust's heterogeneous collections


## Limitations

**Current Limitations:**
- **List creation in if-else blocks**: Create lists OUTSIDE if-else blocks, not inside them
  ```franz
  // ❌ WRONG - Creates compiler error
  result = (if condition
    { temp = [x, y]; <- temp }
    { temp = [y, x]; <- temp }
  )

  // ✅ CORRECT - Create list outside if-else
  first = (if condition {<- x} {<- y})
  second = (if condition {<- y} {<- x})
  result = [first, second]
  ```
- List comprehensions not yet implemented
- Pattern matching / destructuring not available
- Higher-order functions (map/filter/reduce) work in runtime mode but not LLVM native mode yet


## Comparison with Other Languages

| Feature | Franz (LLVM) | Rust | Python | JavaScript |
|---------|--------------|------|--------|------------|
| Heterogeneous lists | ✅ | ✅ | ✅ | ✅ |
| Native compilation | ✅ | ✅ | ❌ | ❌ |
| Zero runtime overhead | ✅ | ✅ | ❌ | ❌ |
| Type-tagged pointers | ✅ | ✅ | ✅ | ✅ |
| C-level performance | ✅ | ✅ | ❌ | ❌ |
| Arbitrary nesting | ✅ | ✅ | ✅ | ✅ |

