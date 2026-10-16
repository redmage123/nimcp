# Walkthrough 08b: Language Core Files Review

**Date**: 2026-02-10
**Reviewer**: Claude Opus 4.6
**Files Reviewed**:
- `src/language/nimcp_language_orchestrator.c` (1428 lines)
- `src/language/nimcp_language_bio_async.c` (628 lines)
- `src/language/nimcp_language_config.c` (361 lines)
- `src/language/integration/nimcp_language_bio_async_bridge.c` (1223 lines)

---

## Summary

| Priority | Count |
|----------|-------|
| P1 (crash/corruption) | 5 |
| P2 (resource leak/wrong behavior) | 6 |
| P3 (missing validation) | 4 |
| **Total** | **15** |

---

## P1 Findings

### P1-1: Text buffer NUL terminator off-by-one heap overflow

**File**: `/home/bbrelin/nimcp/src/language/nimcp_language_orchestrator.c`
**Lines**: 647-656

**Description**: `process_text()` computes available space as `text_capacity - text_length`, copies up to that many bytes, increments `text_length`, then writes `text_buffer[text_length] = '\0'`. When text fills to capacity (e.g., text_length reaches 4096 with text_capacity=4096), the NUL terminator is written at index 4096, one byte beyond the 4096-byte allocation (valid indices 0..4095). This is a 1-byte heap buffer overflow that can corrupt heap metadata.

```c
// Line 647-656:
uint32_t space = orchestrator->input.text_capacity -
                 orchestrator->input.text_length;
uint32_t to_copy = (len < space) ? len : space;

if (to_copy > 0) {
    memcpy(&orchestrator->input.text_buffer[orchestrator->input.text_length],
           text, to_copy);
    orchestrator->input.text_length += to_copy;
    orchestrator->input.text_buffer[orchestrator->input.text_length] = '\0';  // OOB when text_length == text_capacity
}
```

**Fix**: Reserve one byte for the NUL terminator in the space calculation:
```c
uint32_t space = orchestrator->input.text_capacity -
                 orchestrator->input.text_length - 1;  // -1 for NUL terminator
```

---

### P1-2: Shallow copy of comprehension result causes double-free / use-after-free

**File**: `/home/bbrelin/nimcp/src/language/nimcp_language_orchestrator.c`
**Lines**: 714-717

**Description**: `language_orchestrator_get_comprehension()` does a raw struct copy (`*result = orchestrator->current_comprehension`). The `language_comprehension_result_t` struct contains multiple heap-allocated pointers: `words`, `concepts`, `parse_tree`, `semantic_vector`, `prosody.pitch_contour`, `prosody.intensity_contour`. After the shallow copy, both the orchestrator's internal copy and the caller's copy point to the same heap memory. If either side frees the result (the caller via `language_comprehension_result_free()`, or the orchestrator via `language_orchestrator_destroy()` at line 261), the other side has dangling pointers. The next free triggers a double-free.

```c
// Line 716:
*result = orchestrator->current_comprehension;  // Shallow copy of struct with heap pointers
```

**Fix**: Implement a deep copy function `language_comprehension_result_copy()` that duplicates all owned heap allocations, and use it instead of the raw struct assignment. Alternatively, document that the returned result is borrowed and must not be freed by the caller, and remove `language_comprehension_result_free` from the public API contract for copied results.

---

### P1-3: Shallow copy of production plan causes double-free / use-after-free

**File**: `/home/bbrelin/nimcp/src/language/nimcp_language_orchestrator.c`
**Lines**: 732-735

**Description**: Same pattern as P1-2. `language_orchestrator_get_production_plan()` does a shallow struct copy of `language_production_plan_t`, which contains heap-allocated `words`, `phonemes`, `motor_commands`, `prosody.pitch_contour`, and `prosody.intensity_contour`. Both the original and the copy share heap pointers, leading to double-free.

```c
// Line 734:
*plan = orchestrator->current_production;  // Shallow copy of struct with heap pointers
```

**Fix**: Same as P1-2 -- implement deep copy or document borrow semantics.

---

### P1-4: Message size validation skipped in bio-async callback -- potential buffer overread

**File**: `/home/bbrelin/nimcp/src/language/nimcp_language_bio_async.c`
**Lines**: 594-620

**Description**: The message callback explicitly ignores `msg_size` via `(void)msg_size`, then computes a payload pointer as `msg + sizeof(bio_message_header_t)` and reads `header->payload_size` from the message header. The payload pointer and size are passed directly to user-registered handlers without any validation that `msg_size >= sizeof(bio_message_header_t) + header->payload_size`. A malformed or truncated message will cause handlers to read beyond the actual message buffer.

```c
// Lines 595-608:
(void)msg_size;                    // Size explicitly discarded
// ...
const bio_message_header_t* header = (const bio_message_header_t*)msg;
uint32_t message_type = header->type;
const void* payload = (const uint8_t*)msg + sizeof(bio_message_header_t);
uint32_t payload_size = header->payload_size;  // Unvalidated

// Handler called with potentially invalid payload/size:
g_bio_async_ctx->handlers[i].handler(orchestrator, message_type, payload, payload_size);
```

**Fix**: Validate message size before dispatching:
```c
if (msg_size < sizeof(bio_message_header_t)) {
    return NIMCP_ERROR_INVALID_PARAM;
}
if (msg_size < sizeof(bio_message_header_t) + header->payload_size) {
    return NIMCP_ERROR_INVALID_PARAM;
}
```

---

### P1-5: Dangling pointers sent in async bio-router messages

**File**: `/home/bbrelin/nimcp/src/language/integration/nimcp_language_bio_async_bridge.c`
**Lines**: 565, 826-827

**Description**: Several broadcast/send functions store raw pointers to caller-owned data inside message structs, then send those messages via `bio_router_send` or `bio_router_broadcast`. If the router processes these messages asynchronously (which is the design intent of bio-async), the pointers may be invalid by the time the receiver reads them. Three instances:

1. Line 565: `msg.semantic_input = (float*)semantic_input;` in `language_bio_bridge_request_production()`
2. Line 826: `msg.concept_ids = (uint32_t*)concept_ids;` in `language_bio_bridge_broadcast_semantic_state()`
3. Line 827: `msg.activations = (float*)activations;` in `language_bio_bridge_broadcast_semantic_state()`

```c
// Line 565:
msg.semantic_input = (float*)semantic_input;  // Raw pointer to caller stack/heap data

// Lines 826-827:
msg.concept_ids = (uint32_t*)concept_ids;     // Raw pointer, may go stale
msg.activations = (float*)activations;        // Raw pointer, may go stale
```

**Fix**: Copy the data into the message buffer (or into a heap allocation referenced by the message) rather than storing raw pointers. Alternatively, if the messaging system guarantees synchronous processing within `bio_router_send`, document that guarantee prominently to prevent future async-ification from breaking this.

---

## P2 Findings

### P2-1: Global bio-async context never freed (resource leak)

**File**: `/home/bbrelin/nimcp/src/language/nimcp_language_bio_async.c`
**Lines**: 75, 143-165

**Description**: `language_bio_async_register()` allocates `g_bio_async_ctx` at line 103, and `language_bio_async_unregister()` clears its fields and sets `registered = false`, but never calls `nimcp_free(g_bio_async_ctx)`. The global pointer retains the allocation forever. No cleanup/destroy function exists for this module.

**Fix**: Free the context in `language_bio_async_unregister()`:
```c
nimcp_free(g_bio_async_ctx);
g_bio_async_ctx = NULL;
```

---

### P2-2: False positive NIMCP_THROW_TO_IMMUNE in `language_bio_async_is_registered()`

**File**: `/home/bbrelin/nimcp/src/language/nimcp_language_bio_async.c`
**Line**: 171

**Description**: `language_bio_async_is_registered()` is a boolean query function. When `g_bio_async_ctx` is NULL, it means the bio-async system was never registered -- a perfectly normal state. Throwing `NIMCP_THROW_TO_IMMUNE` on this path fires the immune system for routine queries, generating noise and wasting immune resources.

```c
if (!orchestrator || !g_bio_async_ctx) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "...");  // False positive
    return false;
}
```

**Fix**: Remove the throw; just return false:
```c
if (!orchestrator || !g_bio_async_ctx) {
    return false;
}
```

---

### P2-3: False positive NIMCP_THROW_TO_IMMUNE in `language_bio_async_get_context()`

**File**: `/home/bbrelin/nimcp/src/language/nimcp_language_bio_async.c`
**Lines**: 182, 186

**Description**: Same pattern as P2-2. `language_bio_async_get_context()` is a getter that returns NULL when the context is not available. Throwing to the immune system for "orchestrator not registered" or "mismatch" conditions is a false positive -- these are normal query-not-found paths.

```c
if (!orchestrator || !g_bio_async_ctx) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "...");  // False positive
    return NULL;
}
if (g_bio_async_ctx->orchestrator != orchestrator) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "...");  // False positive, also wrong error code
    return NULL;
}
```

**Fix**: Remove both throws; return NULL silently:
```c
if (!orchestrator || !g_bio_async_ctx) {
    return NULL;
}
if (g_bio_async_ctx->orchestrator != orchestrator) {
    return NULL;
}
```

---

### P2-4: Wrong error code at line 186 -- NIMCP_ERROR_NULL_POINTER for mismatch

**File**: `/home/bbrelin/nimcp/src/language/nimcp_language_bio_async.c`
**Line**: 186

**Description**: When `g_bio_async_ctx->orchestrator != orchestrator`, the code throws `NIMCP_ERROR_NULL_POINTER` with message "validation failed". This is not a NULL pointer condition -- it is a parameter mismatch. The error code should be `NIMCP_ERROR_INVALID_PARAMETER` (if the throw is kept at all, which per P2-3 it should not be).

**Fix**: If keeping the throw: change to `NIMCP_ERROR_INVALID_PARAMETER`. Better: remove per P2-3.

---

### P2-5: Conditional `#define NIMCP_ERROR_INVALID_PARAM -1` may shadow real error code

**File**: `/home/bbrelin/nimcp/src/language/nimcp_language_bio_async.c`
**Lines**: 24-26

**Description**: The file conditionally defines `NIMCP_ERROR_INVALID_PARAM` as `-1` if it is not already defined. The canonical value from `nimcp_error_codes.h` is `1002`. If the include chain does not pull in `nimcp_error_codes.h` before this point, the code uses `-1` instead of `1002`, which breaks error code consistency. This depends on include order and is fragile.

```c
#ifndef NIMCP_ERROR_INVALID_PARAM
#define NIMCP_ERROR_INVALID_PARAM -1    // Should be 1002
#endif
```

**Fix**: Remove the conditional define and ensure `nimcp_error_codes.h` (or a header that includes it) is included before this point. Add `#include "utils/error/nimcp_error_codes.h"` explicitly.

---

### P2-6: Storing non-owned pointer as `semantic_input` in production plan

**File**: `/home/bbrelin/nimcp/src/language/nimcp_language_orchestrator.c`
**Line**: 684

**Description**: `language_orchestrator_generate_output()` stores a raw `const float*` pointer (cast to `float*`) from the caller into `current_production.semantic_input`. The production plan struct documents that `semantic_input` is "not owned" (line 992), and `language_production_plan_free()` does not free it. However, `language_orchestrator_get_production_plan()` (P1-3) shallow-copies this struct, giving the caller a pointer to data they do not own and which may become stale. The `const` qualifier is also discarded.

```c
orchestrator->current_production.semantic_input = (float*)semantic_input;  // const discarded, non-owned
```

**Fix**: Deep-copy the semantic input into an owned allocation, or clearly document the lifetime requirements for callers and make the production plan use a const pointer.

---

## P3 Findings

### P3-1: No validation of `input_size` for non-text/non-phoneme input types

**File**: `/home/bbrelin/nimcp/src/language/nimcp_language_orchestrator.c`
**Lines**: 549-601

**Description**: `language_orchestrator_process_input()` accepts `input_size` but never validates it for AUDIO and SEMANTIC input types. The `input_size` parameter is ignored in the AUDIO and SEMANTIC branches, which could lead to processing garbage if the caller provides `input_size=0` with a non-NULL but empty buffer.

**Fix**: Add `input_size > 0` validation at the top guard clause, or validate per input type.

---

### P3-2: No validation of `semantic_dim` or `output`/`max_output` in `generate_output()`

**File**: `/home/bbrelin/nimcp/src/language/nimcp_language_orchestrator.c`
**Lines**: 669-700

**Description**: `language_orchestrator_generate_output()` validates `orchestrator`, `running`, and `semantic_input`, but does not validate `semantic_dim > 0`, `output != NULL`, or `max_output > 0`. A zero semantic_dim is meaningless, and a NULL output buffer with non-zero max_output is a latent crash risk.

**Fix**: Add validation:
```c
if (semantic_dim == 0) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "...");
    return -1;
}
```

---

### P3-3: Zero-size buffer allocation if config values are zero

**File**: `/home/bbrelin/nimcp/src/language/nimcp_language_orchestrator.c`
**Lines**: 1019-1053

**Description**: `orchestrator_init_buffers()` uses `config.phoneme_buffer_size` and `config.max_utterance_words` as allocation sizes without checking for zero. If a caller provides a config with these set to 0, `nimcp_calloc(0, sizeof(...))` behavior is implementation-defined (may return NULL or a unique pointer). A NULL return would be treated as allocation failure and erroneously throw `NIMCP_ERROR_NO_MEMORY`.

**Fix**: Validate buffer sizes > 0 before allocation, or clamp to minimum values.

---

### P3-4: No validation of `max_subscriptions` being zero in bridge config

**File**: `/home/bbrelin/nimcp/src/language/integration/nimcp_language_bio_async_bridge.c`
**Lines**: 152-178

**Description**: `language_bio_bridge_create()` uses `config->max_subscriptions` as the subscription array capacity. If set to 0, `nimcp_calloc(0, sizeof(lang_bio_subscription_t))` has implementation-defined behavior, and subsequent subscription operations would always report "full" despite the system appearing initialized.

**Fix**: Validate `max_subscriptions > 0` or set a minimum default in `language_bio_bridge_default_config()`.

---

## Notes

### Clean Patterns Observed

- **nimcp_language_config.c**: All 9 default config functions follow consistent guard clause patterns with proper throw + return. No bugs found. This file is clean.
- **Guard clauses**: All public API functions in the orchestrator have proper `throw + return` guard clauses.
- **Callback registration**: Both the orchestrator and bio-async handler registration correctly return -1 when slots are full without false-positive throws (lines 927, 538).
- **Bridge lifecycle**: `language_bio_bridge_create`/`destroy` properly handles subscription array allocation and cleanup.
- **String conversion functions**: All `*_to_string()` functions (lines 1275-1428) are safe, returning string literals with a default "UNKNOWN" case.

### Architectural Concerns (Not Counted)

- **Global mutable state**: `g_bio_async_ctx` in `nimcp_language_bio_async.c` is a single global, preventing multiple orchestrators from having independent bio-async contexts. This is documented ("one per orchestrator would require internal storage") but limits testability and concurrent usage.
- **const-correctness**: Multiple places cast away `const` qualifiers (lines 684, 565, 826-827, 913, 953). While not directly buggy, this defeats compiler const-correctness checking.
