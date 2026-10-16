# NIMCP API Reference

**CRITICAL: Read this document before writing ANY new code.**

This document contains the canonical API signatures for NIMCP. Using incorrect APIs
causes compilation failures and wastes development time.

---

## Table of Contents
1. [Memory Management](#memory-management)
2. [Logging](#logging)
3. [Bio-Async System](#bio-async-system)
4. [Threading](#threading)
5. [Platform/Time](#platformtime)
6. [Common Mistakes](#common-mistakes)

---

## Memory Management

**Header:** `#include "utils/memory/nimcp_memory.h"`

### Core Functions (USE THESE)
```c
void* nimcp_malloc(size_t size);
void* nimcp_calloc(size_t count, size_t size);
void* nimcp_realloc(void* ptr, size_t new_size);
void  nimcp_free(void* ptr);
char* nimcp_strdup(const char* str);
```

### Aligned Memory
```c
void* nimcp_aligned_alloc(size_t alignment, size_t size);
void* nimcp_aligned_malloc(size_t size, size_t alignment);
void  nimcp_aligned_free(void* ptr);
```

### Lifecycle
```c
void nimcp_memory_init(void);
void nimcp_memory_cleanup(void);
```

### WRONG - DO NOT USE
```c
// WRONG: nimcp_unified_calloc()    - Does not exist
// WRONG: nimcp_unified_free()      - Does not exist
// WRONG: nimcp_unified_malloc()    - Does not exist
```

---

## Logging

**Header:** `#include "utils/logging/nimcp_logging.h"`

### Simple Logging Macros (USE THESE)
```c
LOG_TRACE(format, ...);
LOG_DEBUG(format, ...);
LOG_INFO(format, ...);
LOG_WARN(format, ...);
LOG_WARNING(format, ...);  // Alias for LOG_WARN
LOG_ERROR(format, ...);
LOG_FATAL(format, ...);
```

### Module-Tagged Logging (PREFERRED FOR MODULES)
```c
LOG_MODULE_TRACE(module, format, ...);
LOG_MODULE_DEBUG(module, format, ...);
LOG_MODULE_INFO(module, format, ...);
LOG_MODULE_WARN(module, format, ...);
LOG_MODULE_ERROR(module, format, ...);
LOG_MODULE_FATAL(module, format, ...);
```

Example:
```c
#define MODULE_NAME "my_module"
LOG_MODULE_DEBUG(MODULE_NAME, "Processing %d items", count);
```

### Legacy Compatibility (ACCEPTABLE)
```c
NIMCP_LOGGING_TRACE(format, ...);
NIMCP_LOGGING_DEBUG(format, ...);
NIMCP_LOGGING_INFO(format, ...);
NIMCP_LOGGING_WARN(format, ...);
NIMCP_LOGGING_ERROR(format, ...);
NIMCP_LOGGING_FATAL(format, ...);
```

### WRONG - DO NOT USE
```c
// WRONG: NIMCP_LOG_DEBUG()      - Does not exist
// WRONG: NIMCP_LOG_ERROR()      - Does not exist
// WRONG: NIMCP_LOG_INFO()       - Does not exist
// WRONG: nimcp_log_debug()      - Use macros instead
```

---

## Bio-Async System

**Headers:**
```c
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
```

### Message Handler Signature (CRITICAL)
```c
typedef nimcp_error_t (*bio_message_handler_t)(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
);
```

### Router Functions
```c
int bio_router_init(void);
int bio_router_register_module(const char* module_name, bio_message_handler_t handler, void* user_data);
int bio_router_register_handler(bio_message_type_t msg_type, bio_message_handler_t handler, void* user_data);
int bio_router_send(const char* target_module, bio_message_type_t msg_type, const void* data, size_t size);
int bio_router_broadcast(bio_message_type_t msg_type, const void* data, size_t size);
int bio_router_process_inbox(void);
```

### Message Type Constants (USE THESE)
```c
BIO_MSG_SPIKE_FIRED
BIO_MSG_WEIGHT_UPDATE
BIO_MSG_NEUROMODULATOR_RELEASE
BIO_MSG_ATTENTION_SHIFT
BIO_MSG_CURIOSITY_SIGNAL
BIO_MSG_PLASTICITY_UPDATE
BIO_MSG_PREDICTION_ERROR
```

### Message Header Type
```c
typedef struct {
    bio_message_type_t type;
    uint32_t sender_id;
    uint32_t target_id;
    uint64_t timestamp;
    uint32_t payload_size;
} bio_message_header_t;
```

### WRONG - DO NOT USE
```c
// WRONG: nimcp_bio_message_t           - Use bio_message_header_t
// WRONG: NIMCP_BIO_MSG_SPIKE_FIRED     - Use BIO_MSG_SPIKE_FIRED
// WRONG: NIMCP_BIO_MSG_ATTENTION_SHIFT - Use BIO_MSG_ATTENTION_SHIFT
// WRONG: nimcp_bio_async_inbox_t*      - Use void* for inbox
```

---

## Threading

**Header:** `#include "utils/thread/nimcp_thread.h"`

### Mutex Operations
```c
nimcp_result_t nimcp_mutex_init(nimcp_mutex_t* mutex, const mutex_attr_t* attr);
nimcp_result_t nimcp_mutex_destroy(nimcp_mutex_t* mutex);
nimcp_result_t nimcp_mutex_lock(nimcp_mutex_t* mutex);
nimcp_result_t nimcp_mutex_trylock(nimcp_mutex_t* mutex);
nimcp_result_t nimcp_mutex_unlock(nimcp_mutex_t* mutex);
```

### Thread Management
```c
nimcp_result_t nimcp_thread_init(void);
nimcp_result_t nimcp_thread_create(nimcp_thread_t* thread,
                                    void* (*start_routine)(void*),
                                    void* arg,
                                    const thread_attr_t* attr);
nimcp_result_t nimcp_thread_join(nimcp_thread_t thread, void** retval);
nimcp_result_t nimcp_thread_detach(nimcp_thread_t thread);
void nimcp_thread_exit(void* retval);
nimcp_thread_t nimcp_thread_self(void);
```

### Read-Write Locks
```c
nimcp_result_t nimcp_rwlock_init(nimcp_rwlock_t* lock);
nimcp_result_t nimcp_rwlock_destroy(nimcp_rwlock_t* lock);
nimcp_result_t nimcp_rwlock_rdlock(nimcp_rwlock_t* lock);
nimcp_result_t nimcp_rwlock_wrlock(nimcp_rwlock_t* lock);
nimcp_result_t nimcp_rwlock_unlock(nimcp_rwlock_t* lock);
```

### Condition Variables
```c
nimcp_result_t nimcp_cond_init(nimcp_cond_t* cond);
nimcp_result_t nimcp_cond_destroy(nimcp_cond_t* cond);
nimcp_result_t nimcp_cond_wait(nimcp_cond_t* cond, nimcp_mutex_t* mutex);
nimcp_result_t nimcp_cond_timedwait(nimcp_cond_t* cond, nimcp_mutex_t* mutex, uint32_t timeout_ms);
nimcp_result_t nimcp_cond_signal(nimcp_cond_t* cond);
nimcp_result_t nimcp_cond_broadcast(nimcp_cond_t* cond);
```

---

## Platform/Time

**Headers:**
```c
#include "utils/platform/nimcp_platform.h"
#include "utils/platform/nimcp_platform_time.h"
```

### Time Functions (USE THESE)
```c
uint64_t nimcp_platform_time_monotonic_ms(void);  // Milliseconds
uint64_t nimcp_platform_time_monotonic_us(void);  // Microseconds
void nimcp_platform_sleep_ms(uint32_t ms);
int nimcp_platform_time_init(void);
```

### WRONG - DO NOT USE
```c
// WRONG: nimcp_platform_time_ms()    - Use nimcp_platform_time_monotonic_ms()
// WRONG: nimcp_time_ms()             - Use nimcp_platform_time_monotonic_ms()
// WRONG: nimcp_get_time_ms()         - Use nimcp_platform_time_monotonic_ms()
```

---

## Common Mistakes

### 1. Memory API
| Wrong | Correct |
|-------|---------|
| `nimcp_unified_calloc(n, size)` | `nimcp_calloc(n, size)` |
| `nimcp_unified_free(ptr)` | `nimcp_free(ptr)` |
| `nimcp_unified_malloc(size)` | `nimcp_malloc(size)` |

### 2. Logging API
| Wrong | Correct |
|-------|---------|
| `NIMCP_LOG_DEBUG(...)` | `LOG_DEBUG(...)` or `LOG_MODULE_DEBUG(mod, ...)` |
| `NIMCP_LOG_ERROR(...)` | `LOG_ERROR(...)` or `LOG_MODULE_ERROR(mod, ...)` |
| `NIMCP_LOG_INFO(...)` | `LOG_INFO(...)` or `LOG_MODULE_INFO(mod, ...)` |

### 3. Bio-Async API
| Wrong | Correct |
|-------|---------|
| `nimcp_bio_message_t` | `bio_message_header_t` |
| `NIMCP_BIO_MSG_*` | `BIO_MSG_*` |
| `nimcp_bio_async_inbox_t*` | `void*` |

### 4. Time API
| Wrong | Correct |
|-------|---------|
| `nimcp_platform_time_ms()` | `nimcp_platform_time_monotonic_ms()` |
| `nimcp_time_ms()` | `nimcp_platform_time_monotonic_ms()` |

### 5. Include Paths
| Wrong | Correct |
|-------|---------|
| `nimcp_broca.h` | `nimcp_broca_adapter.h` |
| `nimcp_unified_memory.h` | `nimcp_memory.h` |

---

## Required Includes by Feature

### Memory Management
```c
#include "utils/memory/nimcp_memory.h"
```

### Logging
```c
#include "utils/logging/nimcp_logging.h"
```

### Bio-Async
```c
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
```

### Threading
```c
#include "utils/thread/nimcp_thread.h"
```

### Time
```c
#include "utils/platform/nimcp_platform_time.h"
```

### Security/BBB
```c
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_bbb_helpers.h"
```

---

## Pre-Code Checklist

Before writing new code:

1. [ ] Read the relevant header file first
2. [ ] Check this API reference for correct function names
3. [ ] Verify include paths are correct
4. [ ] Run `make nimcp` after creating each file to catch errors early
5. [ ] Search for existing usage: `grep -r "function_name" src/`

---

*Last updated: 2025-12-08*
