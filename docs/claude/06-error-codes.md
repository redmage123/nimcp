# Error Code Strategy

NIMCP uses a **stratified error code system** with module-specific ranges. See `/home/bbrelin/nimcp/docs/ERROR_CODE_STRATEGY.md` for full documentation.

**Primary Header**: `include/utils/error/nimcp_error_codes.h`

## Unified Error Handling API

The error handling system provides:
1. **Thread-local error context** - Detailed error info with formatted messages
2. **Cleanup stack pattern** - Guaranteed resource cleanup on partial failures
3. **FEP bridge compatibility** - Conversion between 0/-1 and error codes
4. **Result type pattern** - Optional type-safe error returns

### Thread-Local Error Context

```c
#include "utils/error/nimcp_error_codes.h"

// Set error with formatted message
nimcp_set_error(NIMCP_ERROR_INVALID_PARAMETER, "Expected size > 0, got %zu", size);

// Set error with source location (preferred - use macro)
NIMCP_ERROR_SET(NIMCP_ERROR_NULL_POINTER, "Parameter '%s' is NULL", "brain");

// Get last error
nimcp_error_t code = nimcp_get_last_error();
const char* msg = nimcp_get_error_message();
const nimcp_error_context_t* ctx = nimcp_get_error_context();

// Check and return pattern
NIMCP_ERROR_CHECK(ptr != NULL, NIMCP_ERROR_NULL_POINTER, "ptr is NULL");
NIMCP_ERROR_CHECK(size > 0, NIMCP_ERROR_INVALID_PARAMETER, "size=%zu must be > 0", size);
```

### Cleanup Stack Pattern

For complex initialization with multiple resources:

```c
nimcp_cleanup_stack_t cleanup = {0};

void* res1 = allocate_resource1();
if (!res1) goto cleanup_and_exit;
nimcp_cleanup_push(&cleanup, free, res1, "resource1");

void* res2 = allocate_resource2();
if (!res2) goto cleanup_and_exit;
nimcp_cleanup_push(&cleanup, free, res2, "resource2");

// Success - clear stack to prevent cleanup
nimcp_cleanup_clear(&cleanup);
return success_result;

cleanup_and_exit:
    nimcp_cleanup_execute(&cleanup);  // Executes in reverse order (LIFO)
    return error_result;
```

### FEP Bridge Compatibility

FEP bridges use 0 for success, -1 for error. Use these converters:

```c
// Convert FEP result to nimcp_error_t
nimcp_error_t err = nimcp_from_fep_result(fep_bridge_call());

// Convert nimcp_error_t to FEP result
int fep_result = nimcp_to_fep_result(NIMCP_SUCCESS);  // Returns 0
int fep_error = nimcp_to_fep_result(NIMCP_ERROR_OPERATION_FAILED);  // Returns -1
```

### Result Type Pattern (Optional)

For functions that return values with possible errors:

```c
// Define result type
NIMCP_DEFINE_RESULT(float, FloatResult);

// Function returning result
nimcp_FloatResult_t compute_value(int input) {
    if (input < 0) {
        return NIMCP_RESULT_ERR(FloatResult, NIMCP_ERROR_INVALID_PARAMETER);
    }
    return NIMCP_RESULT_OK(FloatResult, (float)input * 2.0f);
}

// Usage
nimcp_FloatResult_t result = compute_value(5);
if (result.is_ok) {
    printf("Value: %f\n", result.value);
} else {
    printf("Error: %s\n", nimcp_error_string(result.error));
}
```

## Error Code Ranges

| Range | Purpose | Examples |
|-------|---------|----------|
| **-1 to -99** | **Module-local** | Tensor (-1 to -11), LNN (-1 to -15), Packet validation (-40 to -54) |
| **1000-9999** | **Core NIMCP** | INVALID_PARAMETER (1002), NO_MEMORY (2000), BRAIN_CREATION (3000) |
| **10000-19999** | **Brain Regions** | Motor (10000+), Hippocampus (10100+), Entorhinal (10200+) |
| **20000+** | **Module-specific** | Portia (20000+), Swarm (21000+), Security (22000+) |

## When to Use Each Range

| Use Case | Recommended Range | Reasoning |
|----------|-------------------|-----------|
| **Low-level utility** (tensor, LNN) | Negative (-1 to -99) | Fast, local error handling; no global collision risk |
| **High-level subsystem** (Portia, Swarm) | Positive (20000+) | Clear separation from core codes; module identity |
| **Core infrastructure** (memory, threading) | Core (1000-9999) | Centralized, cross-module concerns |

## Common Error Codes

```c
// Core codes (most common)
NIMCP_SUCCESS                  0     // Success
NIMCP_ERROR_INVALID_PARAMETER  1002  // Invalid argument
NIMCP_ERROR_NULL_POINTER       1003  // Unexpected NULL
NIMCP_ERROR_INVALID_STATE      1005  // Object state error
NIMCP_ERROR_NO_MEMORY          2000  // Allocation failed

// Tensor module
NIMCP_TENSOR_OK                0     // Success
NIMCP_TENSOR_ERR_NULL          -1    // NULL pointer
NIMCP_TENSOR_ERR_SHAPE         -2    // Shape mismatch
NIMCP_TENSOR_ERR_ALLOC         -4    // Allocation failed

// Brain region modules (10000-19999)
NIMCP_ERROR_MOTOR_BASE         10000 // Motor cortex base
NIMCP_ERROR_MOTOR_INVALID_INPUT 10001 // Motor: Invalid input
NIMCP_ERROR_HIPPOCAMPUS_BASE   10100 // Hippocampus base
NIMCP_ERROR_ENTORHINAL_BASE    10200 // Entorhinal cortex base

// Portia module
NIMCP_PORTIA_ERROR_NOT_INITIALIZED      20001  // Not initialized
NIMCP_PORTIA_ERROR_TIER_LOCKED          20003  // Tier change blocked

// LNN module
LNN_ERROR_NONE = 0
LNN_ERROR_NULL_POINTER = -1
LNN_ERROR_INVALID_PARAM = -13
LNN_ERROR_OPERATION_FAILED = -12
LNN_ERROR_OUT_OF_MEMORY = -3
```

## Brain Region Error Conversion

Convert between module-local and unified error codes at module boundaries:

```c
#include "utils/error/nimcp_error_codes.h"

// Convert module error to unified NIMCP error
motor_error_t local_err = motor_get_last_error(adapter);
nimcp_error_t unified = motor_error_to_nimcp(local_err);

// Convert back to module error
int motor_err = nimcp_to_motor_error(unified);

// FEP bridge compatibility (0/-1 convention)
nimcp_error_t err = nimcp_from_fep_result(fep_call());  // -1 -> NIMCP_ERROR_OPERATION_FAILED
int fep_result = nimcp_to_fep_result(NIMCP_SUCCESS);   // Returns 0

// Get brain region name from error code
if (nimcp_error_is_brain_region(err)) {
    printf("Error in %s\n", nimcp_error_get_brain_region_name(err));
}
```

## Tensor Memory Safety

The tensor library provides strong memory safety guarantees. See `/home/bbrelin/nimcp/docs/TENSOR_MEMORY_SAFETY_FIXES.md` for details.

**Key guarantees**:
- `nimcp_tensor_destroy()` is **idempotent** (safe to call multiple times)
- `nimcp_tensor_destroy(NULL)` is a **no-op** (NULL-safe)
- Partial cleanup is safe (if creation fails partway)
- No double-free (NULL guards after freeing)
- Refcounting protects shared tensors

**Example**:
```c
nimcp_tensor_t* t = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);
if (!t) {
    // Creation failed - check logs for NIMCP_TENSOR_ERR_RANK or NIMCP_TENSOR_ERR_ALLOC
    return;
}
// ... use tensor ...
nimcp_tensor_destroy(t);  // Always safe, even if creation was partial
```
