#include "error_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// Global error state instance
ErrorState g_error_state = {
    .has_error = false,
    .error_type = ERROR_NONE,
    .error_message = NULL,
    .line_number = 0,
    .try_depth = 0,
    .initialized = false
};

void ErrorState_init(void) {
    if (g_error_state.initialized) {
        return;  // Already initialized
    }

    g_error_state.has_error = false;
    g_error_state.error_type = ERROR_NONE;
    g_error_state.error_message = NULL;
    g_error_state.line_number = 0;
    g_error_state.try_depth = 0;
    g_error_state.initialized = true;
}

void ErrorState_cleanup(void) {
    if (!g_error_state.initialized) {
        return;
    }

    if (g_error_state.error_message != NULL) {
        free(g_error_state.error_message);
        g_error_state.error_message = NULL;
    }

    g_error_state.initialized = false;
    g_error_state.has_error = false;
    g_error_state.try_depth = 0;
}

void ErrorState_setError(ErrorType type, int line_number, const char *format, ...) {
    // Free previous error message if any
    if (g_error_state.error_message != NULL) {
        free(g_error_state.error_message);
        g_error_state.error_message = NULL;
    }

    // Format error message
    char error_buffer[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(error_buffer, sizeof(error_buffer), format, args);
    va_end(args);

    // Set error state
    g_error_state.has_error = true;
    g_error_state.error_type = type;
    g_error_state.error_message = strdup(error_buffer);
    g_error_state.line_number = line_number;

    // If NOT inside a try block, print error and exit
    if (g_error_state.try_depth == 0) {
        const char *error_type_str = ErrorState_typeToString(type);

        if (line_number > 0) {
            fprintf(stderr, "%s @ Line %d: %s\n", error_type_str, line_number, error_buffer);
        } else {
            fprintf(stderr, "%s: %s\n", error_type_str, error_buffer);
        }

        exit(1);
    }
    // Otherwise, error will be caught by try block
}

bool ErrorState_hasError(void) {
    return g_error_state.has_error;
}

const char* ErrorState_getMessage(void) {
    return g_error_state.error_message ? g_error_state.error_message : "Unknown error";
}

ErrorType ErrorState_getType(void) {
    return g_error_state.error_type;
}

int ErrorState_getLineNumber(void) {
    return g_error_state.line_number;
}

void ErrorState_clearError(void) {
    if (g_error_state.error_message != NULL) {
        free(g_error_state.error_message);
        g_error_state.error_message = NULL;
    }

    g_error_state.has_error = false;
    g_error_state.error_type = ERROR_NONE;
    g_error_state.line_number = 0;
}

void ErrorState_enterTry(void) {
    g_error_state.try_depth++;
}

void ErrorState_exitTry(void) {
    if (g_error_state.try_depth > 0) {
        g_error_state.try_depth--;
    }
}

bool ErrorState_inTryBlock(void) {
    return g_error_state.try_depth > 0;
}

const char* ErrorState_typeToString(ErrorType type) {
    switch (type) {
        case ERROR_NONE:      return "No Error";
        case ERROR_RUNTIME:   return "Runtime Error";
        case ERROR_SYNTAX:    return "Syntax Error";
        case ERROR_TYPE:      return "Type Error";
        case ERROR_FILE:      return "File Error";
        case ERROR_IMPORT:    return "Import Error";
        case ERROR_CIRCULAR:  return "Circular Dependency Error";
        case ERROR_MEMORY:    return "Memory Error";
        case ERROR_CUSTOM:    return "Error";
        default:              return "Unknown Error";
    }
}
