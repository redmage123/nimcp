/**
 * @file nimcp_bbb_code_signing.c
 * @brief Blood-Brain Barrier Code Signing - Signature Verification Layer
 *
 * WHAT: Code signing and signature verification for the BBB perimeter defense
 * WHY:  Ensure only trusted, verified code can execute within the neural network
 *       by cryptographically signing and verifying all executable content
 * HOW:  HMAC-based signatures with a trusted key store, SHA256 hashing
 *
 * BIOLOGICAL MODEL:
 * ```
 * Basement membrane -> Code signing verification layer
 *   - Acts as a filter that verifies "credentials" of incoming molecules
 *   - Only allows molecules with proper "signatures" to pass
 * ```
 *
 * SECURITY MODEL:
 * - Uses HMAC-SHA256 for message authentication
 * - Static key for placeholder (production should use HSM/TPM)
 * - Trusted key store with max 32 keys
 * - SHA256 for content hashing
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe operations
 *
 * @author NIMCP Team
 * @date 2025-11-24
 */

#include "security/nimcp_blood_brain_barrier.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "security_bbb_code_signing"
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(bbb_code_signing, MESH_ADAPTER_CATEGORY_SECURITY)


#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>  /* For ssize_t */
#include <pthread.h>

//=============================================================================
// Constants
//=============================================================================

/** Maximum number of trusted keys in the store */
#define MAX_TRUSTED_KEYS 32

/** Maximum key ID length */
#define MAX_KEY_ID_LENGTH 64

/** Maximum key data size */
#define MAX_KEY_SIZE 256

/** HMAC block size for SHA256 */
#define HMAC_BLOCK_SIZE 64

/** SHA256 hash output size */
#define SHA256_HASH_SIZE 32

/** Signature size (HMAC-SHA256 output) */
#define SIGNATURE_SIZE 32

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * WHAT: Trusted key entry in the key store
 * WHY:  Store public keys for signature verification
 * HOW:  Array-based storage with key ID for lookup
 */
typedef struct {
    char key_id[MAX_KEY_ID_LENGTH];    /**< Unique key identifier */
    uint8_t key_data[MAX_KEY_SIZE];    /**< Key material */
    size_t key_size;                    /**< Size of key data */
    bool in_use;                        /**< Whether slot is occupied */
} trusted_key_entry_t;

/**
 * WHAT: Global trusted key store
 * WHY:  Maintain list of trusted signing keys
 * HOW:  Fixed-size array with mutex protection
 */
static struct {
    trusted_key_entry_t keys[MAX_TRUSTED_KEYS];
    uint32_t key_count;
    nimcp_mutex_t mutex;
    bool initialized;
} g_key_store = {
    .key_count = 0,
    .mutex = NIMCP_MUTEX_INITIALIZER,
    .initialized = false
};

/**
 * WHAT: Global HMAC key pointer (must be set before use)
 * WHY:  Force runtime key configuration, prevent hardcoded keys
 * HOW:  Pointer initialized to NULL, must call bbb_set_signing_key()
 *
 * SECURITY: Key must be loaded from secure source (HSM/TPM/config)
 */
static const uint8_t* g_signing_key = NULL;
static size_t g_signing_key_len = 0;

#ifdef NIMCP_BBB_PLACEHOLDER_KEY_WARNING
#warning "BBB code signing key is not configured - calls will fail at runtime"
#endif

//=============================================================================
// SHA256 Implementation (Simplified)
//=============================================================================

/**
 * WHAT: SHA256 round constants
 * WHY:  First 32 bits of fractional parts of cube roots of first 64 primes
 * HOW:  Used in SHA256 compression function
 */
static const uint32_t k256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

/** Right rotate macro */
#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

/** SHA256 functions */
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define EP1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SIG0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ ((x) >> 3))
#define SIG1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))

/**
 * @brief SHA256 context structure
 */
typedef struct {
    uint32_t state[8];     /**< Hash state */
    uint64_t bit_count;    /**< Total bits processed */
    uint8_t buffer[64];    /**< Input buffer */
    uint32_t buffer_len;   /**< Current buffer length */
} sha256_ctx_t;

/**
 * @brief Initialize SHA256 context
 *
 * WHAT: Set initial hash values for SHA256
 * WHY:  Required before hashing data
 * HOW:  Use FIPS-specified initial values
 */
static void sha256_init(sha256_ctx_t* ctx)
{
    if (!ctx) return;

    /* Initial hash values (first 32 bits of fractional parts of square roots) */
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;

    ctx->bit_count = 0;
    ctx->buffer_len = 0;
}

/**
 * @brief Process a 64-byte block
 *
 * WHAT: SHA256 compression function
 * WHY:  Core algorithm that processes each 512-bit block
 * HOW:  64 rounds of mixing using constants and message schedule
 */
static void sha256_transform(sha256_ctx_t* ctx, const uint8_t* block)
{
    if (!ctx || !block) return;

    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h;
    uint32_t t1, t2;

    /* Prepare message schedule */
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i * 4] << 24) |
               ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8) |
               ((uint32_t)block[i * 4 + 3]);
    }

    for (int i = 16; i < 64; i++) {
        w[i] = SIG1(w[i - 2]) + w[i - 7] + SIG0(w[i - 15]) + w[i - 16];
    }

    /* Initialize working variables */
    a = ctx->state[0]; b = ctx->state[1];
    c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5];
    g = ctx->state[6]; h = ctx->state[7];

    /* 64 rounds */
    for (int i = 0; i < 64; i++) {
        t1 = h + EP1(e) + CH(e, f, g) + k256[i] + w[i];
        t2 = EP0(a) + MAJ(a, b, c);
        h = g; g = f; f = e;
        e = d + t1;
        d = c; c = b; b = a;
        a = t1 + t2;
    }

    /* Add to state */
    ctx->state[0] += a; ctx->state[1] += b;
    ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f;
    ctx->state[6] += g; ctx->state[7] += h;
}

/**
 * @brief Update SHA256 with data
 *
 * WHAT: Add data to SHA256 hash computation
 * WHY:  Allows incremental hashing of large data
 * HOW:  Buffer data and process complete blocks
 */
static void sha256_update(sha256_ctx_t* ctx, const uint8_t* data, size_t len)
{
    if (!ctx || !data || len == 0) return;

    ctx->bit_count += len * 8;

    while (len > 0) {
        size_t space = 64 - ctx->buffer_len;
        size_t copy = (len < space) ? len : space;

        memcpy(ctx->buffer + ctx->buffer_len, data, copy);
        ctx->buffer_len += copy;
        data += copy;
        len -= copy;

        if (ctx->buffer_len == 64) {
            sha256_transform(ctx, ctx->buffer);
            ctx->buffer_len = 0;
        }
    }
}

/**
 * @brief Finalize SHA256 and get hash
 *
 * WHAT: Complete SHA256 computation
 * WHY:  Apply padding and extract final hash
 * HOW:  Pad message, append length, process final blocks
 */
static void sha256_final(sha256_ctx_t* ctx, uint8_t* hash)
{
    if (!ctx || !hash) return;

    /* Pad message */
    size_t pad_len = (ctx->buffer_len < 56) ? (56 - ctx->buffer_len) : (120 - ctx->buffer_len);
    uint8_t padding[128] = {0x80};  /* 1 bit followed by zeros */

    sha256_update(ctx, padding, pad_len);

    /* Append bit length (big-endian) */
    uint8_t len_bytes[8];
    for (int i = 0; i < 8; i++) {
        len_bytes[7 - i] = (uint8_t)(ctx->bit_count >> (i * 8));
    }
    sha256_update(ctx, len_bytes, 8);

    /* Extract hash (big-endian) */
    for (int i = 0; i < 8; i++) {
        hash[i * 4] = (uint8_t)(ctx->state[i] >> 24);
        hash[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        hash[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        hash[i * 4 + 3] = (uint8_t)(ctx->state[i]);
    }
}

//=============================================================================
// HMAC Implementation
//=============================================================================

/**
 * @brief Compute HMAC-SHA256
 *
 * WHAT: Keyed-Hash Message Authentication Code using SHA256
 * WHY:  Provides authentication and integrity of signed data
 * HOW:  HMAC = H((K XOR opad) || H((K XOR ipad) || message))
 *
 * @param key HMAC key
 * @param key_len Key length
 * @param data Data to authenticate
 * @param data_len Data length
 * @param mac Output MAC (32 bytes)
 */
static void hmac_sha256(const uint8_t* key, size_t key_len,
                        const uint8_t* data, size_t data_len,
                        uint8_t* mac)
{
    if (!key || !data || !mac) return;

    uint8_t k_ipad[HMAC_BLOCK_SIZE];
    uint8_t k_opad[HMAC_BLOCK_SIZE];
    uint8_t key_hash[SHA256_HASH_SIZE];
    const uint8_t* key_to_use = key;
    size_t key_to_use_len = key_len;

    /* If key is longer than block size, hash it */
    if (key_len > HMAC_BLOCK_SIZE) {
        sha256_ctx_t ctx;
        sha256_init(&ctx);
        sha256_update(&ctx, key, key_len);
        sha256_final(&ctx, key_hash);
        key_to_use = key_hash;
        key_to_use_len = SHA256_HASH_SIZE;
    }

    /* Prepare ipad and opad */
    memset(k_ipad, 0x36, HMAC_BLOCK_SIZE);
    memset(k_opad, 0x5c, HMAC_BLOCK_SIZE);

    for (size_t i = 0; i < key_to_use_len; i++) {
        k_ipad[i] ^= key_to_use[i];
        k_opad[i] ^= key_to_use[i];
    }

    /* Inner hash: H((K XOR ipad) || message) */
    sha256_ctx_t ctx;
    uint8_t inner_hash[SHA256_HASH_SIZE];
    sha256_init(&ctx);
    sha256_update(&ctx, k_ipad, HMAC_BLOCK_SIZE);
    sha256_update(&ctx, data, data_len);
    sha256_final(&ctx, inner_hash);

    /* Outer hash: H((K XOR opad) || inner_hash) */
    sha256_init(&ctx);
    sha256_update(&ctx, k_opad, HMAC_BLOCK_SIZE);
    sha256_update(&ctx, inner_hash, SHA256_HASH_SIZE);
    sha256_final(&ctx, mac);
}

//=============================================================================
// Key Store Management
//=============================================================================

/**
 * @brief Initialize key store (internal)
 *
 * WHAT: Initialize the trusted key store
 * WHY:  Ensure key store is ready before use
 * HOW:  Clear all slots, set initialized flag
 */
static void init_key_store(void)
{
    if (g_key_store.initialized) return;

    nimcp_mutex_lock(&g_key_store.mutex);

    if (!g_key_store.initialized) {
        memset(g_key_store.keys, 0, sizeof(g_key_store.keys));
        g_key_store.key_count = 0;
        g_key_store.initialized = true;
    }

    nimcp_mutex_unlock(&g_key_store.mutex);
}

/**
 * @brief Find key by ID (internal)
 *
 * WHAT: Look up a key in the store by its ID
 * WHY:  Support key retrieval for signature operations
 * HOW:  Linear search through key slots
 *
 * @param key_id Key identifier
 * @return Pointer to key entry or NULL
 */
static trusted_key_entry_t* find_key(const char* key_id)
{
    if (!key_id) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "key_id is NULL");

        return NULL;

    }

    for (uint32_t i = 0; i < MAX_TRUSTED_KEYS; i++) {
        if (g_key_store.keys[i].in_use &&
            strncmp(g_key_store.keys[i].key_id, key_id, MAX_KEY_ID_LENGTH) == 0) {
            return &g_key_store.keys[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_key: operation failed");
    return NULL;
}

//=============================================================================
// Public API Implementation
//=============================================================================

/**
 * @brief Sign code/data with HMAC-SHA256
 *
 * WHAT: Generate cryptographic signature for data
 * WHY:  Allow verification that data has not been tampered with
 * HOW:  HMAC-SHA256 using placeholder key (production: use HSM)
 *
 * @param system BBB system handle
 * @param data Data to sign
 * @param size Size of data
 * @param signature Output signature buffer
 * @param sig_size Size of signature buffer
 * @return Signature length (32), or -1 on error
 */
ssize_t bbb_sign_code(bbb_system_t system, const void* data,
                      size_t size, uint8_t* signature, size_t sig_size)
{
    /* Guard: Invalid parameters */
    if (!data || size == 0 || !signature) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bbb_sign_code: required parameter is NULL (data, signature)");
        return -1;
    }

    /* Guard: Signature buffer too small */
    if (sig_size < SIGNATURE_SIZE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "bbb_sign_code: signature buffer too small");
        return -1;
    }

    /* Guard: Signing key not configured */
    if (!g_signing_key || g_signing_key_len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
            "bbb_sign_code: signing key not configured");
        return -1;
    }

    (void)system;  /* Available for future configuration */

    /* Compute HMAC-SHA256 signature */
    hmac_sha256(g_signing_key, g_signing_key_len,
                (const uint8_t*)data, size, signature);

    return SIGNATURE_SIZE;
}

/**
 * @brief Verify code/data signature
 *
 * WHAT: Verify HMAC-SHA256 signature is valid
 * WHY:  Ensure data has not been modified since signing
 * HOW:  Recompute HMAC and compare with provided signature
 *
 * @param system BBB system handle
 * @param data Data that was signed
 * @param size Size of data
 * @param signature Signature to verify
 * @param sig_size Size of signature
 * @return true if signature is valid
 */
bool bbb_verify_signature(bbb_system_t system, const void* data,
                          size_t size, const uint8_t* signature, size_t sig_size)
{
    /* Guard: Invalid parameters */
    if (!data || size == 0 || !signature) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bbb_verify_signature: required parameter is NULL (data, signature)");
        return false;
    }

    /* Guard: Incorrect signature size */
    if (sig_size != SIGNATURE_SIZE) {
        return false;
    }

    /* Guard: Signing key not configured */
    if (!g_signing_key || g_signing_key_len == 0) {
        LOG_ERROR("BBB signing key not configured - call bbb_set_signing_key() first");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bbb_verify_signature: g_signing_key is NULL");
        return false;
    }

    (void)system;  /* Available for future configuration */

    /* Compute expected signature */
    uint8_t expected[SIGNATURE_SIZE];
    hmac_sha256(g_signing_key, g_signing_key_len,
                (const uint8_t*)data, size, expected);

    /* Constant-time comparison to prevent timing attacks */
    int diff = 0;
    for (size_t i = 0; i < SIGNATURE_SIZE; i++) {
        diff |= (expected[i] ^ signature[i]);
    }

    return (diff == 0);
}

/**
 * @brief Add trusted public key to store
 *
 * WHAT: Register a key for signature verification
 * WHY:  Build trust chain by adding verified keys
 * HOW:  Store key data with identifier in key store
 *
 * @param system BBB system handle
 * @param key_data Public key data
 * @param key_size Size of key data
 * @param key_id Unique identifier for key
 * @return true on success
 */
bool bbb_add_trusted_key(bbb_system_t system, const uint8_t* key_data,
                         size_t key_size, const char* key_id)
{
    /* Guard: Invalid parameters */
    if (!key_data || key_size == 0 || !key_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bbb_add_trusted_key: required parameter is NULL (key_data, key_id)");
        return false;
    }

    /* Guard: Key too large */
    if (key_size > MAX_KEY_SIZE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bbb_add_trusted_key: validation failed");
        return false;
    }

    /* Guard: Key ID too long */
    if (strlen(key_id) >= MAX_KEY_ID_LENGTH) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "bbb_add_trusted_key: capacity exceeded");
        return false;
    }

    (void)system;  /* Available for future configuration */

    /* Initialize key store if needed */
    init_key_store();

    nimcp_mutex_lock(&g_key_store.mutex);

    /* Check if key already exists */
    if (find_key(key_id) != NULL) {
        nimcp_mutex_unlock(&g_key_store.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bbb_add_trusted_key: validation failed");
        return false;  /* Duplicate key ID */
    }

    /* Find empty slot */
    trusted_key_entry_t* slot = NULL;
    for (uint32_t i = 0; i < MAX_TRUSTED_KEYS; i++) {
        if (!g_key_store.keys[i].in_use) {
            slot = &g_key_store.keys[i];
            break;
        }
    }

    if (!slot) {
        nimcp_mutex_unlock(&g_key_store.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bbb_add_trusted_key: slot is NULL");
        return false;  /* Key store full */
    }

    /* Store key */
    strncpy(slot->key_id, key_id, MAX_KEY_ID_LENGTH - 1);
    slot->key_id[MAX_KEY_ID_LENGTH - 1] = '\0';
    memcpy(slot->key_data, key_data, key_size);
    slot->key_size = key_size;
    slot->in_use = true;
    g_key_store.key_count++;

    nimcp_mutex_unlock(&g_key_store.mutex);

    return true;
}

/**
 * @brief Remove trusted key from store
 *
 * WHAT: Remove a key from the trusted key store
 * WHY:  Revoke trust when key is compromised or expired
 * HOW:  Find key by ID and clear its slot
 *
 * @param system BBB system handle
 * @param key_id Key identifier to remove
 * @return true on success
 */
bool bbb_remove_trusted_key(bbb_system_t system, const char* key_id)
{
    /* Guard: Invalid parameters */
    if (!key_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bbb_remove_trusted_key: key_id is NULL");
        return false;
    }

    (void)system;  /* Available for future configuration */

    /* Initialize key store if needed */
    init_key_store();

    nimcp_mutex_lock(&g_key_store.mutex);

    /* Find key */
    trusted_key_entry_t* key = find_key(key_id);
    if (!key) {
        nimcp_mutex_unlock(&g_key_store.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bbb_remove_trusted_key: key is NULL");
        return false;  /* Key not found */
    }

    /* Clear key data securely */
    memset(key->key_data, 0, sizeof(key->key_data));
    memset(key->key_id, 0, sizeof(key->key_id));
    key->key_size = 0;
    key->in_use = false;
    g_key_store.key_count--;

    nimcp_mutex_unlock(&g_key_store.mutex);

    return true;
}

/**
 * @brief Set HMAC signing key (must be called before signing/verification)
 *
 * WHAT: Configure the HMAC key used for code signing
 * WHY:  Prevent hardcoded keys, require runtime configuration
 * HOW:  Stores pointer and length, validates input
 *
 * @param key_data Key material (must remain valid for lifetime of use)
 * @param key_size Size of key in bytes
 * @return true on success
 */
bool bbb_set_signing_key(const uint8_t* key_data, size_t key_size)
{
    /* Guard: Invalid parameters */
    if (!key_data || key_size == 0) {
        LOG_ERROR("Invalid key parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bbb_set_signing_key: key_data is NULL");
        return false;
    }

    /* Guard: Key too short (minimum 16 bytes for security) */
    if (key_size < 16) {
        LOG_ERROR("Key too short (minimum 16 bytes required)");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bbb_set_signing_key: validation failed");
        return false;
    }

    /* Guard: Key too long */
    if (key_size > MAX_KEY_SIZE) {
        LOG_ERROR("Key too long (maximum %u bytes)", MAX_KEY_SIZE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bbb_set_signing_key: validation failed");
        return false;
    }

    /* Store key pointer (caller must ensure key remains valid) */
    g_signing_key = key_data;
    g_signing_key_len = key_size;

    LOG_INFO("BBB signing key configured (%zu bytes)", key_size);
    return true;
}

/**
 * @brief Clear HMAC signing key
 *
 * WHAT: Remove configured signing key
 * WHY:  Security cleanup or key rotation
 * HOW:  Clears global key pointer
 */
void bbb_clear_signing_key(void)
{
    g_signing_key = NULL;
    g_signing_key_len = 0;
    LOG_INFO("BBB signing key cleared");
}

/**
 * @brief Calculate SHA256 hash of data
 *
 * WHAT: Compute cryptographic hash of data
 * WHY:  Generate unique fingerprint for integrity verification
 * HOW:  Full SHA256 implementation
 *
 * @param data Data to hash
 * @param size Size of data
 * @param hash Output hash buffer (32 bytes)
 * @return true on success
 */
bool bbb_calculate_hash(const void* data, size_t size, uint8_t* hash)
{
    /* Guard: Invalid parameters */
    if (!data || size == 0 || !hash) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bbb_calculate_hash: required parameter is NULL (data, hash)");
        return false;
    }

    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, (const uint8_t*)data, size);
    sha256_final(&ctx, hash);

    return true;
}

//=============================================================================
// Test Reset Support
//=============================================================================

/**
 * @brief Reset code signing state for test isolation
 *
 * WHAT: Clear signing key and key store state between tests
 * WHY:  Prevent signing key state from leaking between tests
 * HOW:  Clear the global signing key and reset the key store
 *
 * NOTE: Called by bbb_reset_test_state() for unified BBB reset
 */
void bbb_code_signing_reset_internal(void)
{
    /* Clear signing key */
    g_signing_key = NULL;
    g_signing_key_len = 0;

    /* Reset key store */
    memset(g_key_store.keys, 0, sizeof(g_key_store.keys));
    g_key_store.key_count = 0;
    /* Leave mutex and initialized state intact */
}
