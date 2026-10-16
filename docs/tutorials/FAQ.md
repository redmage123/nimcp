# NIMCP Frequently Asked Questions (FAQ)

## Build Issues

### Q: CMake fails with "CMake version X required"

**A:** Update CMake to version 3.16 or later:

```bash
# Ubuntu
sudo apt update
sudo apt install cmake

# Or use Kitware repository for latest
wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | sudo tee /usr/share/keyrings/kitware-archive-keyring.gpg >/dev/null
echo 'deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ focal main' | sudo tee /etc/apt/sources.list.d/kitware.list >/dev/null
sudo apt update
sudo apt install cmake
```

---

### Q: "libnimcp.so not found" error when running examples

**A:** The library path is not set. Add it to your environment:

```bash
export LD_LIBRARY_PATH=/home/bbrelin/nimcp/build/src/lib:$LD_LIBRARY_PATH
```

To make permanent, add to `~/.bashrc`:

```bash
echo 'export LD_LIBRARY_PATH=/home/bbrelin/nimcp/build/src/lib:$LD_LIBRARY_PATH' >> ~/.bashrc
source ~/.bashrc
```

---

### Q: Build fails with "undefined reference to pthread_*"

**A:** Pthread library not linked. This is usually fixed automatically, but if not:

```bash
cmake -DCMAKE_C_FLAGS="-pthread" -DCMAKE_CXX_FLAGS="-pthread" ..
```

---

### Q: Tests fail with "gtest not found"

**A:** Install Google Test:

```bash
# Ubuntu
sudo apt install libgtest-dev

# Build and install
cd /usr/src/gtest
sudo cmake .
sudo make
sudo cp lib/*.a /usr/lib/

# Or on newer Ubuntu
sudo apt install libgtest-dev googletest
```

---

### Q: Python bindings fail with "cannot find Python.h"

**A:** Install Python development headers:

```bash
# Ubuntu
sudo apt install python3-dev

# macOS (using Homebrew)
brew install python
```

---

## API Questions

### Q: What's the difference between `nimcp_tensor_sum()` return types?

**A:** `nimcp_tensor_sum()` returns a **tensor**, not a scalar:

```c
// WRONG - assumes scalar return
float sum = nimcp_tensor_sum(tensor);

// CORRECT - returns tensor
nimcp_tensor_t* sum_tensor = nimcp_tensor_sum(tensor);
float sum = nimcp_tensor_get_scalar(sum_tensor);
nimcp_tensor_destroy(sum_tensor);
```

---

### Q: How do I create a tensor correctly?

**A:** `nimcp_tensor_create()` requires 3 arguments:

```c
// WRONG - missing rank
nimcp_tensor_t* tensor = nimcp_tensor_create(dims);

// CORRECT - dims, rank, dtype
size_t dims[] = {10, 20, 30};
nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 3, NIMCP_DTYPE_FLOAT32);
```

---

### Q: Which mutex API should I use?

**A:** Use the **thread layer**, not the platform layer:

```c
// CORRECT - thread layer
nimcp_mutex_t* mutex = nimcp_mutex_create(NULL);
nimcp_mutex_lock(mutex);
nimcp_mutex_unlock(mutex);
nimcp_mutex_destroy(mutex);

// Also correct - init existing struct
nimcp_mutex_t mutex;
nimcp_mutex_init(&mutex, NULL);
nimcp_mutex_lock(&mutex);
```

Mutex attributes support: `MUTEX_TYPE_NORMAL`, `MUTEX_TYPE_RECURSIVE`, `MUTEX_TYPE_ERRORCHECK`.

---

### Q: What do platform tiers mean?

**A:** Platform tiers control feature availability:

| Tier | Description | Use Case |
|------|-------------|----------|
| `PLATFORM_TIER_FULL` | All features enabled | Desktop, server |
| `PLATFORM_TIER_MEDIUM` | Most features, reduced precision | Laptop, mobile |
| `PLATFORM_TIER_CONSTRAINED` | Limited features | Embedded systems |
| `PLATFORM_TIER_MINIMAL` | Core only | Microcontrollers |

---

### Q: Why does FEP bridge return 0/-1 instead of NIMCP_OK?

**A:** FEP (Free Energy Principle) bridges use simple return codes:

```c
// FEP bridges
int result = fep_bridge_process(bridge, data);
if (result == 0) {
    // Success
} else {
    // Error (result == -1)
}

// NOT like standard NIMCP functions
nimcp_error_t err = nimcp_function(...);
if (err == NIMCP_OK) { ... }
```

---

### Q: How do B cells produce antibodies?

**A:** B cells must be in PLASMA state:

```c
// B cell must transition through states
nimcp_b_cell_activate(b_cell);        // NAIVE -> ACTIVATED
nimcp_b_cell_differentiate(b_cell);   // ACTIVATED -> PLASMA

// Only PLASMA state produces antibodies
if (nimcp_b_cell_get_state(b_cell) == NIMCP_B_CELL_PLASMA) {
    antibody = nimcp_b_cell_produce_antibody(b_cell, antigen);
}
```

---

## Design Questions

### Q: How do I avoid deadlocks with mutexes?

**A:** Never call public mutex-locking functions from within locked code. Create `*_unlocked()` helper functions:

```c
// PUBLIC function - locks mutex
nimcp_error_t nimcp_module_process(module_t* mod) {
    nimcp_mutex_lock(mod->mutex);
    nimcp_error_t result = nimcp_module_process_unlocked(mod);
    nimcp_mutex_unlock(mod->mutex);
    return result;
}

// INTERNAL helper - assumes mutex held
static nimcp_error_t nimcp_module_process_unlocked(module_t* mod) {
    // ... processing without locking
    // Can call other *_unlocked() functions safely
}
```

---

### Q: What's the metabolic modulation return convention?

**A:** `metabolic_compute_effects()` returns 0 for success, -1 for errors:

```c
int result = metabolic_compute_effects(metabolic, effects);
if (result == 0) {
    // Success - use effects
} else {
    // Error
}
```

---

### Q: Does NIMCP require a GPU?

**A:** No. NIMCP is designed to run on CPU:

- Optimized for modest hardware
- No CUDA/OpenCL dependencies
- GPU acceleration is optional and experimental

---

### Q: Can I use NIMCP in production?

**A:** The Brain API is production-ready. Other components vary:

| Component | Status |
|-----------|--------|
| Brain API | Production ready |
| Ethics Engine | Production ready |
| Curiosity System | In development |
| Knowledge Acquisition | In development |
| Middleware | Stable |
| Networking | Stable |

---

## Performance Questions

### Q: How do I optimize for production?

**A:** Use release build with optimizations:

```bash
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="-O3 -march=native" ..
make -j4
```

---

### Q: How much memory does a brain use?

**A:** Depends on size:

| Size | Approximate Memory |
|------|-------------------|
| TINY | ~1 MB |
| SMALL | ~10 MB |
| MEDIUM | ~50 MB |
| LARGE | ~500 MB |

Reduce memory with pruning:

```c
brain_prune(brain, 0.01f);  // Remove weak connections
```

---

### Q: Why are my tests slow?

**A:** Try parallel test execution:

```bash
# Run tests in parallel
ctest -j4

# Or run specific test suite
./test/unit/test_brain --gtest_filter="BrainTest.*"
```

---

## Getting More Help

- **Documentation**: [docs/INDEX.md](../INDEX.md)
- **Coding Standards**: [docs/claude/02-coding-standards.md](../claude/02-coding-standards.md)
- **API Reference**: [docs/api/API_REFERENCE.md](../api/API_REFERENCE.md)
- **Issue Tracker**: https://github.com/redmage123/nimcp/issues
