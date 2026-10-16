# Pass 6 Walkthrough: Networking NLP + Immune (07b2)

**Date**: 2026-02-15
**Scope**: `src/networking/nlp/` (12 files) + `src/networking/immune/` (3 files)
**Mode**: Review only (no edits)

## Summary

| Severity | Count |
|----------|-------|
| P1 (crash/race/overflow) | 8 |
| P2 (wrong code/false throw/leak) | 67 |
| **Total** | **75** |

### Systemic Patterns
- **~50 wrong function names in NIMCP_THROW_TO_IMMUNE messages** - copy-paste from template functions, especially in compression and neural language
- **~15 false positive throws on normal search-miss paths** - "not found" is not an error
- **~10 wrong/swapped error codes** - NIMCP_ERROR_NO_MEMORY vs NIMCP_ERROR_NULL_POINTER confusion
- **3 VLA stack overflow risks** - unbounded `float array[dim]` on stack
- **6 unaligned memory access** - `*(uint32_t*)ptr` casts on arbitrary byte pointers
- **2 unchecked filter allocations** - NULL filters will crash later
- **3 memory leaks on error paths** - sub-allocations not freed before bridge free

---

## Issues

| # | File | Line | Issue | Description |
|---|------|------|-------|-------------|
| 1 | nimcp_predictive_protocol.c | 167 | P2 false-positive throw | `NIMCP_THROW_TO_IMMUNE` when `chain == NULL` in `find_pattern` - normal "not found" path, not an error |
| 2 | nimcp_predictive_protocol.c | 180 | P2 false-positive throw | `NIMCP_THROW_TO_IMMUNE` when pattern not found in chain traversal - normal search miss |
| 3 | nimcp_predictive_protocol.c | 548-550 | P2 false-positive throw | `NIMCP_THROW_TO_IMMUNE` when no patterns exist (`chain == NULL`) in `predictive_predict` - empty state is not an error |
| 4 | nimcp_predictive_protocol.c | 581 | P2 false-positive throw | `NIMCP_THROW_TO_IMMUNE` when prediction confidence below threshold - normal result, not immune-worthy |
| 5 | nimcp_predictive_protocol.c | 694 | P2 wrong error code | `NIMCP_ERROR_NULL_POINTER` for "prefetch not enabled" - should be `NIMCP_ERROR_INVALID_STATE` or `NIMCP_ERROR_NOT_INITIALIZED` |
| 6 | nimcp_predictive_protocol.c | 748 | P2 wrong error code | Same as #5 - `NIMCP_ERROR_NULL_POINTER` for "prefetch not enabled" |
| 7 | nimcp_predictive_protocol.c | 778 | P2 false-positive throw | `NIMCP_THROW_TO_IMMUNE` on cache miss in prefetch - normal cache behavior |
| 8 | nimcp_neural_language.c | 248 | P2 wrong func name | Throw message says "nlang_expr_finalize" but function is `nlang_expr_serialize` |
| 9 | nimcp_neural_language.c | 255 | P2 wrong func name | Throw message says "nlang_expr_finalize" but function is `nlang_expr_serialize` |
| 10 | nimcp_neural_language.c | 308 | P2 wrong func name | Throw message says "nlang_expr_finalize" but function is `nlang_expr_deserialize` |
| 11 | nimcp_neural_language.c | 315 | P2 wrong func name | Throw message says "nlang_expr_finalize" but function is `nlang_expr_deserialize` |
| 12 | nimcp_neural_language.c | 327 | P2 wrong func name | Throw message says "nlang_expr_finalize" but function is `nlang_expr_deserialize` |
| 13 | nimcp_neural_language.c | 335 | P2 wrong func name | Throw message says "nlang_expr_finalize" but function is `nlang_expr_deserialize` |
| 14 | nimcp_neural_language.c | 341 | P2 wrong func name | Throw message says "nlang_expr_finalize" but function is `nlang_expr_deserialize` |
| 15 | nimcp_neural_language.c | 349 | P2 wrong func name | Throw message says "nlang_expr_finalize" but function is `nlang_expr_deserialize` |
| 16 | nimcp_neural_language.c | 358 | P2 wrong func name | Throw message says "nlang_expr_finalize" but function is `nlang_expr_deserialize` |
| 17 | nimcp_neural_language.c | 367 | P2 wrong func name | Throw message says "nlang_expr_finalize" but function is `nlang_expr_deserialize` |
| 18 | nimcp_neural_language.c | 375 | P2 wrong func name | Throw message says "nlang_expr_finalize" but function is `nlang_expr_deserialize` |
| 19 | nimcp_neural_language.c | 395 | P2 wrong func name | Throw message says "nlang_expr_finalize" but function is `nlang_expr_deserialize` |
| 20 | nimcp_neural_language.c | 402 | P2 wrong func name | Throw message says "nlang_expr_finalize" but function is `nlang_expr_deserialize` |
| 21 | nimcp_neural_language.c | 438 | P2 false-positive throw | `NIMCP_THROW_TO_IMMUNE` on NULL expr in `nlang_expr_validate` - validation returning false is normal |
| 22 | nimcp_neural_language.c | 471 | P2 false-positive throw | `NIMCP_THROW_TO_IMMUNE` on NULL expr in `nlang_expr_verify_checksum` - verification returning false is normal |
| 23 | nimcp_neural_language.c | 760 | P2 wrong func name | Throw message says "nlang_context_init" but function is `nlang_context_define` |
| 24 | nimcp_neural_language.c | 793 | P2 wrong func name | Throw message says "nlang_context_init" but function is `nlang_context_lookup` |
| 25 | nimcp_dialect_learning.c | 155 | P2 false-positive throw | `NIMCP_THROW_TO_IMMUNE` in `find_dialect_entry` when `dl == NULL` - guard clause, not immune-level |
| 26 | nimcp_dialect_learning.c | 172 | P2 false-positive throw | `NIMCP_THROW_TO_IMMUNE` when dialect not found - normal search miss |
| 27 | nimcp_dialect_learning.c | 294 | P1 stack overflow | VLA `float error[dim]` - unbounded dimension could overflow stack |
| 28 | nimcp_dialect_learning.c | 496 | P1 stack overflow | VLA `float predicted[signal_size]` - unbounded signal_size could overflow stack |
| 29 | nimcp_dialect_learning.c | 578 | P1 stack overflow | VLA `float predicted[signal_size]` - unbounded signal_size could overflow stack |
| 30 | nimcp_dialect_learning.c | 697 | P2 shallow copy | `*dialect = entry->dialect` copies `translation_matrix` pointer - caller gets internal pointer that becomes dangling if entry is modified/freed |
| 31 | nimcp_nlp_compression.c | 194 | P2 wrong func name | Throw message says "compute_adler16" but function is `rle_compress` |
| 32 | nimcp_nlp_compression.c | 238 | P2 wrong func name | Throw message says "compute_adler16" but function is `rle_decompress` |
| 33 | nimcp_nlp_compression.c | 251 | P2 wrong func name | Throw message says "compute_adler16" but function is `rle_decompress` |
| 34 | nimcp_nlp_compression.c | 278 | P2 wrong func name | Throw message says "compute_adler16" but function is `delta_compress` |
| 35 | nimcp_nlp_compression.c | 282 | P2 wrong func name | Throw message says "compute_adler16" but function is `delta_compress` |
| 36 | nimcp_nlp_compression.c | 301 | P2 wrong func name | Throw message says "compute_adler16" but function is `delta_decompress` |
| 37 | nimcp_nlp_compression.c | 305 | P2 wrong func name | Throw message says "compute_adler16" but function is `delta_decompress` |
| 38 | nimcp_nlp_compression.c | 353 | P2 false-positive throw | `NIMCP_THROW_TO_IMMUNE` in `dict_find_pattern` when pattern not found - normal search miss |
| 39 | nimcp_nlp_compression.c | 363 | P2 wrong func name | Throw message says "dict_find_pattern" but function is `dict_compress` |
| 40 | nimcp_nlp_compression.c | 393 | P2 wrong func name | Throw message says "dict_find_pattern" but function is `dict_decompress` |
| 41 | nimcp_nlp_compression.c | 407 | P2 wrong func name | Throw message says "dict_find_pattern" but function is `dict_decompress` |
| 42 | nimcp_nlp_compression.c | 415 | P2 wrong func name | Throw message says "dict_find_pattern" but function is `dict_decompress` |
| 43 | nimcp_nlp_compression.c | 441 | P2 wrong func name | Throw message says "dict_find_pattern" but function is `lz77_compress` |
| 44 | nimcp_nlp_compression.c | 480 | P2 wrong func name | Throw message says "dict_find_pattern" but function is `lz77_decompress` |
| 45 | nimcp_nlp_compression.c | 496-497 | P2 wrong func name | Throw message says "dict_find_pattern" but function is `lz77_decompress` |
| 46 | nimcp_nlp_compression.c | 503 | P2 wrong func name | Throw message says "dict_find_pattern" but function is `lz77_decompress` |
| 47 | nimcp_nlp_compression.c | 571 | P2 wrong func name | Throw message says "unknown" but function is `nlp_compress` |
| 48 | nimcp_nlp_compression.c | 577 | P2 wrong func name | Throw message says "unknown" but function is `nlp_compress` |
| 49 | nimcp_nlp_compression.c | 585 | P2 wrong func name | Throw message says "unknown" but function is `nlp_compress` |
| 50 | nimcp_nlp_compression.c | 665 | P2 wrong func name | Throw message says "unknown" but function is `nlp_decompress` |
| 51 | nimcp_nlp_compression.c | 671 | P2 wrong func name | Throw message says "unknown" but function is `nlp_decompress` |
| 52 | nimcp_nlp_compression.c | 685 | P2 wrong func name | Throw message says "unknown" but function is `nlp_decompress` |
| 53 | nimcp_nlp_compression.c | 694 | P2 wrong func name | Throw message says "unknown" but function is `nlp_decompress` |
| 54 | nimcp_nlp_compression.c | 700 | P2 wrong func name | Throw message says "unknown" but function is `nlp_decompress` |
| 55 | nimcp_nlp_compression.c | 737 | P2 wrong func name | Throw message says "unknown" but function is `nlp_decompress` |
| 56 | nimcp_nlp_compression.c | 744 | P2 wrong func name | Throw message says "unknown" but function is `nlp_decompress` |
| 57 | nimcp_nlp_compression.c | 753 | P2 wrong func name | Throw message says "unknown" but function is `nlp_decompress` |
| 58 | nimcp_nlp_protocol_bridge.c | 220 | P2 wrong func name | Throw message says "nlp_bridge_destroy" but function is `nlp_bridge_send_expression` |
| 59 | nimcp_nlp_protocol_bridge.c | 227 | P2 wrong func name + desc | Throw says "nlp_bridge_destroy" and description says "nlang_expr_validate is NULL" - both wrong |
| 60 | nimcp_nlp_protocol_bridge.c | 236 | P2 wrong func name | Throw message says "nlp_bridge_destroy" but function is `nlp_bridge_send_expression` |
| 61 | nimcp_nlp_protocol_bridge.c | 284 | P2 wrong func name | Throw message says "nlp_bridge_destroy" but function is `nlp_bridge_broadcast_expression` |
| 62 | nimcp_nlp_protocol_bridge.c | 291 | P2 wrong func name + desc | Throw says "nlp_bridge_destroy" and description says "nlang_expr_validate is NULL" - both wrong |
| 63 | nimcp_nlp_protocol_bridge.c | 299 | P2 wrong func name | Throw message says "nlp_bridge_destroy" but function is `nlp_bridge_broadcast_expression` |
| 64 | nimcp_nlp_protocol_bridge.c | 341 | P2 wrong func name | Throw message says "unknown" but function is `nlp_bridge_send_urgent` |
| 65 | nimcp_nlp_protocol_bridge.c | 349 | P2 wrong func name | Throw message says "unknown" but function is `nlp_bridge_send_urgent` |
| 66 | nimcp_nlp_session.c | 470 | P2 wrong func name | Throw message says "nlp_calculate_crc16" but function is `validate_transition` |
| 67 | nimcp_protocol_metrics.c | 176 | P2 false-positive throw | `NIMCP_THROW_TO_IMMUNE` when max primitives reached - capacity limit is not an immune error |
| 68 | nimcp_protocol_metrics.c | 258 | P2 wrong error code | `NIMCP_ERROR_NULL_POINTER` for allocation failure - should be `NIMCP_ERROR_NO_MEMORY` |
| 69 | nimcp_protocol_metrics.c | 291 | P2 wrong error code | `NIMCP_ERROR_NULL_POINTER` for mutex init failure - should be `NIMCP_ERROR_NO_MEMORY` or `NIMCP_ERROR_UNKNOWN` |
| 70 | nimcp_semantic_compression.c | 271 | P2 wrong error code | `NIMCP_ERROR_NULL_POINTER` when config validation fails - should be `NIMCP_ERROR_INVALID_PARAM` |
| 71 | nimcp_semantic_compression.c | 524,538 | P1 unaligned access | `*(uint32_t*)ptr` and `*(float*)ptr` on arbitrary byte pointer in serialization - crashes on strict-alignment architectures (ARM, SPARC) |
| 72 | nimcp_semantic_compression.c | 607-611 | P1 unaligned access | Unaligned reads `*(uint32_t*)ptr` and `*(float*)ptr` in deserialization loop |
| 73 | nimcp_nlp_cortical_adapter.c | 146 | P2 wrong error code | `NIMCP_ERROR_NO_MEMORY` when `bbb_check_pointer` fails (NULL input) - should be `NIMCP_ERROR_NULL_POINTER` |
| 74 | nimcp_nlp_cortical_adapter.c | 154 | P2 wrong error code | `NIMCP_ERROR_NULL_POINTER` for allocation failure - should be `NIMCP_ERROR_NO_MEMORY` (swapped with #73) |
| 75 | nimcp_distributed_immune_bridge.c | 131 | P2 wrong error code | `NIMCP_ERROR_NO_MEMORY` when parameters are NULL - should be `NIMCP_ERROR_NULL_POINTER` |
| 76 | nimcp_distributed_immune_bridge.c | 289 | P2 false-positive throw | `NIMCP_THROW_TO_IMMUNE` when stats retrieval returns error - may be transient, not immune-level |
| 77 | nimcp_p2p_immune_bridge.c | 95 | P2 wrong error code | `NIMCP_ERROR_NO_MEMORY` when parameters are NULL - should be `NIMCP_ERROR_NULL_POINTER` |
| 78 | nimcp_p2p_immune_bridge.c | 124 | P1 NULL deref | No NULL check on `nimcp_malloc` return for `filters` allocation - NULL filters pointer will crash on later access |
| 79 | nimcp_p2p_immune_bridge.c | 129 | P2 memory leak | If `bridge_base_init` fails, `nimcp_free(bridge)` is called but `bridge->p2p_modulation.filters` is leaked |
| 80 | nimcp_p2p_immune_bridge.c | 260 | P2 const-cast | `nimcp_platform_mutex_lock` on `const` bridge parameter - implicitly casts away const |
| 81 | nimcp_p2p_immune_bridge.c | 270 | P2 false-positive throw | `NIMCP_THROW_TO_IMMUNE` when peer is not in filter list - normal "not found" return |
| 82 | nimcp_protocol_immune_bridge.c | 101 | P2 wrong error code | `NIMCP_ERROR_NO_MEMORY` when `immune_system` is NULL - should be `NIMCP_ERROR_NULL_POINTER` |
| 83 | nimcp_protocol_immune_bridge.c | 130 | P1 NULL deref | No NULL check on `nimcp_malloc` return for `filters` allocation - NULL filters pointer will crash on later access |
| 84 | nimcp_protocol_immune_bridge.c | 135 | P2 memory leak | If `bridge_base_init` fails, `nimcp_free(bridge)` is called but `filters` allocation is leaked |
| 85 | nimcp_protocol_immune_bridge.c | 277 | P2 const-cast | `nimcp_platform_mutex_lock` on `const` bridge parameter - implicitly casts away const |
| 86 | nimcp_protocol_immune_bridge.c | 295 | P2 false-positive throw | `NIMCP_THROW_TO_IMMUNE` when message type not in filter list - normal "not found" return |

---

## Files Reviewed (No Issues Found)

| File | Notes |
|------|-------|
| nimcp_nlp_crypto.c | Well-written security code. Proper volatile zeroing, CSPRNG usage, OpenSSL/libsodium/software fallback. No issues found. |
| nimcp_nlp.c | Core NLP node lifecycle, UDP socket management, thread control. Clean implementation. |
| nimcp_nlp_message.c | Message serialization with 36-byte header + encrypted payload + 16-byte auth tag. Solid wire format handling. |

---

## P1 Priority Fix Order

1. **nimcp_p2p_immune_bridge.c:124** and **nimcp_protocol_immune_bridge.c:130** - Unchecked malloc for filters. Add NULL check + return error. Simple 2-line fix each.
2. **nimcp_dialect_learning.c:294,496,578** - VLA stack overflow. Replace with `nimcp_malloc` + free, or add dimension cap (`if (dim > 1024) return error`).
3. **nimcp_semantic_compression.c:524,538,607-611** - Unaligned access. Use `memcpy` into local variable instead of pointer cast.

## P2 Systemic Fix Suggestions

1. **Wrong function names (~50)**: Search-and-replace in each file. The pattern is copy-paste from template/first function. Each throw message should match its enclosing function name.
2. **False positive throws (~15)**: Remove `NIMCP_THROW_TO_IMMUNE` from search-miss, validation-rejection, and capacity-limit paths. These are normal control flow.
3. **Wrong error codes (~10)**: Swap `NIMCP_ERROR_NO_MEMORY` and `NIMCP_ERROR_NULL_POINTER` where confused. NULL input = NULL_POINTER, allocation failure = NO_MEMORY.
4. **Memory leaks on error paths (3)**: Free sub-allocations before freeing parent struct in error cleanup.
5. **Const-cast mutex locks (2)**: Remove `const` from function parameter or use mutable mutex pattern.
