# Multi-Base Number Formats



## Overview

Franz supports multiple number format literals for integers and floats, matching modern languages like C, Rust, and Python. This feature enables direct use of hexadecimal, binary, octal, and hexadecimal float literals in your code without manual conversion.

## Integer Formats

### Decimal (Base 10)
Standard integer format, no prefix required.

```franz
x = 42
y = 1000
z = -123
```

### Hexadecimal (Base 16)
Prefix: `0x` or `0X`
Digits: `0-9`, `a-f`, `A-F`

```franz
// Basic hex
color = 0xFF        // 255
addr = 0x1A2B       // 6699

// Case insensitive
val1 = 0xDEAD       // 57005
val2 = 0Xdead       // 57005 (same value)

// Large hex values
large = 0xDEADBEEF  // 3735928559
```

### Binary (Base 2)
Prefix: `0b` or `0B`
Digits: `0-1`

```franz
// Basic binary
flags = 0b1010      // 10
mask = 0b11111111   // 255

// Binary arithmetic
byte = 0b10000000   // 128
result = (add 0b1111 0b0001)  // 16
```

### Octal (Base 8)
Prefix: `0o` or `0O`
Digits: `0-7`

```franz
// Basic octal
perms = 0o755       // 493
val = 0o377         // 255

// Octal operations
x = 0o10            // 8
y = (multiply 0o10 0o10)  // 64
```

### Negative Integers

All integer formats support negative values:

```franz
negHex = -0xFF      // -255
negBin = -0b1010    // -10
negOct = -0o77      // -63
negDec = -42        // -42
```

## Float Formats

### Decimal Float
Standard floating-point notation.

```franz
pi = 3.14159
e = 2.71828
ratio = 0.5
negative = -123.456
```

### Scientific Notation
Format: `mantissa e exponent` or `mantissa E exponent`
Exponent can have optional `+` or `-` sign.

```franz
// Basic scientific
avogadro = 6.022e23     // 6.022 × 10^23
planck = 6.626e-34      // 6.626 × 10^-34

// Positive exponent
large1 = 1.5e2          // 150.0
large2 = 1E+3           // 1000.0

// Negative exponent
small1 = 1.5e-3         // 0.0015
small2 = 3.14E-2        // 0.0314

// Integer mantissa allowed
val = 2e10              // 20000000000.0
```

### Hexadecimal Float (IEEE 754)
Format: `0x mantissa p exponent`
Mantissa: hexadecimal fraction
Exponent: binary (power of 2), can be negative

**Mathematical meaning**: `mantissa × 2^exponent`

```franz
// Basic hex floats
one = 0x1.0p0           // 1.0 × 2^0 = 1.0
two = 0x1.0p1           // 1.0 × 2^1 = 2.0
twelve = 0x1.8p3        // 1.5 × 2^3 = 12.0

// Negative exponent
frac = 0x1.0p-1         // 1.0 × 2^-1 = 0.5
small = 0x1.Ap-3        // 1.625 × 2^-3 = 0.203125

// Precise fractions
fifth = 0x1.999999999999ap-3  // Exactly 1/5

// Case insensitive
val1 = 0x1.5p2          // 5.25
val2 = 0X1.5P2          // 5.25 (same)
```


## Mixed-Base Arithmetic

All number formats work seamlessly in arithmetic operations:

```franz
// Hex + Binary
sum = (add 0xFF 0b1)        // 256

// Octal * Hex
product = (multiply 0o10 0x10)  // 128

// All bases together
result = (add (add 0x10 0b10) 0o10)  // 26

// Scientific + Hex float
mixed = (add 1.5e2 0x1.0p4)  // 166.0
```

## Conversion to Different Bases

Numbers are always stored internally as integers or floats, regardless of the input format. They are displayed as decimal by default:

```franz
hexval = 0xABCD
(println hexval)            // Outputs: 43981 (decimal)

binval = 0b11110000
(println binval)            // Outputs: 240 (decimal)

octval = 0o1234
(println octval)            // Outputs: 668 (decimal)

hexfloat = 0x1.FFFp10
(println hexfloat)          // Outputs: 2047.937500 (decimal)
```

To format numbers in specific bases, use the formatting functions (see [String Formatting](../string-formatting/string-formatting.md)).

## Use Cases

### 1. Bit Manipulation
```franz
// Set specific bits
flags = 0b00000000
flag1 = 0b00000001
flag2 = 0b00000010
flag3 = 0b00000100

enabled = (add (add flag1 flag2) flag3)  // 0b00000111

// Check bit with hex mask
status = 0xFF
mask = 0x0F
masked = (multiply status mask)  // Lower nibble
```

### 2. Color Values
```franz
// RGB colors in hex
red = 0xFF0000
green = 0x00FF00
blue = 0x0000FF
white = 0xFFFFFF
black = 0x000000

// Extract components (simplified)
color = 0xRRGGBB
```

### 3. File Permissions (Unix)
```franz
// Owner: rwx (7), Group: rx (5), Other: rx (5)
perms = 0o755

// Owner: rw (6), Group: r (4), Other: r (4)
filePerms = 0o644
```

### 4. Memory Addresses
```franz
// Memory-mapped I/O addresses
baseAddr = 0x40000000
offset = 0x100
addr = (add baseAddr offset)  // 0x40000100
```

### 5. Scientific Computing
```franz
// Physical constants
speedOfLight = 2.998e8      // m/s
electronCharge = 1.602e-19   // coulombs
avogadro = 6.022e23         // mol^-1

// Precise fractions with hex floats
oneThird = 0x1.5555555555555p-2  // More accurate than 0.333...
```

## Edge Cases

### Zero
All formats support zero:
```franz
dec_zero = 0
hex_zero = 0x0
bin_zero = 0b0
oct_zero = 0o0
float_zero = 0.0
sci_zero = 0e0
hexf_zero = 0x0.0p0
```

### Maximum Values
```franz
// 64-bit signed integer max (depends on platform)
max_positive = 9223372036854775807

// Large hex
large_hex = 0xFFFFFFFFFFFFFFFF

// Large binary (impractical to write, but valid)
large_bin = 0b1111111111111111111111111111111111111111111111111111111111111111
```

### Precision Notes

- **Integers**: 64-bit signed (range: -2^63 to 2^63-1)
- **Floats**: IEEE 754 double precision (53-bit mantissa)
- **Hex floats**: Exact binary representation (no decimal conversion errors)

## Performance

### Compile-Time Parsing
All number literals are parsed **at compile time** (during lexing), not at runtime. This means:

- ✅ Zero runtime overhead
- ✅ Constants are embedded directly in LLVM IR
- ✅ Same performance as decimal literals

### LLVM IR Generation

```llvm
; Decimal: x = 42
%x = alloca i64
store i64 42, i64* %x

; Hex: y = 0xFF
%y = alloca i64
store i64 255, i64* %y

; Binary: z = 0b1010
%z = alloca i64
store i64 10, i64* %z

; Hex float: f = 0x1.8p3
%f = alloca double
store double 1.200000e+01, double* %f
```

All formats compile to identical IR for the same value.


## Comparison with Other Languages

### C/C++
```c
// C supports all these formats (C99+)
int hex = 0xFF;
int oct = 0755;          // ⚠️ Leading 0 (Franz uses 0o)
int bin = 0b1010;        // C23+
double hexf = 0x1.5p2;   // C99+
double sci = 1.5e2;      // Standard
```

### Rust
```rust
// Rust supports all formats
let hex = 0xFF;
let oct = 0o755;         // Explicit 0o prefix
let bin = 0b1010;
// No hex float literals in Rust
let sci = 1.5e2;
```

### Python
```python
# Python 3 supports all formats
hex_val = 0xFF
oct_val = 0o755         # Explicit 0o prefix
bin_val = 0b1010
hex_float = 0x1.5p2     # Python 3.6+ (float.fromhex)
sci = 1.5e2
```

### Franz Advantages
✅ **Consistent prefix syntax**: All non-decimal formats use `0x`, `0b`, `0o`
✅ **Hex float support**: Native IEEE 754 hex float literals
✅ **LLVM native compilation**: Zero runtime overhead
✅ **Seamless mixing**: All formats work together in expressions

## Troubleshooting

### Invalid Digit Errors

**Problem**: `Syntax Error: Invalid digit in number literal`

**Cause**: Using digits outside the valid range for the base.

```franz
// ❌ Wrong
bad_bin = 0b1012    // '2' is invalid in binary
bad_oct = 0o789     // '8' and '9' are invalid in octal

// ✅ Correct
good_bin = 0b1010   // Only 0 and 1
good_oct = 0o755    // Only 0-7
```

### Hex Float Confusion

**Problem**: `0x1.5p2` gives unexpected result

**Cause**: Forgetting that exponent is binary (power of 2), not decimal (power of 10).

```franz
// ❌ Common mistake
val = 0x1.0p10      // NOT 1.0 × 10^10
                    // Actually: 1.0 × 2^10 = 1024.0

// ✅ Correct understanding
val = 0x1.0p3       // 1.0 × 2^3 = 8.0
val = 0x1.5p2       // 1.5 × 2^2 = 6.0
```

### Leading Zero Ambiguity

**Problem**: Is `0123` octal or decimal?

**Franz behavior**: `0123` is **decimal** (123), not octal.
Use explicit `0o` prefix for octal: `0o123` (83 in decimal).

```franz
// ❌ Ambiguous (C-style)
val = 0755          // Treated as decimal 755 in Franz

// ✅ Explicit (Franz-style)
val = 0o755         // Octal 755 (493 in decimal)
```

## Summary

Franz's multi-base number format support provides:

✅ **Hexadecimal integers** (`0xFF`) - Colors, addresses, bit masks
✅ **Binary integers** (`0b1010`) - Bit manipulation, flags
✅ **Octal integers** (`0o755`) - File permissions, legacy systems
✅ **Hexadecimal floats** (`0x1.5p2`) - Exact IEEE 754 representation
✅ **Scientific notation** (`1.5e2`) - Large/small numbers
✅ **LLVM native compilation** - Zero runtime overhead
✅ **Mixed-base arithmetic** - All formats work together seamlessly

This feature brings Franz in line with modern systems programming languages while maintaining simplicity and performance.
