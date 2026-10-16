# Tensor Integration

## API

| Function | Notes |
|----------|-------|
| `nimcp_tensor_create(dims, rank, dtype)` | 3 args -- rank, NOT ndims |
| `nimcp_tensor_sum()` | Returns `nimcp_tensor_t*`, NOT scalar |
| `nimcp_tensor_mul_scalar_()` | In-place multiplication |
| `nimcp_tensor_norm_p(tensor, p)` | Lp norm |
| `nimcp_tensor_numel()` | Element count |
| `nimcp_tensor_data()` / `nimcp_tensor_data_const()` | Raw pointer access |

## GOTCHAs
- `nimcp_tensor_sum()` returns a tensor, not a float -- must extract value
- `nimcp_tensor_create()` takes rank (not ndims) as second arg
- `op_div` uses epsilon clamping (1e-7) -- no LOG_WARN, no div-by-zero issues

## Optimizations
- Fused mul-add operation for hot-path optimization
- GPU tensor operations via CUDA kernels

## Integration Phases
- Phase 1: Gradient Manager, Z-Score Normalizer, Loss Functions
- Phase 2: Optimizers, Visual Cortex, Audio Cortex
- Phase 3: Population Coding, Feature Extractor, Thalamic Router

## Test Coverage: 289 tests
