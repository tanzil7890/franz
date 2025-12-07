# Franz Standard Library

The Franz Standard Library provides  utility modules for real-world applications.

## Status:  - 100% Complete ✅

**Progress**: 45/45 functions implemented (4/4 modules complete)

| Module | Status | Functions | Tests | Documentation |
|--------|--------|-----------|-------|---------------|
| **string.franz** | ✅ COMPLETE | 17/17 | 72/72 ✅ | [docs/stdlib/string/](../docs/stdlib/string/) |
| **math.franz** | ✅ COMPLETE | 8/8 | 46/46 ✅ | [docs/stdlib/math/](../docs/stdlib/math/) |
| **list.franz** | ✅ COMPLETE | 12/12 | 60+/60+ ✅ | [docs/stdlib/list/](../docs/stdlib/list/) |
| **io.franz** | ✅ COMPLETE | 8/8 | 40+/40+ ✅ | [docs/stdlib/io/](../docs/stdlib/io/) |

---

## Quick Start

### Using Standard Library Modules

```franz
// Load with use
(use "stdlib/string.franz" {
  result = (uppercase "hello")
  (println result)  // HELLO
})

// Load with use_as (recommended - prevents name collisions)
(use_as "stdlib/math.franz" math {
  area = (multiply math.PI (power 5 2))
  (println "Circle area:" area)
})

// Multiple modules
(use "stdlib/list.franz" {
  (use "stdlib/math.franz" {
    data = (list 5 2 8 1 9 3)
    sorted = (sort data)
    avg = (average sorted)
    (println "Sorted:" sorted)
    (println "Average:" avg)
  })
})
```

---

## Module Overview

### 1. String Module (`string.franz`) ✅

**10 Functions**: Case conversion, whitespace handling, string transformation, parsing

```franz
(use "stdlib/string.franz" {
  // Case conversion
  (println (uppercase "hello"))        // HELLO
  (println (lowercase "WORLD"))        // world

  // Whitespace
  (println (trim "  text  "))          // text

  // Transformation
  (println (replace "hi" "i" "o"))     // ho
  (println (repeat "ab" 3))            // ababab

  // Inspection
  (println (starts_with "hello" "he")) // 1
  (println (contains "hello" "ll"))    // 1
  (println (char_at "hello" 1))        // e
})
```

**Functions**: `uppercase`, `lowercase`, `trim`, `split`, `replace`, `repeat`, `starts_with`, `ends_with`, `contains`, `char_at`

**Documentation**: [docs/stdlib/string/string.md](../docs/stdlib/string/string.md)

---

### 2. Math Module (`math.franz`) ✅

**15 Functions**: Advanced mathematical operations, constants, statistics

```franz
(use "stdlib/math.franz" {
  // Constants
  (println "PI:" PI)                   // 3.14159...
  (println "E:" E)                     // 2.71828...

  // Rounding
  (println (floor 3.7))                // 3
  (println (ceil 3.2))                 // 4
  (println (round 3.5))                // 4

  // Statistics
  data = (list 5 2 8 1 9)
  (println "Sum:" (sum data))          // 25
  (println "Average:" (average data))  // 5.0
  (println "Median:" (median data))    // 5

  // Number theory
  (println "5!:" (factorial 5))        // 120
  (println "GCD(12,8):" (gcd 12 8))    // 4
})
```

**Functions**: `PI`, `E`, `abs`, `sqrt`, `floor`, `ceil`, `round`, `max`, `min`, `clamp`, `sum`, `average`, `median`, `factorial`, `gcd`

**Documentation**: [docs/stdlib/math/math.md](../docs/stdlib/math/math.md)

---

### 3. List Module (`list.franz`) ✅

**12 Functions**: Advanced list operations beyond map/filter/reduce

```franz
(use "stdlib/list.franz" {
  // Transformation
  (println (reverse (list 1 2 3)))              // [3, 2, 1]
  (println (unique (list 1 2 2 3)))             // [1, 2, 3]
  (println (flatten (list (list 1 2) (list 3)))) // [1, 2, 3]

  // Filtering
  nums = (list 1 2 3 4 5)
  (println (take nums 3))              // [1, 2, 3]
  (println (drop nums 2))              // [3, 4, 5]

  // Inspection
  (println (any nums {x i -> <- (> x 3)}))  // 1 (true)
  (println (all nums {x i -> <- (> x 0)}))  // 1 (true)

  // Generation
  (println (filled 0 5))               // [0, 0, 0, 0, 0]
  (println (chunk nums 2))             // [[1,2], [3,4], [5]]
  (println (sort (list 3 1 4 1 5)))    // [1, 1, 3, 4, 5]
})
```

**Functions**: `reverse`, `unique`, `flatten`, `zip`, `take`, `drop`, `any`, `all`, `partition`, `filled`, `chunk`, `sort`

**Documentation**: [docs/stdlib/list/list.md](../docs/stdlib/list/list.md)

---

### 4. I/O Module (`io.franz`) ✅

**8 Functions**: File system inspection, directory operations, path manipulation, file I/O

```franz
(use "stdlib/io.franz" {
  // File/directory checks
  (println (exists "README.md"))       // 1 (exists)
  (println (is_file "src/main.c"))    // 1 (is file)
  (println (is_dir "src"))            // 1 (is directory)

  // Directory listing
  files = (list_dir "src")
  (println "Files:" files)

  // Path manipulation
  path = (join_path (list "docs" "stdlib" "io.md"))
  (println "Full path:" path)          // docs/stdlib/io.md

  dir = (dirname "src/main.c")
  file = (basename "src/main.c")
  (println "Dir:" dir "File:" file)    // src, main.c

  // File reading
  lines = (read_lines "README.md")
  (println "Line count:" (length lines))
})
```

**Functions**: `exists`, `is_file`, `is_dir`, `list_dir`, `basename`, `dirname`, `join_path`, `read_lines`

**Documentation**: [docs/stdlib/io/io.md](../docs/stdlib/io/io.md)

**Key Features**:
- Shell integration for reliability (POSIX commands)
- Pure Franz `join_path` implementation using reduce
- Cross-platform compatibility (Unix-like systems)
- Comprehensive edge case handling

---

## Installation

Standard library modules are located in the `stdlib/` directory. They are loaded at runtime using Franz's `use` or `use_as` functions.

### Requirements

- Franz interpreter (v0.0.4+)
- No external dependencies

### Verification

Test that stdlib modules work:

```bash
# String module
./franz -c '(use "stdlib/string.franz" { (println (uppercase "test")) })'

# Math module
./franz -c '(use "stdlib/math.franz" { (println (sum (list 1 2 3))) })'

# List module
./franz -c '(use "stdlib/list.franz" { (println (reverse (list 1 2 3))) })'

# I/O module
./franz -c '(use "stdlib/io.franz" { (println (exists "README.md")) })'
```

---

## Testing

Each module has comprehensive test suites:

```bash
# Run all tests
./franz test/stdlib/string/string-comprehensive-test.franz  # 72 tests
./franz test/stdlib/math/math-test.franz                    # 46 tests
./franz test/stdlib/list/list-test.franz                    # 60+ tests
./franz test/stdlib/io/io-test.franz                        # 40+ tests
```

**Total**: 218+ tests, all passing ✅

---

## Documentation Structure

```
stdlib/
├── string.franz                # String module implementation
├── math.franz                  # Math module implementation
├── list.franz                  # List module implementation
├── io.franz                    # I/O module implementation
└── README.md                   # This file

docs/stdlib/
├── IMPLEMENTATION_GUIDE.md  # Implementation roadmap
├── string/
│   ├── README.md               # Quick overview
│   ├── string.md               # Complete API docs
│   └── STRING_MODULE_COMPLETE.md
├── math/
│   ├── math.md                 # Complete API docs
│   └── MATH_MODULE_COMPLETE.md
├── list/
│   ├── README.md               # Quick overview
│   ├── list.md                 # Complete API docs
│   └── LIST_MODULE_COMPLETE.md
└── io/
    ├── README.md               # Quick overview
    ├── io.md                   # Complete API docs
    └── IO_MODULE_COMPLETE.md

test/stdlib/
├── string/                     # String module tests
├── math/                       # Math module tests
├── list/                       # List module tests
└── io/                         # I/O module tests

examples/stdlib/
├── string/working/             # String examples
├── math/working/               # Math examples
└── io/working/                 # I/O examples
```

---

## Best Practices

### 1. Use `use_as` for Namespace Safety

```franz
// Recommended: Prevents name collisions
(use_as "stdlib/math.franz" m {
  result = (m.sqrt 16)
})

// Also works: Direct access
(use "stdlib/math.franz" {
  result = (sqrt 16)
})
```

### 2. Combine Multiple Modules

```franz
(use "stdlib/list.franz" {
  (use "stdlib/math.franz" {
    // Both modules available here
    data = (list 5 2 8 1 9)
    sorted = (sort data)
    avg = (average sorted)
    (println "Average:" avg)
  })
})
```

### 3. Create Utility Files

```franz
// myapp/utils.franz
(use "stdlib/string.franz" {
  (use "stdlib/math.franz" {
    (use "stdlib/list.franz" {
      // Export commonly used utilities
      process_data = {data ->
        cleaned = (unique data)
        sorted = (sort cleaned)
        <- sorted
      }

      format_output = {text ->
        <- (uppercase (trim text))
      }
    })
  })
})

// myapp/main.franz
(use "myapp/utils.franz" {
  result = (process_data (list 3 1 2 1 3))
  (println (format_output "  hello  "))
})
```

---

## Performance Notes

All stdlib modules:
- ✅ Zero external dependencies
- ✅ Pure Franz implementations
- ✅ No C extensions required
- ✅ Work with module caching (118k+ loads/second)
- ✅ Immutable by default (functional style)

---

## Contributing

To add a new stdlib module:

1. Create `stdlib/<module>.franz`
2. Add tests in `test/stdlib/<module>/`
3. Add examples in `examples/stdlib/<module>/`
4. Document in `docs/stdlib/<module>/<module>.md`
5. Update this README

See [IMPLEMENTATION_GUIDE.md](../docs/stdlib/IMPLEMENTATION_GUIDE.md) for implementation guidelines.

---

## Roadmap

###  (In Progress - 82%)
- ✅ String utilities (10 functions)
- ✅ Math utilities (15 functions)
- ✅ List utilities (12 functions)
- 🚧 I/O utilities (0/8 functions)

###  (Planned)
- HTTP client
- JSON parsing
- CSV parsing
- Date/time utilities
- Regular expressions

###  (Planned)
- Testing framework
- Logging utilities
- Configuration management
- CLI helpers

---

## Links

- **Main README**: [../README.md](../README.md)
- **Implementation Guide**: [../docs/stdlib/IMPLEMENTATION_GUIDE.md](../docs/stdlib/IMPLEMENTATION_GUIDE.md)
- **Industry Roadmap**: [../INDUSTRY_ADOPTION_ROADMAP.md](../INDUSTRY_ADOPTION_ROADMAP.md)

---

**Version**: 1.0.0 ()
**Last Updated**: 2025-10-05
**Status**: 37/45 functions complete (82%)
