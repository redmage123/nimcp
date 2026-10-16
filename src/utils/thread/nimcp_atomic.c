/**
 * @file nimcp_atomic.c
 * @brief Implementation of portable atomic operations wrapper
 *
 * WHAT: Non-inline helpers and utility functions for atomic operations
 * WHY:  Provide any non-inline implementations needed for complex operations
 * HOW:  Currently most operations are inline in header, this file provides
 *       documentation and placeholder for future non-inline helpers
 *
 * NOTE: All atomic operations are implemented as inline functions in
 *       nimcp_atomic.h for maximum performance. This file exists for:
 *       1. Consistency with NIMCP project structure
 *       2. Future non-inline utility functions
 *       3. Platform-specific initialization if needed
 */

#include "utils/thread/nimcp_atomic.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(atomic)

//=============================================================================
// Documentation and Usage Examples
//=============================================================================

/**
 * EXAMPLE 1: Lock-free counter
 * ============================
 *
 * nimcp_atomic_int32_t counter;
 * nimcp_atomic_init_i32(&counter, 0);
 *
 * // Multiple threads can safely increment
 * nimcp_atomic_fetch_add_i32(&counter, 1, NIMCP_MEMORY_ORDER_SEQ_CST);
 *
 * // Or use convenience macro
 * NIMCP_ATOMIC_INC_I32(&counter);
 *
 * // Read final value
 * int32_t total = nimcp_atomic_load_i32(&counter, NIMCP_MEMORY_ORDER_ACQUIRE);
 */

/**
 * EXAMPLE 2: Lock-free flag (test-and-set)
 * =========================================
 *
 * nimcp_atomic_int32_t flag;
 * nimcp_atomic_init_i32(&flag, 0);
 *
 * // Try to acquire flag (returns old value)
 * int32_t old = nimcp_atomic_exchange_i32(&flag, 1, NIMCP_MEMORY_ORDER_ACQUIRE);
 * if (old == 0) {
 *     // We got the flag (it was 0, now it's 1)
 *     do_critical_work();
 *     nimcp_atomic_store_i32(&flag, 0, NIMCP_MEMORY_ORDER_RELEASE);
 * }
 */

/**
 * EXAMPLE 3: Compare-and-swap loop (lock-free stack push)
 * ========================================================
 *
 * typedef struct node {
 *     void* data;
 *     struct node* next;
 * } node_t;
 *
 * nimcp_atomic_ptr_t stack_top;
 * nimcp_atomic_init_ptr(&stack_top, NULL);
 *
 * void push(node_t* new_node) {
 *     void* old_top;
 *     do {
 *         old_top = nimcp_atomic_load_ptr(&stack_top, NIMCP_MEMORY_ORDER_RELAXED);
 *         new_node->next = (node_t*)old_top;
 *     } while (!nimcp_atomic_compare_exchange_ptr(&stack_top, &old_top, new_node,
 *                                                   NIMCP_MEMORY_ORDER_RELEASE));
 * }
 *
 * node_t* pop(void) {
 *     void* old_top;
 *     node_t* next;
 *     do {
 *         old_top = nimcp_atomic_load_ptr(&stack_top, NIMCP_MEMORY_ORDER_ACQUIRE);
 *         if (old_top == NULL) return NULL;
 *         next = ((node_t*)old_top)->next;
 *     } while (!nimcp_atomic_compare_exchange_ptr(&stack_top, &old_top, next,
 *                                                   NIMCP_MEMORY_ORDER_RELEASE));
 *     return (node_t*)old_top;
 * }
 */

/**
 * EXAMPLE 4: Memory ordering demonstration
 * =========================================
 *
 * // Thread 1: Producer
 * data = compute_result();  // Non-atomic write
 * nimcp_atomic_store_i32(&ready_flag, 1, NIMCP_MEMORY_ORDER_RELEASE);
 * // RELEASE ensures all prior writes are visible before flag is set
 *
 * // Thread 2: Consumer
 * while (nimcp_atomic_load_i32(&ready_flag, NIMCP_MEMORY_ORDER_ACQUIRE) == 0) {
 *     // Wait
 * }
 * // ACQUIRE ensures all writes prior to flag=1 are now visible
 * use_data(data);  // Safe to read data
 */

/**
 * EXAMPLE 5: Bit manipulation (lock-free flags)
 * ==============================================
 *
 * nimcp_atomic_uint32_t flags;
 * nimcp_atomic_init_u32(&flags, 0);
 *
 * #define FLAG_ACTIVE  0
 * #define FLAG_READY   1
 * #define FLAG_ERROR   2
 *
 * // Set flags
 * NIMCP_ATOMIC_SET_BIT_U32(&flags, FLAG_ACTIVE);
 * NIMCP_ATOMIC_SET_BIT_U32(&flags, FLAG_READY);
 *
 * // Test flag
 * if (NIMCP_ATOMIC_TEST_BIT_U32(&flags, FLAG_READY)) {
 *     process();
 * }
 *
 * // Clear flag
 * NIMCP_ATOMIC_CLEAR_BIT_U32(&flags, FLAG_ACTIVE);
 */

//=============================================================================
// Memory Ordering Guide
//=============================================================================

/**
 * RELAXED: No ordering constraints, only atomicity
 * ================================================
 * - Fastest, no memory barriers
 * - Only guarantees: single variable is atomic
 * - Use for: Simple counters where order doesn't matter
 *
 * Example: Statistics counters
 *   nimcp_atomic_fetch_add_u64(&total_requests, 1, NIMCP_MEMORY_ORDER_RELAXED);
 */

/**
 * ACQUIRE: For loads (reading shared data)
 * =========================================
 * - Prevents reordering of subsequent operations before this load
 * - All writes that happened-before a RELEASE store are now visible
 * - Use for: Reading data published by another thread
 *
 * Example: Consuming published data
 *   if (nimcp_atomic_load_i32(&ready, NIMCP_MEMORY_ORDER_ACQUIRE)) {
 *       use_shared_data();  // Safe, all prior writes are visible
 *   }
 */

/**
 * RELEASE: For stores (publishing shared data)
 * =============================================
 * - Prevents reordering of prior operations after this store
 * - Makes all prior writes visible to threads that ACQUIRE this value
 * - Use for: Publishing data to other threads
 *
 * Example: Publishing data
 *   prepare_shared_data();
 *   nimcp_atomic_store_i32(&ready, 1, NIMCP_MEMORY_ORDER_RELEASE);
 *   // All writes above are now visible to ACQUIRE readers
 */

/**
 * ACQ_REL: Both acquire and release
 * ==================================
 * - For read-modify-write operations
 * - Acts as ACQUIRE for the read, RELEASE for the write
 * - Use for: Atomically updating shared state
 *
 * Example: Lock-free queue operations
 *   nimcp_atomic_fetch_add_i32(&queue_size, 1, NIMCP_MEMORY_ORDER_ACQ_REL);
 */

/**
 * SEQ_CST: Sequentially consistent (default, strongest)
 * ======================================================
 * - Total global ordering of all SEQ_CST operations
 * - Most expensive (full memory barriers on some architectures)
 * - Use when: Unsure about ordering, or need strict global order
 *
 * Example: Default for safety
 *   NIMCP_ATOMIC_INC_I32(&counter);  // Uses SEQ_CST
 */

//=============================================================================
// Performance Notes
//=============================================================================

/**
 * LOCK-FREE GUARANTEES
 * ====================
 * All atomic types and operations in this module are lock-free on:
 * - x86/x86-64: All sizes (8, 16, 32, 64-bit)
 * - ARM64/AArch64: All sizes with proper alignment
 * - ARMv7: 32-bit always, 64-bit depends on CPU (LPAE)
 * - RISC-V: All sizes on RV64, check for RV32
 *
 * To verify at compile time, use C11 atomic_is_lock_free():
 *   #ifdef NIMCP_HAVE_C11_ATOMICS
 *   _Static_assert(atomic_is_lock_free(&((nimcp_atomic_int64_t){0}).value),
 *                  "64-bit atomics are not lock-free!");
 *   #endif
 */

/**
 * PERFORMANCE GUIDELINES
 * ======================
 * 1. Use RELAXED when possible (e.g., statistics counters)
 * 2. Use ACQUIRE/RELEASE for producer-consumer patterns
 * 3. Only use SEQ_CST when you need total ordering
 * 4. Avoid mixing atomic and non-atomic access to same variable
 * 5. Keep atomic variables on separate cache lines to avoid false sharing
 *
 * FALSE SHARING EXAMPLE:
 *   // BAD: Two atomics on same cache line (64 bytes)
 *   struct {
 *       nimcp_atomic_int32_t counter1;  // Offset 0
 *       nimcp_atomic_int32_t counter2;  // Offset 4 (same cache line!)
 *   } bad;
 *
 *   // GOOD: Separate cache lines
 *   struct {
 *       nimcp_atomic_int32_t counter1;
 *       char padding1[60];  // Pad to 64 bytes
 *       nimcp_atomic_int32_t counter2;
 *       char padding2[60];
 *   } good;
 */

/**
 * ABA PROBLEM WARNING
 * ===================
 * Compare-exchange can suffer from ABA problem in lock-free data structures:
 *
 * Thread 1 reads A
 * Thread 2 changes A to B, then back to A
 * Thread 1's CAS succeeds (thinks nothing changed!)
 *
 * Solutions:
 * 1. Tagged pointers (store version counter in unused pointer bits)
 * 2. Hazard pointers (mark nodes as in-use)
 * 3. Use 128-bit CAS if available (__int128 on x86-64)
 *
 * Example tagged pointer:
 *   typedef union {
 *       void* ptr;
 *       struct {
 *           uintptr_t tag : 16;      // Version counter
 *           uintptr_t address : 48;  // Actual pointer
 *       };
 *   } tagged_ptr_t;
 */

//=============================================================================
// Future Extensions (Placeholders)
//=============================================================================

/**
 * Potential future additions:
 * - nimcp_atomic_is_lock_free() runtime check
 * - nimcp_atomic_wait/notify (C11 atomic wait/notify)
 * - 128-bit CAS support (__int128 on x86-64)
 * - ARM-specific optimizations (LDXR/STXR)
 * - RISC-V optimizations (LR/SC)
 */
