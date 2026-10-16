# Cognitive Memory Modules - Quick Reference Guide

## Module IDs (0x0330 - 0x0337)

```c
#define BIO_MODULE_MEMORY                   0x0330  // Engram
#define BIO_MODULE_SEMANTIC_MEMORY          0x0331  // Semantic Memory
#define BIO_MODULE_SYSTEMS_CONSOLIDATION    0x0332  // Systems Consolidation
#define BIO_MODULE_WM_TRANSFER              0x0333  // WM Transfer
#define BIO_MODULE_WORKING_MEMORY           0x0334  // Working Memory
#define BIO_MODULE_AUTOBIOGRAPHICAL_MEMORY  0x0335  // Autobiographical Memory
#define BIO_MODULE_META_LEARNING            0x0336  // Meta-Learning
#define BIO_MODULE_PREDICTIVE               0x0337  // Predictive Processing
```

## Standard Header Pattern

```c
/**
 * @file nimcp_module.c
 * @brief Module description
 *
 * BIO-ASYNC INTEGRATION:
 * - Module ID: 0x033X (BIO_MODULE_NAME)
 * - Publishes: event types
 * - Subscribes: event sources
 */

#define LOG_MODULE "module_name"

#include "cognitive/.../nimcp_module.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// BIO-ASYNC MODULE REGISTRATION
//=============================================================================

#define BIO_MODULE_NAME 0x033X
```

## Logging Patterns

### Function Entry
```c
LOG_INFO("Creating module: param=%u", param);
```

### Error Handling
```c
if (!ptr) {
    LOG_ERROR("Failed to allocate structure (%zu bytes)", sizeof(struct_t));
    return NULL;
}
```

### State Changes
```c
LOG_DEBUG("State transition: %s -> %s", old_state, new_state);
```

### Success Confirmation
```c
LOG_INFO("Operation complete: processed=%u, success=%u", total, success);
```

## Memory Allocation

### Allocation
```c
ptr = nimcp_malloc(size);
ptr = nimcp_calloc(count, size);
ptr = nimcp_realloc(old_ptr, new_size);
```

### Deallocation
```c
nimcp_free(ptr);
```

### Pattern
```c
void* ptr = nimcp_malloc(SIZE);
if (!ptr) {
    LOG_ERROR("Allocation failed (%zu bytes)", SIZE);
    return NULL;
}
LOG_DEBUG("Allocated %zu bytes at %p", SIZE, ptr);
```

## Event Publishing (Future)

```c
// Prepare event
bio_message_t msg = {
    .source_module = BIO_MODULE_NAME,
    .target_module = BIO_MODULE_TARGET,
    .type = EVENT_TYPE,
    .priority = BIO_PRIORITY_NORMAL
};

// Publish
bio_router_publish(router, &msg);
```

## File Locations

| Module | Source File |
|--------|-------------|
| Engram | `src/cognitive/memory/nimcp_engram.c` |
| Semantic Memory | `src/cognitive/memory/nimcp_semantic_memory.c` |
| Systems Consolidation | `src/cognitive/memory/nimcp_systems_consolidation.c` |
| WM Transfer | `src/cognitive/memory/nimcp_wm_transfer.c` |
| Working Memory | `src/cognitive/working_memory/nimcp_working_memory.c` |
| Autobiographical | `src/cognitive/autobiographical_memory/nimcp_autobiographical_memory.c` |
| Meta-Learning | `src/cognitive/meta_learning/nimcp_meta_learning.c` |
| Predictive | `src/cognitive/predictive/nimcp_predictive.c` |

## Verification

```bash
# Run verification script
./verify_cognitive_integration.sh

# Check specific module
grep -A5 "BIO_MODULE_" src/cognitive/path/to/module.c
```

## Build & Test

```bash
# Build
cd build
cmake ..
make

# Test cognitive modules
ctest -R cognitive -V

# Test specific module
ctest -R engram -V
```

## Common Issues

### Missing Include
**Problem:** Undefined reference to `nimcp_malloc`
**Solution:** Add `#include "utils/memory/nimcp_unified_memory.h"`

### Missing Module ID
**Problem:** Module ID not defined
**Solution:** Add `#define BIO_MODULE_NAME 0x033X` after includes

### Missing LOG_MODULE
**Problem:** Logging functions fail
**Solution:** Add `#define LOG_MODULE "name"` before includes

### Wrong Memory Function
**Problem:** Using `malloc` instead of `nimcp_malloc`
**Solution:** Replace all `malloc/calloc/realloc/free` with `nimcp_*` versions

## Module Communication Flow

```
Working Memory (0x0334)
    |
    | [item updates]
    v
WM Transfer (0x0333)
    |
    | [transfer triggers]
    v
Engram (0x0330) <---> Semantic Memory (0x0331)
    |                       ^
    | [engram updates]      | [concept activation]
    v                       |
Systems Consolidation (0x0332)
    |
    | [consolidation]
    v
Autobiographical Memory (0x0335)
```

## Cheat Sheet

| Task | Command |
|------|---------|
| Add bio-async | `#include "async/nimcp_bio_async.h"` |
| Add logging | `#include "utils/logging/nimcp_logging.h"` |
| Add unified mem | `#include "utils/memory/nimcp_unified_memory.h"` |
| Define module | `#define LOG_MODULE "name"` |
| Define ID | `#define BIO_MODULE_NAME 0x033X` |
| Allocate | `ptr = nimcp_malloc(size)` |
| Free | `nimcp_free(ptr)` |
| Log info | `LOG_INFO("msg %d", val)` |
| Log error | `LOG_ERROR("failed: %s", err)` |
| Verify | `./verify_cognitive_integration.sh` |

---

**Quick Ref Version:** 1.0
**Last Updated:** 2025-11-28
