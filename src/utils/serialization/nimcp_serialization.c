#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_serialization.c - Serialization Utilities Implementation
//=============================================================================
/**
 * @file nimcp_serialization.c
 * @brief Implementation of compression and encryption for serialization
 *
 * WHAT: Modular compression/encryption utilities
 * WHY:  Support NIMCP_FORMAT_FLAG_COMPRESSED and NIMCP_FORMAT_FLAG_ENCRYPTED
 * HOW:  zlib for compression (optional), XOR cipher for encryption fallback
 */

#include "utils/serialization/nimcp_serialization.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/logging/nimcp_logging.h"
#include <string.h>
#include <stdlib.h>

//=============================================================================
// Optional zlib Support
//=============================================================================

#ifdef NIMCP_HAVE_ZLIB
#include <zlib.h>
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(serialization)

#define ZLIB_AVAILABLE 1
#else
#define ZLIB_AVAILABLE 0
#endif

//=============================================================================
// Feature Detection
//=============================================================================

bool nimcp_compression_available(void)
{
    LOG_TRACE("Entering nimcp_compression_available");
    return ZLIB_AVAILABLE != 0;
}

bool nimcp_aes_available(void)
{
    LOG_TRACE("Entering nimcp_aes_available");
    // AES not yet implemented - using XOR fallback
    return false;  /* Not available - normal query result */
}

//=============================================================================
// Compression Implementation
//=============================================================================

#if ZLIB_AVAILABLE

/**
 * @brief Compress using zlib deflate
 */
static uint8_t* compress_zlib(const uint8_t* data, size_t size, size_t* out_size)
{
    // Estimate output size (zlib worst case)
    uLong bound = compressBound((uLong)size);
    uint8_t* compressed = nimcp_malloc(bound + sizeof(uint32_t));

    if (!compressed) {
        LOG_ERROR("Failed to allocate compression buffer");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, bound + sizeof(uint32_t), "Failed to allocate compression buffer");
        *out_size = 0;
        return NULL;
    }

    // Store original size at beginning
    memcpy(compressed, &size, sizeof(uint32_t));

    // Compress data
    uLong comp_len = bound;
    int result = compress2(compressed + sizeof(uint32_t), &comp_len,
                           data, (uLong)size, Z_DEFAULT_COMPRESSION);

    if (result != Z_OK) {
        nimcp_free(compressed);
        *out_size = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "compress_zlib: validation failed");
        return NULL;
    }

    *out_size = (size_t)comp_len + sizeof(uint32_t);
    return compressed;
}

/**
 * @brief Decompress using zlib inflate
 */
static uint8_t* decompress_zlib(const uint8_t* data, size_t size, size_t* out_size)
{
    if (size < sizeof(uint32_t)) {
        *out_size = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "decompress_zlib: validation failed");
        return NULL;
    }

    // Read original size
    uint32_t orig_size;
    memcpy(&orig_size, data, sizeof(uint32_t));

    /* P1 fix: Sanity-check orig_size to prevent enormous allocation from crafted data.
     * Typical compression ratios don't exceed 1000:1. */
    if (orig_size > (size - sizeof(uint32_t)) * 1024) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "decompress_zlib: orig_size %u implausibly large for %zu bytes of compressed data",
            orig_size, size - sizeof(uint32_t));
        *out_size = 0;
        return NULL;
    }

    uint8_t* decompressed = nimcp_malloc(orig_size);
    if (!decompressed) {
        LOG_ERROR("Failed to allocate decompression buffer");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, orig_size, "Failed to allocate decompression buffer");
        *out_size = 0;
        return NULL;
    }

    // Decompress data
    uLong decomp_len = (uLong)orig_size;
    int result = uncompress(decompressed, &decomp_len,
                            data + sizeof(uint32_t),
                            (uLong)(size - sizeof(uint32_t)));

    if (result != Z_OK) {
        nimcp_free(decompressed);
        *out_size = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "decompress_zlib: validation failed");
        return NULL;
    }

    *out_size = (size_t)decomp_len;
    return decompressed;
}

#endif /* ZLIB_AVAILABLE */

/**
 * @brief Fallback: return copy of data (no compression)
 */
static uint8_t* compress_fallback(const uint8_t* data, size_t size, size_t* out_size)
{
    uint8_t* copy = nimcp_malloc(size + sizeof(uint32_t));
    if (!copy) {
        LOG_ERROR("Failed to allocate fallback compression buffer");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, size + sizeof(uint32_t), "Failed to allocate fallback compression buffer");
        *out_size = 0;
        return NULL;
    }

    // Store original size at beginning (for consistency)
    memcpy(copy, &size, sizeof(uint32_t));
    memcpy(copy + sizeof(uint32_t), data, size);
    *out_size = size + sizeof(uint32_t);
    return copy;
}

/**
 * @brief Fallback: return copy of data (no decompression)
 */
static uint8_t* decompress_fallback(const uint8_t* data, size_t size, size_t* out_size)
{
    if (size < sizeof(uint32_t)) {
        *out_size = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "decompress_fallback: validation failed");
        return NULL;
    }

    uint32_t orig_size;
    memcpy(&orig_size, data, sizeof(uint32_t));

    /* P1 fix: Validate orig_size against available buffer to prevent heap over-read */
    if ((size_t)orig_size > size - sizeof(uint32_t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "decompress_fallback: orig_size %u exceeds available data %zu",
            orig_size, size - sizeof(uint32_t));
        *out_size = 0;
        return NULL;
    }

    uint8_t* copy = nimcp_malloc(orig_size);
    if (!copy) {
        LOG_ERROR("Failed to allocate fallback decompression buffer");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, orig_size, "Failed to allocate fallback decompression buffer");
        *out_size = 0;
        return NULL;
    }

    memcpy(copy, data + sizeof(uint32_t), orig_size);
    *out_size = (size_t)orig_size;
    return copy;
}

uint8_t* nimcp_compress(const uint8_t* data, size_t size, size_t* out_size)
{
    LOG_TRACE("Entering nimcp_compress");
    if (!data) {
        LOG_ERROR("nimcp_compress: NULL data");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL data in nimcp_compress");
        if (out_size) *out_size = 0;
        return NULL;
    }
    if (size == 0) {
        LOG_ERROR("nimcp_compress: zero size");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Zero size in nimcp_compress");
        if (out_size) *out_size = 0;
        return NULL;
    }
    if (!out_size) {
        LOG_ERROR("nimcp_compress: NULL out_size");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL out_size in nimcp_compress");
        return NULL;
    }

#if ZLIB_AVAILABLE
    return compress_zlib(data, size, out_size);
#else
    return compress_fallback(data, size, out_size);
#endif
}

uint8_t* nimcp_decompress(const uint8_t* data, size_t size, size_t* out_size)
{
    LOG_TRACE("Entering nimcp_decompress");
    if (!data) {
        LOG_ERROR("nimcp_decompress: NULL data");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL data in nimcp_decompress");
        if (out_size) *out_size = 0;
        return NULL;
    }
    if (size == 0) {
        LOG_ERROR("nimcp_decompress: zero size");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Zero size in nimcp_decompress");
        if (out_size) *out_size = 0;
        return NULL;
    }
    if (!out_size) {
        LOG_ERROR("nimcp_decompress: NULL out_size");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL out_size in nimcp_decompress");
        return NULL;
    }

#if ZLIB_AVAILABLE
    return decompress_zlib(data, size, out_size);
#else
    return decompress_fallback(data, size, out_size);
#endif
}

//=============================================================================
// Encryption Implementation (XOR Cipher Fallback)
//=============================================================================

/**
 * @brief XOR cipher encryption/decryption
 *
 * WHAT: Simple XOR-based cipher for basic encryption
 * WHY:  Fallback when AES is not available
 * HOW:  XOR each byte with key (repeating key pattern)
 *
 * NOTE: XOR cipher is NOT cryptographically secure.
 *       Use only for basic obfuscation, not sensitive data.
 */
static uint8_t* xor_cipher(const uint8_t* data, size_t size,
                           const uint8_t* key, size_t key_size,
                           size_t* out_size)
{
    if (!data || !key || key_size == 0 || size == 0) {
        if (out_size) *out_size = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "xor_cipher: validation failed");
        return NULL;
    }

    uint8_t* result = nimcp_malloc(size);
    if (!result) {
        LOG_ERROR("Failed to allocate XOR cipher buffer");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, size, "Failed to allocate XOR cipher buffer");
        *out_size = 0;
        return NULL;
    }

    for (size_t i = 0; i < size; i++) {
        result[i] = data[i] ^ key[i % key_size];
    }

    *out_size = size;
    return result;
}

uint8_t* nimcp_encrypt(const uint8_t* data, size_t size,
                       const uint8_t* key, size_t key_size,
                       size_t* out_size)
{
    if (!data || size == 0 || !key || key_size == 0 || !out_size) {
        if (out_size) *out_size = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_encrypt: validation failed");
        return NULL;
    }

    // Using XOR cipher as fallback (AES not yet implemented)
    return xor_cipher(data, size, key, key_size, out_size);
}

uint8_t* nimcp_decrypt(const uint8_t* data, size_t size,
                       const uint8_t* key, size_t key_size,
                       size_t* out_size)
{
    if (!data || size == 0 || !key || key_size == 0 || !out_size) {
        if (out_size) *out_size = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_decrypt: validation failed");
        return NULL;
    }

    // XOR cipher is symmetric - decrypt is same as encrypt
    return xor_cipher(data, size, key, key_size, out_size);
}

//=============================================================================
// Stream Processing Functions
//=============================================================================

uint8_t* nimcp_read_processed(FILE* file, uint32_t flags, size_t size,
                              const uint8_t* key, size_t key_size,
                              size_t* out_size)
{
    if (!file || size == 0 || !out_size) {
        if (out_size) *out_size = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_read_processed: validation failed");
        return NULL;
    }

    // Read raw data from file
    uint8_t* raw_data = nimcp_malloc(size);
    if (!raw_data) {
        *out_size = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_read_processed: raw_data is NULL");
        return NULL;
    }

    if (fread(raw_data, 1, size, file) != size) {
        nimcp_free(raw_data);
        *out_size = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_read_processed: validation failed");
        return NULL;
    }

    uint8_t* current = raw_data;
    size_t current_size = size;
    bool need_free = false;

    // Decrypt if encrypted
    if (flags & 0x00000002) { // NIMCP_FORMAT_FLAG_ENCRYPTED
        if (!key || key_size == 0) {
            nimcp_free(raw_data);
            *out_size = 0;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_read_processed: key is NULL");
            return NULL;
        }

        size_t decrypted_size;
        uint8_t* decrypted = nimcp_decrypt(current, current_size,
                                            key, key_size, &decrypted_size);
        if (need_free) nimcp_free(current);
        nimcp_free(raw_data);

        if (!decrypted) {
            *out_size = 0;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_read_processed: decrypted is NULL");
            return NULL;
        }

        current = decrypted;
        current_size = decrypted_size;
        need_free = true;
        raw_data = NULL;
    }

    // Decompress if compressed
    if (flags & 0x00000001) { // NIMCP_FORMAT_FLAG_COMPRESSED
        size_t decomp_size;
        uint8_t* decompressed = nimcp_decompress(current, current_size,
                                                  &decomp_size);
        if (need_free) nimcp_free(current);
        else if (raw_data) nimcp_free(raw_data);

        if (!decompressed) {
            *out_size = 0;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_read_processed: decompressed is NULL");
            return NULL;
        }

        current = decompressed;
        current_size = decomp_size;
    } else if (!need_free && raw_data) {
        // No processing done, return raw data
        *out_size = size;
        return raw_data;
    }

    *out_size = current_size;
    return current;
}

bool nimcp_write_processed(FILE* file, const uint8_t* data, size_t size,
                           uint32_t flags, const uint8_t* key, size_t key_size)
{
    if (!file || !data || size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_write_processed: required parameter is NULL (file, data)");
        return false;
    }

    const uint8_t* current = data;
    size_t current_size = size;
    uint8_t* temp1 = NULL;
    uint8_t* temp2 = NULL;

    // Compress if requested
    if (flags & 0x00000001) { // NIMCP_FORMAT_FLAG_COMPRESSED
        size_t comp_size;
        temp1 = nimcp_compress(current, current_size, &comp_size);
        if (!temp1) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_write_processed: temp1 is NULL");
            return false;
        }
        current = temp1;
        current_size = comp_size;
    }

    // Encrypt if requested
    if (flags & 0x00000002) { // NIMCP_FORMAT_FLAG_ENCRYPTED
        if (!key || key_size == 0) {
            if (temp1) nimcp_free(temp1);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_write_processed: validation failed");
            return false;
        }

        size_t enc_size;
        temp2 = nimcp_encrypt(current, current_size, key, key_size, &enc_size);
        if (!temp2) {
            if (temp1) nimcp_free(temp1);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_write_processed: validation failed");
            return false;
        }
        current = temp2;
        current_size = enc_size;
    }

    // Write to file
    bool success = (fwrite(current, 1, current_size, file) == current_size);

    // Cleanup
    if (temp1) nimcp_free(temp1);
    if (temp2) nimcp_free(temp2);

    return success;
}

//=============================================================================
// Serialization Context
//=============================================================================

void nimcp_serialize_ctx_init(nimcp_serialize_ctx_t* ctx)
{
    LOG_TRACE("Entering nimcp_serialize_ctx_init");
    if (!ctx) return;
    memset(ctx, 0, sizeof(nimcp_serialize_ctx_t));
}

void nimcp_serialize_ctx_set_key(nimcp_serialize_ctx_t* ctx,
                                  const uint8_t* key, size_t key_size)
{
    if (!ctx || !key || key_size == 0) return;

    size_t copy_size = key_size > 32 ? 32 : key_size;
    memcpy(ctx->key, key, copy_size);
    ctx->key_size = copy_size;
    ctx->key_set = true;
}

void nimcp_serialize_ctx_set_flags(nimcp_serialize_ctx_t* ctx, uint32_t flags)
{
    LOG_TRACE("Entering nimcp_serialize_ctx_set_flags");
    if (!ctx) return;
    ctx->flags = flags;
}
