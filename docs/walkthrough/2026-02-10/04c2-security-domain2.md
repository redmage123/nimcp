# Security Domain Review - Part 2 (Domain Bridges)

**Date**: 2026-02-10
**Directories**: `src/security/{continual,continual_learning,epistemic,imagination,knowledge,knowledge_graph,language,perception,rcog,sleep,training}/`
**Files reviewed**: 18 `.c` files
**Total findings**: 68 (2 P1, 65 P2, 1 P3)

---

## Table of Contents

1. [P1 Findings (Critical)](#p1-findings-critical)
2. [P2 Findings by File](#p2-findings-by-file)
3. [P3 Findings](#p3-findings)
4. [Summary](#summary)

---

## P1 Findings (Critical)

### P1-1: Buffer overflow in `escape_string` (language bridge)

**File**: `src/security/language/nimcp_security_language_bridge.c`
**Line**: 260
**Priority**: P1 - Buffer overflow

**Description**: The `escape_string()` function allocates `input_len * 4 + 1` bytes, but the worst-case expansion is 6x (for `&quot;`), not 4x. The entities used are:
- `&lt;` = 4 chars
- `&gt;` = 4 chars
- `&amp;` = 5 chars
- `&quot;` = 6 chars
- `&#39;` = 5 chars

A string of all double-quote characters would require `input_len * 6` bytes, overflowing the `4 * input_len` buffer.

```c
/* Line 260 */
size_t max_len = input_len * 4 + 1;  /* WRONG: worst case is 6x (&quot;) */
char* output = (char*)nimcp_malloc(max_len);
```

**Mitigating factor**: Line 305 has a safety margin check `if (j >= max_len - 10) break;` which truncates output before overflow, but this is a fragile defense. If `input_len` is 1 and the char is `"`, we write 6 bytes into a 5-byte buffer before reaching the safety check.

**Fix**: Change allocation to `input_len * 6 + 1` to accommodate worst-case `&quot;` expansion:
```c
size_t max_len = input_len * 6 + 1;
```

---

### P1-2: NULL dereference in `is_suspicious_output` (rcog bridge)

**File**: `src/security/rcog/nimcp_security_rcog_bridge.c`
**Line**: 1387
**Priority**: P1 - NULL pointer dereference

**Description**: The guard clause dereferences `score` before verifying it is non-NULL. The condition checks `!output || output_size == 0 || !score`, but the body immediately writes `*score = 0.0f`. If `score` is NULL (which is the condition being checked), this is a NULL dereference crash.

```c
/* Lines 1386-1389 */
if (!output || output_size == 0 || !score) {
    *score = 0.0f;  /* P1: NULL deref if score is NULL */
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "is_suspicious_output: ...");
    return false;
}
```

**Fix**: Guard the dereference:
```c
if (!output || output_size == 0 || !score) {
    if (score) *score = 0.0f;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "is_suspicious_output: ...");
    return false;
}
```

---

## P2 Findings by File

### continual/nimcp_security_continual_learning_bridge.c

#### P2-1: False positive NIMCP_THROW_TO_IMMUNE in `find_replay_buffer`
**Line**: 192
**Description**: Search function throws when buffer not found. "Not found" is a normal search result, not an error condition. Called in O(N) loops, generates spurious immune system alerts.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_replay_buffer: validation failed");
return -1;
```
**Fix**: Remove the throw; return -1 is sufficient for "not found".

#### P2-2: Wrong error code for allocation failure
**Line**: 286
**Description**: Allocation failure throws `NIMCP_ERROR_NULL_POINTER` instead of `NIMCP_ERROR_NO_MEMORY`. The allocation failed; the pointer is NULL as a consequence, not as a root cause.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
```
**Fix**: Use `NIMCP_ERROR_NO_MEMORY`.

#### P2-3: False positive in `security_cl_is_under_attack`
**Line**: 1421
**Description**: Boolean query function throws for NULL bridge. Query functions are typically called speculatively; throwing to immune system for a NULL check in a query is excessive.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_cl_is_under_attack: bridge is NULL");
return false;
```
**Fix**: Remove throw; just return false.

---

### continual_learning/nimcp_security_continual_learning_fep_bridge.c

#### P2-4: Wrong error message conflating inactive with NULL
**Line**: 342-343
**Description**: Checks `!bridge->state.active` but the error message says "bridge->state is NULL". The state struct exists but `active` is false - these are semantically different conditions.
```c
if (!bridge || !bridge->state.active) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "...required parameter is NULL (bridge, bridge->state)");
```
**Fix**: Use `NIMCP_ERROR_NOT_INITIALIZED` and message "bridge is not active".

#### P2-5: False positive in `security_cl_fep_detect_attack`
**Line**: 706
**Description**: Throws when `!bridge->fep_effects.valid`. Invalid effects is the normal initial state before first computation, not an error.
```c
if (!bridge || !bridge->fep_effects.valid) {
    ...
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "security_cl_fep_detect_attack: validation failed");
```
**Fix**: Remove throw; setting outputs to zero and returning false is the correct behavior.

---

### epistemic/nimcp_security_epistemic_fep_bridge.c

#### P2-6: Wrong error code for rate limiting
**Line**: 1212
**Description**: Rate limiting uses `NIMCP_ERROR_INVALID_PARAM`, but rate limiting is an operational flow control mechanism, not a parameter validation error.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sec_epist_fep_execute_restoration: validation failed");
return -1;  /* Rate limited - too many restorations */
```
**Fix**: Remove throw or use `NIMCP_ERROR_RATE_LIMITED` / `NIMCP_ERROR_OPERATION_FAILED`.

---

### epistemic/nimcp_security_epistemic_bridge.c

#### P2-7: False positive in `security_epist_is_connected`
**Line**: 531
**Description**: Boolean query function throws for NULL bridge.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_epist_is_connected: bridge is NULL");
return false;
```
**Fix**: Remove throw; return false.

#### P2-8: Wrong error code in `security_epist_validate_confidence`
**Line**: 550
**Description**: NULL bridge check uses `NIMCP_ERROR_INVALID_PARAM` instead of `NIMCP_ERROR_NULL_POINTER`. Also, a validation function returning false is its normal behavior, not an error.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "security_epist_validate_confidence: validation failed");
return false;
```
**Fix**: Remove throw or use `NIMCP_ERROR_NULL_POINTER`.

#### P2-9: Wrong error code in `security_epist_verify_belief`
**Line**: 686
**Description**: Same pattern as P2-8. NULL bridge throws `NIMCP_ERROR_INVALID_PARAM`.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "security_epist_verify_belief: validation failed");
```
**Fix**: Remove throw or use `NIMCP_ERROR_NULL_POINTER`.

#### P2-10: Wrong error code in `security_epist_validate_evidence`
**Line**: 981-982
**Description**: Same pattern. NULL bridge/chain throws `NIMCP_ERROR_INVALID_PARAM`.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "security_epist_validate_evidence: validation failed");
```
**Fix**: Remove throw or use `NIMCP_ERROR_NULL_POINTER`.

#### P2-11: False positive in `security_epist_check_circular_evidence` (NORMAL path)
**Line**: 1070
**Description**: Throws on the "no circular evidence found" path. This is the NORMAL, HEALTHY result. The function returns `true` when circular evidence IS found (line 1065) and throws+returns false when none is found. Finding no circular evidence should not trigger the immune system.
```c
/* After searching all links and finding no duplicates: */
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "security_epist_check_circular_evidence: validation failed");
return false;
```
**Fix**: Remove the throw entirely. `return false;` is the correct "no circular evidence" result.

#### P2-12: False positive in `security_epist_is_bio_async_connected`
**Line**: 1607
**Description**: Boolean query function throws for NULL bridge.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_epist_is_bio_async_connected: bridge is NULL");
return false;
```
**Fix**: Remove throw; return false.

---

### imagination/nimcp_security_imagination_fep_bridge.c

#### P2-13: Wrong error message conflating inactive with NULL
**Line**: 297
**Description**: Checks `!bridge->state.active` but says "bridge->state is NULL".
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_imagination_fep_compute_effects: bridge->state is NULL");
```
**Fix**: Use `NIMCP_ERROR_NOT_INITIALIZED` and message "bridge is not active".

---

### imagination/nimcp_security_imagination_bridge.c

#### P2-14: False positive in `security_imagination_is_connected`
**Line**: 270
**Description**: Boolean query function throws for NULL bridge.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_imagination_is_connected: bridge is NULL");
return false;
```
**Fix**: Remove throw; return false.

#### P2-15: False positive in `security_imagination_enforce_bounds`
**Line**: 509
**Description**: Boolean query/enforcement function throws for NULL bridge.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_imagination_enforce_bounds: bridge is NULL");
return false;
```
**Fix**: Remove throw; return false.

#### P2-16: False positive on sandbox not found
**Line**: 524
**Description**: Throws when sandbox ID not found via `find_sandbox_by_id`. This is an operational lookup failure, not an error requiring immune system notification.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "security_imagination_enforce_bounds: validation failed");
return false;
```
**Fix**: Remove throw; return false with logging.

#### P2-17: False positive in `security_imagination_check_depth`
**Line**: 560
**Description**: Boolean query function throws for NULL bridge.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_imagination_check_depth: bridge is NULL");
return false;
```
**Fix**: Remove throw; return false.

#### P2-18: False positive in `security_imagination_is_restricted`
**Line**: 1149
**Description**: Boolean query function throws for NULL bridge.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_imagination_is_restricted: bridge is NULL");
return false;
```
**Fix**: Remove throw; return false.

---

### knowledge/nimcp_security_knowledge_graph_bridge.c

#### P2-19: False positive in `contains_injection_pattern`
**Line**: 164
**Description**: Validation helper function throws for NULL/empty input. Checking whether a NULL string contains an injection pattern should return false, not throw to immune system.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "contains_injection_pattern: str is NULL");
return false;
```
**Fix**: Remove throw; return false.

#### P2-20: False positive in `is_safe_entity_name` (NULL check)
**Line**: 181
**Description**: Validation helper throws for NULL/empty name. Empty name is a normal validation rejection.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "is_safe_entity_name: name is NULL");
return false;
```
**Fix**: Remove throw; return false.

#### P2-21: Wrong error message in `is_safe_entity_name` (character check)
**Line**: 189
**Description**: When a character fails the `isalnum` + allowed-chars check, the error message says "isalnum is NULL", which is nonsensical. The actual condition is "character not in allowed set".
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "is_safe_entity_name: isalnum is NULL");
return false;
```
**Fix**: Remove throw (validation rejection is normal) or fix message to "unsafe character in entity name".

---

### knowledge_graph/nimcp_security_kg_fep_bridge.c

#### P2-22: Wrong error message conflating inactive with NULL
**Line**: 308
**Description**: Checks `!bridge->state.active` but says "bridge->state is NULL".
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_kg_fep_compute_effects: bridge->state is NULL");
```
**Fix**: Use `NIMCP_ERROR_NOT_INITIALIZED` and message "bridge is not active".

---

### language/nimcp_security_language_bridge.c

#### P2-23: False positive in `stristr` (NULL check)
**Line**: 201
**Description**: Case-insensitive search helper throws for NULL inputs. Called from `match_patterns` which may pass NULL patterns.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stristr: required parameter is NULL (haystack, needle)");
return NULL;
```
**Fix**: Remove throw; return NULL.

#### P2-24: False positive in `stristr` (not found)
**Line**: 218
**Description**: Throws when substring not found. "Not found" is the normal result for most searches. Called in O(N) pattern matching loops.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stristr: validation failed");
return NULL;
```
**Fix**: Remove throw; return NULL.

#### P2-25: False positive in `match_patterns` (NULL check)
**Line**: 232
**Description**: Pattern matching helper throws for NULL inputs.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "match_patterns: required parameter is NULL (input, patterns)");
return false;
```
**Fix**: Remove throw; return false.

#### P2-26: False positive in `match_patterns` (no match)
**Line**: 245
**Description**: Throws when no pattern matches. "No match" is the normal result for most inputs to a pattern matcher.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "match_patterns: validation failed");
return false;
```
**Fix**: Remove throw; return false.

#### P2-27: False positive in `escape_string` (empty input)
**Line**: 253-255
**Description**: Throws for NULL/empty input to escape function. Empty string is a valid input.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "escape_string: validation failed");
return NULL;
```
**Fix**: Remove throw; return NULL with `*output_len = 0`.

#### P2-28: False positive in `remove_dangerous_patterns` (empty input)
**Line**: 317-319
**Description**: Same pattern as P2-27. Throws for NULL/empty input.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "remove_dangerous_patterns: validation failed");
return NULL;
```
**Fix**: Remove throw; return NULL with `*output_len = 0`.

#### P2-29: False positive in `security_language_is_connected`
**Line**: 668
**Description**: Boolean query function throws for NULL bridge.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_language_is_connected: bridge is NULL");
return false;
```
**Fix**: Remove throw; return false.

#### P2-30: False positive in `security_language_exceeds_threshold` (NULL check)
**Line**: 1448
**Description**: Boolean query function throws for NULL bridge/text.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_language_exceeds_threshold: required parameter is NULL (bridge, text)");
return false;
```
**Fix**: Remove throw; return false.

#### P2-31: False positive in `security_language_exceeds_threshold` (internal failure)
**Line**: 1454
**Description**: Throws when internal `security_language_get_threat_score` fails. The internal function already handles its own error reporting.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "security_language_exceeds_threshold: validation failed");
return false;
```
**Fix**: Remove throw; return false (let internal function's throw suffice).

#### P2-32: False positive in `security_language_is_bio_async_connected`
**Line**: 1661
**Description**: Boolean query function throws for NULL bridge.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_language_is_bio_async_connected: bridge is NULL");
return false;
```
**Fix**: Remove throw; return false.

---

### language/nimcp_security_language_fep_bridge.c

#### P2-33: Wrong error message conflating inactive with NULL
**Line**: 350
**Description**: Checks `!bridge->state.active` but says "bridge->state is NULL".
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_lang_fep_compute_effects: bridge->state is NULL");
```
**Fix**: Use `NIMCP_ERROR_NOT_INITIALIZED` and message "bridge is not active".

#### P2-34: False positive / wrong message in `sec_lang_fep_broadcast_threat`
**Line**: 1244-1245
**Description**: Checks `!bridge->base.bio_async_enabled` but throws `NIMCP_ERROR_NULL_POINTER` with message "required parameter is NULL (bridge, bridge->base)". Bio-async not being enabled is an operational state, not a NULL pointer.
```c
if (!bridge || !bridge->base.bio_async_enabled) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_lang_fep_broadcast_threat: required parameter is NULL (bridge, bridge->base)");
```
**Fix**: Separate the checks. NULL bridge is an error; bio_async not enabled should return -1 without throwing.

---

### perception/nimcp_security_perception_input_bridge.c

#### P2-35: False positive in `detect_ultrasonic_content`
**Line**: 216
**Description**: Throws for insufficient samples or low sample rate. Having fewer than 4 samples or a sample rate below 40kHz is a normal input condition for non-ultrasonic audio, not an error.
```c
if (num_samples < 4 || sample_rate < 40000) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "detect_ultrasonic_content: validation failed");
    return false;
}
```
**Fix**: Remove throw; return false (input doesn't contain ultrasonic content).

#### P2-36: Wrong error message in `detect_adversarial_visual_pattern`
**Line**: 248
**Description**: The condition checks `!pixels || width < 8 || height < 8`, but the error message says "pixels is NULL". When `width < 8` or `height < 8`, pixels is NOT NULL. Also a false positive: small images are normal inputs.
```c
if (!pixels || width < 8 || height < 8) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "detect_adversarial_visual_pattern: pixels is NULL");
    return false;
}
```
**Fix**: Remove throw; return false (image too small for pattern detection).

---

### rcog/nimcp_security_rcog_bridge.c

#### P2-37: Wrong error code for allocation failure
**Line**: 144
**Description**: Allocation failure throws `NIMCP_ERROR_NULL_POINTER` instead of `NIMCP_ERROR_NO_MEMORY`.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
```
**Fix**: Use `NIMCP_ERROR_NO_MEMORY`.

#### P2-38: False positive in `security_rcog_is_connected`
**Line**: 313
**Description**: Boolean query function throws for NULL bridge.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_rcog_is_connected: bridge is NULL");
return false;
```
**Fix**: Remove throw; return false.

#### P2-39: False positive in `security_rcog_is_tool_whitelisted` (NULL check)
**Line**: 334
**Description**: Whitelisting query function throws for NULL bridge/tool_name. A query returning false is sufficient.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_rcog_is_tool_whitelisted: required parameter is NULL (bridge, tool_name)");
return false;
```
**Fix**: Remove throw; return false.

#### P2-40: False positive on emergency lockdown state
**Line**: 345
**Description**: Throws `NIMCP_ERROR_OPERATION_FAILED` when in emergency lockdown. Lockdown is an operational security state, not an error.
```c
if (bridge->state.emergency_lockdown) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "security_rcog_is_tool_whitelisted: validation failed");
    return false;
}
```
**Fix**: Remove throw; return false (tool not allowed during lockdown).

#### P2-41: False positive on tool not in whitelist
**Line**: 354
**Description**: Throws when `find_whitelist_entry` fails. A tool not being in the whitelist is the normal "not whitelisted" result.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "security_rcog_is_tool_whitelisted: validation failed");
return false;
```
**Fix**: Remove throw; return false.

#### P2-42: False positive in `security_rcog_check_recursion_depth`
**Line**: 741
**Description**: Boolean query function throws for NULL bridge.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_rcog_check_recursion_depth: bridge is NULL");
return false;
```
**Fix**: Remove throw; return false.

#### P2-43: False positive in `security_rcog_is_lockdown`
**Line**: 1254
**Description**: Boolean query function throws for NULL bridge.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_rcog_is_lockdown: bridge is NULL");
return false;
```
**Fix**: Remove throw; return false.

#### P2-44: False positive in `is_suspicious_output`
**Line**: 1388
**Description**: After the P1 NULL deref at line 1387, the throw itself is also a false positive. Querying whether output is suspicious with NULL/empty params should return false without immune notification.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "is_suspicious_output: required parameter is NULL (output, score)");
return false;
```
**Fix**: Remove throw after fixing the P1 NULL deref.

---

### sleep/nimcp_bbb_sleep_bridge.c

#### P2-45: False positive in `bbb_sleep_is_glymphatic_active`
**Line**: 183
**Description**: Boolean query function throws for NULL bridge.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bbb_sleep_is_glymphatic_active: bridge is NULL");
return false;
```
**Fix**: Remove throw; return false.

---

### sleep/nimcp_pattern_db_sleep_bridge.c

#### P2-46: False positive in `pattern_db_sleep_is_consolidating`
**Line**: 260
**Description**: Boolean query function throws for NULL bridge.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_db_sleep_is_consolidating: bridge is NULL");
return false;
```
**Fix**: Remove throw; return false.

---

### sleep/nimcp_rate_limiter_sleep_bridge.c

#### P2-47: False positive in `rate_limiter_sleep_is_relaxed`
**Line**: 252
**Description**: Boolean query function throws for NULL bridge.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rate_limiter_sleep_is_relaxed: bridge is NULL");
return false;
```
**Fix**: Remove throw; return false.

---

### training/nimcp_security_training_fep_bridge.c

#### P2-48: False positive in `find_source_index` (not found)
**Line**: 178
**Description**: Search function throws when source not found. "Not found" is the normal result for a search.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_source_index: validation failed");
return -1;
```
**Fix**: Remove throw; return -1.

#### P2-49: Wrong error message in `security_train_fep_compute_effects`
**Line**: 470
**Description**: Checks `!bridge->state.active || !bridge->fep_system` but says "required parameter is NULL (bridge->state, bridge->fep_system)". The `bridge->state` struct exists; `active` is false.
```c
if (!bridge->state.active || !bridge->fep_system) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "...required parameter is NULL (bridge->state, bridge->fep_system)");
```
**Fix**: Separate checks: inactive state -> `NIMCP_ERROR_NOT_INITIALIZED`; null fep_system -> `NIMCP_ERROR_NULL_POINTER`.

#### P2-50: Wrong error message in `security_train_fep_update_from_poisoning`
**Line**: 613-614
**Description**: Same pattern as P2-49. Conflates `!bridge->state.active` with NULL.
```c
if (!bridge->state.active || !bridge->fep_system) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "...required parameter is NULL (bridge->state, bridge->fep_system)");
```
**Fix**: Same as P2-49.

#### P2-51: Wrong error message in `security_train_fep_update_from_gradient_anomaly`
**Line**: 699
**Description**: Same pattern.
```c
if (!bridge->state.active || !bridge->fep_system) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "...required parameter is NULL (bridge->state, bridge->fep_system)");
```
**Fix**: Same as P2-49.

#### P2-52: Wrong error message in `security_train_fep_update_from_extraction_attempt`
**Line**: 769
**Description**: Same pattern.
```c
if (!bridge->state.active || !bridge->fep_system) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "...required parameter is NULL (bridge->state, bridge->fep_system)");
```
**Fix**: Same as P2-49.

#### P2-53: Wrong error message in `security_train_fep_update_from_backdoor_detection`
**Line**: 833-834
**Description**: Same pattern.
```c
if (!bridge->state.active || !bridge->fep_system) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "...required parameter is NULL (bridge->state, bridge->fep_system)");
```
**Fix**: Same as P2-49.

#### P2-54: False positive in `security_train_fep_should_act`
**Line**: 1148-1149
**Description**: Boolean query function throws for NULL bridge or fep_system.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_train_fep_should_act: required parameter is NULL (bridge, bridge->fep_system)");
return false;
```
**Fix**: Remove throw; return false.

#### P2-55: False positive / wrong message in `security_train_fep_process_messages`
**Line**: 1204-1205
**Description**: Checks `!bridge->base.bio_async_enabled` but throws `NIMCP_ERROR_NULL_POINTER` saying "required parameter is NULL (bridge, bridge->base)". Bio-async not enabled is operational state, not NULL.
```c
if (!bridge || !bridge->base.bio_async_enabled) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "...required parameter is NULL (bridge, bridge->base)");
```
**Fix**: Separate checks. Bio-async not enabled should return 0 (no messages processed) without throwing.

---

### training/nimcp_security_training_bridge.c

#### P2-56: False positive in `find_data_source` (not found)
**Line**: 175
**Description**: Search function throws when data source not found.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_data_source: validation failed");
return -1;
```
**Fix**: Remove throw; return -1.

#### P2-57: False positive in `find_checkpoint` (not found)
**Line**: 196
**Description**: Search function throws when checkpoint not found.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_checkpoint: validation failed");
return -1;
```
**Fix**: Remove throw; return -1.

#### P2-58: Wrong error code for allocation failure
**Line**: 289
**Description**: Allocation failure throws `NIMCP_ERROR_NULL_POINTER` instead of `NIMCP_ERROR_NO_MEMORY`.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
```
**Fix**: Use `NIMCP_ERROR_NO_MEMORY`.

#### P2-59: False positive on capacity exceeded
**Line**: 535
**Description**: Throws when max data sources reached. Capacity limits are operational, not errors.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "security_training_validate_data_source: capacity exceeded");
return false;
```
**Fix**: Remove throw; logging is sufficient. Return false.

#### P2-60: False positive on blocked source
**Line**: 552
**Description**: Throws when source is blocked. Blocked status is a normal validation result.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "security_training_validate_data_source: validation failed");
return false;
```
**Fix**: Remove throw; return false.

#### P2-61: False positive on zero params
**Line**: 1012
**Description**: Throws for `num_params == 0` in gradient anomaly check. Zero params is a degenerate but valid input.
```c
if (num_params == 0) {
    if (anomaly_score) *anomaly_score = 0.0f;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "security_training_check_gradient_anomaly: validation failed");
    return false;
}
```
**Fix**: Remove throw; return false with zero score.

#### P2-62: False positive / wrong message on detection disabled
**Line**: 1270
**Description**: Throws `NIMCP_ERROR_NULL_POINTER` with message "bridge->config is NULL" when concept drift detection is disabled. The config exists; the flag is false.
```c
if (!bridge->config.enable_concept_drift_detection) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "...bridge->config is NULL");
    return false;
}
```
**Fix**: Remove throw; return false (feature disabled, no drift to report).

#### P2-63: False positive on null features
**Line**: 1275
**Description**: Parameter validation throws. While this is a programming error, it fires in a detection function where the caller may be exploring.
```c
if (!current_features || num_features == 0) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "...current_features is NULL");
    return false;
}
```
**Fix**: Remove throw; return false.

#### P2-64: Wrong error message on no baseline
**Line**: 1287
**Description**: Throws `NIMCP_ERROR_NULL_POINTER` saying "bridge->drift_baseline is NULL" when there is simply no baseline data yet. This is a normal early-lifecycle state.
```c
if (!bridge->drift_baseline || bridge->drift_baseline_samples == 0) {
    ...
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "...bridge->drift_baseline is NULL");
    return false;
}
```
**Fix**: Remove throw; return false (no baseline to compare against).

#### P2-65: False positive in `security_training_is_under_attack`
**Line**: 1575
**Description**: Boolean query function throws for NULL bridge.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_training_is_under_attack: bridge is NULL");
return false;
```
**Fix**: Remove throw; return false.

---

## P3 Findings

### P3-1: Thread-unsafe static variable in continual learning bridge

**File**: `src/security/continual/nimcp_security_continual_learning_bridge.c`
**Line**: 1350
**Priority**: P3 - Thread safety

**Description**: `last_retention` is a `static float` used to compute retention delta. In a multi-threaded environment, concurrent calls to this function will race on this shared mutable state, producing incorrect deltas.

```c
static float last_retention = 1.0f;
bridge->cl_effects.retention_delta = retention - last_retention;
last_retention = retention;
```

**Fix**: Move `last_retention` into the bridge struct as an instance variable, or protect with the bridge mutex (which is already held at this point in the function).

---

## Summary

| Priority | Count | Description |
|----------|-------|-------------|
| **P1** | **2** | 1 buffer overflow (escape_string 4x vs 6x), 1 NULL deref (score write before check) |
| **P2** | **65** | 36 false positive NIMCP_THROW_TO_IMMUNE, 13 wrong error messages, 9 wrong error codes, 7 combined false positive + wrong message |
| **P3** | **1** | 1 thread-unsafe static variable |
| **Total** | **68** | |

### P2 Breakdown by Category

| Category | Count | Pattern |
|----------|-------|---------|
| False positive: boolean query functions (`is_connected`, `is_under_attack`, etc.) | 18 | Query returns bool; throwing for NULL bridge is excessive |
| False positive: search not-found paths (`find_*`, `stristr`, `match_patterns`) | 8 | "Not found" is normal search result |
| False positive: validation rejection paths | 7 | Validation returning false is the function's purpose |
| False positive: operational state checks (lockdown, disabled, capacity) | 3 | Operational states are not errors |
| Wrong error message: "bridge->state is NULL" for inactive | 8 | `!bridge->state.active` != NULL; conflates boolean with pointer |
| Wrong error message: other misleading messages | 5 | "isalnum is NULL", "pixels is NULL", "config is NULL", etc. |
| Wrong error code: NIMCP_ERROR_NULL_POINTER for alloc failure | 3 | Should be NIMCP_ERROR_NO_MEMORY |
| Wrong error code: NIMCP_ERROR_INVALID_PARAM for NULL bridge | 4 | Should be NIMCP_ERROR_NULL_POINTER |
| Wrong error code + false positive combined | 7 | Bio-async not enabled != NULL, etc. |
| False positive: internal failure re-throw | 2 | Double-throw (inner function already threw) |

### Files Reviewed (18 total)

| # | File | P1 | P2 | P3 | Lines |
|---|------|----|----|----|----|
| 1 | `continual/nimcp_security_continual_learning_bridge.c` | 0 | 3 | 1 | 1562 |
| 2 | `continual_learning/nimcp_security_continual_learning_fep_bridge.c` | 0 | 2 | 0 | 951 |
| 3 | `epistemic/nimcp_security_epistemic_fep_bridge.c` | 0 | 1 | 0 | 1559 |
| 4 | `epistemic/nimcp_security_epistemic_bridge.c` | 0 | 6 | 0 | 1766 |
| 5 | `imagination/nimcp_security_imagination_fep_bridge.c` | 0 | 1 | 0 | 1155 |
| 6 | `imagination/nimcp_security_imagination_bridge.c` | 0 | 5 | 0 | 1339 |
| 7 | `knowledge/nimcp_security_knowledge_graph_bridge.c` | 0 | 3 | 0 | 1283 |
| 8 | `knowledge_graph/nimcp_security_kg_fep_bridge.c` | 0 | 1 | 0 | 1122 |
| 9 | `language/nimcp_security_language_bridge.c` | 1 | 10 | 0 | 1867 |
| 10 | `language/nimcp_security_language_fep_bridge.c` | 0 | 2 | 0 | 1653 |
| 11 | `perception/nimcp_security_perception_input_bridge.c` | 0 | 2 | 0 | 1132 |
| 12 | `rcog/nimcp_security_rcog_bridge.c` | 1 | 8 | 0 | 1439 |
| 13 | `sleep/nimcp_anomaly_detector_sleep_bridge.c` | 0 | 0 | 0 | 275 |
| 14 | `sleep/nimcp_bbb_sleep_bridge.c` | 0 | 1 | 0 | 232 |
| 15 | `sleep/nimcp_pattern_db_sleep_bridge.c` | 0 | 1 | 0 | 281 |
| 16 | `sleep/nimcp_rate_limiter_sleep_bridge.c` | 0 | 1 | 0 | 262 |
| 17 | `training/nimcp_security_training_fep_bridge.c` | 0 | 8 | 0 | 1320 |
| 18 | `training/nimcp_security_training_bridge.c` | 0 | 10 | 0 | 1710 |
| | **TOTAL** | **2** | **65** | **1** | **20908** |

### Systemic Patterns

1. **Boolean query functions throwing to immune system** (18 occurrences): Every `is_*` and `*_connected` function throws `NIMCP_THROW_TO_IMMUNE` for NULL bridge. These are query functions - returning false is the correct behavior for invalid input.

2. **Search not-found throwing** (8 occurrences): `find_*`, `stristr`, and `match_patterns` functions throw when their search target is not found. "Not found" is a normal return value, not an error.

3. **"bridge->state is NULL" for inactive state** (8 occurrences across 5 FEP bridges): All FEP bridges use the same pattern of checking `!bridge->state.active` but reporting "bridge->state is NULL" with `NIMCP_ERROR_NULL_POINTER`. The state struct exists; it's the `active` flag that's false.

4. **NIMCP_ERROR_NULL_POINTER for allocation failure** (3 occurrences): `continual_learning_bridge`, `rcog_bridge`, and `training_bridge` all use `NIMCP_ERROR_NULL_POINTER` instead of `NIMCP_ERROR_NO_MEMORY` for `nimcp_malloc`/`nimcp_calloc` failures.

5. **`sleep/nimcp_anomaly_detector_sleep_bridge.c` is the only clean file** with zero findings.
