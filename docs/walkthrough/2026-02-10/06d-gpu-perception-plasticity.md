# GPU Perception & Plasticity Walkthrough (2026-02-10)

## Files Reviewed
1. `src/gpu/perception/nimcp_visual_kernels.cu` (2791 lines)
2. `src/gpu/perception/nimcp_speech_cortex_gpu.cu` (2624 lines)
3. `src/gpu/dragonfly/nimcp_dragonfly_vision_kernels.cu` (2690 lines)
4. `src/gpu/plasticity/nimcp_plasticity_kernels.cu` (2219 lines)

---

## File 1: `src/gpu/perception/nimcp_visual_kernels.cu`

### P1-01: Unchecked cudaMalloc in Lucas-Kanade optical flow
- **File**: `nimcp_visual_kernels.cu`
- **Lines**: 618-620
- **Priority**: P1 (GPU memory leak / NULL deref on GPU)
- **Description**: Seven `cudaMalloc` calls (lines 618-620 for d_Ix/d_Iy/d_It, lines 628-631 for d_Ix1/d_Iy1/d_Ix2/d_Iy2) are called without checking return values. If any allocation fails, subsequent code passes NULL device pointers to kernels (undefined behavior on GPU), and earlier successful allocations are never freed.
- **Fix**: Check each `cudaMalloc` return; on failure, free any previously-allocated buffers and return false. Or use `NIMCP_CUDA_RECOVER` with goto-based cleanup.

### P1-02: Unchecked malloc in pyramidal optical flow
- **File**: `nimcp_visual_kernels.cu`
- **Lines**: 703-706
- **Priority**: P1 (NULL dereference)
- **Description**: Four `malloc` calls for `pyramid1`, `pyramid2`, `widths`, `heights` are not NULL-checked. Line 709 immediately dereferences `widths[0]` and `heights[0]`, causing a segfault if allocation fails.
- **Fix**: Check each malloc return for NULL; free any already-allocated arrays and return false on failure.

### P1-03: Unchecked cudaMalloc in pyramid level construction
- **File**: `nimcp_visual_kernels.cu`
- **Lines**: 712-713, 728-729
- **Priority**: P1 (GPU memory leak / NULL deref on GPU)
- **Description**: Pyramid levels are allocated with `cudaMalloc` without error checking. Level 0 (lines 712-713) and loop levels (lines 728-729) all skip return value checks. Line 714-715 immediately calls `cudaMemcpy` with potentially NULL source/dest pointers.
- **Fix**: Use `NIMCP_CUDA_RECOVER` or check `cudaMalloc` return; on failure, free all prior pyramid levels and host arrays.

### P1-04: Unchecked malloc for flow field arrays
- **File**: `nimcp_visual_kernels.cu`
- **Lines**: 747-749
- **Priority**: P1 (NULL dereference)
- **Description**: `flow_x`, `flow_y`, `conf` host pointer arrays are allocated with unchecked `malloc`. Lines 753-755 dereference these as `flow_x[level]` etc., causing segfault if NULL.
- **Fix**: NULL-check each allocation; free prior allocations and pyramid arrays on failure.

### P1-05: Unchecked cudaMalloc inside iterative refinement loop
- **File**: `nimcp_visual_kernels.cu`
- **Lines**: 784-785, 797-799, 806-807
- **Priority**: P1 (GPU memory leak, potential OOM cascade)
- **Description**: Inside a double loop (pyramid levels x iterations), six `cudaMalloc` calls per iteration (`warped`, `d_Ix`, `d_Iy`, `d_It`, `d_du`, `d_dv`) are unchecked. If any fails mid-iteration, earlier buffers in that iteration leak, and the loop continues to allocate more. With 8 pyramid levels and multiple iterations, this can cascade into OOM.
- **Fix**: Check each `cudaMalloc`; on failure, free all iteration-local buffers and break out of loops with cleanup.

### P1-06: kernel_atan2_2d launched with 2D grid but kernel uses 1D indexing
- **File**: `nimcp_visual_kernels.cu`
- **Lines**: 155-161, 914-916
- **Priority**: P1 (incorrect computation)
- **Description**: `kernel_atan2_2d` (line 155) computes `idx = blockIdx.x * blockDim.x + threadIdx.x`, which is a 1D index. However, at line 914 it is launched with a 2D grid (`grid` was set with both x and y dimensions at lines 896-898). The 2D grid causes multiple blocks in the y-dimension to all compute the same 1D indices, meaning only the first row of blocks processes unique elements, while all y-rows redundantly write the same output locations. Elements at indices beyond `gridDim.x * blockDim.x` are never processed.
- **Fix**: Launch `kernel_atan2_2d` with a 1D grid: `kernel_atan2_2d<<<GRID_SIZE(height*width), BLOCK_SIZE>>>(...)`. Alternatively, flatten the 2D grid dimensions into a single x-dimension.

### P1-07: Division by zero in Gabor filter bank creation
- **File**: `nimcp_visual_kernels.cu`
- **Line**: 2235
- **Priority**: P1 (division by zero / NaN)
- **Description**: `s * (log_max - log_min) / (cfg.num_scales - 1)` divides by zero when `cfg.num_scales == 1`. This produces `inf` or `NaN` wavelengths, which propagate into frequency and sigma arrays.
- **Fix**: Guard with `if (cfg.num_scales == 1) { bank->frequencies[0] = 1.0f / cfg.min_wavelength; bank->sigmas[0] = cfg.min_wavelength * cfg.sigma_factor; } else { /* existing loop */ }`.

### P2-01: Sequential NIMCP_CUDA_RECOVER leaks earlier allocations
- **File**: `nimcp_visual_kernels.cu`
- **Lines**: 904-905
- **Priority**: P2 (GPU memory leak on error path)
- **Description**: Two sequential `NIMCP_CUDA_RECOVER(cudaMalloc(...))` for `d_grad_x` and `d_grad_y`. The `NIMCP_CUDA_RECOVER` macro `return false` on failure. If `d_grad_y` allocation fails, `d_grad_x` has already been allocated but is never freed because the macro returns immediately.
- **Fix**: Allocate both, then check both; or use goto-based cleanup. Or allocate d_grad_x, check, then allocate d_grad_y with cleanup of d_grad_x on failure.

### P2-02: Sequential NIMCP_CUDA_RECOVER leaks earlier allocations (horn_schunck)
- **File**: `nimcp_visual_kernels.cu`
- **Lines**: 974-976
- **Priority**: P2 (GPU memory leak on error path)
- **Description**: Three sequential `NIMCP_CUDA_RECOVER(cudaMalloc(...))` for `d_Ix`, `d_Iy`, `d_It`. If the second or third fails, the preceding allocations leak.
- **Fix**: Same as P2-01: use goto-based cleanup or check/free on each failure.

---

## File 2: `src/gpu/perception/nimcp_speech_cortex_gpu.cu`

### P1-08: GPU memory leak from overwritten fft_buffer pointer
- **File**: `nimcp_speech_cortex_gpu.cu`
- **Lines**: 1233-1236
- **Priority**: P1 (GPU memory leak)
- **Description**: Line 1233 allocates GPU memory via `cudaMalloc(&state->fft_buffer, sizeof(nimcp_gpu_tensor_t))`, storing the device pointer in `state->fft_buffer`. Line 1235 then overwrites that pointer with `nimcp_gpu_tensor_create(...)`, which returns a completely different pointer. The original `cudaMalloc`-allocated GPU memory is permanently leaked -- there is no remaining reference to free it.
- **Fix**: Remove lines 1233-1234. The `cudaMalloc` is unnecessary since `nimcp_gpu_tensor_create` handles its own allocation. Replace with just: `state->fft_buffer = nimcp_gpu_tensor_create(ctx, fft_buf_dims, 2, NIMCP_GPU_PRECISION_FP32);`

### P1-09: MFCC delta features written with wrong memory stride
- **File**: `nimcp_speech_cortex_gpu.cu`
- **Lines**: 466, 1481-1504
- **Priority**: P1 (data corruption)
- **Description**: The MFCC tensor is created at line 1481 with shape `[num_frames, total_mfcc]` where `total_mfcc = num_mfcc * (1 + delta + delta2)`. In row-major layout, each frame's data should occupy `total_mfcc` consecutive floats. However, `kernel_apply_dct` (line 466) writes at `mfcc[frame * num_mfcc + coeff]` -- using stride `num_mfcc`, NOT `total_mfcc`. Similarly, the delta kernel at line 504 writes at `delta[frame * feature_dim + feat]` using stride `num_mfcc`. The delta pointer at line 1504 is `mfcc_ptr + num_frames * num_mfcc`, treating the tensor as three separate contiguous blocks rather than interleaved rows. **Result**: The actual data layout is `[BASE_BLOCK | DELTA_BLOCK | DELTA2_BLOCK]` but the tensor shape declares `[num_frames, total_mfcc]` (row-major interleaved). Any consumer reading this tensor as row-major `[num_frames, total_mfcc]` will get scrambled data -- frame N's "base MFCC" will actually be parts of different frames' coefficients.
- **Fix**: Either (a) change `kernel_apply_dct` to write at `mfcc[frame * total_mfcc + coeff]` and pass `total_mfcc` as the stride, updating delta pointer to `mfcc_ptr + frame * total_mfcc + num_mfcc` pattern; or (b) keep the block layout but change tensor shape to `[total_mfcc, num_frames]` or `[3, num_frames, num_mfcc]` to match the actual layout.

### P1-10: Out-of-bounds read in pitch refinement kernel
- **File**: `nimcp_speech_cortex_gpu.cu`
- **Line**: 628
- **Priority**: P1 (out-of-bounds GPU memory read)
- **Description**: `kernel_find_pitch_refined` accesses `ac[lag-1]` inside the loop starting at `lag = min_lag`. When `min_lag` is small (e.g., derived from `sample_rate / max_pitch`, which could be 2-3 for high max_pitch frequencies), `ac[lag-1]` reads at index 1 or 2, which is within bounds. However, if `min_lag == 0` (which can happen if `max_pitch >= sample_rate`), then `ac[lag-1] = ac[-1]` reads before the buffer start -- an out-of-bounds read on GPU memory.
- **Fix**: Start the loop at `max(min_lag, 1)` to ensure `lag-1 >= 0`. Or add a guard: `if (lag < 1) continue;`.

### P2-03: Unchecked cudaMalloc for rectangular window
- **File**: `nimcp_speech_cortex_gpu.cu`
- **Lines**: 2036-2037
- **Priority**: P2 (NULL device pointer to kernel)
- **Description**: `cudaMalloc(&d_rect_window, ...)` has no error check. If it fails, `d_rect_window` is NULL and is passed to `kernel_generate_window` and then `kernel_speech_cortex_frame_audio`, causing undefined behavior on the GPU.
- **Fix**: Use `NIMCP_CUDA_RECOVER` or check the return value; clean up `frames` tensor on failure.

### P2-04: Fixed-size stack array in Levinson-Durbin kernel
- **File**: `nimcp_speech_cortex_gpu.cu`
- **Line**: 726
- **Priority**: P2 (stack overflow on GPU)
- **Description**: `float a_prev[32]` limits the LPC order to 32. If `order > 32` is passed (the parameter is user-configurable via `config->lpc_order`), the kernel writes past the stack array, corrupting GPU thread-local memory. While typical LPC orders are 10-16, there is no runtime guard.
- **Fix**: Add a guard at the start of the kernel: `if (order > 32) return;`. Or better, validate `lpc_order <= 32` in the host-side initialization function.

---

## File 3: `src/gpu/dragonfly/nimcp_dragonfly_vision_kernels.cu`

### P1-11: sqrtf of potentially negative discriminant in optical flow
- **File**: `nimcp_dragonfly_vision_kernels.cu`
- **Line**: 833
- **Priority**: P1 (NaN propagation)
- **Description**: `lambda_min = 0.5f * (trace - sqrtf(trace * trace - 4.0f * det))` computes the eigenvalue of the structure tensor. When `det > trace^2 / 4` (which happens when both eigenvalues are close), the discriminant `trace^2 - 4*det` becomes negative, and `sqrtf` returns NaN. This NaN propagates to the confidence value and flow vectors.
- **Fix**: Use `sqrtf(fmaxf(0.0f, trace * trace - 4.0f * det))` to clamp the discriminant to non-negative, as is done in other optical flow implementations in the codebase.

### P2-05: Unchecked malloc in data association (zero-track path)
- **File**: `nimcp_dragonfly_vision_kernels.cu`
- **Line**: 2105
- **Priority**: P2 (NULL dereference)
- **Description**: `h_assoc = (int*)malloc(n_detections * sizeof(int))` is not NULL-checked. If allocation fails, line 2106 dereferences `h_assoc[i]`, and line 2109 passes it to `cudaMemcpy`.
- **Fix**: Add `if (!h_assoc) return false;` after the malloc.

### P2-06: Unchecked malloc in data association (cost matrix path)
- **File**: `nimcp_dragonfly_vision_kernels.cu`
- **Lines**: 2132-2133
- **Priority**: P2 (NULL dereference)
- **Description**: `h_state` and `h_pred` are allocated without NULL checks. If either fails, line 2135 passes NULL to `cudaMemcpy` (for h_state) or line 2139 dereferences `h_pred[i*3+0]`.
- **Fix**: NULL-check both allocations; free allocated resources on failure.

### P2-07: Unchecked malloc in track management
- **File**: `nimcp_dragonfly_vision_kernels.cu`
- **Lines**: 2199, 2204, 2209
- **Priority**: P2 (NULL dereference)
- **Description**: Three `malloc` calls for `h_assoc`, `h_dets`, `h_state` are not NULL-checked. If any fails, subsequent code dereferences the NULL pointer. `NIMCP_CUDA_RECOVER` on the `cudaMemcpy` would catch a different error (CUDA error), not a host malloc failure.
- **Fix**: NULL-check each allocation; free prior allocations on failure.

### P2-08: Unchecked malloc in target detection
- **File**: `nimcp_dragonfly_vision_kernels.cu`
- **Lines**: 2265, 2270-2271
- **Priority**: P2 (NULL dereference)
- **Description**: `h_detection`, `h_dets`, `h_scores` allocated without NULL checks. If `h_detection` fails, line 2266 passes NULL to `cudaMemcpy`. If `h_dets` or `h_scores` fail, the nested loop at line 2277 dereferences them.
- **Fix**: NULL-check each allocation; free prior allocations and any GPU tensors on failure.

### P2-09: cudaMemcpy errors silently ignored in get_primary_target
- **File**: `nimcp_dragonfly_vision_kernels.cu`
- **Lines**: 1950-1955
- **Priority**: P2 (silent data corruption)
- **Description**: Three `cudaMemcpy` calls to read `h_priority`, `h_state`, `h_confidence` from device to host do not check return values. If any fails (e.g., due to prior async kernel error), the host arrays contain stale/uninitialized stack data, and the function returns incorrect target information.
- **Fix**: Check `cudaMemcpy` return values; return false if any fail.

---

## File 4: `src/gpu/plasticity/nimcp_plasticity_kernels.cu`

**No P1 or significant P2 bugs found.**

This file follows consistent best practices:
- All public API functions check NULL inputs at entry
- All `cudaMalloc` calls use `NIMCP_CUDA_RECOVER` with proper error handling
- `NIMCP_CUDA_RECOVER_LAST` is used after kernel launches
- Cleanup patterns properly free resources on error paths
- Kernel bounds checking is consistent
- No extern "C" wrapping issues with CUDA headers
- No false positive NIMCP_THROW_TO_IMMUNE patterns

---

## Summary

| File | P1 | P2 | Total |
|------|----|----|-------|
| nimcp_visual_kernels.cu | 7 | 2 | 9 |
| nimcp_speech_cortex_gpu.cu | 3 | 2 | 5 |
| nimcp_dragonfly_vision_kernels.cu | 1 | 5 | 6 |
| nimcp_plasticity_kernels.cu | 0 | 0 | 0 |
| **Total** | **11** | **9** | **20** |

### P1 Breakdown by Category
- **GPU memory leak**: 4 (unchecked cudaMalloc cascades in visual, fft_buffer overwrite in speech)
- **NULL dereference**: 2 (unchecked malloc in pyramidal flow)
- **Data corruption**: 2 (MFCC stride mismatch, NaN from sqrtf)
- **Incorrect computation**: 1 (atan2 kernel 2D grid with 1D indexing)
- **Division by zero**: 1 (Gabor bank with num_scales==1)
- **Out-of-bounds read**: 1 (pitch refinement ac[lag-1] when min_lag==0)

### P2 Breakdown by Category
- **GPU memory leak on error**: 2 (sequential NIMCP_CUDA_RECOVER without cleanup)
- **NULL dereference**: 5 (unchecked malloc in dragonfly track/detect functions)
- **Silent error**: 1 (unchecked cudaMemcpy in get_primary_target)
- **Stack overflow**: 1 (fixed a_prev[32] in Levinson-Durbin)

### Patterns Observed
1. **nimcp_visual_kernels.cu** has the worst error handling. The Lucas-Kanade and pyramidal optical flow functions use raw `cudaMalloc`/`malloc` without any checks, unlike the rest of the codebase which uses `NIMCP_CUDA_RECOVER`. These functions appear to be earlier code that was never updated to follow the project's CUDA error handling standards.
2. **nimcp_speech_cortex_gpu.cu** has a critical semantic bug in MFCC layout (P1-09) where kernels write with a stride of `num_mfcc` into a tensor declared as `[num_frames, total_mfcc]`. Any downstream consumer reading this tensor will get incorrect data.
3. **nimcp_dragonfly_vision_kernels.cu** is generally well-written but has unchecked host `malloc` in several track management functions that do device-to-host data transfers.
4. **nimcp_plasticity_kernels.cu** is exemplary and could serve as a reference for how the other files should handle CUDA errors.
5. No false positive `NIMCP_THROW_TO_IMMUNE` patterns were found in any of these files -- they use `LOG_ERROR` + `return` patterns instead.
6. No `extern "C"` wrapping of CUDA headers was found -- all four files correctly include CUDA headers outside of any `extern "C"` blocks.
