# Build and Test Commands

## Build Commands

```bash
# Build main library
cd /home/bbrelin/nimcp/build && cmake .. && make nimcp -j4

# Build Python bindings
cd /home/bbrelin/nimcp/build && make nimcp_python -j4

# Install Python .so (CRITICAL after any neuron_t or synapse changes)
cp /home/bbrelin/nimcp/build/lib/python/nimcp.so ~/.local/lib/python3.12/site-packages/nimcp.cpython-312-x86_64-linux-gnu.so
```

**CRITICAL**: After ANY change to `neuron_t`, `sparse_synapse_storage_t`, or `EMBEDDED_CAPACITY`, you MUST rebuild AND reinstall the Python `.so`. The versioned `.so` in site-packages takes priority over the build directory. Stale `.so` causes SIGSEGV from shifted field offsets.

## CUDA Compilation Notes

- `nvlink` warnings about incompatible libs are normal and can be ignored.
- NVCC does not support C11 `_Atomic`. Use `volatile` + GCC `__atomic_*` builtins in shared headers included by `.cu` files.
- GPU `.cu` files must use `nimcp_malloc/nimcp_calloc/nimcp_realloc/nimcp_free` (not raw `malloc`). Include `utils/memory/nimcp_memory.h`.

## Test Execution

**480+ tests passing** across unit, integration, and cognitive test suites.

### Test directories

- `test/unit/lnn/` — LNN/Hamiltonian tests
- `test/unit/snn/` — SNN spiking network tests
- `test/unit/cnn/` — CNN visual/audio tests
- `test/unit/cognitive/` — Cognitive module tests (enhancements, native language, tokenizer, social interaction, emergent language)
- `test/unit/edge/` — Edge platform tests (sensor hub, safety watchdog, swarm, drones, motor, sim, embodiment, sensorimotor)
- `test/unit/security/` — Security tests (LGSS enhanced, audit log)
- `test/unit/core/` — Core brain tests
- `test/unit/tensor/` — Tensor operation tests

### Core test commands

```bash
# Run specific test by name
cd /home/bbrelin/nimcp/build && ctest -R test_name

# Run all tests with output on failure
cd /home/bbrelin/nimcp/build && ctest --output-on-failure

# Run a specific test binary directly with gtest
./test/unit/lnn/unit_lnn_test_lnn_config --gtest_brief=1
```

### Edge module tests (sensor, watchdog, swarm, drones, motor, sim, embodiment, sensorimotor)

```bash
make unit_edge_test_sensor_hub unit_edge_test_safety_watchdog unit_edge_test_swarm_lifecycle unit_edge_test_swarm_sync unit_edge_test_swarm_byzantine unit_edge_test_drone_bridges unit_edge_test_motor_output unit_edge_test_sim_bridge unit_edge_test_embodiment unit_edge_test_sensorimotor -j4
```

### Cognitive enhancement tests

```bash
make unit_cognitive_test_enhancements unit_cognitive_test_native_language unit_cognitive_test_tokenizer -j4
```

### Security tests (LGSS + audit)

```bash
make unit_security_test_lgss_enhanced unit_security_test_audit_log -j4
```

### Social + emergent language tests

```bash
make unit_cognitive_test_social_interaction unit_cognitive_test_emergent -j4
```

## Training

```bash
# Immersive developmental training
cd /home/bbrelin/nimcp && python3 scripts/immerse_athena.py

# Resume from checkpoint
python3 scripts/immerse_athena.py --resume
```

## Git Workflow

```bash
git add -A && git commit --no-verify -m "message" && git push
```

## Pre-Commit Verification

Always run `make nimcp -j4` before any git commit to verify the build succeeds. New test files must be built and run before committing.
