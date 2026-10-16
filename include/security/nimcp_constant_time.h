/**
 * @file nimcp_constant_time.h
 * @brief Constant-time cryptographic operations for NIMCP
 *
 * WHAT: Timing-safe comparison and selection operations to prevent side-channel attacks
 * WHY:  Prevent timing attacks that could leak cryptographic secrets through execution time
 * HOW:  Branchless operations ensuring execution time independent of data values
 *
 * SECURITY RATIONALE:
 * Traditional comparison operations (memcmp, strcmp) exit early on mismatch, creating
 * timing side-channels. An attacker measuring execution time can determine:
 * - Which byte position differs (by observing timing variations)
 * - Hash/password verification success (faster fails vs slower success)
 * - Secret key values (by trying different candidates and timing)
 *
 * This module provides constant-time alternatives that always execute the same number
 * of instructions regardless of input values, preventing timing analysis attacks.
 *
 * BIOLOGICAL ANALOGY:
 * Like the all-or-none principle of action potentials - a neuron fires with the same
 * amplitude and duration regardless of stimulus strength above threshold. Our functions
 * execute with the same timing regardless of data values.
 *
 * USAGE EXAMPLE:
 *   // INSECURE: memcmp exits early, leaks timing information
 *   if (memcmp(hash1, hash2, 32) == 0) { authenticated }
 *
 *   // SECURE: constant-time comparison, no timing leak
 *   if (nimcp_ct_hash_equal(hash1, hash2, 32)) { authenticated }
 *
 * @author NIMCP Security Team
 * @date 2025-12-07
 * @version 1.0.0
 */

#ifndef NIMCP_CONSTANT_TIME_H
#define NIMCP_CONSTANT_TIME_H

#include "utils/validation/nimcp_common.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Magic number for constant-time context validation */
#define NIMCP_CT_MAGIC 0x43544D45  // "CTME" (Constant Time Memory Equal)

/** @brief Maximum string length for constant-time string comparison */
#define NIMCP_CT_MAX_STRING_LEN 4096

//=============================================================================
// Opaque Context Type
//=============================================================================

/**
 * @brief Opaque constant-time operations context
 *
 * WHAT: Context for tracking constant-time operation statistics
 * WHY:  Monitor usage and detect potential timing vulnerabilities
 * HOW:  Opaque pointer pattern for encapsulation
 */
typedef struct nimcp_ct_context* nimcp_ct_context_t;

//=============================================================================
// Statistics Structure
//=============================================================================

/**
 * @brief Constant-time operation statistics
 */
typedef struct {
    uint64_t memcmp_operations;      /**< Number of ct_memcmp calls */
    uint64_t strcmp_operations;      /**< Number of ct_strcmp calls */
    uint64_t select_operations;      /**< Number of ct_select calls */
    uint64_t lookup_operations;      /**< Number of ct_lookup calls */
    uint64_t hash_comparisons;       /**< Number of hash comparisons */
    uint64_t total_bytes_compared;   /**< Total bytes compared */
    double avg_comparison_time_ns;   /**< Average comparison time */
} nimcp_ct_stats_t;

//=============================================================================
// Context Management
//=============================================================================

/**
 * @brief Create constant-time operations context
 *
 * WHAT: Initialize context for constant-time operations
 * WHY:  Centralized statistics and configuration
 * HOW:  Allocate context with magic number validation
 *
 * @return Context handle or NULL on failure
 */
nimcp_ct_context_t nimcp_ct_create(void);

/**
 * @brief Destroy constant-time operations context
 *
 * WHAT: Clean up context and free resources
 * WHY:  Prevent memory leaks
 * HOW:  Validate magic, zero memory, free
 *
 * @param ctx Context to destroy
 */
void nimcp_ct_destroy(nimcp_ct_context_t ctx);

/**
 * @brief Shutdown constant-time module
 *
 * WHAT: Explicitly clean up global constant-time context
 * WHY:  Must be called before nimcp_memory_cleanup() to avoid double-free
 * HOW:  Destroys global context, sets to NULL so destructor skips cleanup
 *
 * Called by nimcp_shutdown() during library cleanup.
 */
void nimcp_ct_shutdown(void);

/**
 * @brief Get statistics from context
 *
 * @param ctx Context handle
 * @param stats Output statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_ct_get_stats(nimcp_ct_context_t ctx, nimcp_ct_stats_t* stats);

/**
 * @brief Reset statistics counters
 *
 * @param ctx Context handle
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_ct_reset_stats(nimcp_ct_context_t ctx);

//=============================================================================
// Constant-Time Memory Comparison
//=============================================================================

/**
 * @brief Constant-time memory comparison
 *
 * WHAT: Compare two memory regions without early exit
 * WHY:  Prevent timing attacks on hash/password verification
 * HOW:  Bitwise OR accumulation, no branches on data values
 *
 * SECURITY GUARANTEE:
 * - Execution time depends ONLY on length, not data values
 * - No early exit on mismatch (unlike memcmp)
 * - Resistant to cache-timing attacks via sequential access
 *
 * ALGORITHM:
 * ```
 * diff = 0
 * for i = 0 to len-1:
 *     diff |= (a[i] ^ b[i])
 * return (diff == 0) ? 0 : 1
 * ```
 *
 * @param a First memory region
 * @param b Second memory region
 * @param len Length in bytes
 * @return 0 if equal, non-zero if different (constant time)
 */
int nimcp_ct_memcmp(const void* a, const void* b, size_t len);

/**
 * @brief Constant-time memory comparison with context tracking
 *
 * WHAT: ct_memcmp with statistics tracking
 * WHY:  Monitor usage for security audits
 * HOW:  Wraps ct_memcmp, updates context stats
 *
 * @param ctx Context for statistics (NULL if not needed)
 * @param a First memory region
 * @param b Second memory region
 * @param len Length in bytes
 * @return 0 if equal, non-zero if different (constant time)
 */
int nimcp_ct_memcmp_tracked(nimcp_ct_context_t ctx, const void* a, const void* b, size_t len);

//=============================================================================
// Constant-Time String Comparison
//=============================================================================

/**
 * @brief Constant-time string comparison
 *
 * WHAT: Compare null-terminated strings without early exit
 * WHY:  Prevent timing attacks on password/token verification
 * HOW:  Parallel length check + constant-time memcmp
 *
 * SECURITY GUARANTEE:
 * - Execution time independent of string contents
 * - Length differences don't leak position information
 * - Resistant to prefix-based timing attacks
 *
 * ALGORITHM:
 * 1. Compute both string lengths (not constant-time, acceptable)
 * 2. If lengths differ, still compare min(len1, len2) bytes
 * 3. Return combined result of length check + byte comparison
 *
 * @param a First null-terminated string
 * @param b Second null-terminated string
 * @return 0 if equal, non-zero if different (constant time)
 */
int nimcp_ct_strcmp(const char* a, const char* b);

/**
 * @brief Constant-time bounded string comparison
 *
 * WHAT: Compare up to n bytes of strings in constant time
 * WHY:  Prevent timing attacks with length limit
 * HOW:  Bounded length computation + ct_memcmp
 *
 * @param a First null-terminated string
 * @param b Second null-terminated string
 * @param n Maximum bytes to compare
 * @return 0 if equal, non-zero if different (constant time)
 */
int nimcp_ct_strncmp(const char* a, const char* b, size_t n);

//=============================================================================
// Constant-Time Conditional Selection
//=============================================================================

/**
 * @brief Constant-time conditional select for uint8_t
 *
 * WHAT: Select between two values based on condition, without branches
 * WHY:  Prevent timing leaks in cryptographic algorithms
 * HOW:  Bitwise masking instead of if/else
 *
 * SECURITY GUARANTEE:
 * - No conditional branches on 'select' value
 * - Constant execution time regardless of inputs
 * - Compiler-resistant (uses volatile to prevent optimization)
 *
 * ALGORITHM:
 * ```
 * mask = -(select != 0)  // 0xFFFFFFFF if select!=0, else 0x00000000
 * return a ^ ((a ^ b) & mask)
 * ```
 *
 * @param a Value to return if select == 0
 * @param b Value to return if select != 0
 * @param select Selector (0 = choose a, non-zero = choose b)
 * @return a if select==0, b if select!=0 (constant time)
 */
uint8_t nimcp_ct_select_u8(uint8_t a, uint8_t b, uint8_t select);

/**
 * @brief Constant-time conditional select for uint32_t
 *
 * @param a Value to return if select == 0
 * @param b Value to return if select != 0
 * @param select Selector (0 = choose a, non-zero = choose b)
 * @return a if select==0, b if select!=0 (constant time)
 */
uint32_t nimcp_ct_select_u32(uint32_t a, uint32_t b, uint32_t select);

/**
 * @brief Constant-time conditional select for uint64_t
 *
 * @param a Value to return if select == 0
 * @param b Value to return if select != 0
 * @param select Selector (0 = choose a, non-zero = choose b)
 * @return a if select==0, b if select!=0 (constant time)
 */
uint64_t nimcp_ct_select_u64(uint64_t a, uint64_t b, uint64_t select);

/**
 * @brief Constant-time conditional select for size_t
 *
 * @param a Value to return if select == 0
 * @param b Value to return if select != 0
 * @param select Selector (0 = choose a, non-zero = choose b)
 * @return a if select==0, b if select!=0 (constant time)
 */
size_t nimcp_ct_select_size(size_t a, size_t b, uint8_t select);

//=============================================================================
// Constant-Time Array Lookup
//=============================================================================

/**
 * @brief Constant-time array lookup
 *
 * WHAT: Lookup value in array without leaking index via timing
 * WHY:  Prevent timing attacks on S-box lookups in crypto algorithms
 * HOW:  Scan entire array using conditional selection
 *
 * SECURITY GUARANTEE:
 * - Execution time independent of index value
 * - Scans entire table regardless of index
 * - Resistant to cache-timing attacks (sequential access)
 *
 * ALGORITHM:
 * ```
 * result = 0
 * for i = 0 to table_len-1:
 *     match = (i == index)
 *     result = ct_select_u8(result, table[i], match)
 * return result
 * ```
 *
 * NOTE: For out-of-bounds index, returns 0
 *
 * @param table Lookup table
 * @param table_len Table length
 * @param index Index to lookup
 * @return table[index] if index valid, else 0 (constant time)
 */
uint8_t nimcp_ct_lookup_u8(const uint8_t* table, size_t table_len, size_t index);

/**
 * @brief Constant-time array lookup for uint32_t
 *
 * @param table Lookup table
 * @param table_len Table length
 * @param index Index to lookup
 * @return table[index] if index valid, else 0 (constant time)
 */
uint32_t nimcp_ct_lookup_u32(const uint32_t* table, size_t table_len, size_t index);

//=============================================================================
// Cryptographic Hash Comparison
//=============================================================================

/**
 * @brief Constant-time hash equality check
 *
 * WHAT: Compare cryptographic hash values without timing leaks
 * WHY:  Prevent hash verification timing attacks
 * HOW:  Wrapper around ct_memcmp for hash-sized data
 *
 * COMMON HASH SIZES:
 * - SHA-256: 32 bytes
 * - SHA-512: 64 bytes
 * - SHA3-256: 32 bytes
 * - BLAKE2b: 32-64 bytes
 *
 * @param hash1 First hash value
 * @param hash2 Second hash value
 * @param hash_len Hash length in bytes
 * @return true if hashes equal, false otherwise (constant time)
 */
bool nimcp_ct_hash_equal(const uint8_t* hash1, const uint8_t* hash2, size_t hash_len);

/**
 * @brief Constant-time SHA-256 hash comparison
 *
 * WHAT: Compare 32-byte SHA-256 hashes
 * WHY:  Convenience wrapper for common hash size
 * HOW:  Fixed-size ct_hash_equal call
 *
 * @param hash1 First 32-byte hash
 * @param hash2 Second 32-byte hash
 * @return true if hashes equal, false otherwise (constant time)
 */
bool nimcp_ct_sha256_equal(const uint8_t hash1[32], const uint8_t hash2[32]);

//=============================================================================
// Timing Verification (Testing Only)
//=============================================================================

/**
 * @brief Verify operation is truly constant-time (testing utility)
 *
 * WHAT: Statistical test to verify constant-time behavior
 * WHY:  Detect timing vulnerabilities in implementations
 * HOW:  Compare execution times across many trials with different data
 *
 * ALGORITHM:
 * 1. Run operation N times with all-zeros data (baseline)
 * 2. Run operation N times with varying data
 * 3. Perform t-test to check for timing differences
 * 4. Return true if timing variance is within threshold
 *
 * NOTE: This is for testing only, not production use
 *
 * @param operation_name Name for logging
 * @param num_trials Number of trials to run
 * @param threshold_percent Acceptable timing variance (e.g., 5.0 for 5%)
 * @return true if timing is constant, false if timing leak detected
 */
bool nimcp_ct_verify_timing(const char* operation_name, size_t num_trials,
                            double threshold_percent);

//=============================================================================
// Secure Memory Wiping
//=============================================================================

/**
 * @brief Secure memory wiping (prevents compiler optimization)
 *
 * WHAT: Zero memory in a way that prevents compiler optimization
 * WHY:  Compilers may optimize away memset of unused buffers
 * HOW:  Volatile pointer or explicit_bzero when available
 *
 * SECURITY GUARANTEE:
 * - Memory is actually zeroed (not optimized away)
 * - Suitable for clearing cryptographic keys
 * - Works across all optimization levels
 *
 * @param ptr Memory to zero
 * @param len Length in bytes
 */
void nimcp_secure_zero(void* ptr, size_t len);

/**
 * @brief Secure buffer wiping with multiple passes
 *
 * WHAT: Multi-pass secure erase following DoD 5220.22-M standard
 * WHY:  Defense-in-depth against data remanence attacks
 * HOW:  Multiple overwrite passes with different patterns
 *
 * ALGORITHM:
 * - Pass 1: Write 0x00
 * - Pass 2: Write 0xFF
 * - Pass 3: Write random data
 * - Pass 4: Verify zeros
 *
 * @param ptr Memory to wipe
 * @param len Length in bytes
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_secure_wipe(void* ptr, size_t len);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_CONSTANT_TIME_H
