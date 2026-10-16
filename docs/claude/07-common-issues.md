# Common Issues and Solutions

## CRITICAL: Python .so Stale After Struct Changes

After ANY change to `neuron_t`, `sparse_synapse_storage_t`, or `EMBEDDED_CAPACITY`, you MUST:
1. Rebuild: `make nimcp_python -j4`
2. Reinstall: `cp build/lib/python/nimcp.so ~/.local/lib/python3.12/site-packages/nimcp.cpython-312-x86_64-linux-gnu.so`

The versioned `.so` in site-packages takes priority over the build directory. A stale `.so` compiled against old struct layouts causes SIGSEGV from shifted field offsets (e.g., `edp_is_active` reading garbage after `EMBEDDED_CAPACITY` change).

**Current `EMBEDDED_CAPACITY` is 320** (not 128 or 256).

---

## NVCC and _Atomic Incompatibility

NVCC does not support C11 `_Atomic`. In headers shared between C and CUDA:
- Use `volatile` + GCC `__atomic_*` builtins instead of `_Atomic`
- `_Atomic double` requires `-latomic` on GCC/Linux

---

## GPU .cu Files: No Raw malloc

All GPU `.cu` files must use `nimcp_malloc/nimcp_calloc/nimcp_realloc/nimcp_free` (not raw `malloc/calloc/realloc/free`). Include `utils/memory/nimcp_memory.h`.

---

## Tensor Division

`op_div` uses epsilon clamping (1e-7) instead of returning 0 + LOG_WARN. This eliminates ~60 warnings per predict/learn cycle from LNN ODE steps.

---

## Checkpoint Save Threads

Use `daemon=False` on checkpoint save threads. Daemon threads are killed on process exit, which can corrupt checkpoint files.

---

## LNN Gradient Issues

- Per-layer tensors are the real gradients, not `ctx->grad_params`
- LNN gradient clip to 1.0 norm after adjoint computation
- Per-step gradient clamping `[-1e4, 1e4]` prevents accumulation explosion
- `tau_safe` floor 0.01 prevents `1/tau^2` explosion

---

## setjmp/longjmp Requires Volatile

Variables modified between `setjmp` and `longjmp` MUST be declared `volatile`. Without this, the compiler may optimize them into registers that get clobbered by `longjmp`.

---

## Mutex API Confusion

`nimcp_mutex_free()` = destroy + free. This IS correct for heap-allocated mutexes from `nimcp_mutex_create()`. This is NOT a bug. Walkthrough reports flagging "nimcp_mutex_free should be nimcp_mutex_destroy" are false positives for heap-allocated mutexes.

---

## Decision Struct Lifecycle

- Always use `brain_free_decision()` to free decision structs (not `nimcp_free()`)
- Always use `copy_decision_deep()` for caching decisions (not shallow/CoW copy)

---

## Files That Must NEVER Have Raw NIMCP_THROW_TO_IMMUNE

These files would cause infinite recursion:
1. `src/utils/exception/` — ALL files
2. `src/utils/memory/nimcp_memory.c` — Use `MEMORY_SAFE_THROW()` instead
3. `src/utils/memory/nimcp_unified_memory.c` — Use `UMM_SAFE_THROW()` instead
4. `src/security/nimcp_constant_time.c` — Gate with `nimcp_exception_system_is_initialized()`

---

## Memory Implementation Files: Raw malloc Only

`nimcp_memory.c`, `nimcp_unified_memory.c`, and `nimcp_constant_time.c` MUST use raw `malloc/calloc/free/realloc`. Using the nimcp memory wrappers here would cause infinite recursion.

---

## Bio-Router Re-registration

"Bio-async router not available, skipping registration" is normal/expected in tests. Bio-router re-registration messages are LOG_DEBUG (not LOG_WARN).

---

## Guard Clause Pattern

Both braces AND return are required after `NIMCP_THROW_TO_IMMUNE`. The macro alone does not halt execution:

```c
if (error_condition) {
    NIMCP_THROW_TO_IMMUNE(...);
    return NIMCP_ERROR_INVALID_STATE;  // REQUIRED
}
```

---

## CMake Issues

- If `add_subdirectory()` causes duplicate target errors, check if the subdirectory is already added from `test/CMakeLists.txt`
- CMakeLists may reference non-existent test files. Verify with `ls` before adding tests
- Some modules require manual CMakeLists.txt edits

---

## Inconsistent Function Prefixes (Low Priority)

These prefixes deviate from `nimcp_*` convention but are kept for backward compatibility:
- `shannon_*` — in `include/information/nimcp_shannon.h` and related files
- `cross_modal_*` — in `include/information/nimcp_cross_modal.h` and related files
- `neural_network_*` — in `include/core/neuralnet/nimcp_neuralnet.h`

No action required for current development. Refactoring would require a major version bump.

---

## LNN Wiring

Use `lnn_wiring_is_connected()` (inline alias) or `lnn_wiring_has_edge()` to check connectivity.

---

## Brain Immune B Cells

B cells must be in PLASMA state to produce antibodies. State progression: NAIVE -> ACTIVATED -> PLASMA. Use `brain_immune_t_help_b()` to transition from ACTIVATED to PLASMA.
