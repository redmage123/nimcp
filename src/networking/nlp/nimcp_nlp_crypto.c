#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_nlp_crypto.c - Neural Link Protocol Encryption Implementation
//=============================================================================

#define LOG_MODULE "nlp_crypto"
#define LOG_MODULE_ID 0x0A00
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(nlp_crypto)

/**
 * @file nimcp_nlp_crypto.c
 * @brief AES-256-GCM encryption for Neural Link Protocol
 *
 * WHAT: Cryptographic operations for NLP message encryption/decryption
 * WHY:  Secure brain-to-brain communication in hostile environments
 * HOW:  AES-256-GCM via OpenSSL, ChaCha20-Poly1305 fallback via libsodium
 *
 * SECURITY DESIGN:
 *
 * PRIMARY PATH (OpenSSL available):
 * - AES-256-GCM for encryption (NIST approved, hardware accelerated)
 * - HKDF-SHA256 for key derivation (RFC 5869)
 * - X25519 for key exchange (Curve25519 ECDH)
 * - CSPRNG from OpenSSL for nonce generation
 *
 * FALLBACK PATH (libsodium only):
 * - XChaCha20-Poly1305 for encryption (more forgiving nonce)
 * - Blake2b for key derivation
 * - X25519 for key exchange (crypto_box_keypair)
 * - randombytes() for nonce generation
 *
 * NONCE CONSTRUCTION (preventing reuse):
 * ┌──────────────────────────────────────────────────┐
 * │ Byte 0-3:  Unix timestamp (seconds)              │
 * │ Byte 4-5:  Sequence number (16-bit counter)      │
 * │ Byte 6-11: Random bytes (CSPRNG)                 │
 * └──────────────────────────────────────────────────┘
 *
 * This ensures nonce uniqueness even with:
 * - Clock resets (random bytes differ)
 * - Fast message generation (sequence counter)
 * - Multiple nodes (random bytes differ)
 *
 * ATTACK RESISTANCE:
 * - Replay attacks: Timestamp window check (60s default)
 * - Nonce reuse: Impossible due to construction
 * - Man-in-the-middle: Authenticated encryption (GCM tag)
 * - Timing attacks: Constant-time tag comparison
 * - Side channels: Memory zeroing, no secret-dependent branches
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#include "networking/nlp/nimcp_nlp_internal.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/error/nimcp_error_codes.h"
#include "security/nimcp_bbb_helpers.h"
#include "security/nimcp_constant_time.h"
#include "async/nimcp_bio_async.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/platform/nimcp_platform_once.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <time.h>

#define NLP_CRYPTO_MODULE "nlp_crypto"

// Try OpenSSL first
#ifdef NIMCP_ENABLE_OPENSSL
    #define NLP_USE_OPENSSL 1
    #include <openssl/evp.h>
    #include <openssl/rand.h>
    #include <openssl/kdf.h>
    #include <openssl/err.h>
#else
    #define NLP_USE_OPENSSL 0
#endif

// Fallback to libsodium if available
#if !NLP_USE_OPENSSL && defined(NIMCP_ENABLE_ENCRYPTION)
    #define NLP_USE_LIBSODIUM 1
    #include <sodium.h>
#else
    #define NLP_USE_LIBSODIUM 0
#endif

// Neither available - software fallback requires explicit opt-in
#if !NLP_USE_OPENSSL && !NLP_USE_LIBSODIUM
    #ifdef NIMCP_ALLOW_INSECURE_FALLBACK
        #define NLP_USE_SOFTWARE_FALLBACK 1
        #warning "NLP crypto using insecure software fallback - do not use in production!"
    #else
        #define NLP_USE_SOFTWARE_FALLBACK 0
        #error "No secure crypto library available. Define NIMCP_ALLOW_INSECURE_FALLBACK for testing only."
    #endif
#else
    #define NLP_USE_SOFTWARE_FALLBACK 0
#endif

//=============================================================================
// Constants
//=============================================================================

#define NLP_CRYPTO_KEY_SIZE     32  // AES-256
#define NLP_CRYPTO_NONCE_SIZE   12  // GCM standard
#define NLP_CRYPTO_TAG_SIZE     16  // GCM tag
#define NLP_HKDF_SALT_SIZE      32  // HKDF salt
#define NLP_HKDF_INFO_MAX       64  // Max context info

//=============================================================================
// Bio-Async Integration
//=============================================================================

static bio_module_context_t g_crypto_bio_ctx = NULL;
// Use atomic for thread-safe operation ID generation (avoid data races)
static _Atomic uint32_t g_crypto_operation_id = 0;
// Thread-safe one-time initialization for bio-async registration
static nimcp_platform_once_t g_crypto_bio_once = NIMCP_PLATFORM_ONCE_INIT;

/**
 * @brief Internal one-time bio-async registration routine
 * @note Called via nimcp_platform_once() to ensure thread-safe initialization
 */
static void nlp_crypto_bio_register_once(void) {
    if (!bio_router_is_initialized()) return;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SECURITY,
        .module_name = NLP_CRYPTO_MODULE,
        .inbox_capacity = 16,
        .user_data = NULL
    };

    g_crypto_bio_ctx = bio_router_register_module(&info);
    if (g_crypto_bio_ctx) {
        NIMCP_LOGGING_DEBUG(NLP_CRYPTO_MODULE,
            "Registered with bio-router for crypto events");
    }
}

/**
 * @brief Initialize crypto module bio-async registration (thread-safe)
 */
static void nlp_crypto_ensure_bio_registered(void) {
    nimcp_platform_once(&g_crypto_bio_once, nlp_crypto_bio_register_once);
}

/**
 * @brief Broadcast crypto operation completion
 */
static void nlp_crypto_broadcast_complete(uint32_t operation_id,
                                           bio_message_type_t op_type,
                                           uint32_t output_size,
                                           bool success,
                                           float processing_time_us) {
    nlp_crypto_ensure_bio_registered();
    if (!g_crypto_bio_ctx) return;

    bio_msg_nlp_crypto_complete_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header, op_type, BIO_MODULE_SECURITY,
                        BIO_MODULE_ALL, sizeof(msg));
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.channel = success ? BIO_CHANNEL_DOPAMINE : BIO_CHANNEL_NOREPINEPHRINE;
    msg.operation_id = operation_id;
    msg.output_size = output_size;
    msg.success = success;
    msg.error_code = success ? 0 : 1;
    msg.processing_time_us = processing_time_us;

    bio_router_broadcast(g_crypto_bio_ctx, &msg, sizeof(msg));
}

/**
 * @brief Broadcast crypto error
 */
static void nlp_crypto_broadcast_error(uint64_t peer_id, uint32_t operation_id,
                                        uint8_t error_code, const char* message,
                                        bool is_security_violation) {
    nlp_crypto_ensure_bio_registered();
    if (!g_crypto_bio_ctx) return;

    bio_msg_nlp_crypto_error_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header, BIO_MSG_NLP_CRYPTO_ERROR,
                        BIO_MODULE_SECURITY, BIO_MODULE_ALL, sizeof(msg));
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.channel = BIO_CHANNEL_NOREPINEPHRINE;  // Always alert for errors
    msg.peer_id = peer_id;
    msg.operation_id = operation_id;
    msg.error_code = error_code;
    if (message) {
        strncpy(msg.error_message, message, sizeof(msg.error_message) - 1);
    }
    msg.is_security_violation = is_security_violation;

    bio_router_broadcast(g_crypto_bio_ctx, &msg, sizeof(msg));

    // Also log to BBB for security auditing
    bbb_audit_log(is_security_violation ? BBB_AUDIT_ERROR : BBB_AUDIT_WARNING,
                  NLP_CRYPTO_MODULE, "crypto_error",
                  "op=%u code=%u security=%d: %s",
                  operation_id, error_code, is_security_violation,
                  message ? message : "");
}

//=============================================================================
// CRC-16 Implementation (for header checksums)
//=============================================================================

/**
 * @brief CRC-16-CCITT lookup table (polynomial 0x1021)
 *
 * WHAT: Precomputed CRC values for fast calculation
 * WHY:  Header integrity checking needs to be fast
 * HOW:  Standard CRC-16-CCITT with polynomial 0x1021
 */
static const uint16_t crc16_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
    0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
    0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
    0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
    0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
    0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
    0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
    0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
    0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
    0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
    0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
    0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
    0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
    0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
    0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
    0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
    0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
    0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
    0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
    0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
    0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
    0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
    0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};

uint16_t nlp_crc16(const uint8_t* data, size_t len)
{
    uint16_t crc = 0xFFFF;  // Initial value

    for (size_t i = 0; i < len; i++) {
        crc = (crc << 8) ^ crc16_table[((crc >> 8) ^ data[i]) & 0xFF];
    }

    return crc;
}

//=============================================================================
// Crypto State Management
//=============================================================================

int nlp_crypto_init(nlp_node_t node)
{
    // WHAT: Initialize cryptographic subsystem
    // WHY:  Required before any encryption operations
    // HOW:  Allocate crypto state, seed RNG, detect available libraries

    if (!node) {
        LOG_ERROR("nlp_crypto_init: NULL node");
        return -NIMCP_ERROR_NULL_POINTER;
    }

    // Allocate crypto state
    node->crypto = (nlp_crypto_state_t*)nimcp_calloc(1, sizeof(nlp_crypto_state_t));

    if (!node->crypto) {
        LOG_ERROR("nlp_crypto_init: Failed to allocate crypto state");
        return -NIMCP_ERROR_NO_MEMORY;
    }

    nlp_crypto_state_t* crypto = node->crypto;
    memset(crypto, 0, sizeof(*crypto));

#if NLP_USE_OPENSSL
    // OpenSSL initialization
    LOG_INFO("nlp_crypto_init: Using OpenSSL for AES-256-GCM");
    crypto->use_openssl = true;

    // OpenSSL 3.0+ doesn't need explicit initialization
    // Just verify we can get random bytes
    uint8_t test_rand[16];
    if (RAND_bytes(test_rand, sizeof(test_rand)) != 1) {
        LOG_ERROR("nlp_crypto_init: OpenSSL RAND_bytes failed");
        nimcp_free(node->crypto);
        node->crypto = NULL;
        return -NIMCP_ERROR_OPERATION_FAILED;
    }

#elif NLP_USE_LIBSODIUM
    // Libsodium initialization
    LOG_INFO("nlp_crypto_init: Using libsodium for XChaCha20-Poly1305");
    crypto->use_openssl = false;

    if (sodium_init() < 0) {
        LOG_ERROR("nlp_crypto_init: sodium_init() failed");
        nimcp_free(node->crypto);
        node->crypto = NULL;
        return -NIMCP_ERROR_OPERATION_FAILED;
    }

#else
    // Software fallback (INSECURE - testing only)
    LOG_WARN("nlp_crypto_init: Using INSECURE software fallback");
    crypto->use_openssl = false;
#endif

    // Seed RNG counter with timestamp
    crypto->rng_counter = (uint64_t)time(NULL);
    crypto->rng_seed = (uint32_t)(crypto->rng_counter & 0xFFFFFFFF);

    // Generate HKDF salt
#if NLP_USE_OPENSSL
    RAND_bytes(crypto->hkdf_salt, sizeof(crypto->hkdf_salt));
#elif NLP_USE_LIBSODIUM
    randombytes_buf(crypto->hkdf_salt, sizeof(crypto->hkdf_salt));
#else
    // Fallback: use timestamp as salt (INSECURE)
    for (size_t i = 0; i < sizeof(crypto->hkdf_salt); i++) {
        crypto->hkdf_salt[i] = (uint8_t)(crypto->rng_seed >> (i % 4 * 8));
    }
#endif

    crypto->initialized = true;

    // Register with Blood-Brain Barrier for security auditing
    bbb_register_module(NLP_CRYPTO_MODULE, BBB_MODULE_TYPE_NETWORK);

    // Register with bio-async for cognitive integration
    nlp_crypto_ensure_bio_registered();

    bbb_audit_log(BBB_AUDIT_INFO, NLP_CRYPTO_MODULE, "crypto_init",
                  "Crypto subsystem initialized (OpenSSL=%d, libsodium=%d)",
                  NLP_USE_OPENSSL, NLP_USE_LIBSODIUM);

    LOG_INFO("nlp_crypto_init: Crypto subsystem initialized (OpenSSL=%d, libsodium=%d)",
             NLP_USE_OPENSSL, NLP_USE_LIBSODIUM);

    return 0;
}

void nlp_crypto_shutdown(nlp_node_t node)
{
    // WHAT: Clean up crypto subsystem
    // WHY:  Zero sensitive memory, free resources
    // HOW:  Explicit zero then free

    if (!node || !node->crypto) {
        return;
    }

    nlp_crypto_state_t* crypto = node->crypto;

    LOG_INFO("nlp_crypto_shutdown: Stats - nonces=%lu, enc=%lu, dec=%lu, errors=%lu",
             crypto->nonces_generated, crypto->encryptions_performed,
             crypto->decryptions_performed, crypto->crypto_errors);

    // SECURITY: Zero sensitive data using volatile to prevent compiler optimization
    // The compiler may otherwise remove these writes as "dead stores" since
    // the memory is about to be freed
    volatile uint8_t* salt = crypto->hkdf_salt;
    for (size_t i = 0; i < sizeof(crypto->hkdf_salt); i++) {
        salt[i] = 0;
    }

    // SECURITY: Also zero other sensitive state via volatile
    volatile uint32_t* seed_ptr = &crypto->rng_seed;
    volatile uint64_t* counter_ptr = &crypto->rng_counter;
    *seed_ptr = 0;
    *counter_ptr = 0;

    // Free crypto state
    nimcp_free(node->crypto);
    node->crypto = NULL;

    // Reset bio-async registration so re-initialization works
    if (g_crypto_bio_ctx) {
        bio_router_unregister_module(g_crypto_bio_ctx);
        g_crypto_bio_ctx = NULL;
    }
    g_crypto_bio_once = (nimcp_platform_once_t)NIMCP_PLATFORM_ONCE_INIT;
}

//=============================================================================
// Nonce Generation
//=============================================================================

int nlp_crypto_generate_nonce(nlp_node_t node, uint8_t* nonce)
{
    // WHAT: Generate unique 12-byte nonce for AES-GCM
    // WHY:  Nonce MUST be unique for each message (GCM requirement)
    // HOW:  timestamp (4) + sequence (2) + random (6)

    if (!node || !node->crypto || !nonce) {
        LOG_ERROR("nlp_crypto_generate_nonce: Invalid parameters");
        return -NIMCP_ERROR_NULL_POINTER;
    }

    if (!node->crypto->initialized) {
        LOG_ERROR("nlp_crypto_generate_nonce: Crypto not initialized");
        return -NIMCP_ERROR_NOT_INITIALIZED;
    }

    nlp_crypto_state_t* crypto = node->crypto;

    // Bytes 0-3: Unix timestamp (seconds)
    uint32_t timestamp = (uint32_t)time(NULL);
    nonce[0] = (timestamp >> 24) & 0xFF;
    nonce[1] = (timestamp >> 16) & 0xFF;
    nonce[2] = (timestamp >> 8) & 0xFF;
    nonce[3] = timestamp & 0xFF;

    // Bytes 4-5: Sequence counter (wraps at 65536)
    uint16_t sequence = (uint16_t)(crypto->rng_counter & 0xFFFF);
    nonce[4] = (sequence >> 8) & 0xFF;
    nonce[5] = sequence & 0xFF;

    // Bytes 6-11: Random bytes
#if NLP_USE_OPENSSL
    if (RAND_bytes(&nonce[6], 6) != 1) {
        LOG_ERROR("nlp_crypto_generate_nonce: RAND_bytes failed");
        crypto->crypto_errors++;
        return -NIMCP_ERROR_OPERATION_FAILED;
    }
#elif NLP_USE_LIBSODIUM
    randombytes_buf(&nonce[6], 6);
#else
    // Fallback: pseudo-random (INSECURE)
    for (int i = 6; i < 12; i++) {
        crypto->rng_seed = crypto->rng_seed * 1103515245 + 12345;
        nonce[i] = (crypto->rng_seed >> 16) & 0xFF;
    }
#endif

    // Increment counter
    crypto->rng_counter++;
    crypto->nonces_generated++;

    return 0;
}

//=============================================================================
// AES-256-GCM Encryption (OpenSSL)
//=============================================================================

#if NLP_USE_OPENSSL

int nlp_crypto_encrypt(
    nlp_node_t node,
    const uint8_t* key,
    const uint8_t* nonce,
    const uint8_t* plaintext,
    size_t plaintext_len,
    const uint8_t* aad,
    size_t aad_len,
    uint8_t* ciphertext,
    size_t ciphertext_len,
    uint8_t* tag)
{
    // WHAT: Encrypt with AES-256-GCM
    // WHY:  Industry standard, hardware accelerated, authenticated
    // HOW:  OpenSSL EVP API

    if (!node || !node->crypto || !key || !nonce || !plaintext ||
        !ciphertext || !tag) {
        LOG_ERROR("nlp_crypto_encrypt: Invalid parameters");
        return -NIMCP_ERROR_NULL_POINTER;
    }

    if (!node->crypto->initialized) {
        LOG_ERROR("nlp_crypto_encrypt: Crypto not initialized");
        return -NIMCP_ERROR_NOT_INITIALIZED;
    }

    if (ciphertext_len < plaintext_len) {
        LOG_ERROR("nlp_crypto_encrypt: Output buffer too small");
        return -NIMCP_ERROR_BUFFER_TOO_SMALL;
    }

    EVP_CIPHER_CTX* ctx = NULL;
    int len = 0;
    int ciphertext_out_len = 0;
    int ret = -1;

    // Create and initialize context
    ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        LOG_ERROR("nlp_crypto_encrypt: EVP_CIPHER_CTX_new failed");
        goto cleanup;
    }

    // Initialize encryption (AES-256-GCM)
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) {
        LOG_ERROR("nlp_crypto_encrypt: EVP_EncryptInit_ex failed");
        goto cleanup;
    }

    // Set IV (nonce) length - must be 12 bytes for GCM
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, NLP_CRYPTO_NONCE_SIZE, NULL) != 1) {
        LOG_ERROR("nlp_crypto_encrypt: EVP_CIPHER_CTX_ctrl IV length failed");
        goto cleanup;
    }

    // Initialize key and IV
    if (EVP_EncryptInit_ex(ctx, NULL, NULL, key, nonce) != 1) {
        LOG_ERROR("nlp_crypto_encrypt: EVP_EncryptInit_ex key/IV failed");
        goto cleanup;
    }

    // Add AAD (authenticated but not encrypted - header)
    if (aad && aad_len > 0) {
        if (EVP_EncryptUpdate(ctx, NULL, &len, aad, (int)aad_len) != 1) {
            LOG_ERROR("nlp_crypto_encrypt: EVP_EncryptUpdate AAD failed");
            goto cleanup;
        }
    }

    // Encrypt plaintext
    if (EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, (int)plaintext_len) != 1) {
        LOG_ERROR("nlp_crypto_encrypt: EVP_EncryptUpdate plaintext failed");
        goto cleanup;
    }
    ciphertext_out_len = len;

    // Finalize encryption
    if (EVP_EncryptFinal_ex(ctx, ciphertext + len, &len) != 1) {
        LOG_ERROR("nlp_crypto_encrypt: EVP_EncryptFinal_ex failed");
        goto cleanup;
    }
    ciphertext_out_len += len;

    // Get authentication tag
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, NLP_CRYPTO_TAG_SIZE, tag) != 1) {
        LOG_ERROR("nlp_crypto_encrypt: EVP_CIPHER_CTX_ctrl get tag failed");
        goto cleanup;
    }

    node->crypto->encryptions_performed++;
    ret = ciphertext_out_len;

cleanup:
    if (ctx) {
        EVP_CIPHER_CTX_free(ctx);
    }

    if (ret < 0) {
        node->crypto->crypto_errors++;

        // Log OpenSSL errors
        unsigned long err;
        while ((err = ERR_get_error()) != 0) {
            char err_buf[NIMCP_ERROR_BUFFER_SIZE];
            ERR_error_string_n(err, err_buf, sizeof(err_buf));
            LOG_ERROR("nlp_crypto_encrypt: OpenSSL error: %s", err_buf);
        }
    }

    return ret;
}

int nlp_crypto_decrypt(
    nlp_node_t node,
    const uint8_t* key,
    const uint8_t* nonce,
    const uint8_t* ciphertext,
    size_t ciphertext_len,
    const uint8_t* aad,
    size_t aad_len,
    const uint8_t* tag,
    uint8_t* plaintext,
    size_t plaintext_len)
{
    // WHAT: Decrypt and verify AES-256-GCM
    // WHY:  Ensure message authenticity before processing
    // HOW:  Verify tag first, then decrypt

    if (!node || !node->crypto || !key || !nonce || !ciphertext ||
        !tag || !plaintext) {
        LOG_ERROR("nlp_crypto_decrypt: Invalid parameters");
        return -NIMCP_ERROR_NULL_POINTER;
    }

    if (!node->crypto->initialized) {
        LOG_ERROR("nlp_crypto_decrypt: Crypto not initialized");
        return -NIMCP_ERROR_NOT_INITIALIZED;
    }

    if (plaintext_len < ciphertext_len) {
        LOG_ERROR("nlp_crypto_decrypt: Output buffer too small");
        return -NIMCP_ERROR_BUFFER_TOO_SMALL;
    }

    EVP_CIPHER_CTX* ctx = NULL;
    int len = 0;
    int plaintext_out_len = 0;
    int ret = -1;

    // Create and initialize context
    ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        LOG_ERROR("nlp_crypto_decrypt: EVP_CIPHER_CTX_new failed");
        goto cleanup;
    }

    // Initialize decryption (AES-256-GCM)
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) {
        LOG_ERROR("nlp_crypto_decrypt: EVP_DecryptInit_ex failed");
        goto cleanup;
    }

    // Set IV (nonce) length
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, NLP_CRYPTO_NONCE_SIZE, NULL) != 1) {
        LOG_ERROR("nlp_crypto_decrypt: EVP_CIPHER_CTX_ctrl IV length failed");
        goto cleanup;
    }

    // Initialize key and IV
    if (EVP_DecryptInit_ex(ctx, NULL, NULL, key, nonce) != 1) {
        LOG_ERROR("nlp_crypto_decrypt: EVP_DecryptInit_ex key/IV failed");
        goto cleanup;
    }

    // Add AAD
    if (aad && aad_len > 0) {
        if (EVP_DecryptUpdate(ctx, NULL, &len, aad, (int)aad_len) != 1) {
            LOG_ERROR("nlp_crypto_decrypt: EVP_DecryptUpdate AAD failed");
            goto cleanup;
        }
    }

    // Decrypt ciphertext
    if (EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, (int)ciphertext_len) != 1) {
        LOG_ERROR("nlp_crypto_decrypt: EVP_DecryptUpdate ciphertext failed");
        goto cleanup;
    }
    plaintext_out_len = len;

    // Set expected tag value
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, NLP_CRYPTO_TAG_SIZE,
                            (void*)tag) != 1) {
        LOG_ERROR("nlp_crypto_decrypt: EVP_CIPHER_CTX_ctrl set tag failed");
        goto cleanup;
    }

    // Finalize decryption (verifies tag)
    ret = EVP_DecryptFinal_ex(ctx, plaintext + len, &len);
    if (ret > 0) {
        plaintext_out_len += len;
        node->crypto->decryptions_performed++;
        ret = plaintext_out_len;
    } else {
        LOG_ERROR("nlp_crypto_decrypt: Authentication tag verification failed");
        bbb_audit_log(BBB_AUDIT_WARNING, NLP_CRYPTO_MODULE, "auth_tag_failed",
                      "Message authentication tag verification failed - possible tampering");
        ret = -NIMCP_ERROR_OPERATION_FAILED;
    }

cleanup:
    if (ctx) {
        EVP_CIPHER_CTX_free(ctx);
    }

    if (ret < 0) {
        node->crypto->crypto_errors++;

        // Log OpenSSL errors
        unsigned long err;
        while ((err = ERR_get_error()) != 0) {
            char err_buf[NIMCP_ERROR_BUFFER_SIZE];
            ERR_error_string_n(err, err_buf, sizeof(err_buf));
            LOG_ERROR("nlp_crypto_decrypt: OpenSSL error: %s", err_buf);
        }

        bbb_audit_log(BBB_AUDIT_ERROR, NLP_CRYPTO_MODULE, "crypto_error",
                      "Decryption failed, total errors=%lu", node->crypto->crypto_errors);
    }

    return ret;
}

#elif NLP_USE_LIBSODIUM

//=============================================================================
// XChaCha20-Poly1305 Encryption (libsodium fallback)
//=============================================================================

int nlp_crypto_encrypt(
    nlp_node_t node,
    const uint8_t* key,
    const uint8_t* nonce,
    const uint8_t* plaintext,
    size_t plaintext_len,
    const uint8_t* aad,
    size_t aad_len,
    uint8_t* ciphertext,
    size_t ciphertext_len,
    uint8_t* tag)
{
    // WHAT: Encrypt with XChaCha20-Poly1305
    // WHY:  Fallback when OpenSSL unavailable
    // HOW:  libsodium crypto_aead_xchacha20poly1305_ietf_encrypt_detached

    if (!node || !node->crypto || !key || !nonce || !plaintext ||
        !ciphertext || !tag) {
        LOG_ERROR("nlp_crypto_encrypt: Invalid parameters");
        return -NIMCP_ERROR_NULL_POINTER;
    }

    if (!node->crypto->initialized) {
        LOG_ERROR("nlp_crypto_encrypt: Crypto not initialized");
        return -NIMCP_ERROR_NOT_INITIALIZED;
    }

    if (ciphertext_len < plaintext_len) {
        LOG_ERROR("nlp_crypto_encrypt: Output buffer too small");
        return -NIMCP_ERROR_BUFFER_TOO_SMALL;
    }

    // NOTE: libsodium uses 24-byte nonce, but we'll use first 12 bytes
    // and zero-pad the rest for compatibility
    uint8_t full_nonce[24];
    memcpy(full_nonce, nonce, NLP_CRYPTO_NONCE_SIZE);
    memset(full_nonce + NLP_CRYPTO_NONCE_SIZE, 0, 12);

    unsigned long long tag_len = 0;

    if (crypto_aead_xchacha20poly1305_ietf_encrypt_detached(
            ciphertext,
            tag,
            &tag_len,
            plaintext,
            plaintext_len,
            aad,
            aad ? aad_len : 0,
            NULL,  // nsec (not used)
            full_nonce,
            key) != 0) {
        LOG_ERROR("nlp_crypto_encrypt: libsodium encryption failed");
        node->crypto->crypto_errors++;
        return -NIMCP_ERROR_OPERATION_FAILED;
    }

    node->crypto->encryptions_performed++;
    return (int)plaintext_len;
}

int nlp_crypto_decrypt(
    nlp_node_t node,
    const uint8_t* key,
    const uint8_t* nonce,
    const uint8_t* ciphertext,
    size_t ciphertext_len,
    const uint8_t* aad,
    size_t aad_len,
    const uint8_t* tag,
    uint8_t* plaintext,
    size_t plaintext_len)
{
    // WHAT: Decrypt and verify XChaCha20-Poly1305
    // WHY:  Verify authenticity before processing
    // HOW:  libsodium crypto_aead_xchacha20poly1305_ietf_decrypt_detached

    if (!node || !node->crypto || !key || !nonce || !ciphertext ||
        !tag || !plaintext) {
        LOG_ERROR("nlp_crypto_decrypt: Invalid parameters");
        return -NIMCP_ERROR_NULL_POINTER;
    }

    if (!node->crypto->initialized) {
        LOG_ERROR("nlp_crypto_decrypt: Crypto not initialized");
        return -NIMCP_ERROR_NOT_INITIALIZED;
    }

    if (plaintext_len < ciphertext_len) {
        LOG_ERROR("nlp_crypto_decrypt: Output buffer too small");
        return -NIMCP_ERROR_BUFFER_TOO_SMALL;
    }

    // Zero-pad nonce to 24 bytes
    uint8_t full_nonce[24];
    memcpy(full_nonce, nonce, NLP_CRYPTO_NONCE_SIZE);
    memset(full_nonce + NLP_CRYPTO_NONCE_SIZE, 0, 12);

    if (crypto_aead_xchacha20poly1305_ietf_decrypt_detached(
            plaintext,
            NULL,  // nsec (not used)
            ciphertext,
            ciphertext_len,
            tag,
            aad,
            aad ? aad_len : 0,
            full_nonce,
            key) != 0) {
        LOG_ERROR("nlp_crypto_decrypt: Authentication failed");
        node->crypto->crypto_errors++;
        return -NIMCP_ERROR_OPERATION_FAILED;
    }

    node->crypto->decryptions_performed++;
    return (int)ciphertext_len;
}

#else

//=============================================================================
// Software Fallback (INSECURE - testing only)
//=============================================================================

int nlp_crypto_encrypt(
    nlp_node_t node,
    const uint8_t* key,
    const uint8_t* nonce,
    const uint8_t* plaintext,
    size_t plaintext_len,
    const uint8_t* aad,
    size_t aad_len,
    uint8_t* ciphertext,
    size_t ciphertext_len,
    uint8_t* tag)
{
    LOG_WARN("nlp_crypto_encrypt: Using INSECURE software fallback!");

    if (!node || !node->crypto || !key || !nonce || !plaintext ||
        !ciphertext || !tag) {
        return -NIMCP_ERROR_NULL_POINTER;
    }

    if (ciphertext_len < plaintext_len) {
        return -NIMCP_ERROR_BUFFER_TOO_SMALL;
    }

    // Simple XOR "encryption" (NOT SECURE)
    for (size_t i = 0; i < plaintext_len; i++) {
        ciphertext[i] = plaintext[i] ^ key[i % 32] ^ nonce[i % 12];
    }

    // Fake tag (just copy first 16 bytes of ciphertext)
    memcpy(tag, ciphertext, (plaintext_len < 16) ? plaintext_len : 16);

    node->crypto->encryptions_performed++;
    return (int)plaintext_len;
}

int nlp_crypto_decrypt(
    nlp_node_t node,
    const uint8_t* key,
    const uint8_t* nonce,
    const uint8_t* ciphertext,
    size_t ciphertext_len,
    const uint8_t* aad,
    size_t aad_len,
    const uint8_t* tag,
    uint8_t* plaintext,
    size_t plaintext_len)
{
    LOG_WARN("nlp_crypto_decrypt: Using INSECURE software fallback!");

    if (!node || !node->crypto || !key || !nonce || !ciphertext ||
        !tag || !plaintext) {
        return -NIMCP_ERROR_NULL_POINTER;
    }

    if (plaintext_len < ciphertext_len) {
        return -NIMCP_ERROR_BUFFER_TOO_SMALL;
    }

    // Simple XOR "decryption" (same as encryption)
    for (size_t i = 0; i < ciphertext_len; i++) {
        plaintext[i] = ciphertext[i] ^ key[i % 32] ^ nonce[i % 12];
    }

    node->crypto->decryptions_performed++;
    return (int)ciphertext_len;
}

#endif

//=============================================================================
// Key Derivation (HKDF)
//=============================================================================

int nlp_crypto_derive_session_key(
    nlp_node_t node,
    const uint8_t* shared_secret,
    size_t secret_len,
    const uint8_t* info,
    size_t info_len,
    uint8_t* session_key)
{
    // WHAT: Derive session key from shared secret using HKDF
    // WHY:  Convert ECDH output to usable AES key with context binding
    // HOW:  HKDF-SHA256 (OpenSSL) or BLAKE2b (libsodium)

    if (!node || !node->crypto || !shared_secret || !session_key) {
        LOG_ERROR("nlp_crypto_derive_session_key: Invalid parameters");
        return -NIMCP_ERROR_NULL_POINTER;
    }

    if (!node->crypto->initialized) {
        LOG_ERROR("nlp_crypto_derive_session_key: Crypto not initialized");
        return -NIMCP_ERROR_NOT_INITIALIZED;
    }

    if (secret_len == 0 || secret_len > 1024) {
        LOG_ERROR("nlp_crypto_derive_session_key: Invalid secret length %zu", secret_len);
        return -NIMCP_ERROR_INVALID_PARAMETER;
    }

#if NLP_USE_OPENSSL && OPENSSL_VERSION_NUMBER >= 0x10100000L
    // Use OpenSSL HKDF (OpenSSL 1.1.0+)
    EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, NULL);
    if (!pctx) {
        LOG_ERROR("nlp_crypto_derive_session_key: EVP_PKEY_CTX_new_id failed");
        return -NIMCP_ERROR_OPERATION_FAILED;
    }

    int ret = -1;

    if (EVP_PKEY_derive_init(pctx) <= 0) {
        LOG_ERROR("nlp_crypto_derive_session_key: EVP_PKEY_derive_init failed");
        goto hkdf_cleanup;
    }

    if (EVP_PKEY_CTX_set_hkdf_md(pctx, EVP_sha256()) <= 0) {
        LOG_ERROR("nlp_crypto_derive_session_key: EVP_PKEY_CTX_set_hkdf_md failed");
        goto hkdf_cleanup;
    }

    if (EVP_PKEY_CTX_set1_hkdf_salt(pctx, node->crypto->hkdf_salt,
                                     sizeof(node->crypto->hkdf_salt)) <= 0) {
        LOG_ERROR("nlp_crypto_derive_session_key: EVP_PKEY_CTX_set1_hkdf_salt failed");
        goto hkdf_cleanup;
    }

    if (EVP_PKEY_CTX_set1_hkdf_key(pctx, shared_secret, (int)secret_len) <= 0) {
        LOG_ERROR("nlp_crypto_derive_session_key: EVP_PKEY_CTX_set1_hkdf_key failed");
        goto hkdf_cleanup;
    }

    if (info && info_len > 0) {
        if (EVP_PKEY_CTX_add1_hkdf_info(pctx, info, (int)info_len) <= 0) {
            LOG_ERROR("nlp_crypto_derive_session_key: EVP_PKEY_CTX_add1_hkdf_info failed");
            goto hkdf_cleanup;
        }
    }

    size_t outlen = NLP_CRYPTO_KEY_SIZE;
    if (EVP_PKEY_derive(pctx, session_key, &outlen) <= 0) {
        LOG_ERROR("nlp_crypto_derive_session_key: EVP_PKEY_derive failed");
        goto hkdf_cleanup;
    }

    ret = 0;

hkdf_cleanup:
    EVP_PKEY_CTX_free(pctx);
    return ret;

#elif NLP_USE_LIBSODIUM
    // Use libsodium BLAKE2b for key derivation
    // Note: This is not HKDF, but provides similar security properties

    crypto_generichash_state state;

    if (crypto_generichash_init(&state, node->crypto->hkdf_salt,
                                sizeof(node->crypto->hkdf_salt),
                                NLP_CRYPTO_KEY_SIZE) != 0) {
        LOG_ERROR("nlp_crypto_derive_session_key: crypto_generichash_init failed");
        return -NIMCP_ERROR_OPERATION_FAILED;
    }

    crypto_generichash_update(&state, shared_secret, secret_len);

    if (info && info_len > 0) {
        crypto_generichash_update(&state, info, info_len);
    }

    if (crypto_generichash_final(&state, session_key, NLP_CRYPTO_KEY_SIZE) != 0) {
        LOG_ERROR("nlp_crypto_derive_session_key: crypto_generichash_final failed");
        return -NIMCP_ERROR_OPERATION_FAILED;
    }

    return 0;

#else
    // Software fallback: simple hash (INSECURE)
    LOG_WARN("nlp_crypto_derive_session_key: Using insecure hash");

    // Simple hash: XOR all input bytes
    nimcp_secure_zero(session_key, NLP_CRYPTO_KEY_SIZE);

    for (size_t i = 0; i < secret_len; i++) {
        session_key[i % NLP_CRYPTO_KEY_SIZE] ^= shared_secret[i];
    }

    for (size_t i = 0; i < sizeof(node->crypto->hkdf_salt); i++) {
        session_key[i % NLP_CRYPTO_KEY_SIZE] ^= node->crypto->hkdf_salt[i];
    }

    if (info && info_len > 0) {
        for (size_t i = 0; i < info_len; i++) {
            session_key[i % NLP_CRYPTO_KEY_SIZE] ^= info[i];
        }
    }

    return 0;
#endif
}

//=============================================================================
// Keypair Generation (X25519)
//=============================================================================

int nlp_crypto_generate_keypair(
    nlp_node_t node,
    uint8_t* public_key,
    uint8_t* private_key)
{
    // WHAT: Generate X25519 keypair for ECDH key exchange
    // WHY:  Enable perfect forward secrecy with ephemeral keys
    // HOW:  libsodium crypto_box_keypair or OpenSSL EVP

    if (!node || !node->crypto || !public_key || !private_key) {
        LOG_ERROR("nlp_crypto_generate_keypair: Invalid parameters");
        return -NIMCP_ERROR_NULL_POINTER;
    }

    if (!node->crypto->initialized) {
        LOG_ERROR("nlp_crypto_generate_keypair: Crypto not initialized");
        return -NIMCP_ERROR_NOT_INITIALIZED;
    }

#if NLP_USE_LIBSODIUM
    // Use libsodium (simplest)
    if (crypto_box_keypair(public_key, private_key) != 0) {
        LOG_ERROR("nlp_crypto_generate_keypair: crypto_box_keypair failed");
        return -NIMCP_ERROR_OPERATION_FAILED;
    }

    return 0;

#elif NLP_USE_OPENSSL && OPENSSL_VERSION_NUMBER >= 0x10100000L
    // Use OpenSSL EVP for X25519
    EVP_PKEY* pkey = NULL;
    EVP_PKEY_CTX* pctx = NULL;
    int ret = -1;

    pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, NULL);
    if (!pctx) {
        LOG_ERROR("nlp_crypto_generate_keypair: EVP_PKEY_CTX_new_id failed");
        goto keygen_cleanup;
    }

    if (EVP_PKEY_keygen_init(pctx) <= 0) {
        LOG_ERROR("nlp_crypto_generate_keypair: EVP_PKEY_keygen_init failed");
        goto keygen_cleanup;
    }

    if (EVP_PKEY_keygen(pctx, &pkey) <= 0) {
        LOG_ERROR("nlp_crypto_generate_keypair: EVP_PKEY_keygen failed");
        goto keygen_cleanup;
    }

    // Extract raw public key
    size_t pub_len = 32;
    if (EVP_PKEY_get_raw_public_key(pkey, public_key, &pub_len) <= 0 || pub_len != 32) {
        LOG_ERROR("nlp_crypto_generate_keypair: EVP_PKEY_get_raw_public_key failed");
        goto keygen_cleanup;
    }

    // Extract raw private key
    size_t priv_len = 32;
    if (EVP_PKEY_get_raw_private_key(pkey, private_key, &priv_len) <= 0 || priv_len != 32) {
        LOG_ERROR("nlp_crypto_generate_keypair: EVP_PKEY_get_raw_private_key failed");
        goto keygen_cleanup;
    }

    ret = 0;

keygen_cleanup:
    if (pkey) EVP_PKEY_free(pkey);
    if (pctx) EVP_PKEY_CTX_free(pctx);
    return ret;

#else
    // Software fallback (INSECURE - just random bytes)
    LOG_WARN("nlp_crypto_generate_keypair: Using insecure random keypair");

    for (int i = 0; i < 32; i++) {
        node->crypto->rng_seed = node->crypto->rng_seed * 1103515245 + 12345;
        private_key[i] = (node->crypto->rng_seed >> 16) & 0xFF;

        node->crypto->rng_seed = node->crypto->rng_seed * 1103515245 + 12345;
        public_key[i] = (node->crypto->rng_seed >> 16) & 0xFF;
    }

    return 0;
#endif
}
