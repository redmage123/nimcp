# GPU Walkthrough: Tensor Kernels, Wernicke GPU, INT8 Inference, GPU Stubs

**Date**: 2026-02-10
**Files Reviewed**:
1. `src/gpu/tensor/nimcp_tensor_kernels.cu` (2396 lines)
2. `src/gpu/cognitive/nimcp_wernicke_gpu.cu` (2307 lines)
3. `src/gpu/inference/nimcp_int8_inference.cu` (2583 lines)
4. `src/gpu/stubs/nimcp_gpu_stubs.c` (2940 lines)

---

## Summary

| Priority | Count |
|----------|-------|
| P1       | 6     |
| P2       | 8     |
| **Total** | **14** |

---

## Findings by File

### 1. `src/gpu/tensor/nimcp_tensor_kernels.cu`

#### P1-T1: numel overflow in nimcp_gpu_tensor_create (line 126-131)

**Line**: 126-131
**Priority**: P1 - Buffer overflow / integer overflow
**Description**: The `numel` computation in `nimcp_gpu_tensor_create` multiplies dimension values in a loop without overflow checking. The helper function `compute_numel()` at line 67 has a proper `SIZE_MAX` overflow check, but `nimcp_gpu_tensor_create` does not use it. If crafted dimensions cause `numel` to wrap around, the subsequent `cudaMalloc` at line 135 allocates too little memory, and all subsequent kernel launches will read/write out of bounds.
**Code**:
```c
// Line 126-131 (no overflow check)
tensor->numel = 1;
for (int i = ndim - 1; i >= 0; i--) {
    tensor->dims[i] = dims[i];
    tensor->strides[i] = tensor->numel;
    tensor->numel *= dims[i];  // Can overflow silently
}
```
**Fix**: Use `compute_numel()` first to check for overflow before entering the loop, or add an inline overflow check:
```c
size_t numel = compute_numel(dims, ndim);
if (numel == 0 && ndim > 0) {
    LOG_ERROR("Dimension overflow in tensor create");
    free(tensor->dims); free(tensor->strides); free(tensor);
    return NULL;
}
tensor->numel = numel;
// Then compute strides separately
```

---

#### P2-T1: cublasSgeam with NULL B matrix pointer (line 1929-1934)

**Line**: 1933
**Priority**: P2 - Fragile CUDA API usage
**Description**: In `nimcp_gpu_transpose`, `cublasSgeam` is called with `NULL` for the B matrix and `beta=0.0f`. While the cuBLAS documentation says B can be NULL when beta is zero, this behavior is not guaranteed across all cuBLAS versions and GPU architectures. Some older cuBLAS implementations may still dereference B for alignment or size validation.
**Code**:
```c
CUBLAS_CHECK(cublasSgeam(handle,
    CUBLAS_OP_T, CUBLAS_OP_N,
    rows, cols,
    &alpha, (const float*)x->data, cols,
    &beta, NULL, rows,      // NULL B with beta=0
    (float*)out->data, rows));
```
**Fix**: Pass `(const float*)out->data` instead of `NULL` for B, since it's an already-allocated buffer of the correct size. This is safe because `beta=0` means B's contents are multiplied by zero and never contribute to the result.

---

### 2. `src/gpu/cognitive/nimcp_wernicke_gpu.cu`

#### P1-W1: GPU memory leak on cascading cudaMalloc failure (lines 1242-1261)

**Line**: 1242-1261
**Priority**: P1 - GPU memory leak
**Description**: Three consecutive `NIMCP_CUDA_RECOVER` calls allocate `d_seeds`, `d_seed_acts`, and `d_new_activations`. The `NIMCP_CUDA_RECOVER` macro does `return false` on unrecoverable failure. If the second allocation (line 1243, `d_seed_acts`) fails, `d_seeds` (allocated on line 1242) is leaked. If the third allocation (line 1261, `d_new_activations`) fails, both `d_seeds` and `d_seed_acts` are leaked. The cleanup at lines 1296-1299 only executes on the happy path or later failures.
**Code**:
```c
NIMCP_CUDA_RECOVER(cudaMalloc(&d_seeds, num_seeds * sizeof(uint32_t)), GPU_ERROR_OUT_OF_MEMORY);
NIMCP_CUDA_RECOVER(cudaMalloc(&d_seed_acts, num_seeds * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
// ... later ...
NIMCP_CUDA_RECOVER(cudaMalloc(&d_new_activations, ctx->num_concepts * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
```
**Fix**: Replace `NIMCP_CUDA_RECOVER` with `NIMCP_CUDA_RECOVER_GOTO` and a cleanup label, or pre-initialize pointers to NULL and use a goto-cleanup pattern:
```c
uint32_t* d_seeds = NULL;
float* d_seed_acts = NULL;
float* d_new_activations = NULL;
// ... allocate with goto cleanup on failure ...
cleanup:
    if (d_seeds) cudaFree(d_seeds);
    if (d_seed_acts) cudaFree(d_seed_acts);
    if (d_new_activations) cudaFree(d_new_activations);
```

---

#### P1-W2: NULL dereference in wernicke_cpu_spread_activation (lines 1919-1920)

**Line**: 1919-1920
**Priority**: P1 - NULL pointer dereference / crash
**Description**: In `wernicke_cpu_spread_activation` (the CPU fallback in the `#else` block), two `calloc` calls allocate `current` and `next` buffers. Neither is NULL-checked before use. If either allocation fails, `memset(next, 0, ...)` at line 1931 and array access at line 1925 (`current[seed_concepts[s]]`) will dereference NULL, causing a segfault.
**Code**:
```c
float* current = (float*)calloc(num_concepts, sizeof(float));
float* next = (float*)calloc(num_concepts, sizeof(float));

// Used immediately without NULL checks:
current[seed_concepts[s]] = seed_activations[s];  // line 1925
memset(next, 0, num_concepts * sizeof(float));     // line 1931
```
**Fix**: Add NULL checks after both allocations:
```c
float* current = (float*)calloc(num_concepts, sizeof(float));
float* next = (float*)calloc(num_concepts, sizeof(float));
if (!current || !next) {
    free(current);
    free(next);
    return false;
}
```

---

#### P2-W1: Missing CUDA error check after kernel launch in wm_rehearse (line 1558-1562)

**Line**: 1558-1565
**Priority**: P2 - Unchecked CUDA error
**Description**: `wernicke_gpu_wm_rehearse` launches `kernel_wm_rehearse` but never calls `NIMCP_CUDA_RECOVER_LAST` (or `cudaGetLastError`) to check for kernel launch failure. If the kernel fails (e.g., invalid grid dimensions, insufficient shared memory), the error is silently swallowed and `true` is returned. Every other kernel launch in this file checks for errors.
**Code**:
```c
extern "C" bool wernicke_gpu_wm_rehearse(wernicke_gpu_context_t* ctx) {
    if (!ctx || ctx->wm_count == 0) return true;

    kernel_wm_rehearse<<<GRID_SIZE(ctx->wm_count), BLOCK_SIZE, 0, ctx->stream>>>(
        ctx->d_wm_activations,
        ctx->wm_count,
        WERNICKE_WM_REFRESH_AMOUNT
    );
    // No NIMCP_CUDA_RECOVER_LAST here

    ctx->stats.wm_operations++;
    return true;
}
```
**Fix**: Add `NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);` after the kernel launch.

---

### 3. `src/gpu/inference/nimcp_int8_inference.cu`

#### P1-I1: Double-free via shallow copy in nimcp_int8_tensor_clone (line 1051)

**Line**: 1051
**Priority**: P1 - Double-free / heap corruption
**Description**: `nimcp_int8_tensor_clone` uses `memcpy` to copy the `params` struct, which contains pointer fields (`channel_scales`, `channel_zero_points`, `group_scales`, `group_zero_points`). This creates a shallow copy where both original and clone share the same heap-allocated arrays. When `nimcp_int8_tensor_destroy` is called on both (lines 1028-1031), it calls `free()` on these pointers twice, causing heap corruption or a crash.

The same shallow copy issue exists in `nimcp_int8_tensor_from_fp32` at line 959, though it's less severe because the caller's `params` is typically stack-allocated or externally managed.
**Code**:
```c
// Line 1051 - shallow copy of params with pointers
memcpy(&clone->params, &tensor->params, sizeof(nimcp_int8_quant_params_t));

// nimcp_int8_tensor_destroy frees these pointers (lines 1028-1031):
if (tensor->params.channel_scales) free(tensor->params.channel_scales);
if (tensor->params.channel_zero_points) free(tensor->params.channel_zero_points);
if (tensor->params.group_scales) free(tensor->params.group_scales);
if (tensor->params.group_zero_points) free(tensor->params.group_zero_points);
```
**Fix**: Deep-copy the pointer fields after the `memcpy`:
```c
memcpy(&clone->params, &tensor->params, sizeof(nimcp_int8_quant_params_t));
// Deep-copy per-channel arrays
if (tensor->params.channel_scales) {
    clone->params.channel_scales = (float*)malloc(tensor->params.num_channels * sizeof(float));
    if (clone->params.channel_scales)
        memcpy(clone->params.channel_scales, tensor->params.channel_scales,
               tensor->params.num_channels * sizeof(float));
}
if (tensor->params.channel_zero_points) {
    clone->params.channel_zero_points = (int32_t*)malloc(tensor->params.num_channels * sizeof(int32_t));
    if (clone->params.channel_zero_points)
        memcpy(clone->params.channel_zero_points, tensor->params.channel_zero_points,
               tensor->params.num_channels * sizeof(int32_t));
}
// Same for group_scales, group_zero_points
```

---

#### P1-I2: Unchecked cudaMalloc in nimcp_int8_compute_mse (line 2026)

**Line**: 2026-2027
**Priority**: P1 - NULL dereference on GPU
**Description**: `cudaMalloc(&d_mse, sizeof(float))` is called without checking the return value. If the allocation fails, `d_mse` remains uninitialized (or NULL), and the subsequent `cudaMemset(d_mse, 0, sizeof(float))` will either corrupt memory or trigger a CUDA error. The kernel launch will then write to an invalid address.
**Code**:
```c
float* d_mse;
cudaMalloc(&d_mse, sizeof(float));       // No error check
cudaMemset(d_mse, 0, sizeof(float));     // NULL dereference if allocation failed
```
**Fix**: Check the cudaMalloc return value:
```c
float* d_mse;
cudaError_t err = cudaMalloc(&d_mse, sizeof(float));
if (err != cudaSuccess) return -1.0f;
```

---

#### P2-I1: NULL dereference in entropy calibration loop (lines 1284-1306)

**Line**: 1284-1306
**Priority**: P2 - NULL dereference in calibration path
**Description**: Inside the entropy calibration loop, three `calloc` calls allocate `ref_dist`, `quant_dist`, and `expanded` per iteration. None are NULL-checked. If any allocation fails, subsequent array accesses (lines 1289-1295, 1300-1303, 1307-1318) dereference NULL. This loop runs `num_bins - 128` times (typically ~3968 iterations with default 4096 bins), so repeated allocation/free cycles increase fragmentation risk.
**Code**:
```c
for (int threshold_bin = 128; threshold_bin < cal->num_bins; threshold_bin++) {
    float* ref_dist = (float*)calloc(threshold_bin, sizeof(float));   // No NULL check
    float* quant_dist = (float*)calloc(128, sizeof(float));           // No NULL check
    float* expanded = (float*)calloc(threshold_bin, sizeof(float));   // No NULL check
    // ... uses all three arrays ...
    free(ref_dist);
    free(quant_dist);
    free(expanded);
}
```
**Fix**: Add NULL checks and continue/break on failure:
```c
float* ref_dist = (float*)calloc(threshold_bin, sizeof(float));
float* quant_dist = (float*)calloc(128, sizeof(float));
float* expanded = (float*)calloc(threshold_bin, sizeof(float));
if (!ref_dist || !quant_dist || !expanded) {
    free(ref_dist); free(quant_dist); free(expanded);
    break;  // Use best threshold found so far
}
```

---

#### P2-I2: numel overflow in nimcp_int8_tensor_create (lines 922-926)

**Line**: 922-926
**Priority**: P2 - Integer overflow leading to undersized allocation
**Description**: Same pattern as P1-T1. The `numel` computation multiplies dimensions without overflow checking. Overflow would cause `cudaMalloc` at line 929 to allocate too few bytes, and subsequent operations would read/write past the buffer. This is P2 rather than P1 because INT8 tensors are typically created from existing FP32 tensors that have already been validated, reducing the likelihood of crafted dimensions reaching this code.
**Code**:
```c
tensor->numel = 1;
for (size_t i = 0; i < rank; i++) {
    tensor->dims[i] = dims[i];
    tensor->numel *= dims[i];  // No overflow check
}
```
**Fix**: Add overflow check mirroring `compute_numel()`:
```c
tensor->numel = 1;
for (size_t i = 0; i < rank; i++) {
    tensor->dims[i] = dims[i];
    if (dims[i] != 0 && tensor->numel > SIZE_MAX / dims[i]) {
        free(tensor->dims); free(tensor);
        return NULL;
    }
    tensor->numel *= dims[i];
}
```

---

#### P2-I3: Shallow params copy in nimcp_int8_tensor_from_fp32 (line 959)

**Line**: 959
**Priority**: P2 - Potential double-free
**Description**: `nimcp_int8_tensor_from_fp32` copies the caller's `params` struct via `memcpy`, including pointer fields. If the caller passes a `params` with allocated `channel_scales`/`channel_zero_points` arrays, both the caller and the tensor own the same pointers. If the tensor is destroyed before the caller frees their params (or vice versa), the second free is a double-free. Rated P2 because `params` is typically passed as a const pointer from a calibrator and the caller usually doesn't free the individual arrays.
**Code**:
```c
memcpy(&tensor->params, params, sizeof(nimcp_int8_quant_params_t));
```
**Fix**: Deep-copy pointer fields (same pattern as P1-I1 fix), or document that ownership is transferred and the caller must not free the arrays.

---

### 4. `src/gpu/stubs/nimcp_gpu_stubs.c`

#### P1-S1: Division by zero in nimcp_gpu_div stub (line 1209)

**Line**: 1209
**Priority**: P1 - Floating-point division by zero / undefined behavior
**Description**: The CPU stub for `nimcp_gpu_div` performs raw division without the epsilon guard that the CUDA kernel version has (see `kernel_div` at tensor_kernels.cu line 419). If any element of `b` is exactly 0.0, this produces `+/-inf` or NaN. The CUDA version adds `NIMCP_EPS` to prevent this, ensuring behavioral parity is broken between GPU and CPU paths.
**Code**:
```c
// Stub (line 1209) - no guard:
ov[i] = av[i % a->numel] / bv[i % b->numel];

// CUDA kernel (tensor_kernels.cu:419) - has guard:
out[idx] = a[idx] / (b[idx] + ((b[idx] >= 0.0f) ? NIMCP_EPS : -NIMCP_EPS));
```
**Fix**: Add the same epsilon guard as the CUDA version:
```c
float divisor = bv[i % b->numel];
ov[i] = av[i % a->numel] / (divisor + ((divisor >= 0.0f) ? 1e-7f : -1e-7f));
```

---

#### P2-S1: logf without positive clamp in nimcp_gpu_log stub (line 1549)

**Line**: 1549
**Priority**: P2 - NaN / -inf divergence from CUDA path
**Description**: The CPU stub calls `logf(xv[i])` without clamping to positive values. If `xv[i] <= 0`, this produces `-inf` (for 0) or `NaN` (for negative). The CUDA kernel version at tensor_kernels.cu line 763 clamps to `NIMCP_EPS` first.
**Code**:
```c
// Stub (line 1549) - no clamp:
ov[i] = logf(xv[i]);

// CUDA kernel (tensor_kernels.cu:763) - clamps:
out[idx] = logf(fmaxf(x[idx], NIMCP_EPS));
```
**Fix**: Add the clamp:
```c
ov[i] = logf(fmaxf(xv[i], 1e-7f));
```

---

#### P2-S2: sqrtf without non-negative clamp in nimcp_gpu_sqrt stub (line 1573)

**Line**: 1573
**Priority**: P2 - NaN divergence from CUDA path
**Description**: The CPU stub calls `sqrtf(xv[i])` without clamping to non-negative. If `xv[i] < 0`, this produces `NaN`. The CUDA kernel at tensor_kernels.cu line 772 clamps to `0.0f` first.
**Code**:
```c
// Stub (line 1573) - no clamp:
ov[i] = sqrtf(xv[i]);

// CUDA kernel (tensor_kernels.cu:772) - clamps:
out[idx] = sqrtf(fmaxf(x[idx], 0.0f));
```
**Fix**: Add the clamp:
```c
ov[i] = sqrtf(fmaxf(xv[i], 0.0f));
```

---

#### P2-S3: nimcp_gpu_tensor_from_host returns tensor when host_data is NULL (line 871)

**Line**: 871
**Priority**: P2 - Silent data corruption
**Description**: When `host_data` is NULL but `dims`/`ndim`/`precision` are valid, `nimcp_gpu_tensor_create` succeeds and returns a non-NULL tensor. The condition `!tensor || !host_data` then evaluates to `!host_data == true`, and the function returns the tensor without copying any data into it. The caller receives a valid tensor with uninitialized (garbage) contents. This should either return NULL or throw an error when `host_data` is NULL, since the function's purpose is to create a tensor from host data.
**Code**:
```c
nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, ndim, precision);
if (!tensor || !host_data) return tensor;  // Returns valid tensor with garbage data
```
**Fix**: Check `host_data` before creating the tensor, or destroy the tensor on NULL host_data:
```c
if (!host_data) return NULL;
nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, ndim, precision);
if (!tensor) return NULL;
memcpy(tensor->data, host_data, tensor->numel * tensor->elem_size);
return tensor;
```

---

## Known Issues (Not Counted)

These were previously documented and are not counted as new findings:

1. **Stream sync before cudaMemcpy (wernicke_gpu.cu)**: The `wernicke_gpu_semantic_similarity` function at line 1472 already has a `cudaStreamSynchronize` call before the `cudaMemcpy`. This known issue has been partially addressed. Other functions in the file that do async transfers also call `cudaStreamSynchronize` before synchronous copies (e.g., line 1305, line 1550).

---

## Priority Distribution by File

| File | P1 | P2 | Total |
|------|----|----|-------|
| nimcp_tensor_kernels.cu | 1 | 1 | 2 |
| nimcp_wernicke_gpu.cu | 2 | 1 | 3 |
| nimcp_int8_inference.cu | 2 | 3 | 5 |
| nimcp_gpu_stubs.c | 1 | 3 | 4 |
| **Total** | **6** | **8** | **14** |

## Patterns Observed

1. **Shallow struct copies containing pointers**: Both `nimcp_int8_tensor_clone` and `nimcp_int8_tensor_from_fp32` use raw `memcpy` on structs with pointer fields. This is a systemic issue in the INT8 inference module.

2. **GPU/CPU stub behavioral divergence**: The CPU stubs in `nimcp_gpu_stubs.c` are missing safety guards (epsilon for div, clamp for log/sqrt) that the CUDA kernels have. This means code that works correctly on GPU may produce NaN/inf on CPU-only builds.

3. **Cascading NIMCP_CUDA_RECOVER without cleanup**: The `NIMCP_CUDA_RECOVER` macro does `return false` on failure, which bypasses any cleanup of previously allocated GPU resources. Functions with multiple sequential allocations should use `NIMCP_CUDA_RECOVER_GOTO` or a goto-cleanup pattern.

4. **Missing overflow checks in numel computation**: Both `nimcp_gpu_tensor_create` and `nimcp_int8_tensor_create` compute `numel` by multiplying dimensions in a loop without overflow checking, despite a `compute_numel()` helper with proper overflow detection being available in the same file.
