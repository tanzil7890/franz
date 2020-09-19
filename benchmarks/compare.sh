#!/bin/bash

echo "========================================"
echo "  Franz vs Rust Loop Stress Tests"
echo "========================================"
echo ""

# Test 1000 iterations
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Test: 1,000 iterations"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "[Franz LLVM Native]"
./franz test/loop-stress/stress-test-1000.franz 2>&1 | grep -v DEBUG | grep -v 
echo ""
echo "[Rust Optimized]"
/tmp/rust_stress_1000
echo ""

# Test 10000 iterations
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Test: 10,000 iterations"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "[Franz LLVM Native]"
./franz test/loop-stress/stress-test-10000.franz 2>&1 | grep -v DEBUG | grep -v 
echo ""
echo "[Rust Optimized]"
/tmp/rust_stress_10000
echo ""

# Test 100000 iterations
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Test: 100,000 iterations"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "[Franz LLVM Native]"
./franz test/loop-stress/stress-test-100000.franz 2>&1 | grep -v DEBUG | grep -v 
echo ""
echo "[Rust Optimized]"
/tmp/rust_stress_100000
echo ""

echo "========================================"
echo "✓ All tests completed!"
echo "========================================"
echo ""
echo "Observations:"
echo "- Both Franz and Rust use LLVM backend"
echo "- Performance should be equivalent"
echo "- Franz compiles to native code by default"
