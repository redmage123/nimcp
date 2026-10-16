//=============================================================================
// nimcp_prime_signature.h - Prime Signature Generator for Content-Addressable Memory
//=============================================================================
/**
 * @file nimcp_prime_signature.h
 * @brief Prime factorization-based signatures for memory indexing
 *
 * WHAT: Content-addressable memory indexing using prime number signatures
 * WHY:  Enable similarity-based retrieval through unique content fingerprints
 * HOW:  Hash content to prime factor exponents, use set operations for similarity
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Content-Addressable Memory via Prime Signatures:
 *   +-----------------------------------------------------------------------+
 *   |  Each memory receives a unique "fingerprint" based on its content:   |
 *   |                                                                       |
 *   |  Content → Multiple hash functions → Prime index mapping              |
 *   |         → Exponent accumulation → 64-bit signature                   |
 *   |                                                                       |
 *   |  Biological analog: Hippocampal pattern separation                    |
 *   |  - Different inputs → orthogonal sparse codes                        |
 *   |  - Similar inputs → overlapping prime factor sets                    |
 *   |  - Retrieval via pattern completion (Jaccard matching)               |
 *   +-----------------------------------------------------------------------+
 *
 *   Prime Number Properties for Memory:
 *   +-----------------------------------------------------------------------+
 *   |  1. Unique Factorization: Every content maps to unique signature     |
 *   |  2. Composability: Memory combinations = prime factor unions          |
 *   |  3. Efficient Similarity: Jaccard on prime sets ≈ semantic overlap   |
 *   |  4. Locality Sensitivity: Similar content → overlapping primes       |
 *   +-----------------------------------------------------------------------+
 *
 * ALGORITHM:
 *
 *   Content → Signature Generation:
 *   +-----------------------------------------------------------------------+
 *   |  1. Apply multiple hash functions (MurmurHash-inspired) to content   |
 *   |  2. For each hash output:                                             |
 *   |     a. Map hash to prime indices using modulo PRIME_SIG_DIM          |
 *   |     b. Increment corresponding exponent (saturating at 255)          |
 *   |  3. Compute 64-bit hash for fast equality comparison                  |
 *   +-----------------------------------------------------------------------+
 *
 *   Similarity Computation:
 *   +-----------------------------------------------------------------------+
 *   |  Jaccard: intersection / union of prime factor multisets             |
 *   |           = sum(min(e1[i], e2[i])) / sum(max(e1[i], e2[i]))          |
 *   |                                                                       |
 *   |  Cosine:  dot(e1, e2) / (|e1| * |e2|)                                 |
 *   |           Treats exponents as vector components                       |
 *   |                                                                       |
 *   |  Hamming: Count differing bits in hash representation                 |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Signature generation: ~1us for 1KB content
 * - Jaccard similarity: ~50ns (SIMD-optimized path available)
 * - Hash comparison: ~5ns (single 64-bit compare)
 * - Memory per signature: 136 bytes
 *
 * INTEGRATION:
 * - Core: PR Memory Node stores prime_signature_t
 * - Middleware: Resonance engine uses Jaccard for content similarity
 * - API: Content-addressable query by partial signature match
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#ifndef NIMCP_PRIME_SIGNATURE_H
#define NIMCP_PRIME_SIGNATURE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Number of prime factors in signature (first 64 primes) */
#define PRIME_SIG_DIM 64

/** Number of hash iterations for content mapping */
#define PRIME_SIG_HASH_ROUNDS 16

/** Maximum exponent value (uint8_t) */
#define PRIME_SIG_MAX_EXPONENT 255

/** Epsilon for floating point comparisons */
#define PRIME_SIG_EPSILON 1e-9f

//=============================================================================
// Prime Number Table
//=============================================================================

/**
 * @brief First 64 prime numbers for signature indexing
 *
 * These primes are used to map content features to unique indices.
 * The actual values are used for hash mixing and debugging.
 */
static const uint64_t PRIMES_64[64] = {
    2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53,
    59, 61, 67, 71, 73, 79, 83, 89, 97, 101, 103, 107, 109, 113, 127, 131,
    137, 139, 149, 151, 157, 163, 167, 173, 179, 181, 191, 193, 197, 199, 211, 223,
    227, 229, 233, 239, 241, 251, 257, 263, 269, 271, 277, 281, 283, 293, 307, 311
};

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Similarity method enumeration
 */
typedef enum {
    PRIME_SIG_SIMILARITY_JACCARD = 0,  /**< Jaccard index on prime multisets */
    PRIME_SIG_SIMILARITY_COSINE  = 1,  /**< Cosine similarity on exponent vectors */
    PRIME_SIG_SIMILARITY_HAMMING = 2,  /**< Hamming distance on binary representation */
    PRIME_SIG_SIMILARITY_DICE    = 3,  /**< Dice coefficient (2*intersection / sum) */
    PRIME_SIG_SIMILARITY_OVERLAP = 4   /**< Overlap coefficient (intersection / min_size) */
} prime_sig_similarity_method_t;

/**
 * @brief Prime signature for content-addressable memory
 *
 * Represents content as a multiset of prime factors where each prime
 * has an exponent indicating its "weight" in the content.
 *
 * Memory layout: 136 bytes total
 * - primes: 64 bytes (8 * 64 bits, but actually just indices)
 * - exponents: 64 bytes (1 * 64 bytes)
 * - hash: 8 bytes
 * - num_factors: 4 bytes
 */
typedef struct {
    uint64_t primes[PRIME_SIG_DIM];    /**< Prime values at each index (const reference) */
    uint8_t exponents[PRIME_SIG_DIM];  /**< Exponents for each prime (0-255) */
    uint64_t hash;                      /**< 64-bit hash for fast comparison */
    uint32_t num_factors;               /**< Number of non-zero exponents */
} prime_signature_t;

/**
 * @brief Configuration for signature generation
 */
typedef struct {
    uint32_t hash_rounds;        /**< Number of hash iterations (default: 16) */
    uint32_t seed;               /**< Random seed for hash functions */
    bool normalize_exponents;    /**< Normalize exponents after generation */
    float sparsity_target;       /**< Target fraction of non-zero exponents (0-1) */
} prime_sig_config_t;

//=============================================================================
// Creation Functions
//=============================================================================

/**
 * @brief Create an empty (zero) signature
 *
 * Creates a signature with all exponents set to zero.
 * Useful as a starting point for incremental building.
 *
 * @return Pointer to new signature, or NULL on allocation failure
 *
 * Performance: ~100ns (allocation + zeroing)
 *
 * Example:
 *   prime_signature_t* sig = prime_sig_create();
 *   if (sig) {
 *       // Use signature...
 *       prime_sig_destroy(sig);
 *   }
 */
NIMCP_EXPORT prime_signature_t* prime_sig_create(void);

/**
 * @brief Generate signature from raw byte content
 *
 * Applies multiple hash functions to content and maps results to
 * prime factor exponents.
 *
 * @param data Pointer to content bytes
 * @param size Number of bytes
 * @return Pointer to new signature, or NULL on error
 *
 * Performance: ~1us per KB of content
 *
 * Algorithm:
 *   1. Initialize all exponents to 0
 *   2. For each hash round (default 16):
 *      a. Compute MurmurHash variant with round-specific seed
 *      b. Map hash to prime index: idx = hash % PRIME_SIG_DIM
 *      c. Increment exponent at idx (saturating at 255)
 *   3. Compute final 64-bit hash for fast comparison
 *
 * Example:
 *   const char* text = "Hello, World!";
 *   prime_signature_t* sig = prime_sig_from_content(text, strlen(text));
 */
NIMCP_EXPORT prime_signature_t* prime_sig_from_content(const void* data, size_t size);

/**
 * @brief Generate signature from text with semantic preprocessing
 *
 * Specialized text hashing that provides better locality sensitivity
 * for natural language content. Processes character n-grams.
 *
 * @param text Null-terminated text string
 * @return Pointer to new signature, or NULL on error
 *
 * Performance: ~2us per KB of text
 *
 * Algorithm:
 *   1. Extract character 3-grams from text
 *   2. Hash each n-gram and map to prime indices
 *   3. Weight by position (earlier = higher weight)
 *   4. Compute final hash
 */
NIMCP_EXPORT prime_signature_t* prime_sig_from_text(const char* text);

/**
 * @brief Generate signature from float array (feature vectors)
 *
 * Converts floating-point feature vectors to prime signatures,
 * useful for neural network embeddings or sensor data.
 *
 * @param floats Pointer to float array
 * @param count Number of floats
 * @return Pointer to new signature, or NULL on error
 *
 * Performance: ~500ns + 50ns per float
 *
 * Algorithm:
 *   1. Quantize floats to discrete bins
 *   2. Map each bin to prime index
 *   3. Accumulate counts as exponents
 */
NIMCP_EXPORT prime_signature_t* prime_sig_from_floats(const float* floats, size_t count);

/**
 * @brief Generate signature with custom configuration
 *
 * @param data Pointer to content bytes
 * @param size Number of bytes
 * @param config Configuration options
 * @return Pointer to new signature, or NULL on error
 */
NIMCP_EXPORT prime_signature_t* prime_sig_from_content_config(
    const void* data,
    size_t size,
    const prime_sig_config_t* config
);

/**
 * @brief Deep copy a signature
 *
 * @param sig Source signature
 * @return Pointer to new signature copy, or NULL on error
 *
 * Performance: ~100ns
 */
NIMCP_EXPORT prime_signature_t* prime_sig_copy(const prime_signature_t* sig);

/**
 * @brief Free signature memory
 *
 * @param sig Signature to destroy (NULL safe)
 *
 * Performance: ~50ns
 */
NIMCP_EXPORT void prime_sig_destroy(prime_signature_t* sig);

/**
 * @brief Get default configuration
 *
 * @return Default configuration values
 */
NIMCP_EXPORT prime_sig_config_t prime_sig_default_config(void);

//=============================================================================
// Similarity Functions
//=============================================================================

/**
 * @brief Compute Jaccard similarity between two signatures
 *
 * Jaccard index treats signatures as multisets and computes:
 * J(A,B) = |A ∩ B| / |A ∪ B|
 *        = sum(min(e1[i], e2[i])) / sum(max(e1[i], e2[i]))
 *
 * @param s1 First signature
 * @param s2 Second signature
 * @return Jaccard similarity in range [0, 1], or -1 on error
 *
 * Performance: ~50ns (SIMD), ~200ns (scalar)
 *
 * Properties:
 * - Identical signatures: returns 1.0
 * - Disjoint signatures: returns 0.0
 * - Empty signatures (both zero): returns 0.0 (defined, not undefined)
 *
 * Example:
 *   float sim = prime_sig_jaccard(sig1, sig2);
 *   if (sim > 0.8f) {
 *       // Highly similar content
 *   }
 */
NIMCP_EXPORT float prime_sig_jaccard(const prime_signature_t* s1,
                                      const prime_signature_t* s2);

/**
 * @brief Compute cosine similarity between signature exponent vectors
 *
 * Treats exponent arrays as vectors and computes:
 * cos(θ) = (e1 · e2) / (|e1| × |e2|)
 *
 * @param s1 First signature
 * @param s2 Second signature
 * @return Cosine similarity in range [0, 1], or -1 on error
 *
 * Performance: ~60ns (SIMD), ~250ns (scalar)
 *
 * Note: Returns 0 for exponents, which are always non-negative,
 *       so cosine similarity is always in [0, 1] not [-1, 1].
 */
NIMCP_EXPORT float prime_sig_cosine(const prime_signature_t* s1,
                                     const prime_signature_t* s2);

/**
 * @brief Compute Hamming distance on binary representation
 *
 * Counts the number of prime indices where one signature has
 * a non-zero exponent and the other has zero.
 *
 * @param s1 First signature
 * @param s2 Second signature
 * @return Hamming distance in range [0, PRIME_SIG_DIM], or -1 on error
 *
 * Performance: ~30ns
 *
 * Note: This is a distance, not similarity. Lower = more similar.
 */
NIMCP_EXPORT int prime_sig_hamming(const prime_signature_t* s1,
                                    const prime_signature_t* s2);

/**
 * @brief Compute similarity using specified method
 *
 * Generic similarity function that dispatches to appropriate method.
 *
 * @param s1 First signature
 * @param s2 Second signature
 * @param method Similarity method to use
 * @return Similarity value (meaning depends on method), or -1 on error
 *
 * Performance: Varies by method
 */
NIMCP_EXPORT float prime_sig_similarity(const prime_signature_t* s1,
                                         const prime_signature_t* s2,
                                         prime_sig_similarity_method_t method);

//=============================================================================
// Set Operations
//=============================================================================

/**
 * @brief Compose two signatures (union of prime factors)
 *
 * Creates a new signature with max(e1[i], e2[i]) at each index.
 * Represents the "combination" of two memory contents.
 *
 * @param s1 First signature
 * @param s2 Second signature
 * @return New composed signature, or NULL on error
 *
 * Performance: ~100ns
 *
 * Example:
 *   // Combine two related memories
 *   prime_signature_t* combined = prime_sig_compose(mem1_sig, mem2_sig);
 */
NIMCP_EXPORT prime_signature_t* prime_sig_compose(const prime_signature_t* s1,
                                                   const prime_signature_t* s2);

/**
 * @brief Intersect two signatures (common prime factors)
 *
 * Creates a new signature with min(e1[i], e2[i]) at each index.
 * Represents the "shared content" between two memories.
 *
 * @param s1 First signature
 * @param s2 Second signature
 * @return New intersected signature, or NULL on error
 *
 * Performance: ~100ns
 */
NIMCP_EXPORT prime_signature_t* prime_sig_intersect(const prime_signature_t* s1,
                                                     const prime_signature_t* s2);

/**
 * @brief Symmetric difference of two signatures
 *
 * Creates a new signature with |e1[i] - e2[i]| at each index.
 * Represents what's "unique" to each memory.
 *
 * @param s1 First signature
 * @param s2 Second signature
 * @return New difference signature, or NULL on error
 *
 * Performance: ~100ns
 */
NIMCP_EXPORT prime_signature_t* prime_sig_difference(const prime_signature_t* s1,
                                                      const prime_signature_t* s2);

/**
 * @brief Subtract one signature from another
 *
 * Creates a new signature with max(0, e1[i] - e2[i]) at each index.
 * Represents "s1 with s2's content removed".
 *
 * @param s1 First signature (minuend)
 * @param s2 Second signature (subtrahend)
 * @return New result signature, or NULL on error
 *
 * Performance: ~100ns
 */
NIMCP_EXPORT prime_signature_t* prime_sig_subtract(const prime_signature_t* s1,
                                                    const prime_signature_t* s2);

//=============================================================================
// Hashing Functions
//=============================================================================

/**
 * @brief Compute or recompute 64-bit hash for signature
 *
 * The hash is automatically computed during signature creation,
 * but this function can be used to recompute after modifications.
 *
 * @param sig Signature to hash
 * @return 64-bit hash value, or 0 if sig is NULL
 *
 * Performance: ~30ns
 *
 * Algorithm: FNV-1a over exponent array
 */
NIMCP_EXPORT uint64_t prime_sig_hash(const prime_signature_t* sig);

/**
 * @brief Fast hash-based equality check
 *
 * Compares only the 64-bit hashes. Fast but may have false positives
 * (different signatures with same hash). Use prime_sig_equal() for
 * definitive comparison.
 *
 * @param s1 First signature
 * @param s2 Second signature
 * @return true if hashes match, false otherwise
 *
 * Performance: ~5ns
 */
NIMCP_EXPORT bool prime_sig_hash_equal(const prime_signature_t* s1,
                                        const prime_signature_t* s2);

/**
 * @brief Full equality check between signatures
 *
 * Compares all exponents for exact equality. Use this when
 * hash comparison indicates possible match.
 *
 * @param s1 First signature
 * @param s2 Second signature
 * @return true if all exponents match exactly, false otherwise
 *
 * Performance: ~40ns
 */
NIMCP_EXPORT bool prime_sig_equal(const prime_signature_t* s1,
                                   const prime_signature_t* s2);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Check if signature is empty (all exponents zero)
 *
 * @param sig Signature to check
 * @return true if all exponents are zero, false otherwise
 *
 * Performance: ~20ns (early exit on first non-zero)
 */
NIMCP_EXPORT bool prime_sig_is_empty(const prime_signature_t* sig);

/**
 * @brief Count number of non-zero exponents
 *
 * Returns the cached num_factors value. Use prime_sig_recount_factors()
 * if signature was modified externally.
 *
 * @param sig Signature to check
 * @return Number of prime indices with non-zero exponents
 *
 * Performance: ~5ns (returns cached value)
 */
NIMCP_EXPORT uint32_t prime_sig_count_factors(const prime_signature_t* sig);

/**
 * @brief Recount and update num_factors field
 *
 * @param sig Signature to update
 * @return Updated factor count
 *
 * Performance: ~30ns
 */
NIMCP_EXPORT uint32_t prime_sig_recount_factors(prime_signature_t* sig);

/**
 * @brief Convert signature to human-readable string
 *
 * Format: "PrimeSig[hash=0x..., factors=N, exponents=(p0^e0, p1^e1, ...)]"
 * Only non-zero exponents are shown.
 *
 * @param sig Signature to convert
 * @param buf Output buffer
 * @param size Buffer size
 * @return Number of characters written (excluding null terminator),
 *         or required size if buf is NULL
 *
 * Performance: ~500ns
 */
NIMCP_EXPORT size_t prime_sig_to_string(const prime_signature_t* sig,
                                         char* buf,
                                         size_t size);

/**
 * @brief Print signature to stdout for debugging
 *
 * @param sig Signature to print
 *
 * Performance: ~1us (includes I/O)
 */
NIMCP_EXPORT void prime_sig_print(const prime_signature_t* sig);

/**
 * @brief Get exponent at specific prime index
 *
 * @param sig Signature to query
 * @param index Prime index (0 to PRIME_SIG_DIM-1)
 * @return Exponent value, or 0 if index out of range
 *
 * Performance: ~3ns
 */
NIMCP_EXPORT uint8_t prime_sig_get_exponent(const prime_signature_t* sig,
                                             size_t index);

/**
 * @brief Set exponent at specific prime index
 *
 * Updates the exponent and recalculates hash and factor count.
 *
 * @param sig Signature to modify
 * @param index Prime index (0 to PRIME_SIG_DIM-1)
 * @param value New exponent value
 * @return true on success, false if index out of range
 *
 * Performance: ~40ns (includes hash recomputation)
 */
NIMCP_EXPORT bool prime_sig_set_exponent(prime_signature_t* sig,
                                          size_t index,
                                          uint8_t value);

/**
 * @brief Compute total "weight" of signature
 *
 * Returns sum of all exponents, useful for normalization.
 *
 * @param sig Signature to measure
 * @return Sum of all exponents
 *
 * Performance: ~30ns
 */
NIMCP_EXPORT uint32_t prime_sig_total_weight(const prime_signature_t* sig);

/**
 * @brief Normalize signature exponents
 *
 * Scales all exponents proportionally so the maximum is 255.
 * Useful after operations that may have reduced magnitudes.
 *
 * @param sig Signature to normalize (modified in place)
 * @return true on success, false if signature is empty or NULL
 *
 * Performance: ~60ns
 */
NIMCP_EXPORT bool prime_sig_normalize(prime_signature_t* sig);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PRIME_SIGNATURE_H
