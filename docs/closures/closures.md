# Closures (LLVM Native) — Calling + Captures

It env capture by value, universal args, tag-aware return)

Overview
- Closures compile to a heap object: { func_ptr: i8*, env_ptr: i8*, ret_tag: i32 }.
- Env captures are by value: each free variable’s current value is stored in the env struct.
- Call ABI: func(env*: i8*, arg1: i64, arg2: i64, ... ) -> i8* (universal return).
- Runtime tag encodes the logical return: 0=int (i64), 1=float (double), 2=pointer (i8*).

What’s working
- Multi-parameter closures: wrappers now accept all non-env params as i64 and convert inside.
- Capturing closures: free vars from stack are copied by value into the env (no dangling stack ptrs).
- println detection for closure results: println inspects the callee closure’s ret_tag to print ints, floats, or strings correctly when the argument is a closure application.

Usage Examples
- Identity (int + string):
  - identity = {x -> <- x}
  - println "identity(42):" (identity 42)
  - println "identity('hello'):" (identity "hello")
- Multi-parameter:
  - adder = {a b -> <- (add a b)}
  - println "adder(2, 3):" (adder 2 3)
- Capturing closures:
  - make_adder = {n -> <- {x -> <- (add n x)}}
  - add5 = (make_adder 5)
  - println "add5(7):" (add5 7)

CLI / Flags
- No new flags. Existing debug prints can be toggled globally in code, and Logs.md records invocations.

Related Files
- src/llvm-closures/llvm_closures.c — closure struct/compilation/call conv.
- src/llvm-codegen/llvm_ir_gen.c — println tag-aware printing, application dispatch.

Examples
- examples/closures/working/strings.franz — identity on int and string.
- examples/closures/working/multi-arg.franz — adder with 2 params.
- examples/closures/working/captures.franz — make_adder capturing n.

Smoke Test
- scripts/closures-smoke.sh runs the working examples and fails on error.

Notes
- Pointer tag currently prints as string for closure results. Lists/dicts are routed through franz_print_generic when detected by AST (non-closure paths). Follow-up: richer pointer tagging if needed.





-----

