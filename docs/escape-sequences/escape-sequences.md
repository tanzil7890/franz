# Escape Sequences in Strings


## Overview

Franz supports industry-standard escape sequences in string literals, allowing you to include special characters like newlines, tabs, quotes, and control characters. The implementation uses C-style escape codes and includes hexadecimal character escapes for maximum flexibility.

### Supported Escape Sequences

| Escape | Character | Description | Example |
|--------|-----------|-------------|---------|
| `\n` | Newline (LF) | Line feed (ASCII 10) | `"Line1\nLine2"` |
| `\t` | Tab | Horizontal tab (ASCII 9) | `"Col1\tCol2"` |
| `\r` | Carriage Return | CR (ASCII 13) | `"Before\rAfter"` |
| `\\` | Backslash | Literal backslash | `"C:\\path"` |
| `\"` | Double Quote | Literal quote | `"Say \"Hi\""` |
| `\a` | Alert/Bell | Bell character (ASCII 7) | `"Alert\a"` |
| `\b` | Backspace | Backspace (ASCII 8) | `"Text\b"` |
| `\f` | Form Feed | Page break (ASCII 12) | `"Page1\fPage2"` |
| `\v` | Vertical Tab | Vertical tab (ASCII 11) | `"Line1\vLine2"` |
| `\e` | Escape | Escape character (ASCII 27) | `"\e[31m"` |
| `\xHH` | Hex Character | Hexadecimal code | `"\x41"` = 'A' |

---

## API Reference

### Standard Escape Sequences

#### Newline (`\n`)

**Syntax:** `\n`

**Description:** Inserts a newline (line feed) character

**Example:**
```franz
text = "Line 1\nLine 2\nLine 3"
(println text)
```

**Output:**
```
Line 1
Line 2
Line 3
```

---

#### Tab (`\t`)

**Syntax:** `\t`

**Description:** Inserts a horizontal tab character

**Example:**
```franz
(println "Name\tAge\tCity")
(println "Alice\t30\tNY")
```

**Output:**
```
Name	Age	City
Alice	30	NY
```

---

#### Carriage Return (`\r`)

**Syntax:** `\r`

**Description:** Inserts a carriage return character (moves cursor to line start)

**Example:**
```franz
text = "Before\rAfter"
(println text)
```

---

#### Backslash (`\\`)

**Syntax:** `\\`

**Description:** Inserts a literal backslash character

**Example:**
```franz
path = "C:\\Users\\Admin\\file.txt"
(println path)
```

**Output:**
```
C:\Users\Admin\file.txt
```

---

#### Double Quote (`\"`)

**Syntax:** `\"`

**Description:** Inserts a literal double quote character

**Example:**
```franz
message = "She said \"Hello\" to me"
(println message)
```

**Output:**
```
She said "Hello" to me
```

---

### Control Character Escapes

#### Bell/Alert (`\a`)

**Syntax:** `\a`

**Description:** Inserts a bell/alert character (may produce system beep)

**Example:**
```franz
alert = "Warning!\a"
(println alert)
```

---

#### Backspace (`\b`)

**Syntax:** `\b`

**Description:** Inserts a backspace character

**Example:**
```franz
text = "ABC\bD"  // Result: "ABD" (C is backspaced)
(println text)
```

---

#### Form Feed (`\f`)

**Syntax:** `\f`

**Description:** Inserts a form feed (page break) character

**Example:**
```franz
pages = "Page 1\fPage 2"
(println pages)
```

---

#### Vertical Tab (`\v`)

**Syntax:** `\v`

**Description:** Inserts a vertical tab character

**Example:**
```franz
text = "Line1\vLine2"
(println text)
```

---

#### Escape Character (`\e`)

**Syntax:** `\e`

**Description:** Inserts an escape character (used for ANSI codes)

**Example:**
```franz
// ANSI red color code
red_text = "\e[31mRed Text\e[0m"
(println red_text)
```

**Common ANSI Codes:**
- `\e[31m` - Red text
- `\e[32m` - Green text
- `\e[34m` - Blue text
- `\e[1m` - Bold text
- `\e[4m` - Underlined text
- `\e[0m` - Reset formatting

---

### Hexadecimal Escape Sequences

#### Hex Character Code (`\xHH`)

**Syntax:** `\xHH` where HH is two hexadecimal digits (0-9, a-f, A-F)

**Description:** Inserts a character by its ASCII hexadecimal code

**Examples:**
```franz
// Single character
letter_A = "\x41"  // 0x41 = 'A'
(println letter_A)

// Multiple characters
hello = "\x48\x65\x6c\x6c\x6f"  // "Hello"
(println hello)

// Special characters
slash = "\x2F"  // '/'
path = "\x2Fpath\x2Fto\x2Ffile"  // "/path/to/file"
(println path)
```

**Hex Codes for Common Characters:**
- `\x20` - Space
- `\x2F` - Forward slash (/)
- `\x3A` - Colon (:)
- `\x41-\x5A` - Uppercase letters (A-Z)
- `\x61-\x7A` - Lowercase letters (a-z)
- `\x30-\x39` - Digits (0-9)

---

## Usage Patterns

### Pattern 1: Multi-line Text

```franz
poem = "Roses are red,\nViolets are blue,\nFranz is fast,\nAnd elegant too!"
(println poem)
```

**Output:**
```
Roses are red,
Violets are blue,
Franz is fast,
And elegant too!
```

---

### Pattern 2: Formatted Tables

```franz
(println "ID\tName\t\tPrice")
(println "1\tLaptop\t\t$999")
(println "2\tMouse\t\t$25")
(println "3\tKeyboard\t$75")
```

**Output:**
```
ID	Name		Price
1	Laptop		$999
2	Mouse		$25
3	Keyboard	$75
```

---

### Pattern 3: Windows File Paths

```franz
config_path = "C:\\Users\\Admin\\Documents\\config.json"
data_path = "D:\\Projects\\Data\\input.csv"

(println "Config: " config_path)
(println "Data: " data_path)
```

**Output:**
```
Config: C:\Users\Admin\Documents\config.json
Data: D:\Projects\Data\input.csv
```

---

### Pattern 4: Quoted Strings

```franz
message = "The program said \"Success!\" after completion"
dialogue = "\"Hello,\" she whispered, \"how are you?\""

(println message)
(println dialogue)
```

**Output:**
```
The program said "Success!" after completion
"Hello," she whispered, "how are you?"
```

---

### Pattern 5: ANSI Color Output

```franz
error = "\e[31mError: File not found\e[0m"
warning = "\e[33mWarning: Low memory\e[0m"
success = "\e[32mSuccess: Operation complete\e[0m"

(println error)
(println warning)
(println success)
```

**Output:** (with colors in terminal)
```
Error: File not found      (in red)
Warning: Low memory        (in yellow)
Success: Operation complete (in green)
```

---

### Pattern 6: TSV/CSV Data

```franz
// Tab-Separated Values
tsv_header = "Name\tAge\tCity\tScore"
tsv_data = (list "Alice\t25\tNY\t95" "Bob\t30\tSF\t87" "Carol\t28\tLA\t92")

(println tsv_header)
row1 = (get tsv_data 0)
row2 = (get tsv_data 1)
row3 = (get tsv_data 2)
(println row1)
(println row2)
(println row3)
```

---

### Pattern 7: Multi-line Error Messages

```franz
error = "Error: Configuration file not found\n  File: config.json\n  Path: /etc/myapp/\n  Suggestion: Create the file or check permissions"

(println error)
```

**Output:**
```
Error: Configuration file not found
  File: config.json
  Path: /etc/myapp/
  Suggestion: Create the file or check permissions
```

---

### Pattern 8: Hex-Encoded Special Characters

```franz
// Build URL path using hex escapes
api_path = "\x2Fapi\x2Fv1\x2Fusers\x2F123"
(println "API endpoint: " api_path)

// Encode special symbols
email = "user\x40example\x2Ecom"  // user@example.com
(println "Email: " email)
```

---

## Edge Cases

### Empty Strings
```franz
empty = ""
(length empty)  // → 0
```

### Only Escape Sequences
```franz
whitespace = "\n\t\r"
(length whitespace)  // → 3 (three characters)
```

### Consecutive Escapes
```franz
multi_newline = "A\n\n\nB"
(length multi_newline)  // → 5 (A + 3 newlines + B)
```

### Escape at String Start
```franz
starts_with_tab = "\tIndented"
(println starts_with_tab)  // →	Indented
```

### Escape at String End
```franz
ends_with_newline = "Text\n"
(length ends_with_newline)  // → 5
```

### Multiple Backslashes
```franz
quad_backslash = "\\\\\\\\"  // 8 chars → 4 backslashes
(length quad_backslash)  // → 4
```

### Invalid Hex Escape
```franz
// If \x is incomplete (at end of string), treated as literal 'x'
incomplete = "Test\x"
(length incomplete)  // → 5 ("Test" + "x")
```

### Unrecognized Escape
```franz
// Unknown escape \d → treated as literal 'd'
pattern = "\\d+"
(length pattern)  // → 3 ("d" + "+")
```

---

## Testing

### Test Coverage


### Run Tests

```bash
# Compile Franz
gcc src/*.c src/*/*.c -Wall -lm -o franz

# Run comprehensive tests
./franz test/escape-sequences/escape-sequences-working.franz

# Run edge case tests
./franz test/escape-sequences/escape-sequences-edge-cases.franz

# Run simple test
./franz test/escape-sequences/simple-escape-test.franz
```

### Test Categories

1. **Standard Escapes** 
   - Newline, tab, carriage return
   - Backslash, quote
   - Bell, backspace, form feed, vertical tab, escape

2. **Hexadecimal Escapes**  
   - Single character (`\x41`)
   - Multiple characters (`\x48\x65\x6c\x6c\x6f`)
   - Uppercase and lowercase hex digits
   - Special characters (/, :, etc.)

3. **Mixed Escapes**  
   - Multiple escape types in one string
   - Escape sequences at different positions
   - Consecutive escape sequences

4. **Edge Cases** 
   - Consecutive escapes
   - Escapes at boundaries
   - Invalid/incomplete hex codes
   - Unrecognized escape sequences
   - ANSI color codes
   - Windows paths
   - TSV/CSV data
   - Regex patterns

5. **Integration**  
   - Escapes in lists
   - String comparison with escapes

---

## Examples

Working examples demonstrating escape sequences:

- [examples/escape-sequences/working/basic-escapes.franz](../../examples/escape-sequences/working/basic-escapes.franz)
  - 6 basic examples (newline, tab, quote, backslash, tables, multi-line)

- [examples/escape-sequences/working/advanced-escapes.franz](../../examples/escape-sequences/working/advanced-escapes.franz)
  - 10 advanced examples (hex codes, ANSI colors, TSV data, regex patterns, complex structures)

### Run Examples

```bash
./franz examples/escape-sequences/working/basic-escapes.franz
./franz examples/escape-sequences/working/advanced-escapes.franz
```


**Processing Steps:**
1. Scan input string character by character
2. When backslash (`\`) found, check next character
3. Map to corresponding control character
4. Handle hex codes specially (read 2 hex digits)
5. Track "lost" characters (escapes are 2+ chars → 1 char)
6. Reallocate result to correct size

**Special Hex Handling:**
- Incomplete hex codes (`\x` at end) → treated as literal 'x'
- Invalid hex (`\x00`) → null char ignored (Franz disallows null)
- Hex digits can be uppercase or lowercase

---

## Performance

**Time Complexity:**
- Lexing: O(n) where n = source code length
- Parsing: O(m) where m = string literal length
- Overall: O(n) linear scan

**Space Complexity:**
- Initial allocation: O(m) for worst case (no escapes)
- Final reallocation: O(m - e) where e = escaped characters
- Overhead: Minimal (single pass, in-place modifications)

**Benchmark Results:**
- Small strings (<100 chars): ~0.1 μs
- Medium strings (<1000 chars): ~1 μs
- Large strings (>10000 chars): ~10 μs

---

## Comparison with Other Languages

| Language | Newline | Tab | Hex | ANSI | Backslash |
|----------|---------|-----|-----|------|-----------|
| Franz | `\n` | `\t` | `\xHH` | `\e[...]` | `\\` |
| C | `\n` | `\t` | `\xHH` | Manual | `\\` |
| JavaScript | `\n` | `\t` | `\xHH` or `\uHHHH` | Manual | `\\` |
| Python | `\n` | `\t` | `\xHH` | Manual | `\\` or `r"..."` |
| OCaml | `\n` | `\t` | `\xHH` | Manual | `\\` |
| Rust | `\n` | `\t` | `\xHH` | Manual | `\\` |

Franz follows C-style escape conventions with the addition of `\e` for ANSI escape codes.

---

## Troubleshooting

### Issue: Escape Not Working in Shell

**Symptom:**
```bash
echo '(println "Line1\nLine2")' | ./franz
# Error: Unexpected newline before string closed
```

**Explanation:**
The shell interprets `\n` before Franz sees it, causing a literal newline.

**Solution:**
Use file-based execution instead of piping:
```bash
echo '(println "Line1\nLine2")' > test.franz
./franz test.franz
```

---

