# Pass 6 Walkthrough: Mesh Bridges & Region Integrations (07a2)

**Date**: 2026-02-15
**Files Reviewed**: 13
**Scope**: `src/mesh/` bridge and integration files

---

## P1 - Critical (NULL deref, races, deadlocks, OOB)

| # | File | Line | Issue | Description |
|---|------|------|-------|-------------|
| 1 | nimcp_mesh_integration.c | 1009 | NULL deref | `mesh_ordering_submit(integration->ordering, tx)` -- `ordering` can be NULL (line 208 shows creation is non-fatal: "will operate without ordering"), but `submit_transaction()` does not check for NULL before calling. Crash if ordering service failed to create. |
| 2 | nimcp_mesh_integration.c | 521-525 | Race | `mesh_integration_get_adapter()` scans `adapters[]` and `adapter_count` without holding mutex, while `register_adapter()` (line 455) and `unregister_adapter()` (line 486) modify the array under the mutex. Concurrent register+get = torn read / stale pointer. |
| 3 | nimcp_mesh_brain_integration.c | 859-866 | Deadlock | `mesh_brain_integration_unregister_brain()` locks `integration->mutex` at line 859, then calls `mesh_brain_integration_unregister_module()` at line 864, which also locks the same mutex at line 587. Non-recursive mutex = deadlock. |
| 4 | nimcp_mesh_bio_integration.c | 584-585 | Race | `route_message()` unlocks mutex at line 584, then increments `integration->stats.direct_fallback++` at line 585 without lock. Concurrent callers lose stat increments. Same pattern at lines 605-606 and 699-700. |
| 5 | nimcp_mesh_kg_routing_bridge.c | all | Race (no mutex) | Struct `mesh_kg_routing_bridge_t` has NO mutex at all. Every other mesh bridge has one. All public functions that read/write `module_count`, `modules[]`, `topology_cache`, `stats` are thread-unsafe. |

**P1 Total: 5**

---

## P2 - Moderate (Wrong error codes, false positive throws, wrong func names)

| # | File | Line | Issue | Description |
|---|------|------|-------|-------------|
| 1 | nimcp_mesh_integration.c | 340 | Wrong error code | `NIMCP_ERROR_NULL_POINTER` used for `channel_id >= MESH_NUM_STANDARD_CHANNELS` -- should be `NIMCP_ERROR_INVALID_PARAM` or `NIMCP_ERROR_OUT_OF_RANGE`. |
| 2 | nimcp_mesh_integration.c | 393 | Wrong error code | `NIMCP_ERROR_NULL_POINTER` used for `channel_id >= MESH_NUM_STANDARD_CHANNELS` in `get_coordinator_pool` -- same issue as above. |
| 3 | nimcp_mesh_integration.c | 508 | False positive throw | `unregister_adapter` not-found path throws to immune. Attempting to unregister an ID that doesn't exist is a caller error, not an immune-worthy event. |
| 4 | nimcp_mesh_integration.c | 527 | False positive throw + wrong error code + wrong msg | `get_adapter` not-found throws `NIMCP_ERROR_NULL_POINTER` with message "integration is NULL". Should be `NIMCP_ERROR_NOT_FOUND` with "adapter not found". Lookup failure is not immune-worthy. |
| 5 | nimcp_mesh_bio_bridge.c | 251 | Wrong error code | `NIMCP_ERROR_NO_MEMORY` when `bootstrap` is NULL -- should be `NIMCP_ERROR_NULL_POINTER`. |
| 6 | nimcp_mesh_bio_bridge.c | 336 | False positive throw | Already-connected state throws `NIMCP_ERROR_ALREADY_EXISTS` to immune. Idempotent connect attempt is not an immune-worthy error. |
| 7 | nimcp_mesh_bio_bridge.c | 501 | Wrong error code | `NIMCP_ERROR_OUT_OF_MEMORY` instead of project standard `NIMCP_ERROR_NO_MEMORY` for allocation failure. |
| 8 | nimcp_mesh_bio_integration.c | 263 | False positive throw + wrong error code | `get_priority_policy` internal helper throws `NIMCP_ERROR_NULL_POINTER` when policy not found. Not-found in a lookup is normal. Should be silent return NULL. |
| 9 | nimcp_mesh_bio_integration.c | 383 | Wrong error code | `NIMCP_ERROR_NO_MEMORY` when `bootstrap` is NULL -- should be `NIMCP_ERROR_NULL_POINTER`. |
| 10 | nimcp_mesh_bio_integration.c | 492 | False positive throw | Already-connected state throws to immune. Same pattern as bio_bridge. |
| 11 | nimcp_mesh_bio_integration.c | 552 | False positive throw | `mesh_bio_integration_mesh_available()` is a boolean query. Throwing `NIMCP_ERROR_INVALID_PARAM` for NULL input on a query function is excessive. |
| 12 | nimcp_mesh_bio_integration.c | 588 | False positive throw | `route_message` fallback-to-direct path throws `NIMCP_ERROR_NOT_FOUND`. Fallback routing is normal control flow, not an error condition. |
| 13 | nimcp_mesh_bio_integration.c | 591 | False positive throw | `route_message` not-initialized path throws `NIMCP_ERROR_NOT_INITIALIZED`. System not being ready is not immune-worthy. |
| 14 | nimcp_mesh_bio_integration.c | 607 | False positive throw | Route decision rejection path throws `NIMCP_ERROR_NOT_FOUND`. Custom routing decision saying "don't route" is normal flow. |
| 15 | nimcp_mesh_bio_integration.c | 616 | False positive throw | Broadcast not-routed path throws `NIMCP_ERROR_NOT_FOUND`. Not routing broadcasts is configured behavior. |
| 16 | nimcp_mesh_bio_integration.c | 701 | False positive throw | Fallback-after-routing-failure throws `NIMCP_ERROR_NOT_FOUND`. Fallback is expected recovery, not immune-worthy. |
| 17 | nimcp_mesh_bio_integration.c | 1019 | False positive throw | `mesh_bio_routing_hook_impl` throws when integration is invalid. Hook not finding handler is normal (continue to next handler). |
| 18 | nimcp_mesh_bio_integration.c | 1024 | False positive throw | Hook throws when integration is disabled. Disabled integration is configuration, not an error. |
| 19 | nimcp_mesh_bio_integration.c | 1036 | False positive throw | `get_hook()` throws `NIMCP_ERROR_NULL_POINTER` for NULL integration. This is a getter -- should just return NULL silently. |
| 20 | nimcp_mesh_brain_integration.c | 287 | False positive throw + wrong error code + wrong msg | `mesh_brain_region_get_receptive_field` default case throws `NIMCP_ERROR_NULL_POINTER` with "operation failed". Unknown region is `NIMCP_ERROR_INVALID_PARAM` and is a normal lookup result. |
| 21 | nimcp_mesh_brain_integration.c | 488 | False positive throw | Already-registered region throws to immune. Idempotent registration is not immune-worthy. |
| 22 | nimcp_mesh_brain_integration.c | 591 | False positive throw | Unregister not-found throws to immune. Attempting to unregister something not registered is a caller issue, not immune-worthy. |
| 23 | nimcp_mesh_health_bridge.c | 106 | Wrong error code | `NIMCP_ERROR_NO_MEMORY` when `bootstrap` is NULL -- should be `NIMCP_ERROR_NULL_POINTER`. |
| 24 | nimcp_mesh_health_bridge.c | 336 | False positive throw | Unregister not-found throws to immune. Not finding a record during unregister is a normal condition. |
| 25 | nimcp_mesh_health_bridge.c | 434 | False positive throw | `update_metrics` not-found throws to immune. Participant not being registered is a caller issue, not immune-worthy. |
| 26 | nimcp_mesh_health_bridge.c | 523 | False positive throw | `get_health` not-found throws to immune. Query returning not-found is normal. |
| 27 | nimcp_mesh_exception_bridge.c | 112 | Wrong error code | `NIMCP_ERROR_NO_MEMORY` when `bootstrap` is NULL -- should be `NIMCP_ERROR_NULL_POINTER`. |
| 28 | nimcp_mesh_exception_bridge.c | 527-528 | Missing bounds check | `severity_counts[antigen.severity]` and `category_counts[antigen.category]` -- no bounds check before array index. Currently safe because `classify()` only assigns enum values, but defensive code should validate `antigen.severity < 6` and `antigen.category < 10`. |
| 29 | nimcp_mesh_exception_bridge.c | 696 | Missing bounds check | Same as above -- `severity_counts[antigen.severity]++` in `route_error()` without bounds validation. |
| 30 | nimcp_mesh_kg_routing_bridge.c | 79 | False positive throw + wrong error code | `find_module` throws `NIMCP_ERROR_NULL_POINTER` when module not found. Not-found is normal lookup behavior, not immune-worthy. Should return NULL silently. |
| 31 | nimcp_mesh_kg_routing_bridge.c | 92 | False positive throw + wrong error code | `modules_connected_direct` throws `NIMCP_ERROR_NULL_POINTER` when from_mod or to_mod is NULL (from find_module returning NULL). This is a normal condition. |
| 32 | nimcp_mesh_kg_routing_bridge.c | 96 | False positive throw + wrong error code | Same function throws `NIMCP_ERROR_NULL_POINTER` when modules lack wiring info. Modules not having wiring is normal. |
| 33 | nimcp_mesh_kg_routing_bridge.c | 151 | Wrong error code | `NIMCP_ERROR_NO_MEMORY` when `router` is NULL -- should be `NIMCP_ERROR_NULL_POINTER`. |
| 34 | nimcp_mesh_hippocampus_integration.c | 411 | Wrong error code | `NIMCP_ERROR_OUT_OF_MEMORY` instead of project standard `NIMCP_ERROR_NO_MEMORY`. |
| 35 | nimcp_mesh_hippocampus_integration.c | 431 | Wrong error code | `NIMCP_ERROR_NO_MEMORY` when mutex creation fails -- should be `NIMCP_ERROR_NULL_POINTER` (mutex is NULL). |
| 36 | nimcp_mesh_amygdala_integration.c | 554 | Wrong error code | `NIMCP_ERROR_OUT_OF_MEMORY` instead of project standard `NIMCP_ERROR_NO_MEMORY`. |
| 37 | nimcp_mesh_amygdala_integration.c | 584 | Wrong error code | `NIMCP_ERROR_NO_MEMORY` when mutex creation fails -- should be `NIMCP_ERROR_NULL_POINTER`. |
| 38 | nimcp_mesh_basal_ganglia_integration.c | 314 | Wrong error code | `NIMCP_ERROR_OUT_OF_MEMORY` instead of project standard `NIMCP_ERROR_NO_MEMORY`. |
| 39 | nimcp_mesh_basal_ganglia_integration.c | 333 | Wrong error code | `NIMCP_ERROR_NO_MEMORY` when mutex creation fails -- should be `NIMCP_ERROR_NULL_POINTER`. |
| 40 | nimcp_mesh_global_workspace_integration.c | 443 | Wrong error code | `NIMCP_ERROR_OUT_OF_MEMORY` instead of project standard `NIMCP_ERROR_NO_MEMORY`. |
| 41 | nimcp_mesh_global_workspace_integration.c | 462 | Wrong error code | `NIMCP_ERROR_NO_MEMORY` when mutex creation fails -- should be `NIMCP_ERROR_NULL_POINTER`. |
| 42 | nimcp_mesh_medulla_integration.c | 581 | Wrong error code | `NIMCP_ERROR_OUT_OF_MEMORY` instead of project standard `NIMCP_ERROR_NO_MEMORY`. |
| 43 | nimcp_mesh_medulla_integration.c | 611 | Wrong error code | `NIMCP_ERROR_NO_MEMORY` when mutex creation fails -- should be `NIMCP_ERROR_NULL_POINTER`. |
| 44 | nimcp_mesh_thalamus_integration.c | 353 | Wrong error code | `NIMCP_ERROR_OUT_OF_MEMORY` instead of project standard `NIMCP_ERROR_NO_MEMORY`. |
| 45 | nimcp_mesh_thalamus_integration.c | 381 | Wrong error code | `NIMCP_ERROR_NO_MEMORY` when mutex creation fails -- should be `NIMCP_ERROR_NULL_POINTER`. |

**P2 Total: 45**

---

## P3 - Minor (Style, messages)

| # | File | Line | Issue | Description |
|---|------|------|-------|-------------|
| 1 | nimcp_mesh_integration.c | 527 | Wrong message | Throw message says "integration is NULL" but actual condition is adapter not found by participant_id. |
| 2 | nimcp_mesh_bio_integration.c | 263 | Wrong message | Throw message says "validation failed" but actual condition is priority policy not found in lookup. |
| 3 | nimcp_mesh_bio_integration.c | 588 | Vague message | Multiple throws use generic "error condition" message that provides no diagnostic value. |
| 4 | nimcp_mesh_bio_integration.c | 607 | Vague message | Same generic "error condition" pattern. |
| 5 | nimcp_mesh_bio_integration.c | 616 | Vague message | Same generic "error condition" pattern. |
| 6 | nimcp_mesh_bio_integration.c | 701 | Vague message | Same generic "error condition" pattern. |
| 7 | nimcp_mesh_bio_integration.c | 1019 | Vague message | Same generic "error condition" pattern. |
| 8 | nimcp_mesh_brain_integration.c | 287 | Wrong message | Says "operation failed" but should describe "unknown brain region". |
| 9 | nimcp_mesh_brain_integration.c | 488 | Vague message | Generic "error condition" for already-exists state. |
| 10 | nimcp_mesh_brain_integration.c | 591 | Vague message | Generic "error condition" for not-found state. |
| 11 | nimcp_mesh_health_bridge.c | 336 | Vague message | Generic "error condition" for not-found. |
| 12 | nimcp_mesh_health_bridge.c | 434 | Vague message | Generic "error condition" for not-found. |
| 13 | nimcp_mesh_health_bridge.c | 523 | Vague message | Generic "error condition" for not-found. |
| 14 | nimcp_mesh_kg_routing_bridge.c | 79 | Wrong message | Says "validation failed" but actual condition is module not found. |

**P3 Total: 14**

---

## Summary

| Severity | Count |
|----------|-------|
| P1 - Critical | 5 |
| P2 - Moderate | 45 |
| P3 - Minor | 14 |
| **Total** | **64** |

### Systemic Patterns

| Pattern | Count | Files Affected |
|---------|-------|----------------|
| Wrong error code (NIMCP_ERROR_NO_MEMORY for NULL param) | 8 | bio_bridge, bio_integration, health_bridge, exception_bridge, kg_routing_bridge, hippocampus, amygdala, basal_ganglia, gw, medulla, thalamus |
| Wrong error code (NIMCP_ERROR_OUT_OF_MEMORY vs NO_MEMORY) | 7 | bio_bridge, hippocampus, amygdala, basal_ganglia, gw, medulla, thalamus |
| Wrong error code (NIMCP_ERROR_NULL_POINTER for non-null conditions) | 4 | integration (x2), kg_routing_bridge (x2) |
| False positive throw (lookup not-found) | 15 | integration, bio_bridge, bio_integration, brain_integration, health_bridge, kg_routing_bridge |
| False positive throw (already-exists/connected) | 3 | bio_bridge, bio_integration, brain_integration |
| False positive throw (normal control flow) | 6 | bio_integration (fallback routing, hook, disabled state) |
| Missing thread safety | 2 | integration (get_adapter), kg_routing_bridge (entire file) |
| Vague/wrong throw messages | 14 | Multiple files |

### Recommended Fix Priority

1. **P1 #3 (Deadlock)**: `unregister_brain` must either use recursive mutex or call an `_unlocked()` helper for `unregister_module`. Highest risk -- will deadlock on any brain teardown.
2. **P1 #1 (NULL deref)**: `submit_transaction` must check `integration->ordering != NULL` before calling `mesh_ordering_submit`.
3. **P1 #5 (No mutex in kg_routing_bridge)**: Add mutex to struct and lock/unlock in all public functions.
4. **P1 #2 (get_adapter race)**: Add mutex lock/unlock around the adapter scan in `get_adapter`.
5. **P1 #4 (Stats race)**: Move `stats.direct_fallback++` before `nimcp_mutex_unlock()` in all three locations.
6. **P2 batch**: Fix all 8 instances of `NIMCP_ERROR_NO_MEMORY` for NULL params (sed replacement). Fix all 7 `NIMCP_ERROR_OUT_OF_MEMORY` to `NIMCP_ERROR_NO_MEMORY`. Remove ~24 false positive throws.
