# Portia Accelerator Detection - Quick Start Guide

## 5-Minute Quick Start

### 1. Basic Detection (3 lines of code)

```c
#include "portia/nimcp_portia_accelerator.h"

portia_accelerator_system_t sys = portia_accelerator_init(NULL);
uint32_t count = portia_accelerator_detect_all(sys);
portia_accelerator_shutdown(sys);
```

### 2. Get Best Accelerator

```c
portia_accelerator_system_t sys = portia_accelerator_init(NULL);
portia_accelerator_detect_all(sys);

accelerator_info_t best;
if (portia_accelerator_get_best(sys, &best)) {
    printf("Using: %s - %.2f TFlops, %.2f W\n",
           best.name, best.peak_tflops, best.power_watts);
}

portia_accelerator_shutdown(sys);
```

### 3. Power-Efficient Setup

```c
portia_accelerator_config_t config = {
    .detect_gpu = false,
    .detect_npu = true,
    .detect_tpu = true,
    .power_budget_watts = 10.0f,
    .prefer_low_power = true
};

portia_accelerator_system_t sys = portia_accelerator_init(&config);
portia_accelerator_detect_all(sys);
```

---

## Common Use Cases

### Training (High Performance)

```c
portia_accelerator_config_t config = portia_accelerator_default_config();
config.min_memory_gb = 8.0f;
config.min_tflops = 10.0f;

portia_accelerator_system_t sys = portia_accelerator_init(&config);
portia_accelerator_detect_gpu(sys); // Only GPUs

accelerator_info_t gpu;
if (portia_accelerator_get_best(sys, &gpu)) {
    // Use GPU for training
}
```

### Edge Inference (Low Power)

```c
portia_accelerator_config_t config = {
    .detect_npu = true,
    .detect_tpu = true,
    .power_budget_watts = 5.0f,
    .min_tflops = 0.5f,
    .prefer_low_power = true,
    .require_int8 = true
};

portia_accelerator_system_t sys = portia_accelerator_init(&config);
portia_accelerator_detect_all(sys);
```

### Enumerate All

```c
portia_accelerator_system_t sys = portia_accelerator_init(NULL);
uint32_t count = portia_accelerator_detect_all(sys);

for (uint32_t i = 0; i < count; i++) {
    accelerator_info_t info;
    portia_accelerator_get_info(sys, i, &info);
    portia_accelerator_print_info(&info);
}
```

---

## API Cheat Sheet

### Initialization
| Function | Purpose |
|----------|---------|
| `portia_accelerator_default_config()` | Get default config |
| `portia_accelerator_init(config)` | Initialize system |
| `portia_accelerator_shutdown(sys)` | Cleanup |

### Detection
| Function | Purpose |
|----------|---------|
| `portia_accelerator_detect_all(sys)` | Detect all types |
| `portia_accelerator_detect_gpu(sys)` | Detect GPUs only |
| `portia_accelerator_detect_npu(sys)` | Detect NPUs only |

### Query
| Function | Purpose |
|----------|---------|
| `portia_accelerator_get_best(sys, info)` | Get optimal accelerator |
| `portia_accelerator_get_info(sys, idx, info)` | Get by index |
| `portia_accelerator_get_by_type(sys, type, info)` | Get by type |
| `portia_accelerator_get_count(sys)` | Total count |

### Selection
| Function | Purpose |
|----------|---------|
| `portia_accelerator_set_preferred(sys, type)` | Set preferred type |
| `portia_accelerator_get_preferred(sys)` | Get current preferred |
| `portia_accelerator_is_available(sys, type)` | Check availability |

### Utilities
| Function | Purpose |
|----------|---------|
| `portia_accelerator_type_name(type)` | Get type name |
| `portia_accelerator_print_info(info)` | Print details |
| `portia_accelerator_estimate_power(info, util)` | Estimate power |
| `portia_accelerator_calculate_score(info, cfg)` | Score accelerator |

---

## Accelerator Types

```c
ACCELERATOR_TYPE_GPU   // Graphics Processing Unit
ACCELERATOR_TYPE_NPU   // Neural Processing Unit
ACCELERATOR_TYPE_DSP   // Digital Signal Processor
ACCELERATOR_TYPE_FPGA  // Field Programmable Gate Array
ACCELERATOR_TYPE_TPU   // Tensor Processing Unit
```

---

## Configuration Options

```c
typedef struct {
    bool detect_gpu;           // Enable GPU detection
    bool detect_npu;           // Enable NPU detection
    bool detect_dsp;           // Enable DSP detection
    bool detect_fpga;          // Enable FPGA detection
    bool detect_tpu;           // Enable TPU detection
    bool auto_select;          // Auto-pick best
    float power_budget_watts;  // Max power
    float min_memory_gb;       // Min memory
    float min_tflops;          // Min performance
    bool prefer_low_power;     // Prefer efficiency
    bool require_fp16;         // Require FP16
    bool require_int8;         // Require INT8
} portia_accelerator_config_t;
```

---

## Accelerator Info

```c
typedef struct {
    accelerator_type_t type;   // Type of accelerator
    char name[64];             // Device name
    char vendor[32];           // Vendor name
    uint32_t compute_units;    // Compute units
    uint64_t memory_bytes;     // Memory size
    float peak_tflops;         // Peak performance
    float power_watts;         // Power consumption
    bool available;            // Is available
    bool initialized;          // Is initialized
} accelerator_info_t;
```

---

## Example Workflows

### Workflow 1: Simple Detection
```
init() → detect_all() → get_best() → shutdown()
```

### Workflow 2: Type-Specific
```
init() → detect_gpu() → get_by_type(GPU) → set_preferred(GPU) → shutdown()
```

### Workflow 3: Custom Selection
```
init(custom_config) → detect_all() → enumerate_all() →
    calculate_scores() → select_best() → shutdown()
```

---

## Error Handling

All functions return appropriate error indicators:

- **NULL pointer**: Functions return 0, false, or NULL
- **Invalid index**: Returns false
- **No accelerators**: Returns 0 or false (not an error)
- **BBB violations**: Logged and returns error

Always check return values:

```c
accelerator_info_t info;
if (!portia_accelerator_get_best(sys, &info)) {
    // Handle no accelerator case
    // Fall back to CPU
}
```

---

## Performance Tips

1. **Cache detection results**: Don't re-detect on every call
2. **Use type-specific detection**: Faster than detect_all()
3. **Set power budgets**: Prevents thermal issues
4. **Prefer low power for edge**: Better battery life
5. **Benchmark actual workloads**: Scoring is heuristic

---

## Common Pitfalls

### ❌ Don't Do This

```c
// Forgot to initialize
portia_accelerator_detect_all(NULL); // Crash!

// No error checking
accelerator_info_t info;
portia_accelerator_get_best(sys, &info);
printf("%s\n", info.name); // May be empty!

// No cleanup
portia_accelerator_init(NULL);
// Memory leak!
```

### ✅ Do This

```c
// Proper initialization
portia_accelerator_system_t sys = portia_accelerator_init(NULL);
if (!sys) {
    fprintf(stderr, "Init failed\n");
    return 1;
}

// Check return values
accelerator_info_t info;
if (portia_accelerator_get_best(sys, &info)) {
    printf("Best: %s\n", info.name);
} else {
    printf("No accelerators, using CPU\n");
}

// Always cleanup
portia_accelerator_shutdown(sys);
```

---

## Debugging

### Enable Debug Logging

```c
#include "utils/logging/nimcp_logging.h"

nimcp_log_init("portia_debug.log");
nimcp_log_set_level(NIMCP_LOG_LEVEL_DEBUG);

// Your accelerator code here

nimcp_log_shutdown();
```

### Check Detection Results

```c
portia_accelerator_system_t sys = portia_accelerator_init(NULL);
uint32_t count = portia_accelerator_detect_all(sys);

printf("Detected: %u\n", count);
printf("Type mask: 0x%X\n", portia_accelerator_get_type_mask(sys));

portia_accelerator_print_all(sys); // Detailed info
```

---

## Building

### With CMake

```cmake
target_link_libraries(your_app
    nimcp_portia_accelerator
    nimcp_bbb_helpers
    nimcp_logging
    nimcp_memory
)
```

### Compile Flags

```bash
gcc -o my_app my_app.c \
    -I/path/to/nimcp/include \
    -L/path/to/nimcp/lib \
    -lnimcp_portia_accelerator \
    -ldl -lpthread
```

---

## Testing

### Run Unit Tests

```bash
cd build
ctest -R test_accelerator -V
```

### Run Demo

```bash
./examples/portia_accelerator_demo
```

---

## Platform-Specific Notes

### Linux
- All accelerator types supported
- Requires /dev access for device files
- May need sudo for some operations

### macOS
- GPU and NPU (Apple Silicon) supported
- Limited DSP/FPGA support
- Use Metal for Apple GPU

### Windows
- GPU detection via CUDA/DirectML
- Limited NPU support
- Requires vendor drivers

---

## Need Help?

- **Full Docs**: docs/PORTIA_ACCELERATOR_DETECTION.md
- **Demo Code**: examples/portia_accelerator_demo.c
- **Tests**: test/unit/portia/test_accelerator.cpp
- **Issues**: https://github.com/nimcp/nimcp/issues

---

**Last Updated:** 2025-12-08
**Version:** 1.0.0
