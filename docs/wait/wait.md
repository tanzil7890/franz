# wait - Pause Execution

## Overview

The `wait` function pauses program execution for a specified duration in seconds. Useful for animations, rate limiting, delays, and time-based control flow.

## Syntax

```franz
(wait duration)
```

**Parameters:**
- `duration` - `float` or `integer` - Number of seconds to wait

**Returns:** `void`

## How It Works

Uses the C standard library `usleep()` function for microsecond precision delays:

```c
Generic *StdLib_wait(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);

  enum Type allowedTypes[] = {TYPE_INT, TYPE_FLOAT};
  validateType(allowedTypes, 2, args[0]->type, 1, lineNumber, "wait");

  double seconds = (args[0]->type == TYPE_INT)
    ? (double) *((int *) args[0]->p_val)
    : *((double *) args[0]->p_val);

  usleep((int) (seconds * 1000000));  // Convert to microseconds

  return Generic_new(TYPE_VOID, NULL, 0);
}
```

## Examples

### Basic Delay

```franz
(println "Starting...")
(wait 1)  // Wait 1 second
(println "Done!")

// Output:
// Starting...
// (1 second pause)
// Done!
```

### Animation Loop

```franz
// Animated countdown
(loop 5 {i ->
  remaining = (subtract 4 i)
  (println remaining "...")
  (wait 1)
})
(println "Blast off!")

// Output:
// 4...
// (1 second)
// 3...
// (1 second)
// 2...
// (1 second)
// 1...
// (1 second)
// 0...
// (1 second)
// Blast off!
```

### Progress Indicator

```franz
// Simple progress bar
show_progress = {total ->
  (loop total {i ->
    percent = (divide (multiply i 100) total)
    (print "\rProgress: " (integer percent) "%")
    (wait 0.1)
  })
  (println "\rProgress: 100% - Complete!")
}

(show_progress 20)
```

### Rate Limiting

```franz
// Process items with delay between each
process_with_delay = {items delay ->
  results = (list)

  (loop (length items) {i ->
    item = (get items i)
    result = (process_item item)
    results = (insert results result)

    (if (less_than i (subtract (length items) 1)) {
      (wait delay)  // Wait between items
    })
  })

  <- results
}

items = (list "a" "b" "c" "d")
results = (process_with_delay items 0.5)  // 500ms between items
```

### Blinking Effect

```franz
// Blink text on/off
blink = {text times ->
  (loop times {i ->
    (print "\r" text)
    (wait 0.5)
    (print "\r" (join (map (range (length text)) {x i -> <- " "})))
    (wait 0.5)
  })
}

(blink "ALERT!" 5)
```

### Timeout with Retry

```franz
// Retry operation with exponential backoff
retry_with_backoff = {fn max_attempts ->
  attempt = 0

  (until void {state i ->
    attempt = (add attempt 1)

    result = (fn)

    (if (is result void) {
      (if (less_than attempt max_attempts) {
        backoff = (power 2 attempt)  // 2^attempt seconds
        (println "Retry" attempt "after" backoff "seconds...")
        (wait backoff)
        <- "retry"
      } {
        (println "Failed after" max_attempts "attempts")
        <- void
      })
    } {
      <- void
    })
  })
}
```

### Frame-Rate Control

```franz
// Maintain consistent frame rate
fps = 30
frame_duration = (divide 1 fps)

render_loop = {frames ->
  (loop frames {i ->
    start = (time)

    // Render frame
    (println "Frame" i)

    // Wait for remainder of frame time
    elapsed = (subtract (time) start)
    remaining = (subtract frame_duration elapsed)

    (if (greater_than remaining 0) {
      (wait remaining)
    })
  })
}

(render_loop 90)  // 90 frames at 30 fps = 3 seconds
```

### Loading Animation

```franz
// Spinner animation
spinner = {duration ->
  frames = (list "|" "/" "-" "\\")
  frame_count = (length frames)
  start = (time)

  (until void {state i ->
    elapsed = (subtract (time) start)

    (if (less_than elapsed duration) {
      frame_idx = (remainder i frame_count)
      spinner_char = (get frames frame_idx)
      (print "\r" spinner_char " Loading...")
      (wait 0.1)
      <- "continue"
    } {
      (println "\r✓ Done!     ")
      <- void
    })
  })
}

(spinner 3)  // 3 second loading animation
```

## Precision

- **Resolution:** Microsecond precision (0.000001 seconds)
- **Minimum Wait:** Platform-dependent (typically ~1ms)
- **Maximum Wait:** Limited by `int` range in microseconds (~2147 seconds)

**Examples:**
```franz
(wait 0.001)    // 1 millisecond
(wait 0.0001)   // 100 microseconds
(wait 1)        // 1 second
(wait 60)       // 1 minute
```

## Use Cases

### 1. Animation Timing
```franz
// Smooth animation
(loop 100 {i ->
  (print "\rProgress: " i "%")
  (wait 0.05)  // 50ms per frame
})
```

### 2. Rate Limiting
```franz
// API calls with rate limit
(loop (length requests) {i ->
  (make_api_call (get requests i))
  (wait 1)  // 1 request per second
})
```

### 3. User Experience
```franz
// Natural dialog timing
(println "Processing your request...")
(wait 0.5)
(println "Analyzing data...")
(wait 1)
(println "Complete!")
```

### 4. Polling
```franz
// Poll until condition met
(until "ready" {state i ->
  status = (check_status)
  (if (is status "ready") {
    <- "ready"
  } {
    (wait 0.5)  // Poll every 500ms
    <- "waiting"
  })
})
```

### 5. Debouncing
```franz
// Simple debounce
debounce = {fn delay ->
  last_call = (time)

  <- {args ->
    now = (time)
    since_last = (subtract now last_call)

    (if (greater_than since_last delay) {
      last_call = now
      <- (fn args)
    } {
      <- void
    })
  }
}
```

## Common Patterns

### Sleep Function (Alias)
```franz
sleep = {seconds -> (wait seconds)}

(sleep 2)  // Wait 2 seconds
```

### Delay Between Actions
```franz
actions = (list
  {-> (println "First")}
  {-> (println "Second")}
  {-> (println "Third")}
)

(loop (length actions) {i ->
  action = (get actions i)
  (action)
  (if (less_than i (subtract (length actions) 1)) {
    (wait 1)
  })
})
```

### Timed Sequence
```franz
sequence = {steps ->
  (loop (length steps) {i ->
    step = (get steps i)
    message = (dict_get step "message")
    duration = (dict_get step "duration")

    (println message)
    (wait duration)
  })
}

(sequence (list
  (dict "message" "Step 1" "duration" 1)
  (dict "message" "Step 2" "duration" 2)
  (dict "message" "Step 3" "duration" 0.5)
))
```

### Heartbeat
```franz
// Periodic heartbeat
heartbeat = {interval count ->
  (loop count {i ->
    (println "♥ heartbeat" i)
    (wait interval)
  })
}

(heartbeat 1 10)  // 10 heartbeats, 1 second apart
```

## Comparison with Other Languages

| Language | Function | Franz Equivalent |
|----------|----------|-----------------|
| **Python** | `time.sleep(seconds)` | `(wait seconds)` |
| **JavaScript** | `await sleep(ms)` | `(wait (divide ms 1000))` |
| **Ruby** | `sleep(seconds)` | `(wait seconds)` |
| **Go** | `time.Sleep(duration)` | `(wait seconds)` |
| **Rust** | `thread::sleep(duration)` | `(wait seconds)` |

## Related Functions

- [`time`](../time/time.md) - Get current time for timing operations
- `until` - Loop until condition met (can include waits)
- `loop` - Fixed iteration loops (can include waits)

## Best Practices

1. **Convert Milliseconds to Seconds**
   ```franz
   ms_to_seconds = {ms -> <- (divide ms 1000)}

   (wait (ms_to_seconds 500))  // Wait 500ms
   ```

2. **Validate Wait Duration**
   ```franz
   safe_wait = {duration ->
     (if (greater_than duration 0) {
       (wait duration)
     } {
       (println "Warning: Invalid wait duration" duration)
     })
   }
   ```

3. **Combine with Timing**
   ```franz
   // Ensure minimum time elapsed
   start = (time)
   (do_work)
   elapsed = (subtract (time) start)

   min_duration = 1
   (if (less_than elapsed min_duration) {
     (wait (subtract min_duration elapsed))
   })
   ```

4. **Feedback During Long Waits**
   ```franz
   wait_with_progress = {duration ->
     steps = 10
     step_duration = (divide duration steps)

     (loop steps {i ->
       (print ".")
       (wait step_duration)
     })
     (println "")
   }

   (println "Please wait")
   (wait_with_progress 5)  // 5 second wait with dots
   ```
