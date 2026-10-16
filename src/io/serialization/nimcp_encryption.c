
#define LOG_MODULE "nimcp_encryption"
#define LOG_MODULE_ID 0x052E
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(encryption)

/**
 * @file nimcp_encryption.c
 * @brief Implementation of encryption utilities for NIMCP serialization
 */

#include "io/serialization/nimcp_encryption.h"
#include <string.h>

// Conditionally compile encryption support based on CMake detection
#ifdef NIMCP_ENABLE_ENCRYPTION

#include <sodium.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

// Global BBB security system
static bbb_system_t g_bbb_system = NULL;



//=============================================================================
// Security Initialization
//=============================================================================

/**
 * @brief Initialize security subsystem for encryption
 *
 * WHAT: Create and configure BBB system for input validation
 * WHY: Protect against malicious external input
 * HOW: Initialize with conservative security settings
 */
static void encryption_security_init(void) {
    if (g_bbb_system) {
        return;  // Already initialized
    }

    bbb_config_t config = bbb_default_config();
    config.strict_mode = false;  // Don't block, just log
    config.default_action = BBB_ACTION_LOG;
    config.input.validate_strings = true;
    config.input.validate_integers = true;
    config.input.max_string_length = 4096;  // Reasonable limit

    g_bbb_system = bbb_system_create(&config);
    if (!g_bbb_system) {
        LOG_ERROR("encryption: Failed to initialize security subsystem");
    } else {
        LOG_INFO("encryption: Security subsystem initialized");
    }
}

/**
 * @brief Cleanup security subsystem
 * WHY: __attribute__((destructor)) ensures cleanup on library unload,
 *      since this static function would otherwise never be called.
 */
__attribute__((destructor))
static void encryption_security_cleanup(void) {
    if (g_bbb_system) {
        bbb_system_destroy(g_bbb_system);
        g_bbb_system = NULL;
    }
}


// Constants for our encryption format
#define SALT_BYTES 16
#define NONCE_BYTES crypto_aead_xchacha20poly1305_ietf_NPUBBYTES  // 24 bytes
#define KEY_BYTES crypto_aead_xchacha20poly1305_ietf_KEYBYTES    // 32 bytes
#define TAG_BYTES crypto_aead_xchacha20poly1305_ietf_ABYTES      // 16 bytes

// Argon2id parameters (balanced security/performance)
#define ARGON2_OPSLIMIT crypto_pwhash_OPSLIMIT_INTERACTIVE
#define ARGON2_MEMLIMIT crypto_pwhash_MEMLIMIT_INTERACTIVE
#define ARGON2_ALG crypto_pwhash_ALG_ARGON2ID13

bool nimcp_encryption_available(void)
{
    return true;
}

size_t nimcp_encrypted_size(size_t plaintext_len)
{
    return SALT_BYTES + NONCE_BYTES + plaintext_len + TAG_BYTES;
}

/**
 * @brief Derive encryption key from password using Argon2id
 * @param password User password
 * @param password_len Length of password
 * @param salt Random salt
 * @param key Output buffer for derived key (KEY_BYTES)
 * @return true on success, false on error
 */
static bool derive_key_from_password(
    const char* password,
    size_t password_len,
    const uint8_t* salt,
    uint8_t* key
)
{
    if (!password || !salt || !key || password_len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "derive_key_from_password: invalid parameters");

            return false;
    }

    // Derive key using Argon2id
    if (crypto_pwhash(
            key, KEY_BYTES,
            password, password_len,
            salt,
            ARGON2_OPSLIMIT,
            ARGON2_MEMLIMIT,
            ARGON2_ALG
        ) != 0) {
        // Out of memory or invalid parameters
        NIMCP_THROW_IO(NIMCP_ERROR_OPERATION_FAILED, "encryption",
                      "Argon2id key derivation failed - out of memory or invalid parameters");
        return false;
    }

    return true;
}

bool nimcp_encrypt_with_password(
    const uint8_t* plaintext,
    size_t plaintext_len,
    const char* password,
    size_t password_len,
    uint8_t* ciphertext,
    size_t* ciphertext_len
)
{
    // Validate inputs
    if (!plaintext || !password || !ciphertext || !ciphertext_len) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "nimcp_encrypt_with_password: invalid parameters");

            return false;
    }

    if (plaintext_len == 0 || password_len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_encrypt_with_password: plaintext_len is zero");
        return false;
    }

    // Check output buffer size
    size_t required_size = nimcp_encrypted_size(plaintext_len);
    if (*ciphertext_len < required_size) {
        *ciphertext_len = required_size;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_encrypt_with_password: validation failed");
        return false;
    }

    // Initialize libsodium (safe to call multiple times)
    if (sodium_init() < 0) {
        NIMCP_THROW_IO(NIMCP_ERROR_NOT_INITIALIZED, "libsodium",
                      "Failed to initialize libsodium encryption library");
        return false;
    }

    // Generate random salt
    uint8_t salt[SALT_BYTES];
    randombytes_buf(salt, SALT_BYTES);

    // Derive encryption key from password
    uint8_t key[KEY_BYTES];
    if (!derive_key_from_password(password, password_len, salt, key)) {
        sodium_memzero(key, KEY_BYTES);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_encrypt_with_password: derive_key_from_password is NULL");
        return false;
    }

    // Generate random nonce
    uint8_t nonce[NONCE_BYTES];
    randombytes_buf(nonce, NONCE_BYTES);

    // Build output: [salt][nonce][ciphertext+tag]
    memcpy(ciphertext, salt, SALT_BYTES);
    memcpy(ciphertext + SALT_BYTES, nonce, NONCE_BYTES);

    // Encrypt with authenticated encryption
    unsigned long long actual_ciphertext_len;
    if (crypto_aead_xchacha20poly1305_ietf_encrypt(
            ciphertext + SALT_BYTES + NONCE_BYTES,  // Output: ciphertext + tag
            &actual_ciphertext_len,
            plaintext,                               // Input plaintext
            plaintext_len,
            NULL, 0,                                 // No additional data
            NULL,                                    // No secret nonce (we use random)
            nonce,
            key
        ) != 0) {
        // Encryption failed
        NIMCP_THROW_IO(NIMCP_ERROR_OPERATION_FAILED, "encryption",
                      "XChaCha20-Poly1305 encryption failed for data of size %zu", plaintext_len);
        sodium_memzero(key, KEY_BYTES);
        return false;
    }

    // Clean up sensitive data
    sodium_memzero(key, KEY_BYTES);

    // Set actual output size
    *ciphertext_len = SALT_BYTES + NONCE_BYTES + actual_ciphertext_len;

    return true;
}

bool nimcp_decrypt_with_password(
    const uint8_t* ciphertext,
    size_t ciphertext_len,
    const char* password,
    size_t password_len,
    uint8_t* plaintext,
    size_t* plaintext_len
)
{
    // Validate inputs
    if (!ciphertext || !password || !plaintext || !plaintext_len) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "nimcp_decrypt_with_password: invalid parameters");

            return false;
    }

    if (password_len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_decrypt_with_password: password_len is zero");
        return false;
    }

    // Check minimum ciphertext size
    size_t min_size = SALT_BYTES + NONCE_BYTES + TAG_BYTES;
    if (ciphertext_len < min_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_decrypt_with_password: validation failed");
        return false;
    }

    // Initialize libsodium
    if (sodium_init() < 0) {
        NIMCP_THROW_IO(NIMCP_ERROR_NOT_INITIALIZED, "libsodium",
                      "Failed to initialize libsodium encryption library");
        return false;
    }

    // Extract salt and nonce from ciphertext
    const uint8_t* salt = ciphertext;
    const uint8_t* nonce = ciphertext + SALT_BYTES;
    const uint8_t* encrypted_data = ciphertext + SALT_BYTES + NONCE_BYTES;
    size_t encrypted_data_len = ciphertext_len - SALT_BYTES - NONCE_BYTES;

    // Derive decryption key from password
    uint8_t key[KEY_BYTES];
    if (!derive_key_from_password(password, password_len, salt, key)) {
        sodium_memzero(key, KEY_BYTES);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_decrypt_with_password: derive_key_from_password is NULL");
        return false;
    }

    // Check output buffer size
    size_t expected_plaintext_len = encrypted_data_len - TAG_BYTES;
    if (*plaintext_len < expected_plaintext_len) {
        *plaintext_len = expected_plaintext_len;
        sodium_memzero(key, KEY_BYTES);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_decrypt_with_password: validation failed");
        return false;
    }

    // Decrypt and verify authentication tag
    unsigned long long actual_plaintext_len;
    if (crypto_aead_xchacha20poly1305_ietf_decrypt(
            plaintext,                    // Output plaintext
            &actual_plaintext_len,
            NULL,                         // No secret nonce
            encrypted_data,               // Input: ciphertext + tag
            encrypted_data_len,
            NULL, 0,                      // No additional data
            nonce,
            key
        ) != 0) {
        // Decryption failed (wrong password or tampered data)
        NIMCP_THROW_IO(NIMCP_ERROR_OPERATION_FAILED, "decryption",
                      "XChaCha20-Poly1305 decryption failed - wrong password or tampered data");
        sodium_memzero(key, KEY_BYTES);
        return false;
    }

    // Clean up sensitive data
    sodium_memzero(key, KEY_BYTES);

    // Set actual output size
    *plaintext_len = actual_plaintext_len;

    return true;
}

#else  // NIMCP_ENABLE_ENCRYPTION not defined

#include <stdio.h>

/**
 * WARNING: Encryption is NOT available in this build.
 * libsodium was not found at compile time.
 * All encrypt/decrypt calls will fail and return false.
 * Data will NOT be encrypted — it will be stored/transmitted in plaintext.
 */

// Stub implementations when encryption is not available
bool nimcp_encryption_available(void)
{
    return false;  /* Encryption not compiled - normal query result */
}

size_t nimcp_encrypted_size(size_t plaintext_len)
{
    (void)plaintext_len;
    return 0;
}

bool nimcp_encrypt_with_password(
    const uint8_t* plaintext,
    size_t plaintext_len,
    const char* password,
    size_t password_len,
    uint8_t* ciphertext,
    size_t* ciphertext_len
)
{
    (void)plaintext;
    (void)plaintext_len;
    (void)password;
    (void)password_len;
    (void)ciphertext;
    (void)ciphertext_len;
    fprintf(stderr, "WARNING: nimcp_encrypt_with_password called but encryption "
            "is unavailable (libsodium not compiled). Data will NOT be encrypted.\n");
    return false;
}

bool nimcp_decrypt_with_password(
    const uint8_t* ciphertext,
    size_t ciphertext_len,
    const char* password,
    size_t password_len,
    uint8_t* plaintext,
    size_t* plaintext_len
)
{
    (void)ciphertext;
    (void)ciphertext_len;
    (void)password;
    (void)password_len;
    (void)plaintext;
    (void)plaintext_len;
    fprintf(stderr, "WARNING: nimcp_decrypt_with_password called but encryption "
            "is unavailable (libsodium not compiled). Cannot decrypt data.\n");
    return false;
}

#endif  // NIMCP_ENABLE_ENCRYPTION
