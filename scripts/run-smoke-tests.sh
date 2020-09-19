#!/bin/bash
# Automated smoke test runner for Franz scoping modes
# Usage: ./scripts/run-smoke-tests.sh

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== Franz Scoping Smoke Test Runner ===${NC}"
echo ""

# Check if franz binary exists
if [ ! -f "./franz" ]; then
  echo -e "${RED}Error: franz binary not found. Please compile first:${NC}"
  echo "  find src -name '*.c' -not -name 'check.c' | xargs gcc -Wall -lm -o franz"
  exit 1
fi

# Track results
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Test function
run_test() {
  local TEST_NAME="$1"
  local TEST_FILE="$2"
  local SCOPING_MODE="$3"

  echo -e "${YELLOW}Running: $TEST_NAME${NC}"

  TOTAL_TESTS=$((TOTAL_TESTS + 1))

  if ./franz --scoping="$SCOPING_MODE" "$TEST_FILE" 2>&1 | grep -q "All smoke tests PASSED"; then
    echo -e "${GREEN}✓ PASSED${NC}"
    PASSED_TESTS=$((PASSED_TESTS + 1))
  else
    echo -e "${RED}✗ FAILED${NC}"
    FAILED_TESTS=$((FAILED_TESTS + 1))
  fi

  echo ""
}

# Run lexical scoping smoke tests
echo -e "${BLUE}--- Lexical Scoping Smoke Tests ---${NC}"
run_test "Lexical Scoping" "test/smoke-tests/smoke-test-lexical.franz" "lexical"

# Run dynamic scoping smoke tests
echo -e "${BLUE}--- Dynamic Scoping Smoke Tests ---${NC}"
run_test "Dynamic Scoping" "test/smoke-tests/smoke-test-dynamic.franz" "dynamic"

# Run examples to verify they work
echo -e "${BLUE}--- Example Verification ---${NC}"

# Lexical examples
for example in examples/scoping/lexical/working/*.franz; do
  if [ -f "$example" ]; then
    basename_example=$(basename "$example")
    echo -e "${YELLOW}Running: $basename_example (lexical)${NC}"
    TOTAL_TESTS=$((TOTAL_TESTS + 1))

    if ./franz --scoping=lexical "$example" > /dev/null 2>&1; then
      echo -e "${GREEN}✓ PASSED${NC}"
      PASSED_TESTS=$((PASSED_TESTS + 1))
    else
      echo -e "${RED}✗ FAILED${NC}"
      FAILED_TESTS=$((FAILED_TESTS + 1))
    fi
    echo ""
  fi
done

# Lexical patterns
for example in examples/scoping/lexical/patterns/*.franz; do
  if [ -f "$example" ]; then
    basename_example=$(basename "$example")
    echo -e "${YELLOW}Running: $basename_example (lexical)${NC}"
    TOTAL_TESTS=$((TOTAL_TESTS + 1))

    if ./franz --scoping=lexical "$example" > /dev/null 2>&1; then
      echo -e "${GREEN}✓ PASSED${NC}"
      PASSED_TESTS=$((PASSED_TESTS + 1))
    else
      echo -e "${RED}✗ FAILED${NC}"
      FAILED_TESTS=$((FAILED_TESTS + 1))
    fi
    echo ""
  fi
done

# Dynamic examples
for example in examples/scoping/dynamic/working/*.franz; do
  if [ -f "$example" ]; then
    basename_example=$(basename "$example")
    echo -e "${YELLOW}Running: $basename_example (dynamic)${NC}"
    TOTAL_TESTS=$((TOTAL_TESTS + 1))

    if ./franz --scoping=dynamic "$example" > /dev/null 2>&1; then
      echo -e "${GREEN}✓ PASSED${NC}"
      PASSED_TESTS=$((PASSED_TESTS + 1))
    else
      echo -e "${RED}✗ FAILED${NC}"
      FAILED_TESTS=$((FAILED_TESTS + 1))
    fi
    echo ""
  fi
done

# Summary
echo -e "${BLUE}=== Smoke Test Summary ===${NC}"
echo -e "Total tests: $TOTAL_TESTS"
echo -e "${GREEN}Passed: $PASSED_TESTS${NC}"
echo -e "${RED}Failed: $FAILED_TESTS${NC}"
echo ""

if [ $FAILED_TESTS -eq 0 ]; then
  echo -e "${GREEN}✓ All smoke tests PASSED!${NC}"
  exit 0
else
  echo -e "${RED}✗ Some tests FAILED!${NC}"
  exit 1
fi
