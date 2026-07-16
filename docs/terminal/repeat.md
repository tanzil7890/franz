# String Repeat Function - repeat()



## Overview

The `repeat(string, count)` function repeats a string a specified number of times. Essential for creating progress bars, drawing borders, formatting output, and generating patterns.

**Performance:**
- **LLVM Mode:** C-level performance via `strcat()` in a loop
- **Runtime Mode:** Identical C implementation
- **Memory efficient:** Single allocation with size pre-calculated

## API Reference

### repeat(string, count)

Repeats a string `count` times and returns the concatenated result.

**Syntax:**
```franz
(repeat string count)
```

**Parameters:**
- `string` (TYPE_STRING) - The string to repeat
- `count` (TYPE_INT) - Number of repetitions (must be ≥ 0)

**Returns:**
- `TYPE_STRING` - Concatenated result
- **Empty string** when `count ≤ 0`
- **Original string** when `count = 1`

**Examples:**
```franz
// Basic repeat
(repeat "A" 5)              // → "AAAAA"

// With symbols
(repeat "★" 3)              // → "★★★"

// Empty result
(repeat "X" 0)              // → ""
(repeat "X" -5)             // → ""

// Store and use
bar = (repeat "█" 10)
(println bar)               // → "██████████"
```

## Usage Examples

### 1. Progress Bars
```franz
percent = 75
filled = (divide percent 2)      // 0-50 scale
empty = (subtract 50 filled)

bar_filled = (repeat "█" filled)
bar_empty = (repeat "░" empty)
(println "Progress: [" bar_filled bar_empty "]" percent "%")
```

Output:
```
Progress: [█████████████████████████████████████░░░░░░░░░░░░░] 75%
```

### 2. Box Drawing
```franz
width = 40
top = (repeat "─" width)
side = (repeat " " (subtract width 2))

(println "┌" top "┐")
(println "│" side "│")
(println "│" "  Welcome to Franz  " side "│")
(println "│" side "│")
(println "└" top "┘")
```

Output:
```
┌────────────────────────────────────────┐
│                                        │
│  Welcome to Franz                      │
│                                        │
└────────────────────────────────────────┘
```

### 3. Text Centering
```franz
text = "Hello, World!"
width = (columns)
text_len = 13  // Length of "Hello, World!"
padding_total = (subtract width text_len)
padding_left = (divide padding_total 2)

spaces = (repeat " " padding_left)
(println spaces text)
```

### 4. Creating Separators
```franz
// Simple separator
(println (repeat "=" 60))

// Header with separator
(println "Report Title")
(println (repeat "-" 13))

// Multi-character separator
(println (repeat "=-" 30))  // → "=-=-=-=-=-=-..."
```

### 5. Indentation
```franz
level = 3
indent = (repeat "  " level)  // 2 spaces per level

(println indent "- Item 1")
(println indent "- Item 2")
```

Output:
```
      - Item 1
      - Item 2
```

### 6. Pattern Generation
```franz
// ASCII art patterns
(println (repeat "/\\" 10))     // → "/\/\/\/\/\/\/\/\/\/\"
(println (repeat "* " 20))      // → "* * * * * * * * * * ..."

// Grid pattern
(loop (less_than i 5)
  {
    (println (repeat "# " 10))
    i = (add i 1)
  })
```

### 7. Loading Indicator
```franz
// Animated dots (in a loop)
counter = 0
(loop (less_than counter 4)
  {
    dots = (repeat "." counter)
    (print "Loading" dots "   \r")  // \r returns to start of line
    (wait 0.5)
    counter = (add counter 1)
  })
```

### 8. Table Column Padding
```franz
name = "Alice"
padding = (repeat " " (subtract 20 5))  // 20 - len("Alice")
(println name padding "Developer")
```

Output:
```
Alice               Developer
```

### 9. Terminal-Width Bars
```franz
// Full-width separator
width = (columns)
(println (repeat "─" width))

// Bar with margins
margin = 5
bar_width = (subtract width (multiply margin 2))
left_margin = (repeat " " margin)
(println left_margin (repeat "─" bar_width))
```

### 10. Multi-Line Borders
```franz
width = 60
height = 10

// Top border
(println (repeat "═" width))

// Middle rows
(loop (less_than i height)
  {
    (println (repeat " " width))
    i = (add i 1)
  })

// Bottom border
(println (repeat "═" width))
```

## Implementation Details

### Algorithm

Both Runtime and LLVM modes use the same approach:

```c
// Pseudocode
char* repeat(const char* str, int count) {
  if (count <= 0) return "";

  int str_len = strlen(str);
  int total_size = (str_len * count) + 1;  // +1 for null terminator

  char* result = malloc(total_size);
  result[0] = '\0';  // Initialize as empty string

  for (int i = 0; i < count; i++) {
    strcat(result, str);  // Append str to result
  }

  return result;
}
```

**Complexity:**
- **Time:** O(n × m) where n = string length, m = count
- **Space:** O(n × m) for the result string

## Best Practices

### 1. Pre-calculate String Length
```franz
// Good: Calculate once
width = (columns)
bar = (repeat "─" width)

// Avoid: Calculating multiple times
(println (repeat "─" (columns)))
(println (repeat "─" (columns)))
```

### 2. Handle Edge Cases
```franz
// Good: Validate count
count = user_input
(if (less_than count 0)
  {count = 0}
  {})
bar = (repeat "█" count)

// Avoid: Assuming positive count
bar = (repeat "█" user_input)  // May fail if negative
```

### 3. Store Repeated Strings for Reuse
```franz
// Good: Reuse computed strings
separator = (repeat "=" 80)
(println separator)
(println "Content")
(println separator)

// Avoid: Recomputing identical strings
(println (repeat "=" 80))
(println "Content")
(println (repeat "=" 80))
```

### 4. Be Mindful of Memory
```franz
// Good: Reasonable repetition
bar = (repeat "█" 100)

// Warning: Very large counts use significant memory
huge = (repeat "A" 1000000)  // 1MB allocation
```

### 5. Use for Visual Elements Only
```franz
// Good: Visual formatting
(println (repeat "-" 40))

// Avoid: Logic with repeated strings
// Bad: Creating lists via string manipulation
items = (repeat "item," 10)  // → "item,item,item,..."
// Better: Use proper list data structure
```

## Common Patterns

### 1. **Progress Bars**
```franz
filled = (repeat "█" progress)
empty = (repeat "░" (subtract 50 progress))
(println "[" filled empty "]")
```

### 2. **ASCII Art Borders**
```franz
(println "╔" (repeat "═" 38) "╗")
(println "║" (repeat " " 38) "║")
(println "╚" (repeat "═" 38) "╝")
```

### 3. **Indentation**
```franz
indent = (repeat "  " level)
(println indent text)
```

### 4. **Padding**
```franz
padding = (repeat " " (subtract width text_length))
(println text padding)
```

### 5. **Separators**
```franz
(println (repeat "─" (columns)))
```

## Performance Considerations

### Time Complexity
- **O(n × m)** where n = string length, m = count
- `repeat("A", 1000)` → 1,000 iterations
- `repeat("ABC", 100)` → 100 iterations × 3 chars = 300 operations

### Memory Usage
- **O(n × m)** - Total size = string_length × count + 1 byte
- `repeat("X", 100)` → 101 bytes
- `repeat("ABC", 1000)` → 3,001 bytes

### Optimization Tips
1. **Cache results** - Don't recompute identical strings
2. **Validate count** - Check bounds before allocation
3. **Consider alternatives** - For very large repetitions, use loops
4. **Profile memory** - Use `leaks` on macOS or `valgrind` on Linux

## Limitations

### 1. No Built-in Length Check
Franz doesn't have a `length()` function in LLVM mode yet. You need to track string lengths manually:

```franz
// Manual length tracking
text = "Hello"
text_len = 5  // Must know this
padding = (repeat " " (subtract 20 text_len))
```

### 2. Memory Allocation
Large repetitions allocate significant memory. Be cautious with:
```franz
// Warning: Allocates 10MB
huge = (repeat "A" 10000000)
```

### 3. No Unicode Width Awareness
Wide characters (emoji, CJK) may display incorrectly:
```franz
// May not align properly depending on terminal
(repeat "😀" 10)  // Each emoji is 2 columns wide visually
```

## Comparison with Other Languages

| Language    | Function                      | Example                     |
|-------------|-------------------------------|------------------------------|
| **Franz**   | `(repeat "X" 5)`              | `(repeat "A" 5)` → "AAAAA"   |
| **Python**  | `"X" * 5`                     | `"A" * 5` → "AAAAA"          |
| **Rust**    | `"X".repeat(5)`               | `"A".repeat(5)` → "AAAAA"    |
| **Go**      | `strings.Repeat("X", 5)`      | `strings.Repeat("A", 5)`     |
| **JS**      | `"X".repeat(5)`               | `"A".repeat(5)` → "AAAAA"    |
| **C**       | Manual loop with `strcat`     | Complex implementation       |

Franz provides C-level performance with functional syntax.

## Error Handling

### Invalid Arguments

**Wrong type for string:**
```franz
(repeat 123 5)
// ERROR: Expected TYPE_STRING at argument 1, got TYPE_INT
```

**Wrong type for count:**
```franz
(repeat "A" "five")
// ERROR: Expected TYPE_INT at argument 2, got TYPE_STRING
```

**Wrong argument count:**
```franz
(repeat "A")
// ERROR: Function 'repeat' requires 2 arguments, got 1

(repeat "A" 5 10)
// ERROR: Function 'repeat' requires 2 arguments, got 3
```

### Edge Cases (Handled Gracefully)

**Negative count:**
```franz
(repeat "X" -5)  // → "" (empty string)
```

**Zero count:**
```franz
(repeat "X" 0)   // → "" (empty string)
```

**Empty string:**
```franz
(repeat "" 100)  // → "" (empty string)
```
