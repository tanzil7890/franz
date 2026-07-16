# time - Get Current Time

## Overview

The `time` function returns the current time in seconds since program start as a floating-point number. Useful for benchmarking, performance measurement, and timing operations.


## Syntax

```franz
(time)
```

**Parameters:** None

**Returns:** `float` - Seconds elapsed since program start

## How It Works

Uses the C standard library `clock()` function to measure CPU time:

```c
Generic *StdLib_time(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(0, 0, length, lineNumber);

  double time_seconds = ((double) clock()) / ((double) CLOCKS_PER_SEC);
  double *p_time = malloc(sizeof(double));
  *p_time = time_seconds;

  return Generic_new(TYPE_FLOAT, p_time, lineNumber);
}
```

## Examples

### Basic Timing

```franz
start = (time)
(println "Current time:" start "seconds")

// Output: Current time: 0.000123 seconds
```

### Performance Benchmarking

```franz
// Measure execution time of a block
start_time = (time)

(loop 1000 {i ->
  x = (multiply i i)
})

end_time = (time)
elapsed = (subtract end_time start_time)
(println "1000 iterations took" elapsed "seconds")

// Output: 1000 iterations took 0.001234 seconds
```

### Function Timing

```franz
// Time a specific function
time_function = {fn ->
  start = (time)
  result = (fn)
  end = (time)
  elapsed = (subtract end start)

  (println "Function took" elapsed "seconds")
  <- result
}

// Usage
(time_function {->
  (loop 10000 {i -> x = (add i 1)})
})
```

### Compare Algorithms

```franz
// Compare two approaches
approach_a = {->
  sum = 0
  (loop 1000 {i ->
    sum = (add sum i)
  })
  <- sum
}

approach_b = {->
  <- (reduce (range 1000) {acc x i -> <- (add acc x)} 0)
}

// Time approach A
start_a = (time)
result_a = (approach_a)
time_a = (subtract (time) start_a)

// Time approach B
start_b = (time)
result_b = (approach_b)
time_b = (subtract (time) start_b)

(println "Approach A:" time_a "seconds")
(println "Approach B:" time_b "seconds")
```

### Rate Limiting

```franz
// Process items with rate limit check
process_with_limit = {items max_time ->
  start = (time)
  processed = (list)

  (loop (length items) {i ->
    elapsed = (subtract (time) start)

    (if (less_than elapsed max_time) {
      item = (get items i)
      processed = (insert processed item)
    })
  })

  <- processed
}

items = (range 100)
result = (process_with_limit items 0.01)  // 10ms limit
(println "Processed" (length result) "items in time limit")
```

### Animation Timing

```franz
// Simple animation loop with timing
animate = {frames duration ->
  start = (time)
  frame_time = (divide duration frames)
  current_frame = 0

  (until "done" {state i ->
    elapsed = (subtract (time) start)
    expected_frame = (integer (divide elapsed frame_time))

    (if (less_than expected_frame frames) {
      (if (greater_than expected_frame current_frame) {
        current_frame = expected_frame
        (println "Frame" current_frame)
      })
      <- "running"
    } {
      <- "done"
    })
  })
}

(animate 10 1.0)  // 10 frames over 1 second
```

## Use Cases

### 1. Performance Measurement
```franz
// Measure code execution speed
start = (time)
// ... code to measure ...
duration = (subtract (time) start)
(println "Execution time:" duration "s")
```

### 2. Benchmarking
```franz
// Compare different implementations
implementations = (list impl_a impl_b impl_c)
times = (map implementations {impl i ->
  start = (time)
  (impl)
  <- (subtract (time) start)
})
(println "Times:" times)
```

### 3. Profiling
```franz
// Simple profiler
profile = {name fn ->
  start = (time)
  result = (fn)
  duration = (subtract (time) start)
  (println "[PROFILE]" name ":" duration "s")
  <- result
}

data = (profile "data_load" {-> (read_file "data.txt")})
processed = (profile "processing" {-> (map data process_fn)})
```

### 4. Timeout Detection
```franz
// Execute with timeout
with_timeout = {fn timeout_seconds ->
  start = (time)
  result = void

  (until void {state i ->
    elapsed = (subtract (time) start)

    (if (greater_than elapsed timeout_seconds) {
      (println "TIMEOUT after" timeout_seconds "seconds")
      <- void
    } {
      result = (fn)
      <- void
    })
  })

  <- result
}
```

## Precision

- **Resolution:** Depends on system `CLOCKS_PER_SEC` (typically microsecond precision)
- **Accuracy:** Measures CPU time, not wall-clock time
- **Overhead:** Very low (~1 microsecond per call)

## Comparison with Other Languages

| Language | Function | Franz Equivalent |
|----------|----------|-----------------|
| **Python** | `time.time()` | `(time)` |
| **JavaScript** | `Date.now()` / `performance.now()` | `(time)` |
| **Ruby** | `Time.now` | `(time)` |
| **Go** | `time.Now()` | `(time)` |
| **Rust** | `Instant::now()` | `(time)` |

## Common Patterns

### Timer Helper Function
```franz
timer = {->
  start = (time)
  <- {->
    <- (subtract (time) start)
  }
}

// Usage
t = (timer)
// ... do work ...
(println "Elapsed:" (t) "seconds")
```

### Stopwatch
```franz
stopwatch = {->
  start = (time)
  laps = (list)

  <- (dict
    "lap" {->
      current = (time)
      lap_time = (subtract current start)
      laps = (insert laps lap_time)
      <- lap_time
    }
    "total" {->
      <- (subtract (time) start)
    }
    "laps" {->
      <- laps
    }
  )
}

// Usage
sw = (stopwatch)
(dict_get sw "lap")   // Lap 1
(dict_get sw "lap")   // Lap 2
(println "Total:" (dict_get sw "total"))
```

### Rate Calculator
```franz
calculate_rate = {count duration ->
  <- (divide count duration)
}

start = (time)
(loop 10000 {i -> x = (add i 1)})
duration = (subtract (time) start)

rate = (calculate_rate 10000 duration)
(println "Rate:" (integer rate) "ops/sec")
```

## Related Functions

- [`wait`](../wait/wait.md) - Pause execution for specified duration
- `print` / `println` - Display timing results
- `subtract` - Calculate time differences
- `divide` - Calculate rates

## Best Practices

1. **Measure Multiple Runs**
   ```franz
   // Get average time
   runs = 10
   total_time = 0

   (loop runs {i ->
     start = (time)
     (my_function)
     total_time = (add total_time (subtract (time) start))
   })

   avg_time = (divide total_time runs)
   (println "Average time:" avg_time "seconds")
   ```

2. **Warm Up Before Benchmarking**
   ```franz
   // Warm up (exclude from measurement)
   (loop 100 {i -> (my_function)})

   // Now measure
   start = (time)
   (loop 1000 {i -> (my_function)})
   duration = (subtract (time) start)
   ```

3. **Report Human-Readable Times**
   ```franz
   format_time = {seconds ->
     <- (if (less_than seconds 0.001) {
       <- (join (multiply seconds 1000000) "μs")
     } (less_than seconds 1) {
       <- (join (multiply seconds 1000) "ms")
     } {
       <- (join seconds "s")
     })
   }

   duration = (subtract (time) start)
   (println "Duration:" (format_time duration))
   ```

