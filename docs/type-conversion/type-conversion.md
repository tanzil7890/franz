# Type Conversion Functions



## Overview

Franz provides three  type conversion functions that enable seamless conversion between strings, integers, and floats. These functions are fully integrated into the LLVM native compiler for maximum performance.

## Available Functions

### `(integer x)`
Converts a value to an integer type.

**Supported Input Types:**
- `string` → Uses C stdlib `atoi()` for parsing
- `float` → Truncates decimal portion (not rounded)
- `integer` → Identity operation (returns same value)

**Behavior:**
- String parsing handles positive/negative numbers, leading zeros
- Float conversion truncates toward zero (7.9 → 7, -3.7 → -3)
- Invalid strings return 0 (C stdlib behavior)
- Empty strings return 0

**Examples:**
```franz
// String to integer
age = (integer "25")        // 25
count = (integer "-10")     // -10

// Float to integer (truncation)
price = (integer 19.99)     // 19
temp = (integer -2.5)       // -2

// Identity
num = (integer 42)          // 42
```

### `(float x)`
Converts a value to a floating-point type.

**Supported Input Types:**
- `string` → Uses C stdlib `atof()` for parsing
- `integer` → Converts to double-precision float
- `float` → Identity operation (returns same value)

**Behavior:**
- String parsing supports decimal points and scientific notation
- Integer conversion is exact (no precision loss for small values)
- Invalid strings return 0.0 (C stdlib behavior)

**Examples:**
```franz
// String to float
pi = (float "3.14159")      // 3.141590
negative = (float "-2.5")   // -2.500000

// Integer to float
whole = (float 100)         // 100.000000

// Scientific notation string
distance = (float "1.5e8")  // 150000000.000000
```

### `(string x)`
Converts a value to a string representation.

**Supported Input Types:**
- `integer` → Uses `snprintf` with `%lld` format
- `float` → Uses `snprintf` with `%f` format (6 decimal places)
- `string` → Identity operation (returns same value)

**Behavior:**
- Integer conversion produces exact decimal representation
- Float conversion uses standard C formatting (trailing zeros included)
- Stack-allocated buffers (50 bytes) for efficient conversion
- No memory leaks - buffers are function-scoped

**Examples:**
```franz
// Integer to string
score = (string 9850)       // "9850"
negative = (string -100)    // "-100"

// Float to string
temp = (string 36.6)        // "36.600000"
small = (string 0.0001)     // "0.000100"

// Identity
msg = (string "hello")      // "hello"
```

## Type Conversion Matrix

| From ↓ To → | integer | float | string |
|-------------|---------|-------|--------|
| **integer** | identity | exact | decimal repr |
| **float**   | truncate | identity | formatted |
| **string**  | atoi() | atof() | identity |

## Advanced Usage

### Chained Conversions

```franz
// Round-trip conversion (lossless for integers)
original = 12345
as_string = (string original)
back = (integer as_string)  // 12345

// Multi-step parsing
input = "99.99"
as_float = (float input)
as_int = (integer as_float)  // 99 (truncated)
```

### Type Conversion in Arithmetic

```franz
// Parse user input and calculate
input1 = "100"
input2 = "50"
sum = (add (integer input1) (integer input2))  // 150

// Mixed type operations
base = (integer "10")
result = (multiply (float base) 2.5)  // 25.000000
```

### Data Validation Pattern

```franz
// Parse and validate numeric input
age_input = "25"
age = (integer age_input)
// Can now use 'age' in numeric comparisons
```

## Edge Cases

### String Parsing Edge Cases

```franz
// Empty string
(integer "")      // 0
(float "")        // 0.0

// Invalid strings
(integer "abc")   // 0 (atoi behavior)
(float "xyz")     // 0.0 (atof behavior)

// Partial parsing
(integer "123abc")  // 123 (stops at non-digit)
(float "3.14foo")   // 3.14 (stops at non-numeric)
```

### Numeric Truncation

```franz
// Float to int truncation (toward zero)
(integer 7.9)    // 7 (not 8)
(integer -7.9)   // -7 (not -8)
(integer 0.999)  // 0
```

### Boundary Values

```franz
// Zero
(integer "0")    // 0
(string 0)       // "0"

// Large numbers
(integer "999999")    // 999999
(string 999999)       // "999999"

// Very small floats
(string 0.00001)      // "0.000010"
```

## Performance Characteristics

### LLVM Native Compilation

All type conversions compile to **native LLVM IR** with zero interpreter overhead:

- **`integer(string)`**: Direct call to C `atoi()` + sign extension
- **`float(string)`**: Direct call to C `atof()`
- **`string(int/float)`**: Stack-allocated buffer + `snprintf()` call
- **Numeric conversions**: LLVM cast instructions (`sitofp`, `fptosi`)

**Performance:**
- String parsing: ~100ns per call (C stdlib speed)
- Numeric casts: ~1-2 CPU cycles (native LLVM instructions)
- String formatting: ~200ns per call (snprintf overhead)

## Implementation Details

### LLVM IR Generation

The type conversion functions are implemented as:

1. **C Runtime Integration**: Direct calls to `atoi()`, `atof()`, `snprintf()`
2. **Type Checking**: Runtime type detection using LLVM `LLVMTypeOf()`
3. **Instruction Generation**:
   - `LLVMBuildSIToFP` for int→float
   - `LLVMBuildFPToSI` for float→int
   - `LLVMBuildCall2` for C stdlib functions

#

### Working Examples

**Comprehensive Examples:** [`examples/type-conversion/working/comprehensive-conversions.franz`](../../examples/type-conversion/working/comprehensive-conversions.franz)

**Demonstrates:**
- String to numeric conversions
- Numeric to string conversions
- Cross-type arithmetic
- Round-tripping
- Truncation behavior
- Real-world use cases

**Run Examples:**
```bash
./franz examples/type-conversion/working/comprehensive-conversions.franz
```

## Comparison with Other Languages

| Language | int() | float() | str() |
|----------|-------|---------|-------|
| **Franz** | ✅ atoi | ✅ atof | ✅ snprintf |
| Python | ✅ int() | ✅ float() | ✅ str() |
| JavaScript | ✅ parseInt() | ✅ parseFloat() | ✅ String() |
| Rust | ✅ parse() | ✅ parse() | ✅ to_string() |
| Go | ✅ strconv.Atoi | ✅ strconv.ParseFloat | ✅ fmt.Sprint |

Franz's implementation matches  with C-level performance.

