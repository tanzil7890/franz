#!/usr/bin/env python3
import subprocess
import sys
from pathlib import Path

# Skip these (infinite/interactive/input-based)
SKIP_FILES = {
    "10-print.franz",
    "game-of-life.franz",
    "cube.franz",
    "fish.franz",
    "car.franz",
    # Interactive examples (use input())
    "collatz.franz",
    "factorial.franz",
    "gcd.franz",
    "geometric-mean.franz",
    "pig-latin.franz",
    "split.franz",
    # Timed examples
    "countdown.franz",
}

# Skip these patterns
SKIP_PATTERNS = ["failing", "circular"]

passed = 0
failed = 0
skipped = 0
failed_files = []

print("=== Testing All Franz Examples ===\n")

for example in sorted(Path("examples").rglob("*.franz")):
    # Check if should skip
    if example.name in SKIP_FILES:
        print(f"⊘ SKIP: {example} (infinite/interactive)")
        skipped += 1
        continue

    if any(pattern in str(example) for pattern in SKIP_PATTERNS):
        print(f"⊘ SKIP: {example} (intentional fail)")
        skipped += 1
        continue

    # Run test with timeout (10s for slower examples like progress.franz)
    try:
        result = subprocess.run(
            ["./franz", str(example)],
            capture_output=True,
            timeout=10,
            text=True
        )
        if result.returncode == 0:
            print(f"✓ PASS: {example}")
            passed += 1
        else:
            print(f"✗ FAIL: {example} (exit code: {result.returncode})")
            failed += 1
            failed_files.append(str(example))
    except subprocess.TimeoutExpired:
        print(f"✗ TIMEOUT: {example}")
        failed += 1
        failed_files.append(str(example))

print(f"\n=== Summary ===")
print(f"Passed: {passed}")
print(f"Failed: {failed}")
print(f"Skipped: {skipped}")
print(f"Total: {passed + failed + skipped}")

if failed > 0:
    print(f"\nFailed files:")
    for f in failed_files:
        print(f"  - {f}")
    sys.exit(1)
else:
    print("\n✅ All tests passed!")
    sys.exit(0)
