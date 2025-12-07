#include "number_parse.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

//  Multi-base integer parsing
int64_t parseInteger(const char *str) {
  if (!str) return 0;

  // Check for negative sign
  int negative = 0;
  const char *p = str;
  if (*p == '-') {
    negative = 1;
    p++;
  }

  // Check for base prefixes
  if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
    // Hexadecimal: 0x1A
    return strtoll(str, NULL, 16);
  } else if (p[0] == '0' && (p[1] == 'b' || p[1] == 'B')) {
    // Binary: 0b1010
    p += 2;  // Skip '0b'
    int64_t result = 0;
    while (*p == '0' || *p == '1') {
      result = result * 2 + (*p - '0');
      p++;
    }
    return negative ? -result : result;
  } else if (p[0] == '0' && (p[1] == 'o' || p[1] == 'O')) {
    // Octal: 0o17
    return strtoll(str + (negative ? 1 : 0) + 2, NULL, 8) * (negative ? -1 : 1);
  } else {
    // Decimal: 123
    return strtoll(str, NULL, 10);
  }
}

//  Multi-format float parsing
double parseFloat(const char *str) {
  if (!str) return 0.0;

  // Check for hexadecimal float: 0x1.5p2
  if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
    // Use C99 strtod which supports hex floats
    return strtod(str, NULL);
  } else {
    // Regular decimal or scientific notation
    return atof(str);
  }
}

//  Format integer to custom base
char* formatInteger(int64_t value, int base) {
  if (base < 2 || base > 36) {
    base = 10;  // Default to decimal for invalid base
  }

  // Allocate buffer (64 bits = max 64 binary digits + sign + prefix + null)
  char *buffer = malloc(70);
  if (!buffer) return NULL;

  int negative = value < 0;
  uint64_t absValue = negative ? -value : value;

  char *digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  char temp[70];
  int pos = 0;

  // Handle zero special case
  if (absValue == 0) {
    buffer[0] = '0';
    buffer[1] = '\0';
    return buffer;
  }

  // Convert to base
  while (absValue > 0) {
    temp[pos++] = digits[absValue % base];
    absValue /= base;
  }

  // Build result string with prefix
  int bufPos = 0;

  if (negative) {
    buffer[bufPos++] = '-';
  }

  // Add prefix for non-decimal bases
  if (base == 16) {
    buffer[bufPos++] = '0';
    buffer[bufPos++] = 'x';
  } else if (base == 2) {
    buffer[bufPos++] = '0';
    buffer[bufPos++] = 'b';
  } else if (base == 8) {
    buffer[bufPos++] = '0';
    buffer[bufPos++] = 'o';
  }

  // Reverse the digits
  for (int i = pos - 1; i >= 0; i--) {
    buffer[bufPos++] = temp[i];
  }

  buffer[bufPos] = '\0';
  return buffer;
}

//  Format float with custom precision
char* formatFloat(double value, int precision) {
  // Clamp precision to valid range
  if (precision < 0) precision = 0;
  if (precision > 17) precision = 17;  // Max precision for double

  // Allocate buffer (large enough for any double with precision)
  char *buffer = malloc(50);
  if (!buffer) return NULL;

  // Build format string
  char format[10];
  snprintf(format, sizeof(format), "%%.%df", precision);

  // Format the float
  snprintf(buffer, 50, format, value);

  return buffer;
}
