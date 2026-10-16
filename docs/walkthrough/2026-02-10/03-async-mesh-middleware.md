# Code Review: Async, Mesh, Middleware, Networking

**Date**: 2026-02-10
**Reviewer**: Claude Opus 4.6
**Scope**: src/async/ (17 files), src/middleware/ (~77 files), src/mesh/ (36 files), src/networking/ (23 files)

---

## Summary

| Priority | Count | Description |
|----------|-------|-------------|
| P1 (Critical/Crash) | 8 | NULL deref, platform_once not reset, race conditions |
| P2 (Logic/Correctness) | 14 | False positive NIMCP_THROW_TO_IMMUNE, resource leaks, wrong error codes |
| P3 (Style/Robustness) | 6 | Missing const, magic numbers, inconsistent error messages |

---

## P1 Findings (Critical/Crash)

### P1-1: bio_router subsystem platform_once variables never reset on shutdown

**File**: `/home/bbrelin/nimcp/src/async/nimcp_bio_router.c`
**Lines**: 1792, 2011, 2230, 2477

**Description**: Four `nimcp_platform_once_t` variables (`g_signal_mutex_once`, `g_wave_mutex_once`, `g_subscription_once`, `g_emotion_reg_once`) are initialized with `NIMCP_PLATFORM_ONCE_INIT` but are never reset during `bio_router_shutdown()`. The main `g_router_init_once` IS correctly reset at line 800, but these four subsystem once-variables are not. If the bio-router is shut down and re-initialized (common in tests), these mutexes will not be re-initialized because `nimcp_platform_once()` will think the init already ran. The global arrays (`g_signal_observers`, `g_wave_callbacks`, `g_subscriptions`, `g_emotion_registrations`) and their counts are also never zeroed on shutdown, so stale entries persist across re-initialization.

**Impact**: After shutdown + re-init, the mutexes protecting the signal observer, glial wave callback, subscription, and emotion registration subsystems will be in a destroyed/undefined state. Any lock/unlock on them is undefined behavior, potentially leading to deadlocks or crashes.

**Fix**: In `bio_router_shutdown()`, before the final cleanup section, add:

```c
/* Reset subsystem once-flags for re-initialization */
g_signal_mutex_once = (nimcp_platform_once_t)NIMCP_PLATFORM_ONCE_INIT;
g_signal_observer_count = 0;
memset(g_signal_observers, 0, sizeof(g_signal_observers));

g_wave_mutex_once = (nimcp_platform_once_t)NIMCP_PLATFORM_ONCE_INIT;
g_wave_callback_count = 0;
memset(g_wave_callbacks, 0, sizeof(g_wave_callbacks));

g_subscription_once = (nimcp_platform_once_t)NIMCP_PLATFORM_ONCE_INIT;
g_subscription_count = 0;
memset(g_subscriptions, 0, sizeof(g_subscriptions));

g_emotion_reg_once = (nimcp_platform_once_t)NIMCP_PLATFORM_ONCE_INIT;
g_emotion_registration_count = 0;
memset(g_emotion_registrations, 0, sizeof(g_emotion_registrations));
```

### P1-2: networking NLP platform_once variables never reset on shutdown

**File**: `/home/bbrelin/nimcp/src/networking/nlp/nimcp_nlp_session.c`
**Line**: 133

**File**: `/home/bbrelin/nimcp/src/networking/nlp/nimcp_nlp_crypto.c`
**Line**: 126

**Description**: `g_session_bio_once` and `g_crypto_bio_once` are `nimcp_platform_once_t` variables used for one-time bio-async registration in the NLP session and crypto modules. Neither has a corresponding shutdown function that resets them to `NIMCP_PLATFORM_ONCE_INIT`. If the bio-async system is shut down and re-initialized, these modules will not re-register with the bio-router because the once-guard already fired. The stale `g_session_bio_ctx` and `g_crypto_bio_ctx` pointers will reference freed memory.

**Impact**: Use-after-free on the bio module context pointers after bio-router shutdown + re-init, leading to potential crashes or data corruption.

**Fix**: Add shutdown functions for each module that reset the once-flag and NULL out the context pointer, and call them from the appropriate shutdown path. Alternatively, add a listener for bio-router shutdown events that resets these.

### P1-3: shared_state_invoke_callbacks called with ambiguous lock state in bio_async

**File**: `/home/bbrelin/nimcp/src/async/nimcp_bio_async.c`
**Lines**: 803-853

**Description**: The comment on `shared_state_invoke_callbacks()` at line 799 says "This function MUST be called WITHOUT holding shared->mutex." However, the comment at lines 832-835 contradicts this, saying "The caller (nimcp_bio_promise_complete, etc.) already holds the mutex." The function reads `shared->callbacks` (a linked list) without holding the mutex, which is safe only if no other thread can modify the list concurrently. But the function copies callback pointers into a local array and then invokes them -- if the callbacks are freed by another thread between the copy and the invocation (e.g., via `shared_state_release` on another thread), this is a use-after-free.

The actual safety depends on whether the mutex is held or not when this function is called. The contradictory comments suggest confusion about the contract.

**Impact**: Potential use-after-free if callbacks are accessed after the shared state's refcount drops to zero on another thread.

**Fix**: Clarify and enforce the locking contract. The safest approach is to hold the mutex while copying the callback list to the local array, then release it before invoking callbacks:

```c
nimcp_mutex_lock(&shared->mutex);
/* Copy callback pointers to local array */
bio_callback_node_t* cb = shared->callbacks;
while (cb && callback_count < BIO_MAX_CALLBACKS) {
    callbacks_copy[callback_count] = cb;
    userdata_copy[callback_count] = cb->user_data;
    callback_count++;
    cb = cb->next;
}
nimcp_mutex_unlock(&shared->mutex);
/* Now invoke outside lock */
```

### P1-4: bio_router_register_module leaks module entry on failed context allocation

**File**: `/home/bbrelin/nimcp/src/async/nimcp_bio_router.c`
**Lines**: 962-968

**Description**: After successfully registering a new module (allocating inbox, handler mutex, incrementing `module_count`), the function unlocks the mutex at line 959 and then attempts to allocate the context struct at line 962. If `nimcp_calloc` returns NULL, the function returns NULL but the module entry remains in the registry with an incremented count, a valid magic number, and initialized inbox/handler structures. This means the module slot is "claimed" but has no corresponding context handle, making it impossible to unregister or clean up.

**Impact**: Orphaned module entry that permanently occupies a slot in the registry. The inbox and handler mutex are leaked. Under repeated registration failures, this could exhaust the module registry.

**Fix**: Move the context allocation to before the unlock, or on failure, re-acquire the lock and undo the registration (destroy inbox, handler mutex, decrement count, clear magic).

### P1-5: bio_msg_queue_grow debug assertion can cause deadlock in non-debug builds

**File**: `/home/bbrelin/nimcp/src/async/nimcp_bio_router.c`
**Lines**: 278-288

**Description**: The `bio_msg_queue_grow()` function has a debug assertion that calls `nimcp_platform_mutex_trylock()` on the queue mutex to verify the caller holds it. If trylock succeeds (meaning the mutex was NOT held), it unlocks and returns an error. However, this check is only present in `#ifndef NDEBUG` builds. In release builds, the function proceeds without verifying the mutex is held. If called without the lock in a release build, data corruption would occur silently. More critically, the trylock pattern itself is incorrect for recursive mutexes -- if the mutex is recursive, trylock can succeed even when the caller holds it, giving a false positive on the "not held" check.

**Impact**: Silent data corruption in release builds if the function is called without the lock held. False assertion failure if the mutex happens to be recursive.

**Fix**: This is a development-time assertion and is acceptable as-is if the mutex is known to be non-recursive (which it is, since `nimcp_platform_mutex_init(&queue->mutex, false)` passes `false` for recursive). However, add a comment clarifying this dependency. Consider using a thread ID check instead of trylock for more robust assertions.

### P1-6: p2pnode mutex is stack-embedded, not heap-allocated

**File**: `/home/bbrelin/nimcp/src/networking/p2p/nimcp_p2pnode.c`
**Line**: 258

**Description**: The `p2p_node_struct` contains `nimcp_mutex_t lock` as a direct member (not a pointer). However, `nimcp_mutex_t` in the NIMCP codebase is typically used as `nimcp_mutex_t*` (pointer returned by `nimcp_mutex_create()`). The struct uses it as a direct value. If `nimcp_mutex_init()` is called on `&node->lock` (taking the address of the embedded struct), this works correctly. However, if the code elsewhere tries to call `nimcp_mutex_destroy(node->lock)` (passing by value instead of pointer), it will fail. This depends on how the mutex is initialized and destroyed elsewhere in the same file.

**Impact**: Depends on usage pattern -- if correctly used with address-of, this is a P3 style issue. If confused with pointer semantics, it is a crash.

**Fix**: Verify that `nimcp_mutex_init(&node->lock, ...)` and `nimcp_mutex_destroy(&node->lock)` are used consistently throughout the file. If the codebase convention is pointer-based mutexes, consider switching to `nimcp_mutex_t* lock` and using `nimcp_mutex_create(NULL)`.

### P1-7: router_immune_bridge_create uses incorrect error code for NULL parameters

**File**: `/home/bbrelin/nimcp/src/async/immune/nimcp_bio_router_immune_bridge.c`
**Line**: 234

**Description**: When `router` or `immune_system` is NULL, the error thrown is `NIMCP_ERROR_NO_MEMORY` with message "required parameter is NULL (router, immune_system)". The error code should be `NIMCP_ERROR_NULL_POINTER`, not `NIMCP_ERROR_NO_MEMORY`. This incorrect code could confuse error handling logic that switches on error codes.

**Impact**: Incorrect error classification may cause callers to retry allocation instead of fixing the NULL parameter.

**Fix**: Change `NIMCP_ERROR_NO_MEMORY` to `NIMCP_ERROR_NULL_POINTER` at line 234.

### P1-8: bio_router_shutdown potential use-after-free on g_router_brain_kg_mutex

**File**: `/home/bbrelin/nimcp/src/async/nimcp_bio_router.c`
**Lines**: 178, 796-801

**Description**: `g_router_brain_kg_mutex` is initialized in `init_router_mutex_once()` at line 204, and `g_router_brain_kg_mutex_initialized` is set to true. However, during `bio_router_shutdown()`, neither `g_router_brain_kg_mutex` is destroyed nor is `g_router_brain_kg_mutex_initialized` reset to false. When `g_router_init_once` is reset at line 800, a subsequent re-init will call `init_router_mutex_once()` again, which calls `nimcp_platform_mutex_init(&g_router_brain_kg_mutex, false)` on an already-initialized mutex. On some platforms (e.g., POSIX), calling `pthread_mutex_init` on an already-initialized mutex is undefined behavior.

**Impact**: Undefined behavior on re-initialization, potentially corrupting the mutex state.

**Fix**: In `bio_router_shutdown()`, before resetting `g_router_init_once`, add:
```c
if (g_router_brain_kg_mutex_initialized) {
    nimcp_platform_mutex_destroy(&g_router_brain_kg_mutex);
    g_router_brain_kg_mutex_initialized = false;
}
nimcp_platform_mutex_destroy(&g_router_init_mutex);
```

---

## P2 Findings (Logic/Correctness)

### P2-1: False positive NIMCP_THROW_TO_IMMUNE in find_quarantined_node

**File**: `/home/bbrelin/nimcp/src/async/immune/nimcp_bio_router_immune_bridge.c`
**Line**: 162

**Description**: `find_quarantined_node()` throws `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_quarantined_node: validation failed")` when a node is not found in the quarantine list. Not finding a node is normal -- it means the node is not quarantined. Callers at lines 496 and 975 check the return value to determine quarantine status. This throw fires on every lookup of a non-quarantined node, flooding the immune system with false positive alerts.

**Fix**: Remove the NIMCP_THROW_TO_IMMUNE at line 162. Just return NULL.

### P2-2: False positive NIMCP_THROW_TO_IMMUNE in find_inflammation_impact

**File**: `/home/bbrelin/nimcp/src/async/immune/nimcp_bio_router_immune_bridge.c`
**Line**: 186

**Description**: Same pattern as P2-1. `find_inflammation_impact()` throws on normal "region not found" path. Caller at line 452 uses the return value to check if inflammation exists.

**Fix**: Remove the NIMCP_THROW_TO_IMMUNE at line 186. Just return NULL.

### P2-3: False positive NIMCP_THROW_TO_IMMUNE in find_brain (replication)

**File**: `/home/bbrelin/nimcp/src/networking/replication/nimcp_replication.c`
**Line**: 693

**Description**: `find_brain()` throws `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_brain: validation failed")` when a brain is not found in the registry. Not finding a brain is a normal lookup result, not an error condition. This is a classic false positive pattern -- search/lookup "not found" paths should not throw to immune.

**Fix**: Remove the NIMCP_THROW_TO_IMMUNE at line 693. Just return NULL.

### P2-4: False positive NIMCP_THROW_TO_IMMUNE in subscriber_unsubscribe

**File**: `/home/bbrelin/nimcp/src/middleware/events/nimcp_event_subscriber.c`
**Line**: 251

**Description**: `subscriber_unsubscribe()` throws `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "subscriber_unsubscribe: operation failed")` when a subscription ID is not found. This could happen normally if a subscription was already removed or never existed. Should return an error code without throwing to immune.

**Fix**: Remove the NIMCP_THROW_TO_IMMUNE at line 251. Return the error code directly.

### P2-5: False positive NIMCP_THROW_TO_IMMUNE in subscriber_pause and subscriber_resume

**File**: `/home/bbrelin/nimcp/src/middleware/events/nimcp_event_subscriber.c`
**Lines**: 274, 297

**Description**: Both `subscriber_pause()` and `subscriber_resume()` throw to immune when a subscription ID is not found. This is a normal "not found" condition, not an error worth alerting the immune system about.

**Fix**: Remove the NIMCP_THROW_TO_IMMUNE calls at lines 274 and 297.

### P2-6: protocol_metrics error messages reference wrong function name

**File**: `/home/bbrelin/nimcp/src/async/nimcp_protocol_metrics.c`
**Lines**: 270, 289, 297, 326, 375, 390, 397, 443, 466

**Description**: Multiple NIMCP_THROW_TO_IMMUNE calls in `protocol_metrics_record()`, `protocol_metrics_increment()`, `protocol_metrics_observe()`, `protocol_metrics_get()`, and `protocol_metrics_get_summary()` reference the wrong function name "protocol_metrics_destroy" in their error messages. For example, at line 270: `"protocol_metrics_destroy: required parameter is NULL (metrics, name)"` -- but this is actually in `protocol_metrics_record()`.

**Impact**: Misleading error messages make debugging difficult. The wrong function name in the message will send developers looking at the wrong code.

**Fix**: Update each error message to reference the correct enclosing function name.

### P2-7: networking NLP uses predictive_protocol_config_t (not predictive_config_t)

**File**: `/home/bbrelin/nimcp/src/networking/nlp/nimcp_predictive_protocol.c`
**Lines**: 87, 277

**Description**: The networking NLP module defines its own `predictive_protocol_config_t` type at line 87 and uses it at line 277. Meanwhile, the async module uses `predictive_config_t` (in `src/async/nimcp_predictive_protocol.c`). These are two different types for two different predictive protocol implementations. The MEMORY.md correctly notes "predictive_config_t (async) vs predictive_protocol_config_t (networking) confusion". While these are intentionally different types for different modules, the similar naming is a correctness hazard -- if a developer passes the wrong config type to the wrong create function, the compiler may not catch it if the types happen to be structurally compatible.

**Impact**: Potential type confusion if developers mix up the two config types. No current bugs, but a maintainability hazard.

**Fix**: Consider renaming one to make the distinction clearer, e.g., `nlp_predictive_config_t` for the networking variant.

### P2-8: mesh_coordinator_pool election uses simple majority, not BFT quorum

**File**: `/home/bbrelin/nimcp/src/mesh/nimcp_mesh_coordinator_pool.c`
**Lines**: 273-304

**Description**: The `get_election_winner()` function uses simple majority `(active / 2) + 1` for leader election. The comment at line 279 correctly notes this is for crash-fault tolerance, not Byzantine fault tolerance. However, the CLAUDE.md states "Mesh needs 4+ coordinators for BFT; plurality fallback." The code does implement plurality fallback (lines 296-304), which is correct. The BFT concern is architectural -- the current implementation handles crash faults but not Byzantine faults. This is documented and intentional.

**Impact**: No immediate bug, but the system cannot tolerate Byzantine (malicious) coordinators with this quorum formula. For BFT, the quorum should be `(2 * active / 3) + 1`.

**Fix**: If BFT is required, change the quorum calculation. The current implementation is correct for crash-fault tolerance as documented.

### P2-9: bio_async handle_tracker_shutdown sets initialized=false before mutex unlock

**File**: `/home/bbrelin/nimcp/src/async/nimcp_bio_async.c`
**Lines**: 352-357

**Description**: In `handle_tracker_shutdown()`, `g_handle_tracker.initialized` is set to `false` at line 354 before `nimcp_mutex_unlock` at line 356. The comment says "Set initialized=false BEFORE destroying mutex. This prevents other threads from trying to use the mutex after destruction." However, another thread could be checking `initialized` without the mutex (e.g., in `handle_tracker_register` at line 370 or `handle_tracker_remove` at line 414), see `false`, and return early. Meanwhile, the shutdown thread still holds the mutex and hasn't destroyed it yet. This creates a window where a concurrent `bio_alloc` could fall through to `nimcp_malloc` instead of unified memory, but this is safe (just suboptimal). The actual danger is that after `nimcp_mutex_destroy` at line 357, any thread that was blocked on `nimcp_mutex_lock` at lines 381 or 424 will have undefined behavior.

**Impact**: Potential undefined behavior if a thread is blocked on the mutex when it is destroyed.

**Fix**: The current approach is the best available for a global singleton -- setting `initialized = false` before unlock is the right order. The real fix would be to ensure no threads are calling `handle_tracker_register/remove` during shutdown. This is a shutdown-ordering concern, not a code fix.

### P2-10: semantic_compressor error messages reference wrong function names

**File**: `/home/bbrelin/nimcp/src/async/nimcp_semantic_compression.c`
**Lines**: 155, 392, 399

**Description**: At line 155, `find_best_primitive()` has error message "vector_cosine_similarity: required parameter is NULL" -- wrong function name. At line 392, `nimcp_semantic_compressor_create()` has error message "nimcp_semantic_compressor_default_config: config is NULL" -- wrong function name. At line 399, same issue.

**Fix**: Correct the function names in the error messages.

### P2-11: event_queue_get_last_error referenced in error messages that don't match

**File**: `/home/bbrelin/nimcp/src/middleware/events/nimcp_event_queue.c`
**Lines**: 162, 238

**Description**: NIMCP_THROW_TO_IMMUNE calls at lines 162 and 238 reference "event_queue_get_last_error" in their messages, but these are in `event_copy_payload_from_pool()` and other internal functions. The error messages should reference the actual function name where the error occurs.

**Fix**: Correct the function names in error messages.

### P2-12: bio_orchestrator_start unlocks mutex, does work, re-locks -- TOCTOU window

**File**: `/home/bbrelin/nimcp/src/async/nimcp_bio_async_orchestrator.c`
**Lines**: 233-258

**Description**: In `bio_orchestrator_start()`, the function acquires the mutex, sets state to RUNNING, then unlocks the mutex at line 234 to call `bio_orchestrator_discover_all_wiring()` and `bio_orchestrator_invoke_handler_callbacks()`. After these calls return, it re-locks the mutex. The code has a P2 fix using `state_version` to detect if another thread modified state during the unlocked window (lines 250-258). While the detection is there, the function continues regardless even if state changed. This means the "started" log message at line 262 may be printed even if the orchestrator was stopped by another thread during the unlocked window.

**Impact**: The state version check logs a warning but does not correct the situation. The orchestrator may be in STOPPED state when the "started" message is logged.

**Fix**: After re-acquiring the mutex, check if `orchestrator->state == BIO_ORCHESTRATOR_RUNNING` and only log "started" if it still is.

### P2-13: surface_bio_async_bridge send_message_internal fires false positive throw

**File**: `/home/bbrelin/nimcp/src/async/bridges/nimcp_surface_bio_async_bridge.c`
**Line**: 352

**Description**: `send_message_internal()` throws `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "send_message_internal: bridge->connected is NULL")` when `bridge->connected` is false. Being disconnected is not a NULL pointer error -- it is a normal state. The error code and message are both wrong.

**Fix**: Change to a more appropriate handling: either remove the throw entirely (dropping a message when disconnected is expected) or change the error code to `NIMCP_ERROR_INVALID_STATE` with message "send_message_internal: bridge is not connected".

### P2-14: bio_router_register_module creates context OUTSIDE the lock -- race window

**File**: `/home/bbrelin/nimcp/src/async/nimcp_bio_router.c`
**Lines**: 959-977

**Description**: After setting up the new module entry under the modules mutex, the function unlocks at line 959, then allocates the context at line 962. Between unlock and context allocation, another thread could:
1. Register a new module that reuses the same slot (if the new entry's magic was somehow cleared)
2. Call shutdown, which iterates all modules and destroys their inboxes

The entry is valid (magic set, inbox initialized) but has no context handle yet. While unlikely in practice, this is a correctness gap.

**Impact**: Very unlikely race condition, but the architecture allows for it.

**Fix**: Allocate the context before unlocking, or defer the module_count increment until after context allocation succeeds.

---

## P3 Findings (Style/Robustness)

### P3-1: Magic numbers in bio_router signal observer registry

**File**: `/home/bbrelin/nimcp/src/async/nimcp_bio_router.c`
**Lines**: 1789, 1816

**Description**: The signal observer array has a hardcoded capacity of 256 (line 1789: `g_signal_observers[256]`) and the check at line 1816 compares against literal `256`. This should be a named constant like `MAX_SIGNAL_OBSERVERS`.

**Fix**: Define `#define MAX_SIGNAL_OBSERVERS 256` and use it consistently.

### P3-2: Magic numbers in bio_router wave callback registry

**File**: `/home/bbrelin/nimcp/src/async/nimcp_bio_router.c`
**Line**: 2008

**Description**: The wave callback array `g_wave_callbacks[128]` uses a hardcoded 128. Should be `#define MAX_WAVE_CALLBACKS 128`.

**Fix**: Define a named constant.

### P3-3: Missing const qualifier on config parameter getters in mesh_coordinator

**File**: `/home/bbrelin/nimcp/src/mesh/nimcp_mesh_coordinator.c`
**Lines**: 340, 368, 396

**Description**: `mesh_coordinator_get_role()`, `mesh_coordinator_get_state()`, and `mesh_coordinator_get_level()` correctly take `const mesh_coordinator_t*`, which is good. However, `compute_load()` at line 106 also takes const correctly. No issue here -- this was checked and is clean.

### P3-3 (actual): Inconsistent error code usage in router_immune_bridge_create

**File**: `/home/bbrelin/nimcp/src/async/immune/nimcp_bio_router_immune_bridge.c`
**Lines**: 273, 284, 296, 309

**Description**: The error codes used for allocation failures alternate between `NIMCP_ERROR_NO_MEMORY` (lines 273, 284, 296) and `NIMCP_ERROR_NULL_POINTER` (line 309 for `recent_anomalies`). All four are the same type of error (malloc returned NULL), so they should all use `NIMCP_ERROR_NO_MEMORY`.

**Fix**: Change line 309 from `NIMCP_ERROR_NULL_POINTER` to `NIMCP_ERROR_NO_MEMORY`.

### P3-4: bio_async_plasticity_bridge_is_connected reads without lock

**File**: `/home/bbrelin/nimcp/src/async/bridges/nimcp_bio_async_plasticity_bridge.c`
**Lines**: 327-333

**Description**: `bio_async_plasticity_bridge_is_connected()` reads `bridge->connected` without acquiring the bridge lock. While this is a `bool` read (which is typically atomic on modern architectures), the NIMCP codebase convention uses locks for all shared state access in bridge patterns. Other is_connected functions in other bridges also skip the lock, so this appears to be a deliberate choice for performance.

**Impact**: Minor -- torn reads on bool are unlikely on modern hardware.

**Fix**: Consider adding BRIDGE_LOCK/BRIDGE_UNLOCK for consistency, or add a comment explaining why locking is deliberately skipped.

### P3-5: wiring_diagram_load_platform boundary check is off

**File**: `/home/bbrelin/nimcp/src/async/nimcp_wiring_diagram.c`
**Line**: 372

**Description**: The check `if (tier > PLATFORM_TIER_FULL)` validates the platform tier, but the `TIER_NAMES[]` array at line 60 has 7 entries: "basic", "minimal", "constrained", "medium", "full", "neuromorphic", "quantum". If `platform_tier_t` includes values for neuromorphic and quantum tiers beyond `PLATFORM_TIER_FULL`, then `TIER_NAMES[tier]` would be a valid access, but the function rejects them. This may be intentional (only full and below are supported), but the array definition suggests otherwise.

**Impact**: Cannot load wiring for neuromorphic or quantum tiers even though names are defined.

**Fix**: Either restrict `TIER_NAMES[]` to only the supported tiers, or change the check to `tier >= sizeof(TIER_NAMES)/sizeof(TIER_NAMES[0])` to support all defined tiers.

### P3-6: Inconsistent LOG_ERROR format in semantic_compression

**File**: `/home/bbrelin/nimcp/src/async/nimcp_semantic_compression.c`
**Lines**: 309, 314, 321, 329

**Description**: Several `LOG_ERROR` calls in `add_primitive_unlocked()` use a two-argument form: `LOG_ERROR(COMPRESSION_MODULE, "message")`. The NIMCP logging convention uses `LOG_ERROR("format", args...)` without a module name as the first argument. If `LOG_ERROR` is defined as a macro that takes `(module, fmt, ...)` this works, but if it follows the standard `(fmt, ...)` convention, the module name string will be interpreted as the format string, and the actual message will be an extra argument.

**Impact**: Depends on the `LOG_ERROR` macro definition. If it matches the two-arg form, no issue. If not, the log output will be garbled.

**Fix**: Verify the LOG_ERROR macro signature and ensure consistent usage.

---

## Files Reviewed Without Findings

The following files were reviewed and found to have no significant issues (good code quality, proper error handling, correct lock usage):

**Async:**
- `nimcp_bio_async_fep_bridge.c` - FEP bridge uses `return 0` / `return -1` convention correctly
- `nimcp_bio_router_fep_bridge.c` - Proper FEP bridge return conventions
- `nimcp_semantic_compression_fep_bridge.c` - Clean FEP bridge
- `nimcp_predictive_protocol_fep_bridge.c` - Proper FEP bridge
- `nimcp_biological_timescales_fep_bridge.c` - Clean
- `nimcp_async_integration_bridge.c` - Proper integration bridge

**Mesh:**
- `nimcp_mesh_coordinator.c` - Clean lifecycle, proper mutex usage
- `nimcp_mesh_coordinator_pool.c` - Good BFT election with plurality fallback, proper vote duplicate detection
- `nimcp_mesh_channel.c` - Proper resource cleanup in error paths
- `nimcp_mesh_endorsement.c` - Clean endorsement protocol
- `nimcp_mesh_transaction.c` - Proper transaction lifecycle
- `nimcp_mesh_topology.c` - Clean graph operations
- `nimcp_mesh_msp.c` - Proper credential management
- `nimcp_mesh_participant.c` - Clean registry operations
- `nimcp_mesh_ordering.c` - Raft log properly managed
- `nimcp_mesh_bootstrap.c` - Auto-registers 5 modules correctly
- `nimcp_mesh_pattern_cache.c` - CoW cache with proper LRU
- `nimcp_mesh_integration.c` - Clean adapter pattern

**Middleware:**
- `nimcp_event_bus.c` - Good once-flag reset on cleanup (line 90), atomic running flag
- `nimcp_event_queue.c` - Proper heap-based priority queue
- `nimcp_middleware_pipeline.c` - Clean pipeline pattern
- `nimcp_circular_buffer.c` - Proper ring buffer implementation
- `nimcp_sliding_window.c` - Clean windowed buffer
- `nimcp_training_module.c` - Proper training lifecycle
- `nimcp_loss_functions.c` - Clean math implementations
- `nimcp_optimizers.c` - Proper optimizer state management
- `nimcp_gradient_manager.c` - Clean gradient accumulation
- `nimcp_normalization/*.c` - Proper normalizer implementations

**Networking:**
- `nimcp_protocol.c` - Clean serialization with proper BBB integration
- `nimcp_p2pnode.c` - Good hash table peer management, proper validation
- `nimcp_msg_framing.c` - Clean message framing
- `nimcp_msg_router.c` - Proper message routing
- `nimcp_events.c` - Clean event handling

---

## Cross-Cutting Observations

### Pattern: platform_once reset on shutdown

The codebase has a consistent pattern of resetting `nimcp_platform_once_t` variables during shutdown to allow re-initialization. This is correctly done in:
- `bio_async.c` (line 360): `g_handle_tracker_once`
- `bio_router.c` (line 800): `g_router_init_once`
- `event_bus.c` (line 90): `g_security_init_once`
- `nimcp_shannon.c` (line 112): `g_shannon_init_once`

But NOT done for:
- `bio_router.c`: `g_signal_mutex_once`, `g_wave_mutex_once`, `g_subscription_once`, `g_emotion_reg_once` (P1-1)
- `nlp_session.c`: `g_session_bio_once` (P1-2)
- `nlp_crypto.c`: `g_crypto_bio_once` (P1-2)

### Pattern: False positive NIMCP_THROW_TO_IMMUNE on lookup failures

Previous passes fixed many false positives, but several remain in:
- `nimcp_bio_router_immune_bridge.c`: `find_quarantined_node`, `find_inflammation_impact` (P2-1, P2-2)
- `nimcp_replication.c`: `find_brain` (P2-3)
- `nimcp_event_subscriber.c`: `subscriber_unsubscribe`, `subscriber_pause`, `subscriber_resume` (P2-4, P2-5)

All follow the same pattern: a search function that throws to immune when the item is not found, which is a normal code path.

### Pattern: Wrong function names in error messages

Multiple files have NIMCP_THROW_TO_IMMUNE messages that reference the wrong function name. This appears to be a copy-paste artifact from batch error message addition. Found in:
- `nimcp_protocol_metrics.c` (P2-6): 9 instances referencing "protocol_metrics_destroy"
- `nimcp_semantic_compression.c` (P2-10): 3 instances
- `nimcp_event_queue.c` (P2-11): 2 instances

---

## Priority Summary for Remediation

**Immediately actionable (P1):**
1. Reset 4 subsystem once-flags in `bio_router_shutdown()` (P1-1) -- **High impact, easy fix**
2. Add shutdown for NLP session/crypto bio-async contexts (P1-2) -- **Medium complexity**
3. Destroy `g_router_brain_kg_mutex` on shutdown (P1-8) -- **Easy fix**
4. Fix error code in `router_immune_bridge_create` (P1-7) -- **Trivial fix**
5. Fix module entry leak on context alloc failure (P1-4) -- **Medium complexity**

**Should fix (P2):**
1. Remove 5 false positive NIMCP_THROW_TO_IMMUNE (P2-1 through P2-5) -- **Easy, batch fix**
2. Fix ~14 wrong function names in error messages (P2-6, P2-10, P2-11) -- **Easy, tedious**
3. Fix surface bridge false positive throw (P2-13) -- **Trivial**

**Nice to have (P3):**
1. Add named constants for magic numbers (P3-1, P3-2)
2. Fix inconsistent error codes (P3-3)
3. Verify LOG_ERROR macro compatibility (P3-6)
