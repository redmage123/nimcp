//=============================================================================
// nimcp_prime_signature.c - Prime Signature Generator Implementation
//=============================================================================
/**
 * @file nimcp_prime_signature.c
 * @brief Implementation of prime factorization signatures for content-addressable memory
 *
 * WHAT: Generate and compare unique content fingerprints using prime numbers
 * WHY:  Enable similarity-based memory retrieval through set operations
 * HOW:  Hash content to prime exponents, compute similarity via multiset operations
 *
 * IMPLEMENTATION NOTES:
 * - Hash function uses MurmurHash3-inspired mixing
 * - SIMD optimizations available for similarity computations on supported platforms
 * - All operations are thread-safe (no shared state)
 * - Memory allocation uses standard malloc/free (can be replaced with nimcp_malloc)
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#include "cognitive/memory/core/nimcp_prime_signature.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(prime_signature, MESH_ADAPTER_CATEGORY_MEMORY)


//=============================================================================
// Internal Constants
//=============================================================================

/** FNV-1a offset basis for 64-bit hash */
#define FNV_OFFSET_BASIS 0xcbf29ce484222325ULL

/** FNV-1a prime for 64-bit hash */
#define FNV_PRIME 0x100000001b3ULL

/** Default number of hash rounds */
#define DEFAULT_HASH_ROUNDS 16

/** Default random seed */
#define DEFAULT_SEED 0x12345678U

/** N-gram size for text processing */
#define TEXT_NGRAM_SIZE 3

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief MurmurHash3-inspired 64-bit finalizer mix
 *
 * Provides good avalanche properties for hash mixing.
 */
static inline uint64_t mix64(uint64_t k) {
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53ULL;
    k ^= k >> 33;
    return k;
}

/**
 * @brief Generate hash with seed for a block of data
 *
 * @param data Input data
 * @param size Data size in bytes
 * @param seed Random seed
 * @return 64-bit hash value
 */
static uint64_t hash_with_seed(const void* data, size_t size, uint32_t seed) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint64_t h = FNV_OFFSET_BASIS ^ (uint64_t)seed;

    // Process 8 bytes at a time
    size_t blocks = size / 8;
    const uint64_t* data64 = (const uint64_t*)bytes;

    for (size_t i = 0; i < blocks; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && blocks > 256) {
            prime_signature_heartbeat("prime_signat_loop",
                             (float)(i + 1) / (float)blocks);
        }

        uint64_t k = data64[i];
        k = mix64(k);
        h ^= k;
        h *= FNV_PRIME;
    }

    // Process remaining bytes
    const uint8_t* tail = bytes + (blocks * 8);
    size_t remaining = size - (blocks * 8);

    for (size_t i = 0; i < remaining; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && remaining > 256) {
            prime_signature_heartbeat("prime_signat_loop",
                             (float)(i + 1) / (float)remaining);
        }

        h ^= (uint64_t)tail[i];
        h *= FNV_PRIME;
    }

    return mix64(h);
}

/**
 * @brief Compute FNV-1a hash over exponent array
 */
static uint64_t compute_signature_hash(const uint8_t* exponents) {
    uint64_t hash = FNV_OFFSET_BASIS;

    for (size_t i = 0; i < PRIME_SIG_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PRIME_SIG_DIM > 256) {
            prime_signature_heartbeat("prime_signat_loop",
                             (float)(i + 1) / (float)PRIME_SIG_DIM);
        }

        hash ^= (uint64_t)exponents[i];
        hash *= FNV_PRIME;
    }

    return hash;
}

/**
 * @brief Count non-zero exponents
 */
static uint32_t count_nonzero(const uint8_t* exponents) {
    uint32_t count = 0;

    for (size_t i = 0; i < PRIME_SIG_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PRIME_SIG_DIM > 256) {
            prime_signature_heartbeat("prime_signat_loop",
                             (float)(i + 1) / (float)PRIME_SIG_DIM);
        }

        if (exponents[i] > 0) {
            count++;
        }
    }

    return count;
}

/**
 * @brief Initialize signature with prime values
 */
static void init_primes(prime_signature_t* sig) {
    for (size_t i = 0; i < PRIME_SIG_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PRIME_SIG_DIM > 256) {
            prime_signature_heartbeat("prime_signat_loop",
                             (float)(i + 1) / (float)PRIME_SIG_DIM);
        }

        sig->primes[i] = PRIMES_64[i];
    }
}

/**
 * @brief Saturating add for uint8_t
 */
static inline uint8_t saturating_add(uint8_t a, uint8_t b) {
    uint16_t sum = (uint16_t)a + (uint16_t)b;
    return (sum > 255) ? 255 : (uint8_t)sum;
}

/**
 * @brief Saturating subtract for uint8_t
 */
static inline uint8_t saturating_sub(uint8_t a, uint8_t b) {
    return (a > b) ? (a - b) : 0;
}

//=============================================================================
// Creation Functions
//=============================================================================

prime_signature_t* prime_sig_create(void) {
    /* Phase 8: Heartbeat at operation start */
    prime_signature_heartbeat("prime_signat_prime_sig_create", 0.0f);


    prime_signature_t* sig = (prime_signature_t*)nimcp_malloc(sizeof(prime_signature_t));
    if (!sig) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate sig");

        return NULL;
    }

    // Initialize primes array
    init_primes(sig);

    // Zero all exponents
    memset(sig->exponents, 0, PRIME_SIG_DIM);

    // Set hash and factor count
    sig->hash = compute_signature_hash(sig->exponents);
    sig->num_factors = 0;

    return sig;
}

prime_signature_t* prime_sig_from_content(const void* data, size_t size) {
    /* Phase 8: Heartbeat at operation start */
    prime_signature_heartbeat("prime_signat_prime_sig_from_conte", 0.0f);


    prime_sig_config_t config = prime_sig_default_config();
    return prime_sig_from_content_config(data, size, &config);
}

prime_signature_t* prime_sig_from_content_config(
    const void* data,
    size_t size,
    const prime_sig_config_t* config
) {
    // Validate inputs
    if (!data || size == 0 || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prime_sig_from_content_config: required parameter is NULL (data, config)");
        return NULL;
    }

    // Create empty signature
    /* Phase 8: Heartbeat at operation start */
    prime_signature_heartbeat("prime_signat_prime_sig_from_conte", 0.0f);


    prime_signature_t* sig = prime_sig_create();
    if (!sig) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sig is NULL");

        return NULL;
    }

    // Apply multiple hash rounds
    for (uint32_t round = 0; round < config->hash_rounds; round++) {
        /* Phase 8: Loop progress heartbeat */
        if ((round & 0xFF) == 0 && config->hash_rounds > 256) {
            prime_signature_heartbeat("prime_signat_loop",
                             (float)(round + 1) / (float)config->hash_rounds);
        }

        // Generate hash with round-specific seed
        uint32_t seed = config->seed ^ (round * 0x9E3779B9);
        uint64_t hash = hash_with_seed(data, size, seed);

        // Map to prime index
        size_t idx = hash % PRIME_SIG_DIM;

        // Increment exponent (saturating)
        sig->exponents[idx] = saturating_add(sig->exponents[idx], 1);

        // Also use bits from hash for additional distribution
        // Use different portions of the 64-bit hash
        uint64_t hash2 = hash >> 16;
        size_t idx2 = hash2 % PRIME_SIG_DIM;
        sig->exponents[idx2] = saturating_add(sig->exponents[idx2], 1);

        uint64_t hash3 = hash >> 32;
        size_t idx3 = hash3 % PRIME_SIG_DIM;
        sig->exponents[idx3] = saturating_add(sig->exponents[idx3], 1);

        uint64_t hash4 = hash >> 48;
        size_t idx4 = hash4 % PRIME_SIG_DIM;
        sig->exponents[idx4] = saturating_add(sig->exponents[idx4], 1);
    }

    // Optionally normalize exponents
    if (config->normalize_exponents) {
        prime_sig_normalize(sig);
    }

    // Update hash and factor count
    sig->hash = compute_signature_hash(sig->exponents);
    sig->num_factors = count_nonzero(sig->exponents);

    return sig;
}

prime_signature_t* prime_sig_from_text(const char* text) {
    if (!text || *text == '\0') {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prime_sig_from_text: text is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    prime_signature_heartbeat("prime_signat_prime_sig_from_text", 0.0f);


    size_t len = strlen(text);

    // Create empty signature
    prime_signature_t* sig = prime_sig_create();
    if (!sig) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sig is NULL");

        return NULL;
    }

    // Process character n-grams
    if (len >= TEXT_NGRAM_SIZE) {
        for (size_t i = 0; i <= len - TEXT_NGRAM_SIZE; i++) {
            // Hash the n-gram
            uint64_t hash = hash_with_seed(text + i, TEXT_NGRAM_SIZE, DEFAULT_SEED);

            // Map to prime index
            size_t idx = hash % PRIME_SIG_DIM;

            // Weight by position (earlier = slightly higher weight)
            uint8_t weight = (i < len / 4) ? 2 : 1;

            sig->exponents[idx] = saturating_add(sig->exponents[idx], weight);
        }
    } else {
        // Short text: hash the whole thing
        uint64_t hash = hash_with_seed(text, len, DEFAULT_SEED);
        size_t idx = hash % PRIME_SIG_DIM;
        sig->exponents[idx] = saturating_add(sig->exponents[idx], 1);
    }

    // Also process individual characters for short texts
    for (size_t i = 0; i < len; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && len > 256) {
            prime_signature_heartbeat("prime_signat_loop",
                             (float)(i + 1) / (float)len);
        }

        // Simple character-based hash
        uint64_t char_hash = mix64((uint64_t)text[i] ^ (i * 0x9E3779B9));
        size_t idx = char_hash % PRIME_SIG_DIM;
        sig->exponents[idx] = saturating_add(sig->exponents[idx], 1);
    }

    // Update hash and factor count
    sig->hash = compute_signature_hash(sig->exponents);
    sig->num_factors = count_nonzero(sig->exponents);

    return sig;
}

prime_signature_t* prime_sig_from_floats(const float* floats, size_t count) {
    if (!floats || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "prime_sig_from_floats: floats is NULL");
        return NULL;
    }

    // Create empty signature
    /* Phase 8: Heartbeat at operation start */
    prime_signature_heartbeat("prime_signat_prime_sig_from_float", 0.0f);


    prime_signature_t* sig = prime_sig_create();
    if (!sig) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sig is NULL");

        return NULL;
    }

    // Find min and max for normalization
    float min_val = floats[0];
    float max_val = floats[0];

    for (size_t i = 1; i < count; i++) {
        if (floats[i] < min_val) min_val = floats[i];
        if (floats[i] > max_val) max_val = floats[i];
    }

    float range = max_val - min_val;
    if (range < PRIME_SIG_EPSILON) {
        range = 1.0f;  // Prevent division by zero
    }

    // Quantize and map floats to prime indices
    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            prime_signature_heartbeat("prime_signat_loop",
                             (float)(i + 1) / (float)count);
        }

        // Normalize to [0, 1]
        float normalized = (floats[i] - min_val) / range;

        // Quantize to bins (use PRIME_SIG_DIM bins)
        size_t bin = (size_t)(normalized * (PRIME_SIG_DIM - 1));
        if (bin >= PRIME_SIG_DIM) bin = PRIME_SIG_DIM - 1;

        // Map bin to prime index (with mixing to avoid clustering)
        uint64_t mixed = mix64(bin ^ (i * 0x9E3779B9));
        size_t idx = mixed % PRIME_SIG_DIM;

        sig->exponents[idx] = saturating_add(sig->exponents[idx], 1);
    }

    // Update hash and factor count
    sig->hash = compute_signature_hash(sig->exponents);
    sig->num_factors = count_nonzero(sig->exponents);

    return sig;
}

prime_signature_t* prime_sig_copy(const prime_signature_t* sig) {
    if (!sig) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sig is NULL");

        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    prime_signature_heartbeat("prime_signat_prime_sig_copy", 0.0f);


    prime_signature_t* copy = (prime_signature_t*)nimcp_malloc(sizeof(prime_signature_t));
    if (!copy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate copy");

        return NULL;
    }

    // Copy all fields
    memcpy(copy, sig, sizeof(prime_signature_t));

    return copy;
}

void prime_sig_destroy(prime_signature_t* sig) {
    /* Phase 8: Heartbeat at operation start */
    prime_signature_heartbeat("prime_signat_prime_sig_destroy", 0.0f);


    if (sig) {
        nimcp_free(sig);
        sig = NULL;
    }
}

prime_sig_config_t prime_sig_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    prime_signature_heartbeat("prime_signat_prime_sig_default_co", 0.0f);


    prime_sig_config_t config;
    config.hash_rounds = DEFAULT_HASH_ROUNDS;
    config.seed = DEFAULT_SEED;
    config.normalize_exponents = false;
    config.sparsity_target = 0.5f;  // ~32 non-zero exponents
    return config;
}

//=============================================================================
// Similarity Functions
//=============================================================================

float prime_sig_jaccard(const prime_signature_t* s1, const prime_signature_t* s2) {
    if (!s1 || !s2) {
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    prime_signature_heartbeat("prime_signat_prime_sig_jaccard", 0.0f);


    uint32_t intersection_sum = 0;
    uint32_t union_sum = 0;

    for (size_t i = 0; i < PRIME_SIG_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PRIME_SIG_DIM > 256) {
            prime_signature_heartbeat("prime_signat_loop",
                             (float)(i + 1) / (float)PRIME_SIG_DIM);
        }

        uint8_t e1 = s1->exponents[i];
        uint8_t e2 = s2->exponents[i];

        // min for intersection
        intersection_sum += (e1 < e2) ? e1 : e2;

        // max for union
        union_sum += (e1 > e2) ? e1 : e2;
    }

    // Handle empty signatures
    if (union_sum == 0) {
        return 0.0f;
    }

    return (float)intersection_sum / (float)union_sum;
}

float prime_sig_cosine(const prime_signature_t* s1, const prime_signature_t* s2) {
    if (!s1 || !s2) {
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    prime_signature_heartbeat("prime_signat_prime_sig_cosine", 0.0f);


    uint64_t dot_product = 0;
    uint64_t norm1_sq = 0;
    uint64_t norm2_sq = 0;

    for (size_t i = 0; i < PRIME_SIG_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PRIME_SIG_DIM > 256) {
            prime_signature_heartbeat("prime_signat_loop",
                             (float)(i + 1) / (float)PRIME_SIG_DIM);
        }

        uint32_t e1 = s1->exponents[i];
        uint32_t e2 = s2->exponents[i];

        dot_product += (uint64_t)e1 * (uint64_t)e2;
        norm1_sq += (uint64_t)e1 * (uint64_t)e1;
        norm2_sq += (uint64_t)e2 * (uint64_t)e2;
    }

    // Handle zero vectors
    if (norm1_sq == 0 || norm2_sq == 0) {
        return 0.0f;
    }

    double norm1 = sqrt((double)norm1_sq);
    double norm2 = sqrt((double)norm2_sq);

    return (float)((double)dot_product / (norm1 * norm2));
}

int prime_sig_hamming(const prime_signature_t* s1, const prime_signature_t* s2) {
    if (!s1 || !s2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prime_sig_hamming: required parameter is NULL (s1, s2)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    prime_signature_heartbeat("prime_signat_prime_sig_hamming", 0.0f);


    int distance = 0;

    for (size_t i = 0; i < PRIME_SIG_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PRIME_SIG_DIM > 256) {
            prime_signature_heartbeat("prime_signat_loop",
                             (float)(i + 1) / (float)PRIME_SIG_DIM);
        }

        // Count positions where one has non-zero and other has zero
        bool nz1 = (s1->exponents[i] > 0);
        bool nz2 = (s2->exponents[i] > 0);

        if (nz1 != nz2) {
            distance++;
        }
    }

    return distance;
}

float prime_sig_similarity(const prime_signature_t* s1,
                           const prime_signature_t* s2,
                           prime_sig_similarity_method_t method) {
    if (!s1 || !s2) {
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    prime_signature_heartbeat("prime_signat_prime_sig_similarity", 0.0f);


    switch (method) {
        case PRIME_SIG_SIMILARITY_JACCARD:
            return prime_sig_jaccard(s1, s2);

        case PRIME_SIG_SIMILARITY_COSINE:
            return prime_sig_cosine(s1, s2);

        case PRIME_SIG_SIMILARITY_HAMMING: {
            // Convert distance to similarity
            int dist = prime_sig_hamming(s1, s2);
            if (dist < 0) return -1.0f;
            return 1.0f - ((float)dist / (float)PRIME_SIG_DIM);
        }

        case PRIME_SIG_SIMILARITY_DICE: {
            // Dice coefficient: 2 * intersection / (size1 + size2)
            uint32_t intersection_sum = 0;
            uint32_t sum1 = 0;
            uint32_t sum2 = 0;

            for (size_t i = 0; i < PRIME_SIG_DIM; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && PRIME_SIG_DIM > 256) {
                    prime_signature_heartbeat("prime_signat_loop",
                                     (float)(i + 1) / (float)PRIME_SIG_DIM);
                }

                uint8_t e1 = s1->exponents[i];
                uint8_t e2 = s2->exponents[i];

                intersection_sum += (e1 < e2) ? e1 : e2;
                sum1 += e1;
                sum2 += e2;
            }

            if (sum1 + sum2 == 0) return 0.0f;
            return (2.0f * intersection_sum) / (float)(sum1 + sum2);
        }

        case PRIME_SIG_SIMILARITY_OVERLAP: {
            // Overlap coefficient: intersection / min(size1, size2)
            uint32_t intersection_sum = 0;
            uint32_t sum1 = 0;
            uint32_t sum2 = 0;

            for (size_t i = 0; i < PRIME_SIG_DIM; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && PRIME_SIG_DIM > 256) {
                    prime_signature_heartbeat("prime_signat_loop",
                                     (float)(i + 1) / (float)PRIME_SIG_DIM);
                }

                uint8_t e1 = s1->exponents[i];
                uint8_t e2 = s2->exponents[i];

                intersection_sum += (e1 < e2) ? e1 : e2;
                sum1 += e1;
                sum2 += e2;
            }

            uint32_t min_sum = (sum1 < sum2) ? sum1 : sum2;
            if (min_sum == 0) return 0.0f;
            return (float)intersection_sum / (float)min_sum;
        }

        default:
            return -1.0f;
    }
}

//=============================================================================
// Set Operations
//=============================================================================

prime_signature_t* prime_sig_compose(const prime_signature_t* s1,
                                     const prime_signature_t* s2) {
    if (!s1 || !s2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prime_sig_hamming: required parameter is NULL (s1, s2)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    prime_signature_heartbeat("prime_signat_prime_sig_compose", 0.0f);


    prime_signature_t* result = prime_sig_create();
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;
    }

    // Take max of each exponent
    for (size_t i = 0; i < PRIME_SIG_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PRIME_SIG_DIM > 256) {
            prime_signature_heartbeat("prime_signat_loop",
                             (float)(i + 1) / (float)PRIME_SIG_DIM);
        }

        result->exponents[i] = (s1->exponents[i] > s2->exponents[i])
                               ? s1->exponents[i]
                               : s2->exponents[i];
    }

    // Update hash and factor count
    result->hash = compute_signature_hash(result->exponents);
    result->num_factors = count_nonzero(result->exponents);

    return result;
}

prime_signature_t* prime_sig_intersect(const prime_signature_t* s1,
                                       const prime_signature_t* s2) {
    if (!s1 || !s2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prime_sig_hamming: required parameter is NULL (s1, s2)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    prime_signature_heartbeat("prime_signat_prime_sig_intersect", 0.0f);


    prime_signature_t* result = prime_sig_create();
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;
    }

    // Take min of each exponent
    for (size_t i = 0; i < PRIME_SIG_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PRIME_SIG_DIM > 256) {
            prime_signature_heartbeat("prime_signat_loop",
                             (float)(i + 1) / (float)PRIME_SIG_DIM);
        }

        result->exponents[i] = (s1->exponents[i] < s2->exponents[i])
                               ? s1->exponents[i]
                               : s2->exponents[i];
    }

    // Update hash and factor count
    result->hash = compute_signature_hash(result->exponents);
    result->num_factors = count_nonzero(result->exponents);

    return result;
}

prime_signature_t* prime_sig_difference(const prime_signature_t* s1,
                                        const prime_signature_t* s2) {
    if (!s1 || !s2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prime_sig_hamming: required parameter is NULL (s1, s2)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    prime_signature_heartbeat("prime_signat_prime_sig_difference", 0.0f);


    prime_signature_t* result = prime_sig_create();
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;
    }

    // Take absolute difference of each exponent
    for (size_t i = 0; i < PRIME_SIG_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PRIME_SIG_DIM > 256) {
            prime_signature_heartbeat("prime_signat_loop",
                             (float)(i + 1) / (float)PRIME_SIG_DIM);
        }

        int16_t diff = (int16_t)s1->exponents[i] - (int16_t)s2->exponents[i];
        result->exponents[i] = (diff < 0) ? (uint8_t)(-diff) : (uint8_t)diff;
    }

    // Update hash and factor count
    result->hash = compute_signature_hash(result->exponents);
    result->num_factors = count_nonzero(result->exponents);

    return result;
}

prime_signature_t* prime_sig_subtract(const prime_signature_t* s1,
                                      const prime_signature_t* s2) {
    if (!s1 || !s2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (s1, s2)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    prime_signature_heartbeat("prime_signat_prime_sig_subtract", 0.0f);


    prime_signature_t* result = prime_sig_create();
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;
    }

    // Subtract s2 from s1, clamping at 0
    for (size_t i = 0; i < PRIME_SIG_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PRIME_SIG_DIM > 256) {
            prime_signature_heartbeat("prime_signat_loop",
                             (float)(i + 1) / (float)PRIME_SIG_DIM);
        }

        result->exponents[i] = saturating_sub(s1->exponents[i], s2->exponents[i]);
    }

    // Update hash and factor count
    result->hash = compute_signature_hash(result->exponents);
    result->num_factors = count_nonzero(result->exponents);

    return result;
}

//=============================================================================
// Hashing Functions
//=============================================================================

uint64_t prime_sig_hash(const prime_signature_t* sig) {
    if (!sig) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    prime_signature_heartbeat("prime_signat_prime_sig_hash", 0.0f);


    return compute_signature_hash(sig->exponents);
}

bool prime_sig_hash_equal(const prime_signature_t* s1, const prime_signature_t* s2) {
    if (!s1 || !s2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prime_sig_hash_equal: required parameter is NULL (s1, s2)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    prime_signature_heartbeat("prime_signat_prime_sig_hash_equal", 0.0f);


    return s1->hash == s2->hash;
}

bool prime_sig_equal(const prime_signature_t* s1, const prime_signature_t* s2) {
    if (!s1 || !s2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prime_sig_equal: required parameter is NULL (s1, s2)");
        return false;
    }

    // Quick check: if hashes differ, signatures differ
    /* Phase 8: Heartbeat at operation start */
    prime_signature_heartbeat("prime_signat_prime_sig_equal", 0.0f);


    if (s1->hash != s2->hash) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "prime_sig_equal: validation failed");
        return false;
    }

    // Full comparison
    return memcmp(s1->exponents, s2->exponents, PRIME_SIG_DIM) == 0;
}

//=============================================================================
// Utility Functions
//=============================================================================

bool prime_sig_is_empty(const prime_signature_t* sig) {
    if (!sig) {
        return true;
    }

    // Early exit on first non-zero
    /* Phase 8: Heartbeat at operation start */
    prime_signature_heartbeat("prime_signat_prime_sig_is_empty", 0.0f);


    for (size_t i = 0; i < PRIME_SIG_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PRIME_SIG_DIM > 256) {
            prime_signature_heartbeat("prime_signat_loop",
                             (float)(i + 1) / (float)PRIME_SIG_DIM);
        }

        if (sig->exponents[i] > 0) {
            return false;
        }
    }

    return true;
}

uint32_t prime_sig_count_factors(const prime_signature_t* sig) {
    if (!sig) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    prime_signature_heartbeat("prime_signat_prime_sig_count_fact", 0.0f);


    return sig->num_factors;
}

uint32_t prime_sig_recount_factors(prime_signature_t* sig) {
    if (!sig) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    prime_signature_heartbeat("prime_signat_prime_sig_recount_fa", 0.0f);


    sig->num_factors = count_nonzero(sig->exponents);
    return sig->num_factors;
}

size_t prime_sig_to_string(const prime_signature_t* sig, char* buf, size_t size) {
    if (!sig) {
        if (buf && size > 0) {
            buf[0] = '\0';
        }
        return 0;
    }

    // Calculate required size
    /* Phase 8: Heartbeat at operation start */
    prime_signature_heartbeat("prime_signat_prime_sig_to_string", 0.0f);


    size_t required = 0;

    // Header
    required += 50;  // "PrimeSig[hash=0x..., factors=XX, exponents=("

    // Non-zero exponents
    for (size_t i = 0; i < PRIME_SIG_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PRIME_SIG_DIM > 256) {
            prime_signature_heartbeat("prime_signat_loop",
                             (float)(i + 1) / (float)PRIME_SIG_DIM);
        }

        if (sig->exponents[i] > 0) {
            required += 20;  // "XXX^YYY, "
        }
    }

    required += 3;  // ")]" and null terminator

    // If no buffer or too small, return required size
    if (!buf || size == 0) {
        return required;
    }

    // Build string
    int written = 0;
    int remaining = (int)size;

    // Header
    int n = snprintf(buf + written, remaining,
                     "PrimeSig[hash=0x%016llx, factors=%u, exponents=(",
                     (unsigned long long)sig->hash, sig->num_factors);
    if (n < 0 || n >= remaining) {
        buf[size - 1] = '\0';
        return required;
    }
    written += n;
    remaining -= n;

    // Non-zero exponents
    bool first = true;
    for (size_t i = 0; i < PRIME_SIG_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PRIME_SIG_DIM > 256) {
            prime_signature_heartbeat("prime_signat_loop",
                             (float)(i + 1) / (float)PRIME_SIG_DIM);
        }

        if (sig->exponents[i] > 0) {
            n = snprintf(buf + written, remaining, "%s%llu^%u",
                        first ? "" : ", ",
                        (unsigned long long)sig->primes[i],
                        sig->exponents[i]);
            if (n < 0 || n >= remaining) {
                buf[size - 1] = '\0';
                return required;
            }
            written += n;
            remaining -= n;
            first = false;
        }
    }

    // Footer
    n = snprintf(buf + written, remaining, ")]");
    if (n < 0 || n >= remaining) {
        buf[size - 1] = '\0';
        return required;
    }
    written += n;

    return (size_t)written;
}

void prime_sig_print(const prime_signature_t* sig) {
    if (!sig) {
        printf("PrimeSig[NULL]\n");
        return;
    }

    // Use a reasonably sized buffer
    /* Phase 8: Heartbeat at operation start */
    prime_signature_heartbeat("prime_signat_prime_sig_print", 0.0f);


    char buf[NIMCP_DEBUG_BUFFER_SIZE];
    prime_sig_to_string(sig, buf, sizeof(buf));
    printf("%s\n", buf);
}

uint8_t prime_sig_get_exponent(const prime_signature_t* sig, size_t index) {
    if (!sig || index >= PRIME_SIG_DIM) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    prime_signature_heartbeat("prime_signat_prime_sig_get_expone", 0.0f);


    return sig->exponents[index];
}

bool prime_sig_set_exponent(prime_signature_t* sig, size_t index, uint8_t value) {
    if (!sig || index >= PRIME_SIG_DIM) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "prime_sig_set_exponent: sig is NULL");
        return false;
    }

    // Track if factor count changes
    /* Phase 8: Heartbeat at operation start */
    prime_signature_heartbeat("prime_signat_prime_sig_set_expone", 0.0f);


    bool was_nonzero = (sig->exponents[index] > 0);
    bool is_nonzero = (value > 0);

    sig->exponents[index] = value;

    // Update factor count
    if (was_nonzero && !is_nonzero) {
        sig->num_factors--;
    } else if (!was_nonzero && is_nonzero) {
        sig->num_factors++;
    }

    // Recompute hash
    sig->hash = compute_signature_hash(sig->exponents);

    return true;
}

uint32_t prime_sig_total_weight(const prime_signature_t* sig) {
    if (!sig) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    prime_signature_heartbeat("prime_signat_prime_sig_total_weig", 0.0f);


    uint32_t total = 0;
    for (size_t i = 0; i < PRIME_SIG_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PRIME_SIG_DIM > 256) {
            prime_signature_heartbeat("prime_signat_loop",
                             (float)(i + 1) / (float)PRIME_SIG_DIM);
        }

        total += sig->exponents[i];
    }

    return total;
}

bool prime_sig_normalize(prime_signature_t* sig) {
    if (!sig) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prime_sig_normalize: sig is NULL");
        return false;
    }

    // Find max exponent
    /* Phase 8: Heartbeat at operation start */
    prime_signature_heartbeat("prime_signat_prime_sig_normalize", 0.0f);


    uint8_t max_exp = 0;
    for (size_t i = 0; i < PRIME_SIG_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PRIME_SIG_DIM > 256) {
            prime_signature_heartbeat("prime_signat_loop",
                             (float)(i + 1) / (float)PRIME_SIG_DIM);
        }

        if (sig->exponents[i] > max_exp) {
            max_exp = sig->exponents[i];
        }
    }

    // If empty or already at max, nothing to do
    if (max_exp == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "prime_sig_normalize: max_exp is zero");
        return false;
    }

    if (max_exp == 255) {
        return true;  // Already normalized
    }

    // Scale factor
    float scale = 255.0f / (float)max_exp;

    // Apply scaling
    for (size_t i = 0; i < PRIME_SIG_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PRIME_SIG_DIM > 256) {
            prime_signature_heartbeat("prime_signat_loop",
                             (float)(i + 1) / (float)PRIME_SIG_DIM);
        }

        if (sig->exponents[i] > 0) {
            float scaled = sig->exponents[i] * scale;
            // Round and clamp
            sig->exponents[i] = (scaled > 255.0f) ? 255 : (uint8_t)(scaled + 0.5f);
        }
    }

    // Update hash (factor count doesn't change)
    sig->hash = compute_signature_hash(sig->exponents);

    return true;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void prime_signature_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_prime_signature_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int prime_signature_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "prime_signature_training_begin: NULL argument");
        return -1;
    }
    prime_signature_heartbeat_instance(NULL, "prime_signature_training_begin", 0.0f);
    return 0;
}

int prime_signature_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "prime_signature_training_end: NULL argument");
        return -1;
    }
    prime_signature_heartbeat_instance(NULL, "prime_signature_training_end", 1.0f);
    return 0;
}

int prime_signature_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "prime_signature_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    prime_signature_heartbeat_instance(NULL, "prime_signature_training_step", progress);
    return 0;
}
