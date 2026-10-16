# Pass 5 Walkthrough Summary (2026-02-15)

## Scope
Complete codebase review across all 11 module areas, using 10+ parallel review agents.
Reviewed ~700+ source files via line-by-line reading and grep-based pattern scanning.

## Final Totals

| Module | P1 | P2 | P3 |
|--------|----|----|-----|
| Security | 3 | 12 | 6 |
| Cognitive | 3 | 27 | 4 |
| Swarm | 5 | 14 | 4 |
| GPU | 7 | 11 | 4 |
| Utils | 5 | 29 | 2 |
| Plasticity | 1 | 67 | 1 |
| Net/Mesh | 5 | 52 | 2 |
| Brain Regions | 29 | 489 | 102 |
| Core Brain | 5 | 50 | 6 |
| IO/Misc/Directives | 33 | 8 | 8 |
| Middleware/Async | 3 | 26 | 0 |
| **TOTAL** | **99** | **785** | **139** |

## Top P1 Issues (Critical)

### Crashes
1. **cuRAND state race conditions** (5 GPU files) - Multiple GPU threads concurrently read-modify-write same curandState via `states[idx % num_states]`
2. **Division by zero in subcortical** (striatum, substantia_nigra, globus_pallidus) - `num_neurons / num_actions` when `num_actions == 0`
3. **Slab allocator writes CPU pointers to GPU memory** (gpu_pool.cu) - CPU pointer arithmetic in device memory
4. **Ternary packing alignment violation + data corruption** (ternary_kernels.cu) - Casts `uint8_t*` to `unsigned int*` for atomics
5. **Missing NULL check after malloc** (scientific_reasoning.c, entorhinal.c) - Dereferences potential NULL

### Security
6. **ABBA deadlock in toctou_guard.c** - `cancel` acquires token_lock then guard_lock; `validate_custom` acquires them in reverse
7. **Dangling pointer in capability.c:663** - `holder->name = name` stores caller's pointer without copy
8. **XOR "encryption" placeholders** (encrypted_audit.c, security_memory_bridge.c) - Provides zero actual security
9. **Validation module integer overflow** (validate.c:1087,1337) - Validator itself can be bypassed

### Data Corruption
10. **bio_router_broadcast const-cast corruption** (bio_router.c) - Modifies caller's const message header
11. **Use-after-free in NLP cortical adapter** (nlp_cortical_adapter.c:211) - Freed before clearing global pointer
12. **Integer overflow in protocol packet size** (protocol.c:176,842) - `sizeof(header) + payload_len` wraps uint32_t

### Performance/DoS
13. **Astrocyte unconditional THROW** (astrocyte_plasticity.c) - Fires NIMCP_THROW_TO_IMMUNE on every call (8 functions)
14. **qsort comparator false positive throws** (swarm_multi.c:247, kg_schema.c:132) - O(N log N) immune system throws per sort
15. **Combinatorial harm pattern_match loop** (combinatorial_harm.c:442) - Throws for every non-matching pattern in loop

## Top Systemic P2 Patterns

| Pattern | Count | Description |
|---------|-------|-------------|
| Wrong error code: NULL_POINTER for alloc failure | ~500+ | Should be NIMCP_ERROR_NO_MEMORY |
| False positive NIMCP_THROW_TO_IMMUNE | ~150+ | find/search/lookup "not found", bool getters, validation rejection |
| Wrong function name in THROW messages | ~60+ | Copy-paste errors across bridge files |
| Thread-unsafe `rand()` | ~25 | brain, cognitive, swarm, plasticity, brain regions |
| Raw malloc mutex pattern | ~21 | Should use nimcp_mutex_create() |
| Mutex memory leak in bridge destroy | ~15 | nimcp_platform_mutex_destroy() without nimcp_free(mutex) |
| Integer overflow in size calculations | ~15 | count * element_size without overflow check |
| Wrong error code: BUFFER_OVERFLOW for params | ~25 | Should be INVALID_PARAM or OUT_OF_RANGE |
| Thread-unsafe static variables | ~20 | TOCTOU races, unprotected counters |
| Race conditions on stats fields | ~10 | Stats updated without mutex/atomics |

## Recommended Fix Prioritization

### Tier 1 - Fix immediately (crashes/security)
- cuRAND race conditions (allocate 1 state per thread)
- Protocol integer overflow (check sizeof + payload_len for wrap)
- bio_router const-cast (copy header before modifying)
- ABBA deadlock in toctou_guard (consistent lock ordering)
- Division by zero in subcortical (guard num_actions == 0)
- NLP cortical adapter use-after-free (clear global before free)
- Capability.c dangling pointer (strdup the name)
- Slab allocator GPU memory corruption
- Ternary packing alignment violation
- Validation module overflow checks

### Tier 2 - Systematic sweep (wrong codes/false throws)
- Replace ~500 NIMCP_ERROR_NULL_POINTER with NIMCP_ERROR_NO_MEMORY for alloc failures
- Remove ~150 false positive NIMCP_THROW_TO_IMMUNE from find/search/lookup/bool-getter functions
- Fix ~60 wrong function names in THROW messages
- Fix ~25 wrong NIMCP_ERROR_BUFFER_OVERFLOW codes

### Tier 3 - Thread safety and cleanup
- Replace ~25 rand() calls with thread-safe alternatives
- Fix ~15 mutex memory leaks in bridge destroy paths
- Add overflow checks to ~15 size calculations
- Fix ~21 raw malloc mutex patterns to use nimcp_mutex_create()
- Fix ~20 thread-unsafe static variables

## Comparison with Previous Passes
- Pass 2: 77 P1, 141 P2, 119 P3
- Pass 3: 49 P1, 172 P2, 90 P3
- Pass 4: 163 P1, 473 P2, 121 P3
- **Pass 5: 99 P1, 785 P2, 139 P3**

Note: Pass 5 P2 count is higher because it includes systemic patterns (e.g., 453 wrong error codes in brain regions counted individually). Many P1s from this pass are wrong error codes that were classified as P1 due to being in security-sensitive or crash-adjacent code paths.
