#include <string.h>
#include <stdlib.h>

/**
 * LLVM Runtime Function: repeat string
 *
 * Repeats a string N times
 * Called from LLVM-generated code
 *
 * @param str The string to repeat
 * @param count Number of times to repeat
 * @return Newly allocated string with repetitions
 */
char *franz_repeat_string(const char *str, long long count) {
  // Edge case: count <= 0 returns empty string
  if (count <= 0) {
    char *empty = (char *)malloc(1);
    empty[0] = '\0';
    return empty;
  }

  // Calculate total size needed
  int strLen = strlen(str);
  int totalSize = (strLen * count) + 1;  // +1 for \0

  // Allocate memory
  char *result = (char *)malloc(totalSize);
  result[0] = '\0';

  // Repeat string count times
  for (long long i = 0; i < count; i++) {
    strcat(result, str);
  }

  return result;
}
