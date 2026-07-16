# Algebraic Data Types (Variants)

Franz supports tagged unions (variants) via standard library functions. This follows Franz’s philosophy: no new syntax, everything is a function.

## API

- `(variant tag value1 value2 ...)` → `(list any)`
  - Builds a variant value as `[tag, [values...]]`
- `(variant_tag v)` → `string`
  - Returns the tag
- `(variant_values v)` → `(list any)`
  - Returns the list of values
- `(match v tag1 fn1 tag2 fn2 ... [defaultFn])` → any
  - Executes the matching function with the variant’s values as arguments
  - Optional `defaultFn` is called with the variant as a single argument

## Examples

### Option

```franz
some = (variant "Some" 42)
none = (variant "None")

(match some
  "Some" {x -> (println "Some:" x)}
  "None" {-> (println "None")}
)
```

### Either

```franz
left  = (variant "Left" "oops")
right = (variant "Right" 99)

handle = {v -> (match v
  "Left"  {msg -> (println "Error:" msg)}
  "Right" {val -> (println "Value:" val)}
)}

(handle left)
(handle right)
```

## Types and Checking

- At runtime, variants are lists: `[string, list]`. This keeps the core minimal.
- The type checker approximates:
  - `variant: (string -> (list any))`
  - `variant_tag: ((list any) -> string)`
  - `variant_values: ((list any) -> (list any))`
  - `match: (any -> any)` (variadic; return type unified from case functions when possible)

## Subject Reduction Guard

- Use `--assert-types` to check before running:
  - `./franz --assert-types examples/adt/working/option.franz`
