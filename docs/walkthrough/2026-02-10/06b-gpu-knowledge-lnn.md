# GPU Knowledge Graph, LNN, & Statistics Kernel Review

**Date**: 2026-02-10
**Reviewer**: Claude Opus 4.6
**Files Reviewed**:
1. `src/gpu/knowledge/nimcp_knowledge_graph_kernels.cu` (3901 lines)
2. `src/gpu/lnn/nimcp_lnn_kernels.cu` (3628 lines)
3. `src/gpu/lnn/nimcp_lnn_ode_kernels.cu` (2308 lines)
4. `src/gpu/statistics/nimcp_statistics_kernels.cu` (2673 lines)

---

## P1 Findings (Critical)

### P1-01: DFS kernel atomicSub underflow race
- **File**: `nimcp_knowledge_graph_kernels.cu`, line 631
- **Priority**: P1 - Buffer overflow / undefined behavior
- **Description**: In `kernel_dfs`, `atomicSub(stack_ptr, 1)` returns the OLD value. If multiple blocks atomically decrement the same stack pointer simultaneously, one block sees `old_ptr=1` and correctly reads `stack[0]`, but another block that also saw `old_ptr>0` before the first block's decrement may decrement again causing `stack_ptr` to go to `UINT32_MAX` (unsigned underflow). The `atomicAdd(stack_ptr, 1)` restore on line 636 only fires if old_ptr was already 0 -- but the race is between two successful decrements where the stack had only one element.
- **Fix**: Use `atomicMax`/CAS loop pattern: `old_ptr = atomicSub(stack_ptr, 1); if (old_ptr == 0) { atomicAdd(stack_ptr, 1); /* restore */ }`. The current code does this but the window between the atomic ops on different blocks can lead to `stack_ptr` going negative (wrapping to UINT32_MAX). Consider using `atomicCAS` loop: read ptr, if ptr > 0 attempt CAS(ptr, ptr-1), retry on failure.

### P1-02: DAO dao_find_similar_impl unchecked cudaMalloc
- **File**: `nimcp_knowledge_graph_kernels.cu`, lines 3092, 3097
- **Priority**: P1 - NULL pointer dereference on GPU
- **Description**: `cudaMalloc(&d_query, ...)` and `cudaMalloc(&d_scores, ...)` have no error checking. If either fails, the subsequent `cudaMemcpy` and kernel launch will operate on NULL device pointers, causing GPU segfault or undefined behavior.
- **Fix**: Check return values: `if (cudaMalloc(&d_query, ...) != cudaSuccess) { return -1; }` and similarly for `d_scores`. Free `d_query` on `d_scores` failure.

### P1-03: DAO dao_find_similar_impl unchecked host malloc
- **File**: `nimcp_knowledge_graph_kernels.cu`, line 3110
- **Priority**: P1 - NULL pointer dereference
- **Description**: `h_scores = malloc(self->max_entities * sizeof(float))` is not checked for NULL before `cudaMemcpy(h_scores, ...)` dereferences it.
- **Fix**: Add `if (!h_scores) { cudaFree(d_query); cudaFree(d_scores); return -1; }`.

### P1-04: nimcp_kg_train_step massive unchecked cudaMalloc chain
- **File**: `nimcp_knowledge_graph_kernels.cu`, lines 3414-3427
- **Priority**: P1 - NULL pointer dereference on GPU
- **Description**: 11 consecutive `cudaMalloc` calls with no error checking. If any fails (especially on the first few large allocations), all subsequent `cudaMemcpy` and kernel launches use NULL device pointers. This function allocates `8 * batch_size * dim * sizeof(float) + 3 * batch_size * sizeof(int)` of device memory with zero validation.
- **Fix**: Use `CUDA_CHECK()` or `NIMCP_CUDA_RECOVER()` macros around each allocation, with cascading cleanup on failure.

### P1-05: nimcp_kg_train_step unchecked host malloc
- **File**: `nimcp_knowledge_graph_kernels.cu`, line 3443
- **Priority**: P1 - NULL pointer dereference
- **Description**: `neg_tail_idx = malloc(batch_size * sizeof(int))` is not checked before being used in the loop on line 3444.
- **Fix**: Add NULL check before use; on failure, free all 11 device allocations and return -1.

### P1-06: nimcp_kg_transe_score unchecked mallocs
- **File**: `nimcp_knowledge_graph_kernels.cu`, lines 3513-3515
- **Priority**: P1 - NULL pointer dereference
- **Description**: Three `malloc` calls for `h_emb`, `r_emb`, `t_emb` with no NULL checks. If any fails, `cudaMemcpy` on lines 3517-3519 writes to NULL.
- **Fix**: Add NULL checks for all three; free already-allocated on partial failure.

### P1-07: nimcp_kg_predict_tail/head unchecked mallocs
- **File**: `nimcp_knowledge_graph_kernels.cu`, lines 3548-3550, 3589-3591
- **Priority**: P1 - NULL pointer dereference
- **Description**: `h_emb`, `r_emb`, `query` (and corresponding in predict_head: `t_emb`, `r_emb`, `query`) allocated without NULL checks, then used in cudaMemcpy.
- **Fix**: Add NULL checks with cascading cleanup.

### P1-08: nimcp_kg_find_path unchecked mallocs for embedding vectors
- **File**: `nimcp_knowledge_graph_kernels.cu`, lines 3315-3316
- **Priority**: P1 - NULL pointer dereference
- **Description**: `source_emb` and `target_emb` malloc'd without NULL checks. If either fails, `dao->read_embedding()` writes to NULL (which internally does `cudaMemcpy` into the NULL pointer).
- **Fix**: Add `if (!source_emb || !target_emb) { free(source_emb); free(target_emb); nimcp_kg_result_destroy(result); return -1; }`.

### P1-09: LNN DOPRI5 unchecked cudaMalloc for d_max_error
- **File**: `nimcp_lnn_kernels.cu`, lines 789-791
- **Priority**: P1 - NULL pointer dereference on GPU
- **Description**: In the adaptive DOPRI5 stepping path, `cudaMalloc(&d_max_error, sizeof(float))` is not checked. If it fails, `cudaMemset(d_max_error, 0, ...)` on line 792 and the kernel launch writing to `d_max_error` will crash.
- **Fix**: Use `NIMCP_CUDA_RECOVER()` macro or check return code with cleanup.

### P1-10: LNN spectral radius unchecked cudaMalloc chain
- **File**: `nimcp_lnn_kernels.cu`, lines 2218-2221
- **Priority**: P1 - NULL pointer dereference on GPU
- **Description**: `nimcp_lnn_compute_spectral_radius` has 4 consecutive `cudaMalloc` calls (`d_A`, `d_v`, `d_Av`, `d_norm`) with zero error checking. If any fails, the power iteration loop kernel launches will crash.
- **Fix**: Check each allocation; cascade cleanup on failure.

### P1-11: LNN spectral radius unchecked host calloc
- **File**: `nimcp_lnn_kernels.cu`, line 2227
- **Priority**: P1 - NULL pointer dereference
- **Description**: `h_v = calloc(n, sizeof(float))` is not checked before `cudaMemcpy(d_v, h_v, ...)` which would read from NULL.
- **Fix**: Add `if (!h_v) { cudaFree(d_A); cudaFree(d_v); cudaFree(d_Av); cudaFree(d_norm); return -1.0f; }`.

### P1-12: Statistics workspace missing error check for d_temp2
- **File**: `nimcp_statistics_kernels.cu`, lines 1011-1013
- **Priority**: P1 - GPU memory leak / NULL dereference
- **Description**: `err = cudaMalloc(&ws->d_temp2, ws->temp_size)` on line 1011 stores the error in `err`, but line 1013 immediately overwrites `err` with `cudaMalloc(&ws->d_partial_sums, ...)` without checking whether `d_temp2` succeeded. If `d_temp2` allocation fails, `ws->d_temp2` is NULL but execution continues. Later code using `ws->d_temp2` will crash.
- **Fix**: Add `if (err != cudaSuccess) goto cleanup_ws;` after line 1011.

### P1-13: Statistics bootstrap unchecked cudaMalloc
- **File**: `nimcp_statistics_kernels.cu`, line 1440
- **Priority**: P1 - NULL pointer dereference on GPU
- **Description**: `cudaMalloc(&d_mean_tmp, sizeof(float))` in the variance branch of bootstrap is not checked. If it fails, `nimcp_stats_gpu_variance_batch()` receives a NULL pointer.
- **Fix**: Use `NIMCP_CUDA_RECOVER()` macro or check manually.

### P1-14: DAO dao_create_embedding_impl unchecked host malloc
- **File**: `nimcp_knowledge_graph_kernels.cu`, line 3001
- **Priority**: P1 - NULL pointer dereference
- **Description**: `h_valid = malloc(sizeof(int))` is not checked before `cudaMemcpy(h_valid, ...)` on line 3002 dereferences it, and `*h_valid` on line 3003.
- **Fix**: Add `if (!h_valid) return -1;` after the malloc.

---

## P2 Findings (Important)

### P2-01: DFS kernel warp shuffle with partial participation
- **File**: `nimcp_knowledge_graph_kernels.cu`, lines 640-641
- **Priority**: P2 - Correctness (potential undefined values)
- **Description**: `__shfl_sync(0xFFFFFFFF, current, 0)` broadcasts thread 0's value to all threads in the warp. This is correct for broadcasting, BUT if `blockDim.x < 32`, threads beyond blockDim.x are still expected in the mask `0xFFFFFFFF`. The actual risk is low since CUDA blocks typically have >= 32 threads (BLOCK_SIZE is likely 256), but if blockDim.x is not a multiple of 32, the last partial warp has inactive threads included in the mask.
- **Fix**: Use `__activemask()` instead of `0xFFFFFFFF` or ensure BLOCK_SIZE is always a warp multiple.

### P2-02: Unchecked cudaMemcpy in nimcp_gpu_shortest_path
- **File**: `nimcp_knowledge_graph_kernels.cu`, lines 839-840
- **Priority**: P2 - Silent data corruption
- **Description**: Two `cudaMemcpy` calls copying distances and parents from device to host have no error checking. If these fail (e.g., device memory was already freed), the path reconstruction loop on lines 857-860 operates on uninitialized host memory.
- **Fix**: Check `cudaMemcpy` return values; on failure, cleanup and return false.

### P2-03: Attention aggregation shared memory sizing
- **File**: `nimcp_knowledge_graph_kernels.cu`, line 1840
- **Priority**: P2 - Potential shared memory overflow
- **Description**: `shared_size = (attn_dim + graph->num_nodes) * sizeof(float)`. The `graph->num_nodes` term is used to store attention scores for ALL nodes, but the attention kernel only needs space for the neighbors of each node (i.e., the degree of that node). For dense graphs where num_nodes is large (e.g., 100K+), this requests enormous shared memory (400KB+) which exceeds GPU limits (48KB-164KB). The MAX_SHARED_MEM check on line 1842 catches this, but it means the function fails for any reasonably-sized graph.
- **Fix**: The shared memory should be sized as `(attn_dim + max_degree) * sizeof(float)` where `max_degree` is precomputed. This requires scanning `row_offsets` to find the maximum node degree.

### P2-04: Dead forward declaration inside function body (LNN Heun)
- **File**: `nimcp_lnn_kernels.cu`, lines 542-543
- **Priority**: P2 - Dead code / compilation warning
- **Description**: `__global__ void kernel_heun_combine(...)` is forward-declared inside the `nimcp_gpu_lnn_heun_step` host function body. You cannot declare `__global__` functions inside host functions in CUDA. This code appears to be leftover from a refactor -- it is never called (the actual combination is done with `kernel_add_scaled` on lines 546-552).
- **Fix**: Remove the dead forward declaration on lines 542-543.

### P2-05: DAO unchecked cudaMemcpy calls throughout
- **File**: `nimcp_knowledge_graph_kernels.cu`, lines 3002, 3010-3011, 3015, 3029, 3035-3036, 3049, 3055-3056, 3069, 3076
- **Priority**: P2 - Silent data corruption
- **Description**: All DAO CRUD operations (`dao_create_embedding_impl`, `dao_read_embedding_impl`, `dao_update_embedding_impl`, `dao_delete_embedding_impl`) use unchecked `cudaMemcpy` calls. Any failure results in silent corruption -- reading stale data, writing to wrong locations, or missing validity updates.
- **Fix**: Check `cudaMemcpy` return values and return -1 on failure.

### P2-06: Statistics cov_to_corr kernel misses columns for p > 256
- **File**: `nimcp_statistics_kernels.cu`, lines 1258-1266
- **Priority**: P2 - Silent incorrect results
- **Description**: For `p <= 256`, the kernel is launched with `p` blocks and `p` threads per block. In the kernel, `j = threadIdx.x`, so all columns are processed. For `p > 256`, line 1266 launches with `min(p, 1024u)` threads, meaning `j` only goes up to 1023 at most. Columns beyond 1023 are never normalized. The comment on line 1264 ("Would need a different kernel") acknowledges this but the fallback still has the same limitation for p > 1024.
- **Fix**: Add a thread-striding loop in `kernel_cov_to_corr`: `for (uint32_t j = threadIdx.x; j < n; j += blockDim.x)`.

### P2-07: Bootstrap kernel curandState out-of-bounds access
- **File**: `nimcp_statistics_kernels.cu`, line 478
- **Priority**: P2 - Out-of-bounds read/write
- **Description**: `states[resample_idx % (gridDim.x * blockDim.x)]` assumes the curandState array has `gridDim.x * blockDim.x` elements. However, the RNG initialization (in `nimcp_stats_gpu_rng_create`) may allocate a different number of states (typically `num_streams`). If `gridDim.x * blockDim.x > rng->num_states`, the modulo wraps within the wrong range and the index can still exceed the actual state array size.
- **Fix**: Pass `num_states` as an additional kernel parameter and use `resample_idx % num_states`.

### P2-08: Statistics quantiles O(n^2) bubble sort
- **File**: `nimcp_statistics_kernels.cu`, lines 1317-1325
- **Priority**: P2 - Performance / DoS risk
- **Description**: The quantile computation uses O(n^2) bubble sort on the CPU after copying data from GPU. For large n (e.g., n=100K), this takes ~10 billion comparisons, essentially hanging the function. The comment mentions "CUB radix sort" as the production approach but uses bubble sort.
- **Fix**: Use `qsort()` from stdlib for O(n log n) performance, or implement a GPU-side CUB sort.

### P2-09: LNN ODE kernel unchecked dims access
- **File**: `nimcp_lnn_ode_kernels.cu`, line 1069
- **Priority**: P2 - Potential out-of-bounds read
- **Description**: `input_sequence->dims[1]` is accessed without verifying that `input_sequence->ndim >= 2`. If a 1D tensor is passed, `dims[1]` is uninitialized or zero.
- **Fix**: Add `if (!input_sequence->dims || input_sequence->ndim < 2) return false;` before accessing dims[1].

### P2-10: nimcp_kg_train_step gradient application is wrong
- **File**: `nimcp_knowledge_graph_kernels.cu`, lines 3472-3477
- **Priority**: P2 - Incorrect training results
- **Description**: The gradient update only applies `d_grad_head` to ALL entity embeddings and `d_grad_rel` to ALL relation embeddings. The gradients for `d_grad_tail` and `d_grad_neg` (negative samples) are computed but never applied. Also, `kernel_kg_embedding_update` is applied to the entire embedding table with `n = max_entities * dim`, but the gradient buffer `d_grad_head` only has `batch_size * dim` elements. When `max_entities > batch_size`, the kernel reads beyond the gradient buffer.
- **Fix**: Use scatter-update that maps gradients back to their respective entity indices, not a bulk update to the entire embedding table.

### P2-11: DAO num_entities counter not thread-safe
- **File**: `nimcp_knowledge_graph_kernels.cu`, lines 3017, 3078
- **Priority**: P2 - Data race
- **Description**: `self->num_entities++` (line 3017) and `self->num_entities--` (line 3078) are non-atomic increments on a host counter. If multiple threads call DAO create/delete concurrently, the count becomes incorrect.
- **Fix**: Use `atomicAdd` or protect with a mutex.

---

## Summary

| Priority | Count | Files Affected |
|----------|-------|----------------|
| P1       | 14    | knowledge_graph (10), lnn (3), statistics (2) |
| P2       | 11    | knowledge_graph (6), lnn (2), statistics (3) |
| **Total**| **25**| **4 files** |

### Breakdown by Category

| Category | P1 | P2 |
|----------|----|----|
| Unchecked cudaMalloc (NULL GPU ptr) | 6 | 0 |
| Unchecked host malloc (NULL ptr) | 6 | 0 |
| Unchecked cudaMemcpy | 0 | 2 |
| Missing error check (gap in chain) | 1 | 0 |
| Race condition / atomics | 1 | 1 |
| Buffer overread | 0 | 1 |
| Silent wrong results | 0 | 2 |
| Dead code | 0 | 1 |
| Performance / DoS | 0 | 1 |
| Shared memory sizing | 0 | 1 |
| Warp shuffle safety | 0 | 1 |
| Incorrect gradient application | 0 | 1 |

### Risk Assessment

The most critical cluster of issues is in the **Knowledge Graph DAO and TransE training functions** (lines 2994-3615). These functions were clearly added later (they follow a different coding style -- no CUDA_CHECK/NIMCP_CUDA_RECOVER macros, raw cudaMalloc without error checks, raw malloc without NULL checks). All 10 P1s in this file come from this section.

The **LNN kernels** (both files) are generally well-structured with proper error recovery macros in the main CUDA path, but the DOPRI5 adaptive stepping and spectral radius computation use raw cudaMalloc without the project's recovery macros.

The **statistics kernels** are mostly well-protected by NIMCP_CUDA_RECOVER macros, with the notable exception of the workspace allocation gap (d_temp2) and the bootstrap variance path.

### False Positive Assessment

No NIMCP_THROW_TO_IMMUNE calls were found in any of the four files. These GPU kernel files use LOG_ERROR + return patterns and CUDA_CHECK/NIMCP_CUDA_RECOVER macros instead, which is correct for GPU code. No false positive NIMCP_THROW_TO_IMMUNE issues exist.
