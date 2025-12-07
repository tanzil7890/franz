#!/bin/bash

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Counters
PASSED=0
FAILED=0
SKIPPED=0

# Known infinite/interactive examples to skip
SKIP_FILES=(
  "10-print.franz"
  "game-of-life.franz"
  "cube.franz"
  "fish.franz"
  "car.franz"
)

# Known failing directories
SKIP_PATTERNS=(
  "*/failing/*"
  "*/circular/*"
)

echo "=== Testing All Franz Examples ==="
echo ""

# Function to check if file should be skipped
should_skip() {
  local file="$1"
  local basename=$(basename "$file")

  # Check skip files
  for skip in "${SKIP_FILES[@]}"; do
    if [ "$basename" = "$skip" ]; then
      return 0
    fi
  done

  # Check skip patterns
  for pattern in "${SKIP_PATTERNS[@]}"; do
    if [[ "$file" == $pattern ]]; then
      return 0
    fi
  done

  return 1
}

# Test all examples
while IFS= read -r example; do
  if should_skip "$example"; then
    echo -e "${YELLOW}⊘ SKIP${NC}: $example (infinite/interactive/intentional-fail)"
    SKIPPED=$((SKIPPED + 1))
    continue
  fi

  # Run with 5 second timeout
  if timeout 5s ./franz "$example" > /dev/null 2>&1; then
    echo -e "${GREEN}✓ PASS${NC}: $example"
    PASSED=$((PASSED + 1))
  else
    EXIT_CODE=$?
    if [ $EXIT_CODE -eq 124 ]; then
      echo -e "${RED}✗ TIMEOUT${NC}: $example (likely infinite loop)"
    else
      echo -e "${RED}✗ FAIL${NC}: $example (exit code: $EXIT_CODE)"
    fi
    FAILED=$((FAILED + 1))
  fi
done < <(find examples -name "*.franz" | sort)

echo ""
echo "=== Test Summary ==="
echo -e "${GREEN}Passed: $PASSED${NC}"
echo -e "${RED}Failed: $FAILED${NC}"
echo -e "${YELLOW}Skipped: $SKIPPED${NC}"
echo "Total: $((PASSED + FAILED + SKIPPED))"
echo ""

if [ $FAILED -eq 0 ]; then
  echo -e "${GREEN}✅ All tests passed!${NC}"
  exit 0
else
  echo -e "${RED}❌ Some tests failed${NC}"
  exit 1
fi
