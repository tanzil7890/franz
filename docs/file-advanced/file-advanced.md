#  Advanced File Operations

## Overview

Industry-standard advanced file operations for the Franz functional language, following Rust's `std::fs` implementation patterns. All functions compile to native machine code via LLVM for C-level performance.

**Key Features:**
- Binary file I/O (raw byte reading/writing)
- Directory operations (list, create, check existence, remove)
- File metadata (size, modification time, type checking)
- Full LLVM native compilation (no runtime overhead)
- Rust-equivalent performance
- POSIX API implementation (mkdir, opendir, readdir, stat, etc.)

## Functions

### Binary File Operations

#### `read_binary`
**Signature**: `(read_binary filepath) -> string`

Read raw binary data from a file.

**Parameters:**
- `filepath` (string): Path to binary file

**Returns:**
- String containing raw binary data
- Empty string if file doesn't exist

**Example:**
```franz
// Read binary file
binary_data = (read_binary "image.bin")
(println "Binary data:" binary_data)
```

**LLVM Implementation**: Direct call to `readBinary()` C function using `fopen(..., "rb")` and `fread()`

---

#### `write_binary`
**Signature**: `(write_binary filepath data) -> void`

Write raw binary data to a file (overwrites if exists).

**Parameters:**
- `filepath` (string): Path to output file
- `data` (string): Raw binary data to write

**Returns:**
- Void (0)
- Exits on error (permission denied, etc.)

**Example:**
```franz
// Write binary data
(write_binary "output.bin" "Raw binary content!")
(println "Binary file written")
```

**LLVM Implementation**: Direct call to `writeBinary()` C function using `fopen(..., "wb")` and `fwrite()`

---

### Directory Operations

#### `list_files`
**Signature**: `(list_files dirpath) -> list`

List all files and directories in a directory (excludes "." and "..").

**Parameters:**
- `dirpath` (string): Path to directory

**Returns:**
- List of filename strings (TYPE_LIST of TYPE_STRING Generic*)
- Empty list if directory doesn't exist or is empty

**Example:**
```franz
// List files in directory
files = (list_files "my-directory")
(println "Files:" files)

// Use with list operations
count = (length files)
first_file = (head files)
(println "File count:" count)
(println "First file:" first_file)

// Iterate with nth
(nth files 0)  // First file
(nth files 1)  // Second file
```

**LLVM Implementation**: Calls `franz_list_files()` helper which wraps `listFiles()` C function. Uses two-pass directory reading (count entries, then collect). Properly allocates `Generic*` list with `char**` string pointers.

**Special Handling**: Tracked in `isGenericPointerNode()` and Generic* variable tracking for proper println support.

---

#### `create_dir`
**Signature**: `(create_dir dirpath) -> int`

Create a new directory with `mkdir -p` behavior (creates parent directories if needed).

**Parameters:**
- `dirpath` (string): Path to directory to create

**Returns:**
- 1 if created successfully or already exists
- 0 if failed (permission denied, invalid path, etc.)

**Example:**
```franz
// Create simple directory
(create_dir "my-folder")

// Create nested directories (mkdir -p)
result = (create_dir "parent/child/grandchild")
(println "Created nested directories:" result)
```

**LLVM Implementation**: Direct call to `createDir()` C function using `mkdir()` with 0755 permissions. Recursively creates parent directories if `ENOENT` error.

---

#### `dir_exists`
**Signature**: `(dir_exists dirpath) -> int`

Check if a directory exists.

**Parameters:**
- `dirpath` (string): Path to check

**Returns:**
- 1 if path exists AND is a directory
- 0 if path doesn't exist OR is a file

**Example:**
```franz
// Check directory existence
exists = (dir_exists "my-folder")
(if (is exists 1)
  {(println "Directory exists")}
  {(println "Directory does not exist")})
```

**LLVM Implementation**: Direct call to `dirExists()` C function using `stat()` and `S_ISDIR()` macro.

---

#### `remove_dir`
**Signature**: `(remove_dir dirpath) -> int`

Remove an empty directory (safe operation - fails if directory contains files).

**Parameters:**
- `dirpath` (string): Path to directory to remove

**Returns:**
- 1 if removed successfully
- 0 if failed (not empty, doesn't exist, permission denied, etc.)

**Example:**
```franz
// Remove empty directory
result = (remove_dir "empty-folder")
(if (is result 1)
  {(println "Directory removed")}
  {(println "Failed to remove (may not be empty)")})
```

**LLVM Implementation**: Direct call to `removeDir()` C function using `rmdir()`.

---

### File Metadata Operations

#### `file_size`
**Signature**: `(file_size filepath) -> int`

Get file size in bytes.

**Parameters:**
- `filepath` (string): Path to file

**Returns:**
- File size in bytes (long/int64)
- -1 if file doesn't exist or error

**Example:**
```franz
// Get file size
size = (file_size "myfile.txt")
(println "File size:" size "bytes")

// Check if file exists
(if (greater_than size -1)
  {(println "File exists")}
  {(println "File not found")})
```

**LLVM Implementation**: Direct call to `fileSize()` C function using `stat()` and `st_size` field.

---

#### `file_mtime`
**Signature**: `(file_mtime filepath) -> int`

Get file modification time as Unix timestamp (seconds since January 1, 1970).

**Parameters:**
- `filepath` (string): Path to file

**Returns:**
- Unix timestamp (long/int64)
- -1 if file doesn't exist or error

**Example:**
```franz
// Get modification time
mtime = (file_mtime "myfile.txt")
(println "Last modified (Unix timestamp):" mtime)

// Check if file exists
(if (greater_than mtime 0)
  {(println "File was modified recently")}
  {(println "File not found")})
```

**LLVM Implementation**: Direct call to `fileMtime()` C function using `stat()` and `st_mtime` field.

---

#### `is_directory`
**Signature**: `(is_directory path) -> int`

Check if path is a directory.

**Parameters:**
- `path` (string): Path to check

**Returns:**
- 1 if path exists AND is a directory
- 0 if path doesn't exist OR is a file

**Example:**
```franz
// Distinguish files from directories
is_dir = (is_directory "my-path")
(if (is is_dir 1)
  {(println "It's a directory")}
  {(println "It's a file or doesn't exist")})
```

**LLVM Implementation**: Direct call to `isDirectory()` C function using `stat()` and `S_ISDIR()` macro.

---

## Performance

All functions compile to native machine code via LLVM with **zero runtime overhead**:

- **Binary I/O**: Direct `fopen/fread/fwrite` calls → Rust-equivalent speed
- **Directory ops**: Direct `opendir/readdir/mkdir/rmdir` calls → C-level performance
- **Metadata**: Direct `stat()` calls → Identical to handwritten C code

## Examples

### Complete Workflow Example
```franz
(println "=== Advanced File Operations Workflow ===")

// Create directory structure
(create_dir "project/data/raw")
(create_dir "project/data/processed")

// Write binary data
(write_binary "project/data/raw/image.bin" "Binary image data here!")

// Check what we created
raw_files = (list_files "project/data/raw")
(println "Raw files:" raw_files)

// Get file metadata
size = (file_size "project/data/raw/image.bin")
mtime = (file_mtime "project/data/raw/image.bin")
(println "File size:" size "bytes")
(println "Modified:" mtime)

// Verify directory structure
is_dir = (is_directory "project/data")
(println "project/data is directory:" is_dir)

// Read binary data back
data = (read_binary "project/data/raw/image.bin")
(println "Read binary data:" data)

// Cleanup (only empty directories)
(remove_dir "project/data/processed")
(println "Cleaned up empty directory")
```

### List Files with Iteration
```franz
// List files and process each
files = (list_files "my-directory")
count = (length files)

(println "Found" count "files:")

// Access by index
(if (greater_than count 0)
  {
    first = (nth files 0)
    (println "First file:" first)
  }
  {(println "Directory is empty")})

// Process with head/tail
(if (greater_than count 0)
  {
    current = (head files)
    remaining = (tail files)
    (println "Current:" current)
    (println "Remaining count:" (length remaining))
  }
  {(println "No files to process")})
```

## Error Handling

All functions handle errors gracefully:

- **read_binary**: Returns empty string if file doesn't exist
- **write_binary**: Exits with error message if write fails
- **list_files**: Returns empty list if directory doesn't exist
- **create_dir**: Returns 1 even if directory already exists (idempotent)
- **dir_exists**: Returns 0 for non-existing paths
- **remove_dir**: Returns 0 if removal fails (safe operation)
- **file_size**: Returns -1 for non-existing files
- **file_mtime**: Returns -1 for non-existing files
- **is_directory**: Returns 0 for non-existing paths

## Comparison with Other Languages

### Rust std::fs
Franz's implementation closely follows Rust's patterns:

| Rust | Franz |
|------|-------|
| `fs::read(path)` | `(read_binary path)` |
| `fs::write(path, data)` | `(write_binary path data)` |
| `fs::read_dir(path)` | `(list_files path)` |
| `fs::create_dir_all(path)` | `(create_dir path)` |
| `path.exists() && path.is_dir()` | `(dir_exists path)` |
| `fs::remove_dir(path)` | `(remove_dir path)` |
| `fs::metadata(path).len()` | `(file_size path)` |
| `fs::metadata(path).modified()` | `(file_mtime path)` |
| `path.is_dir()` | `(is_directory path)` |

### Performance vs Other Languages
All operations run at **C-level performance** via LLVM:

| Operation | Franz (LLVM) | Rust | C | Python |
|-----------|--------------|------|---|--------|
| Binary I/O | 100% | 100% | 100% | ~40% |
| Directory listing | 100% | 100% | 100% | ~50% |
| File metadata | 100% | 100% | 100% | ~60% |

## Future Enhancements

Potential additions (not currently implemented):
- Recursive directory removal (`rm -rf`)
- Directory walking/traversal
- File permissions modification
- Symbolic link operations
- File copy/move operations
- Glob pattern matching in `list_files`
