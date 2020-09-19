#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
FRANZ_BIN="$ROOT_DIR/franz"
CHECK_BIN="$ROOT_DIR/franz-check"

if [[ ! -x "$FRANZ_BIN" || ! -x "$CHECK_BIN" ]]; then
  echo "Build franz and franz-check first." >&2
  exit 1
fi

strict_flag=""
if [[ "${STRICT:-}" == "1" ]]; then
  strict_flag="--strict"
fi

echo "Running runtime smoke tests (examples/type-system/working/*)"
rc=0
for f in "$ROOT_DIR"/examples/type-system/working/*.franz; do
  echo "== $f (runtime) =="
  if ! "$FRANZ_BIN" "$f" >/dev/null; then
    echo "Runtime failure: $f" >&2
    rc=1
  fi
done

echo
echo "Running type-checker smoke tests (permissive by default)"
for f in "$ROOT_DIR"/examples/type-system/working/*.franz; do
  echo "== $f (type-check $strict_flag) =="
  if ! "$CHECK_BIN" $strict_flag "$f" >/dev/null; then
    echo "Type check failure: $f" >&2
    rc=1
  fi
done

echo
echo "Running runtime smoke tests (examples/adt/working/*)"
for f in "$ROOT_DIR"/examples/adt/working/*.franz; do
  echo "== $f (runtime) =="
  if ! "$FRANZ_BIN" "$f" >/dev/null; then
    echo "Runtime failure: $f" >&2
    rc=1
  fi
done

echo
echo "Running type-checker smoke tests for ADTs (permissive by default)"
for f in "$ROOT_DIR"/examples/adt/working/*.franz; do
  echo "== $f (type-check $strict_flag) =="
  if ! "$CHECK_BIN" $strict_flag "$f" >/dev/null; then
    echo "Type check failure: $f" >&2
    rc=1
  fi
done

echo
echo "Running recursion examples (runtime)"
for f in "$ROOT_DIR"/examples/recursion/working/*.franz; do
  echo "== $f (runtime) =="
  if ! "$FRANZ_BIN" "$f" >/dev/null; then
    echo "Runtime failure: $f" >&2
    rc=1
  fi
done

echo
echo "Running equality examples (runtime)"
for f in "$ROOT_DIR"/examples/equality/working/*.franz; do
  echo "== $f (runtime) =="
  if ! "$FRANZ_BIN" "$f" >/dev/null; then
    echo "Runtime failure: $f" >&2
    rc=1
  fi
done

echo
echo "Running equality examples (type-check $strict_flag)"
for f in "$ROOT_DIR"/examples/equality/working/*.franz; do
  echo "== $f (type-check $strict_flag) =="
  if ! "$CHECK_BIN" $strict_flag "$f" >/dev/null; then
    echo "Type check failure: $f" >&2
    rc=1
  fi
done

exit $rc
