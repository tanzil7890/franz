# Closure Test Suite

This directory contains comprehensive tests for Franz's snapshot-based closure implementation ().

## Test Files

### Core Functionality Tests

#### [snapshot-test.franz](snapshot-test.franz)
**Purpose**: Comprehensive test suite for snapshot-based closures
**Tests**: 6/6 passing
**Coverage**:
- Test 1: Simple closure with no free variables
- Test 2: Closure with one free variable
- Test 3: Closure with multiple free variables
- Test 4: Closure with higher-order functions (map)
- Test 5: Multiple closures sharing a variable
- Test 6: Closure called multiple times (reuse)

**Memory Leaks**: ✅ 0 leaks for 0 bytes

```bash
./franz test/closure/snapshot-test.franz
leaks -atExit -- ./franz test/closure/snapshot-test.franz
```

#### [closure-creation-test.franz](closure-creation-test.franz)
**Purpose**: Original  closure creation tests
**Tests**: 8/8 passing
**Coverage**:
- Simple closures
- Nested closures
- Multiple free variables
- Closures with map/reduce
- Function returns

**Memory Leaks**: ✅ 0 leaks for 0 bytes

```bash
./franz test/closure/closure-creation-test.franz
```

### Edge Case Tests

#### [minimal-test.franz](minimal-test.franz)
**Purpose**: Minimal test - single closure in global scope
**Tests**: Simple closure with one free variable (builtin `add`)

```bash
./franz test/closure/minimal-test.franz
leaks -atExit -- ./franz test/closure/minimal-test.franz
# Expected: 0 leaks ✅
```

#### [simple-local-test.franz](simple-local-test.franz)
**Purpose**: Test closure creation in local function scope
**Tests**: Closure created inside a function that captures outer variable

```bash
./franz test/closure/simple-local-test.franz
leaks -atExit -- ./franz test/closure/simple-local-test.franz
# Expected: 0 leaks ✅
```

#### [debug-nested-scope.franz](debug-nested-scope.franz)
**Purpose**: Test nested closure returning from function
**Tests**: Function returns a closure that captures its local variable

```bash
./franz test/closure/debug-nested-scope.franz
leaks -atExit -- ./franz test/closure/debug-nested-scope.franz
# Expected: 0 leaks ✅
```

#### [debug-freevar.franz](debug-freevar.franz)
**Purpose**: Debug test for free variable analysis
**Tests**: Basic free variable capture from global scope

```bash
./franz test/closure/debug-freevar.franz
```

#### [local-scope-test.franz](local-scope-test.franz)
**Purpose**: Test closures in local scope wrapper function
**Tests**: Multiple closures created in wrapper function local scope

```bash
./franz test/closure/local-scope-test.franz
```

## Running All Tests

### Functionality Test
```bash
for test in test/closure/*.franz; do
  echo "Testing: $(basename $test)"
  ./franz "$test" > /tmp/test_output.txt 2>&1
  if [ $? -eq 0 ]; then
    echo "  ✓ PASS"
  else
    echo "  ✗ FAIL"
    tail -3 /tmp/test_output.txt
  fi
done
```

### Memory Leak Test
```bash
echo "=== MEMORY LEAK TEST RESULTS ==="
for test in test/closure/snapshot-test.franz \
            test/closure/minimal-test.franz \
            test/closure/debug-nested-scope.franz \
            test/closure/simple-local-test.franz; do
  echo ""
  echo "Test: $(basename $test)"
  leaks -atExit -- ./franz "$test" 2>&1 | grep "Process.*leaks" | tail -1
done
```

**Expected Result**: All tests should show **0 leaks for 0 total leaked bytes** ✅

## Test Coverage

### Feature Coverage
- ✅ Simple closures (no free variables)
- ✅ Closures with free variables
- ✅ Multiple free variables
- ✅ Nested closures
- ✅ Closures returned from functions
- ✅ Higher-order functions (map, reduce)
- ✅ Multiple closures sharing variables
- ✅ Closure reuse (multiple calls)
- ✅ Local scope closures
- ✅ Global scope closures

### Memory Management Coverage
- ✅ Zero leaks in all tests
- ✅ Proper refCount management
- ✅ Dict ownership semantics
- ✅ Scope cleanup
- ✅ Closure cleanup
- ✅ Snapshot cleanup

## Test Patterns

### Pattern 1: Global Scope Closure
```franz
global_var = 42
my_closure = {param -> <- (add param global_var)}
result = (my_closure 8)
(println result)  // 50
```

### Pattern 2: Local Scope Closure
```franz
wrapper = {->
  local_var = 5
  my_closure = {param -> <- (add param local_var)}
  <- my_closure
}
closure = (wrapper)
result = (closure 3)
(println result)  // 8
```

### Pattern 3: Nested Closure Return
```franz
make_adder = {x ->
  <- {y -> <- (add x y)}
}
add5 = (make_adder 5)
result = (add5 3)
(println result)  // 8
```

### Pattern 4: Closure with Map
```franz
nums = (range 5)
multiplier = 3
triple = {item idx -> <- (multiply item multiplier)}
result = (map nums triple)
(println result)  // (0 3 6 9 12)
```

### Pattern 5: Multiple Closures Sharing Variable
```franz
shared = 10
closure1 = {x -> <- (add x shared)}
closure2 = {y -> <- (multiply y shared)}
r1 = (closure1 5)   // 15
r2 = (closure2 5)   // 50
```

## Memory Leak Resolution History

###  Scope-Based Closures
- **Leak Count**: 584 leaks
- **Cause**: Circular references (Scope → Closure → Scope)
- **Status**: Fixed by 

###  Initial: Snapshot-Based Closures
- **Leak Count**: 147 leaks
- **Cause**: Dict not owning keys/values properly
- **Status**: Fixed by Dict ownership updates

###  Final: Production Implementation
- **Leak Count**: **0 leaks** ✅
- **Fixes Applied**:
  1. Dict_set_inplace increments refCounts
  2. Dict_free decrements refCounts
  3. Temporary key cleanup in Closure_new
  4. Scope_get returns NULL for lineNumber=-1
  5. Removed manual refCount++ in snapshot restoration

## Known Limitations

### Test 9: Nested Closures with Local Assignments
The original `closure-creation-test.franz` Test 9 (nested closures where inner function is assigned locally) previously failed because free variable analysis didn't capture locally-assigned functions. This was resolved by the `Scope_get` fix that returns NULL for undefined variables during snapshot creation.

**Status**: ✅ Now working with 0 leaks

## Debugging Failed Tests

If a test fails:

1. **Run with output**:
   ```bash
   ./franz test/closure/failing-test.franz
   ```

2. **Check for memory leaks**:
   ```bash
   leaks -atExit -- ./franz test/closure/failing-test.franz
   ```

3. **Enable debug output** (if debug flags were compiled in):
   ```bash
   gcc src/*.c src/*/*.c -DDEBUG_SCOPE_FREE -DDEBUG_GENERIC_FREE \
       -DDEBUG_CLOSURE_FREE -Wall -lm -o franz
   ./franz test/closure/failing-test.franz 2>&1 | less
   ```

4. **Use debug mode** (Franz's built-in debug):
   ```bash
   ./franz -d test/closure/failing-test.franz
   ```

## Contributing New Tests

When adding new closure tests:

1. **Name Pattern**: `<feature>-test.franz` (e.g., `recursive-closure-test.franz`)
2. **Test Structure**:
   ```franz
   (println "=== Test Name ===")
   (println "")

   (println "Test 1: Description")
   // test code
   (println "✓ PASS: Test 1")

   (println "")
   (println "All tests passed!")
   ```

3. **Verify Zero Leaks**:
   ```bash
   leaks -atExit -- ./franz test/closure/your-test.franz
   ```

4. **Update this README**: Add your test to the appropriate section

## References

- **Implementation**: [docs/lexical-scoping/.6_COMPLETE.md](../../docs/lexical-scoping/.6_COMPLETE.md)
- **Status**: [docs/lexical-scoping/.5_2.6_STATUS.md](../../docs/lexical-scoping/.5_2.6_STATUS.md)
- **Memory Strategy**: [docs/lexical-scoping/MEMORY_MANAGEMENT_STRATEGY.md](../../docs/lexical-scoping/MEMORY_MANAGEMENT_STRATEGY.md)
