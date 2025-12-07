#include "file_advanced.h"
#include "../generic.h"
#include "../list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>

/**
 *  Advanced File Operations Implementation
 *
 * Industry-standard file operations using POSIX APIs:
 * - fopen/fread/fwrite for binary I/O
 * - opendir/readdir for directory listings
 * - mkdir for directory creation
 * - stat for file metadata
 */

// ============================================================================
// Binary File Operations
// ============================================================================

char *readBinary(char *path, size_t *outSize) {
  FILE *p_file = fopen(path, "rb");  // "rb" = read binary mode

  if (p_file == NULL) {
    *outSize = 0;
    return NULL;  // File doesn't exist or can't be read
  }

  // Get file size
  fseek(p_file, 0, SEEK_END);
  long fileSize = ftell(p_file);
  rewind(p_file);

  if (fileSize < 0) {
    fclose(p_file);
    *outSize = 0;
    return NULL;
  }

  // Allocate buffer for binary data
  char *data = malloc(fileSize);
  if (data == NULL) {
    fclose(p_file);
    *outSize = 0;
    return NULL;
  }

  // Read binary data
  size_t bytesRead = fread(data, 1, fileSize, p_file);
  fclose(p_file);

  *outSize = bytesRead;
  return data;
}

void writeBinary(char *path, char *data, size_t size, int lineNumber) {
  FILE *p_file = fopen(path, "wb");  // "wb" = write binary mode

  if (p_file == NULL) {
    printf(
      "Runtime Error @ Line %i: Cannot write binary file \"%s\".\n",
      lineNumber, path
    );
    exit(0);
  }

  // Write binary data
  size_t bytesWritten = fwrite(data, 1, size, p_file);
  fclose(p_file);

  if (bytesWritten != size) {
    printf(
      "Runtime Error @ Line %i: Failed to write complete binary data to \"%s\".\n",
      lineNumber, path
    );
    exit(0);
  }
}

// ============================================================================
// Directory Operations
// ============================================================================

List *listFiles(char *dirpath, int lineNumber) {
  DIR *dir = opendir(dirpath);
  if (dir == NULL) {
    // Return empty list if directory doesn't exist or can't be read
    Generic **emptyArray = NULL;
    return List_new(emptyArray, 0);
  }

  // First pass: count entries
  int count = 0;
  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    // Skip "." and ".." entries
    if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
      count++;
    }
  }

  // Rewind directory to beginning
  rewinddir(dir);

  // Allocate array for list items
  Generic **items = malloc(count * sizeof(Generic *));
  int index = 0;

  // Second pass: collect filenames
  while ((entry = readdir(dir)) != NULL) {
    // Skip "." and ".." entries
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    // Create string Generic for filename
    // Allocate string on heap
    char *filename = malloc(strlen(entry->d_name) + 1);
    strcpy(filename, entry->d_name);

    // Allocate char** on heap (Generic stores char** for strings, not char*)
    char **filenamePtr = malloc(sizeof(char *));
    *filenamePtr = filename;

    Generic *filenameGeneric = Generic_new(TYPE_STRING, filenamePtr, 0);

    items[index++] = filenameGeneric;
  }

  closedir(dir);

  // Create list from array
  List *fileList = List_new(items, count);

  // Free temporary array (not the Generic* objects, they're now owned by the list)
  free(items);

  return fileList;
}

int createDir(char *dirpath, int lineNumber) {
  // Try to create directory with rwxr-xr-x permissions (0755)
  #ifdef _WIN32
    int result = _mkdir(dirpath);
  #else
    int result = mkdir(dirpath, 0755);
  #endif

  if (result == 0) {
    return 1;  // Success
  }

  // Check if directory already exists
  if (errno == EEXIST) {
    return 1;  // Already exists, treat as success
  }

  // Check if parent directory doesn't exist
  if (errno == ENOENT) {
    // Try to create parent directories recursively (mkdir -p behavior)
    char *pathCopy = malloc(strlen(dirpath) + 1);
    strcpy(pathCopy, dirpath);

    // Find last '/' to get parent directory
    char *lastSlash = strrchr(pathCopy, '/');
    if (lastSlash != NULL && lastSlash != pathCopy) {
      *lastSlash = '\0';  // Terminate at parent directory

      // Recursively create parent
      if (createDir(pathCopy, lineNumber) == 1) {
        free(pathCopy);
        // Try creating the directory again
        #ifdef _WIN32
          result = _mkdir(dirpath);
        #else
          result = mkdir(dirpath, 0755);
        #endif
        return (result == 0 || errno == EEXIST) ? 1 : 0;
      }
    }
    free(pathCopy);
  }

  return 0;  // Failed
}

int dirExists(char *dirpath) {
  struct stat statbuf;

  if (stat(dirpath, &statbuf) != 0) {
    return 0;  // Path doesn't exist
  }

  // Check if it's a directory
  return S_ISDIR(statbuf.st_mode) ? 1 : 0;
}

int removeDir(char *dirpath, int lineNumber) {
  int result = rmdir(dirpath);

  if (result == 0) {
    return 1;  // Success
  }

  // Failed (directory not empty, doesn't exist, or permission denied)
  return 0;
}

// ============================================================================
// File Metadata Operations
// ============================================================================

long fileSize(char *path) {
  struct stat statbuf;

  if (stat(path, &statbuf) != 0) {
    return -1;  // Error (file doesn't exist or can't stat)
  }

  // Return file size in bytes
  return (long)statbuf.st_size;
}

long fileMtime(char *path) {
  struct stat statbuf;

  if (stat(path, &statbuf) != 0) {
    return -1;  // Error (file doesn't exist or can't stat)
  }

  // Return modification time as Unix timestamp
  return (long)statbuf.st_mtime;
}

int isDirectory(char *path) {
  struct stat statbuf;

  if (stat(path, &statbuf) != 0) {
    return 0;  // Path doesn't exist
  }

  // Check if it's a directory
  return S_ISDIR(statbuf.st_mode) ? 1 : 0;
}
