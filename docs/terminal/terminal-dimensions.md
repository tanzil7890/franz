# Terminal Dimension Functions - rows() and columns()


## Overview

Franz provides `rows()` and `columns()` functions to query the current terminal dimensions. These functions enable responsive terminal UIs, dynamic layouts, and terminal-aware formatting.

**Performance:**
- **LLVM Mode:** C-level performance via direct `ioctl(TIOCGWINSZ)` system call
- **Runtime Mode:** Same system call, identical behavior
- **Zero overhead:** Direct CPU instructions, no interpreter layer

## API Reference

### rows()

Returns the number of rows (height) in the current terminal.

**Syntax:**
```franz
(rows)
```

**Parameters:** None

**Returns:**
- `TYPE_INT` - Terminal height in rows (lines)
- **Default:** 24 (when not a TTY or ioctl fails)

**Examples:**
```franz
// Get terminal height
height = (rows)
(println "Terminal has" height "rows")

// Use in calculations
center_row = (divide height 2)

// Responsive output
(if (less_than height 20)
  {(println "Small terminal detected")}
  {(println "Standard terminal size")})
```

### columns()

Returns the number of columns (width) in the current terminal.

**Syntax:**
```franz
(columns)
```

**Parameters:** None

**Returns:**
- `TYPE_INT` - Terminal width in columns (characters)
- **Default:** 80 (when not a TTY or ioctl fails)

**Examples:**
```franz
// Get terminal width
width = (columns)
(println "Terminal width:" width "columns")

// Calculate margins
margin = (divide (subtract width 60) 2)

// Fit content to width
max_line_length = (subtract width 10)
```

## Implementation Details

### System Call
Both functions use the POSIX `ioctl()` system call with `TIOCGWINSZ` request:

```c
struct winsize w;
ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
// rows: w.ws_row
// columns: w.ws_col
```

### Fallback Behavior
When the program is not running in a TTY (e.g., piped input/output, non-interactive), or when `ioctl()` fails, the functions return standard defaults:
- **rows:** 24 (classic terminal height)
- **columns:** 80 (classic terminal width)

## Usage Examples

### 1. Simple Terminal Query
```franz
(println "=== Terminal Information ===")
(println "Rows:" (rows))
(println "Columns:" (columns))
```

Output:
```
=== Terminal Information ===
Rows: 50
Columns: 120
```

### 2. Centered Text
```franz
text = "Welcome to Franz"
width = (columns)
padding = (divide (subtract width 16) 2)
(println (repeat " " padding) text)
```

### 3. Responsive Box Drawing
```franz
width = (columns)
border = (repeat "─" (subtract width 4))
(println "┌" border "┐")
(println "│" (repeat " " (subtract width 4)) "│")
(println "└" border "┘")
```

### 4. Progress Bar (Terminal-Width Aware)
```franz
width = (columns)
bar_width = (subtract width 20)  // Space for labels
filled = (divide (multiply bar_width percent) 100)
empty = (subtract bar_width filled)

bar_filled = (repeat "█" filled)
bar_empty = (repeat "░" empty)
(println "Progress: [" bar_filled bar_empty "]" percent "%")
```

### 5. Dynamic Table Layout
```franz
width = (columns)

(if (greater_than width 100)
  {
    // Wide terminal: 3-column layout
    col_width = (divide width 3)
    (println "Column1" (repeat " " col_width) "Column2" (repeat " " col_width) "Column3")
  }
  {
    // Narrow terminal: 1-column layout
    (println "Column1")
    (println "Column2")
    (println "Column3")
  })
```

### 6. Paginated Output
```franz
height = (rows)
page_size = (subtract height 5)  // Reserve space for header/footer

counter = 0
(loop (less_than counter 100)
  {
    (println "Line" counter)
    counter = (add counter 1)

    // Pause at page boundaries
    (if (is (remainder counter page_size) 0)
      {
        (println "--- Press Enter for next page ---")
        (input)
      }
      {})
  })
```

### 7. Viewport Clipping
```franz
width = (columns)
text = "This is a very long line that might exceed terminal width and needs to be clipped"

(if (greater_than (length text) width)
  {
    // Clip to terminal width
    clipped = (substring text 0 (subtract width 3))
    (println clipped "...")
  }
  {
    (println text)
  })
```

## Best Practices

### 1. Always Handle Non-TTY Scenarios
```franz
// Good: Check if dimensions are reasonable
width = (columns)
(if (greater_than width 20)
  {/* Use actual width */}
  {/* Fallback for very narrow terminals */})

// Avoid: Assuming specific dimensions
// Bad: bar = (repeat "─" 80)
```

### 2. Cache Dimension Queries
```franz
// Good: Query once if dimensions won't change
width = (columns)
height = (rows)
(loop ...)  // Use cached width/height

// Avoid: Querying in tight loops (adds overhead)
(loop (println (repeat "─" (columns))))  // Queries every iteration
```

### 3. Leave Margins
```franz
// Good: Leave space for margins
width = (columns)
content_width = (subtract width 10)  // 5-char margins on each side

// Avoid: Using full terminal width (hard to read)
bar = (repeat "─" (columns))  // Goes edge-to-edge
```

### 4. Responsive Design
```franz
// Good: Adapt to terminal size
width = (columns)
(if (less_than width 60)
  {/* Compact layout */}
  {/* Full layout */})

// Avoid: Fixed layouts that break in narrow terminals
```

### 5. Test in Different Terminal Sizes
```bash
# Test with standard size (80x24)
./franz your_program.franz

# Test with wide terminal (resize your terminal first)
./franz your_program.franz

# Test with piped output (defaults to 80x24)
./franz your_program.franz | cat
```

## Common Use Cases

### 1. **Progress Indicators**
Show download/upload progress with bars that adapt to terminal width.

### 2. **Box Drawing**
Create ASCII art boxes that fit the current terminal size.

### 3. **Table Formatting**
Adjust column widths based on available space.

### 4. **Pagination**
Split long output into pages based on terminal height.

### 5. **Text Wrapping**
Break long lines to fit within terminal width.

### 6. **Centered Display**
Center headers, titles, and important messages.

### 7. **Responsive Menus**
Adjust menu layout (horizontal vs vertical) based on terminal size.

### 8. **Dashboard Layouts**
Create monitoring dashboards that adapt to available space.

## Limitations

### 1. Static Query
Dimensions are queried at the moment of the function call. If the user resizes the terminal during program execution, you need to call `rows()` / `columns()` again to get updated values.

### 2. Non-TTY Defaults
When output is piped or redirected, the functions return defaults (80x24). Programs should handle this gracefully.

### 3. No Resize Signal
Franz doesn't currently provide a way to detect terminal resize events (SIGWINCH). Programs must manually re-query dimensions.

### 4. ANSI Limitation
The functions return character-based dimensions. Wide characters (emoji, CJK) may display incorrectly if your layout doesn't account for them.

## Comparison with Other Languages

| Language    | Function                     | Notes                        |
|-------------|------------------------------|------------------------------|
| **Franz**   | `(rows)` `(columns)`         | Zero-overhead system call    |
| **Python**  | `os.get_terminal_size()`     | Returns namedtuple           |
| **Rust**    | `terminal_size::terminal_size()` | External crate required  |
| **Go**      | `terminal.Size()`            | golang.org/x/term package    |
| **Node.js** | `process.stdout.rows/columns`| Property access              |
| **C**       | `ioctl(STDOUT, TIOCGWINSZ)`  | Direct system call           |

Franz matches C performance with a cleaner functional syntax.
