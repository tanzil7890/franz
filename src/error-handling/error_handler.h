#ifndef ERROR_HANDLER_V2_H
#define ERROR_HANDLER_V2_H

#include <stdbool.h>
#include "../generic.h"

/*
 * Franz Error Handling System V2 - Flag-based approach
 *
 * This system uses error flags instead of setjmp/longjmp to avoid
 * corrupting the evaluator's state. Errors are propagated up the
 * call stack explicitly.
 */

// Error types
typedef enum {
    ERROR_NONE = 0,
    ERROR_RUNTIME,
    ERROR_SYNTAX,
    ERROR_TYPE,
    ERROR_FILE,
    ERROR_IMPORT,
    ERROR_CIRCULAR,
    ERROR_MEMORY,
    ERROR_CUSTOM
} ErrorType;

// Global error state
typedef struct {
    bool has_error;              // Is there an active error?
    ErrorType error_type;        // Type of error
    char *error_message;         // Error message (malloc'd)
    int line_number;             // Line where error occurred
    int try_depth;               // Current try block nesting depth
    bool initialized;            // Is system initialized?
} ErrorState;

// Global error state instance
extern ErrorState g_error_state;

/**
 * Initialize error handling system
 */
void ErrorState_init(void);

/**
 * Cleanup error handling system
 */
void ErrorState_cleanup(void);

/**
 * Set an error (called by error() function or runtime errors)
 */
void ErrorState_setError(ErrorType type, int line_number, const char *format, ...);

/**
 * Check if there's an active error
 */
bool ErrorState_hasError(void);

/**
 * Get error message
 */
const char* ErrorState_getMessage(void);

/**
 * Get error type
 */
ErrorType ErrorState_getType(void);

/**
 * Get error line number
 */
int ErrorState_getLineNumber(void);

/**
 * Clear the current error
 */
void ErrorState_clearError(void);

/**
 * Enter a try block (increment try depth)
 */
void ErrorState_enterTry(void);

/**
 * Exit a try block (decrement try depth)
 */
void ErrorState_exitTry(void);

/**
 * Check if we're inside a try block
 */
bool ErrorState_inTryBlock(void);

/**
 * Convert error type to string
 */
const char* ErrorState_typeToString(ErrorType type);

#endif // ERROR_HANDLER_V2_H
