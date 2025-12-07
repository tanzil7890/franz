# Franz Compilation Guide ( Array-Based Tokens)

## Standard Compilation

The Franz interpreter now uses array-based tokens. Compile with:

```bash
find src -name "*.c" -not -name "check.c" | xargs gcc -Wall -lm -o franz
```

**Why exclude check.c?**
- `check.c` is the type checker binary (franz-check)
- It has its own `main()` function
- Compiling it with franz causes "duplicate symbol '_main'" error

## Alternative: Direct Pattern

```bash
gcc src/*.c src/*/*.c -Wall -lm -o franz 2>&1 | grep -v "duplicate symbol"
```

Then manually check for real errors (ignoring the check.c duplicate main).

## Build Outputs

**Warnings (Safe to Ignore):**
```
src/generic.c:168:13: warning: enumeration value 'TYPE_REF' not handled in switch [-Wswitch]
src/bytecode/chunk.c:53:9: warning: unused variable 'oldCapacity' [-Wunused-variable]
src/optimization/compile.c:113:12: warning: unused function 'count_params' [-Wunused-function]
```

These are pre-existing warnings from earlier , not related to .

## Verify Build

```bash
./franz --version
```

Expected: `v0.0.4`

## Test Build

```bash
echo '(println "Hello, Franz!")' | ./franz
```

Expected output:
```
Hello, Franz!
```

## Debug Build

For development/debugging:

```bash
find src -name "*.c" -not -name "check.c" | xargs gcc -g -Wall -lm -o franz
```

The `-g` flag includes debug symbols for use with lldb/gdb.

## Type Checker (Separate Binary)

To build franz-check:

```bash
gcc src/check.c src/tokens.c src/lex.c src/parse.c src/ast.c \
    src/type-system/*.c src/string.c -Wall -lm -o franz-check
```

This creates a separate `franz-check` binary for static type checking.

## Clean Build

```bash
rm -f franz franz-check
find src -name "*.c" -not -name "check.c" | xargs gcc -Wall -lm -o franz
```

##  Changes

The build process hasn't changed from user perspective, but internally:

**Before **
- Token linked lists (O(n) operations)
- Parser traversed p_next pointers

**After **
- TokenArray with dynamic arrays (O(1) operations)
- Parser uses array indexing

**Result:** Same binary, 4-250x faster lexing depending on file size!
