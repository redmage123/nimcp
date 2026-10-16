/**
 * @file nimcp_atomic.h
 * @brief Portable atomic operations wrapper for NIMCP threading utilities
 *
 * WHAT: Lock-free atomic operations with memory ordering control
 * WHY:  Provide portable, efficient atomic primitives across compilers/platforms
 * HOW:  C11 stdatomic.h where available, fallback to GCC builtins
 *
 * USAGE:
 *   nimcp_atomic_int32_t counter;
 *   nimcp_atomic_init_i32(&counter, 0);
 *   nimcp_atomic_fetch_add_i32(&counter, 1, NIMCP_MEMORY_ORDER_SEQ_CST);
 *   int32_t val = nimcp_atomic_load_i32(&counter, NIMCP_MEMORY_ORDER_ACQUIRE);
 *
 * PERFORMANCE:
 *   - Lock-free on all modern architectures (x86, ARM, RISC-V)
 *   - Inline functions minimize overhead
 *   - Memory ordering allows fine-grained control
 *
 * PORTABILITY:
 *   - C11 stdatomic.h (preferred)
 *   - GCC/Clang __atomic builtins (fallback)
 *   - Tested on: x86-64, ARM64, RISC-V
 */

#ifndef NIMCP_ATOMIC_H
#define NIMCP_ATOMIC_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Compiler and Feature Detection
//=============================================================================

/**
 * WHAT: Detect C11 atomics availability
 * WHY:  Use modern standard when available, fallback to builtins
 * HOW:  Check __STDC_VERSION__ and compiler-specific macros
 */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_ATOMICS__)
    #define NIMCP_HAVE_C11_ATOMICS 1
    #include <stdatomic.h>
#elif defined(__GNUC__) || defined(__clang__)
    #define NIMCP_HAVE_GCC_ATOMICS 1
#else
    #error "No atomic operations support detected. Requires C11 or GCC/Clang."
#endif

//=============================================================================
// Memory Ordering Enumeration
//=============================================================================

/**
 * WHAT: Memory ordering constraints for atomic operations
 * WHY:  Control visibility of memory operations across threads
 * HOW:  Maps to C11 memory_order or compiler intrinsics
 *
 * RELAXED:  No ordering constraints, only atomicity guaranteed
 * ACQUIRE:  Prevents reordering of subsequent loads/stores before this operation
 * RELEASE:  Prevents reordering of prior loads/stores after this operation
 * ACQ_REL:  Both acquire and release semantics
 * SEQ_CST:  Sequentially consistent (strongest, total global order)
 *
 * PERFORMANCE: Relaxed < Acquire/Release < Seq_Cst (weakest to strongest)
 */
typedef enum {
    NIMCP_MEMORY_ORDER_RELAXED,  ///< No ordering, just atomicity
    NIMCP_MEMORY_ORDER_ACQUIRE,  ///< Acquire barrier (for loads)
    NIMCP_MEMORY_ORDER_RELEASE,  ///< Release barrier (for stores)
    NIMCP_MEMORY_ORDER_ACQ_REL,  ///< Both acquire and release
    NIMCP_MEMORY_ORDER_SEQ_CST   ///< Sequentially consistent (default)
} nimcp_memory_order_t;

//=============================================================================
// Atomic Type Definitions
//=============================================================================

/**
 * WHAT: Opaque atomic types for different integer sizes and pointers
 * WHY:  Ensure proper alignment and prevent non-atomic access
 * HOW:  Wrap C11 _Atomic or use struct with aligned fields
 */

#ifdef NIMCP_HAVE_C11_ATOMICS
    // Use C11 atomic types
    typedef struct { _Atomic int32_t value; } nimcp_atomic_int32_t;
    typedef struct { _Atomic int64_t value; } nimcp_atomic_int64_t;
    typedef struct { _Atomic uint32_t value; } nimcp_atomic_uint32_t;
    typedef struct { _Atomic uint64_t value; } nimcp_atomic_uint64_t;
    typedef struct { _Atomic bool value; } nimcp_atomic_bool_t;
    typedef struct { _Atomic uintptr_t value; } nimcp_atomic_ptr_t;
#else
    // Use properly aligned types for GCC atomics
    typedef struct { int32_t value __attribute__((aligned(4))); } nimcp_atomic_int32_t;
    typedef struct { int64_t value __attribute__((aligned(8))); } nimcp_atomic_int64_t;
    typedef struct { uint32_t value __attribute__((aligned(4))); } nimcp_atomic_uint32_t;
    typedef struct { uint64_t value __attribute__((aligned(8))); } nimcp_atomic_uint64_t;
    typedef struct { bool value __attribute__((aligned(1))); } nimcp_atomic_bool_t;
    typedef struct { uintptr_t value __attribute__((aligned(sizeof(void*)))); } nimcp_atomic_ptr_t;
#endif

//=============================================================================
// Internal Helper: Memory Order Conversion
//=============================================================================

/**
 * WHAT: Convert NIMCP memory order to compiler-specific order
 * WHY:  Abstract away differences between C11 and GCC atomics
 * HOW:  Inline function mapping enum to appropriate constants
 */
#ifdef NIMCP_HAVE_C11_ATOMICS
static inline memory_order nimcp_to_c11_order(nimcp_memory_order_t order)
{
    switch (order) {
        case NIMCP_MEMORY_ORDER_RELAXED: return memory_order_relaxed;
        case NIMCP_MEMORY_ORDER_ACQUIRE: return memory_order_acquire;
        case NIMCP_MEMORY_ORDER_RELEASE: return memory_order_release;
        case NIMCP_MEMORY_ORDER_ACQ_REL: return memory_order_acq_rel;
        case NIMCP_MEMORY_ORDER_SEQ_CST: return memory_order_seq_cst;
        default: return memory_order_seq_cst;
    }
}
#else
static inline int nimcp_to_gcc_order(nimcp_memory_order_t order)
{
    switch (order) {
        case NIMCP_MEMORY_ORDER_RELAXED: return __ATOMIC_RELAXED;
        case NIMCP_MEMORY_ORDER_ACQUIRE: return __ATOMIC_ACQUIRE;
        case NIMCP_MEMORY_ORDER_RELEASE: return __ATOMIC_RELEASE;
        case NIMCP_MEMORY_ORDER_ACQ_REL: return __ATOMIC_ACQ_REL;
        case NIMCP_MEMORY_ORDER_SEQ_CST: return __ATOMIC_SEQ_CST;
        default: return __ATOMIC_SEQ_CST;
    }
}
#endif

//=============================================================================
// Initialization Functions
//=============================================================================

/**
 * WHAT: Initialize atomic variable to a specific value
 * WHY:  Set initial state before concurrent access
 * HOW:  Non-atomic store to value field
 *
 * NOTE: Must be called before any atomic operations
 * No synchronization required (happens-before concurrent access)
 */

static inline void nimcp_atomic_init_i32(nimcp_atomic_int32_t* a, int32_t val)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    atomic_init(&a->value, val);
#else
    a->value = val;
#endif
}

static inline void nimcp_atomic_init_i64(nimcp_atomic_int64_t* a, int64_t val)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    atomic_init(&a->value, val);
#else
    a->value = val;
#endif
}

static inline void nimcp_atomic_init_u32(nimcp_atomic_uint32_t* a, uint32_t val)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    atomic_init(&a->value, val);
#else
    a->value = val;
#endif
}

static inline void nimcp_atomic_init_u64(nimcp_atomic_uint64_t* a, uint64_t val)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    atomic_init(&a->value, val);
#else
    a->value = val;
#endif
}

static inline void nimcp_atomic_init_ptr(nimcp_atomic_ptr_t* a, void* val)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    atomic_init(&a->value, (uintptr_t)val);
#else
    a->value = (uintptr_t)val;
#endif
}

static inline void nimcp_atomic_init_bool(nimcp_atomic_bool_t* a, bool val)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    atomic_init(&a->value, val);
#else
    a->value = val;
#endif
}

//=============================================================================
// Load Operations
//=============================================================================

/**
 * WHAT: Atomically load a value
 * WHY:  Read shared data safely from multiple threads
 * HOW:  C11 atomic_load_explicit or GCC __atomic_load_n
 *
 * @param a     Pointer to atomic variable
 * @param order Memory ordering (typically ACQUIRE or SEQ_CST for loads)
 * @return Current value
 */

static inline int32_t nimcp_atomic_load_i32(const nimcp_atomic_int32_t* a, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return atomic_load_explicit(&a->value, nimcp_to_c11_order(order));
#else
    return __atomic_load_n(&a->value, nimcp_to_gcc_order(order));
#endif
}

static inline int64_t nimcp_atomic_load_i64(const nimcp_atomic_int64_t* a, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return atomic_load_explicit(&a->value, nimcp_to_c11_order(order));
#else
    return __atomic_load_n(&a->value, nimcp_to_gcc_order(order));
#endif
}

static inline uint32_t nimcp_atomic_load_u32(const nimcp_atomic_uint32_t* a, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return atomic_load_explicit(&a->value, nimcp_to_c11_order(order));
#else
    return __atomic_load_n(&a->value, nimcp_to_gcc_order(order));
#endif
}

static inline uint64_t nimcp_atomic_load_u64(const nimcp_atomic_uint64_t* a, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return atomic_load_explicit(&a->value, nimcp_to_c11_order(order));
#else
    return __atomic_load_n(&a->value, nimcp_to_gcc_order(order));
#endif
}

static inline void* nimcp_atomic_load_ptr(const nimcp_atomic_ptr_t* a, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return (void*)atomic_load_explicit(&a->value, nimcp_to_c11_order(order));
#else
    return (void*)__atomic_load_n(&a->value, nimcp_to_gcc_order(order));
#endif
}

static inline bool nimcp_atomic_load_bool(const nimcp_atomic_bool_t* a, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return atomic_load_explicit(&a->value, nimcp_to_c11_order(order));
#else
    return __atomic_load_n(&a->value, nimcp_to_gcc_order(order));
#endif
}

//=============================================================================
// Store Operations
//=============================================================================

/**
 * WHAT: Atomically store a value
 * WHY:  Write shared data safely from multiple threads
 * HOW:  C11 atomic_store_explicit or GCC __atomic_store_n
 *
 * @param a     Pointer to atomic variable
 * @param val   Value to store
 * @param order Memory ordering (typically RELEASE or SEQ_CST for stores)
 */

static inline void nimcp_atomic_store_i32(nimcp_atomic_int32_t* a, int32_t val, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    atomic_store_explicit(&a->value, val, nimcp_to_c11_order(order));
#else
    __atomic_store_n(&a->value, val, nimcp_to_gcc_order(order));
#endif
}

static inline void nimcp_atomic_store_i64(nimcp_atomic_int64_t* a, int64_t val, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    atomic_store_explicit(&a->value, val, nimcp_to_c11_order(order));
#else
    __atomic_store_n(&a->value, val, nimcp_to_gcc_order(order));
#endif
}

static inline void nimcp_atomic_store_u32(nimcp_atomic_uint32_t* a, uint32_t val, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    atomic_store_explicit(&a->value, val, nimcp_to_c11_order(order));
#else
    __atomic_store_n(&a->value, val, nimcp_to_gcc_order(order));
#endif
}

static inline void nimcp_atomic_store_u64(nimcp_atomic_uint64_t* a, uint64_t val, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    atomic_store_explicit(&a->value, val, nimcp_to_c11_order(order));
#else
    __atomic_store_n(&a->value, val, nimcp_to_gcc_order(order));
#endif
}

static inline void nimcp_atomic_store_ptr(nimcp_atomic_ptr_t* a, void* val, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    atomic_store_explicit(&a->value, (uintptr_t)val, nimcp_to_c11_order(order));
#else
    __atomic_store_n(&a->value, (uintptr_t)val, nimcp_to_gcc_order(order));
#endif
}

static inline void nimcp_atomic_store_bool(nimcp_atomic_bool_t* a, bool val, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    atomic_store_explicit(&a->value, val, nimcp_to_c11_order(order));
#else
    __atomic_store_n(&a->value, val, nimcp_to_gcc_order(order));
#endif
}

//=============================================================================
// Fetch-Add Operations
//=============================================================================

/**
 * WHAT: Atomically add to variable and return old value
 * WHY:  Increment/decrement counters without locks
 * HOW:  C11 atomic_fetch_add or GCC __atomic_fetch_add
 *
 * @param a     Pointer to atomic variable
 * @param val   Value to add
 * @param order Memory ordering
 * @return Previous value (before addition)
 */

static inline int32_t nimcp_atomic_fetch_add_i32(nimcp_atomic_int32_t* a, int32_t val, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return atomic_fetch_add_explicit(&a->value, val, nimcp_to_c11_order(order));
#else
    return __atomic_fetch_add(&a->value, val, nimcp_to_gcc_order(order));
#endif
}

static inline int64_t nimcp_atomic_fetch_add_i64(nimcp_atomic_int64_t* a, int64_t val, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return atomic_fetch_add_explicit(&a->value, val, nimcp_to_c11_order(order));
#else
    return __atomic_fetch_add(&a->value, val, nimcp_to_gcc_order(order));
#endif
}

static inline uint32_t nimcp_atomic_fetch_add_u32(nimcp_atomic_uint32_t* a, uint32_t val, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return atomic_fetch_add_explicit(&a->value, val, nimcp_to_c11_order(order));
#else
    return __atomic_fetch_add(&a->value, val, nimcp_to_gcc_order(order));
#endif
}

static inline uint64_t nimcp_atomic_fetch_add_u64(nimcp_atomic_uint64_t* a, uint64_t val, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return atomic_fetch_add_explicit(&a->value, val, nimcp_to_c11_order(order));
#else
    return __atomic_fetch_add(&a->value, val, nimcp_to_gcc_order(order));
#endif
}

//=============================================================================
// Fetch-Sub Operations
//=============================================================================

/**
 * WHAT: Atomically subtract from variable and return old value
 * WHY:  Decrement counters without locks
 * HOW:  C11 atomic_fetch_sub or GCC __atomic_fetch_sub
 *
 * @param a     Pointer to atomic variable
 * @param val   Value to subtract
 * @param order Memory ordering
 * @return Previous value (before subtraction)
 */

static inline int32_t nimcp_atomic_fetch_sub_i32(nimcp_atomic_int32_t* a, int32_t val, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return atomic_fetch_sub_explicit(&a->value, val, nimcp_to_c11_order(order));
#else
    return __atomic_fetch_sub(&a->value, val, nimcp_to_gcc_order(order));
#endif
}

static inline int64_t nimcp_atomic_fetch_sub_i64(nimcp_atomic_int64_t* a, int64_t val, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return atomic_fetch_sub_explicit(&a->value, val, nimcp_to_c11_order(order));
#else
    return __atomic_fetch_sub(&a->value, val, nimcp_to_gcc_order(order));
#endif
}

static inline uint32_t nimcp_atomic_fetch_sub_u32(nimcp_atomic_uint32_t* a, uint32_t val, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return atomic_fetch_sub_explicit(&a->value, val, nimcp_to_c11_order(order));
#else
    return __atomic_fetch_sub(&a->value, val, nimcp_to_gcc_order(order));
#endif
}

static inline uint64_t nimcp_atomic_fetch_sub_u64(nimcp_atomic_uint64_t* a, uint64_t val, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return atomic_fetch_sub_explicit(&a->value, val, nimcp_to_c11_order(order));
#else
    return __atomic_fetch_sub(&a->value, val, nimcp_to_gcc_order(order));
#endif
}

//=============================================================================
// Fetch-And Operations
//=============================================================================

/**
 * WHAT: Atomically perform bitwise AND and return old value
 * WHY:  Clear bits in flags/bitmasks without locks
 * HOW:  C11 atomic_fetch_and or GCC __atomic_fetch_and
 *
 * @param a     Pointer to atomic variable
 * @param val   Value to AND with
 * @param order Memory ordering
 * @return Previous value (before AND)
 */

static inline int32_t nimcp_atomic_fetch_and_i32(nimcp_atomic_int32_t* a, int32_t val, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return atomic_fetch_and_explicit(&a->value, val, nimcp_to_c11_order(order));
#else
    return __atomic_fetch_and(&a->value, val, nimcp_to_gcc_order(order));
#endif
}

static inline int64_t nimcp_atomic_fetch_and_i64(nimcp_atomic_int64_t* a, int64_t val, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return atomic_fetch_and_explicit(&a->value, val, nimcp_to_c11_order(order));
#else
    return __atomic_fetch_and(&a->value, val, nimcp_to_gcc_order(order));
#endif
}

static inline uint32_t nimcp_atomic_fetch_and_u32(nimcp_atomic_uint32_t* a, uint32_t val, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return atomic_fetch_and_explicit(&a->value, val, nimcp_to_c11_order(order));
#else
    return __atomic_fetch_and(&a->value, val, nimcp_to_gcc_order(order));
#endif
}

static inline uint64_t nimcp_atomic_fetch_and_u64(nimcp_atomic_uint64_t* a, uint64_t val, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return atomic_fetch_and_explicit(&a->value, val, nimcp_to_c11_order(order));
#else
    return __atomic_fetch_and(&a->value, val, nimcp_to_gcc_order(order));
#endif
}

//=============================================================================
// Fetch-Or Operations
//=============================================================================

/**
 * WHAT: Atomically perform bitwise OR and return old value
 * WHY:  Set bits in flags/bitmasks without locks
 * HOW:  C11 atomic_fetch_or or GCC __atomic_fetch_or
 *
 * @param a     Pointer to atomic variable
 * @param val   Value to OR with
 * @param order Memory ordering
 * @return Previous value (before OR)
 */

static inline int32_t nimcp_atomic_fetch_or_i32(nimcp_atomic_int32_t* a, int32_t val, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return atomic_fetch_or_explicit(&a->value, val, nimcp_to_c11_order(order));
#else
    return __atomic_fetch_or(&a->value, val, nimcp_to_gcc_order(order));
#endif
}

static inline int64_t nimcp_atomic_fetch_or_i64(nimcp_atomic_int64_t* a, int64_t val, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return atomic_fetch_or_explicit(&a->value, val, nimcp_to_c11_order(order));
#else
    return __atomic_fetch_or(&a->value, val, nimcp_to_gcc_order(order));
#endif
}

static inline uint32_t nimcp_atomic_fetch_or_u32(nimcp_atomic_uint32_t* a, uint32_t val, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return atomic_fetch_or_explicit(&a->value, val, nimcp_to_c11_order(order));
#else
    return __atomic_fetch_or(&a->value, val, nimcp_to_gcc_order(order));
#endif
}

static inline uint64_t nimcp_atomic_fetch_or_u64(nimcp_atomic_uint64_t* a, uint64_t val, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return atomic_fetch_or_explicit(&a->value, val, nimcp_to_c11_order(order));
#else
    return __atomic_fetch_or(&a->value, val, nimcp_to_gcc_order(order));
#endif
}

//=============================================================================
// Fetch-Xor Operations
//=============================================================================

/**
 * WHAT: Atomically perform bitwise XOR and return old value
 * WHY:  Toggle bits in flags/bitmasks without locks
 * HOW:  C11 atomic_fetch_xor or GCC __atomic_fetch_xor
 *
 * @param a     Pointer to atomic variable
 * @param val   Value to XOR with
 * @param order Memory ordering
 * @return Previous value (before XOR)
 */

static inline int32_t nimcp_atomic_fetch_xor_i32(nimcp_atomic_int32_t* a, int32_t val, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return atomic_fetch_xor_explicit(&a->value, val, nimcp_to_c11_order(order));
#else
    return __atomic_fetch_xor(&a->value, val, nimcp_to_gcc_order(order));
#endif
}

static inline int64_t nimcp_atomic_fetch_xor_i64(nimcp_atomic_int64_t* a, int64_t val, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return atomic_fetch_xor_explicit(&a->value, val, nimcp_to_c11_order(order));
#else
    return __atomic_fetch_xor(&a->value, val, nimcp_to_gcc_order(order));
#endif
}

static inline uint32_t nimcp_atomic_fetch_xor_u32(nimcp_atomic_uint32_t* a, uint32_t val, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return atomic_fetch_xor_explicit(&a->value, val, nimcp_to_c11_order(order));
#else
    return __atomic_fetch_xor(&a->value, val, nimcp_to_gcc_order(order));
#endif
}

static inline uint64_t nimcp_atomic_fetch_xor_u64(nimcp_atomic_uint64_t* a, uint64_t val, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return atomic_fetch_xor_explicit(&a->value, val, nimcp_to_c11_order(order));
#else
    return __atomic_fetch_xor(&a->value, val, nimcp_to_gcc_order(order));
#endif
}

//=============================================================================
// Compare-Exchange Operations
//=============================================================================

/**
 * WHAT: Atomically compare and conditionally swap
 * WHY:  Foundation for lock-free algorithms (CAS loop)
 * HOW:  C11 atomic_compare_exchange_strong or GCC __atomic_compare_exchange_n
 *
 * Compares *a with *expected:
 *   - If equal: stores desired into *a and returns true
 *   - If not equal: loads *a into *expected and returns false
 *
 * @param a        Pointer to atomic variable
 * @param expected Pointer to expected value (updated on failure)
 * @param desired  Value to store if comparison succeeds
 * @param order    Memory ordering for success case
 * @return true if exchange occurred, false otherwise
 *
 * USAGE PATTERN (lock-free increment):
 *   int32_t old, new;
 *   do {
 *       old = nimcp_atomic_load_i32(&counter, NIMCP_MEMORY_ORDER_RELAXED);
 *       new = old + 1;
 *   } while (!nimcp_atomic_compare_exchange_i32(&counter, &old, new, NIMCP_MEMORY_ORDER_RELEASE));
 */

static inline bool nimcp_atomic_compare_exchange_i32(nimcp_atomic_int32_t* a, int32_t* expected, int32_t desired,
                                                       nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return atomic_compare_exchange_strong_explicit(&a->value, expected, desired, nimcp_to_c11_order(order),
                                                     nimcp_to_c11_order(NIMCP_MEMORY_ORDER_RELAXED));
#else
    return __atomic_compare_exchange_n(&a->value, expected, desired, false, nimcp_to_gcc_order(order),
                                        nimcp_to_gcc_order(NIMCP_MEMORY_ORDER_RELAXED));
#endif
}

static inline bool nimcp_atomic_compare_exchange_i64(nimcp_atomic_int64_t* a, int64_t* expected, int64_t desired,
                                                       nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return atomic_compare_exchange_strong_explicit(&a->value, expected, desired, nimcp_to_c11_order(order),
                                                     nimcp_to_c11_order(NIMCP_MEMORY_ORDER_RELAXED));
#else
    return __atomic_compare_exchange_n(&a->value, expected, desired, false, nimcp_to_gcc_order(order),
                                        nimcp_to_gcc_order(NIMCP_MEMORY_ORDER_RELAXED));
#endif
}

static inline bool nimcp_atomic_compare_exchange_u32(nimcp_atomic_uint32_t* a, uint32_t* expected, uint32_t desired,
                                                       nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return atomic_compare_exchange_strong_explicit(&a->value, expected, desired, nimcp_to_c11_order(order),
                                                     nimcp_to_c11_order(NIMCP_MEMORY_ORDER_RELAXED));
#else
    return __atomic_compare_exchange_n(&a->value, expected, desired, false, nimcp_to_gcc_order(order),
                                        nimcp_to_gcc_order(NIMCP_MEMORY_ORDER_RELAXED));
#endif
}

static inline bool nimcp_atomic_compare_exchange_u64(nimcp_atomic_uint64_t* a, uint64_t* expected, uint64_t desired,
                                                       nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return atomic_compare_exchange_strong_explicit(&a->value, expected, desired, nimcp_to_c11_order(order),
                                                     nimcp_to_c11_order(NIMCP_MEMORY_ORDER_RELAXED));
#else
    return __atomic_compare_exchange_n(&a->value, expected, desired, false, nimcp_to_gcc_order(order),
                                        nimcp_to_gcc_order(NIMCP_MEMORY_ORDER_RELAXED));
#endif
}

static inline bool nimcp_atomic_compare_exchange_ptr(nimcp_atomic_ptr_t* a, void** expected, void* desired,
                                                       nimcp_memory_order_t order)
{
    uintptr_t expected_val = (uintptr_t)*expected;
    bool result;

#ifdef NIMCP_HAVE_C11_ATOMICS
    result = atomic_compare_exchange_strong_explicit(&a->value, &expected_val, (uintptr_t)desired,
                                                       nimcp_to_c11_order(order),
                                                       nimcp_to_c11_order(NIMCP_MEMORY_ORDER_RELAXED));
#else
    result = __atomic_compare_exchange_n(&a->value, &expected_val, (uintptr_t)desired, false,
                                          nimcp_to_gcc_order(order),
                                          nimcp_to_gcc_order(NIMCP_MEMORY_ORDER_RELAXED));
#endif

    *expected = (void*)expected_val;
    return result;
}

static inline bool nimcp_atomic_compare_exchange_bool(nimcp_atomic_bool_t* a, bool* expected, bool desired,
                                                        nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return atomic_compare_exchange_strong_explicit(&a->value, expected, desired, nimcp_to_c11_order(order),
                                                     nimcp_to_c11_order(NIMCP_MEMORY_ORDER_RELAXED));
#else
    return __atomic_compare_exchange_n(&a->value, expected, desired, false, nimcp_to_gcc_order(order),
                                        nimcp_to_gcc_order(NIMCP_MEMORY_ORDER_RELAXED));
#endif
}

//=============================================================================
// Exchange Operations
//=============================================================================

/**
 * WHAT: Atomically swap value and return old value
 * WHY:  Unconditional swap (unlike compare-exchange)
 * HOW:  C11 atomic_exchange or GCC __atomic_exchange_n
 *
 * @param a     Pointer to atomic variable
 * @param val   New value to store
 * @param order Memory ordering
 * @return Previous value
 */

static inline int32_t nimcp_atomic_exchange_i32(nimcp_atomic_int32_t* a, int32_t val, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return atomic_exchange_explicit(&a->value, val, nimcp_to_c11_order(order));
#else
    return __atomic_exchange_n(&a->value, val, nimcp_to_gcc_order(order));
#endif
}

static inline int64_t nimcp_atomic_exchange_i64(nimcp_atomic_int64_t* a, int64_t val, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return atomic_exchange_explicit(&a->value, val, nimcp_to_c11_order(order));
#else
    return __atomic_exchange_n(&a->value, val, nimcp_to_gcc_order(order));
#endif
}

static inline uint32_t nimcp_atomic_exchange_u32(nimcp_atomic_uint32_t* a, uint32_t val, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return atomic_exchange_explicit(&a->value, val, nimcp_to_c11_order(order));
#else
    return __atomic_exchange_n(&a->value, val, nimcp_to_gcc_order(order));
#endif
}

static inline uint64_t nimcp_atomic_exchange_u64(nimcp_atomic_uint64_t* a, uint64_t val, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return atomic_exchange_explicit(&a->value, val, nimcp_to_c11_order(order));
#else
    return __atomic_exchange_n(&a->value, val, nimcp_to_gcc_order(order));
#endif
}

static inline void* nimcp_atomic_exchange_ptr(nimcp_atomic_ptr_t* a, void* val, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return (void*)atomic_exchange_explicit(&a->value, (uintptr_t)val, nimcp_to_c11_order(order));
#else
    return (void*)__atomic_exchange_n(&a->value, (uintptr_t)val, nimcp_to_gcc_order(order));
#endif
}

static inline bool nimcp_atomic_exchange_bool(nimcp_atomic_bool_t* a, bool val, nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    return atomic_exchange_explicit(&a->value, val, nimcp_to_c11_order(order));
#else
    return __atomic_exchange_n(&a->value, val, nimcp_to_gcc_order(order));
#endif
}

//=============================================================================
// Memory Barriers
//=============================================================================

/**
 * WHAT: Standalone memory fence/barrier
 * WHY:  Enforce memory ordering without atomic operation
 * HOW:  C11 atomic_thread_fence or GCC __atomic_thread_fence
 *
 * Use when you need ordering guarantees without atomic read/write
 * Example: Ensuring all prior writes are visible before flag is set
 *
 * @param order Memory ordering constraint
 */
static inline void nimcp_atomic_thread_fence(nimcp_memory_order_t order)
{
#ifdef NIMCP_HAVE_C11_ATOMICS
    atomic_thread_fence(nimcp_to_c11_order(order));
#else
    __atomic_thread_fence(nimcp_to_gcc_order(order));
#endif
}

//=============================================================================
// Convenience Macros
//=============================================================================

/**
 * WHAT: Common atomic operation shortcuts
 * WHY:  Simplify frequent patterns (increment, decrement, test-and-set)
 * HOW:  Macros wrapping fetch operations with intuitive names
 */

// Increment (returns new value, not old)
#define NIMCP_ATOMIC_INC_I32(a) (nimcp_atomic_fetch_add_i32((a), 1, NIMCP_MEMORY_ORDER_SEQ_CST) + 1)
#define NIMCP_ATOMIC_INC_I64(a) (nimcp_atomic_fetch_add_i64((a), 1, NIMCP_MEMORY_ORDER_SEQ_CST) + 1)
#define NIMCP_ATOMIC_INC_U32(a) (nimcp_atomic_fetch_add_u32((a), 1, NIMCP_MEMORY_ORDER_SEQ_CST) + 1)
#define NIMCP_ATOMIC_INC_U64(a) (nimcp_atomic_fetch_add_u64((a), 1, NIMCP_MEMORY_ORDER_SEQ_CST) + 1)

// Decrement (returns new value, not old)
#define NIMCP_ATOMIC_DEC_I32(a) (nimcp_atomic_fetch_sub_i32((a), 1, NIMCP_MEMORY_ORDER_SEQ_CST) - 1)
#define NIMCP_ATOMIC_DEC_I64(a) (nimcp_atomic_fetch_sub_i64((a), 1, NIMCP_MEMORY_ORDER_SEQ_CST) - 1)
#define NIMCP_ATOMIC_DEC_U32(a) (nimcp_atomic_fetch_sub_u32((a), 1, NIMCP_MEMORY_ORDER_SEQ_CST) - 1)
#define NIMCP_ATOMIC_DEC_U64(a) (nimcp_atomic_fetch_sub_u64((a), 1, NIMCP_MEMORY_ORDER_SEQ_CST) - 1)

// Set bit (OR with bit mask)
#define NIMCP_ATOMIC_SET_BIT_U32(a, bit) nimcp_atomic_fetch_or_u32((a), (1U << (bit)), NIMCP_MEMORY_ORDER_SEQ_CST)
#define NIMCP_ATOMIC_SET_BIT_U64(a, bit) nimcp_atomic_fetch_or_u64((a), (1ULL << (bit)), NIMCP_MEMORY_ORDER_SEQ_CST)

// Clear bit (AND with inverted mask)
#define NIMCP_ATOMIC_CLEAR_BIT_U32(a, bit) nimcp_atomic_fetch_and_u32((a), ~(1U << (bit)), NIMCP_MEMORY_ORDER_SEQ_CST)
#define NIMCP_ATOMIC_CLEAR_BIT_U64(a, bit) nimcp_atomic_fetch_and_u64((a), ~(1ULL << (bit)), NIMCP_MEMORY_ORDER_SEQ_CST)

// Toggle bit (XOR with bit mask)
#define NIMCP_ATOMIC_TOGGLE_BIT_U32(a, bit) nimcp_atomic_fetch_xor_u32((a), (1U << (bit)), NIMCP_MEMORY_ORDER_SEQ_CST)
#define NIMCP_ATOMIC_TOGGLE_BIT_U64(a, bit) nimcp_atomic_fetch_xor_u64((a), (1ULL << (bit)), NIMCP_MEMORY_ORDER_SEQ_CST)

// Test bit (returns true if bit is set)
#define NIMCP_ATOMIC_TEST_BIT_U32(a, bit) \
    ((nimcp_atomic_load_u32((a), NIMCP_MEMORY_ORDER_SEQ_CST) & (1U << (bit))) != 0)
#define NIMCP_ATOMIC_TEST_BIT_U64(a, bit) \
    ((nimcp_atomic_load_u64((a), NIMCP_MEMORY_ORDER_SEQ_CST) & (1ULL << (bit))) != 0)

//=============================================================================
// Convenience Functions (Increment/Decrement)
//=============================================================================

/**
 * WHAT: Convenience functions for increment/decrement operations
 * WHY:  Provide function versions (not just macros) for better type safety
 * HOW:  Inline functions wrapping fetch_add/fetch_sub with return of new value
 *
 * These functions return the NEW value (after increment/decrement), unlike
 * fetch_add/fetch_sub which return the OLD value.
 */

/**
 * @brief Atomically increment and return new value
 * @param a Pointer to atomic variable
 * @return New value after increment
 */
static inline int32_t nimcp_atomic_increment_i32(nimcp_atomic_int32_t* a)
{
    return nimcp_atomic_fetch_add_i32(a, 1, NIMCP_MEMORY_ORDER_SEQ_CST) + 1;
}

static inline int64_t nimcp_atomic_increment_i64(nimcp_atomic_int64_t* a)
{
    return nimcp_atomic_fetch_add_i64(a, 1, NIMCP_MEMORY_ORDER_SEQ_CST) + 1;
}

static inline uint32_t nimcp_atomic_increment_u32(nimcp_atomic_uint32_t* a)
{
    return nimcp_atomic_fetch_add_u32(a, 1, NIMCP_MEMORY_ORDER_SEQ_CST) + 1;
}

static inline uint64_t nimcp_atomic_increment_u64(nimcp_atomic_uint64_t* a)
{
    return nimcp_atomic_fetch_add_u64(a, 1, NIMCP_MEMORY_ORDER_SEQ_CST) + 1;
}

/**
 * @brief Atomically decrement and return new value
 * @param a Pointer to atomic variable
 * @return New value after decrement
 */
static inline int32_t nimcp_atomic_decrement_i32(nimcp_atomic_int32_t* a)
{
    return nimcp_atomic_fetch_sub_i32(a, 1, NIMCP_MEMORY_ORDER_SEQ_CST) - 1;
}

static inline int64_t nimcp_atomic_decrement_i64(nimcp_atomic_int64_t* a)
{
    return nimcp_atomic_fetch_sub_i64(a, 1, NIMCP_MEMORY_ORDER_SEQ_CST) - 1;
}

static inline uint32_t nimcp_atomic_decrement_u32(nimcp_atomic_uint32_t* a)
{
    return nimcp_atomic_fetch_sub_u32(a, 1, NIMCP_MEMORY_ORDER_SEQ_CST) - 1;
}

static inline uint64_t nimcp_atomic_decrement_u64(nimcp_atomic_uint64_t* a)
{
    return nimcp_atomic_fetch_sub_u64(a, 1, NIMCP_MEMORY_ORDER_SEQ_CST) - 1;
}

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_ATOMIC_H
