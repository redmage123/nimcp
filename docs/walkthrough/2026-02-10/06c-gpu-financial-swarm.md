# GPU Review: Financial Risk, Swarm Memory, Swarm Kernels, Oscillations

**Date**: 2026-02-10
**Reviewer**: Claude Opus 4.6
**Files Reviewed**:
1. `src/gpu/financial/nimcp_financial_risk_gpu.cu` (3248 lines)
2. `src/gpu/swarm/nimcp_swarm_memory_gpu.cu` (3028 lines)
3. `src/gpu/swarm/nimcp_swarm_kernels.cu` (2410 lines)
4. `src/gpu/oscillations/nimcp_oscillations_kernels.cu` (2610 lines)

---

## File 1: nimcp_financial_risk_gpu.cu

### F1-P1-01: Unchecked malloc in fin_risk_gpu_compute (NULL dereference)
- **Line**: 1147
- **Priority**: P1
- **Description**: `h_partial = (float*)malloc(reduce_blocks * sizeof(float))` has no NULL check. On line 1148 it is passed to `cudaMemcpy` as the destination, and on lines 1151-1153 it is dereferenced in a loop. If malloc returns NULL, this is undefined behavior (crash).
- **Fix**: Add `if (!h_partial) { cudaEventDestroy(start_event); cudaEventDestroy(end_event); cudaFree(d_returns); cudaFree(d_sorted); cudaFree(d_partial); return false; }`

### F1-P1-02: Unchecked cudaMalloc for d_cvar (GPU memory leak / invalid kernel arg)
- **Line**: 1183
- **Priority**: P1
- **Description**: `cudaMalloc(&d_cvar, sizeof(float))` is called without checking the return value. If it fails, `d_cvar` remains NULL and is passed to `kernel_compute_cvar` as a kernel argument and later to `cudaMemcpy` at line 1189, leading to undefined GPU behavior.
- **Fix**: Check return value. On failure, free all prior allocations (d_returns, d_sorted, d_partial, h_partial, events) and return false.

### F1-P1-03: Unchecked malloc for h_returns (NULL dereference)
- **Line**: 1260
- **Priority**: P1
- **Description**: `h_returns = (float*)malloc(n * sizeof(float))` has no NULL check. Immediately used at line 1261 as cudaMemcpy destination and lines 1265-1270 for loop dereferencing.
- **Fix**: Add NULL check with cleanup of all allocated resources.

### F1-P1-04: CUDA event leak on early return paths
- **Lines**: 1085-1088, with early returns possible at 1147-1294
- **Priority**: P1
- **Description**: `cudaEventCreate(&start_event)` and `cudaEventCreate(&end_event)` are called at lines 1086-1087, but the function can exit early (e.g., if malloc at line 1147 fails and triggers a crash, or if the unchecked cudaMalloc at 1183 causes issues). Even once the NULL checks from F1-P1-01/02/03 are fixed, every early return path must destroy the events. Currently, only the happy path at lines 1285-1286 destroys them.
- **Fix**: Use a goto-based cleanup pattern, or ensure every error return path calls `cudaEventDestroy(start_event); cudaEventDestroy(end_event);`.

### F1-P1-05: Yang-Zhang - 3 unchecked cudaMalloc calls
- **Lines**: 1511-1513
- **Priority**: P1
- **Description**: `cudaMalloc(&d_overnight, ...)`, `cudaMalloc(&d_oc, ...)`, `cudaMalloc(&d_rs, ...)` are all unchecked. If any fail, the NULL pointer is passed to `kernel_yang_zhang_components` at line 1515, and later to `cudaMemcpy` at lines 1523-1525. This causes undefined GPU behavior and potential crashes.
- **Fix**: Check each cudaMalloc return value. On failure, free any prior successful allocations and the OHLC cleanup resources, then `goto cleanup_ohlc`.

### F1-P1-06: Yang-Zhang - 3 unchecked malloc calls (NULL dereference)
- **Lines**: 1519-1521
- **Priority**: P1
- **Description**: `h_overnight`, `h_oc`, `h_rs` are allocated via malloc without NULL checks. They are immediately used as cudaMemcpy destinations (lines 1523-1525) and dereferenced in a loop (lines 1528-1531).
- **Fix**: Add NULL checks. On failure, free GPU allocations (d_overnight, d_oc, d_rs) and host allocations, then `goto cleanup_ohlc`.

### F1-P1-07: Unchecked malloc in FIN_VOL_SIMPLE case (NULL dereference)
- **Line**: 1358
- **Priority**: P1
- **Description**: `h_partial = (float*)malloc(ret_reduce * sizeof(float))` has no NULL check. Used immediately at line 1359 as cudaMemcpy destination and dereferenced at line 1362.
- **Fix**: Add NULL check and goto cleanup_vol on failure.

### F1-P1-08: Unchecked cudaMalloc in FIN_VOL_EWMA case
- **Line**: 1380
- **Priority**: P1
- **Description**: `cudaMalloc(&d_var_out, sizeof(float))` is unchecked. If it fails, d_var_out is NULL, and it is passed to `kernel_ewma_volatility` and `cudaMemcpy` at lines 1385-1388.
- **Fix**: Check return value. On failure, `goto cleanup_vol`.

### F1-P1-09: Unchecked cudaMalloc in extended risk (d_cvar)
- **Line**: 1932
- **Priority**: P1
- **Description**: `cudaMalloc(&d_cvar, sizeof(float))` is unchecked in `fin_risk_gpu_extended`. Used as kernel argument at lines 1934, 1940, and as cudaMemcpy source at lines 1937, 1943.
- **Fix**: Check return value. On failure, free d_returns, d_sorted, and return false.

### F1-P1-10: Unchecked cudaMalloc in extended risk (d_var_out for EWMA)
- **Line**: 1951
- **Priority**: P1
- **Description**: `cudaMalloc(&d_var_out, sizeof(float))` is unchecked inside the EWMA branch of `fin_risk_gpu_extended`. Used as kernel argument and cudaMemcpy source.
- **Fix**: Check return value. On failure, free prior GPU allocations.

### F1-P1-11: Unchecked malloc in extended risk (h_returns)
- **Line**: 1974
- **Priority**: P1
- **Description**: `h_returns = (float*)malloc(n * sizeof(float))` has no NULL check. Used at line 1975 as cudaMemcpy destination and dereferenced in loop at lines 1979-1984.
- **Fix**: Add NULL check. On failure, clean up GPU allocations and return false.

### F1-P1-12: Unchecked cudaMalloc pair in max_drawdown
- **Lines**: 2317-2318
- **Priority**: P1
- **Description**: `cudaMalloc(&d_partial_min, ...)` and `cudaMalloc(&d_partial_idx, ...)` are both unchecked. If the first succeeds but the second fails, d_partial_min leaks. Both are used as kernel arguments at line 2320 and cudaMemcpy sources at lines 2327-2328.
- **Fix**: Check each return value with proper rollback cleanup of all prior GPU allocations (d_prices, d_drawdown, d_peak, and any preceding successful alloc in this pair).

### F1-P1-13: Three unchecked malloc calls in max_drawdown (NULL dereference)
- **Lines**: 2324, 2325, 2340
- **Priority**: P1
- **Description**: `h_partial_min`, `h_partial_idx`, and `h_peak` are allocated without NULL checks. All are used as cudaMemcpy destinations and dereferenced in loops.
- **Fix**: Add NULL checks with proper cleanup.

### F1-P1-14: Unchecked malloc pair in batch risk (NULL dereference)
- **Lines**: 2569-2570
- **Priority**: P1
- **Description**: `h_means` and `h_variances` allocated without NULL checks. Used at lines 2572-2573 as cudaMemcpy destinations and dereferenced in loop at lines 2580-2603.
- **Fix**: Add NULL checks. On failure, free GPU allocations and return false.

### F1-P1-15: Division by zero in drawdown kernel
- **Line**: 628
- **Priority**: P1
- **Description**: `drawdown[i] = (prices[i] - max_so_far) / max_so_far;` divides by `max_so_far` which is initialized to `prices[0]`. If `prices[0]` is 0.0f, this is a division by zero on the GPU, producing NaN or Inf that propagates through the drawdown computation.
- **Fix**: Add a guard: `if (max_so_far > 1e-10f) drawdown[i] = (prices[i] - max_so_far) / max_so_far; else drawdown[i] = 0.0f;`

---

## File 2: nimcp_swarm_memory_gpu.cu

### F2-P1-01: Unchecked malloc pair in nimcp_radix_sort_floats (NULL dereference)
- **Lines**: 936-937
- **Priority**: P1
- **Description**: `h_keys = (float*)malloc(n * sizeof(float))` and `h_indices = (unsigned int*)malloc(n * sizeof(unsigned int))` have no NULL checks. Both are used immediately: `h_keys` at line 939 as cudaMemcpy destination and dereferenced at lines 946-950; `h_indices` at lines 941-943 for initialization and line 955 for cudaMemcpy.
- **Fix**: Add NULL checks after each malloc. On failure, free any prior successful allocation and return -1.

### F2-P2-01: Dead code with race condition in kernel_radix_scatter
- **Lines**: 585-627
- **Priority**: P2
- **Description**: `kernel_radix_scatter` contains a race condition: `s_offsets` (shared memory) is initialized per-block from a global prefix sum, then `atomicAdd` is used to increment offsets within shared memory. But with multiple blocks processing the same digit values via the grid-stride loop, each block tracks its own offsets independently, causing multiple blocks to write to overlapping positions in `keys_out`. However, this kernel is never called -- only `kernel_radix_scatter_ranked` (lines 664, 828, 894) is used in practice.
- **Fix**: Remove dead code or add a comment marking it as unused. If it needs to work, require a proper global prefix sum of per-block histograms.

---

## File 3: nimcp_swarm_kernels.cu

### F3-P2-01: atomicMax on float via int cast only valid for positive values
- **Line**: 1001
- **Priority**: P2
- **Description**: `atomicMax((int*)&best_bids[best_task], __float_as_int(my_bid))` uses the IEEE 754 trick where comparing int representations of floats preserves ordering -- but only for non-negative floats. For negative floats, the sign bit inverts the comparison order. If `my_bid` could ever be negative, the atomic max would produce incorrect results. The code at line 1018 checks `best_bids[idx] > 0.0f` suggesting positive values are expected, but this assumption is not enforced.
- **Fix**: Either (a) assert/clamp `my_bid >= 0` before the atomicMax, or (b) use the standard two's-complement fixup: convert negative floats before the atomicMax by flipping all bits when the sign bit is set.

---

## File 4: nimcp_oscillations_kernels.cu

### F4-P1-01: GPU memory leak in PLV - sequential NIMCP_CUDA_RECOVER
- **Lines**: 350-353
- **Priority**: P1
- **Description**: Four sequential `NIMCP_CUDA_RECOVER(cudaMalloc(...))` calls for `d_cos_diff`, `d_sin_diff`, `d_sum_cos`, `d_sum_sin`. Each macro does `return false` on unrecoverable failure. If the 2nd allocation fails, `d_cos_diff` leaks. If the 3rd fails, the first two leak. If the 4th fails, the first three leak.
- **Fix**: Use goto-based cleanup or manual checks with incremental rollback. For example:
  ```
  if (cudaMalloc(&d_cos_diff, ...) != cudaSuccess) goto plv_cleanup;
  if (cudaMalloc(&d_sin_diff, ...) != cudaSuccess) goto plv_cleanup;
  // etc.
  plv_cleanup: cudaFree(d_cos_diff); cudaFree(d_sin_diff); ...
  ```

### F4-P1-02: GPU memory leak in PLI - sequential NIMCP_CUDA_RECOVER
- **Lines**: 429-430
- **Priority**: P1
- **Description**: `NIMCP_CUDA_RECOVER(cudaMalloc(&d_signs, ...))` followed by `NIMCP_CUDA_RECOVER(cudaMalloc(&d_sum, ...))`. If `d_sum` allocation fails (return false from macro), `d_signs` leaks.
- **Fix**: Same goto-based pattern.

### F4-P1-03: GPU memory leak in global sync index
- **Lines**: 524-525
- **Priority**: P1
- **Description**: `NIMCP_CUDA_RECOVER(cudaMalloc(&d_sync_re, ...))` then `NIMCP_CUDA_RECOVER(cudaMalloc(&d_sync_im, ...))`. If `d_sync_im` allocation fails, `d_sync_re` leaks.
- **Fix**: Same goto-based pattern.

### F4-P1-04: GPU memory leak in PAC compute - short-circuit allocation chain
- **Lines**: 950-956
- **Priority**: P1
- **Description**: Four `cudaMalloc` calls connected by `||`: `d_phase_signal`, `d_phase_unused`, `d_amp_envelope`, `d_amp_phase_unused`. If the 2nd, 3rd, or 4th allocation fails, the short-circuit `||` triggers the cleanup block which only frees `d_phase_filtered` and `d_amp_filtered` (lines 954-955), leaking whichever allocations in the chain succeeded before the failure.
- **Fix**: Check each allocation individually with incremental cleanup:
  ```
  if (cudaMalloc(&d_phase_signal, ...) != cudaSuccess) { /* free d_phase_filtered, d_amp_filtered */ return -1; }
  if (cudaMalloc(&d_phase_unused, ...) != cudaSuccess) { cudaFree(d_phase_signal); /* free others */ return -1; }
  // etc.
  ```

### F4-P1-05: Unchecked cudaMalloc trio in PAC compute
- **Lines**: 1020-1022
- **Priority**: P1
- **Description**: `cudaMalloc(&d_phase_bins, ...)`, `cudaMalloc(&d_bin_counts, ...)`, `cudaMalloc(&d_mi, ...)` are all unchecked. If any fail, NULL pointers are passed to `cudaMemset` (lines 1023-1025), kernel launches (line 1028, 1033, 1037), and `cudaMemcpy` (line 1040). This causes undefined GPU behavior.
- **Fix**: Check each allocation. On failure, clean up all prior allocations (including d_phase_filtered, d_amp_filtered, d_phase_signal, d_phase_unused, d_amp_envelope, d_amp_phase_unused, and all tensor objects) and return -1.

### F4-P1-06: Unchecked cudaMalloc for d_mvl in PAC compute
- **Line**: 1050
- **Priority**: P1
- **Description**: `cudaMalloc(&d_mvl, sizeof(float))` is unchecked. If it fails, NULL is passed to `cudaMemset` and kernel launch at line 1052, and `cudaMemcpy` at line 1054.
- **Fix**: Check the return value and handle error with cleanup.

### F4-P1-07: GPU memory + cuFFT plan leak in Hilbert phase
- **Lines**: 1284-1315
- **Priority**: P1
- **Description**: Sequential allocation chain for `d_spectrum` (1284), `d_filter` (1285), `d_signal_copy` (1289). Each uses `NIMCP_CUDA_RECOVER` which returns false on failure. If `d_filter` alloc fails, `d_spectrum` leaks. If `d_signal_copy` fails, both `d_spectrum` and `d_filter` leak. Additionally, `CUFFT_CHECK` calls at lines 1294, 1297, 1310, 1315 return false without freeing any of the GPU allocations or the cuFFT plan. If `cufftPlan1d` at line 1294 fails after all 3 allocations succeeded, all 3 leak. If `cufftExecR2C` at line 1297 fails, the plan and all 3 allocations leak. The `d_analytic_im` allocation at line 1314 (via NIMCP_CUDA_RECOVER) also returns false without cleaning up the first cuFFT plan or any prior allocations.
- **Fix**: Use goto-based cleanup to ensure all allocations and cuFFT plans are freed on any error path.

### F4-P1-08: GPU memory + cuFFT plan leak in Hilbert amplitude
- **Lines**: 1358-1384
- **Priority**: P1
- **Description**: Sequential NIMCP_CUDA_RECOVER for `d_complex_signal` (1358), `d_spectrum` (1359). If `d_spectrum` fails, `d_complex_signal` leaks. Then `CUFFT_CHECK(cufftPlan1d(...))` at 1367 returns false without freeing either. `CUFFT_CHECK(cufftExecC2C(...))` at 1370 returns false leaking plan + both allocations. `NIMCP_CUDA_RECOVER(cudaMalloc(&d_filter, ...))` at 1378 returns false leaking plan + both prior allocations. Second `CUFFT_CHECK(cufftExecC2C(...))` at 1384 same issue.
- **Fix**: Same goto-based cleanup pattern.

### F4-P1-09: GPU memory + cuFFT plan leak chain in coherence computation
- **Lines**: 1778-1800
- **Priority**: P1
- **Description**: Ten sequential `NIMCP_CUDA_RECOVER(cudaMalloc(...))` calls for `d_windowed1`, `d_windowed2`, `d_spectrum1`, `d_spectrum2`, `d_cross`, `d_psd1`, `d_psd2` (lines 1778-1784), then memset calls (1787-1789), then a cuFFT plan creation at 1793, then three more NIMCP_CUDA_RECOVER allocations for `d_cross_temp`, `d_psd1_temp`, `d_psd2_temp` (1798-1800). Each NIMCP_CUDA_RECOVER failure returns false without freeing prior allocations. The cuFFT plan at line 1793 is created between allocation groups -- if later allocations fail, the plan leaks too.
- **Fix**: Use goto-based cleanup with a label after the allocation section, ensuring all allocated buffers and cuFFT plan are freed.

### F4-P1-10: CUFFT_CHECK in coherence loop leaks allocations and plan
- **Lines**: 1813-1814
- **Priority**: P1
- **Description**: Inside the segment-processing loop (lines 1803-1822), `CUFFT_CHECK(cufftExecR2C(...))` at lines 1813-1814 will `return false` if the FFT fails, leaking all 10 GPU allocations, the 3 temp allocations, and the cuFFT plan.
- **Fix**: Replace `CUFFT_CHECK` with a pattern that goes to cleanup instead of returning directly.

---

## Summary

| File | P1 | P2 | Total |
|------|----|----|-------|
| nimcp_financial_risk_gpu.cu | 15 | 0 | 15 |
| nimcp_swarm_memory_gpu.cu | 1 | 1 | 2 |
| nimcp_swarm_kernels.cu | 0 | 1 | 1 |
| nimcp_oscillations_kernels.cu | 10 | 0 | 10 |
| **Total** | **26** | **2** | **28** |

### Breakdown by Category

| Category | Count |
|----------|-------|
| NULL dereference (unchecked host malloc) | 9 |
| GPU memory leak (unchecked cudaMalloc) | 8 |
| GPU memory leak (NIMCP_CUDA_RECOVER chain) | 6 |
| cuFFT plan + GPU memory leak (CUFFT_CHECK returns) | 3 |
| Division by zero (kernel) | 1 |
| Dead code with race condition | 1 |

### Key Patterns

1. **Unchecked host malloc**: The financial risk file has a pervasive pattern of calling `malloc()` and immediately using the result without a NULL check. This appears in `fin_risk_gpu_compute`, `fin_risk_gpu_volatility`, `fin_risk_gpu_volatility_ohlc`, `fin_risk_gpu_extended`, `fin_risk_gpu_max_drawdown`, and `fin_risk_gpu_batch`.

2. **Unchecked cudaMalloc**: Several `cudaMalloc` calls in the financial risk file bypass the `NIMCP_CUDA_RECOVER` pattern and directly call `cudaMalloc` without checking the return. These typically happen for small single-value allocations (d_cvar, d_var_out, d_mvl) that are passed to kernels.

3. **NIMCP_CUDA_RECOVER sequential leak pattern**: The oscillations file consistently uses sequential `NIMCP_CUDA_RECOVER` calls for multi-buffer allocations. Since the macro does `return false` on failure, this creates a leak pattern where all preceding successful allocations in the chain are orphaned. This pattern appears in PLV, PLI, global sync, Hilbert phase, Hilbert amplitude, and coherence computations.

4. **CUFFT_CHECK leak pattern**: The `CUFFT_CHECK` macro does `return false` on cuFFT errors. When cuFFT calls fail after GPU memory has been allocated and/or cuFFT plans created, those resources leak. This affects both Hilbert transform functions and the coherence computation.

### Notes

- **CUDA headers not in extern "C"**: All four files correctly include CUDA headers (`cuda_runtime.h`, `curand_kernel.h`, `cufft.h`) before any `extern "C"` blocks or project headers. No issues found.
- **Guard clause pattern**: The files consistently use throw + return for guard clauses. No missing-return-after-throw issues found.
- **False positive NIMCP_THROW_TO_IMMUNE**: No NIMCP_THROW_TO_IMMUNE usage found in any of the four files. They use `NIMCP_THROW_GPU` instead.
- **The `static __shared__`** in `nimcp_swarm_memory_gpu.cu` line 70 is a common CUDA pattern and is NOT a bug -- each block gets its own shared memory regardless of the `static` keyword.
