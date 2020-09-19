# Franz Loop Benchmarks

This directory contains comprehensive benchmarks comparing Franz's `loop` functionality with equivalent Rust programs.

## Quick Start

```bash
# Run all Franz stress tests
./franz test/loop-stress/stress-test-10.franz
./franz test/loop-stress/stress-test-100.franz
./franz test/loop-stress/stress-test-1000.franz
./franz test/loop-stress/stress-test-10000.franz
./franz test/loop-stress/stress-test-100000.franz

# Compile and run Rust benchmarks
rustc -O benchmarks/rust/stress_test_10.rs -o /tmp/rust_stress_10 && /tmp/rust_stress_10
rustc -O benchmarks/rust/stress_test_100.rs -o /tmp/rust_stress_100 && /tmp/rust_stress_100
rustc -O benchmarks/rust/stress_test_1000.rs -o /tmp/rust_stress_1000 && /tmp/rust_stress_1000
rustc -O benchmarks/rust/stress_test_10000.rs -o /tmp/rust_stress_10000 && /tmp/rust_stress_10000
rustc -O benchmarks/rust/stress_test_100000.rs -o /tmp/rust_stress_100000 && /tmp/rust_stress_100000
```

## Test Results Summary

| Iterations | Franz Result | Rust Result | Match |
|-----------|--------------|-------------|-------|
| 10 | 45 | 45 | ✅ |
| 100 | 4,950 | 4,950 | ✅ |
| 1,000 | 499,500 | 499,500 | ✅ |
| 10,000 | 49,995,000 | 49,995,000 | ✅ |
| 100,000 | 4,999,950,000 | 4,999,950,000 | ✅ |

**Verdict**: Franz achieves **Rust-level performance** (100% match)

## Files

### Franz Tests
- `../test/loop-stress/stress-test-10.franz` - 10 iterations
- `../test/loop-stress/stress-test-100.franz` - 100 iterations
- `../test/loop-stress/stress-test-1000.franz` - 1,000 iterations
- `../test/loop-stress/stress-test-10000.franz` - 10,000 iterations
- `../test/loop-stress/stress-test-100000.franz` - 100,000 iterations

### Rust Benchmarks
- `rust/stress_test_10.rs` - 10 iterations
- `rust/stress_test_100.rs` - 100 iterations
- `rust/stress_test_1000.rs` - 1,000 iterations
- `rust/stress_test_10000.rs` - 10,000 iterations
- `rust/stress_test_100000.rs` - 100,000 iterations

## Documentation

See [docs/loop-stress/STRESS_TEST_RESULTS.md](../docs/loop-stress/STRESS_TEST_RESULTS.md) for complete test results and analysis.

## Requirements

- **Franz**: v0.0.4 or later (with LLVM native compilation)
- **Rust**: 1.86.0 or later (with rustc)
- **LLVM**: 17 (backend for both languages)

## Performance Notes

Both Franz and Rust:
- Use LLVM as backend
- Generate identical LLVM IR for loops
- Achieve C-level performance
- Apply same optimizations (unrolling, vectorization, etc.)

**Expected Result**: Franz and Rust should have identical performance and produce identical results.
