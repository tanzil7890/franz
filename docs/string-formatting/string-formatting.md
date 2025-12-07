# String Formatting Functions



## Overview

Franz provides powerful string formatting functions to convert numbers to different bases and control float precision. These functions complement the multi-base number literals by allowing runtime conversion and custom output formatting.

## Functions

### format-int

Convert an integer to a string representation in a specified base.

**Syntax**:
```franz
(format-int value base)
```

**Parameters**:
- `value` (integer): The number to format
- `base` (integer): Target base (2, 8, 10, or 16)

**Returns**: String with appropriate prefix:
- Base 2 (binary): `"0b..."`
- Base 8 (octal): `"0o..."`
- Base 10 (decimal): `"..."` (no prefix)
- Base 16 (hexadecimal): `"0x..."` (uppercase digits)

**Examples**:
```franz
// Binary formatting
bin = (format-int 255 2)
(println bin)  // "0b11111111"

// Octal formatting
oct = (format-int 255 8)
(println oct)  // "0o377"

// Decimal formatting
dec = (format-int 255 10)
(println dec)  // "255"

// Hexadecimal formatting
hex = (format-int 255 16)
(println hex)  // "0xFF"

// Negative numbers
neg = (format-int -42 16)
(println neg)  // "-0x2A"
```

**Error Handling**:
```franz
// Invalid base causes runtime error
bad = (format-int 100 5)
// Runtime Error @ Line X: format-int base must be 2, 8, 10, or 16
```

### format-float

Convert a float to a string with specified decimal precision.

**Syntax**:
```franz
(format-float value precision)
```

**Parameters**:
- `value` (float): The number to format
- `precision` (integer): Number of decimal places (0-17)

**Returns**: String with fixed decimal precision

**Examples**:
```franz
// Basic precision control
pi = 3.14159265359

p0 = (format-float pi 0)
(println p0)  // "3"

p2 = (format-float pi 2)
(println p2)  // "3.14"

p5 = (format-float pi 5)
(println p5)  // "3.14159"

p10 = (format-float pi 10)
(println p10)  // "3.1415926536"

// Scientific notation to fixed
large = 1.5e3
str = (format-float large 2)
(println str)  // "1500.00"

// Hex float to fixed
hexf = 0x1.8p3
str2 = (format-float hexf 2)
(println str2)  // "12.00"

// Negative numbers
neg = (format-float -123.456 2)
(println neg)  // "-123.46"
```

**Precision Clamping**:
- Precision < 0 → Clamped to 0
- Precision > 17 → Clamped to 17 (IEEE 754 double limit)

## Use Cases

### 1. Number Base Conversion

Convert numbers between different representations:

```franz
// User input in decimal, output in hex
userInput = 4095
hexDisplay = (format-int userInput 16)
(println "Color code: " hexDisplay)  // "Color code: 0xFFF"

// Show all representations
num = 42
(println "Binary: " (format-int num 2))    // "Binary: 0b101010"
(println "Octal: " (format-int num 8))     // "Octal: 0o52"
(println "Decimal: " (format-int num 10))  // "Decimal: 42"
(println "Hex: " (format-int num 16))      // "Hex: 0x2A"
```

### 2. Bit Pattern Display

Show binary representation for debugging:

```franz
// Display byte as binary
showByte = {val ->
  binary = (format-int val 2)
  (println "Byte: " binary)
}

(showByte 255)  // "Byte: 0b11111111"
(showByte 128)  // "Byte: 0b10000000"

// Bit flag display
flags = 0b00101101
(println "Active flags: " (format-int flags 2))
// "Active flags: 0b101101"
```

### 3. Currency Formatting

Format monetary values with fixed precision:

```franz
// Price display
formatPrice = {amount ->
  (format-float amount 2)
}

price1 = 19.99
price2 = 100.5
price3 = 7.0

(println "$" (formatPrice price1))  // "$19.99"
(println "$" (formatPrice price2))  // "$100.50"
(println "$" (formatPrice price3))  // "$7.00"

// Tax calculation
subtotal = 99.99
taxRate = 0.0825
tax = (multiply subtotal taxRate)
total = (add subtotal tax)

(println "Subtotal: $" (formatPrice subtotal))  // "$99.99"
(println "Tax: $" (formatPrice tax))            // "$8.25"
(println "Total: $" (formatPrice total))        // "$108.24"
```

### 4. Scientific Display

Convert scientific notation to readable format:

```franz
// Physical constants
speedOfLight = 2.998e8  // m/s
display = (format-float speedOfLight 0)
(println "Speed of light: " display " m/s")
// "Speed of light: 299800000 m/s"

// Microscopic measurements
atomSize = 1.5e-10  // meters
micro = (multiply atomSize 1e9)  // Convert to nanometers
nm = (format-float micro 2)
(println "Atom diameter: " nm " nm")
// "Atom diameter: 0.15 nm"
```

### 5. Percentage Formatting

Display percentages with custom precision:

```franz
// Completion percentage
completed = 17
total = 23
ratio = (divide (multiply completed 1.0) total)
percentage = (multiply ratio 100.0)
display = (format-float percentage 1)

(println "Progress: " display "%")
// "Progress: 73.9%"

// Financial returns
investment = 10000.0
returns = 10750.0
gain = (subtract returns investment)
gainPct = (multiply (divide gain investment) 100.0)
formatted = (format-float gainPct 2)

(println "Return: " formatted "%")
// "Return: 7.50%"
```

### 6. Hex Dump Utility

Create hex dump of memory-like data:

```franz
// Format byte array as hex
dumpBytes = {bytes ->
  hex1 = (format-int bytes 16)
  (println hex1)
}

(dumpBytes 0xDE)  // "0xDE"
(dumpBytes 0xAD)  // "0xAD"
(dumpBytes 0xBE)  // "0xBE"
(dumpBytes 0xEF)  // "0xEF"

// More compact display (could extract raw digits)
showHexPair = {b1 b2 ->
  h1 = (format-int b1 16)
  h2 = (format-int b2 16)
  (println h1 " " h2)
}

(showHexPair 0xCA 0xFE)  // "0xCA 0xFE"
```

### 7. Precision Control in Calculations

Avoid floating-point display issues:

```franz
// Without precision control
result = (divide 1.0 3.0)
(println result)  // "0.333333" (default precision varies)

// With precision control
thirds = (divide 1.0 3.0)
precise = (format-float thirds 15)
(println precise)  // "0.333333333333333"

// Round to specific precision
calculation = (multiply 3.7 8.2)
rounded = (format-float calculation 1)
(println rounded)  // "30.3"
```

### 8. Base Conversion Pipeline

Chain conversions for different representations:

```franz
// Decimal → Hex → Binary visualization
original = 171

step1 = (format-int original 16)
(println "Step 1 (Hex): " step1)  // "0xAB"

step2 = (format-int original 2)
(println "Step 2 (Binary): " step2)  // "0b10101011"

// Compare formats
showAllBases = {num ->
  (println "Decimal: " (format-int num 10))
  (println "Hex:     " (format-int num 16))
  (println "Binary:  " (format-int num 2))
  (println "Octal:   " (format-int num 8))
}

(showAllBases 255)
// Decimal: 255
// Hex:     0xFF
// Binary:  0b11111111
// Octal:   0o377
```

## Performance

### Memory Management

Both functions allocate strings on the heap:
- `formatInteger`: 70-byte buffer (handles 64-bit binary + sign + prefix)
- `formatFloat`: 50-byte buffer (handles any double with max precision)

**Memory responsibility**: The caller must manage the returned string (Franz's garbage collection handles this automatically).

### Time Complexity

- **format-int**: O(log_base(value)) - proportional to number of digits
  - Binary (base 2): Most digits, slowest
  - Hex (base 16): Fewest digits, fastest
  - Typical: < 1 microsecond for 64-bit integers

- **format-float**: O(1) - uses optimized snprintf
  - Fixed cost regardless of value
  - Typical: < 1 microsecond

### LLVM IR Generation

```llvm
; format-int compiled to LLVM IR
%1 = call i8* @formatInteger(i64 %value, i32 %base)

; format-float compiled to LLVM IR
%2 = call i8* @formatFloat(double %value, i32 %precision)

; Both functions are external C functions linked at compile time
declare i8* @formatInteger(i64, i32)
declare i8* @formatFloat(double, i32)
```


## Edge Cases

### format-int Edge Cases

**Zero**:
```franz
(format-int 0 2)   // "0" (no prefix for zero)
(format-int 0 8)   // "0"
(format-int 0 10)  // "0"
(format-int 0 16)  // "0"
```

**Negative numbers**:
```franz
(format-int -255 2)   // "-0b11111111"
(format-int -255 16)  // "-0xFF"
// Sign comes BEFORE prefix
```

**Large values**:
```franz
maxInt = 9223372036854775807  // 2^63 - 1
(format-int maxInt 16)
// "0x7FFFFFFFFFFFFFFF"

(format-int maxInt 2)
// "0b111111111111111111111111111111111111111111111111111111111111111"
// (63 ones)
```

**Invalid base** (runtime error):
```franz
(format-int 100 3)   // Error: base must be 2, 8, 10, or 16
(format-int 100 36)  // Error: base must be 2, 8, 10, or 16
```

### format-float Edge Cases

**Precision clamping**:
```franz
(format-float 3.14 -5)   // "3" (negative → 0)
(format-float 3.14 100)  // "3.14159000000000012" (clamped to 17)
```

**Very small numbers**:
```franz
tiny = 0.0000001
(format-float tiny 10)  // "0.0000001000"

zero = 0.000000000001
(format-float zero 5)   // "0.00000"
```

**Scientific notation rounding**:
```franz
large = 1e100
(format-float large 2)
// "99999999999999991611392.00" (precision loss for huge values)
```

**Negative zero**:
```franz
negZero = (subtract 0.0 0.0)
(format-float negZero 2)  // "0.00" or "-0.00" (platform dependent)
```

## Comparison with Other Languages

### Python
```python
# Python format functions
bin(255)         # '0b11111111'
oct(255)         # '0o377'
hex(255)         # '0xff'
format(3.14, '.2f')  # '3.14'

# Franz equivalent
(format-int 255 2)     # "0b11111111"
(format-int 255 8)     # "0o377"
(format-int 255 16)    # "0xFF"
(format-float 3.14 2)  # "3.14"
```

### JavaScript
```javascript
// JavaScript format functions
(255).toString(2)     // '11111111' (no prefix!)
(255).toString(8)     // '377' (no prefix!)
(255).toString(16)    // 'ff' (no prefix!)
(3.14).toFixed(2)     // '3.14'

// Franz is more consistent with prefixes
```

### Rust
```rust
// Rust formatting
format!("{:b}", 255)   // "11111111" (no prefix)
format!("{:o}", 255)   // "377" (no prefix)
format!("{:x}", 255)   // "ff" (lowercase)
format!("{:.2}", 3.14) // "3.14"

// Franz includes prefixes automatically
```

### C
```c
// C formatting (manual prefix)
char buf[100];
sprintf(buf, "0x%X", 255);    // "0xFF" (manual prefix)
sprintf(buf, "0b...", ...);   // No binary format in standard C!
sprintf(buf, "%.2f", 3.14);   // "3.14"

// Franz provides built-in binary formatting
```

## Related Documentation

- **Number Formats**: [docs/number-formats/number-formats.md](../number-formats/number-formats.md) - Multi-base number literals
- **Type Conversion**: [docs/type-conversion/type-conversion.md](../type-conversion/type-conversion.md) - Convert between types
- **LLVM Implementation**: [docs/native-compilation/LLVM_TEST_CHECKLIST.md](../native-compilation/LLVM_TEST_CHECKLIST.md) -  implementation details

## Troubleshooting

### Incorrect Base Error

**Problem**: `Runtime Error: format-int base must be 2, 8, 10, or 16`

**Solution**: Only use supported bases (2, 8, 10, 16).

```franz
// ❌ Wrong
(format-int 100 3)   // Base 3 not supported
(format-int 100 12)  // Base 12 not supported

// ✅ Correct
(format-int 100 2)   // Binary: "0b1100100"
(format-int 100 8)   // Octal: "0o144"
(format-int 100 10)  // Decimal: "100"
(format-int 100 16)  // Hex: "0x64"
```

### Type Mismatch

**Problem**: Passing wrong type to format functions

**Solution**: Ensure correct types:
- `format-int` expects integers (will convert floats)
- `format-float` expects floats (will convert integers)

```franz
// ✅ Automatic conversion in LLVM mode
(format-int 3.7 16)    // Converts float→int: "0x3"
(format-float 42 2)    // Converts int→float: "42.00"

// Types are handled automatically
```

### Precision Loss Warning

**Problem**: Large numbers lose precision when formatted as float

```franz
huge = 9223372036854775807  // Max int64
asFloat = (format-float huge 2)
// May lose precision: "9223372036854775808.00" (rounded)
```

**Solution**: Use `format-int` for large integers:
```franz
huge = 9223372036854775807
asHex = (format-int huge 16)
// Exact: "0x7FFFFFFFFFFFFFFF"
```

## Summary

Franz's string formatting functions provide:

✅ **format-int** - Convert integers to binary, octal, decimal, or hex strings
✅ **format-float** - Control decimal precision for floats
✅ **Automatic prefixes** - 0b, 0o, 0x added for binary, octal, hex
✅ **Type conversion** - Automatic int↔float conversion in LLVM mode
✅ **LLVM native compilation** - Compiled to C functions, linked at runtime
✅ **Comprehensive testing** - 44 tests covering all edge cases

These functions enable runtime number formatting, complementing the compile-time multi-base literals for complete number representation control.
