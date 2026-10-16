# Pass 6 Walkthrough: NLP Bridges (07b3)

**Scope**: `src/nlp/*.c` and `src/nlp/immune/*.c` (6 files, 3557 lines)

**Files reviewed**:
- `src/nlp/nimcp_nlp.c` (901 lines)
- `src/nlp/nimcp_spike_nlp.c` (592 lines)
- `src/nlp/nimcp_multimodal_nlp_bridge.c` (576 lines)
- `src/nlp/immune/nimcp_nlp_immune_bridge.c` (611 lines)
- `src/nlp/immune/nimcp_spike_nlp_immune_bridge.c` (387 lines)
- `src/nlp/immune/nimcp_multimodal_nlp_immune_bridge.c` (490 lines)

---

## Summary

| Severity | Count |
|----------|-------|
| P1       | 1     |
| P2       | 19    |

---

## Findings

| # | File | Line | Issue | Description |
|---|------|------|-------|-------------|
| 1 | nimcp_multimodal_nlp_bridge.c | 38-39, 119 | P1 race | Global `g_phoneme_lexicon` and `g_lexicon_size` are unprotected by any mutex. `multimodal_nlp_phonemes_to_tokens()` lazily initializes at line 119 (`if (!g_phoneme_lexicon)`) and reads globals without locking. Concurrent callers can double-malloc (leak) or read partially-initialized data. |
| 2 | nimcp_multimodal_nlp_bridge.c | 230 | P2 wrong error code | `NIMCP_ERROR_INVALID_PARAM` used for `multimodal_nlp_phonemes_to_tokens()` failure. Should be `NIMCP_ERROR_OPERATION_FAILED`. Message says "operation failed" but code says INVALID_PARAM. |
| 3 | nimcp_multimodal_nlp_bridge.c | 248 | P2 wrong error code | `NIMCP_ERROR_INVALID_PARAM` used for `nlp_network_forward()` failure. Should be `NIMCP_ERROR_OPERATION_FAILED`. |
| 4 | nimcp_multimodal_nlp_bridge.c | 274 | P2 wrong error code + wrong message | `NIMCP_ERROR_INVALID_PARAM` with message "audio_cortex_process is NULL". The function returned false (processing failed), not a NULL pointer. Should be `NIMCP_ERROR_OPERATION_FAILED` with accurate message like "audio_cortex_process failed". |
| 5 | nimcp_multimodal_nlp_bridge.c | 407 | P2 wrong error code + wrong message | `NIMCP_ERROR_INVALID_PARAM` with message "visual_cortex_process is NULL". Same pattern as #4 -- function returned false, not NULL. Should be `NIMCP_ERROR_OPERATION_FAILED`. |
| 6 | nimcp_multimodal_nlp_bridge.c | 442 | P2 wrong error code + wrong message | `NIMCP_ERROR_INVALID_PARAM` with message "visual_cortex_process is NULL" for a boolean false return. Should be `NIMCP_ERROR_OPERATION_FAILED` with "visual_cortex_process failed". |
| 7 | nimcp_multimodal_nlp_bridge.c | 451 | P2 wrong error code + wrong message | `NIMCP_ERROR_INVALID_PARAM` with message "multimodal_nlp_visual_to_tokens is NULL". Function returned false, not NULL. Should be `NIMCP_ERROR_OPERATION_FAILED`. |
| 8 | nimcp_multimodal_nlp_bridge.c | 463 | P2 wrong error code + wrong message | `NIMCP_ERROR_INVALID_PARAM` with message "nlp_network_forward is NULL". Function returned false, not NULL. Should be `NIMCP_ERROR_OPERATION_FAILED`. |
| 9 | nimcp_nlp.c | 487 | P2 overflow | `nimcp_malloc(sequence_length * embedding_dim * sizeof(float))` -- both `sequence_length` and `embedding_dim` are `uint32_t`. Their product can overflow `uint32_t` before promotion to `size_t`, allocating a too-small buffer. Subsequent `memcpy` at line 505 would write out of bounds. |
| 10 | nimcp_nlp.c | 314 | P2 overflow | `max_seq_len * config->attention_config.output_dim` stored in `uint32_t attention_output_size`. No overflow check. If both are large, the product wraps and a too-small buffer is allocated at line 315. |
| 11 | nimcp_nlp.c | 812 | P2 overflow | `uint32_t total_elements = sequence_length * output_dim` -- no overflow check. Used to iterate `output[]` and `target[]` arrays for loss computation. If overflowed, would read out of bounds. |
| 12 | nimcp_spike_nlp.c | 51-57 | P2 fragile struct aliasing | Redeclares `struct neural_network_struct` with only the first 5 fields. If the real struct changes field order or inserts fields before these, all field accesses will silently read wrong memory. This is undefined behavior if layouts diverge. |
| 13 | nimcp_spike_nlp_immune_bridge.c | 49-66 | P2 unused parameter + assumes sorted | `compute_spike_synchrony()` accepts `time_window_ms` but never uses it. Also assumes `spike_times` array is sorted ascending -- if not, `uint64_t` subtraction at line 59 wraps to a huge value and synchrony is underreported. |
| 14 | nimcp_spike_nlp_immune_bridge.c | 231 | P2 wrong error code | `NIMCP_ERROR_NULL_POINTER` thrown when `num_spikes == 0`. Zero spike count is not a null pointer error. Should be `NIMCP_ERROR_INVALID_ARGUMENT` for the `num_spikes == 0` case, or split the guard into separate checks. |
| 15 | nimcp_spike_nlp_immune_bridge.c | 268 | P2 wrong error code | Same pattern as #14: `NIMCP_ERROR_NULL_POINTER` thrown for `num_spikes == 0` in `spike_nlp_immune_release_il10_from_healthy`. |
| 16 | nimcp_spike_nlp_immune_bridge.c | 200-207 | P2 wrong inflammation level | When `stats.inflammation_sites == 0`, code falls through to `level = INFLAMMATION_LOCAL` (since `0 < 3` is true). Compare with `nimcp_nlp_immune_bridge.c:257` which correctly checks `== 0` first and assigns `INFLAMMATION_NONE`. Inflammation effects are applied even when there is no inflammation. |
| 17 | nimcp_multimodal_nlp_immune_bridge.c | 188-195 | P2 wrong inflammation level | Same as #16. When `stats.inflammation_sites == 0`, `level = INFLAMMATION_LOCAL` instead of `INFLAMMATION_NONE`. Missing check for the zero-sites case. |
| 18 | nimcp_nlp_immune_bridge.c | 144-148 | P2 silent mutex alloc failure | If `nimcp_malloc(sizeof(nimcp_mutex_t))` fails, `bridge->base.mutex` stays NULL. The bridge is still returned to the caller. Update functions do check for NULL mutex before locking (safe), but the failure is never logged or reported. |
| 19 | nimcp_spike_nlp_immune_bridge.c | 127-131 | P2 silent mutex alloc failure | Same pattern as #18. Mutex allocation failure silently ignored. |
| 20 | nimcp_multimodal_nlp_immune_bridge.c | 109-113 | P2 silent mutex alloc failure | Same pattern as #18 and #19. Mutex allocation failure silently ignored. |
