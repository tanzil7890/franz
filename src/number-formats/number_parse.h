#ifndef NUMBER_PARSE_H
#define NUMBER_PARSE_H

#include <stdint.h>

//  Multi-base number parsing utilities
// Handles hexadecimal, binary, octal integers and hexadecimal floats

/**
 * Parse integer from string with multi-base support
 * Supports:
 *   - Decimal: "123", "-456"
 *   - Hexadecimal: "0x1A", "0XFF"
 *   - Binary: "0b1010", "0B11111111"
 *   - Octal: "0o17", "0O777"
 *
 * Returns: Parsed integer value
 */
int64_t parseInteger(const char *str);

/**
 * Parse float from string with multi-format support
 * Supports:
 *   - Decimal: "3.14", "-2.5"
 *   - Scientific: "1.5e2", "1E-3"
 *   - Hexadecimal: "0x1.5p2", "0X1.Ap-3"
 *
 * Returns: Parsed floating-point value
 */
double parseFloat(const char *str);

/**
 * Format integer to string with custom base
 * Base: 2 (binary), 8 (octal), 10 (decimal), 16 (hexadecimal)
 *
 * Returns: Newly allocated string (caller must free)
 */
char* formatInteger(int64_t value, int base);

/**
 * Format float to string with custom precision
 * Precision: Number of decimal places (0-17)
 *
 * Returns: Newly allocated string (caller must free)
 */
char* formatFloat(double value, int precision);

#endif // NUMBER_PARSE_H
