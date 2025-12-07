# Multi-Statement Function Bodies

Franz supports **multi-statement function bodies** with local variable assignments, enabling you to write complex, readable functions.

## Basic Syntax

```franz
function_name = {param1 param2 param3 ->
  local_var1 = (expression1)
  local_var2 = (expression2)
  local_var3 = (expression3)
  <- final_result
}
```

## Simple Example

```franz
// Calculate circle area with intermediate steps
circle_area = {radius ->
  pi = 3.14159
  radius_squared = (power radius 2)
  area = (multiply pi radius_squared)
  <- area
}

result = (circle_area 5.0)  // → 78.539750
```

## Key Features

### 1. Local Variables

Each statement in the function body can assign to local variables:

```franz
calculate_total = {price quantity discount ->
  subtotal = (multiply price quantity)
  discount_amount = (multiply subtotal discount)
  total = (subtract subtotal discount_amount)
  <- total
}
```

### 2. Return Value

The **last statement's value** becomes the function's return value:

```franz
max_of_three = {a b c ->
  max_ab = (if (greater_than a b) {<- a} {<- b})
  result = (if (greater_than max_ab c) {<- max_ab} {<- c})
  <- result  // This is returned
}
```

### 3. Works with All Features

Multi-statement bodies work with:
- **Recursion**
- **Closures**
- **Conditionals** (`if`)
- **Loops** (`loop`, `until`)
- **All operators** (arithmetic, logic, comparisons)

## Real-World Examples

### Input Validation

```franz
validate_age_range = {age min_age max_age ->
  is_old_enough = (greater_than age min_age)
  is_young_enough = (less_than age max_age)
  is_valid = (and is_old_enough is_young_enough)
  <- is_valid
}

valid = (validate_age_range 25 18 65)  // → 1
```

### Access Control

```franz
check_access = {is_admin is_owner is_premium ->
  admin_access = is_admin
  owner_access = (and is_owner is_premium)
  has_access = (or admin_access owner_access)
  <- has_access
}

access = (check_access 0 1 1)  // → 1 (premium owner)
```

### Complex Calculation

```franz
discount_calculator = {price quantity is_member ->
  base_price = (multiply price quantity)
  member_discount = (if is_member {<- 0.10} {<- 0})
  volume_discount = (if (greater_than quantity 10) {<- 0.05} {<- 0})
  total_discount = (add member_discount volume_discount)
  discount_amount = (multiply base_price total_discount)
  final_price = (subtract base_price discount_amount)
  <- final_price
}
```

### Recursive Algorithm

```franz
// Fibonacci with multi-statement body
fib = {n ->
  is_base = (less_than n 2)
  result = (if is_base
    {<- n}
    {
      n1 = (subtract n 1)
      n2 = (subtract n 2)
      f1 = (fib n1)
      f2 = (fib n2)
      <- (add f1 f2)
    }
  )
  <- result
}

result = (fib 8)  // → 21
```

### XOR Implementation

```franz
xor = {a b ->
  both = (and a b)
  either = (or a b)
  not_both = (not both)
  result = (and either not_both)
  <- result
}

r1 = (xor 1 0)  // → 1 (different)
r2 = (xor 1 1)  // → 0 (same)
```

## Best Practices

### 1. Use Descriptive Variable Names

```franz
// ✅ GOOD - Clear intent
calculate_bmi = {weight_kg height_m ->
  height_squared = (power height_m 2)
  bmi = (divide weight_kg height_squared)
  <- bmi
}

// ❌ BAD - Unclear
calc = {w h ->
  x = (power h 2)
  y = (divide w x)
  <- y
}
```

### 2. Break Complex Logic Into Steps

```franz
// ✅ GOOD - Easy to understand
password_strength = {password ->
  has_min_length = (greater_than (length password) 7)
  has_uppercase = (contains_uppercase password)
  has_number = (contains_number password)
  is_strong = (and (and has_min_length has_uppercase) has_number)
  <- is_strong
}

// ❌ BAD - Hard to read
password_strength = {password ->
  <- (and (and (greater_than (length password) 7) (contains_uppercase password)) (contains_number password))
}
```

### 3. Keep Functions Focused

Each function should do **one thing well**:

```franz
// ✅ GOOD - Single responsibility
validate_email = {email ->
  has_at = (contains email "@")
  has_dot = (contains email ".")
  min_length = (greater_than (length email) 5)
  is_valid = (and (and has_at has_dot) min_length)
  <- is_valid
}

// ✅ GOOD - Separate function for complex validation
validate_user = {age email password ->
  age_valid = (validate_age age)
  email_valid = (validate_email email)
  password_valid = (validate_password password)
  all_valid = (and (and age_valid email_valid) password_valid)
  <- all_valid
}
```

## Common Patterns

### Pattern 1: Conditional Assignment

```franz
get_discount = {is_member total_purchase ->
  base_discount = (if is_member {<- 0.10} {<- 0})
  volume_bonus = (if (greater_than total_purchase 100) {<- 0.05} {<- 0})
  total_discount = (add base_discount volume_bonus)
  <- total_discount
}
```

### Pattern 2: Multi-Step Transformation

```franz
convert_temperature = {fahrenheit ->
  celsius = (multiply (subtract fahrenheit 32) 0.5556)
  kelvin = (add celsius 273.15)
  <- kelvin
}
```

### Pattern 3: Validation Pipeline

```franz
validate_input = {value min_val max_val ->
  is_number = (is_numeric value)
  in_range = (and (greater_than value min_val) (less_than value max_val))
  is_valid = (and is_number in_range)
  <- is_valid
}
```

## Performance

Multi-statement bodies compile to **native machine code** via LLVM with:
- **Zero runtime overhead** - Same performance as single-statement bodies
- **Full optimization** - LLVM optimizes away unnecessary intermediate variables
- **C-level speed** - Identical performance to handwritten C code

## Comparison with Other Languages

| Language | Syntax | Franz Equivalent |
|----------|--------|------------------|
| **Rust** | `{ let x = 5; let y = 10; x + y }` | `{<- result = (add x y); <- result}` |
| **OCaml** | `let x = 5 in let y = 10 in x + y` | `{x = 5; y = 10; <- (add x y)}` |
| **JavaScript** | `function() { let x = 5; let y = 10; return x + y; }` | `{x = 5; y = 10; <- (add x y)}` |
| **Python** | `def f(): x = 5; y = 10; return x + y` | `{x = 5; y = 10; <- (add x y)}` |

Franz's multi-statement bodies match modern language standards!
