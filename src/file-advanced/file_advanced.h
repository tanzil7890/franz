#ifndef FILE_ADVANCED_H
#define FILE_ADVANCED_H

#include <stddef.h>
#include "../list.h"

/**
 *  Advanced File Operations
 *
 * This module provides industry-standard file operations:
 * - Binary file I/O (raw byte reading/writing)
 * - Directory operations (list, create, check existence)
 * - File metadata (size, modification time, type checking)
 *
 * All functions follow Franz conventions:
 * - String paths for file/directory names
 * - Integer returns for success/failure (1/0)
 * - Error messages with line numbers
 */

// ============================================================================
// Binary File Operations
// ============================================================================

/**
 * Read raw binary data from file
 * @param path File path to read
 * @param outSize Pointer to store size of data read (in bytes)
 * @return char* - raw binary data, or NULL on failure
 * Note: Caller must free returned pointer
 */
char *readBinary(char *path, size_t *outSize);

/**
 * Write raw binary data to file
 * @param path File path to write
 * @param data Raw binary data to write
 * @param size Size of data in bytes
 * @param lineNumber Line number for error reporting
 * @return void
 * Note: Will exit on error (permission denied, etc.)
 */
void writeBinary(char *path, char *data, size_t size, int lineNumber);

// ============================================================================
// Directory Operations
// ============================================================================

/**
 * List all files/directories in a directory
 * @param dirpath Directory path to list
 * @param lineNumber Line number for error reporting
 * @return List* - List of filename strings (TYPE_STRING Generic*)
 * Note: Returns empty list if directory is empty or doesn't exist
 */
List *listFiles(char *dirpath, int lineNumber);

/**
 * Create a new directory
 * @param dirpath Directory path to create
 * @param lineNumber Line number for error reporting
 * @return int - 1 if created successfully, 0 if failed
 * Note: Creates parent directories if needed (mkdir -p behavior)
 */
int createDir(char *dirpath, int lineNumber);

/**
 * Check if directory exists
 * @param dirpath Directory path to check
 * @return int - 1 if exists and is a directory, 0 otherwise
 */
int dirExists(char *dirpath);

/**
 * Remove an empty directory
 * @param dirpath Directory path to remove
 * @param lineNumber Line number for error reporting
 * @return int - 1 if removed successfully, 0 if failed
 * Note: Only removes empty directories (safe operation)
 */
int removeDir(char *dirpath, int lineNumber);

// ============================================================================
// File Metadata Operations
// ============================================================================

/**
 * Get file size in bytes
 * @param path File path to check
 * @return long - File size in bytes, or -1 on error
 */
long fileSize(char *path);

/**
 * Get file modification time
 * @param path File path to check
 * @return long - Unix timestamp (seconds since 1970), or -1 on error
 */
long fileMtime(char *path);

/**
 * Check if path is a directory
 * @param path Path to check
 * @return int - 1 if path is a directory, 0 otherwise
 */
int isDirectory(char *path);

#endif // FILE_ADVANCED_H
