/**
 * @file nimcp_encryption.h
 * @brief Encryption utilities for NIMCP serialization
 *
 * Provides authenticated encryption for network serialization using libsodium.
 * Supports password-based encryption with secure key derivation.
 *
 * SECURITY FEATURES:
 * - Argon2id key derivation (memory-hard, resistant to GPU attacks)
 * - XChaCha20-Poly1305 authenticated encryption (confidentiality + integrity)
 * - Random salts and nonces (prevents rainbow table attacks)
 * - Constant-time operations (resistant to timing attacks)
 *
 * USAGE:
 * - Encryption is OPTIONAL at runtime
 * - If libsodium unavailable at build time, functions return errors
 * - Applications choose whether to encrypt per-call
 */

#ifndef NIMCP_ENCRYPTION_H
#define NIMCP_ENCRYPTION_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Check if encryption support is available
 * @return true if compiled with libsodium, false otherwise
 */
bool nimcp_encryption_available(void);

/**
 * @brief Get required size for encrypted data
 * @param plaintext_len Length of plaintext in bytes
 * @return Size needed for encrypted output (includes salt, nonce, tag)
 */
size_t nimcp_encrypted_size(size_t plaintext_len);

/**
 * @brief Encrypt data with password-based encryption
 *
 * Uses Argon2id for key derivation and XChaCha20-Poly1305 for encryption.
 * Format: [salt (16 bytes)][nonce (24 bytes)][ciphertext + auth tag]
 *
 * @param plaintext Input data to encrypt
 * @param plaintext_len Length of plaintext in bytes
 * @param password Password for key derivation (UTF-8 string)
 * @param password_len Length of password in bytes
 * @param ciphertext Output buffer (must be at least nimcp_encrypted_size(plaintext_len))
 * @param ciphertext_len Pointer to size of output buffer, updated with actual size
 * @return true on success, false on error
 */
bool nimcp_encrypt_with_password(
    const uint8_t* plaintext,
    size_t plaintext_len,
    const char* password,
    size_t password_len,
    uint8_t* ciphertext,
    size_t* ciphertext_len
);

/**
 * @brief Decrypt data encrypted with nimcp_encrypt_with_password
 *
 * @param ciphertext Encrypted data (includes salt, nonce, ciphertext, tag)
 * @param ciphertext_len Length of encrypted data
 * @param password Password for key derivation (UTF-8 string)
 * @param password_len Length of password in bytes
 * @param plaintext Output buffer for decrypted data
 * @param plaintext_len Pointer to size of output buffer, updated with actual size
 * @return true on success, false on authentication/decryption failure
 */
bool nimcp_decrypt_with_password(
    const uint8_t* ciphertext,
    size_t ciphertext_len,
    const char* password,
    size_t password_len,
    uint8_t* plaintext,
    size_t* plaintext_len
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_ENCRYPTION_H
