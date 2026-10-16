# Pass 6 Walkthrough: Emotion Modules (04a2)

**Date**: 2026-02-15
**Scope**: `src/cognitive/emotion/`, `src/cognitive/emotions/`, `src/cognitive/emotional_tagging/`
**Files reviewed**: 12

## Summary

| Priority | Count |
|----------|-------|
| P1       | 2     |
| P2       | 17    |
| **Total**| **19**|

## Findings

| # | File | Line | Issue | Description |
|---|------|------|-------|-------------|
| 1 | emotional_tagging/nimcp_emotional_tagging_substrate_bridge.c | 108 | P1: no mutex init | `emotional_tagging_substrate_bridge_create` uses `nimcp_calloc` but never calls `bridge_base_init()`. The `bridge_base_t base` field has no mutex allocated. Update and get_effects functions have no locking at all -- concurrent calls are thread-unsafe. |
| 2 | emotional_tagging/nimcp_emotional_tagging_substrate_bridge.c | 128 | P1: no mutex cleanup | `emotional_tagging_substrate_bridge_destroy` never calls `bridge_base_cleanup()`. Consistent with no init, but if init is added, cleanup must match. |
| 3 | emotions/nimcp_emotional_system.c | 705 | P2: wrong func name in throw | `emotion_system_set_state` throws with message `"emotion_system_get_tag: system is NULL"` -- wrong function name. |
| 4 | emotions/nimcp_emotional_system.c | 775 | P2: wrong func name in throw | `emotion_system_decay` throws with message `"emotion_system_get_tag: system is NULL"` -- wrong function name. |
| 5 | emotions/nimcp_emotional_system.c | 809 | P2: wrong func name in throw | `emotion_system_update_multimodal` throws with message `"emotion_system_get_tag: system is NULL"` -- wrong function name. |
| 6 | emotions/nimcp_emotional_system.c | 904 | P2: wrong error code + misleading message | `emotion_system_regulate`: check is `!system->config.enable_emotion_regulation` (a bool), but throws `NIMCP_ERROR_NULL_POINTER` with message `"system->config is NULL"`. Should be `NIMCP_ERROR_INVALID_STATE` with accurate message. |
| 7 | emotions/nimcp_emotional_system.c | 928 | P2: wrong error code + misleading message | `emotion_system_auto_regulate`: same bool check throws `NIMCP_ERROR_NULL_POINTER` with message `"system->config is NULL"`. Should be `NIMCP_ERROR_INVALID_STATE`. |
| 8 | emotions/nimcp_emotional_system.c | 955 | P2: false positive throw | `emotion_system_auto_regulate`: when regulation is not needed (`!needs_regulation`), throws `NIMCP_ERROR_NULL_POINTER` with message `"needs_regulation is NULL"`. Not needing regulation is normal behavior, not an error. Remove throw. |
| 9 | emotion/nimcp_emotion_snn_bridge.c | 1004 | P2: wrong error code convention | `emotion_snn_connect_bio_async` returns `NIMCP_ERROR_NULL_POINTER` (large positive int) on NULL guard, but all other functions in the file return `0`/`-1`. Should return `-1`. |
| 10 | emotion/nimcp_emotion_snn_bridge.c | 1025 | P2: wrong error code convention | `emotion_snn_disconnect_bio_async` same issue: returns `NIMCP_ERROR_NULL_POINTER` instead of `-1`. |
| 11 | emotion/nimcp_emotion_substrate_bridge.c | 364 | P2: wrong error code | `emotion_substrate_bridge_create`: allocation failure throws `NIMCP_ERROR_NULL_POINTER` with message `"bridge is NULL"`. Should be `NIMCP_ERROR_NO_MEMORY` with message `"Failed to allocate bridge"`. |
| 12 | emotion/nimcp_emotion_plasticity_bridge.c | 536 | P2: false positive throw | `emotion_plasticity_unregister_synapse`: synapse not found throws `NIMCP_ERROR_INVALID_PARAM` with message `"operation failed"`. Not-found is a normal search result, not an error. Remove throw. |
| 13 | emotion/nimcp_emotion_plasticity_bridge.c | 564 | P2: false positive throw | `emotion_plasticity_get_synapse`: synapse not found throws `NIMCP_ERROR_INVALID_PARAM` with message `"validation failed"`. Not-found is a normal lookup result. Remove throw. |
| 14 | emotion/nimcp_health_emotion_bridge.c | 498 | P2: false positive throw | `health_emotion_permits_action`: action denied due to emotional instability throws `NIMCP_ERROR_INVALID_PARAM`. Denying actions is normal gating behavior. Fires on every denied action. Remove throw. |
| 15 | emotion/nimcp_health_emotion_bridge.c | 507 | P2: false positive throw | `health_emotion_permits_action`: action denied under stress throws `NIMCP_ERROR_INVALID_PARAM`. Same issue -- normal gating. Remove throw. |
| 16 | emotion/nimcp_health_emotion_bridge.c | 516 | P2: false positive throw | `health_emotion_permits_action`: action denied during emotional crisis throws `NIMCP_ERROR_INVALID_PARAM`. Same issue -- normal gating. Remove throw. |
| 17 | emotional_tagging/nimcp_emotional_tagging.c | 596 | P2: false positive throw | `emotional_tag_is_valid`: NULL tag throws `NIMCP_ERROR_NULL_POINTER`. This is a validation/query function -- NULL is a valid query input meaning "not valid". Should just return false without throwing. |
| 18 | emotional_tagging/nimcp_emotional_tagging_substrate_bridge.c | 152 | P2: wrong error code | `emotional_tagging_substrate_bridge_update`: when `substrate_get_metabolic_state()` fails, throws `NIMCP_ERROR_INVALID_PARAM` with message `"validation failed"`. Should propagate actual substrate error or use `NIMCP_ERROR_INVALID_STATE`. |
| 19 | emotion/nimcp_emotion_fep_bridge.c | 244 | P2: implicit const-cast | `emotion_fep_bridge_get_state` and `get_stats` pass `const` bridge's `base.mutex` to `nimcp_platform_mutex_lock()` which takes non-const pointer. Implicit const discard. Same pattern as SNN bridge const-casts but less explicit. |

## Notes

- **emotion/nimcp_emotion_thalamic_bridge.c**: Clean. No issues found.
- **emotions/nimcp_emotional_system_sleep_bridge.c**: Clean. No issues found.
- **emotional_tagging/nimcp_emotional_tagging_fep_bridge.c**: Clean. Well-structured.
- **emotional_tagging/nimcp_emotional_tagging_thalamic_bridge.c**: Clean. No issues found.
- **emotion/nimcp_emotion_snn_bridge.c** lines 941, 956, 974, 978: Explicit const-casts `((emotion_snn_bridge_t*)bridge)->base.mutex` in const getters. Acceptable pattern for const getters needing mutex, but noted for completeness.
- **emotion/nimcp_emotion_plasticity_bridge.c** lines 1068, 1077, 1095, 1107: Same explicit const-cast pattern in const getters.
- **emotion/nimcp_emotion_fep_bridge.c**: Uses `nimcp_platform_mutex_lock/unlock` (platform layer) while other bridges use `nimcp_mutex_lock/unlock` (thread layer). Both work but inconsistent.
