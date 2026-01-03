# Monikit Operations That Expand on Teletype Functionality

## Overview

Monikit extends the core Teletype-style scripting with additional operations focused on enhanced pattern manipulation, advanced control flow, and system management. While maintaining compatibility with Teletype's fundamental concepts, Monikit adds significant software-specific functionality.

## Enhanced Pattern Operations

### Advanced Pattern Math Functions

#### P.ADD - Add value to all pattern elements
```rust
"P.ADD" => {
    if start_idx + 1 >= parts.len() {
        return None;
    }
    if let Some((val, consumed)) = eval_expr_fn(parts, start_idx + 1, variables, patterns, counters, scripts, script_index, scale) {
        let working = patterns.working;
        let pattern = &mut patterns.patterns[working];
        for i in 0..pattern.length {
            pattern.data[i] = pattern.data[i].saturating_add(val);
        }
        return Some((val, 1 + consumed));
    }
    None
}
```
```c
static void op_P_ADD_get(const void *NOTUSED(data), scene_state_t *ss,
                         exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t val = cs_pop(cs);
    int16_t pn = ss->variables.p_n;
    int16_t len = ss_get_pattern_len(ss, pn);
    for (int16_t i = 0; i < len; i++) {
        int32_t res = (int32_t)ss_get_pattern_val(ss, pn, i) + val;
        if (res > INT16_MAX) res = INT16_MAX;
        if (res < INT16_MIN) res = INT16_MIN;
        ss_set_pattern_val(ss, pn, i, (int16_t)res);
    }
    cs_push(cs, val);
    tele_pattern_updated();
}
```

#### P.SUB - Subtract value from all pattern elements
```rust
"P.SUB" => {
    if start_idx + 1 >= parts.len() {
        return None;
    }
    if let Some((val, consumed)) = eval_expr_fn(parts, start_idx + 1, variables, patterns, counters, scripts, script_index, scale) {
        let working = patterns.working;
        let pattern = &mut patterns.patterns[working];
        for i in 0..pattern.length {
            pattern.data[i] = pattern.data[i].saturating_sub(val);
        }
        return Some((val, 1 + consumed));
    }
    None
}
```
```c
static void op_P_SUB_get(const void *NOTUSED(data), scene_state_t *ss,
                         exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t val = cs_pop(cs);
    int16_t pn = ss->variables.p_n;
    int16_t len = ss_get_pattern_len(ss, pn);
    for (int16_t i = 0; i < len; i++) {
        int32_t res = (int32_t)ss_get_pattern_val(ss, pn, i) - val;
        if (res > INT16_MAX) res = INT16_MAX;
        if (res < INT16_MIN) res = INT16_MIN;
        ss_set_pattern_val(ss, pn, i, (int16_t)res);
    }
    cs_push(cs, val);
    tele_pattern_updated();
}
```

#### P.MUL - Multiply all pattern elements by value
```rust
"P.MUL" => {
    if start_idx + 1 >= parts.len() {
        return None;
    }
    if let Some((val, consumed)) = eval_expr_fn(parts, start_idx + 1, variables, patterns, counters, scripts, script_index, scale) {
        let working = patterns.working;
        let pattern = &mut patterns.patterns[working];
        for i in 0..pattern.length {
            pattern.data[i] = pattern.data[i].saturating_mul(val);
        }
        return Some((val, 1 + consumed));
    }
    None
}
```
```c
static void op_P_MUL_get(const void *NOTUSED(data), scene_state_t *ss,
                         exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t val = cs_pop(cs);
    int16_t pn = ss->variables.p_n;
    int16_t len = ss_get_pattern_len(ss, pn);
    for (int16_t i = 0; i < len; i++) {
        int32_t res = (int32_t)ss_get_pattern_val(ss, pn, i) * val;
        if (res > INT16_MAX) res = INT16_MAX;
        if (res < INT16_MIN) res = INT16_MIN;
        ss_set_pattern_val(ss, pn, i, (int16_t)res);
    }
    cs_push(cs, val);
    tele_pattern_updated();
}
```

#### P.DIV - Divide all pattern elements by value
```rust
"P.DIV" => {
    if start_idx + 1 >= parts.len() {
        return None;
    }
    if let Some((val, consumed)) = eval_expr_fn(parts, start_idx + 1, variables, patterns, counters, scripts, script_index, scale) {
        if val == 0 {
            return None;
        }
        let working = patterns.working;
        let pattern = &mut patterns.patterns[working];
        for i in 0..pattern.length {
            pattern.data[i] = pattern.data[i] / val;
        }
        return Some((val, 1 + consumed));
    }
    None
}
```
```c
static void op_P_DIV_get(const void *NOTUSED(data), scene_state_t *ss,
                         exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t val = cs_pop(cs);
    if (val == 0) return;
    int16_t pn = ss->variables.p_n;
    int16_t len = ss_get_pattern_len(ss, pn);
    for (int16_t i = 0; i < len; i++) {
        int16_t cur = ss_get_pattern_val(ss, pn, i);
        ss_set_pattern_val(ss, pn, i, cur / val);
    }
    cs_push(cs, val);
    tele_pattern_updated();
}
```

#### P.MOD - Modulo all pattern elements by value
```rust
"P.MOD" => {
    if start_idx + 1 >= parts.len() {
        return None;
    }
    if let Some((val, consumed)) = eval_expr_fn(parts, start_idx + 1, variables, patterns, counters, scripts, script_index, scale) {
        if val == 0 {
            return None;
        }
        let working = patterns.working;
        let pattern = &mut patterns.patterns[working];
        for i in 0..pattern.length {
            pattern.data[i] = pattern.data[i] % val;
        }
        return Some((val, 1 + consumed));
    }
    None
}
```
```c
static void op_P_MOD_get(const void *NOTUSED(data), scene_state_t *ss,
                         exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t val = cs_pop(cs);
    if (val == 0) return;
    int16_t pn = ss->variables.p_n;
    int16_t len = ss_get_pattern_len(ss, pn);
    for (int16_t i = 0; i < len; i++) {
        int16_t cur = ss_get_pattern_val(ss, pn, i);
        ss_set_pattern_val(ss, pn, i, cur % val);
    }
    cs_push(cs, val);
    tele_pattern_updated();
}
```

#### P.SCALE - Scale all pattern elements to range
```rust
"P.SCALE" => {
    if start_idx + 2 >= parts.len() {
        return None;
    }
    if let Some((new_min, consumed1)) = eval_expr_fn(parts, start_idx + 1, variables, patterns, counters, scripts, script_index, scale) {
        if let Some((new_max, consumed2)) = eval_expr_fn(parts, start_idx + 1 + consumed1, variables, patterns, counters, scripts, script_index, scale) {
            if new_min == new_max {
                return None;
            }
            let working = patterns.working;
            let pattern = &mut patterns.patterns[working];
            if pattern.length == 0 {
                return None;
            }
            let old_min = pattern.data[..pattern.length].iter().copied().min().unwrap_or(0);
            let old_max = pattern.data[..pattern.length].iter().copied().max().unwrap_or(0);
            if old_min == old_max {
                for i in 0..pattern.length {
                    pattern.data[i] = new_min;
                }
            } else {
                for i in 0..pattern.length {
                    let old_val = pattern.data[i] as i32;
                    let scaled = ((old_val - old_min as i32) * (new_max as i32 - new_min as i32)) / (old_max as i32 - old_min as i32) + new_min as i32;
                    pattern.data[i] = scaled.clamp(i16::MIN as i32, i16::MAX as i32) as i16;
                }
            }
            let len = pattern.length;
            return Some((len as i16, 1 + consumed1 + consumed2));
        }
    }
    None
}
```
```c
static void op_P_SCALE_get(const void *NOTUSED(data), scene_state_t *ss,
                           exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t new_min = cs_pop(cs);
    int16_t new_max = cs_pop(cs);
    if (new_min == new_max) return;
    int16_t pn = ss->variables.p_n;
    int16_t len = ss_get_pattern_len(ss, pn);
    if (len == 0) return;

    int16_t old_min = INT16_MAX;
    int16_t old_max = INT16_MIN;
    for (int16_t i = 0; i < len; i++) {
        int16_t val = ss_get_pattern_val(ss, pn, i);
        if (val < old_min) old_min = val;
        if (val > old_max) old_max = val;
    }

    for (int16_t i = 0; i < len; i++) {
        int32_t val = (int32_t)ss_get_pattern_val(ss, pn, i);
        if (old_min == old_max) {
            val = new_min;
        } else {
            val = ((val - old_min) * (new_max - new_min)) / (old_max - old_min) + new_min;
        }
        if (val > INT16_MAX) val = INT16_MAX;
        if (val < INT16_MIN) val = INT16_MIN;
        ss_set_pattern_val(ss, pn, i, (int16_t)val);
    }
    cs_push(cs, len);
    tele_pattern_updated();
}
```

### Pattern Analysis Functions

#### P.MIN - Get minimum value
```rust
"P.MIN" => {
    let pattern = &patterns.patterns[patterns.working];
    let slice = &pattern.data[..pattern.length];
    Some((*slice.iter().min().unwrap_or(&0), 1))
}
```
```c
static void op_P_MIN_get(const void *NOTUSED(data), scene_state_t *ss,
                         exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = ss->variables.p_n;
    int16_t len = ss_get_pattern_len(ss, pn);
    if (len == 0) {
        cs_push(cs, 0);
        return;
    }
    int16_t min_val = INT16_MAX;
    for (int16_t i = 0; i < len; i++) {
        int16_t val = ss_get_pattern_val(ss, pn, i);
        if (val < min_val) min_val = val;
    }
    cs_push(cs, min_val);
}
```

#### P.MAX - Get maximum value
```rust
"P.MAX" => {
    let pattern = &patterns.patterns[patterns.working];
    let slice = &pattern.data[..pattern.length];
    Some((*slice.iter().max().unwrap_or(&0), 1))
}
```
```c
static void op_P_MAX_get(const void *NOTUSED(data), scene_state_t *ss,
                         exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = ss->variables.p_n;
    int16_t len = ss_get_pattern_len(ss, pn);
    if (len == 0) {
        cs_push(cs, 0);
        return;
    }
    int16_t max_val = INT16_MIN;
    for (int16_t i = 0; i < len; i++) {
        int16_t val = ss_get_pattern_val(ss, pn, i);
        if (val > max_val) max_val = val;
    }
    cs_push(cs, max_val);
}
```

#### P.SUM - Get sum of all values
```rust
"P.SUM" => {
    let pattern = &patterns.patterns[patterns.working];
    let sum: i16 = pattern.data[..pattern.length].iter().sum();
    Some((sum, 1))
}
```
```c
static void op_P_SUM_get(const void *NOTUSED(data), scene_state_t *ss,
                         exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = ss->variables.p_n;
    int16_t len = ss_get_pattern_len(ss, pn);
    int32_t sum = 0;
    for (int16_t i = 0; i < len; i++) {
        sum += ss_get_pattern_val(ss, pn, i);
    }
    cs_push(cs, (int16_t)sum);
}
```

#### P.AVG - Get average of all values
```rust
"P.AVG" => {
    let pattern = &patterns.patterns[patterns.working];
    if pattern.length > 0 {
        let sum: i32 = pattern.data[..pattern.length].iter().map(|&x| x as i32).sum();
        Some(((sum / pattern.length as i32) as i16, 1))
    } else {
        Some((0, 1))
    }
}
```
```c
static void op_P_AVG_get(const void *NOTUSED(data), scene_state_t *ss,
                         exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = ss->variables.p_n;
    int16_t len = ss_get_pattern_len(ss, pn);
    if (len == 0) {
        cs_push(cs, 0);
        return;
    }
    int32_t sum = 0;
    for (int16_t i = 0; i < len; i++) {
        sum += ss_get_pattern_val(ss, pn, i);
    }
    cs_push(cs, (int16_t)(sum / len));
}
```

#### P.FND - Find index of first occurrence of value
```rust
"P.FND" => {
    if start_idx + 1 >= parts.len() {
        return None;
    }
    if let Some((search_val, consumed)) = eval_expr_fn(parts, start_idx + 1, variables, patterns, counters, scripts, script_index, scale) {
        let pattern = &patterns.patterns[patterns.working];
        let slice = &pattern.data[..pattern.length];
        let result = slice.iter().position(|&x| x == search_val)
            .map(|i| i as i16)
            .unwrap_or(-1);
        return Some((result, 1 + consumed));
    }
    None
}
```
```c
static void op_P_FND_get(const void *NOTUSED(data), scene_state_t *ss,
                         exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t search_val = cs_pop(cs);
    int16_t pn = ss->variables.p_n;
    int16_t len = ss_get_pattern_len(ss, pn);
    for (int16_t i = 0; i < len; i++) {
        if (ss_get_pattern_val(ss, pn, i) == search_val) {
            cs_push(cs, i);
            return;
        }
    }
    cs_push(cs, -1);
}
```

### Pattern Manipulation Functions

#### P.SORT - Sort the pattern
```rust
"P.SORT" => {
    let working = patterns.working;
    let pattern = &mut patterns.patterns[working];
    let len = pattern.length;
    pattern.data[..len].sort();
    Some((len as i16, 1))
}
```
```c
static int compare_int16(const void *a, const void *b) {
    return (*(int16_t *)a - *(int16_t *)b);
}

static void op_P_SORT_get(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t pn = ss->variables.p_n;
    int16_t len = ss_get_pattern_len(ss, pn);
    // Note: Direct pointer access would be needed for qsort,
    // or a manual sort using ss_get/ss_set.
    // Assuming we can access the array directly for sorting:
    qsort(ss->patterns[pn].data, len, sizeof(int16_t), compare_int16);
    cs_push(cs, len);
    tele_pattern_updated();
}
```





## Counter System

### Auto-incrementing Counters
```rust
pub fn eval_counter_expression(
    expr: &str,
    counters: &mut Counters,
) -> Option<(i16, usize)> {
    match expr {
        "N1" => {
            let current = counters.values[0];
            let min = counters.min[0];
            let max = counters.max[0];
            counters.values[0] = if max == 0 {
                current.wrapping_add(1)
            } else {
                let next = current + 1;
                if next > max { min } else { next }
            };
            Some((current, 1))
        }
        // ... (repeated for N2-N4)
    }
}
```
```c
// Implementation assumes counters struct added to scene_state_t
static void op_N_get(const void *data, scene_state_t *ss, exec_state_t *es,
                     command_state_t *cs) {
    uint8_t index = (uint8_t)(intptr_t)data;
    int16_t current = ss->counters.values[index];
    int16_t min = ss->counters.min[index];
    int16_t max = ss->counters.max[index];
    
    int16_t next;
    if (max == 0) {
        next = current + 1; // Standard wrapping behavior
    } else {
        next = current + 1;
        if (next > max) next = min;
    }
    ss->counters.values[index] = next;
    cs_push(cs, current);
}
```

## Logic Extensions

### Additional Logic Functions

#### EITH - Random choice between two values
```rust
"EITH" => {
    // ... (See Rust code above)
}
```
```c
static void op_EITH_get(const void *NOTUSED(data), scene_state_t *ss,
                        exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t a = cs_pop(cs);
    int16_t b = cs_pop(cs);
    random_state_t *r = &ss->rand_states.s.toss.rand; 
    cs_push(cs, (random_next(r) & 1) ? a : b);
}
```

#### TOG - Alternates between values
```rust
"TOG" => {
    // ... (See Rust code above)
}
```
```c
// Requires state storage. 
// Assuming a new toggle state mechanism or reusing an existing variable.
static void op_TOG_get(const void *NOTUSED(data), scene_state_t *ss,
                       exec_state_t *es, command_state_t *cs) {
    int16_t a = cs_pop(cs);
    int16_t b = cs_pop(cs);
    // Hypothetical state access based on call location (hash in Monokit)
    // For Teletype C, we might use a dedicated array indexed by script/line
    // or simply toggle a global flag if no per-instance state exists.
    static uint8_t toggle_state = 0; 
    toggle_state = !toggle_state;
    cs_push(cs, toggle_state ? a : b);
}
```

## Pattern Randomization Operations

### RND.P - Randomize current pattern
Randomizes all elements in the working pattern within the specified range (defaults to 0-127).
```rust
// RND.P [min] [max]
"RND.P" => {
    let (mut min, mut max) = if parts.len() >= 3 {
        // ... evaluates min/max expressions ...
    } else {
        (0, 127)
    };
    let pattern = &mut patterns.patterns[patterns.working];
    for i in 0..pattern.length {
        pattern.data[i] = rng.gen_range(min..=max);
    }
}
```
```c
static void op_RND_P_get(const void *NOTUSED(data), scene_state_t *ss,
                         exec_state_t *es, command_state_t *cs) {
    int16_t min = 0; 
    int16_t max = 127;
    // Argument parsing simplified for example
    if (cs->stack_ptr >= 2) { 
        min = cs_pop(cs);
        max = cs_pop(cs);
    }
    
    int16_t pn = ss->variables.p_n;
    int16_t len = ss_get_pattern_len(ss, pn);
    random_state_t *r = &ss->rand_states.s.pattern.rand;

    for (int16_t i = 0; i < len; i++) {
        ss_set_pattern_val(ss, pn, i, (random_next(r) % (max - min + 1)) + min);
    }
    tele_pattern_updated();
}
```

### RND.PN - Randomize specific pattern
Randomizes all elements in pattern N within the specified range.
```rust
// RND.PN <pat> [min] [max]
"RND.PN" => {
    let pat = eval_pattern_index(parts[1])?;
    let (mut min, mut max) = if parts.len() >= 4 {
        // ... evaluates min/max expressions ...
    } else {
        (0, 127)
    };
    let pattern = &mut patterns.patterns[pat];
    for i in 0..pattern.length {
        pattern.data[i] = rng.gen_range(min..=max);
    }
}
```
```c
static void op_RND_PN_get(const void *NOTUSED(data), scene_state_t *ss,
                          exec_state_t *es, command_state_t *cs) {
    int16_t pn = cs_pop(cs);
    int16_t min = 0; 
    int16_t max = 127;
    // Argument handling...
    
    int16_t len = ss_get_pattern_len(ss, pn);
    random_state_t *r = &ss->rand_states.s.pattern.rand;

    for (int16_t i = 0; i < len; i++) {
        ss_set_pattern_val(ss, pn, i, (random_next(r) % (max - min + 1)) + min);
    }
    tele_pattern_updated();
}
```

### RND.PALL - Randomize all patterns
Randomizes all elements in all 6 patterns within the specified range.
```rust
// RND.PALL [min] [max]
"RND.PALL" => {
    let (mut min, mut max) = if parts.len() >= 3 {
        // ... evaluates min/max expressions ...
    } else {
        (0, 127)
    };
    for pat_idx in 0..6 {
        let pattern = &mut patterns.patterns[pat_idx];
        for i in 0..pattern.length {
            pattern.data[i] = rng.gen_range(min..=max);
        }
    }
}
```
```c
static void op_RND_PALL_get(const void *NOTUSED(data), scene_state_t *ss,
                            exec_state_t *es, command_state_t *cs) {
    int16_t min = 0; 
    int16_t max = 127;
    // Argument handling...
    
    random_state_t *r = &ss->rand_states.s.pattern.rand;

    for (int16_t pn = 0; pn < PATTERN_COUNT; pn++) {
        int16_t len = ss_get_pattern_len(ss, pn);
        for (int16_t i = 0; i < len; i++) {
            ss_set_pattern_val(ss, pn, i, (random_next(r) % (max - min + 1)) + min);
        }
    }
    tele_pattern_updated();
}
```

## SEQ Notation

Monokit introduces a powerful inline sequence notation that allows complex musical patterns to be defined within a single string. The `SEQ` command parses this string and advances through the sequence on each call.

### SEQ Command
Evaluates and advances a sequence string. The sequence state is stored uniquely for each `SEQ` command in a script.

```rust
// Example: SEQ "C3 E3 G3"
// Returns: 0, 4, 7, 0, 4, 7...
```

### Token Types

The sequence string is a space-separated list of tokens.

*   **Notes**: Standard note names (e.g., `C3`, `F#4`, `Bb2`) are converted to semitones relative to C3 (0).
*   **Numbers**: Literal integer values (e.g., `0`, `12`, `-5`).
*   **Rest**: `_` or `.` (Value 0).
*   **Trigger**: `X` (Value 1).
*   **Random Trigger**: `?` (Randomly returns 0 or 1).

### Modifiers & Grouping

*   **Repetition**: Append `*N` to repeat a step `N` times.
    *   `C3*4` -> `C3 C3 C3 C3`
    *   `X*2 _*2` -> `X X _ _`
*   **Alternation**: Enclose options in `< ... >` to cycle through them sequentially.
    *   `<C3 E3>` -> First call `C3`, second call `E3`, third call `C3`...
*   **Random Choice**: Enclose options in `{ ... }` to pick one randomly.
    *   `{C3 E3 G3}` -> Randomly returns one of the notes on each step.

### SEQ Implementation Details
```rust
pub fn eval_seq_expression(
    expr: &str,
    parts: &[&str],
    start_idx: usize,
    patterns: &mut PatternStorage,
    script_index: usize,
) -> Option<(i16, usize)> {
    if expr != "SEQ" {
        return None;
    }

    // SEQ requires a quoted pattern string
    if start_idx + 1 >= parts.len() {
        return None;
    }

    // Extract quoted string (may span multiple parts)
    let (pattern, consumed) = extract_quoted_string(parts, start_idx + 1)?;

    // Parse the pattern
    let steps = parse_seq_pattern(&pattern).ok()?;

    if steps.is_empty() {
        return Some((0, 1 + consumed));
    }

    // Get/create state for this sequence using toggle_state HashMap
    let key = format!("seq_{}_{}", script_index, pattern);

    // Get current step index (drop the mutable reference immediately)
    let step_index = {
        let index = patterns.toggle_state.entry(key.clone()).or_insert(0);
        *index % steps.len()
    };

    let step = &steps[step_index];
    let value = eval_step(step, patterns, script_index, &pattern, step_index);

    // Store the returned value for highlighting
    patterns.toggle_last_value.insert(key.clone(), value);

    // Advance to next step
    let index = patterns.toggle_state.entry(key).or_insert(0);
    *index = (*index).wrapping_add(1);

    Some((value, 1 + consumed))
}
```

### Token Parsing Logic
```rust
fn parse_single_token(token: &str) -> Result<SeqStep, String> {
    let upper = token.to_uppercase();
    match upper.as_str() {
        "_" | "." => Ok(SeqStep::Rest),
        "X" => Ok(SeqStep::Value(1)),
        "?" => Ok(SeqStep::RandomTrigger),
        s => {
            if let Some(semitones) = parse_note_name(s) {
                Ok(SeqStep::Value(semitones))
            } else if let Ok(num) = s.parse::<i16>() {
                Ok(SeqStep::Value(num))
            } else {
                Err(format!("Invalid sequence token: {}", token))
            }
        }
    }
}
```
```c
/*
 * C Implementation Note:
 * SEQ requires parsing a string literal at runtime or compile time.
 * In Teletype C, this would likely involve:
 * 1. Storing sequence state (current index) in a new struct in scene_state_t.
 * 2. A parser function that advances through the string token by token.
 */

typedef struct {
    uint8_t index;
    // ... potentially other state like loop counters
} seq_state_t;

// Hypothetical Op
static void op_SEQ_get(const void *NOTUSED(data), scene_state_t *ss,
                       exec_state_t *es, command_state_t *cs) {
    // 1. Get the sequence string from the command arguments (if stored)
    //    or parse the next string literal in the script.
    // 2. Retrieve state for this specific SEQ instance (e.g. by script line).
    // 3. Parse current token at state->index.
    // 4. Handle modifiers (*, <>, {}) and advance state->index.
    // 5. Push result to stack.
}
```

## Preset System

### Script Presets
- `PSET <script> <name>` - Load preset into script
- `PSET.SAVE <script> <name>` - Save script as preset
- `PSET.DEL <name>` - Delete user preset
- `PSETS` - List all presets

## Key Differences from Teletype

1. **Enhanced Pattern Math**: Direct mathematical operations on entire patterns
2. **SEQ Notation**: Rich inline sequence notation with musical features
3. **Preset System**: Modular script component management
4. **Counter System**: Auto-incrementing counters with bounds
5. **Logic Extensions**: Additional logic functions like EITH and TOG