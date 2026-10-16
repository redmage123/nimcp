/**
 * @file nimcp_constant_time.c
 * @brief Implementation of constant-time cryptographic operations
 *
 * WHAT: Timing-safe comparison and selection operations
 * WHY:  Prevent side-channel timing attacks on cryptographic operations
 * HOW:  Branchless algorithms with data-independent execution time
 *
 * SECURITY PRINCIPLES:
 * 1. No conditional branches on secret data
 * 2. No table lookups with secret indices (except ct_lookup which is safe)
 * 3. No early exit conditions based on data values
 * 4. Volatile pointers to prevent compiler optimization
 * 5. Sequential memory access to resist cache timing attacks
 *
 * TESTING:
 * All functions are tested for:
 * - Functional correctness
 * - Constant-time behavior via statistical timing analysis
 * - Resistance to compiler optimization
 *
 * @author NIMCP Security Team
 * @date 2025-12-07
 */

#include "security/nimcp_constant_time.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/exception/nimcp_exception.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdatomic.h>

#define LOG_MODULE "constant_time"
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(constant_time, MESH_ADAPTER_CATEGORY_SECURITY)


// Platform-specific secure random
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#else
#include <fcntl.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/random.h>
#endif
#endif

//=============================================================================
// Secure Random Helper
//=============================================================================

/**
 * @brief Generate cryptographically secure random bytes
 *
 * WHAT: Fill buffer with secure random bytes
 * WHY:  rand() is not cryptographically secure - predictable output could
 *       compromise security timing tests and memory wipe patterns
 * HOW:  Use getrandom() (Linux) or /dev/urandom (fallback)
 *
 * @param buf Buffer to fill with random bytes
 * @param len Number of bytes to generate
 * @return true on success, false on failure
 */
static bool s_secure_random_bytes(uint8_t* buf, size_t len)
{
    if (!buf || len == 0) {
        return false;
    }

#ifdef _WIN32
    // Windows: Use BCryptGenRandom
    if (BCryptGenRandom(NULL, buf, (ULONG)len, BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0) {
        return true;
    }
    return false;

#elif defined(__linux__)
    // Linux: Try getrandom() first
    ssize_t result = getrandom(buf, len, 0);
    if (result == (ssize_t)len) {
        return true;
    }

    // Fallback to /dev/urandom
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        ssize_t bytes_read = read(fd, buf, len);
        close(fd);
        if (bytes_read == (ssize_t)len) {
            return true;
        }
    }

    LOG_ERROR("Failed to generate secure random bytes");
    return false;

#else
    // Other POSIX: Use /dev/urandom
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        ssize_t bytes_read = read(fd, buf, len);
        close(fd);
        if (bytes_read == (ssize_t)len) {
            return true;
        }
    }

    LOG_ERROR("Failed to generate secure random bytes");
    return false;
#endif
}

//=============================================================================
// Internal Context Structure
//=============================================================================

/**
 * WHAT: Internal constant-time context structure
 * WHY:  Track statistics and configuration
 * HOW:  Opaque pointer pattern with magic number validation
 */
struct nimcp_ct_context {
    uint32_t magic;                  /**< Magic number for validation */
    nimcp_ct_stats_t stats;          /**< Operation statistics */
    bool bio_async_registered;      /**< Bio-async registration status */
    bio_module_context_t bio_ctx;    /**< Bio-async module context */
};

//=============================================================================
// Global State
//=============================================================================

/** P2-SEC-5: Use _Atomic to prevent constructor/destructor race on g_default_ctx */
static _Atomic(nimcp_ct_context_t) g_default_ctx = NULL;

/**
 * WHAT: Atomic counters for thread-safe global statistics updates
 * WHY:  Prevent race conditions when multiple threads update stats
 * HOW:  Use stdatomic for lock-free concurrent updates
 */
static _Atomic uint64_t g_atomic_hash_comparisons = 0;
static _Atomic uint64_t g_atomic_total_bytes_compared = 0;

//=============================================================================
// Bio-Async Message Handlers
//=============================================================================

/**
 * WHAT: Handle bio-async messages for constant-time module
 * WHY:  Integration with NIMCP's async messaging system
 * HOW:  Process requests for stats, verification tests
 */
static nimcp_error_t s_ct_message_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    nimcp_ct_context_t ctx = (nimcp_ct_context_t)user_data;

    if (!ctx || ctx->magic != NIMCP_CT_MAGIC) {
        LOG_ERROR("Invalid context in message handler");
        return NIMCP_ERROR_INVALID_PARAMETER;
    }

    // Parse message header
    if (msg_size < sizeof(bio_message_header_t)) {
        LOG_ERROR("Message too small");
        return NIMCP_ERROR_BUFFER_TOO_SMALL;
    }

    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    // Handle different message types
    switch (header->type) {
        case BIO_MSG_HEALTH_CHECK: {
            // Send back statistics
            nimcp_ct_stats_t stats;
            nimcp_ct_get_stats(ctx, &stats);

            if (response_promise) {
                nimcp_bio_promise_complete(response_promise, &stats);
            }

            LOG_DEBUG("Stats request handled: %lu comparisons",
                     stats.memcmp_operations);
            return NIMCP_SUCCESS;
        }

        default:
            LOG_WARN("Unknown message type: %u", header->type);
            return NIMCP_ERROR_NOT_IMPLEMENTED;
    }
}

//=============================================================================
// KG-Driven Wiring Callback
//=============================================================================

/**
 * @brief Wiring callback for KG-driven handler registration
 *
 * Called by the orchestrator with discovered message types from the knowledge graph.
 * Registers handlers based on message types discovered at runtime.
 *
 * @param ctx Bio-async module context
 * @param message_types Array of discovered message types
 * @param message_count Number of message types
 * @param user_data User-provided context
 * @return 0 on success, -1 on error
 */
static int constant_time_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    (void)user_data;

    int registered = 0;
    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            case BIO_MSG_HEALTH_CHECK:
                bio_router_register_handler(ctx, message_types[i], s_ct_message_handler);
                registered++;
                break;
            default:
                LOG_DEBUG("Unknown message type %d in wiring callback", message_types[i]);
                break;
        }
    }

    LOG_INFO("KG-driven wiring callback registered %d handlers", registered);
    return (registered > 0) ? 0 : -1;
}

/**
 * WHAT: Register constant-time module with bio-async router
 * WHY:  Enable async communication and monitoring
 * HOW:  Register module ID and message handlers using KG-driven wiring
 */
static nimcp_error_t s_register_bio_async(nimcp_ct_context_t ctx)
{
    if (ctx->bio_async_registered) {
        return NIMCP_SUCCESS;  // Already registered
    }

    bio_router_t router = bio_router_get_global();
    if (!router) {
        LOG_WARN("Bio-async router not available, skipping registration");
        return NIMCP_SUCCESS;  // Not fatal
    }

    bio_module_info_t module_info = {
        .module_id = BIO_MODULE_SECURITY,
        .module_name = "security_constant_time",
        .inbox_capacity = 64,
        .user_data = ctx
    };

    ctx->bio_ctx = bio_router_register_module(&module_info);
    if (!ctx->bio_ctx) {
        LOG_ERROR("Failed to register with bio-async router");
        return NIMCP_ERROR_INVALID_PARAMETER;
    }

    // Try KG-driven wiring callback registration first
    nimcp_error_t result = bio_router_register_wiring_callback(
        BIO_MODULE_SECURITY,
        (void*)constant_time_wiring_handler_callback,
        ctx
    );

    if (result == NIMCP_SUCCESS) {
        LOG_INFO("KG-driven wiring callback registered successfully");
    } else {
        // Fallback to legacy handler registration
        LOG_INFO("Falling back to legacy handler registration");

        LEGACY_HANDLER_REGISTRATION(
            result = bio_router_register_handler(ctx->bio_ctx,
                                                  BIO_MSG_HEALTH_CHECK,
                                                  s_ct_message_handler)
        );
        if (result != NIMCP_SUCCESS) {
            LOG_ERROR("Failed to register message handler: %d", result);
            return NIMCP_ERROR_OPERATION_FAILED;
        }
    }

    ctx->bio_async_registered = true;
    LOG_INFO("Constant-time module registered with bio-async");

    return NIMCP_SUCCESS;
}

//=============================================================================
// Context Management
//=============================================================================

nimcp_ct_context_t nimcp_ct_create(void)
{
    nimcp_ct_context_t ctx = (nimcp_ct_context_t)calloc(1, sizeof(struct nimcp_ct_context));
    if (!ctx) {
        LOG_ERROR("Failed to allocate constant-time context");
        if (nimcp_exception_system_is_initialized()) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_ct_create: context allocation failed");
        }
        return NULL;
    }

    ctx->magic = NIMCP_CT_MAGIC;
    memset(&ctx->stats, 0, sizeof(nimcp_ct_stats_t));
    ctx->bio_async_registered = false;
    ctx->bio_ctx = NULL;

    // Register with bio-async if available
    s_register_bio_async(ctx);

    LOG_INFO("Constant-time context created");

    return ctx;
}

void nimcp_ct_destroy(nimcp_ct_context_t ctx)
{
    if (!ctx) {
        return;
    }

    if (ctx->magic != NIMCP_CT_MAGIC) {
        LOG_ERROR("Invalid magic number in context destroy");
        return;
    }

    // Unregister from bio-async
    if (ctx->bio_async_registered && ctx->bio_ctx) {
        bio_router_unregister_module(ctx->bio_ctx);
    }

    // Zero sensitive data
    ctx->magic = 0;
    nimcp_secure_zero(ctx, sizeof(struct nimcp_ct_context));

    // Clear global reference if this is the default context
    // (prevents destructor from accessing freed memory)
    // P2-SEC-5: Atomic CAS to prevent race
    nimcp_ct_context_t expected = ctx;
    atomic_compare_exchange_strong(&g_default_ctx, &expected, NULL);

    free(ctx);

    LOG_INFO("Constant-time context destroyed");
}

nimcp_result_t nimcp_ct_get_stats(nimcp_ct_context_t ctx, nimcp_ct_stats_t* stats)
{
    if (!ctx || ctx->magic != NIMCP_CT_MAGIC) {
        LOG_ERROR("Invalid context");
        return NIMCP_ERROR_INVALID_PARAMETER;
    }

    if (!stats) {
        LOG_ERROR("NULL stats pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    memcpy(stats, &ctx->stats, sizeof(nimcp_ct_stats_t));

    // Add global atomic counters to context stats (thread-safe read)
    // Context tracks per-context operations; global tracks cross-context operations
    stats->hash_comparisons += atomic_load(&g_atomic_hash_comparisons);
    stats->total_bytes_compared += atomic_load(&g_atomic_total_bytes_compared);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_ct_reset_stats(nimcp_ct_context_t ctx)
{
    if (!ctx || ctx->magic != NIMCP_CT_MAGIC) {
        LOG_ERROR("Invalid context");
        return NIMCP_ERROR_INVALID_PARAMETER;
    }

    memset(&ctx->stats, 0, sizeof(nimcp_ct_stats_t));
    LOG_DEBUG("Statistics reset");

    return NIMCP_SUCCESS;
}

//=============================================================================
// Constant-Time Memory Comparison
//=============================================================================

int nimcp_ct_memcmp(const void* a, const void* b, size_t len)
{
    if (!a || !b) {
        LOG_ERROR("NULL pointer in ct_memcmp");
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    if (len == 0) {
        return 0;
    }

    // WHAT: Use volatile to prevent compiler optimization
    // WHY:  Compiler might optimize away the loop if it detects memcmp pattern
    // HOW:  Volatile forces actual memory reads
    const volatile uint8_t* pa = (const volatile uint8_t*)a;
    const volatile uint8_t* pb = (const volatile uint8_t*)b;

    // WHAT: Accumulate differences using bitwise OR
    // WHY:  No early exit, constant-time regardless of where bytes differ
    // HOW:  diff becomes non-zero if any byte differs
    volatile uint8_t diff = 0;

    for (size_t i = 0; i < len; i++) {
        diff |= (pa[i] ^ pb[i]);
    }

    // WHAT: Convert to 0 or 1 without branches
    // WHY:  Standard return value convention (0 = equal)
    // HOW:  Branchless conversion: (diff | -diff) >> 7 produces 0 or 1
    //       Using volatile to prevent compiler optimization of ternary
    //       Alternative: use bitwise operations to derive result
    //
    // SECURITY: The ternary operator can be optimized by compiler into a branch.
    //           Instead, we use branchless arithmetic:
    //           If diff != 0, then (diff | (-diff)) has MSB set, >> 7 gives 1 (for uint8_t)
    //           If diff == 0, then 0 | 0 = 0, >> 7 gives 0
    volatile uint8_t result = (uint8_t)((diff | ((uint8_t)(-(int8_t)diff))) >> 7);
    return (int)result;
}

int nimcp_ct_memcmp_tracked(nimcp_ct_context_t ctx, const void* a, const void* b, size_t len)
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    int result = nimcp_ct_memcmp(a, b, len);

    clock_gettime(CLOCK_MONOTONIC, &end);

    // P2-SEC-4: Update statistics atomically for thread safety
    if (ctx && ctx->magic == NIMCP_CT_MAGIC) {
        __atomic_fetch_add(&ctx->stats.memcmp_operations, 1, __ATOMIC_RELAXED);
        __atomic_fetch_add(&ctx->stats.total_bytes_compared, len, __ATOMIC_RELAXED);

        // Update average timing
        double elapsed_ns = (end.tv_sec - start.tv_sec) * 1e9 +
                           (end.tv_nsec - start.tv_nsec);

        // Running average (relaxed ordering sufficient for stats)
        uint64_t total_ops = __atomic_load_n(&ctx->stats.memcmp_operations, __ATOMIC_RELAXED) +
                            __atomic_load_n(&ctx->stats.strcmp_operations, __ATOMIC_RELAXED) +
                            __atomic_load_n(&ctx->stats.hash_comparisons, __ATOMIC_RELAXED);

        if (total_ops > 0) {
            double old_avg;
            __atomic_load(&ctx->stats.avg_comparison_time_ns, &old_avg, __ATOMIC_RELAXED);
            double new_avg = (old_avg * (total_ops - 1) + elapsed_ns) / total_ops;
            __atomic_store(&ctx->stats.avg_comparison_time_ns, &new_avg, __ATOMIC_RELAXED);
        }
    }

    return result;
}

//=============================================================================
// Constant-Time String Comparison
//=============================================================================

int nimcp_ct_strcmp(const char* a, const char* b)
{
    if (!a || !b) {
        LOG_ERROR("NULL pointer in ct_strcmp");
        return NIMCP_ERROR_NULL_POINTER;
    }

    // WHAT: Compute string lengths (not constant-time, but acceptable)
    // WHY:  String length is not typically secret; content is
    // HOW:  Standard strlen (we could make this CT too if needed)
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);

    // WHAT: Compare lengths using constant-time select
    // WHY:  Don't want to leak which string is longer
    // HOW:  Use minimum length for comparison
    size_t min_len = (len_a < len_b) ? len_a : len_b;
    size_t max_len = (len_a > len_b) ? len_a : len_b;

    // WHAT: Constant-time memory comparison using min_len
    // WHY:  Prevent heap buffer over-read when strings have different lengths.
    //        The shorter string's allocation may be < max_len bytes.
    //        Length difference is already captured by len_diff below.
    // FIX:  P1-SEC-2 - compare only min_len bytes of content
    int mem_diff = nimcp_ct_memcmp(a, b, min_len);

    // WHAT: Constant-time length comparison
    // WHY:  Combine both checks without branches
    // SECURITY: Branchless conversion of length mismatch to 0 or 1
    //           XOR lengths, then use same branchless trick as memcmp
    size_t len_xor = len_a ^ len_b;
    volatile int len_diff = (int)((len_xor | ((size_t)(-(intptr_t)(len_xor != 0)))) != 0);

    // Return combined result using bitwise OR (no branch)
    return mem_diff | len_diff;
}

int nimcp_ct_strncmp(const char* a, const char* b, size_t n)
{
    if (!a || !b) {
        LOG_ERROR("NULL pointer in ct_strncmp");
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    if (n == 0) {
        return 0;
    }

    // WHAT: Bounded length computation
    // WHY:  Limit comparison length for safety
    size_t len_a = strnlen(a, n);
    size_t len_b = strnlen(b, n);

    size_t min_len = (len_a < len_b) ? len_a : len_b;
    if (min_len > n) {
        min_len = n;
    }

    int mem_diff = nimcp_ct_memcmp(a, b, min_len);

    // SECURITY: Branchless length comparison (same pattern as nimcp_ct_strcmp)
    size_t len_xor = len_a ^ len_b;
    volatile int len_diff = (int)((len_xor | ((size_t)(-(intptr_t)(len_xor != 0)))) != 0);

    return mem_diff | len_diff;
}

//=============================================================================
// Constant-Time Conditional Selection
//=============================================================================

uint8_t nimcp_ct_select_u8(uint8_t a, uint8_t b, uint8_t select)
{
    // WHAT: Create mask from select without branches
    // WHY:  Mask is 0x00 if select==0, 0xFF if select!=0
    // HOW:  Negate the boolean, which extends sign bit
    volatile uint8_t mask = (uint8_t)(-(volatile int8_t)(select != 0));

    // WHAT: Use mask to select between a and b
    // WHY:  a ^ ((a ^ b) & mask) = a when mask=0, b when mask=0xFF
    // HOW:  Bitwise operations only, no branches
    return (uint8_t)(a ^ ((a ^ b) & mask));
}

uint32_t nimcp_ct_select_u32(uint32_t a, uint32_t b, uint32_t select)
{
    volatile uint32_t mask = (uint32_t)(-(volatile int32_t)(select != 0));
    return (uint32_t)(a ^ ((a ^ b) & mask));
}

uint64_t nimcp_ct_select_u64(uint64_t a, uint64_t b, uint64_t select)
{
    volatile uint64_t mask = (uint64_t)(-(volatile int64_t)(select != 0));
    return (uint64_t)(a ^ ((a ^ b) & mask));
}

size_t nimcp_ct_select_size(size_t a, size_t b, uint8_t select)
{
    volatile size_t mask = (size_t)(-(volatile intptr_t)(select != 0));
    return (size_t)(a ^ ((a ^ b) & mask));
}

//=============================================================================
// Constant-Time Array Lookup
//=============================================================================

uint8_t nimcp_ct_lookup_u8(const uint8_t* table, size_t table_len, size_t index)
{
    if (!table) {
        LOG_ERROR("NULL table in ct_lookup_u8");
        return 0;
    }

    if (table_len == 0) {
        return 0;
    }

    // WHAT: Scan entire table using conditional selection
    // WHY:  Execution time independent of index value
    // HOW:  Check each position, accumulate result
    volatile uint8_t result = 0;

    for (size_t i = 0; i < table_len; i++) {
        // WHAT: Create mask if this is the target index
        // WHY:  Mask is used for constant-time selection
        uint8_t is_match = (uint8_t)(i == index);

        // WHAT: Conditionally update result
        // WHY:  Always scans entire table regardless of index
        result = nimcp_ct_select_u8(result, table[i], is_match);
    }

    return (uint8_t)result;
}

uint32_t nimcp_ct_lookup_u32(const uint32_t* table, size_t table_len, size_t index)
{
    if (!table) {
        LOG_ERROR("NULL table in ct_lookup_u32");
        return 0;
    }

    if (table_len == 0) {
        return 0;
    }

    volatile uint32_t result = 0;

    for (size_t i = 0; i < table_len; i++) {
        uint32_t is_match = (uint32_t)(i == index);
        result = nimcp_ct_select_u32(result, table[i], is_match);
    }

    return (uint32_t)result;
}

//=============================================================================
// Cryptographic Hash Comparison
//=============================================================================

bool nimcp_ct_hash_equal(const uint8_t* hash1, const uint8_t* hash2, size_t hash_len)
{
    if (!hash1 || !hash2) {
        LOG_ERROR("NULL hash pointer");
        return false;
    }

    if (hash_len == 0) {
        LOG_WARN("Zero-length hash comparison");
        return true;
    }

    // Update global stats atomically (thread-safe)
    // SECURITY FIX: Use atomic operations to prevent race conditions
    atomic_fetch_add(&g_atomic_hash_comparisons, 1);
    atomic_fetch_add(&g_atomic_total_bytes_compared, hash_len);

    int result = nimcp_ct_memcmp(hash1, hash2, hash_len);

    LOG_DEBUG("Hash comparison: %s (%zu bytes)",
             result == 0 ? "EQUAL" : "DIFFERENT", hash_len);

    return (result == 0);
}

bool nimcp_ct_sha256_equal(const uint8_t hash1[32], const uint8_t hash2[32])
{
    return nimcp_ct_hash_equal(hash1, hash2, 32);
}

//=============================================================================
// Timing Verification (Testing Only)
//=============================================================================

bool nimcp_ct_verify_timing(const char* operation_name, size_t num_trials,
                            double threshold_percent)
{
    if (!operation_name || num_trials < 100) {
        LOG_ERROR("Invalid timing verification parameters");
        return false;
    }

    LOG_INFO("Verifying constant-time behavior for %s (%zu trials, %.1f%% threshold)",
             operation_name, num_trials, threshold_percent);

    // WHAT: Statistical timing analysis
    // WHY:  Detect timing variations that could leak information
    // HOW:  Compare timing distributions for different data patterns

    // Allocate test buffers
    size_t test_size = 256;
    uint8_t* buf_zeros = (uint8_t*)calloc(test_size, 1);
    uint8_t* buf_random = (uint8_t*)calloc(test_size, 1);

    if (!buf_zeros || !buf_random) {
        free(buf_zeros);
        free(buf_random);
        return false;
    }

    // Fill random buffer with cryptographically secure random bytes
    if (!s_secure_random_bytes(buf_random, test_size)) {
        // Fallback: use a fixed pattern if secure random fails (test will still work)
        LOG_WARN("Secure random unavailable, using fixed pattern for timing test");
        for (size_t i = 0; i < test_size; i++) {
            buf_random[i] = (uint8_t)(i ^ 0xA5);
        }
    }

    // Measure timing for zeros
    struct timespec start, end;
    double total_time_zeros = 0.0;

    for (size_t i = 0; i < num_trials; i++) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        nimcp_ct_memcmp(buf_zeros, buf_zeros, test_size);
        clock_gettime(CLOCK_MONOTONIC, &end);

        total_time_zeros += (end.tv_sec - start.tv_sec) * 1e9 +
                           (end.tv_nsec - start.tv_nsec);
    }

    // Measure timing for random data
    double total_time_random = 0.0;

    for (size_t i = 0; i < num_trials; i++) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        nimcp_ct_memcmp(buf_random, buf_zeros, test_size);
        clock_gettime(CLOCK_MONOTONIC, &end);

        total_time_random += (end.tv_sec - start.tv_sec) * 1e9 +
                            (end.tv_nsec - start.tv_nsec);
    }

    // Calculate averages
    double avg_zeros = total_time_zeros / num_trials;
    double avg_random = total_time_random / num_trials;

    // P2-SEC-6: Guard against division by zero when avg_zeros is near zero
    if (avg_zeros < 1.0) {
        LOG_WARN("Timing measurement too fast to verify (avg_zeros=%.2f ns)", avg_zeros);
        free(buf_zeros);
        free(buf_random);
        return false;
    }

    // Calculate percentage difference
    double diff_percent = fabs(avg_random - avg_zeros) / avg_zeros * 100.0;

    LOG_INFO("Timing results: zeros=%.2f ns, random=%.2f ns, diff=%.2f%%",
             avg_zeros, avg_random, diff_percent);

    // Clean up
    free(buf_zeros);
    free(buf_random);

    // Check if within threshold
    bool is_constant_time = (diff_percent <= threshold_percent);

    if (is_constant_time) {
        LOG_INFO("Constant-time verified: %s (%.2f%% <= %.2f%%)",
                operation_name, diff_percent, threshold_percent);
    } else {
        LOG_WARN("Timing leak detected: %s (%.2f%% > %.2f%%)",
                operation_name, diff_percent, threshold_percent);
    }

    return is_constant_time;
}

//=============================================================================
// Secure Memory Wiping
//=============================================================================

void nimcp_secure_zero(void* ptr, size_t len)
{
    if (!ptr || len == 0) {
        return;
    }

#if defined(_WIN32)
    // Windows: Use SecureZeroMemory
    SecureZeroMemory(ptr, len);

#elif defined(__GLIBC__) && __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 25
    // Linux with glibc >= 2.25: Use explicit_bzero
    explicit_bzero(ptr, len);

#elif defined(__OpenBSD__) || defined(__FreeBSD__)
    // BSD systems: explicit_bzero available
    explicit_bzero(ptr, len);

#else
    // WHAT: Fallback using volatile pointer
    // WHY:  Prevents compiler from optimizing away the memset
    // HOW:  Volatile forces actual memory writes
    volatile uint8_t* p = (volatile uint8_t*)ptr;
    for (size_t i = 0; i < len; i++) {
        p[i] = 0;
    }

    // WHAT: Memory barrier to prevent reordering
    // WHY:  Ensure zeroing happens before function returns
    // HOW:  Compiler barrier (prevents reordering at compile-time)
    __asm__ __volatile__("" ::: "memory");
#endif

    LOG_DEBUG("Secure zero: %zu bytes", len);
}

nimcp_result_t nimcp_secure_wipe(void* ptr, size_t len)
{
    if (!ptr) {
        LOG_ERROR("NULL pointer in secure_wipe");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (len == 0) {
        return NIMCP_SUCCESS;
    }

    volatile uint8_t* p = (volatile uint8_t*)ptr;

    // WHAT: Multi-pass overwrite following DoD 5220.22-M
    // WHY:  Defense-in-depth against data remanence attacks
    // HOW:  Multiple passes with different patterns

    // Pass 1: Write 0x00
    for (size_t i = 0; i < len; i++) {
        p[i] = 0x00;
    }

    // Pass 2: Write 0xFF
    for (size_t i = 0; i < len; i++) {
        p[i] = 0xFF;
    }

    // Pass 3: Write cryptographically secure random data
    // Allocate temporary buffer for secure random bytes
    uint8_t* random_buf = (uint8_t*)calloc(len, 1);
    if (random_buf && s_secure_random_bytes(random_buf, len)) {
        for (size_t i = 0; i < len; i++) {
            p[i] = random_buf[i];
        }
        free(random_buf);
    } else {
        // Fallback: XOR pattern if secure random unavailable
        // This is less ideal but still provides some entropy through the pattern
        LOG_WARN("Secure random unavailable for wipe, using XOR pattern");
        if (random_buf) {
            free(random_buf);
        }
        for (size_t i = 0; i < len; i++) {
            p[i] = (uint8_t)((i * 0x5A) ^ 0xC3 ^ ((i >> 8) & 0xFF));
        }
    }

    // Pass 4: Final zero
    for (size_t i = 0; i < len; i++) {
        p[i] = 0x00;
    }

    // Memory barrier
    __asm__ __volatile__("" ::: "memory");

    LOG_DEBUG("Secure wipe: %zu bytes (4 passes)", len);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Module Initialization
//=============================================================================

/**
 * WHAT: Initialize global constant-time context
 * WHY:  Provide default context for stateless API calls
 * HOW:  Create context on first use (lazy initialization)
 */
static void __attribute__((constructor)) s_ct_module_init(void)
{
    /* P2-SEC-5: Atomic load/store for thread-safe lazy init */
    if (!atomic_load(&g_default_ctx)) {
        nimcp_ct_context_t ctx = nimcp_ct_create();
        if (ctx) {
            nimcp_ct_context_t expected = NULL;
            if (!atomic_compare_exchange_strong(&g_default_ctx, &expected, ctx)) {
                /* Another thread raced and won; destroy our duplicate */
                nimcp_ct_destroy(ctx);
            } else {
                LOG_INFO("Constant-time module initialized");
            }
        }
    }
}

/**
 * WHAT: Cleanup global constant-time context
 * WHY:  Prevent memory leaks on shutdown
 * HOW:  Destroy context at program exit
 *
 * NOTE: This destructor is a fallback. Normally, nimcp_ct_shutdown()
 *       is called explicitly before memory cleanup to avoid double-free.
 */
static void __attribute__((destructor)) s_ct_module_cleanup(void)
{
    /* P2-SEC-5: Atomic exchange to prevent double-free race */
    nimcp_ct_context_t ctx = atomic_exchange(&g_default_ctx, NULL);
    if (ctx) {
        nimcp_ct_destroy(ctx);
        LOG_INFO("Constant-time module cleaned up");
    }
}

/**
 * WHAT: Explicit shutdown for constant-time module
 * WHY:  Must be called before nimcp_memory_cleanup() to avoid double-free
 * HOW:  Destroy context and set to NULL so destructor doesn't re-free
 *
 * LIFECYCLE:
 * - nimcp_shutdown() calls this before nimcp_memory_cleanup()
 * - Destructor sees g_default_ctx == NULL and skips cleanup
 * - Prevents double-free when unified memory is destroyed
 */
void nimcp_ct_shutdown(void)
{
    /* P2-SEC-5: Atomic exchange to prevent double-free race */
    nimcp_ct_context_t ctx = atomic_exchange(&g_default_ctx, NULL);
    if (ctx) {
        nimcp_ct_destroy(ctx);
        LOG_INFO("Constant-time module explicitly shut down");
    }
}
