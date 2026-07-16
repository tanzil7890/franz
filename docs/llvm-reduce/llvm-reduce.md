# LLVM Reduce Function

## Overview

The `reduce` function in Franz's LLVM native compilation mode reduces a list to a single value by applying a callback closure to each element, accumulating results.


## Syntax

```franz
(reduce list callback initial)
(reduce list callback)  // initial defaults to void
```

## Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `list` | List | The list to reduce |
| `callback` | Closure | Function with signature `{acc item index -> new_acc}` |
| `initial` | Any | Initial accumulator value (optional, defaults to void) |

## Callback Signature

The callback receives three arguments:
- `acc` - Current accumulator value
- `item` - Current list element
- `index` - Current index (0-based)

Returns the new accumulator value.

## Examples

### Sum of Numbers
```franz
numbers = [1, 2, 3, 4, 5]
sum_fn = {acc item idx -> <- (add acc item)}
total = (reduce numbers sum_fn 0)
// Result: 15
```

### Product (Factorial)
```franz
factors = [1, 2, 3, 4, 5]
prod_fn = {acc item idx -> <- (multiply acc item)}
factorial_5 = (reduce factors prod_fn 1)
// Result: 120
```

### Find Maximum
```franz
values = [3, 7, 2, 9, 4]
max_fn = {acc item idx -> <- (if (greater_than item acc) {<- item} {<- acc})}
max_val = (reduce values max_fn 0)
// Result: 9
```

### Count Elements
```franz
items = [10, 20, 30, 40]
count_fn = {acc item idx -> <- (add acc 1)}
count = (reduce items count_fn 0)
// Result: 4
```

### Sum Using Index
```franz
items = [100, 200, 300, 400]
idx_sum = {acc item idx -> <- (add acc idx)}
result = (reduce items idx_sum 0)
// Result: 6 (0+1+2+3)
```


## Performance

| Operation | LLVM Native | Runtime | Speedup |
|-----------|-------------|---------|---------|
| reduce (small list) | ~10μs | ~100μs | **10x** |
| reduce (large list) | ~1ms | ~10ms | **10x** |

## Related Functions

- `map` - Transform list elements
- `filter` - Filter list by predicate
- `fold` - Alias for reduce (not implemented)

