#!/bin/bash

echo "========================================"
echo "Franz vs Rust Loop Performance Benchmark"
echo "========================================"
echo ""

# Test 1: 10 iterations
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "TEST 1: 10 Iterations"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "--- Franz (LLVM Native Compilation) ---"
time ./franz test/loop-stress/stress-test-10.franz 2>&1 | grep -v DEBUG | grep -v 
echo ""
echo "--- Rust (Release Mode) ---"
rustc -O benchmarks/rust/stress_test_10.rs -o /tmp/rust_stress_10 2>/dev/null
time /tmp/rust_stress_10
echo ""

# Test 2: 100 iterations
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "TEST 2: 100 Iterations"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "--- Franz ---"
time ./franz test/loop-stress/stress-test-100.franz 2>&1 | grep -v DEBUG | grep -v 
echo ""
echo "--- Rust ---"
rustc -O benchmarks/rust/stress_test_100.rs -o /tmp/rust_stress_100 2>/dev/null
time /tmp/rust_stress_100
echo ""

# Test 3: 1,000 iterations
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "TEST 3: 1,000 Iterations"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "--- Franz ---"
time ./franz test/loop-stress/stress-test-1000.franz 2>&1 | grep -v DEBUG | grep -v 
echo ""
echo "--- Rust ---"
rustc -O benchmarks/rust/stress_test_1000.rs -o /tmp/rust_stress_1000 2>/dev/null
time /tmp/rust_stress_1000
echo ""

# Test 4: 10,000 iterations
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "TEST 4: 10,000 Iterations"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "--- Franz ---"
time ./franz test/loop-stress/stress-test-10000.franz 2>&1 | grep -v DEBUG | grep -v 
echo ""
echo "--- Rust ---"
rustc -O benchmarks/rust/stress_test_10000.rs -o /tmp/rust_stress_10000 2>/dev/null
time /tmp/rust_stress_10000
echo ""

# Test 5: 100,000 iterations
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "TEST 5: 100,000 Iterations"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "--- Franz ---"
time ./franz test/loop-stress/stress-test-100000.franz 2>&1 | grep -v DEBUG | grep -v 
echo ""
echo "--- Rust ---"
rustc -O benchmarks/rust/stress_test_100000.rs -o /tmp/rust_stress_100000 2>/dev/null
time /tmp/rust_stress_100000
echo ""

echo "========================================"
echo "Benchmark Complete!"
echo "========================================"
echo ""
echo "Summary:"
echo "- Franz uses LLVM native compilation (same backend as Rust)"
echo "- Both should have similar performance (C-equivalent speed)"
echo "- Franz compilation time includes parsing + LLVM IR generation"
echo "- Rust compilation was pre-done with 'rustc -O'"
