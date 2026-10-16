# NIMCP Error Code Strategy

## Overview

NIMCP uses a **stratified error code system** with module-specific ranges to avoid collisions and enable quick identification of error sources.

## Error Code Ranges

### Core System Codes (1000-9999)
Defined in `include/utils/error/nimcp_error_codes.h`

| Range | Category | Examples |
|-------|----------|----------|
| **1000-1999** | **General Errors** | INVALID_PARAMETER (1002), NULL_POINTER (1003), INVALID_STATE (1005) |
| **2000-2999** | **Memory Errors** | NO_MEMORY (2000), BUFFER_TOO_SMALL (2001), BUFFER_OVERFLOW (2002) |
| **3000-3999** | **Neural/Brain** | BRAIN_CREATION (3000), DIMENSION_MISMATCH (3004) |
| **4000-4999** | **I/O Errors** | FILE_NOT_FOUND (4000), SERIALIZATION (4006) |
| **5000-5999** | **Configuration** | CONFIG_INVALID (5000), CONFIG_PARSE (5001) |
| **6000-6999** | **Threading** | THREAD_CREATE (6000), MUTEX_LOCK (6002), DEADLOCK (6005) |
| **7000-7999** | **Signal/Fault** | SIGSEGV (7001), SIGABRT (7002), CRASH_RECOVERY (7006) |
| **8000-8999** | **Cognitive** | WORKING_MEMORY (8000), THEORY_OF_MIND (8005) |

### Brain Region Errors (10000-19999)
Each brain region gets a 100-code range. The module-local enum value N maps to `NIMCP_ERROR_<REGION>_BASE + N`.

| Range | Brain Region | Base Constant |
|-------|--------------|---------------|
| **10000-10099** | Motor Cortex | `NIMCP_ERROR_MOTOR_BASE` |
| **10100-10199** | Hippocampus | `NIMCP_ERROR_HIPPOCAMPUS_BASE` |
| **10200-10299** | Entorhinal Cortex | `NIMCP_ERROR_ENTORHINAL_BASE` |
| **10300-10399** | Prefrontal Cortex | `NIMCP_ERROR_PREFRONTAL_BASE` |
| **10400-10499** | Cerebellum | `NIMCP_ERROR_CEREBELLUM_BASE` |
| **10500-10599** | Thalamus | `NIMCP_ERROR_THALAMUS_BASE` |
| **10600-10699** | Hypothalamus | `NIMCP_ERROR_HYPOTHALAMUS_BASE` |
| **10700-10799** | Amygdala | `NIMCP_ERROR_AMYGDALA_BASE` |
| **10800-10899** | Basal Ganglia | `NIMCP_ERROR_BASAL_GANGLIA_BASE` |
| **10900-10999** | Cingulate Cortex | `NIMCP_ERROR_CINGULATE_BASE` |
| **11000-11099** | Insula | `NIMCP_ERROR_INSULA_BASE` |
| **11100-11199** | Occipital Lobe | `NIMCP_ERROR_OCCIPITAL_BASE` |
| **11200-11299** | Parietal Lobe | `NIMCP_ERROR_PARIETAL_BASE` |
| **11300-11399** | Temporal Lobe | `NIMCP_ERROR_TEMPORAL_BASE` |
| **11400-11499** | Broca's Area | `NIMCP_ERROR_BROCA_BASE` |
| **11500-11599** | Wernicke's Area | `NIMCP_ERROR_WERNICKE_BASE` |
| **11600-11699** | Brainstem | `NIMCP_ERROR_BRAINSTEM_BASE` |
| **11700-11799** | Parahippocampal | `NIMCP_ERROR_PARAHIPPOCAMPAL_BASE` |
| **11800-11899** | Perirhinal Cortex | `NIMCP_ERROR_PERIRHINAL_BASE` |
| **11900-11999** | VTA | `NIMCP_ERROR_VTA_BASE` |

### Module-Specific Positive Codes (20000+)
Reserved for module-specific high-level error codes:

| Range | Module | Base |
|-------|--------|------|
| **20000-20999** | **Portia** | 20000 |
| **21000-21999** | **Swarm** | 21000 |
| **22000-22999** | **Security** | 22000 |
| **23000-23999** | **Reserved** | 23000 |

**Example (Portia):**
```c
#define NIMCP_PORTIA_ERROR_BASE 20000
#define NIMCP_PORTIA_ERROR_NOT_INITIALIZED      (NIMCP_PORTIA_ERROR_BASE + 1)  // 20001
#define NIMCP_PORTIA_ERROR_TIER_LOCKED          (NIMCP_PORTIA_ERROR_BASE + 3)  // 20003
#define NIMCP_PORTIA_ERROR_POWER_CRITICAL       (NIMCP_PORTIA_ERROR_BASE + 4)  // 20004
```

### Module-Specific Negative Codes (-1 to -99)
Small, focused modules use negative codes for local error handling:

| Range | Module | Header |
|-------|--------|--------|
| **-1 to -11** | **Tensor Library** | `include/utils/tensor/nimcp_tensor.h` |
| **-1 to -15** | **LNN Module** | `include/lnn/nimcp_lnn_types.h` |
| **-40 to -54** | **Packet Validation** | `include/utils/validation/nimcp_common.h` |

**Example (Tensor):**
```c
typedef enum {
    NIMCP_TENSOR_OK = 0,
    NIMCP_TENSOR_ERR_NULL = -1,
    NIMCP_TENSOR_ERR_SHAPE = -2,
    NIMCP_TENSOR_ERR_RANK = -3,
    NIMCP_TENSOR_ERR_ALLOC = -4,
    NIMCP_TENSOR_ERR_BROADCAST = -5,
    NIMCP_TENSOR_ERR_EINSUM = -6,
    NIMCP_TENSOR_ERR_DTYPE = -7,
    NIMCP_TENSOR_ERR_CONTIGUOUS = -8,
    NIMCP_TENSOR_ERR_INDEX = -9,
    NIMCP_TENSOR_ERR_GRAD = -10,
    NIMCP_TENSOR_ERR_INVALID = -11
} nimcp_tensor_error_t;
```

## Design Rationale

### Why Stratified Codes?

1. **Quick Source Identification**: Error code range immediately indicates which subsystem failed
2. **Collision Avoidance**: Modules can define local codes without coordination
3. **Type Safety**: Module-specific enums catch type mismatches at compile time
4. **Clarity**: Code like `NIMCP_TENSOR_ERR_BROADCAST` is more descriptive than `-5`

### When to Use Each Range

| Use Case | Recommended Range | Reasoning |
|----------|-------------------|-----------|
| **Low-level utility** (tensor, LNN) | Negative (-1 to -99) | Fast, local error handling; no global collision risk |
| **High-level subsystem** (Portia, Swarm) | Positive (20000+) | Clear separation from core codes; module identity |
| **Core infrastructure** (memory, threading) | Core (1000-9999) | Centralized, cross-module concerns |

### Common Pitfalls

#### ❌ Wrong: Mixing core and module codes
```c
// DON'T: Returning core code from module function
int tensor_func() {
    return NIMCP_ERROR_INVALID_PARAMETER;  // Wrong namespace
}
```

#### ✅ Right: Use module-specific codes
```c
// DO: Return module-specific error
int tensor_func() {
    return NIMCP_TENSOR_ERR_SHAPE;  // Clear module context
}
```

#### ❌ Wrong: Magic number errors
```c
// DON'T: Return raw -1 without enum
int some_func() {
    return -1;  // Unclear what failed
}
```

#### ✅ Right: Named error codes
```c
// DO: Use descriptive enum
int some_func() {
    return NIMCP_TENSOR_ERR_NULL;  // Clear error cause
}
```

## Brain Region Error Conversion

Use the provided conversion functions to translate between module-local error codes and unified NIMCP error codes:

```c
#include "utils/error/nimcp_error_codes.h"

// Convert motor_error_t to nimcp_error_t
motor_error_t local_err = MOTOR_ERROR_INVALID_INPUT;
nimcp_error_t unified = motor_error_to_nimcp(local_err);
// unified == NIMCP_ERROR_MOTOR_INVALID_INPUT (10001)

// Convert back
int motor_err = nimcp_to_motor_error(unified);
// motor_err == 1 (MOTOR_ERROR_INVALID_INPUT)

// FEP bridge compatibility (0/-1 convention)
nimcp_error_t from_fep = nimcp_from_fep_result(fep_bridge_call());
int fep_result = nimcp_to_fep_result(NIMCP_SUCCESS);  // Returns 0

// Check if error is from a brain region
if (nimcp_error_is_brain_region(err)) {
    const char* region = nimcp_error_get_brain_region_name(err);
    // region == "Motor Cortex", "Hippocampus", etc.
}
```

### Adding a New Brain Region Module

1. Reserve a 100-code range in `nimcp_error_codes.h`:
```c
/* New Region Errors (12000-12099) */
#define NIMCP_ERROR_NEWREGION_BASE        12000
#define NIMCP_ERROR_NEWREGION_NONE        (NIMCP_ERROR_NEWREGION_BASE + 0)
#define NIMCP_ERROR_NEWREGION_INVALID     (NIMCP_ERROR_NEWREGION_BASE + 1)
// ... add specific errors
```

2. Generate conversion functions using the macro:
```c
NIMCP_DEFINE_ERROR_CONVERTER(newregion, NEWREGION, 4)  // 4 = internal error offset
```

3. Add error strings in `nimcp_error_codes.c`:
```c
case NIMCP_ERROR_NEWREGION_INVALID: return "NewRegion: Invalid input";
```

4. Update `nimcp_error_get_brain_region_name()` with the new region.

## Error Handling Best Practices

### 1. Check Module Boundary
Functions at module boundaries should translate errors:

```c
// Internal: Uses module codes
static nimcp_tensor_error_t internal_tensor_op() {
    return NIMCP_TENSOR_ERR_BROADCAST;
}

// Public API: Can map to core codes if needed
int nimcp_public_api() {
    nimcp_tensor_error_t err = internal_tensor_op();
    if (err != NIMCP_TENSOR_OK) {
        // Optionally map to core code for callers
        NIMCP_LOGGING_ERROR("Tensor broadcast failed");
        return NIMCP_ERROR_INVALID_PARAMETER;
    }
    return NIMCP_SUCCESS;
}
```

### 2. Preserve Error Context
When propagating errors up the stack, preserve specificity:

```c
// Good: Preserve module error
nimcp_tensor_error_t err = nimcp_tensor_matmul(...);
if (err != NIMCP_TENSOR_OK) {
    NIMCP_LOGGING_ERROR("Matrix multiply failed: %d", err);
    return err;  // Caller gets specific error
}
```

### 3. Document Error Return Values
Always document which error codes a function can return:

```c
/**
 * @brief Create tensor
 * @return Tensor handle or NULL on error
 *
 * Error conditions (check logs for details):
 * - NULL: Allocation failed (NIMCP_TENSOR_ERR_ALLOC)
 * - NULL: Invalid rank (NIMCP_TENSOR_ERR_RANK)
 */
nimcp_tensor_t* nimcp_tensor_create(...);
```

## Quick Reference

### Core Codes (Most Common)
```c
NIMCP_SUCCESS                  0     // Success
NIMCP_ERROR_INVALID_PARAMETER  1002  // Invalid argument
NIMCP_ERROR_NULL_POINTER       1003  // Unexpected NULL
NIMCP_ERROR_INVALID_STATE      1005  // Object state error
NIMCP_ERROR_OPERATION_FAILED   1006  // Generic failure
NIMCP_ERROR_NO_MEMORY          2000  // Allocation failed
```

### Tensor Module
```c
NIMCP_TENSOR_OK                0     // Success
NIMCP_TENSOR_ERR_NULL          -1    // NULL pointer
NIMCP_TENSOR_ERR_SHAPE         -2    // Shape mismatch
NIMCP_TENSOR_ERR_ALLOC         -4    // Allocation failed
```

### LNN Module
```c
LNN_ERROR_NONE                 0     // Success
LNN_ERROR_NULL_POINTER         -1    // NULL argument
LNN_ERROR_INVALID_PARAM        -13   // Invalid parameter
LNN_ERROR_OPERATION_FAILED     -12   // Operation failed
LNN_ERROR_OUT_OF_MEMORY        -3    // Allocation failed
```

### Portia Module
```c
NIMCP_SUCCESS                  0     // Success
NIMCP_PORTIA_ERROR_NOT_INITIALIZED      20001  // Not initialized
NIMCP_PORTIA_ERROR_TIER_LOCKED          20003  // Tier change blocked
NIMCP_PORTIA_ERROR_POWER_CRITICAL       20004  // Power emergency
```

## Migration Guide

### If You Find Inconsistent Error Codes

1. **Identify the module**: Is this core infrastructure or a subsystem?
2. **Check existing range**: Look for module's error code header
3. **Add to appropriate range**:
   - Core → `nimcp_error_codes.h`
   - Module-specific → Module's header file
4. **Update this document**: Add to the table above

### Example: Adding New Swarm Error

```c
// In include/swarm/nimcp_swarm_errors.h
#define NIMCP_SWARM_ERROR_BASE 21000
#define NIMCP_SWARM_ERROR_CONSENSUS_FAILED  (NIMCP_SWARM_ERROR_BASE + 1)  // 21001
#define NIMCP_SWARM_ERROR_NODE_UNREACHABLE  (NIMCP_SWARM_ERROR_BASE + 2)  // 21002
```

## Error Code Lookup Tool

For quick error code identification:

```bash
# Find error code definition
grep -r "1002" include/utils/error/
# Output: #define NIMCP_ERROR_INVALID_PARAMETER 1002

# Find all Portia errors
grep "NIMCP_PORTIA_ERROR" include/portia/nimcp_portia.h

# Find module-specific ranges
grep -E "ERROR_BASE|ERR_NULL" include/
```

---

**Last Updated**: 2025-12-24
**Maintainer**: NIMCP Development Team
