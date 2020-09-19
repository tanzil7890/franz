#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>

/**
 * Terminal Runtime Functions for LLVM
 *
 * These C functions are called from LLVM-generated code to query terminal dimensions.
 * They use the ioctl(TIOCGWINSZ) system call to get current terminal size.
 *
 * Fallback values:
 * - Default rows: 24 (standard terminal height)
 * - Default columns: 80 (standard terminal width)
 */

/**
 * Get terminal height (number of rows)
 * Called from LLVM-compiled Franz code
 *
 * @return Terminal height in lines (default: 24 if ioctl fails)
 */
long long franz_get_terminal_rows(void) {
  struct winsize w;

  // Try to get terminal size via ioctl
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) {
    // Fallback to standard terminal height
    return 24;
  }

  // Return actual terminal height
  return (long long)w.ws_row;
}

/**
 * Get terminal width (number of columns)
 * Called from LLVM-compiled Franz code
 *
 * @return Terminal width in characters (default: 80 if ioctl fails)
 */
long long franz_get_terminal_columns(void) {
  struct winsize w;

  // Try to get terminal size via ioctl
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) {
    // Fallback to standard terminal width
    return 80;
  }

  // Return actual terminal width
  return (long long)w.ws_col;
}
