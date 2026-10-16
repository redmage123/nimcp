//=============================================================================
// nimcp_serialization.h - Serialization Utilities with Compression/Encryption
//=============================================================================
/**
 * @file nimcp_serialization.h
 * @brief Modular serialization helpers for NIMCP persistence
 *
 * WHAT: Compression and encryption utilities for serialization
 * WHY:  Support NIMCP_FORMAT_FLAG_COMPRESSED and NIMCP_FORMAT_FLAG_ENCRYPTED
 * HOW:  zlib for compression (with fallback), AES/XOR for encryption
 *
 * USAGE:
 *   // Compress data
 *   size_t comp_size;
 *   uint8_t* compressed = nimcp_compress(data, size, &comp_size);
 *
 *   // Decompress data
 *   size_t decomp_size;
 *   uint8_t* decompressed = nimcp_decompress(compressed, comp_size, &decomp_size);
 *
 * PERFORMANCE:
 *   - Compression: O(n) with moderate constant factor
 *   - Encryption: O(n) linear time
 */

#ifndef NIMCP_SERIALIZATION_H
#define NIMCP_SERIALIZATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Feature Detection
//=============================================================================

/**
 * @brief Check if zlib compression is available
 * @return true if zlib is available, false otherwise
 */
bool nimcp_compression_available(void);

/**
 * @brief Check if AES encryption is available
 * @return true if AES is available, false for XOR fallback
 */
bool nimcp_aes_available(void);

//=============================================================================
// Compression Functions
//=============================================================================

/**
 * @brief Compress data using zlib (or no-op fallback)
 *
 * WHAT: Compress binary data for storage
 * WHY:  Reduce file sizes for brain persistence
 * HOW:  zlib deflate if available, otherwise returns copy
 *
 * @param data Input data buffer
 * @param size Size of input data
 * @param out_size Output: size of compressed data
 * @return Compressed data (caller must free), or NULL on error
 */
uint8_t* nimcp_compress(const uint8_t* data, size_t size, size_t* out_size);

/**
 * @brief Decompress data compressed by nimcp_compress
 *
 * WHAT: Decompress binary data from storage
 * WHY:  Restore compressed brain persistence data
 * HOW:  zlib inflate if available, otherwise returns copy
 *
 * @param data Compressed data buffer
 * @param size Size of compressed data
 * @param out_size Output: size of decompressed data
 * @return Decompressed data (caller must free), or NULL on error
 */
uint8_t* nimcp_decompress(const uint8_t* data, size_t size, size_t* out_size);

//=============================================================================
// Encryption Functions
//=============================================================================

/**
 * @brief Encrypt data for secure storage
 *
 * WHAT: Encrypt binary data with key
 * WHY:  Protect sensitive brain state data
 * HOW:  AES-256-CBC if available, XOR cipher fallback
 *
 * @param data Input data buffer
 * @param size Size of input data
 * @param key Encryption key (32 bytes for AES, any length for XOR)
 * @param key_size Size of encryption key
 * @param out_size Output: size of encrypted data
 * @return Encrypted data (caller must free), or NULL on error
 */
uint8_t* nimcp_encrypt(const uint8_t* data, size_t size,
                       const uint8_t* key, size_t key_size,
                       size_t* out_size);

/**
 * @brief Decrypt data encrypted by nimcp_encrypt
 *
 * WHAT: Decrypt binary data with key
 * WHY:  Restore encrypted brain persistence data
 * HOW:  AES-256-CBC if available, XOR cipher fallback
 *
 * @param data Encrypted data buffer
 * @param size Size of encrypted data
 * @param key Decryption key (must match encryption key)
 * @param key_size Size of decryption key
 * @param out_size Output: size of decrypted data
 * @return Decrypted data (caller must free), or NULL on error
 */
uint8_t* nimcp_decrypt(const uint8_t* data, size_t size,
                       const uint8_t* key, size_t key_size,
                       size_t* out_size);

//=============================================================================
// Stream Processing Functions
//=============================================================================

/**
 * @brief Read and decompress/decrypt file content
 *
 * WHAT: Read file with optional decompression/decryption
 * WHY:  Unified interface for loading persistence files
 * HOW:  Detect format flags and apply appropriate transforms
 *
 * @param file Open file handle (positioned after header)
 * @param flags Format flags (NIMCP_FORMAT_FLAG_*)
 * @param size Size of raw data to read
 * @param key Encryption key (NULL if not encrypted)
 * @param key_size Size of encryption key
 * @param out_size Output: size of processed data
 * @return Processed data (caller must free), or NULL on error
 */
uint8_t* nimcp_read_processed(FILE* file, uint32_t flags, size_t size,
                              const uint8_t* key, size_t key_size,
                              size_t* out_size);

/**
 * @brief Write compressed/encrypted data to file
 *
 * WHAT: Write data with optional compression/encryption
 * WHY:  Unified interface for saving persistence files
 * HOW:  Apply transforms based on format flags
 *
 * @param file Open file handle (positioned for writing)
 * @param data Data to write
 * @param size Size of data
 * @param flags Format flags (NIMCP_FORMAT_FLAG_*)
 * @param key Encryption key (NULL if not encrypted)
 * @param key_size Size of encryption key
 * @return true on success, false on error
 */
bool nimcp_write_processed(FILE* file, const uint8_t* data, size_t size,
                           uint32_t flags, const uint8_t* key, size_t key_size);

//=============================================================================
// Serialization Context
//=============================================================================

/**
 * @brief Serialization context for stateful operations
 */
typedef struct {
    uint32_t flags;           /**< Format flags */
    uint8_t key[32];          /**< Encryption key */
    size_t key_size;          /**< Key size */
    bool key_set;             /**< Whether key is set */
} nimcp_serialize_ctx_t;

/**
 * @brief Initialize serialization context
 */
void nimcp_serialize_ctx_init(nimcp_serialize_ctx_t* ctx);

/**
 * @brief Set encryption key for context
 */
void nimcp_serialize_ctx_set_key(nimcp_serialize_ctx_t* ctx,
                                  const uint8_t* key, size_t key_size);

/**
 * @brief Set format flags for context
 */
void nimcp_serialize_ctx_set_flags(nimcp_serialize_ctx_t* ctx, uint32_t flags);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SERIALIZATION_H */
