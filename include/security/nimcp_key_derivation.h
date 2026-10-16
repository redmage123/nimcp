/**
 * @file nimcp_key_derivation.h
 * @brief Key derivation functions for NIMCP cryptographic operations
 *
 * WHAT: Secure key derivation using Argon2id and PBKDF2-HMAC-SHA256
 * WHY:  Transform passwords/passphrases into cryptographic keys resistant to brute-force
 * HOW:  Memory-hard algorithms that resist GPU/ASIC attacks
 *
 * CRYPTOGRAPHIC BACKGROUND:
 *
 * KEY DERIVATION FUNCTIONS (KDFs):
 * Transform low-entropy passwords into high-entropy cryptographic keys through
 * computational hardness. Essential for:
 * - Password-based encryption
 * - Key stretching for authentication
 * - Deriving multiple keys from one master secret
 *
 * ARGON2ID (Recommended):
 * - Winner of Password Hashing Competition (PHC) 2015
 * - Hybrid mode: combines data-independent (Argon2i) and data-dependent (Argon2d) approaches
 * - Memory-hard: requires substantial RAM, making GPU/ASIC attacks expensive
 * - Configurable time, memory, and parallelism costs
 * - Resistant to side-channel attacks (constant-time operations)
 *
 * PBKDF2-HMAC-SHA256 (Fallback):
 * - NIST SP 800-132 standard, widely supported
 * - Simpler than Argon2, but less resistant to dedicated hardware
 * - Good for compatibility when Argon2 unavailable
 * - Uses iterative HMAC to increase computational cost
 *
 * SECURITY PARAMETERS:
 * - Memory Cost: Higher = more resistant to parallel attacks (GPUs/ASICs)
 * - Time Cost: Higher = more iterations, slower but more secure
 * - Parallelism: Number of threads (Argon2 only)
 * - Salt: Random unique value preventing rainbow table attacks
 *
 * BIOLOGICAL ANALOGY:
 * Like the brain's synaptic consolidation - converting short-term weak memories
 * (passwords) into long-term strong memories (keys) through repeated rehearsal
 * (iterations) and distributed storage (memory-hardness). Just as memories become
 * harder to forge with more consolidation, derived keys become harder to crack
 * with more iterations and memory.
 *
 * USAGE EXAMPLE:
 * ```c
 * // Create KDF context with Argon2id
 * nimcp_kdf_config_t config = {
 *     .algorithm = NIMCP_KDF_ARGON2ID,
 *     .memory_kb = 65536,      // 64 MB
 *     .iterations = 3,          // Time cost
 *     .parallelism = 4          // 4 threads
 * };
 *
 * nimcp_kdf_context_t ctx = nimcp_kdf_create(&config);
 *
 * // Generate random salt
 * uint8_t salt[16];
 * nimcp_kdf_generate_salt(salt, sizeof(salt));
 *
 * // Derive 32-byte key from password
 * uint8_t key[32];
 * nimcp_kdf_derive(ctx, "my_password", 11, salt, sizeof(salt), key, sizeof(key));
 *
 * // Use key for encryption...
 *
 * // Clean up (securely wipes memory)
 * nimcp_kdf_destroy(ctx);
 * ```
 *
 * @author NIMCP Security Team
 * @date 2025-12-07
 * @version 1.0.0
 */

#ifndef NIMCP_KEY_DERIVATION_H
#define NIMCP_KEY_DERIVATION_H

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

/** @brief Magic number for KDF context validation */
#define NIMCP_KDF_MAGIC 0x4B444632  // "KDF2"

/** @brief Minimum salt length (128 bits recommended by NIST) */
#define NIMCP_KDF_MIN_SALT_LEN 16

/** @brief Recommended salt length (256 bits) */
#define NIMCP_KDF_RECOMMENDED_SALT_LEN 32

/** @brief Maximum password length */
#define NIMCP_KDF_MAX_PASSWORD_LEN 1024

/** @brief Maximum derived key length */
#define NIMCP_KDF_MAX_KEY_LEN 1024

/** @brief Default Argon2 memory cost (64 MB) */
#define NIMCP_KDF_DEFAULT_MEMORY_KB 65536

/** @brief Default Argon2 time cost (iterations) */
#define NIMCP_KDF_DEFAULT_ITERATIONS 3

/** @brief Default Argon2 parallelism */
#define NIMCP_KDF_DEFAULT_PARALLELISM 4

/** @brief Default PBKDF2 iterations (NIST minimum: 10,000 for HMAC-SHA256) */
#define NIMCP_KDF_PBKDF2_MIN_ITERATIONS 10000

/** @brief Recommended PBKDF2 iterations (100,000+) */
#define NIMCP_KDF_PBKDF2_RECOMMENDED_ITERATIONS 100000

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Key derivation algorithm types
 *
 * WHAT: Supported KDF algorithms
 * WHY:  Different algorithms for different security/performance tradeoffs
 * HOW:  Enumeration for algorithm selection
 */
typedef enum {
    NIMCP_KDF_ARGON2ID = 0,        /**< Argon2id (recommended, memory-hard) */
    NIMCP_KDF_PBKDF2_SHA256 = 1,   /**< PBKDF2-HMAC-SHA256 (fallback) */
    NIMCP_KDF_ALGORITHM_COUNT = 2
} nimcp_kdf_algorithm_t;

//=============================================================================
// Opaque Context Type
//=============================================================================

/**
 * @brief Opaque key derivation context
 *
 * WHAT: Context for key derivation operations
 * WHY:  Encapsulates algorithm state, configuration, and statistics
 * HOW:  Opaque pointer pattern for information hiding
 */
typedef struct nimcp_kdf_context* nimcp_kdf_context_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief Key derivation configuration
 *
 * WHAT: Parameters for KDF algorithm
 * WHY:  Configurable security/performance tradeoff
 * HOW:  Structure with algorithm-specific parameters
 *
 * PARAMETER GUIDELINES:
 *
 * ARGON2ID:
 * - memory_kb: 64MB (desktop), 16MB (mobile), 1GB+ (server)
 * - iterations: 3 (balanced), 1 (fast), 10+ (paranoid)
 * - parallelism: Number of CPU cores available
 *
 * PBKDF2:
 * - iterations: 100,000 (minimum), 1,000,000+ (recommended 2025)
 * - memory_kb: Ignored (PBKDF2 not memory-hard)
 * - parallelism: Ignored
 */
typedef struct {
    nimcp_kdf_algorithm_t algorithm;  /**< KDF algorithm to use */
    uint32_t memory_kb;               /**< Memory cost in KB (Argon2 only) */
    uint32_t iterations;              /**< Time cost / iterations */
    uint32_t parallelism;             /**< Parallel lanes (Argon2 only) */
    bool enable_logging;              /**< Log derivation operations */
    bool enable_statistics;           /**< Track timing statistics */
} nimcp_kdf_config_t;

//=============================================================================
// Statistics Structure
//=============================================================================

/**
 * @brief Key derivation statistics
 */
typedef struct {
    uint64_t derivations_performed;   /**< Total key derivations */
    uint64_t total_bytes_derived;     /**< Total output bytes */
    uint64_t salts_generated;         /**< Random salts generated */
    double avg_derivation_time_ms;    /**< Average derivation time */
    double max_derivation_time_ms;    /**< Maximum derivation time */
    double total_derivation_time_ms;  /**< Total time spent deriving */
    nimcp_kdf_algorithm_t algorithm;  /**< Algorithm in use */
    uint32_t current_memory_kb;       /**< Current memory cost */
    uint32_t current_iterations;      /**< Current iterations */
} nimcp_kdf_stats_t;

//=============================================================================
// Context Management
//=============================================================================

/**
 * @brief Get default KDF configuration
 *
 * WHAT: Returns recommended default parameters
 * WHY:  Simplify usage with secure defaults
 * HOW:  Static configuration with current best practices
 *
 * DEFAULTS:
 * - Algorithm: Argon2id
 * - Memory: 64 MB
 * - Iterations: 3
 * - Parallelism: 4
 *
 * @return Default configuration structure
 */
nimcp_kdf_config_t nimcp_kdf_default_config(void);

/**
 * @brief Create key derivation context
 *
 * WHAT: Initialize KDF with specified configuration
 * WHY:  Prepare for key derivation operations
 * HOW:  Allocate context, validate parameters, register with bio-async
 *
 * VALIDATION:
 * - memory_kb: Must be >= 8192 (8 MB minimum for Argon2)
 * - iterations: Must be >= 1 for Argon2, >= 10000 for PBKDF2
 * - parallelism: Must be >= 1, <= 64
 *
 * @param config Configuration (NULL for defaults)
 * @return Context handle or NULL on failure
 */
nimcp_kdf_context_t nimcp_kdf_create(const nimcp_kdf_config_t* config);

/**
 * @brief Destroy key derivation context
 *
 * WHAT: Clean up context and securely wipe memory
 * WHY:  Prevent key material leakage after use
 * HOW:  Secure zero internal buffers, unregister bio-async, free memory
 *
 * SECURITY:
 * - All internal buffers are securely wiped
 * - Derived keys in caller's memory must be wiped separately
 * - Magic number is cleared to prevent use-after-free
 *
 * @param ctx Context to destroy
 */
void nimcp_kdf_destroy(nimcp_kdf_context_t ctx);

/**
 * @brief Update KDF configuration
 *
 * WHAT: Change algorithm parameters after creation
 * WHY:  Adjust security/performance tradeoff dynamically
 * HOW:  Validate new parameters, update context
 *
 * NOTE: Does not affect previously derived keys
 *
 * @param ctx Context handle
 * @param config New configuration
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_kdf_update_config(nimcp_kdf_context_t ctx,
                                       const nimcp_kdf_config_t* config);

//=============================================================================
// Key Derivation Operations
//=============================================================================

/**
 * @brief Derive cryptographic key from password
 *
 * WHAT: Transform password into cryptographic key using configured KDF
 * WHY:  Generate high-entropy keys from low-entropy passwords
 * HOW:  Apply Argon2id or PBKDF2 with salt and configured parameters
 *
 * SECURITY REQUIREMENTS:
 * - password: User-provided passphrase (entropy varies)
 * - salt: MUST be random, unique per derivation (use nimcp_kdf_generate_salt)
 * - key_out: Output buffer (caller must securely wipe after use)
 * - key_len: Desired key length (typically 32 bytes for AES-256)
 *
 * TIMING:
 * - Argon2id (64MB, t=3): ~100-500ms depending on CPU
 * - PBKDF2 (100k iterations): ~50-200ms
 * - Time increases linearly with iterations/memory
 *
 * ALGORITHM SELECTION:
 * - Argon2id: Use when memory-hardness is important (most cases)
 * - PBKDF2: Use for compatibility or when memory is constrained
 *
 * @param ctx Context handle
 * @param password Password/passphrase
 * @param password_len Password length in bytes
 * @param salt Random salt
 * @param salt_len Salt length (minimum 16 bytes, recommended 32)
 * @param key_out Output buffer for derived key
 * @param key_len Desired key length in bytes
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_kdf_derive(
    nimcp_kdf_context_t ctx,
    const char* password,
    size_t password_len,
    const uint8_t* salt,
    size_t salt_len,
    uint8_t* key_out,
    size_t key_len
);

/**
 * @brief Derive key with additional associated data
 *
 * WHAT: KDF with additional context/domain separation
 * WHY:  Prevent key reuse across different contexts
 * HOW:  Include additional data in derivation (Argon2 AD parameter)
 *
 * USE CASES:
 * - Domain separation: "encryption", "authentication", etc.
 * - Protocol binding: Include protocol version
 * - User binding: Include username/userid
 *
 * NOTE: PBKDF2 doesn't support AD directly; it's concatenated with password
 *
 * @param ctx Context handle
 * @param password Password/passphrase
 * @param password_len Password length
 * @param salt Random salt
 * @param salt_len Salt length
 * @param ad Additional associated data (can be NULL)
 * @param ad_len Associated data length
 * @param key_out Output buffer for derived key
 * @param key_len Desired key length
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_kdf_derive_with_ad(
    nimcp_kdf_context_t ctx,
    const char* password,
    size_t password_len,
    const uint8_t* salt,
    size_t salt_len,
    const uint8_t* ad,
    size_t ad_len,
    uint8_t* key_out,
    size_t key_len
);

//=============================================================================
// Salt Generation
//=============================================================================

/**
 * @brief Generate cryptographically secure random salt
 *
 * WHAT: Generate random salt for key derivation
 * WHY:  Prevent rainbow table attacks, ensure uniqueness
 * HOW:  Use platform CSPRNG (getrandom, BCryptGenRandom, etc.)
 *
 * SECURITY:
 * - Uses cryptographically secure RNG
 * - Each salt must be unique per derivation
 * - Salt does NOT need to be secret (but must be stored)
 * - Minimum 16 bytes, recommended 32 bytes
 *
 * PLATFORMS:
 * - Linux: getrandom() syscall
 * - Windows: BCryptGenRandom()
 * - BSD/macOS: arc4random_buf()
 * - Fallback: /dev/urandom
 *
 * @param salt Output buffer for salt
 * @param salt_len Salt length (minimum 16, recommended 32)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_kdf_generate_salt(uint8_t* salt, size_t salt_len);

//=============================================================================
// Statistics and Monitoring
//=============================================================================

/**
 * @brief Get KDF statistics
 *
 * @param ctx Context handle
 * @param stats Output statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_kdf_get_stats(nimcp_kdf_context_t ctx, nimcp_kdf_stats_t* stats);

/**
 * @brief Reset statistics counters
 *
 * @param ctx Context handle
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_kdf_reset_stats(nimcp_kdf_context_t ctx);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get algorithm name as string
 *
 * @param algorithm Algorithm type
 * @return Algorithm name string
 */
const char* nimcp_kdf_algorithm_name(nimcp_kdf_algorithm_t algorithm);

/**
 * @brief Verify KDF parameters are secure
 *
 * WHAT: Check if configuration meets security requirements
 * WHY:  Prevent weak parameter choices
 * HOW:  Validate against current best practices (2025)
 *
 * CHECKS:
 * - Argon2: memory >= 64MB, iterations >= 3, parallelism >= 1
 * - PBKDF2: iterations >= 100,000
 * - Salt length >= 16 bytes
 *
 * @param config Configuration to validate
 * @param salt_len Salt length to validate
 * @return true if parameters are secure, false if weak
 */
bool nimcp_kdf_verify_params(const nimcp_kdf_config_t* config, size_t salt_len);

/**
 * @brief Estimate derivation time
 *
 * WHAT: Predict how long derivation will take
 * WHY:  Help users tune parameters for acceptable UX
 * HOW:  Run quick benchmark with current parameters
 *
 * NOTE: Estimates may vary by ±50% depending on system load
 *
 * @param ctx Context handle
 * @param estimated_ms Output: estimated time in milliseconds
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_kdf_estimate_time(nimcp_kdf_context_t ctx, double* estimated_ms);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_KEY_DERIVATION_H
