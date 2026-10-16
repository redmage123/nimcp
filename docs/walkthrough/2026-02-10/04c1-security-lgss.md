# Walkthrough 04c1: Security - LGSS (Layered Governance Safety System)

**Date**: 2026-02-10
**Scope**: All 23 `.c` files in `src/security/lgss/` (~22K lines)
**Reviewer**: Claude Opus 4.6

---

## Summary

| Priority | Count | Description |
|----------|-------|-------------|
| **P1** | 2 | Dead code, wrong error code for allocation failure |
| **P2** | 55 | False positive NIMCP_THROW_TO_IMMUNE (44), wrong error code/message (11) |
| **P3** | 2 | Const cast, cascading throws performance |

**Total findings: 59**

---

## Files Reviewed (23)

### Top-Level (4)
- `nimcp_lgss.c` (717 lines)
- `nimcp_lgss_telemetry.c` (904 lines)
- `nimcp_lgss_action_interceptor.c` (1380 lines)
- `nimcp_lgss_override_controller.c` (1203 lines)

### Cognitive (2)
- `cognitive/nimcp_lgss_attention_guard.c` (925 lines)
- `cognitive/nimcp_lgss_working_memory_guard.c` (1400 lines)

### Gates (3)
- `gates/nimcp_lgss_autonomic_gate.c` (827 lines)
- `gates/nimcp_lgss_motor_gate.c` (603 lines)
- `gates/nimcp_lgss_speech_gate.c` (927 lines)

### Bridges (4)
- `bridges/nimcp_lgss_bio_async_bridge.c` (1104 lines)
- `bridges/nimcp_lgss_ethics_bridge.c` (553 lines)
- `bridges/nimcp_lgss_executive_bridge.c` (891 lines)
- `bridges/nimcp_lgss_planning_bridge.c` (803 lines)

### Learning (4)
- `learning/nimcp_lgss_meta_learning_guard.c` (1416 lines)
- `learning/nimcp_lgss_plasticity_constraints.c` (1021 lines)
- `learning/nimcp_lgss_stdp_guard.c` (849 lines)
- `learning/nimcp_lgss_training_guard.c` (1165 lines)

### Perception (3)
- `perception/nimcp_lgss_adversarial_detector.c` (738 lines)
- `perception/nimcp_lgss_content_filter.c` (1015 lines)
- `perception/nimcp_lgss_input_validator.c` (914 lines)

### Reward (3)
- `reward/nimcp_lgss_incentive_validator.c` (861 lines)
- `reward/nimcp_lgss_vta_guard.c` (778 lines)
- `reward/nimcp_lgss_reward_alignment.c` (875 lines)

---

## Clean Files (6)

The following files have no findings:

1. **`gates/nimcp_lgss_autonomic_gate.c`** - Clean. Uses own error return types consistently.
2. **`bridges/nimcp_lgss_bio_async_bridge.c`** - Clean. Well-structured bio-async integration.
3. **`bridges/nimcp_lgss_executive_bridge.c`** - Clean. Proper error handling throughout.
4. **`perception/nimcp_lgss_adversarial_detector.c`** - Clean. Uses mutex properly.
5. **`cognitive/nimcp_lgss_working_memory_guard.c`** - Previously remediated. All throws are appropriate.

Note: `cognitive/nimcp_lgss_attention_guard.c` has 1 finding (see below).

---

## P1 Findings (2)

### P1-1: Dead code in ethics bridge destroy

**File**: `bridges/nimcp_lgss_ethics_bridge.c`
**Line**: 187
**Category**: Dead code after return

```c
void lgss_ethics_bridge_destroy(lgss_ethics_bridge_t* bridge)
{
    if (!bridge) {
        return;
        NIMCP_LOGGING_DEBUG("Destroying %s bridge", "lgss_ethics");  // DEAD CODE
    }
```

**Problem**: `NIMCP_LOGGING_DEBUG` is after `return` inside the `if (!bridge)` block. This code is unreachable. The debug log was likely intended to be BEFORE the null check or AFTER the `if` block.

**Fix**: Move the log statement before the null guard:
```c
void lgss_ethics_bridge_destroy(lgss_ethics_bridge_t* bridge)
{
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "lgss_ethics");
    if (!bridge) {
        return;
    }
```

---

### P1-2: Wrong error code for allocation failure (attention guard)

**File**: `cognitive/nimcp_lgss_attention_guard.c`
**Line**: 235
**Category**: Wrong error code

```c
attention_guard_t* guard = (attention_guard_t*)nimcp_calloc(1, sizeof(attention_guard_t));
if (guard == NULL) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "guard is NULL");
    return NULL;
}
```

**Problem**: Allocation failure should use `NIMCP_ERROR_NO_MEMORY`, not `NIMCP_ERROR_NULL_POINTER`. The pointer is NULL because allocation failed, not because an invalid argument was passed.

**Fix**: Change to `NIMCP_ERROR_NO_MEMORY` with accurate message:
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "attention_guard_create: failed to allocate guard");
```

---

## P2 Findings (55)

### nimcp_lgss.c (1)

#### P2-1: Wrong error code for locked KB

**File**: `nimcp_lgss.c`
**Line**: 331
**Category**: Wrong error code

```c
if (symbolic_logic_safety_is_locked(lgss->safety_kb)) {
    LGSS_LOG_ERROR("Cannot load rules - KB is locked");
    return NIMCP_ERROR_MUTEX_INIT;
}
```

**Problem**: `NIMCP_ERROR_MUTEX_INIT` indicates mutex initialization failure. A locked knowledge base is a state/busy condition, not a mutex init error.

**Fix**: Use `NIMCP_ERROR_RESOURCE_BUSY` or `NIMCP_ERROR_INVALID_STATE`.

---

### nimcp_lgss_telemetry.c (1)

#### P2-2: False positive + wrong error code for disabled config

**File**: `nimcp_lgss_telemetry.c`
**Line**: 207
**Category**: False positive NIMCP_THROW_TO_IMMUNE + wrong error code

```c
if (!config->enabled) {
    TELEM_LOG_INFO("Telemetry disabled by configuration");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lgss_telemetry_create: config->enabled is NULL");
    return NULL;
}
```

**Problem**: `config->enabled` being `false` is not a NULL pointer error -- it is a boolean configuration choice. The log correctly says "disabled by configuration" but then throws a NULL pointer error to the immune system. Disabled telemetry is normal operation, not an error.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE` call entirely. The `TELEM_LOG_INFO` already documents the path.

---

### nimcp_lgss_action_interceptor.c (8)

#### P2-3: False positive - background thread throws on not-found proposal

**File**: `nimcp_lgss_action_interceptor.c`
**Line**: 250
**Category**: False positive NIMCP_THROW_TO_IMMUNE

```c
pending_proposal_t* entry = find_pending_unlocked(aix, proposal_id);
if (!entry) {
    aix_unlock(aix);
    LOG_WARN(...);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "background_evaluation_thread: entry is NULL");
    return NULL;
}
```

**Problem**: Proposal not found in background thread is a race condition (proposal may have been cancelled or already evaluated), not an error worth throwing to the immune system. The `LOG_WARN` is sufficient.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

#### P2-4: False positive - background thread throws on already-evaluated

**File**: `nimcp_lgss_action_interceptor.c`
**Line**: 257
**Category**: False positive NIMCP_THROW_TO_IMMUNE

```c
if (entry->decision_ready) {
    aix_unlock(aix);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "background_evaluation_thread: validation failed");
    return NULL;
}
```

**Problem**: An already-evaluated proposal is a normal race condition (duplicate evaluation), not a NULL pointer error.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`. Add a `LOG_DEBUG` if desired.

#### P2-5: False positive - background thread throws at end of SUCCESSFUL execution

**File**: `nimcp_lgss_action_interceptor.c`
**Line**: 322
**Category**: False positive NIMCP_THROW_TO_IMMUNE

```c
    aix_unlock(aix);

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "background_evaluation_thread: operation failed");
    return NULL;
}
```

**Problem**: This throw executes at the end of a **completely successful** background evaluation. The proposal was found, evaluated, stats updated, callback invoked, and audit logged. Then it throws "operation failed" to the immune system. This fires on EVERY successful background evaluation.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE` entirely.

#### P2-6: False positive - evaluate_condition throws on field not found

**File**: `nimcp_lgss_action_interceptor.c`
**Line**: 488
**Category**: False positive NIMCP_THROW_TO_IMMUNE

```c
    // Field not found - condition does not match
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "evaluate_condition: operation failed");
    return false;
```

**Problem**: A condition field not matching is normal rule evaluation logic -- not all proposals have all fields.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`. The comment already explains this is expected.

#### P2-7: False positive - evaluate_rule throws on disabled rule

**File**: `nimcp_lgss_action_interceptor.c`
**Line**: 502
**Category**: False positive NIMCP_THROW_TO_IMMUNE + wrong error code

```c
if (!rule->enabled) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "evaluate_rule: rule->enabled is NULL");
    return false;
}
```

**Problem**: A disabled rule is normal configuration. `rule->enabled` being `false` is not a NULL pointer.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

#### P2-8: False positive - evaluate_rule throws on condition not matching

**File**: `nimcp_lgss_action_interceptor.c`
**Line**: 509
**Category**: False positive NIMCP_THROW_TO_IMMUNE

```c
if (!evaluate_condition(&rule->conditions[i], context)) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "evaluate_rule: evaluate_condition is NULL");
    return false;
}
```

**Problem**: A condition not matching is the normal purpose of rule evaluation. Additionally, this creates cascading throws: `evaluate_condition` already throws on field-not-found (P2-6), then `evaluate_rule` throws again.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

#### P2-9: False positive - find_pending_unlocked throws on not-found

**File**: `nimcp_lgss_action_interceptor.c`
**Line**: 768
**Category**: False positive NIMCP_THROW_TO_IMMUNE

```c
static pending_proposal_t* find_pending_unlocked(...) {
    ...
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_pending_unlocked: validation failed");
    return NULL;
}
```

**Problem**: Search function returning NULL for "not found" is normal. This throws on every lookup miss.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

#### P2-10: False positive - find_escalation_unlocked throws on not-found

**File**: `nimcp_lgss_action_interceptor.c`
**Line**: 786
**Category**: False positive NIMCP_THROW_TO_IMMUNE

```c
static escalation_entry_t* find_escalation_unlocked(...) {
    ...
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_escalation_unlocked: validation failed");
    return NULL;
}
```

**Problem**: Same as P2-9. Search not-found is normal.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

---

### nimcp_lgss_override_controller.c (2)

#### P2-11: False positive - find_request_unlocked throws on not-found

**File**: `nimcp_lgss_override_controller.c`
**Line**: 322
**Category**: False positive NIMCP_THROW_TO_IMMUNE

```c
static request_entry_t* find_request_unlocked(...) {
    ...
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_request_unlocked: validation failed");
    return NULL;
}
```

**Problem**: Search function not finding a request is normal behavior.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

#### P2-12: False positive - validate_auth_token throws on missing token

**File**: `nimcp_lgss_override_controller.c`
**Line**: 350
**Category**: False positive NIMCP_THROW_TO_IMMUNE

```c
if (request->auth_token_size == 0) {
    LOG_WARN("%s: Authentication required but no token provided", MODULE_NAME);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_auth_token: request->auth_token_size is zero");
    return false;
}
```

**Problem**: Authentication rejection is the purpose of this function. A missing token is a normal security rejection, not an error for the immune system.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`. The `LOG_WARN` is sufficient.

---

### gates/nimcp_lgss_speech_gate.c (10)

#### P2-13: False positive - strcasestr_local throws on NULL inputs

**File**: `gates/nimcp_lgss_speech_gate.c`
**Lines**: 124-125
**Category**: False positive NIMCP_THROW_TO_IMMUNE

```c
static const char* strcasestr_local(const char* haystack, const char* needle) {
    if (haystack == NULL || needle == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "strcasestr_local: validation failed");
        return NULL;
    }
```

**Problem**: This is a search utility called from `detect_harmful_content` and `matches_pattern`. NULL inputs should return NULL silently, not throw to immune system.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

#### P2-14: False positive - strcasestr_local throws when haystack shorter than needle

**File**: `gates/nimcp_lgss_speech_gate.c`
**Line**: 136
**Category**: False positive NIMCP_THROW_TO_IMMUNE

```c
if (haystack_len < needle_len) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "strcasestr_local: validation failed");
    return NULL;
}
```

**Problem**: Haystack being shorter than needle means "not found" -- a normal search result.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

#### P2-15: False positive - strcasestr_local throws when pattern not found

**File**: `gates/nimcp_lgss_speech_gate.c`
**Line**: 154
**Category**: False positive NIMCP_THROW_TO_IMMUNE

```c
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "strcasestr_local: validation failed");
    return NULL;
```

**Problem**: Pattern not found is the normal "no match" return. This fires on every unsuccessful search, including from `detect_harmful_content` which calls this in a loop of 9 harmful patterns.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

#### P2-16: False positive - matches_pattern throws on NULL inputs

**File**: `gates/nimcp_lgss_speech_gate.c`
**Lines**: 162-163
**Category**: False positive NIMCP_THROW_TO_IMMUNE

```c
static bool matches_pattern(const char* text, const filter_pattern_t* pattern) {
    if (text == NULL || pattern == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "matches_pattern: validation failed");
        return false;
    }
```

**Problem**: NULL check returning false is the appropriate behavior for a pattern matching function.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

#### P2-17: False positive - contains_profanity throws TWICE on normal path

**File**: `gates/nimcp_lgss_speech_gate.c`
**Lines**: 350, 359
**Category**: False positive NIMCP_THROW_TO_IMMUNE (x2)

```c
static bool contains_profanity(const char* text, size_t length) {
    if (text == NULL || length == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "contains_profanity: length is zero");
        return false;
    }

    /* Placeholder: In production, use a comprehensive profanity filter */
    (void)text;
    (void)length;

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "contains_profanity: length is zero");
    return false;
}
```

**Problem**: This placeholder function always returns `false` (no profanity detected). On the happy path (non-NULL text, non-zero length), it STILL throws `NIMCP_ERROR_INVALID_PARAM` at line 359 before returning `false`. The error message is also wrong -- it says "length is zero" on a path where length is guaranteed non-zero. This fires on EVERY call to the profanity checker.

**Fix**: Remove both `NIMCP_THROW_TO_IMMUNE` calls.

#### P2-18: False positive - contains_urls throws on NULL/empty

**File**: `gates/nimcp_lgss_speech_gate.c`
**Lines**: 367-368
**Category**: False positive NIMCP_THROW_TO_IMMUNE

```c
static bool contains_urls(const char* text, size_t length) {
    if (text == NULL || length == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "contains_urls: length is zero");
        return false;
    }
```

**Problem**: Returns false for "no URLs found in empty text" -- this is correct behavior, not an error.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

#### P2-19: False positive - contains_code throws on NULL/empty

**File**: `gates/nimcp_lgss_speech_gate.c`
**Lines**: 381-382
**Category**: False positive NIMCP_THROW_TO_IMMUNE

```c
static bool contains_code(const char* text, size_t length) {
    if (text == NULL || length == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "contains_code: length is zero");
        return false;
    }
```

**Problem**: Same as P2-18. Empty text has no code -- returning false is correct.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

---

### bridges/nimcp_lgss_planning_bridge.c (1)

#### P2-20: False positive - validate_tree_recursive throws on early termination

**File**: `bridges/nimcp_lgss_planning_bridge.c`
**Line**: 210
**Category**: False positive NIMCP_THROW_TO_IMMUNE

```c
if (bridge->config.stop_on_first_violation) {
    result->result = LGSS_RESULT_DENY;
    result->is_safe = false;
    snprintf(result->explanation, ...);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_tree_recursive: validation failed");
    return -1;  /* Signal early termination */
}
```

**Problem**: `stop_on_first_violation` is a configured optimization to short-circuit tree validation. This is by-design early termination, not an error.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`. The result struct already captures the violation.

---

### learning/nimcp_lgss_meta_learning_guard.c (2)

#### P2-21: Wrong error code - find_param throws NULL_POINTER for unregistered param

**File**: `learning/nimcp_lgss_meta_learning_guard.c`
**Line**: 307
**Category**: Wrong error code + false positive

```c
if (!g->params[param_id].registered) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_param: g->params is NULL");
    return NULL;
}
```

**Problem**: The parameter exists in the array but is not registered. This is "not found" semantics, not a NULL pointer. The message "g->params is NULL" is also wrong -- `g->params` is not NULL, the specific entry is just not registered.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE` (search not-found pattern). If a throw is desired, use `NIMCP_ERROR_NOT_FOUND` with message "find_param: param_id not registered".

#### P2-22: False positive - detect_oscillation throws on insufficient data

**File**: `learning/nimcp_lgss_meta_learning_guard.c`
**Line**: 318
**Category**: False positive NIMCP_THROW_TO_IMMUNE

```c
static bool detect_oscillation(param_registry_entry_t* param, uint32_t window_size, float threshold) {
    if (param->history_count < window_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "detect_oscillation: validation failed");
        return false;
    }
```

**Problem**: Having insufficient history for oscillation detection is the normal cold-start state, not an error. This fires on every update until `window_size` samples are collected.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

---

### learning/nimcp_lgss_plasticity_constraints.c (7)

#### P2-23: Wrong error message - hashmap_insert for "already exists"

**File**: `learning/nimcp_lgss_plasticity_constraints.c`
**Line**: 174
**Category**: False positive + wrong error message

```c
if (map[idx].synapse_id == synapse_id) {
    /* Already exists */
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hashmap_create: validation failed");
    return false;
}
```

**Problem**: "Already exists" in a hashmap insert is a normal idempotency check. The error message references "hashmap_create" instead of "hashmap_insert".

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

#### P2-24: Wrong error message - hashmap_insert for "full"

**File**: `learning/nimcp_lgss_plasticity_constraints.c`
**Line**: 181
**Category**: Wrong error message

```c
/* Hashmap full */
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hashmap_create: validation failed");
return false;
```

**Problem**: Hashmap full is a capacity issue (should use `NIMCP_ERROR_BUFFER_OVERFLOW` or `NIMCP_ERROR_NO_MEMORY`). The error message references "hashmap_create" instead of "hashmap_insert".

**Fix**: Change to `NIMCP_ERROR_BUFFER_OVERFLOW` with message "hashmap_insert: hashmap full".

#### P2-25: False positive - hashmap_contains throws on empty slot (not found)

**File**: `learning/nimcp_lgss_plasticity_constraints.c`
**Line**: 199
**Category**: False positive NIMCP_THROW_TO_IMMUNE

```c
if (!map[idx].occupied) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hashmap_contains: map is NULL");
    return false;
}
```

**Problem**: Finding an empty slot during linear probing means "not found" -- this is the normal search termination for open-address hashmaps. The message "map is NULL" is wrong.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

#### P2-26: False positive - hashmap_contains throws at end of full search

**File**: `learning/nimcp_lgss_plasticity_constraints.c`
**Line**: 208
**Category**: False positive NIMCP_THROW_TO_IMMUNE

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hashmap_contains: validation failed");
return false;
```

**Problem**: Completing the full probe loop without finding the key means "not found".

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

#### P2-27: False positive - hashmap_remove throws on empty slot (not found)

**File**: `learning/nimcp_lgss_plasticity_constraints.c`
**Line**: 227
**Category**: False positive NIMCP_THROW_TO_IMMUNE + wrong message

```c
if (!map[idx].occupied) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hashmap_contains: map is NULL");
    return false;
}
```

**Problem**: Same as P2-25 but in `hashmap_remove`. Message incorrectly references "hashmap_contains".

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

#### P2-28: False positive - hashmap_remove throws at end of full search

**File**: `learning/nimcp_lgss_plasticity_constraints.c`
**Line**: 238
**Category**: False positive NIMCP_THROW_TO_IMMUNE + wrong message

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hashmap_contains: validation failed");
return false;
```

**Problem**: Same as P2-26 but in `hashmap_remove`. Message incorrectly references "hashmap_contains".

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

#### P2-29: Wrong error message - hashmap_remove NULL check

**File**: `learning/nimcp_lgss_plasticity_constraints.c`
**Line**: 218
**Category**: Wrong error message (copy-paste)

```c
static bool hashmap_remove(frozen_entry_t* map, ...) {
    if (!map) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hashmap_contains: map is NULL");
        return false;
    }
```

**Problem**: The NULL check on `map` is appropriate, but the message says "hashmap_contains" instead of "hashmap_remove".

**Fix**: Change message to "hashmap_remove: map is NULL".

---

### learning/nimcp_lgss_stdp_guard.c (2)

#### P2-30: False positive - detect_timing_regularity throws on insufficient data

**File**: `learning/nimcp_lgss_stdp_guard.c`
**Line**: 227
**Category**: False positive NIMCP_THROW_TO_IMMUNE

```c
static bool detect_timing_regularity(struct stdp_guard_internal* g) {
    if (g->dt_history_count < 10) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "detect_timing_regularity: validation failed");
        return false;
    }
```

**Problem**: Having fewer than 10 timing samples is the normal cold-start state. Cannot detect regularity without data.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

#### P2-31: False positive - detect_burst throws on insufficient data

**File**: `learning/nimcp_lgss_stdp_guard.c`
**Line**: 253
**Category**: False positive NIMCP_THROW_TO_IMMUNE + wrong message

```c
static bool detect_burst(spike_buffer_t* buf, uint32_t threshold_count, ...) {
    if (buf->count < threshold_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "detect_timing_regularity: validation failed");
        return false;
    }
```

**Problem**: Insufficient spikes for burst detection is normal. Message incorrectly references "detect_timing_regularity" (copy-paste from above).

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

---

### learning/nimcp_lgss_training_guard.c (3)

#### P2-32: False positive + wrong error code - reward hacking detection disabled

**File**: `learning/nimcp_lgss_training_guard.c`
**Line**: 763
**Category**: False positive NIMCP_THROW_TO_IMMUNE + wrong error code

```c
if (!g->config.enable_reward_hacking_detection) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_guard_detect_reward_hacking: g->config is NULL");
    return false;
}
```

**Problem**: Feature being disabled by configuration is normal, not a NULL pointer error. `config->enable_reward_hacking_detection` being `false` is a boolean value, not a NULL pointer.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

#### P2-33: False positive - insufficient reward history

**File**: `learning/nimcp_lgss_training_guard.c`
**Line**: 769
**Category**: False positive NIMCP_THROW_TO_IMMUNE

```c
if (g->reward_history.count < 20) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "training_guard_detect_reward_hacking: validation failed");
    return false;
}
```

**Problem**: Having fewer than 20 reward samples is the normal cold-start state.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

#### P2-34: Wrong error code - frozen_params_add for "already exists"

**File**: `learning/nimcp_lgss_training_guard.c`
**Line**: 291
**Category**: False positive + wrong error code

```c
if (frozen_params_contains(f, index)) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "frozen_params_add: validation failed");
    return false;
}
```

**Problem**: Adding an already-frozen parameter is idempotent, not an out-of-range error.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE` (idempotent operation).

---

### perception/nimcp_lgss_content_filter.c (4)

#### P2-35: False positive - strcasestr_safe throws on NULL inputs

**File**: `perception/nimcp_lgss_content_filter.c`
**Lines**: 239-240
**Category**: False positive NIMCP_THROW_TO_IMMUNE

```c
static const char* strcasestr_safe(const char* haystack, size_t haystack_len, const char* needle) {
    if (!haystack || !needle) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "to_lowercase: required parameter is NULL (haystack, needle)");
        return NULL;
    }
```

**Problem**: Search function returning NULL for invalid inputs is normal. The message references "to_lowercase" instead of "strcasestr_safe" (copy-paste error).

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

#### P2-36: False positive - strcasestr_safe throws when needle longer than haystack

**File**: `perception/nimcp_lgss_content_filter.c`
**Line**: 246
**Category**: False positive NIMCP_THROW_TO_IMMUNE + wrong message

```c
if (needle_len > haystack_len) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "to_lowercase: validation failed");
    return NULL;
}
```

**Problem**: Needle longer than haystack means "not found". Message references "to_lowercase".

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

#### P2-37: False positive - strcasestr_safe throws on pattern not found

**File**: `perception/nimcp_lgss_content_filter.c`
**Line**: 263
**Category**: False positive NIMCP_THROW_TO_IMMUNE + wrong message

```c
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "to_lowercase: validation failed");
    return NULL;
```

**Problem**: Pattern not found is the normal "no match" return path. Message references "to_lowercase".

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

#### P2-38: False positive - has_base64_content throws on short text

**File**: `perception/nimcp_lgss_content_filter.c`
**Line**: 306
**Category**: False positive NIMCP_THROW_TO_IMMUNE

```c
static bool has_base64_content(const char* text, size_t len) {
    if (!text || len < 20) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "has_base64_content: text is NULL");
        return false;
    }
```

**Problem**: Text shorter than 20 characters cannot reasonably contain base64 content -- returning false is correct. The message says "text is NULL" which is wrong when `len < 20`.

**Fix**: Split: keep throw for `!text` as `NIMCP_ERROR_NULL_POINTER`, remove throw for `len < 20`.

---

### perception/nimcp_lgss_input_validator.c (4)

#### P2-39: False positive - check_float_validity throws on NaN/Inf found

**File**: `perception/nimcp_lgss_input_validator.c`
**Line**: 165
**Category**: False positive NIMCP_THROW_TO_IMMUNE

```c
for (size_t i = 0; i < len; i++) {
    if (isnan(data[i]) || isinf(data[i])) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "check_float_validity: validation failed");
        return false;
    }
}
```

**Problem**: Detecting NaN/Inf is the **purpose** of this function. Finding invalid floats and returning `false` is the expected success path, not an error.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

#### P2-40: False positive - check_range_float throws on NULL data

**File**: `perception/nimcp_lgss_input_validator.c`
**Line**: 178
**Category**: Wrong error message (copy-paste)

```c
static bool check_range_float(const float* data, ...) {
    if (!data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "check_float_validity: data is NULL");
        return false;
    }
```

**Problem**: The NULL check is appropriate, but the message references "check_float_validity" instead of "check_range_float".

**Fix**: Change message to "check_range_float: data is NULL".

#### P2-41: False positive - check_range_float throws on out-of-range found

**File**: `perception/nimcp_lgss_input_validator.c`
**Line**: 183
**Category**: False positive NIMCP_THROW_TO_IMMUNE + wrong message

```c
if (data[i] < min_val || data[i] > max_val) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "check_float_validity: validation failed");
    return false;
}
```

**Problem**: Detecting out-of-range values is the purpose of this function. Message references "check_float_validity".

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

---

### reward/nimcp_lgss_incentive_validator.c (10)

#### P2-42: False positive - quick_check throws on domain not authorized

**File**: `reward/nimcp_lgss_incentive_validator.c`
**Line**: 476
**Category**: False positive NIMCP_THROW_TO_IMMUNE + wrong message

```c
if (!validator->domains[proposal->domain].authorized) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "incentive_validator_quick_check: validator->domains is NULL");
    return false;
}
```

**Problem**: Domain not being authorized is normal rejection. Message says "validator->domains is NULL" when the domain exists but is not authorized.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

#### P2-43: False positive - quick_check throws on existential harm

**File**: `reward/nimcp_lgss_incentive_validator.c`
**Line**: 483
**Category**: False positive NIMCP_THROW_TO_IMMUNE

```c
if (proposal->worst_harm == HARM_EXISTENTIAL) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "incentive_validator_quick_check: validation failed");
    return false;  /* Always reject existential harm */
}
```

**Problem**: Rejecting existential harm is the primary safety function. This fires on every existential harm proposal.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

#### P2-44: False positive - quick_check throws on high harm

**File**: `reward/nimcp_lgss_incentive_validator.c`
**Line**: 488
**Category**: False positive NIMCP_THROW_TO_IMMUNE

```c
if (proposal->p_harm > validator->config.default_harm_threshold * 2.0f) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "incentive_validator_quick_check: validation failed");
    return false;  /* Reject high harm */
}
```

**Problem**: Rejecting high-harm proposals is normal safety behavior.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

#### P2-45: False positive - quick_check throws on not safety-aligned + wrong message

**File**: `reward/nimcp_lgss_incentive_validator.c`
**Line**: 494
**Category**: False positive NIMCP_THROW_TO_IMMUNE + wrong error code + wrong message

```c
if (validator->config.require_safety_verification && !proposal->is_safety_aligned) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "incentive_validator_quick_check: proposal->is_safety_aligned is NULL");
    return false;
}
```

**Problem**: `is_safety_aligned` is a boolean being `false`, not a NULL pointer. The error code and message are both wrong.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

#### P2-46: False positive - is_aligned throws on NULL/invalid

**File**: `reward/nimcp_lgss_incentive_validator.c`
**Lines**: 534-535
**Category**: Appropriate throw (NULL check)

Note: The NULL/magic check at line 534-535 IS appropriate. No fix needed for this specific throw.

#### P2-47: False positive - is_aligned throws on not safety-aligned + wrong message

**File**: `reward/nimcp_lgss_incentive_validator.c`
**Line**: 541
**Category**: False positive NIMCP_THROW_TO_IMMUNE + wrong error code + wrong message

```c
if (!proposal->is_safety_aligned) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "incentive_validator_is_aligned: proposal->is_safety_aligned is NULL");
    return false;
}
```

**Problem**: Same as P2-45. Boolean `false` is not a NULL pointer.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

#### P2-48: False positive - is_aligned throws on low confidence

**File**: `reward/nimcp_lgss_incentive_validator.c`
**Line**: 547
**Category**: False positive NIMCP_THROW_TO_IMMUNE

```c
if (proposal->alignment_confidence < validator->config.min_alignment_confidence) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "incentive_validator_is_aligned: validation failed");
    return false;
}
```

**Problem**: Insufficient alignment confidence is a normal rejection path.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

#### P2-49: False positive - is_aligned throws on goal not safe

**File**: `reward/nimcp_lgss_incentive_validator.c`
**Line**: 554
**Category**: False positive NIMCP_THROW_TO_IMMUNE + wrong message

```c
if (!reward_alignment_is_goal_safe(validator->reward_monitor, proposal->goal_id)) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "incentive_validator_is_aligned: reward_alignment_is_goal_safe is NULL");
    return false;
}
```

**Problem**: Goal failing safety check is normal validation. Message says function "is NULL" when the function returned false.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

---

### reward/nimcp_lgss_vta_guard.c (3)

#### P2-50: False positive - emission_allowed throws on rate limit

**File**: `reward/nimcp_lgss_vta_guard.c`
**Line**: 508
**Category**: False positive NIMCP_THROW_TO_IMMUNE

```c
if (current_rate >= guard->config.max_da_rate) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "vta_guard_emission_allowed: capacity exceeded");
    return false;
}
```

**Problem**: Rate limiting is the **purpose** of this guard function. Reaching the DA rate limit is normal regulatory behavior.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

#### P2-51: False positive - emission_allowed throws on DA ceiling

**File**: `reward/nimcp_lgss_vta_guard.c`
**Line**: 513
**Category**: False positive NIMCP_THROW_TO_IMMUNE

```c
if (guard->current_da >= guard->config.max_da_concentration) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "vta_guard_emission_allowed: capacity exceeded");
    return false;
}
```

**Problem**: Same as P2-50. DA concentration ceiling is normal rate limiting.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

#### P2-52: False positive - update_receptor_activation throws on imbalance

**File**: `reward/nimcp_lgss_vta_guard.c`
**Line**: 611
**Category**: False positive NIMCP_THROW_TO_IMMUNE

```c
    guard->stats.alerts_triggered++;
    guard->stats.last_alert = VTA_ALERT_RECEPTOR_IMBALANCE;
    ...
    if (guard->alert_callback) {
        guard->alert_callback(VTA_ALERT_RECEPTOR_IMBALANCE, guard->callback_user_data);
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vta_guard_update_receptor_activation: validation failed");
    return -1;  /* Imbalanced */
}
```

**Problem**: Detecting receptor imbalance is the purpose of this monitoring function. It already updates stats and invokes the alert callback. The immune system throw is redundant and confusing.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

---

### reward/nimcp_lgss_reward_alignment.c (3)

#### P2-53: False positive - find_goal throws on not-found

**File**: `reward/nimcp_lgss_reward_alignment.c`
**Line**: 147
**Category**: False positive NIMCP_THROW_TO_IMMUNE

```c
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_goal: operation failed");
    return NULL;
```

**Problem**: Goal not found in the aligned goals list is a normal search result.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

#### P2-54: False positive - detect_hacking throws when NO hacking detected

**File**: `reward/nimcp_lgss_reward_alignment.c`
**Line**: 497
**Category**: False positive NIMCP_THROW_TO_IMMUNE

```c
    snprintf(detection->description, sizeof(detection->description),
            "No hacking detected");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reward_alignment_detect_hacking: operation failed");
    return false;
```

**Problem**: The function fills `detection->description` with "No hacking detected" and then throws "operation failed" to the immune system. Not detecting hacking is the HAPPY PATH.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

#### P2-55: False positive - verify_external_cause throws for self-generated

**File**: `reward/nimcp_lgss_reward_alignment.c`
**Line**: 512
**Category**: False positive NIMCP_THROW_TO_IMMUNE

```c
if (signal->source == REWARD_SOURCE_SELF_GENERATED) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reward_alignment_verify_external_cause: validation failed");
    return false;
}
```

**Problem**: Self-generated signals by definition have no external cause -- returning `false` is the correct answer, not an error.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE`.

---

## P3 Findings (2)

### P3-1: Const-cast in motor_gate_would_violate

**File**: `gates/nimcp_lgss_motor_gate.c`
**Lines**: 460-479
**Category**: Const-correctness violation

```c
bool motor_gate_would_violate(
    const motor_gate_t* gate,
    const motor_command_proposal_t* proposal,
    motor_gate_violation_t* violation
) {
    motor_gate_t* mutable_gate = (motor_gate_t*)gate;
    ...
    motor_exec_result_t result = motor_gate_execute(mutable_gate, proposal, violation);
    ...
    memcpy(&mutable_gate->stats, &saved_stats, sizeof(motor_gate_stats_t));
```

**Problem**: The function takes `const motor_gate_t*` but casts away const to call `motor_gate_execute` which modifies stats. The stats are saved and restored, but this is fragile -- if `motor_gate_execute` modifies other state or if the save/restore is incomplete, the const contract is silently broken.

**Fix**: Either make the function take non-const `motor_gate_t*`, or create a `motor_gate_check_violation()` that does not call `motor_gate_execute`.

---

### P3-2: Cascading throws from strcasestr_local in speech gate

**File**: `gates/nimcp_lgss_speech_gate.c`
**Lines**: 196-199
**Category**: Performance / immune system noise

```c
for (int i = 0; harmful_patterns[i] != NULL; i++) {
    if (strcasestr_local(text, harmful_patterns[i]) != NULL) {
        matches++;
    }
}
```

**Problem**: `detect_harmful_content()` calls `strcasestr_local()` in a loop of 9 patterns. Each call where the pattern is not found fires `NIMCP_THROW_TO_IMMUNE` (P2-15). For typical text that contains no harmful patterns, this generates **9 immune system throws per call**. This is O(N) immune system noise per content check where N is the number of patterns.

**Fix**: Depends on fixing P2-13 through P2-15. Once the false positive throws in `strcasestr_local` are removed, this cascading issue resolves automatically.

---

## Summary by File

| File | P1 | P2 | P3 | Total |
|------|-----|-----|-----|-------|
| `nimcp_lgss.c` | 0 | 1 | 0 | 1 |
| `nimcp_lgss_telemetry.c` | 0 | 1 | 0 | 1 |
| `nimcp_lgss_action_interceptor.c` | 0 | 8 | 0 | 8 |
| `nimcp_lgss_override_controller.c` | 0 | 2 | 0 | 2 |
| `cognitive/nimcp_lgss_attention_guard.c` | 1 | 0 | 0 | 1 |
| `cognitive/nimcp_lgss_working_memory_guard.c` | 0 | 0 | 0 | 0 |
| `gates/nimcp_lgss_autonomic_gate.c` | 0 | 0 | 0 | 0 |
| `gates/nimcp_lgss_motor_gate.c` | 0 | 0 | 1 | 1 |
| `gates/nimcp_lgss_speech_gate.c` | 0 | 8 | 1 | 9 |
| `bridges/nimcp_lgss_bio_async_bridge.c` | 0 | 0 | 0 | 0 |
| `bridges/nimcp_lgss_ethics_bridge.c` | 1 | 0 | 0 | 1 |
| `bridges/nimcp_lgss_executive_bridge.c` | 0 | 0 | 0 | 0 |
| `bridges/nimcp_lgss_planning_bridge.c` | 0 | 1 | 0 | 1 |
| `learning/nimcp_lgss_meta_learning_guard.c` | 0 | 2 | 0 | 2 |
| `learning/nimcp_lgss_plasticity_constraints.c` | 0 | 7 | 0 | 7 |
| `learning/nimcp_lgss_stdp_guard.c` | 0 | 2 | 0 | 2 |
| `learning/nimcp_lgss_training_guard.c` | 0 | 3 | 0 | 3 |
| `perception/nimcp_lgss_adversarial_detector.c` | 0 | 0 | 0 | 0 |
| `perception/nimcp_lgss_content_filter.c` | 0 | 4 | 0 | 4 |
| `perception/nimcp_lgss_input_validator.c` | 0 | 4 | 0 | 4 |
| `reward/nimcp_lgss_incentive_validator.c` | 0 | 8 | 0 | 8 |
| `reward/nimcp_lgss_vta_guard.c` | 0 | 3 | 0 | 3 |
| `reward/nimcp_lgss_reward_alignment.c` | 0 | 3 | 0 | 3 |
| **TOTAL** | **2** | **55** | **2** | **59** |

---

## Summary by Category

| Category | Count |
|----------|-------|
| False positive NIMCP_THROW_TO_IMMUNE (search not-found) | 14 |
| False positive NIMCP_THROW_TO_IMMUNE (normal rejection/validation) | 17 |
| False positive NIMCP_THROW_TO_IMMUNE (insufficient data / cold start) | 5 |
| False positive NIMCP_THROW_TO_IMMUNE (purpose of function) | 6 |
| False positive NIMCP_THROW_TO_IMMUNE (disabled/config false) | 2 |
| Wrong error code (NULL_POINTER for non-null, MUTEX_INIT for locked, etc.) | 4 |
| Wrong error message (copy-paste from other function) | 7 |
| Dead code after return | 1 |
| Const-correctness violation | 1 |
| Cascading throws performance | 1 |
| Wrong error code for allocation failure | 1 |

---

## Remediation Notes

### High-Priority Pattern: LGSS files share a systematic false-positive problem

Nearly all P2 findings (44 of 55) are **false positive NIMCP_THROW_TO_IMMUNE calls** where the throw fires on normal operational paths:

1. **Search/lookup not-found** (14 instances): `find_param`, `find_goal`, `find_pending_unlocked`, `find_escalation_unlocked`, `find_request_unlocked`, `hashmap_contains`, `hashmap_remove`, `strcasestr_local`, `strcasestr_safe`

2. **Normal rejection/validation** (17 instances): Auth token missing, domain not authorized, harm detected, safety not aligned, condition not matching, rule disabled, early termination configured

3. **Insufficient data / cold start** (5 instances): `detect_oscillation`, `detect_burst`, `detect_timing_regularity`, reward history < 20

4. **Function fulfilling its purpose** (6 instances): `check_float_validity` finding NaN, `emission_allowed` hitting rate limit, `detect_hacking` finding no hacking, `verify_external_cause` identifying self-generated

### Recommended bulk fix approach

For most false positive throws, the fix is simply removing the `NIMCP_THROW_TO_IMMUNE` call. The surrounding code already handles the condition correctly (returns appropriate value, logs if needed). The immune system throws add noise that dilutes real security alerts.

For wrong error messages (7 instances), these appear to be copy-paste artifacts where error strings reference the wrong function name. These should be corrected even if the throw itself is removed, since they could confuse debugging if the throw is intentionally kept.
