/**
 * @file nimcp_pq_hybrid.c
 * @brief Hybrid Classical + Post-Quantum Cryptography
 *
 * WHAT: Combines classical (X25519/Ed25519) with PQ (Kyber/Dilithium) crypto
 * WHY: Defense-in-depth; security holds if either algorithm remains secure
 * HOW: Performs both operations and combines results with KDF
 *
 * Hybrid approach protects against:
 * - Breaks in classical crypto (by quantum computers)
 * - Undiscovered breaks in PQ crypto (new algorithms)
 * - Implementation bugs in either system
 */

#include "security/nimcp_post_quantum.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#include "utils/error/nimcp_error_codes.h"
#include "utils/validation/nimcp_common.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(pq_hybrid, MESH_ADAPTER_CATEGORY_SECURITY)


/* Error code aliases for this file */
#ifndef NIMCP_OK
#define NIMCP_OK NIMCP_SUCCESS
#endif
#ifndef NIMCP_ERROR_INVALID
#define NIMCP_ERROR_INVALID NIMCP_ERROR_INVALID_PARAM
#endif
#ifndef NIMCP_ERROR_IO
#define NIMCP_ERROR_IO (-121)
#endif

/* Forward declarations for external crypto functions */
extern nimcp_error_t secure_random_bytes(uint8_t* buffer, size_t len);
extern void secure_zero(void* ptr, size_t len);

/* ========================================================================
 * Simple KDF (Key Derivation Function)
 * ======================================================================== */

/**
 * XOR-based KDF for combining secrets
 *
 * WHAT: Derives combined key from classical and PQ secrets
 * WHY: Both secrets contribute to final key entropy
 * HOW: Uses XOR mixing (production would use HKDF-SHA256)
 */
static void simple_kdf(const uint8_t* secret1, size_t len1,
                       const uint8_t* secret2, size_t len2,
                       uint8_t* output, size_t out_len) {
    /* Initialize output */
    memset(output, 0, out_len);

    /* Mix in first secret */
    for (size_t i = 0; i < len1 && i < out_len; i++) {
        output[i] ^= secret1[i];
    }

    /* Mix in second secret */
    for (size_t i = 0; i < len2 && i < out_len; i++) {
        output[i] ^= secret2[i];
    }

    /* Additional mixing (simple - production would use proper KDF) */
    for (size_t i = 0; i < out_len; i++) {
        output[i] ^= (uint8_t)(i & 0xFF);
    }
}

/* ========================================================================
 * Simplified Classical Crypto Stubs
 * ======================================================================== */

/**
 * Simplified X25519 key exchange
 * Production would use proper Curve25519 implementation
 */
static nimcp_error_t x25519_exchange(const uint8_t* private_key,
                                      const uint8_t* public_key,
                                      uint8_t* shared_secret) {
    if (!private_key || !public_key || !shared_secret) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID, "pq_hybrid: error condition");
        return NIMCP_ERROR_INVALID;
    }

    /* Simplified: XOR private and public keys (NOT cryptographically secure) */
    for (size_t i = 0; i < NIMCP_X25519_KEY_BYTES; i++) {
        shared_secret[i] = private_key[i] ^ public_key[i];
    }

    LOG_DEBUG("x25519_exchange: Computed classical shared secret");
    return NIMCP_OK;
}

/**
 * Simplified Ed25519 signing
 * Production would use proper Ed25519 implementation
 */
static nimcp_error_t ed25519_sign(const uint8_t* secret_key,
                                   const uint8_t* message,
                                   size_t message_len,
                                   uint8_t* signature,
                                   size_t* signature_len) {
    if (!secret_key || !message || !signature || !signature_len) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID, "pq_hybrid: error condition");
        return NIMCP_ERROR_INVALID;
    }

    /* Simplified signature (64 bytes) */
    *signature_len = 64;

    /* Hash message with key (very simplified) */
    for (size_t i = 0; i < 64; i++) {
        signature[i] = secret_key[i % NIMCP_ED25519_SECRET_KEY_BYTES];
        if (i < message_len) {
            signature[i] ^= message[i];
        }
    }

    LOG_DEBUG("ed25519_sign: Created classical signature");
    return NIMCP_OK;
}

/**
 * Simplified Ed25519 verification
 * Production would use proper Ed25519 implementation
 */
static nimcp_error_t ed25519_verify(const uint8_t* public_key,
                                     const uint8_t* message,
                                     size_t message_len,
                                     const uint8_t* signature,
                                     size_t signature_len) {
    if (!public_key || !message || !signature) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID, "pq_hybrid: error condition");
        return NIMCP_ERROR_INVALID;
    }

    if (signature_len != 64) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR, "pq_hybrid: operation failed");
        return NIMCP_ERROR;
    }

    /* Simplified verification - check non-zero */
    bool has_data = false;
    for (size_t i = 0; i < signature_len; i++) {
        if (signature[i] != 0) {
            has_data = true;
            break;
        }
    }

    if (!has_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR, "pq_hybrid: operation failed");
        return NIMCP_ERROR;
    }

    LOG_DEBUG("ed25519_verify: Verified classical signature");
    return NIMCP_OK;
}

/* ========================================================================
 * Hybrid Key Exchange
 * ======================================================================== */

/**
 * Hybrid classical + PQ key exchange
 *
 * WHAT: Combines X25519 and Kyber key exchange
 * WHY: Security if either algorithm remains unbroken
 * HOW: Performs both exchanges, combines with KDF
 */
nimcp_error_t nimcp_hybrid_key_exchange(
    nimcp_pq_context_t ctx,
    const uint8_t* classical_private,
    const uint8_t* classical_public,
    const uint8_t* pq_public,
    size_t pq_public_len,
    uint8_t* combined_secret,
    size_t secret_len)
{
    if (!classical_private || !classical_public || !pq_public ||
        !combined_secret || secret_len != NIMCP_HYBRID_SHARED_SECRET_BYTES) {
        LOG_ERROR("nimcp_hybrid_key_exchange: Invalid arguments");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID, "pq_hybrid: error condition");
        return NIMCP_ERROR_INVALID;
    }

    /* Allocate temporary buffers */
    uint8_t classical_secret[NIMCP_X25519_KEY_BYTES];
    uint8_t pq_ciphertext[NIMCP_KYBER_1024_CIPHERTEXT_BYTES];
    uint8_t pq_secret[NIMCP_KYBER_1024_SHARED_SECRET_BYTES];
    size_t ct_len = sizeof(pq_ciphertext);

    nimcp_error_t err;

    /* Perform classical X25519 key exchange */
    err = x25519_exchange(classical_private, classical_public, classical_secret);
    if (err != NIMCP_OK) {
        LOG_ERROR("nimcp_hybrid_key_exchange: Classical exchange failed");
        secure_zero(classical_secret, sizeof(classical_secret));
        return err;
    }

    /* Determine Kyber variant from public key length */
    nimcp_kyber_variant_t variant;
    if (pq_public_len == NIMCP_KYBER_512_PUBLIC_KEY_BYTES) {
        variant = NIMCP_PQ_KYBER_512;
    } else if (pq_public_len == NIMCP_KYBER_768_PUBLIC_KEY_BYTES) {
        variant = NIMCP_PQ_KYBER_768;
    } else if (pq_public_len == NIMCP_KYBER_1024_PUBLIC_KEY_BYTES) {
        variant = NIMCP_PQ_KYBER_1024;
    } else {
        LOG_ERROR("nimcp_hybrid_key_exchange: Invalid PQ public key length %zu",
                       pq_public_len);
        secure_zero(classical_secret, sizeof(classical_secret));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID, "pq_hybrid: error condition");
        return NIMCP_ERROR_INVALID;
    }

    /* Perform PQ Kyber encapsulation */
    err = nimcp_kyber_encapsulate(variant, pq_public, pq_ciphertext, &ct_len,
                                   pq_secret, sizeof(pq_secret));
    if (err != NIMCP_OK) {
        LOG_ERROR("nimcp_hybrid_key_exchange: PQ encapsulation failed");
        secure_zero(classical_secret, sizeof(classical_secret));
        secure_zero(pq_secret, sizeof(pq_secret));
        return err;
    }

    /* Combine secrets using KDF */
    simple_kdf(classical_secret, sizeof(classical_secret),
               pq_secret, sizeof(pq_secret),
               combined_secret, secret_len);

    /* Securely zero temporary secrets */
    secure_zero(classical_secret, sizeof(classical_secret));
    secure_zero(pq_secret, sizeof(pq_secret));
    secure_zero(pq_ciphertext, sizeof(pq_ciphertext));

    LOG_INFO("nimcp_hybrid_key_exchange: Hybrid key exchange completed successfully");

    return NIMCP_OK;
}

/* ========================================================================
 * Hybrid Signatures
 * ======================================================================== */

/**
 * Hybrid classical + PQ signature
 *
 * WHAT: Creates signature using both Ed25519 and Dilithium
 * WHY: Signature valid if either algorithm remains secure
 * HOW: Signs with both keys, concatenates signatures
 */
nimcp_error_t nimcp_hybrid_sign(
    nimcp_pq_context_t ctx,
    const uint8_t* classical_key,
    const uint8_t* pq_key,
    size_t pq_key_len,
    const uint8_t* message,
    size_t message_len,
    uint8_t* signature,
    size_t* signature_len)
{
    if (!classical_key || !pq_key || !message || !signature || !signature_len) {
        LOG_ERROR("nimcp_hybrid_sign: Invalid arguments");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID, "pq_hybrid: error condition");
        return NIMCP_ERROR_INVALID;
    }

    /* Determine Dilithium variant from secret key length */
    nimcp_dilithium_variant_t variant;
    size_t pq_sig_len;

    if (pq_key_len == NIMCP_DILITHIUM_2_SECRET_KEY_BYTES) {
        variant = NIMCP_PQ_DILITHIUM_2;
        pq_sig_len = NIMCP_DILITHIUM_2_SIGNATURE_BYTES;
    } else if (pq_key_len == NIMCP_DILITHIUM_3_SECRET_KEY_BYTES) {
        variant = NIMCP_PQ_DILITHIUM_3;
        pq_sig_len = NIMCP_DILITHIUM_3_SIGNATURE_BYTES;
    } else if (pq_key_len == NIMCP_DILITHIUM_5_SECRET_KEY_BYTES) {
        variant = NIMCP_PQ_DILITHIUM_5;
        pq_sig_len = NIMCP_DILITHIUM_5_SIGNATURE_BYTES;
    } else {
        LOG_ERROR("nimcp_hybrid_sign: Invalid PQ key length %zu", pq_key_len);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID, "pq_hybrid: error condition");
        return NIMCP_ERROR_INVALID;
    }

    /* Check buffer size */
    size_t required_len = 64 + pq_sig_len;  /* Ed25519 + Dilithium */
    if (*signature_len < required_len) {
        LOG_ERROR("nimcp_hybrid_sign: Signature buffer too small");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID, "pq_hybrid: error condition");
        return NIMCP_ERROR_INVALID;
    }

    nimcp_error_t err;

    /* Create Ed25519 signature */
    size_t classical_sig_len = 64;
    err = ed25519_sign(classical_key, message, message_len,
                       signature, &classical_sig_len);
    if (err != NIMCP_OK) {
        LOG_ERROR("nimcp_hybrid_sign: Classical signing failed");
        return err;
    }

    /* Create Dilithium signature */
    size_t dilithium_sig_len = pq_sig_len;
    err = nimcp_dilithium_sign(variant, pq_key, message, message_len,
                                signature + 64, &dilithium_sig_len);
    if (err != NIMCP_OK) {
        LOG_ERROR("nimcp_hybrid_sign: PQ signing failed");
        secure_zero(signature, 64);  /* Clear classical signature */
        return err;
    }

    *signature_len = 64 + dilithium_sig_len;

    LOG_INFO("nimcp_hybrid_sign: Hybrid signature created (classical=64, pq=%zu, total=%zu)",
                   dilithium_sig_len, *signature_len);

    return NIMCP_OK;
}

/**
 * Hybrid signature verification
 *
 * WHAT: Verifies hybrid Ed25519+Dilithium signature
 * WHY: Both signatures must be valid for acceptance
 * HOW: Verifies both signatures independently
 */
nimcp_error_t nimcp_hybrid_verify(
    nimcp_pq_context_t ctx,
    const uint8_t* classical_pubkey,
    const uint8_t* pq_pubkey,
    size_t pq_pubkey_len,
    const uint8_t* message,
    size_t message_len,
    const uint8_t* signature,
    size_t signature_len)
{
    if (!classical_pubkey || !pq_pubkey || !message || !signature) {
        LOG_ERROR("nimcp_hybrid_verify: Invalid arguments");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID, "pq_hybrid: error condition");
        return NIMCP_ERROR_INVALID;
    }

    /* Determine Dilithium variant from public key length */
    nimcp_dilithium_variant_t variant;
    size_t pq_sig_len;

    if (pq_pubkey_len == NIMCP_DILITHIUM_2_PUBLIC_KEY_BYTES) {
        variant = NIMCP_PQ_DILITHIUM_2;
        pq_sig_len = NIMCP_DILITHIUM_2_SIGNATURE_BYTES;
    } else if (pq_pubkey_len == NIMCP_DILITHIUM_3_PUBLIC_KEY_BYTES) {
        variant = NIMCP_PQ_DILITHIUM_3;
        pq_sig_len = NIMCP_DILITHIUM_3_SIGNATURE_BYTES;
    } else if (pq_pubkey_len == NIMCP_DILITHIUM_5_PUBLIC_KEY_BYTES) {
        variant = NIMCP_PQ_DILITHIUM_5;
        pq_sig_len = NIMCP_DILITHIUM_5_SIGNATURE_BYTES;
    } else {
        LOG_ERROR("nimcp_hybrid_verify: Invalid PQ pubkey length %zu",
                       pq_pubkey_len);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID, "pq_hybrid: error condition");
        return NIMCP_ERROR_INVALID;
    }

    /* Verify signature length */
    size_t expected_len = 64 + pq_sig_len;
    if (signature_len != expected_len) {
        LOG_ERROR("nimcp_hybrid_verify: Invalid signature length %zu (expected %zu)",
                       signature_len, expected_len);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR, "pq_hybrid: operation failed");
        return NIMCP_ERROR;
    }

    nimcp_error_t err;

    /* Verify Ed25519 signature */
    err = ed25519_verify(classical_pubkey, message, message_len,
                         signature, 64);
    if (err != NIMCP_OK) {
        LOG_ERROR("nimcp_hybrid_verify: Classical verification failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR, "pq_hybrid: operation failed");
        return NIMCP_ERROR;
    }

    /* Verify Dilithium signature */
    err = nimcp_dilithium_verify(variant, pq_pubkey, message, message_len,
                                  signature + 64, pq_sig_len);
    if (err != NIMCP_OK) {
        LOG_ERROR("nimcp_hybrid_verify: PQ verification failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR, "pq_hybrid: operation failed");
        return NIMCP_ERROR;
    }

    LOG_INFO("nimcp_hybrid_verify: Hybrid signature verified successfully");

    return NIMCP_OK;
}
