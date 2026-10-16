# GPU Memory Management

**Last Updated:** 2026-04-19 (CSR V2 persistent memory)

## GPU Memory Budget (RTX 5090, 32 GB VRAM)

| Component | Peak Use |
|-----------|---------:|
| ANN weights (150K neurons, teacher) | ~600 MB |
| SNN LIF state (1.8M × 3 floats) | ~22 MB |
| SNN CSR weights (1.45B × 4 bytes) | **5.8 GB** |
| SNN CSR col_indices (1.45B × 4 bytes) | **5.8 GB** |
| SNN CSR row_ptr | ~7 MB |
| Spike vectors (1.8M × 4 bytes) | ~7 MB |
| LNN state (512 × sparse) | ~1 MB |
| CNN weights (4 cortices × ~500K params) | ~8 MB |
| GPU plasticity state | ~1-2 GB |
| Transient tensors (inference, backprop) | ~2-3 GB |
| **Total** | **~15-16 GB** (16+ GB headroom) |

Note: memory budget was revised after SNN became primary (previously ANN
was 2M → 8 GB). Current ANN is 150K teacher, dramatically reducing
weight-matrix footprint. SNN CSR still dominates.

After CSR V2 persistent residency (2026-04), the 11.6 GB of CSR data lives
on the GPU continuously instead of being re-uploaded per step.

## CSR Lifecycle (V2 — current, 2026-04-19)

```
Population created
    │
    ▼
snn_csr_create(n_neurons, capacity)
    │   entries allocated (COO mode)
    ▼
snn_csr_add_entry_coo(...) × N
    │
    ▼
snn_csr_finalize(csr)
    │   sorted, row_ptr built
    │   entries now in CSR form
    ▼
snn_csr_prepare_gpu(csr, pop_offsets, n_pops)
    │   HOST: weights[] and flat_col_idx[] allocated + filled
    │   gpu_ready = true
    ▼
snn_csr_upload_to_gpu(csr, gpu_ctx)          ◀── V2
    │   DEVICE: d_weights, d_flat_col_idx, d_row_ptr allocated + uploaded
    │   d_gpu_ctx stored for lifecycle management
    │   gpu_resident = true
    │
    ├── Per training step (no transfer): kernel reads d_* directly
    │
    ├── On homeostatic scaling: weights[] updated, then
    │   snn_csr_sync_weights_to_gpu(csr) syncs H→D
    │   (uses d_gpu_ctx stored in struct — no caller-supplied ctx)
    │
    └── On destroy:
         snn_csr_release_gpu(csr)   ◀── frees using d_gpu_ctx
         snn_csr_destroy(csr)       ◀── frees host allocations
```

### GPU Context Persistence (fix 2026-04-19)

Previously `snn_csr_sync_weights_to_gpu` and `snn_csr_release_gpu` passed
NULL context to `nimcp_gpu_free` / `nimcp_gpu_memcpy`, relying on the
backend accepting NULL. Not guaranteed across all backends.

Current design stores the context at upload time in `csr->d_gpu_ctx`,
so lifecycle operations have a valid context. The brain guarantees ctx
outlives the CSR.

Fields added to `snn_csr_storage_t`:
```c
void* d_weights;
void* d_flat_col_idx;
void* d_row_ptr;
void* d_gpu_ctx;       /* nimcp_gpu_context_t* used for upload */
bool  gpu_resident;
```

## GPU Memory API

From `include/gpu/context/nimcp_gpu_context.h`:

```c
void* nimcp_gpu_malloc(nimcp_gpu_context_t* ctx, size_t size_bytes);
void  nimcp_gpu_free(nimcp_gpu_context_t* ctx, void* dev_ptr);
int   nimcp_gpu_memcpy(nimcp_gpu_context_t* ctx,
                         void* dst, const void* src,
                         size_t size_bytes,
                         nimcp_gpu_memcpy_kind_t kind);
```

`nimcp_gpu_memcpy_kind_t` values:
- `GPU_MEMCPY_HOST_TO_DEVICE`
- `GPU_MEMCPY_DEVICE_TO_HOST`
- `GPU_MEMCPY_DEVICE_TO_DEVICE`
- `GPU_MEMCPY_HOST_TO_HOST`

NULL context accepts default context.

## Before/After (Phase A.1 measurement)

### Before V2 (legacy path)
Per step, per population (46 of them):
```
Upload weights             ~1.3 GB     ◀── PCIe Gen4 ≈ 50 GB/s
Upload col_indices         ~1.3 GB
Upload row_ptr             ~7 MB
Launch kernel              ~5 ms
Download I_syn             ~7 MB
Destroy GPU tensors        ~1 ms
```
Total: ~1.3-1.5s per step (PCIe-bound).

### After V2
Per step, per population:
```
(CSR already resident — no upload)
Upload external_current    ~7 MB       ◀── still per-step, OK
Launch kernel              ~5 ms
Download I_syn             ~7 MB
```
Total: ~100-200ms per step estimated.

## Weight Update Lifecycle

Weights change during training via:
1. R-STDP rule (modifies `csr->entries[i].weight`)
2. Homeostatic scaling (multiplies all entries in a population)

The `csr->weights[]` flat array is kept in sync with entries via explicit
sync pass in `snn_homeostatic_scaling()`. When `gpu_resident=true`, that
sync also pushes to device via `snn_csr_sync_weights_to_gpu()`.

## Memory Leak Prevention

`snn_csr_destroy()` always calls `snn_csr_release_gpu()` first. All device
allocations go through the gpu_ctx abstraction which tracks them for
leak detection. Use `compute-sanitizer` during development.

## Known Gaps

1. **Weight update granularity** — currently syncs entire weights array
   on every homeostatic pass. Could be per-population partial sync for
   populations that actually changed.

2. **GPU-side R-STDP** — R-STDP currently runs on CPU, modifying host
   entries[]. GPU implementation would be faster but requires atomics
   for correctness.

3. **Context leakage** — `nimcp_gpu_free(NULL, ...)` relies on default
   context being set. If multiple contexts exist, this could free from
   wrong context. Practically not an issue (single-context design).

## See Also

- [20_snn.md](20_snn.md) — SNN data structures
- [10_training_paradigm.md](10_training_paradigm.md) — where SNN fits
