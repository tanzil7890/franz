# LLVM Math Functions API Reference



## Overview

Franz's LLVM native compiler includes 10 mathematical functions that achieve C-level performance through direct integration with the C math library (`libm`). All functions support automatic type promotion between integers and floats.

**Performance**: Rust-level performance (direct LLVM IR → machine code, no interpreter overhead)

---

## Quick Reference

| Function | Signature | Example | Result | Type |
|----------|-----------|---------|--------|------|
| **remainder** | `(remainder a b)` | `(remainder 10 3)` | `1` | INT/FLOAT |
| **power** | `(power base exp)` | `(power 2 8)` | `256` | FLOAT |
| **random** | `(random)` | `(random)` | `0.0-1.0` | FLOAT |
| **random_int** | `(random_int max)` | `(random_int 6)` | `0-5` | INT |
| **random_range** | `(random_range min max)` | `(random_range -10 40)` | `-10.0 to <40.0` | FLOAT |
| **random_seed** | `(random_seed n)` | `(random_seed 42)` | `void (returns 0)` | INT/VOID |
| **floor** | `(floor x)` | `(floor 3.7)` | `3` | FLOAT |
| **ceil** | `(ceil x)` | `(ceil 3.2)` | `4` | FLOAT |
| **round** | `(round x)` | `(round 3.5)` | `4` | FLOAT |
| **abs** | `(abs x)` | `(abs -42)` | `42` | INT/FLOAT |
| **min** | `(min x1 x2 ...)` | `(min 5 2 8 1)` | `1` | INT/FLOAT |
| **max** | `(max x1 x2 ...)` | `(max 5 2 8 1)` | `8` | INT/FLOAT |
| **sqrt** | `(sqrt x)` | `(sqrt 16)` | `4.0` | FLOAT |

---

## remainder

**Modulo/remainder operation** - Returns the remainder after division.

### Syntax
```franz
(remainder dividend divisor)
```

### Parameters
- `dividend` (INT or FLOAT) - The number to be divided
- `divisor` (INT or FLOAT) - The number to divide by

### Returns
- **INT**: If both arguments are integers (uses LLVM `srem`)
- **FLOAT**: If either argument is float (uses C `fmod()`)

### Examples
```franz
(remainder 10 3)        // → 1 (int)
(remainder -10 3)       // → -1 (negative remainder)
(remainder 10.5 3.0)    // → 1.5 (float)
(remainder 17 5)        // → 2
```



---

## power

**Exponentiation** - Raises a base to an exponent.

### Syntax
```franz
(power base exponent)
```

### Parameters
- `base` (INT or FLOAT) - The base number
- `exponent` (INT or FLOAT) - The power to raise to

### Returns
- **FLOAT**: Always returns float (uses C `pow()`)

### Examples
```franz
(power 2 8)           // → 256.0 (2^8)
(power 2.0 0.5)       // → 1.414214 (square root of 2)
(power 5 0)           // → 1.0 (anything^0 = 1)
(power 10 -2)         // → 0.01 (negative exponents)
(power 3 3)           // → 27.0
```


### Use Cases
- Scientific calculations
- Compound interest: `(power (add 1.0 rate) years)`
- Square roots: `(power x 0.5)` (or use `sqrt`)
- Distance calculations: Pythagorean theorem


---

## random

**Random number generation** - Returns a random float between 0.0 and 1.0.

### Syntax
```franz
(random)
```

### Parameters
- None

### Returns
- **FLOAT**: Random value in range [0.0, 1.0)

### Examples
```franz
r1 = (random)              // → 0.0-1.0
r2 = (random)              // → Different value
(random)                   // → Each call returns new value
```



### Use Cases
- Game development
- Simulations
- Random sampling
- Testing/fuzzing

### Usage Patterns
```franz
// Random integer in range [min, max]
min = 1
max = 10
random_int = (add min (floor (multiply (random) (add (subtract max min) 1))))

// Random float in range [min, max]
min = 0.0
max = 100.0
random_float = (add min (multiply (random) (subtract max min)))

// Random boolean (0 or 1)
random_bool = (floor (multiply (random) 2))
```

---

## random_int

**Random integer generation** - Returns a random integer in the range `[0, n)`.

### Syntax
```franz
(random_int n)
```

### Parameters
- `n` (INT/FLOAT) - Upper bound (exclusive). Floats are truncated to int.

### Returns
- **INT**: Random integer in `[0, n)`. Use `(add (random_int 6) 1)` to emulate a 1-6 dice roll.

### Examples
```franz
(random_int 6)    // → 0..5
(random_int 2)    // → 0 or 1 (coin flip)
(random_int 256)  // → 0..255 (RGB channel)
```

### Implementation Details
- Uses `rand()` then `% n` (signed remainder) in LLVM.
- Negative values are corrected to stay non-negative.
- Float arguments are converted to integers.

---

## random_range

**Random float in a custom range** - Returns a float in `[min, max)`.

### Syntax
```franz
(random_range min max)
```

### Parameters
- `min` (INT/FLOAT) - Lower bound (inclusive)
- `max` (INT/FLOAT) - Upper bound (exclusive)

### Returns
- **FLOAT**: Random float scaled to the provided range.

### Examples
```franz
(random_range -10 40)    // → -10.0 .. <40.0
(random_range 0 5)       // → 0.0 .. <5.0
(random_range 8000 9000) // → 8000.0 .. <9000.0 (ports)
```

### Implementation Details
- Normalizes `rand()` over `RAND_MAX`, then scales by `(max - min)` and offsets by `min`.
- Int arguments are promoted to float.

---

## random_seed

**Seed the RNG** - Sets the RNG seed for reproducible sequences.

### Syntax
```franz
(random_seed n)
```

### Parameters
- `n` (INT/FLOAT) - Seed value (truncated to 32-bit int).

### Returns
- **INT**: Always returns `0` (dummy value representing void).

### Examples
```franz
(random_seed 42)
(random_int 10)   // deterministic after seed
(random_range 0 1)
```

### Implementation Details
- Calls `srand(n)` in LLVM native mode.
- Accepts floats by converting to int.

---

## floor

**Round down** - Returns the largest integer less than or equal to the input.

### Syntax
```franz
(floor x)
```

### Parameters
- `x` (INT or FLOAT) - The number to round down

### Returns
- **FLOAT**: Rounded-down value (even for integer inputs)

### Examples
```franz
(floor 3.7)           // → 3.0
(floor -2.3)          // → -3.0 (rounds toward negative infinity)
(floor 5)             // → 5.0 (integers unchanged)
(floor 0.1)           // → 0.0
(floor -0.9)          // → -1.0
```

### Implementation Details
- **Uses C floor()**: Calls `floor()` from `<math.h>`
- **Always returns float**: Even for integer inputs

### Use Cases
- Converting floats to integers (rounding down)
- Truncating decimal places
- Grid/tile calculations in games
- Binning continuous data

---

## ceil

**Round up** - Returns the smallest integer greater than or equal to the input.

### Syntax
```franz
(ceil x)
```

### Parameters
- `x` (INT or FLOAT) - The number to round up

### Returns
- **FLOAT**: Rounded-up value

### Examples
```franz
(ceil 3.2)            // → 4.0
(ceil -2.7)           // → -2.0 (rounds toward positive infinity)
(ceil 5)              // → 5.0 (integers unchanged)
(ceil 0.1)            // → 1.0
(ceil -0.1)           // → 0.0
```

### Implementation Details
- **Uses C ceil()**: Calls `ceil()` from `<math.h>`
- **Always returns float**: Even for integer inputs

### Use Cases
- Calculating required pages/chunks (always round up)
- Ensuring minimum allocation sizes
- Progress bars (always show partial progress as full unit)

---

## round

**Round to nearest** - Rounds to the nearest integer (half-away-from-zero).

### Syntax
```franz
(round x)
```

### Parameters
- `x` (INT or FLOAT) - The number to round

### Returns
- **FLOAT**: Rounded value

### Examples
```franz
(round 3.5)           // → 4.0 (rounds up at .5)
(round 3.4)           // → 3.0 (rounds down)
(round -2.5)          // → -2.0 (rounds toward zero at .5)
(round 5)             // → 5.0 (integers unchanged)
```

### Implementation Details
- **Uses C round()**: Calls `round()` from `<math.h>`
- **Rounding strategy**: "Round half away from zero"
- **Always returns float**: Even for integer inputs

### Use Cases
- Displaying values to users (no decimal places)
- Financial calculations (nearest dollar/cent)
- UI/layout calculations

### Rounding Strategy Comparison
```franz
// 0.5 edge cases
(floor 3.5)    // → 3.0 (always down)
(ceil 3.5)     // → 4.0 (always up)
(round 3.5)    // → 4.0 (nearest, ties away from zero)

(floor -3.5)   // → -4.0
(ceil -3.5)    // → -3.0
(round -3.5)   // → -3.0
```

---

## abs

**Absolute value** - Returns the non-negative value of a number.

### Syntax
```franz
(abs x)
```

### Parameters
- `x` (INT or FLOAT) - The number to take absolute value of

### Returns
- **INT**: If input is integer (uses LLVM `select`)
- **FLOAT**: If input is float (uses C `fabs()`)

### Examples
```franz
(abs -42)             // → 42 (int)
(abs 17)              // → 17 (positive unchanged)
(abs -3.14)           // → 3.14 (float)
(abs 0)               // → 0
```

### Implementation Details
- **Integer path**: Uses LLVM conditional select: `if x < 0 then -x else x`
- **Float path**: Calls C `fabs()` from `<math.h>`
- **Preserves type**: INT → INT, FLOAT → FLOAT

### Use Cases
- Distance calculations: `(abs (subtract a b))`
- Error/deviation measurements
- Range checking

---

## min

**Minimum value** - Returns the smallest value from 1 or more arguments (variadic).

### Syntax
```franz
(min x1)
(min x1 x2)
(min x1 x2 x3)
(min x1 x2 x3 ...)  // Arbitrary number of arguments
```

### Parameters
- `x1, x2, ...` (INT or FLOAT) - Values to compare (minimum 1 argument)

### Returns
- **INT**: If all arguments are integers
- **FLOAT**: If any argument is float (promotes all to float)

### Examples
```franz
(min 5 2)             // → 2 (two arguments)
(min 5 2 8 1)         // → 1 (four arguments)
(min 3.5 1.2 4.8)     // → 1.2 (floats)
(min 5 2.5 8)         // → 2.5 (mixed → all promoted to float)
(min 42)              // → 42 (single argument)
```

### Implementation Details
- **Variadic**: Accepts 1 to N arguments
- **Type promotion**: If any argument is FLOAT, all promoted to FLOAT
- **Algorithm**: Iterative pairwise comparison using LLVM `select` with `fcmp olt`
- **Memory**: Dynamic allocation for argument array (freed after use)

### Use Cases
- Finding lowest temperature, price, score
- Clamping values: `(max lower_bound (min value upper_bound))`
- Statistical analysis

---

## max

**Maximum value** - Returns the largest value from 1 or more arguments (variadic).

### Syntax
```franz
(max x1)
(max x1 x2)
(max x1 x2 x3)
(max x1 x2 x3 ...)  // Arbitrary number of arguments
```

### Parameters
- `x1, x2, ...` (INT or FLOAT) - Values to compare (minimum 1 argument)

### Returns
- **INT**: If all arguments are integers
- **FLOAT**: If any argument is float (promotes all to float)

### Examples
```franz
(max 5 2)             // → 5 (two arguments)
(max 5 2 8 1)         // → 8 (four arguments)
(max 3.5 1.2 4.8)     // → 4.8 (floats)
(max 5 2.5 8 1.0)     // → 8.0 (mixed → all promoted to float)
(max 42)              // → 42 (single argument)
```

### Implementation Details
- **Variadic**: Accepts 1 to N arguments
- **Type promotion**: If any argument is FLOAT, all promoted to FLOAT
- **Algorithm**: Iterative pairwise comparison using LLVM `select` with `fcmp ogt`
- **Memory**: Dynamic allocation for argument array (freed after use)

### Use Cases
- Finding highest temperature, price, score
- Ensuring minimum values: `(max 0 result)` (never negative)
- Statistical analysis

---

## sqrt

**Square root** - Returns the square root of a number.

### Syntax
```franz
(sqrt x)
```

### Parameters
- `x` (INT or FLOAT) - The number to take square root of (non-negative)

### Returns
- **FLOAT**: Square root value

### Examples
```franz
(sqrt 16)             // → 4.0 (perfect square)
(sqrt 2.0)            // → 1.414214 (irrational)
(sqrt 100.0)          // → 10.0
(sqrt 0)              // → 0.0
```

### Implementation Details
- **Uses C sqrt()**: Calls `sqrt()` from `<math.h>`
- **Always returns float**: Even for integer inputs
- **Type promotion**: Automatically promotes integers to float

### Use Cases
- Distance calculations (Pythagorean theorem)
- Standard deviation
- Geometry (circle radius, sphere volume)
- Physics simulations

### Common Patterns
```franz
// Distance between two 2D points
x1 = 0
y1 = 0
x2 = 3
y2 = 4
dx = (subtract x2 x1)
dy = (subtract y2 y1)
distance = (sqrt (add (power dx 2) (power dy 2)))  // → 5.0

// Alternative to power for square root
(sqrt x)              // Preferred
(power x 0.5)         // Also works
```

### Edge Cases
- Negative input: Returns NaN (no error checking)
- Zero: Returns 0.0
- Large numbers: Works up to FLOAT precision limits

---

## Type Promotion Rules

All math functions follow consistent type promotion:

1. **Both INT**: Result is INT (if function supports it)
2. **Either FLOAT**: Both promoted to FLOAT, result is FLOAT
3. **Always FLOAT**: Some functions always return FLOAT (power, sqrt, floor, ceil, round, random)

### Examples
```franz
// Both INT → INT
(remainder 10 3)      // → 1 (INT)
(min 5 2 8)           // → 2 (INT)
(abs -42)             // → 42 (INT)

// Mixed INT/FLOAT → FLOAT
(remainder 10 3.0)    // → 1.0 (FLOAT)
(min 5 2.5 8)         // → 2.5 (FLOAT)

// Always FLOAT
(power 2 8)           // → 256.0 (always FLOAT)
(sqrt 16)             // → 4.0 (always FLOAT)
(floor 5)             // → 5.0 (always FLOAT)
```

---

## Error Handling

**Important**: These functions perform minimal error checking for performance.

| Function | Error Condition | Behavior |
|----------|----------------|----------|
| remainder | Divide by zero | Undefined behavior |
| power | Negative base with fractional exp | Returns NaN |
| sqrt | Negative input | Returns NaN |
| floor/ceil/round | N/A | Always safe |
| abs | N/A | Always safe |
| min/max | Zero arguments | Compile error |
| random | N/A | Always safe |

**Validation**: All functions validate argument count at compile-time:
```
ERROR: remainder requires exactly 2 arguments at line X
ERROR: power requires exactly 2 arguments at line X
ERROR: floor requires exactly 1 argument at line X
ERROR: min requires at least 1 argument at line X
```

---

## Performance

### Execution Speed
- **Native performance**: C-level speed (direct libm calls)
- **Zero overhead**: No interpreter, direct LLVM IR → machine code
- **Optimized**: Compiler optimizations applied (inlining, constant folding)

### Comparison
| Mode | Speed | Notes |
|------|-------|-------|
| Interpreter | Baseline | AST traversal overhead |
| LLVM Native | 10-100x faster | Direct machine code |
| C Native | Same as LLVM | Franz LLVM matches C performance |

### Memory Usage
- **Type promotion**: Automatic, no manual conversion needed
- **Variadic functions**: Dynamic array allocation (freed immediately)
- **Cleanup**: All memory properly freed after execution

---

## Complete Examples

### Example 1: Scientific Calculator
```franz
// Circle calculations
radius = 5.0
pi = 3.14159
area = (multiply pi (power radius 2))
circumference = (multiply 2.0 (multiply pi radius))
(println "Area:" area)                    // → 78.539750
(println "Circumference:" circumference)  // → 31.415900

// Distance between points
x1 = 0
y1 = 0
x2 = 3
y2 = 4
distance = (sqrt (add (power (subtract x2 x1) 2) (power (subtract y2 y1) 2)))
(println "Distance:" distance)            // → 5.0
```

### Example 2: Statistics
```franz
// Temperature statistics
day1 = 72
day2 = 68
day3 = 75
day4 = 71
day5 = 69
lowest = (min day1 day2 day3 day4 day5)
highest = (max day1 day2 day3 day4 day5)
(println "Min temperature:" lowest)       // → 68
(println "Max temperature:" highest)      // → 75
```

### Example 3: Financial Calculations
```franz
// Compound interest
principal = 1000.0
rate = 0.05
years = 10.0
final_amount = (multiply principal (power (add 1.0 rate) years))
(println "Final amount: $" final_amount)  // → $1628.89
```

### Example 4: Rounding for Display
```franz
value = 3.14159265
(println "Original:" value)
(println "Rounded:" (round value))        // → 3
(println "Floor:" (floor value))          // → 3
(println "Ceil:" (ceil value))            // → 4
```

### Example 5: Random Number Generation
```franz
// Generate 3 random numbers
r1 = (random)
r2 = (random)
r3 = (random)
(println "Random:" r1)                    // → 0.0-1.0
(println "Random:" r2)                    // → 0.0-1.0
(println "Random:" r3)                    // → 0.0-1.0

// Random integer in range [1, 10]
random_int = (add 1 (floor (multiply (random) 10)))
(println "Random int:" random_int)        // → 1-10
```

---

## Testing

### Test Suite
**File**: [test/llvm-math/math-comprehensive-test.franz](../../test/llvm-math/math-comprehensive-test.franz)

**Results**: 27/27 tests passing (100%)

**Categories**:
- remainder: 3 tests
- power: 3 tests
- random: 3 tests
- floor/ceil/round: 6 tests
- abs: 3 tests
- min/max: 6 tests
- sqrt: 3 tests

### Running Tests
```bash
# Compile Franz with LLVM support
find src -name "*.c" -not -name "check.c" | xargs gcc \
  -I/opt/homebrew/Cellar/llvm@17/17.0.6/include \
  -L/opt/homebrew/Cellar/llvm@17/17.0.6/lib \
  -lLLVM-17 -Wall -lm -o franz

# Run comprehensive test suite
./franz --llvm-native test/llvm-math/math-comprehensive-test.franz
```

---
