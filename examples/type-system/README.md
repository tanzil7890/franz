Franz Type System Examples

- Folder purpose: collect all type-system related examples in one place.

- Subfolders:
  - `working/` — Examples that run with the current interpreter. Signatures `(sig ...)` are parsed and ignored at runtime.
  - `failing/` — Examples that either intentionally demonstrate type errors or rely on checker-only assumptions (e.g., single-arg callbacks for `map`). They may fail at runtime today.

- How to run:
  - Runtime: `./franz examples/type-system/working/<file>.franz`
  - Type check (permissive): `./franz-check examples/type-system/<subfolder>/<file>.franz`
  - Type check (fail on errors): `./franz-check --strict examples/type-system/<subfolder>/<file>.franz`

CI/automation
- A simple smoke test script runs all working examples via runtime and non-strict checker:
  - `scripts/smoke.sh`
  - Set `STRICT=1 scripts/smoke.sh` to enable `--strict` in the checker run.

Notes
- `map` in Franz passes two arguments to callbacks: `(item index -> ...)`. Prefer two-parameter callbacks in runnable examples.
- The type checker currently reports messages but exits successfully; stricter failure behavior is planned.
