/**
 * @file nimcp_post_quantum.h
 * @brief Post-Quantum Cryptography for NIMCP
 *
 * WHAT: Quantum-resistant cryptographic primitives using CRYSTALS-Kyber and Dilithium
 * WHY: Protect against future quantum computer attacks on classical cryptography
 * HOW: Implements NIST-standardized post-quantum algorithms with hybrid classical+PQ modes
 *
 * Features:
 * - CRYSTALS-Kyber key encapsulation (KEM)
 * - CRYSTALS-Dilithium digital signatures
 * - Hybrid classical+PQ modes for gradual migration
 * - Integration with bio-async messaging
 * - Secure memory handling and constant-time operations
 */

#ifndef NIMCP_POST_QUANTUM_H
#define NIMCP_POST_QUANTUM_H

#include "utils/error/nimcp_error_codes.h"
#include "utils/validation/nimcp_common.h"
#include "async/nimcp_bio_router.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Magic numbers for validation */
#define NIMCP_PQ_CONTEXT_MAGIC      0x504B5943  /* "PQYC" */
#define NIMCP_KYBER_KEYPAIR_MAGIC   0x4B594252  /* "KYBR" */
#define NIMCP_DILITHIUM_KEYPAIR_MAGIC 0x44494C54 /* "DILT" */

/* Kyber parameters (NIST security levels) */
#define NIMCP_KYBER_512_PUBLIC_KEY_BYTES   800
#define NIMCP_KYBER_512_SECRET_KEY_BYTES   1632
#define NIMCP_KYBER_512_CIPHERTEXT_BYTES   768
#define NIMCP_KYBER_512_SHARED_SECRET_BYTES 32

#define NIMCP_KYBER_768_PUBLIC_KEY_BYTES   1184
#define NIMCP_KYBER_768_SECRET_KEY_BYTES   2400
#define NIMCP_KYBER_768_CIPHERTEXT_BYTES   1088
#define NIMCP_KYBER_768_SHARED_SECRET_BYTES 32

#define NIMCP_KYBER_1024_PUBLIC_KEY_BYTES  1568
#define NIMCP_KYBER_1024_SECRET_KEY_BYTES  3168
#define NIMCP_KYBER_1024_CIPHERTEXT_BYTES  1568
#define NIMCP_KYBER_1024_SHARED_SECRET_BYTES 32

/* Dilithium parameters (NIST security levels) */
#define NIMCP_DILITHIUM_2_PUBLIC_KEY_BYTES  1312
#define NIMCP_DILITHIUM_2_SECRET_KEY_BYTES  2528
#define NIMCP_DILITHIUM_2_SIGNATURE_BYTES   2420

#define NIMCP_DILITHIUM_3_PUBLIC_KEY_BYTES  1952
#define NIMCP_DILITHIUM_3_SECRET_KEY_BYTES  4000
#define NIMCP_DILITHIUM_3_SIGNATURE_BYTES   3293

#define NIMCP_DILITHIUM_5_PUBLIC_KEY_BYTES  2592
#define NIMCP_DILITHIUM_5_SECRET_KEY_BYTES  4864
#define NIMCP_DILITHIUM_5_SIGNATURE_BYTES   4595

/* Hybrid mode parameters */
#define NIMCP_X25519_KEY_BYTES              32
#define NIMCP_ED25519_PUBLIC_KEY_BYTES      32
#define NIMCP_ED25519_SECRET_KEY_BYTES      64
#define NIMCP_HYBRID_SHARED_SECRET_BYTES    64

/**
 * Post-quantum context (opaque)
 */
typedef struct nimcp_pq_context* nimcp_pq_context_t;

/**
 * Kyber security variants
 */
typedef enum {
    NIMCP_PQ_KYBER_512,   /* NIST Level 1 - 128-bit security */
    NIMCP_PQ_KYBER_768,   /* NIST Level 3 - 192-bit security */
    NIMCP_PQ_KYBER_1024   /* NIST Level 5 - 256-bit security */
} nimcp_kyber_variant_t;

/**
 * Dilithium security variants
 */
typedef enum {
    NIMCP_PQ_DILITHIUM_2,  /* NIST Level 2 - 128-bit security */
    NIMCP_PQ_DILITHIUM_3,  /* NIST Level 3 - 192-bit security */
    NIMCP_PQ_DILITHIUM_5   /* NIST Level 5 - 256-bit security */
} nimcp_dilithium_variant_t;

/**
 * Kyber keypair structure
 */
typedef struct {
    uint32_t magic;
    nimcp_kyber_variant_t variant;
    uint8_t* public_key;
    size_t public_key_len;
    uint8_t* secret_key;
    size_t secret_key_len;
} nimcp_kyber_keypair_t;

/**
 * Dilithium keypair structure
 */
typedef struct {
    uint32_t magic;
    nimcp_dilithium_variant_t variant;
    uint8_t* public_key;
    size_t public_key_len;
    uint8_t* secret_key;
    size_t secret_key_len;
} nimcp_dilithium_keypair_t;

/**
 * Hybrid mode configuration
 */
typedef struct {
    bool enable_classical;     /* Use X25519/Ed25519 */
    bool enable_pq;            /* Use Kyber/Dilithium */
    bool require_both;         /* Both must succeed */
    bool allow_pq_fallback;    /* Fallback to PQ if classical fails */
} nimcp_hybrid_config_t;

/**
 * Post-quantum statistics
 */
typedef struct {
    uint64_t kyber_keygens;
    uint64_t kyber_encapsulations;
    uint64_t kyber_decapsulations;
    uint64_t dilithium_keygens;
    uint64_t dilithium_signs;
    uint64_t dilithium_verifications;
    uint64_t hybrid_operations;
    uint64_t failures;
} nimcp_pq_stats_t;

/**
 * Post-quantum configuration
 */
typedef struct {
    nimcp_kyber_variant_t default_kyber_variant;
    nimcp_dilithium_variant_t default_dilithium_variant;
    nimcp_hybrid_config_t hybrid_config;
    bool enable_logging;
    bio_module_context_t bio_ctx;  /* Optional bio-async integration */
} nimcp_pq_config_t;

/* ========================================================================
 * Context Management
 * ======================================================================== */

/**
 * Create post-quantum cryptography context
 *
 * WHAT: Initializes PQ crypto system with specified configuration
 * WHY: Centralized management of PQ operations and state
 * HOW: Allocates context, initializes RNG, sets up bio-async if provided
 *
 * @param config Configuration (NULL for defaults)
 * @return Context handle or NULL on error
 */
nimcp_pq_context_t nimcp_pq_context_create(const nimcp_pq_config_t* config);

/**
 * Destroy post-quantum context
 *
 * WHAT: Safely releases all PQ crypto resources
 * WHY: Prevent memory leaks and ensure secure key erasure
 * HOW: Zeros sensitive memory, frees allocations, unregisters from bio-async
 *
 * @param ctx Context to destroy
 */
void nimcp_pq_context_destroy(nimcp_pq_context_t ctx);

/**
 * Get post-quantum statistics
 *
 * @param ctx Post-quantum context
 * @param stats Output statistics structure
 * @return NIMCP_OK on success
 */
nimcp_error_t nimcp_pq_get_stats(nimcp_pq_context_t ctx, nimcp_pq_stats_t* stats);

/* ========================================================================
 * CRYSTALS-Kyber Key Encapsulation Mechanism (KEM)
 * ======================================================================== */

/**
 * Generate Kyber keypair
 *
 * WHAT: Creates public/secret key pair for Kyber KEM
 * WHY: Enable quantum-resistant key exchange
 * HOW: Uses secure random to generate keys according to Kyber spec
 *
 * @param variant Kyber security variant (512/768/1024)
 * @param keypair Output keypair structure (allocated by function)
 * @return NIMCP_OK on success
 */
nimcp_error_t nimcp_kyber_keygen(nimcp_kyber_variant_t variant,
                                  nimcp_kyber_keypair_t* keypair);

/**
 * Kyber encapsulation
 *
 * WHAT: Encapsulates a shared secret using public key
 * WHY: Sender generates shared secret for symmetric encryption
 * HOW: Creates ciphertext and shared secret using Kyber encapsulation
 *
 * @param variant Kyber security variant
 * @param public_key Recipient's public key
 * @param ciphertext Output ciphertext buffer (must be pre-allocated)
 * @param ciphertext_len Input: buffer size, Output: actual ciphertext length
 * @param shared_secret Output shared secret (32 bytes)
 * @param shared_secret_len Size of shared secret buffer (must be 32)
 * @return NIMCP_OK on success
 */
nimcp_error_t nimcp_kyber_encapsulate(
    nimcp_kyber_variant_t variant,
    const uint8_t* public_key,
    uint8_t* ciphertext,
    size_t* ciphertext_len,
    uint8_t* shared_secret,
    size_t shared_secret_len
);

/**
 * Kyber decapsulation
 *
 * WHAT: Recovers shared secret from ciphertext using secret key
 * WHY: Recipient extracts shared secret for symmetric decryption
 * HOW: Uses Kyber decapsulation to recover the shared secret
 *
 * @param variant Kyber security variant
 * @param secret_key Recipient's secret key
 * @param ciphertext Received ciphertext
 * @param ciphertext_len Length of ciphertext
 * @param shared_secret Output shared secret (32 bytes)
 * @param shared_secret_len Size of shared secret buffer (must be 32)
 * @return NIMCP_OK on success
 */
nimcp_error_t nimcp_kyber_decapsulate(
    nimcp_kyber_variant_t variant,
    const uint8_t* secret_key,
    const uint8_t* ciphertext,
    size_t ciphertext_len,
    uint8_t* shared_secret,
    size_t shared_secret_len
);

/**
 * Free Kyber keypair
 *
 * WHAT: Securely erases and frees Kyber keypair memory
 * WHY: Prevent key material leakage
 * HOW: Zeros memory before freeing
 *
 * @param keypair Keypair to free
 */
void nimcp_kyber_keypair_free(nimcp_kyber_keypair_t* keypair);

/**
 * Get Kyber parameter sizes
 *
 * @param variant Kyber variant
 * @param public_key_len Output: public key size
 * @param secret_key_len Output: secret key size
 * @param ciphertext_len Output: ciphertext size
 * @return NIMCP_OK on success
 */
nimcp_error_t nimcp_kyber_get_sizes(
    nimcp_kyber_variant_t variant,
    size_t* public_key_len,
    size_t* secret_key_len,
    size_t* ciphertext_len
);

/* ========================================================================
 * CRYSTALS-Dilithium Digital Signatures
 * ======================================================================== */

/**
 * Generate Dilithium keypair
 *
 * WHAT: Creates public/secret key pair for Dilithium signatures
 * WHY: Enable quantum-resistant digital signatures
 * HOW: Uses secure random to generate keys according to Dilithium spec
 *
 * @param variant Dilithium security variant (2/3/5)
 * @param keypair Output keypair structure (allocated by function)
 * @return NIMCP_OK on success
 */
nimcp_error_t nimcp_dilithium_keygen(nimcp_dilithium_variant_t variant,
                                      nimcp_dilithium_keypair_t* keypair);

/**
 * Sign message with Dilithium
 *
 * WHAT: Creates digital signature over message
 * WHY: Authenticate message origin and integrity
 * HOW: Uses Dilithium signature algorithm with secret key
 *
 * @param variant Dilithium security variant
 * @param secret_key Signer's secret key
 * @param message Message to sign
 * @param message_len Length of message
 * @param signature Output signature buffer (must be pre-allocated)
 * @param signature_len Input: buffer size, Output: actual signature length
 * @return NIMCP_OK on success
 */
nimcp_error_t nimcp_dilithium_sign(
    nimcp_dilithium_variant_t variant,
    const uint8_t* secret_key,
    const uint8_t* message,
    size_t message_len,
    uint8_t* signature,
    size_t* signature_len
);

/**
 * Verify Dilithium signature
 *
 * WHAT: Verifies digital signature on message
 * WHY: Authenticate message origin and detect tampering
 * HOW: Uses Dilithium verification algorithm with public key
 *
 * @param variant Dilithium security variant
 * @param public_key Signer's public key
 * @param message Message that was signed
 * @param message_len Length of message
 * @param signature Signature to verify
 * @param signature_len Length of signature
 * @return NIMCP_OK if valid, error otherwise
 */
nimcp_error_t nimcp_dilithium_verify(
    nimcp_dilithium_variant_t variant,
    const uint8_t* public_key,
    const uint8_t* message,
    size_t message_len,
    const uint8_t* signature,
    size_t signature_len
);

/**
 * Free Dilithium keypair
 *
 * WHAT: Securely erases and frees Dilithium keypair memory
 * WHY: Prevent key material leakage
 * HOW: Zeros memory before freeing
 *
 * @param keypair Keypair to free
 */
void nimcp_dilithium_keypair_free(nimcp_dilithium_keypair_t* keypair);

/**
 * Get Dilithium parameter sizes
 *
 * @param variant Dilithium variant
 * @param public_key_len Output: public key size
 * @param secret_key_len Output: secret key size
 * @param signature_len Output: signature size
 * @return NIMCP_OK on success
 */
nimcp_error_t nimcp_dilithium_get_sizes(
    nimcp_dilithium_variant_t variant,
    size_t* public_key_len,
    size_t* secret_key_len,
    size_t* signature_len
);

/* ========================================================================
 * Hybrid Classical + Post-Quantum Operations
 * ======================================================================== */

/**
 * Hybrid key exchange
 *
 * WHAT: Combines classical (X25519) and PQ (Kyber) key exchange
 * WHY: Defense-in-depth; protects if either algorithm is broken
 * HOW: Performs both exchanges and combines secrets with KDF
 *
 * @param ctx Post-quantum context
 * @param classical_private X25519 private key (32 bytes)
 * @param classical_public Peer's X25519 public key (32 bytes)
 * @param pq_public Peer's Kyber public key
 * @param pq_public_len Length of Kyber public key
 * @param combined_secret Output combined secret (64 bytes)
 * @param secret_len Size of output buffer (must be 64)
 * @return NIMCP_OK on success
 */
nimcp_error_t nimcp_hybrid_key_exchange(
    nimcp_pq_context_t ctx,
    const uint8_t* classical_private,
    const uint8_t* classical_public,
    const uint8_t* pq_public,
    size_t pq_public_len,
    uint8_t* combined_secret,
    size_t secret_len
);

/**
 * Hybrid signature
 *
 * WHAT: Creates signature using both Ed25519 and Dilithium
 * WHY: Signature remains valid if either algorithm is secure
 * HOW: Signs with both keys and concatenates signatures
 *
 * @param ctx Post-quantum context
 * @param classical_key Ed25519 secret key (64 bytes)
 * @param pq_key Dilithium secret key
 * @param pq_key_len Length of Dilithium secret key
 * @param message Message to sign
 * @param message_len Length of message
 * @param signature Output combined signature buffer
 * @param signature_len Input: buffer size, Output: actual length
 * @return NIMCP_OK on success
 */
nimcp_error_t nimcp_hybrid_sign(
    nimcp_pq_context_t ctx,
    const uint8_t* classical_key,
    const uint8_t* pq_key,
    size_t pq_key_len,
    const uint8_t* message,
    size_t message_len,
    uint8_t* signature,
    size_t* signature_len
);

/**
 * Hybrid signature verification
 *
 * WHAT: Verifies hybrid Ed25519+Dilithium signature
 * WHY: Both signatures must be valid for acceptance
 * HOW: Verifies both signatures independently
 *
 * @param ctx Post-quantum context
 * @param classical_pubkey Ed25519 public key (32 bytes)
 * @param pq_pubkey Dilithium public key
 * @param pq_pubkey_len Length of Dilithium public key
 * @param message Message that was signed
 * @param message_len Length of message
 * @param signature Combined signature
 * @param signature_len Length of signature
 * @return NIMCP_OK if both signatures valid
 */
nimcp_error_t nimcp_hybrid_verify(
    nimcp_pq_context_t ctx,
    const uint8_t* classical_pubkey,
    const uint8_t* pq_pubkey,
    size_t pq_pubkey_len,
    const uint8_t* message,
    size_t message_len,
    const uint8_t* signature,
    size_t signature_len
);

/* ========================================================================
 * Utility Functions
 * ======================================================================== */

/**
 * Get security level in bits
 *
 * @param kyber_variant Kyber variant
 * @return Security level (128, 192, or 256)
 */
int nimcp_kyber_security_level(nimcp_kyber_variant_t variant);

/**
 * Get security level in bits
 *
 * @param dilithium_variant Dilithium variant
 * @return Security level (128, 192, or 256)
 */
int nimcp_dilithium_security_level(nimcp_dilithium_variant_t variant);

/**
 * Self-test post-quantum implementations
 *
 * WHAT: Runs known-answer tests for all PQ algorithms
 * WHY: Verify correct implementation before use
 * HOW: Tests against reference vectors
 *
 * @param ctx Post-quantum context
 * @return NIMCP_OK if all tests pass
 */
nimcp_error_t nimcp_pq_self_test(nimcp_pq_context_t ctx);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_POST_QUANTUM_H */
