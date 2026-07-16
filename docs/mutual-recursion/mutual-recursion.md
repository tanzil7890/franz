# Mutual Recursion in Franz



## What is Mutual Recursion?

**Mutual recursion** occurs when two or more functions call each other in a circular dependency.

### Example Problem:
```
Function A calls Function B
Function B calls Function A
```

### Classic Example: `is_even` and `is_odd`

**Mathematical definition:**
- A number is **even** if:
  - It's 0 (base case), OR
  - It's 1 less than an **odd** number
- A number is **odd** if:
  - It's not 0 (base case), AND
  - It's 1 less than an **even** number

**These definitions reference each other!**

---

## How OCaml Handles It

### OCaml Requires `let rec ... and`

```ocaml
(* OCaml syntax for mutual recursion *)
let rec is_even n =
  if n = 0 then true
  else is_odd (n - 1)

and is_odd n =
  if n = 0 then false
  else is_even (n - 1)

(* Usage *)
is_even 4  (* true *)
is_odd 5   (* true *)
```

**Why OCaml needs `and`:**
- OCaml evaluates definitions sequentially
- Without `and`, `is_even` would try to call `is_odd` before it exists
- The `let rec ... and` construct defines both simultaneously

---

## How Franz Handles It

### Franz: No Special Keyword Needed! ✅

```franz
// Franz supports mutual recursion naturally
is_even = {n ->
  <- (if (is n 0)
    {<- 1}
    {<- (is_odd (subtract n 1))})
}

is_odd = {n ->
  <- (if (is n 0)
    {<- 0}
    {<- (is_even (subtract n 1))})
}

// Works immediately!
(println "is_even(4) = " (is_even 4))  // 1 (true)
(println "is_odd(5) = " (is_odd 5))    // 1 (true)
```

**Why Franz doesn't need `rec` or `and`:**
1. **Dynamic scoping:** Functions look up names at call-time, not definition-time
2. **Late binding:** Variable names are resolved when functions execute
3. **All functions are recursive by default:** No distinction between `let` and `let rec`

---

## More Examples

### Example 1: Even/Odd (Number Parity)

```franz
// Define mutually recursive functions
is_even = {n ->
  <- (if (is n 0)
    {<- 1}
    {<- (is_odd (subtract n 1))})
}

is_odd = {n ->
  <- (if (is n 0)
    {<- 0}
    {<- (is_even (subtract n 1))})
}

// Test cases
(println "Even/Odd Tests:")
(println "  is_even(0) = " (is_even 0))  // 1
(println "  is_even(1) = " (is_even 1))  // 0
(println "  is_even(4) = " (is_even 4))  // 1
(println "  is_odd(0) = " (is_odd 0))    // 0
(println "  is_odd(1) = " (is_odd 1))    // 1
(println "  is_odd(5) = " (is_odd 5))    // 1
```

**Output:**
```
Even/Odd Tests:
  is_even(0) = 1
  is_even(1) = 0
  is_even(4) = 1
  is_odd(0) = 0
  is_odd(1) = 1
  is_odd(5) = 1
```

### Example 2: Tree Traversal

```franz
// Parse nested lists vs atoms
// process_list and process_item call each other

process_list = {lst ->
  <- (if (is (length lst) 0)
    {<- (list)}
    {<-
      first = (process_item (get lst 0))
      rest = (process_list (get lst 1 (length lst)))
      <- (insert rest first 0)
    }
  )
}

process_item = {item ->
  <- (if (is (type item) "list")
    {<- (process_list item)}      // Mutual recursion!
    {<- (multiply item 2)}         // Base case: double numbers
  )
}

// Test with nested structure
data = (list 1 (list 2 3) 4 (list 5 (list 6)))
result = (process_list data)
(println "Processed: " result)
// Output: (list 2 (list 4 6) 8 (list 10 (list 12)))
```

### Example 3: State Machine

```franz
// State machine: states A and B call each other

state_a = {count ->
  (println "  State A: count = " count)
  <- (if (greater_than count 10)
    {<- "Done from A"}
    {<- (state_b (add count 1))})
}

state_b = {count ->
  (println "  State B: count = " count)
  <- (if (greater_than count 10)
    {<- "Done from B"}
    {<- (state_a (add count 2))})
}

(println "State Machine:")
result = (state_a 0)
(println "Result: " result)
```

**Output:**
```
State Machine:
  State A: count = 0
  State B: count = 1
  State A: count = 3
  State B: count = 4
  State A: count = 6
  State B: count = 7
  State A: count = 9
  State B: count = 10
  State A: count = 12
Result: Done from A
```

### Example 4: Parser (Expression and Term)

```franz
// Simplified parser with mutual recursion
// parse_expr can contain parse_term
// parse_term can contain parse_expr (in parens)

parse_expr = {tokens ->
  // Simplified: parse term, then check for operators
  term = (parse_term tokens)
  // In real parser, would check for + or - and recurse
  <- term
}

parse_term = {tokens ->
  // Simplified: check if token is number or parenthesized expression
  token = (get tokens 0)
  <- (if (is token "(")
    {<- (parse_expr (get tokens 1 (length tokens)))}  // Mutual recursion!
    {<- (integer token)}  // Base case: just a number
  )
}

// Example usage (simplified)
tokens1 = (list "42")
(println "Parse '42': " (parse_term tokens1))

tokens2 = (list "(" "123")
(println "Parse '(123': " (parse_term tokens2))
```

---

## How It Works Internally

### Franz's Scope Resolution

When Franz evaluates `is_even`:

```franz
is_even = {n ->
  <- (if (is n 0)
    {<- 1}
    {<- (is_odd (subtract n 1))})  // Looks up 'is_odd' here
}
```

**Evaluation steps:**

1. **Definition time:**
   - Store function AST in global scope under name `is_even`
   - Does NOT evaluate function body
   - Does NOT look up `is_odd` yet

2. **Call time** (when you call `is_even 4`):
   - Create new scope for function execution
   - NOW look up `is_odd` in current scope
   - By this time, `is_odd` is defined (late binding)

**Key insight:** Functions are not evaluated until called, so forward references work!

### Contrast with OCaml

**OCaml (static scoping + eager checking):**
```ocaml
(* This FAILS in OCaml: *)
let is_even n =
  if n = 0 then true
  else is_odd (n - 1)  (* ERROR: is_odd not defined yet *)

let is_odd n = ...
```

**Franz (dynamic scoping + lazy lookup):**
```franz
// This WORKS in Franz:
is_even = {n ->
  <- (is_odd (subtract n 1))  // ✅ Looked up at call-time
}

is_odd = {n -> ...}
```

---

## Comparison Table

| Feature | OCaml | Franz |
|---------|-------|-------|
| **Mutual recursion** | Needs `let rec ... and` | ✅ Works naturally |
| **Syntax** | Special keyword | No special syntax |
| **Scoping** | Static (lexical) | Dynamic |
| **Name resolution** | Definition-time | Call-time |
| **Forward references** | Not allowed (without `and`) | ✅ Allowed |
| **All functions recursive** | No (`let` vs `let rec`) | ✅ Yes |

---

## Advantages of Franz's Approach

### 1. **Simpler Syntax**
```franz
// No special keywords needed
func_a = {/* calls func_b */}
func_b = {/* calls func_a */}
```

vs OCaml:
```ocaml
(* Must use 'and' keyword *)
let rec func_a = ...
and func_b = ...
```

### 2. **Flexible Definition Order**

```franz
// Can even reassign later if needed
helper = void  // Placeholder

main = {-> (helper 10)}

helper = {x -> <- (multiply x 2)}  // Define after use

(print (main))  // Works! Prints: 20
```

OCaml would reject this at compile-time.

### 3. **Natural for Dynamic Languages**

Franz's approach is common in dynamic languages:
- Python: Functions can reference names defined later in file
- JavaScript: Function hoisting allows forward references
- Ruby: Methods can call methods defined later

### 4. **No Mental Overhead**

**OCaml forces you to think:**
- "Do I need `rec`?"
- "Do I need `and`?"
- "What order should I define things?"

**Franz:**
- Just define functions
- They work

---

## Disadvantages of Franz's Approach

### 1. **Typos Caught at Runtime**

```franz
is_even = {n ->
  <- (is_odd (subtract n 1))  // Typo: should be 'is_oddd'
}

is_odd = {n -> ...}

// Error only happens when you CALL is_even
(is_even 4)  // Runtime Error: is_odd not defined
```

OCaml would catch this at compile-time.

### 2. **Harder to Refactor**

```franz
// Renamed is_odd to check_odd
check_odd = {n -> ...}

// Forgot to update is_even
is_even = {n ->
  <- (is_odd (subtract n 1))  // BUG! is_odd doesn't exist
}

// Won't know until runtime
```

OCaml's compiler would immediately show the error.

### 3. **No Dependency Analysis**

OCaml can analyze dependencies:
```ocaml
(* Compiler knows: is_even depends on is_odd *)
let rec is_even n = ... is_odd ...
and is_odd n = ... is_even ...
```

Franz can't (until runtime):
```franz
is_even = {/* depends on what? Unknown until execution */}
```

---

## Best Practices for Franz

### 1. **Define Helper Functions Before Main Functions**

```franz
// Good: helpers first
is_odd = {n -> ...}
is_even = {n -> ...}

// Then use them
result = (is_even 10)
```

### 2. **Use Comments to Document Dependencies**

```franz
// is_even depends on: is_odd
is_even = {n ->
  <- (is_odd (subtract n 1))
}

// is_odd depends on: is_even
is_odd = {n ->
  <- (is_even (subtract n 1))
}
```

### 3. **Test Early and Often**

```franz
// Define functions
is_even = {n -> ...}
is_odd = {n -> ...}

// Test immediately to catch name errors
(println "Test: " (is_even 4))
(println "Test: " (is_odd 5))
```

### 4. **Consider Type Signatures**

```franz
// Document expected behavior
(sig is_even (integer -> integer))
(sig is_odd (integer -> integer))

is_even = {n -> ...}
is_odd = {n -> ...}

// Run franz-check to catch issues
// $ franz-check mycode.franz
```

---


---

## Real-World Example: JSON Parser

```franz
// Complete example: parse JSON (simplified)

parse_value = {str ->
  first_char = (get str 0)
  <- (if (is first_char "{")
    {<- (parse_object str)}
    (is first_char "[")
    {<- (parse_array str)}
    (is first_char "\"")
    {<- (parse_string str)}
    {<- (parse_number str)}
  )
}

parse_object = {str ->
  // Object contains values (mutual recursion)
  (println "Parsing object...")
  <- (parse_value (get str 1 (length str)))
}

parse_array = {str ->
  // Array contains values (mutual recursion)
  (println "Parsing array...")
  <- (parse_value (get str 1 (length str)))
}

parse_string = {str ->
  <- (get str 1 (subtract (length str) 1))
}

parse_number = {str ->
  <- (integer str)
}

// Test
(println "Result: " (parse_value "{\"key\": 123}"))
(println "Result: " (parse_value "[1, 2, 3]"))
(println "Result: " (parse_value "\"hello\""))
(println "Result: " (parse_value "42"))
```

**This pattern (mutual recursion) is essential for:**
- Parsers (expressions contain terms, terms contain expressions)
- Tree traversal (process nodes → process children → process nodes)
- State machines (state A → state B → state A)
- Game AI (evaluate move → evaluate counter-move → evaluate move)

---

## Conclusion

### Franz DOES Support Mutual Recursion ✅

**Summary:**
- ✅ No `rec` keyword needed (all functions recursive by default)
- ✅ No `and` keyword needed (late binding allows forward references)
- ✅ Works naturally with dynamic scoping
- ✅ Simpler syntax than OCaml for this use case

**Trade-offs:**
- ✅ Simpler to write
- ❌ Errors caught at runtime (not compile-time)
- ❌ No static dependency analysis

**Recommendation:**
- Use mutual recursion freely in Franz
- Add type signatures with `sig` for documentation
- Test thoroughly (errors surface at runtime)
- Consider `franz-check` for static analysis

