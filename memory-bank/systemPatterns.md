# System Patterns

## Source Organization
```
nimcp/
├── src/                    # Implementation (2,200+ .c files)
│   ├── api/               # External API layer
│   ├── core/              # Brain, neurons, synapses, topology
│   ├── cognitive/         # Higher cognition (emotions, memory, attention)
│   ├── plasticity/        # Learning mechanisms (STDP, BCM, homeostatic)
│   ├── glial/             # Astrocytes, oligodendrocytes, microglia
│   ├── security/          # BBB, immune system, validation
│   ├── mesh/              # Distributed networking
│   ├── gpu/               # CUDA acceleration
│   ├── quantum/           # Quantum statistics
│   ├── lnn/               # Liquid Neural Networks
│   ├── snn/               # Spiking Neural Networks
│   ├── swarm/             # Distributed swarm intelligence
│   ├── perception/        # Sensory processing
│   ├── language/          # NLP, linguistics
│   └── ...                # 30+ more modules
├── include/               # Headers (2,369 .h files)
├── test/                  # Test suites
├── docs/                  # Documentation
│   └── claude/            # AI-readable docs
│       └── modules/       # Per-module documentation
└── build/                 # CMake build output
```

## Naming Conventions
- Functions: `nimcp_<module>_<action>()` (e.g., `nimcp_brain_create()`)
- Types: `nimcp_<type>_t` (e.g., `nimcp_neuron_t`)
- Constants: `NIMCP_<CATEGORY>_<NAME>` (e.g., `NIMCP_ERROR_INVALID`)
- Files: `nimcp_<module>.c`, `nimcp_<module>.h`

## Critical API Patterns

### Tensor API
- `nimcp_tensor_sum()` returns `nimcp_tensor_t*`, NOT scalar
- `nimcp_tensor_create(dims, rank, dtype)` requires 3 args

### Mutex API (Thread Layer)
- `nimcp_mutex_create(attr)` — allocate and init, returns `nimcp_mutex_t*`
- `nimcp_mutex_init(mutex, attr)` — init existing struct
- **Deadlock prevention**: Create `*_unlocked()` helpers for internal calls

### Return Values
- FEP bridges: `0` for success, `-1` for errors
- Standard functions: `nimcp_error_t` codes (NIMCP_OK, NIMCP_ERROR_*)

### Brain Immune System
- B cells must be in PLASMA state to produce antibodies
- State progression: NAIVE → ACTIVATED → PLASMA

### Platform Tiers
- `PLATFORM_TIER_FULL/MEDIUM/CONSTRAINED/MINIMAL`

## Build Commands
```bash
# Standard build
cd /home/bbrelin/nimcp/build && cmake .. && make nimcp -j4

# Run tests
ctest --output-on-failure

# Git workflow
git add -A && git commit --no-verify -m "message" && git push
```

## Test Categories
- `unit_*` — Unit tests for individual functions
- `integration_*` — Cross-module integration tests
- `regression_*` — Backward compatibility tests
- `e2e_*` — End-to-end scenario tests
