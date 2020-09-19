#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT_DIR/franz"

run() {
  local file="$1"
  echo "--- Running: $file" >&2
  "$BIN" "$file" > /dev/null
}

main() {
  run "$ROOT_DIR/examples/closures/working/strings.franz"
  run "$ROOT_DIR/examples/closures/working/multi-arg.franz"
  run "$ROOT_DIR/examples/closures/working/captures.franz"
  echo "All closure smoke tests passed." >&2
}

main "$@"

