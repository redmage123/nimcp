/**
 * @file nimcp_key_derivation.c
 * @brief Implementation of key derivation functions
 *
 * WHAT: Argon2id and PBKDF2-HMAC-SHA256 key derivation implementations
 * WHY:  Secure password-based key generation resistant to brute-force
 * HOW:  Memory-hard algorithms with configurable time/memory/parallelism costs
 *
 * IMPLEMENTATION NOTES:
 * - Argon2id uses libsodium's crypto_pwhash when available
 * - PBKDF2 uses platform HMAC-SHA256 or OpenSSL
 * - All sensitive data securely wiped after use
 * - Bio-async integration for monitoring and async operations
 * - Comprehensive logging (without leaking secrets)
 *
 * SECURITY CONSIDERATIONS:
 * - Passwords never logged
 * - Keys never logged
 * - Salts are logged (they're not secret)
 * - Timing information logged for performance monitoring
 * - All intermediate buffers securely wiped
 *
 * @author NIMCP Security Team
 * @date 2025-12-07
 */

#include "security/nimcp_key_derivation.h"
#include "security/nimcp_constant_time.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define LOG_MODULE "key_derivation"
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(key_derivation, MESH_ADAPTER_CATEGORY_SECURITY)


// Platform-specific includes
#ifdef NIMCP_ENABLE_LIBSODIUM
#include <sodium.h>
#endif

#ifdef NIMCP_ENABLE_OPENSSL
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#endif

// Fallback: implement HMAC-SHA256 ourselves
#ifndef NIMCP_ENABLE_OPENSSL
#include "security/nimcp_security.h"  // For hash functions
#endif

// Platform-specific secure random
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#else
#include <fcntl.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/random.h>
#endif
#endif

//=============================================================================
// Internal Context Structure
//=============================================================================

struct nimcp_kdf_context {
    uint32_t magic;                      /**< Magic number validation */
    nimcp_kdf_config_t config;           /**< Current configuration */
    nimcp_kdf_stats_t stats;             /**< Statistics */
    bool bio_async_registered;          /**< Bio-async registration */
    bio_module_context_t bio_ctx;        /**< Bio-async context */
    void* algorithm_state;               /**< Algorithm-specific state */
};

//=============================================================================
// Internal Helper: SHA-256 (if not using OpenSSL)
//=============================================================================

#ifndef NIMCP_ENABLE_OPENSSL

// Simple SHA-256 implementation for PBKDF2
// Based on FIPS 180-4 specification

#define SHA256_BLOCK_SIZE 64
#define SHA256_DIGEST_SIZE 32

typedef struct {
    uint32_t state[8];
    uint64_t count;
    uint8_t buffer[SHA256_BLOCK_SIZE];
} sha256_ctx_t;

static const uint32_t s_sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define ROTRIGHT(a,b) (((a) >> (b)) | ((a) << (32-(b))))
#define CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTRIGHT(x,2) ^ ROTRIGHT(x,13) ^ ROTRIGHT(x,22))
#define EP1(x) (ROTRIGHT(x,6) ^ ROTRIGHT(x,11) ^ ROTRIGHT(x,25))
#define SIG0(x) (ROTRIGHT(x,7) ^ ROTRIGHT(x,18) ^ ((x) >> 3))
#define SIG1(x) (ROTRIGHT(x,17) ^ ROTRIGHT(x,19) ^ ((x) >> 10))

static void s_sha256_transform(sha256_ctx_t* ctx, const uint8_t data[])
{
    uint32_t a, b, c, d, e, f, g, h, t1, t2, m[64];
    uint32_t i;

    // Prepare message schedule
    for (i = 0; i < 16; i++) {
        m[i] = ((uint32_t)data[i * 4] << 24) |
               ((uint32_t)data[i * 4 + 1] << 16) |
               ((uint32_t)data[i * 4 + 2] << 8) |
               ((uint32_t)data[i * 4 + 3]);
    }
    for (; i < 64; i++) {
        m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];
    }

    // Initialize working variables
    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    // Main loop
    for (i = 0; i < 64; i++) {
        t1 = h + EP1(e) + CH(e, f, g) + s_sha256_k[i] + m[i];
        t2 = EP0(a) + MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    // Update state
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void s_sha256_init(sha256_ctx_t* ctx)
{
    ctx->count = 0;
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
}

static void s_sha256_update(sha256_ctx_t* ctx, const uint8_t* data, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++) {
        ctx->buffer[ctx->count % SHA256_BLOCK_SIZE] = data[i];
        ctx->count++;
        if (ctx->count % SHA256_BLOCK_SIZE == 0) {
            s_sha256_transform(ctx, ctx->buffer);
        }
    }
}

static void s_sha256_final(sha256_ctx_t* ctx, uint8_t hash[SHA256_DIGEST_SIZE])
{
    uint32_t i = (uint32_t)(ctx->count % SHA256_BLOCK_SIZE);

    // Padding
    ctx->buffer[i++] = 0x80;
    if (i > 56) {
        while (i < 64) {
            ctx->buffer[i++] = 0x00;
        }
        s_sha256_transform(ctx, ctx->buffer);
        i = 0;
    }

    while (i < 56) {
        ctx->buffer[i++] = 0x00;
    }

    // Append length
    uint64_t bit_len = ctx->count * 8;
    ctx->buffer[63] = (uint8_t)bit_len;
    ctx->buffer[62] = (uint8_t)(bit_len >> 8);
    ctx->buffer[61] = (uint8_t)(bit_len >> 16);
    ctx->buffer[60] = (uint8_t)(bit_len >> 24);
    ctx->buffer[59] = (uint8_t)(bit_len >> 32);
    ctx->buffer[58] = (uint8_t)(bit_len >> 40);
    ctx->buffer[57] = (uint8_t)(bit_len >> 48);
    ctx->buffer[56] = (uint8_t)(bit_len >> 56);

    s_sha256_transform(ctx, ctx->buffer);

    // Output hash
    for (i = 0; i < 8; i++) {
        hash[i * 4] = (uint8_t)(ctx->state[i] >> 24);
        hash[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        hash[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        hash[i * 4 + 3] = (uint8_t)ctx->state[i];
    }

    // Secure wipe
    nimcp_secure_zero(ctx, sizeof(sha256_ctx_t));
}

//=============================================================================
// Internal Helper: HMAC-SHA256 (if not using OpenSSL)
//=============================================================================

static void s_hmac_sha256(const uint8_t* key, size_t key_len,
                          const uint8_t* data, size_t data_len,
                          uint8_t output[32])
{
    sha256_ctx_t ctx;
    uint8_t k_ipad[SHA256_BLOCK_SIZE];
    uint8_t k_opad[SHA256_BLOCK_SIZE];
    uint8_t key_buf[SHA256_BLOCK_SIZE];
    uint8_t inner_hash[SHA256_DIGEST_SIZE];

    // If key is longer than block size, hash it first
    if (key_len > SHA256_BLOCK_SIZE) {
        s_sha256_init(&ctx);
        s_sha256_update(&ctx, key, key_len);
        s_sha256_final(&ctx, key_buf);
        key_len = SHA256_DIGEST_SIZE;
    } else {
        memcpy(key_buf, key, key_len);
    }

    // Pad key to block size
    memset(key_buf + key_len, 0, SHA256_BLOCK_SIZE - key_len);

    // Create ipad and opad
    for (size_t i = 0; i < SHA256_BLOCK_SIZE; i++) {
        k_ipad[i] = key_buf[i] ^ 0x36;
        k_opad[i] = key_buf[i] ^ 0x5c;
    }

    // Inner hash: H(K XOR ipad || message)
    s_sha256_init(&ctx);
    s_sha256_update(&ctx, k_ipad, SHA256_BLOCK_SIZE);
    s_sha256_update(&ctx, data, data_len);
    s_sha256_final(&ctx, inner_hash);

    // Outer hash: H(K XOR opad || inner_hash)
    s_sha256_init(&ctx);
    s_sha256_update(&ctx, k_opad, SHA256_BLOCK_SIZE);
    s_sha256_update(&ctx, inner_hash, SHA256_DIGEST_SIZE);
    s_sha256_final(&ctx, output);

    // Secure wipe
    nimcp_secure_zero(k_ipad, SHA256_BLOCK_SIZE);
    nimcp_secure_zero(k_opad, SHA256_BLOCK_SIZE);
    nimcp_secure_zero(key_buf, SHA256_BLOCK_SIZE);
    nimcp_secure_zero(inner_hash, SHA256_DIGEST_SIZE);
}

#endif  // !NIMCP_ENABLE_OPENSSL

//=============================================================================
// Internal Helper: PBKDF2-HMAC-SHA256 Implementation
//=============================================================================

/**
 * WHAT: PBKDF2 implementation following RFC 8018
 * WHY:  Fallback when Argon2 unavailable or for compatibility
 * HOW:  Iterative HMAC-SHA256 with salting
 */
static nimcp_result_t s_pbkdf2_sha256(
    const char* password, size_t password_len,
    const uint8_t* salt, size_t salt_len,
    uint32_t iterations,
    uint8_t* output, size_t output_len)
{
    NIMCP_CHECK_THROW(password && salt && output, NIMCP_ERROR_NULL_POINTER, "password, salt, or output is NULL");

    if (iterations < NIMCP_KDF_PBKDF2_MIN_ITERATIONS) {
        LOG_WARN("PBKDF2 iterations too low: %u < %u",
                 iterations, NIMCP_KDF_PBKDF2_MIN_ITERATIONS);
    }

    // PBKDF2 formula: DK = T1 || T2 || ... || Tdklen/hlen
    // where Ti = F(Password, Salt, iterations, i)
    // and F(P, S, c, i) = U1 XOR U2 XOR ... XOR Uc
    // where U1 = PRF(P, S || INT_32_BE(i))
    //       U2 = PRF(P, U1)
    //       ...
    //       Uc = PRF(P, Uc-1)

    const size_t hash_len = 32;  // SHA-256 output size
    const size_t num_blocks = (output_len + hash_len - 1) / hash_len;

    uint8_t block[32];
    uint8_t u_current[32];
    uint8_t u_result[32];

    for (size_t block_idx = 1; block_idx <= num_blocks; block_idx++) {
        // Prepare salt || block_index
        size_t salt_block_len = salt_len + 4;
        uint8_t* salt_block = (uint8_t*)nimcp_malloc(salt_block_len);
        NIMCP_CHECK_THROW(salt_block, NIMCP_ERROR_MEMORY, "failed to allocate salt block");

        memcpy(salt_block, salt, salt_len);
        salt_block[salt_len] = (uint8_t)(block_idx >> 24);
        salt_block[salt_len + 1] = (uint8_t)(block_idx >> 16);
        salt_block[salt_len + 2] = (uint8_t)(block_idx >> 8);
        salt_block[salt_len + 3] = (uint8_t)block_idx;

        // U1 = PRF(password, salt || block_index)
#ifdef NIMCP_ENABLE_OPENSSL
        HMAC(EVP_sha256(), password, (int)password_len,
             salt_block, salt_block_len, u_current, NULL);
#else
        s_hmac_sha256((const uint8_t*)password, password_len,
                     salt_block, salt_block_len, u_current);
#endif

        memcpy(u_result, u_current, hash_len);

        // U2 through Uc
        for (uint32_t iter = 1; iter < iterations; iter++) {
#ifdef NIMCP_ENABLE_OPENSSL
            HMAC(EVP_sha256(), password, (int)password_len,
                 u_current, hash_len, u_current, NULL);
#else
            s_hmac_sha256((const uint8_t*)password, password_len,
                         u_current, hash_len, u_current);
#endif

            // XOR into result
            for (size_t i = 0; i < hash_len; i++) {
                u_result[i] ^= u_current[i];
            }
        }

        // Copy to output
        size_t copy_len = hash_len;
        if (block_idx == num_blocks) {
            // Last block may be partial
            copy_len = output_len - (block_idx - 1) * hash_len;
        }

        memcpy(output + (block_idx - 1) * hash_len, u_result, copy_len);

        // Secure wipe
        nimcp_secure_zero(salt_block, salt_block_len);
        nimcp_free(salt_block);
    }

    // Secure wipe intermediate buffers
    nimcp_secure_zero(block, sizeof(block));
    nimcp_secure_zero(u_current, sizeof(u_current));
    nimcp_secure_zero(u_result, sizeof(u_result));

    return NIMCP_SUCCESS;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Wiring callback for KG-driven handler registration
 *
 * Called by the orchestrator with discovered message types from the knowledge graph.
 * Registers handlers based on message types discovered at runtime.
 *
 * @param ctx Bio-async module context
 * @param message_types Array of discovered message types
 * @param message_count Number of message types
 * @param user_data User-provided context
 * @return 0 on success, -1 on error
 */
static int key_derivation_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
);

static nimcp_error_t s_kdf_message_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    nimcp_kdf_context_t ctx = (nimcp_kdf_context_t)user_data;

    if (!ctx || ctx->magic != NIMCP_KDF_MAGIC) {
        LOG_ERROR("Invalid KDF context in message handler");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (msg_size < sizeof(bio_message_header_t)) {
        return NIMCP_ERROR_BUFFER_TOO_SMALL;
    }

    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    switch (header->type) {
        case BIO_MSG_HEALTH_CHECK: {
            nimcp_kdf_stats_t stats;
            nimcp_kdf_get_stats(ctx, &stats);

            if (response_promise) {
                nimcp_bio_promise_complete(response_promise, &stats);
            }

            LOG_DEBUG("KDF stats request: %lu derivations", stats.derivations_performed);
            return NIMCP_SUCCESS;
        }

        default:
            LOG_WARN("Unknown KDF message type: %u", header->type);
            return NIMCP_ERROR_NOT_IMPLEMENTED;
    }
}

static nimcp_error_t s_register_bio_async(nimcp_kdf_context_t ctx)
{
    if (ctx->bio_async_registered) {
        return NIMCP_SUCCESS;
    }

    bio_router_t router = bio_router_get_global();
    if (!router) {
        LOG_WARN("Bio-async router not available");
        return NIMCP_SUCCESS;  // Not fatal
    }

    bio_module_info_t module_info = {
        .module_id = BIO_MODULE_SECURITY,
        .module_name = "security_key_derivation",
        .inbox_capacity = 64,
        .user_data = ctx
    };

    ctx->bio_ctx = bio_router_register_module(&module_info);
    if (!ctx->bio_ctx) {
        LOG_ERROR("Failed to register KDF with bio-async");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Try KG-driven wiring callback registration first
    nimcp_error_t result = bio_router_register_wiring_callback(
        BIO_MODULE_SECURITY,
        (void*)key_derivation_wiring_handler_callback,
        ctx
    );

    if (result == NIMCP_SUCCESS) {
        LOG_INFO("KG-driven wiring callback registered successfully");
    } else {
        // Fallback to legacy handler registration
        LOG_INFO("Falling back to legacy handler registration");

        LEGACY_HANDLER_REGISTRATION(
            result = bio_router_register_handler(ctx->bio_ctx,
                                                  BIO_MSG_HEALTH_CHECK,
                                                  s_kdf_message_handler)
        );
        if (result != NIMCP_SUCCESS) {
            LOG_ERROR("Failed to register KDF handler: %d", result);
            return NIMCP_ERROR_NOT_SUPPORTED;
        }
    }

    ctx->bio_async_registered = true;
    LOG_INFO("KDF module registered with bio-async");

    return NIMCP_SUCCESS;
}

/**
 * @brief Wiring callback implementation for KG-driven handler registration
 */
static int key_derivation_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    (void)user_data;

    int registered = 0;
    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            case BIO_MSG_HEALTH_CHECK:
                bio_router_register_handler(ctx, message_types[i], s_kdf_message_handler);
                registered++;
                break;
            default:
                LOG_DEBUG("Unknown message type %d in wiring callback", message_types[i]);
                break;
        }
    }

    LOG_INFO("KG-driven wiring callback registered %d handlers", registered);
    return (registered > 0) ? 0 : -1;
}

//=============================================================================
// Configuration and Context Management
//=============================================================================

nimcp_kdf_config_t nimcp_kdf_default_config(void)
{
    nimcp_kdf_config_t config = {
        .algorithm = NIMCP_KDF_ARGON2ID,
        .memory_kb = NIMCP_KDF_DEFAULT_MEMORY_KB,
        .iterations = NIMCP_KDF_DEFAULT_ITERATIONS,
        .parallelism = NIMCP_KDF_DEFAULT_PARALLELISM,
        .enable_logging = true,
        .enable_statistics = true
    };

    return config;
}

nimcp_kdf_context_t nimcp_kdf_create(const nimcp_kdf_config_t* config)
{
    // Use defaults if config is NULL
    nimcp_kdf_config_t default_config = nimcp_kdf_default_config();
    const nimcp_kdf_config_t* cfg = config ? config : &default_config;

    // Validate configuration
    if (cfg->algorithm >= NIMCP_KDF_ALGORITHM_COUNT) {
        LOG_ERROR("Invalid KDF algorithm: %d", cfg->algorithm);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_kdf_create: capacity exceeded");
        return NULL;
    }

    if (cfg->algorithm == NIMCP_KDF_ARGON2ID) {
        if (cfg->memory_kb < 8192) {
            LOG_ERROR("Argon2 memory too low: %u KB (minimum 8192 KB)", cfg->memory_kb);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_kdf_create: validation failed");
            return NULL;
        }
        if (cfg->iterations < 1) {
            LOG_ERROR("Argon2 iterations must be >= 1");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_kdf_create: validation failed");
            return NULL;
        }
        if (cfg->parallelism < 1 || cfg->parallelism > 64) {
            LOG_ERROR("Argon2 parallelism must be 1-64");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_kdf_create: validation failed");
            return NULL;
        }
    } else if (cfg->algorithm == NIMCP_KDF_PBKDF2_SHA256) {
        if (cfg->iterations < NIMCP_KDF_PBKDF2_MIN_ITERATIONS) {
            LOG_WARN("PBKDF2 iterations low: %u (recommended >= %u)",
                     cfg->iterations, NIMCP_KDF_PBKDF2_RECOMMENDED_ITERATIONS);
        }
    }

    // Allocate context
    nimcp_kdf_context_t ctx = (nimcp_kdf_context_t)nimcp_calloc(1, sizeof(struct nimcp_kdf_context));
    if (!ctx) {
        LOG_ERROR("Failed to allocate KDF context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return NULL;
    }

    ctx->magic = NIMCP_KDF_MAGIC;
    memcpy(&ctx->config, cfg, sizeof(nimcp_kdf_config_t));
    memset(&ctx->stats, 0, sizeof(nimcp_kdf_stats_t));
    ctx->stats.algorithm = cfg->algorithm;
    ctx->stats.current_memory_kb = cfg->memory_kb;
    ctx->stats.current_iterations = cfg->iterations;
    ctx->bio_async_registered = false;
    ctx->bio_ctx = NULL;
    ctx->algorithm_state = NULL;

    // Initialize libsodium if using Argon2
#ifdef NIMCP_ENABLE_LIBSODIUM
    if (cfg->algorithm == NIMCP_KDF_ARGON2ID) {
        if (sodium_init() < 0) {
            LOG_ERROR("Failed to initialize libsodium");
            nimcp_free(ctx);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_kdf_create: validation failed");
            return NULL;
        }
    }
#endif

    // Register with bio-async
    s_register_bio_async(ctx);

    LOG_INFO("KDF context created: algorithm=%s, memory=%u KB, iterations=%u",
             nimcp_kdf_algorithm_name(cfg->algorithm),
             cfg->memory_kb, cfg->iterations);

    return ctx;
}

void nimcp_kdf_destroy(nimcp_kdf_context_t ctx)
{
    if (!ctx) {
        return;
    }

    if (ctx->magic != NIMCP_KDF_MAGIC) {
        LOG_ERROR("Invalid magic in KDF destroy");
        return;
    }

    // Unregister from bio-async
    if (ctx->bio_async_registered && ctx->bio_ctx) {
        bio_router_unregister_module(ctx->bio_ctx);
    }

    // Wipe algorithm state if any
    if (ctx->algorithm_state) {
        nimcp_secure_zero(ctx->algorithm_state, 4096);  // Generous size
        nimcp_free(ctx->algorithm_state);
    }

    // Secure wipe and free
    ctx->magic = 0;
    nimcp_secure_zero(ctx, sizeof(struct nimcp_kdf_context));
    nimcp_free(ctx);

    LOG_INFO("KDF context destroyed");
}

nimcp_result_t nimcp_kdf_update_config(nimcp_kdf_context_t ctx, const nimcp_kdf_config_t* config)
{
    NIMCP_CHECK_THROW(ctx && ctx->magic == NIMCP_KDF_MAGIC, NIMCP_ERROR_INVALID_PARAM, "invalid KDF context");
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    // Validate new configuration (same checks as create)
    NIMCP_CHECK_THROW(config->algorithm < NIMCP_KDF_ALGORITHM_COUNT, NIMCP_ERROR_INVALID_PARAM, "invalid algorithm");

    if (config->algorithm == NIMCP_KDF_ARGON2ID) {
        NIMCP_CHECK_THROW(config->memory_kb >= 8192 && config->iterations >= 1 &&
                          config->parallelism >= 1 && config->parallelism <= 64, NIMCP_ERROR_INVALID_PARAM, "invalid Argon2 parameters");
    }

    // Update configuration
    memcpy(&ctx->config, config, sizeof(nimcp_kdf_config_t));
    ctx->stats.algorithm = config->algorithm;
    ctx->stats.current_memory_kb = config->memory_kb;
    ctx->stats.current_iterations = config->iterations;

    LOG_INFO("KDF configuration updated");

    return NIMCP_SUCCESS;
}

//=============================================================================
// Key Derivation Operations
//=============================================================================

nimcp_result_t nimcp_kdf_derive(
    nimcp_kdf_context_t ctx,
    const char* password,
    size_t password_len,
    const uint8_t* salt,
    size_t salt_len,
    uint8_t* key_out,
    size_t key_len)
{
    return nimcp_kdf_derive_with_ad(ctx, password, password_len,
                                    salt, salt_len,
                                    NULL, 0,
                                    key_out, key_len);
}

nimcp_result_t nimcp_kdf_derive_with_ad(
    nimcp_kdf_context_t ctx,
    const char* password,
    size_t password_len,
    const uint8_t* salt,
    size_t salt_len,
    const uint8_t* ad,
    size_t ad_len,
    uint8_t* key_out,
    size_t key_len)
{
    NIMCP_CHECK_THROW_MSG(ctx && ctx->magic == NIMCP_KDF_MAGIC, NIMCP_ERROR_INVALID_PARAM, "invalid KDF context");
    NIMCP_CHECK_THROW_MSG(password && salt && key_out, NIMCP_ERROR_NULL_POINTER, "password, salt, or key_out is NULL");
    NIMCP_CHECK_THROW_MSG(password_len > 0 && password_len <= NIMCP_KDF_MAX_PASSWORD_LEN, NIMCP_ERROR_INVALID_PARAM, "invalid password length: %zu", password_len);
    NIMCP_CHECK_THROW_MSG(salt_len >= NIMCP_KDF_MIN_SALT_LEN, NIMCP_ERROR_INVALID_PARAM, "salt too short: %zu < %d", salt_len, NIMCP_KDF_MIN_SALT_LEN);
    NIMCP_CHECK_THROW_MSG(key_len > 0 && key_len <= NIMCP_KDF_MAX_KEY_LEN, NIMCP_ERROR_INVALID_PARAM, "invalid key length: %zu", key_len);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    nimcp_result_t result = NIMCP_SUCCESS;

    if (ctx->config.enable_logging) {
        LOG_INFO("Deriving key: algorithm=%s, key_len=%zu, salt_len=%zu",
                 nimcp_kdf_algorithm_name(ctx->config.algorithm),
                 key_len, salt_len);
    }

    // Perform derivation based on algorithm
    if (ctx->config.algorithm == NIMCP_KDF_ARGON2ID) {
#ifdef NIMCP_ENABLE_LIBSODIUM
        // Use libsodium's crypto_pwhash
        unsigned long long ops_limit = ctx->config.iterations;
        size_t mem_limit = (size_t)ctx->config.memory_kb * 1024;

        int ret = crypto_pwhash(
            key_out, key_len,
            password, password_len,
            salt,
            ops_limit,
            mem_limit,
            crypto_pwhash_ALG_ARGON2ID13
        );

        if (ret != 0) {
            LOG_ERROR("Argon2id derivation failed");
            result = NIMCP_ERROR_INVALID_PARAM;
        }
#else
        // Argon2 not available, fall back to PBKDF2
        LOG_WARN("Argon2id not available, using PBKDF2 fallback");
        result = s_pbkdf2_sha256(password, password_len,
                                salt, salt_len,
                                NIMCP_KDF_PBKDF2_RECOMMENDED_ITERATIONS,
                                key_out, key_len);
#endif
    } else if (ctx->config.algorithm == NIMCP_KDF_PBKDF2_SHA256) {
        result = s_pbkdf2_sha256(password, password_len,
                                salt, salt_len,
                                ctx->config.iterations,
                                key_out, key_len);
    } else {
        LOG_ERROR("Unsupported algorithm: %d", ctx->config.algorithm);
        result = NIMCP_ERROR_NOT_IMPLEMENTED;
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    // Update statistics
    if (ctx->config.enable_statistics && result == NIMCP_SUCCESS) {
        ctx->stats.derivations_performed++;
        ctx->stats.total_bytes_derived += key_len;

        double elapsed_ms = (end.tv_sec - start.tv_sec) * (double)NIMCP_MS_PER_SEC +
                           (end.tv_nsec - start.tv_nsec) / (double)NIMCP_NS_PER_MS;

        ctx->stats.total_derivation_time_ms += elapsed_ms;
        ctx->stats.avg_derivation_time_ms =
            ctx->stats.total_derivation_time_ms / ctx->stats.derivations_performed;

        if (elapsed_ms > ctx->stats.max_derivation_time_ms) {
            ctx->stats.max_derivation_time_ms = elapsed_ms;
        }

        if (ctx->config.enable_logging) {
            LOG_INFO("Key derived in %.2f ms", elapsed_ms);
        }
    }

    return result;
}

//=============================================================================
// Salt Generation
//=============================================================================

nimcp_result_t nimcp_kdf_generate_salt(uint8_t* salt, size_t salt_len)
{
    NIMCP_CHECK_THROW_MSG(salt, NIMCP_ERROR_NULL_POINTER, "salt buffer is NULL");
    NIMCP_CHECK_THROW_MSG(salt_len >= NIMCP_KDF_MIN_SALT_LEN, NIMCP_ERROR_INVALID_PARAM, "salt too short: %zu < %d", salt_len, NIMCP_KDF_MIN_SALT_LEN);

    // Platform-specific CSPRNG
#if defined(__linux__) && defined(SYS_getrandom)
    // Linux: getrandom syscall (kernel 3.17+)
    ssize_t ret = getrandom(salt, salt_len, 0);
    NIMCP_CHECK_THROW_MSG(ret >= 0 && (size_t)ret == salt_len, NIMCP_ERROR_INVALID_PARAM, "getrandom failed");

#elif defined(_WIN32)
    // Windows: BCryptGenRandom
    NTSTATUS status = BCryptGenRandom(NULL, salt, (ULONG)salt_len,
                                     BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    NIMCP_CHECK_THROW_MSG(BCRYPT_SUCCESS(status), NIMCP_ERROR_INVALID_PARAM, "BCryptGenRandom failed: 0x%08lx", status);

#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    // BSD/macOS: arc4random_buf
    arc4random_buf(salt, salt_len);

#else
    // Fallback: /dev/urandom
    int fd = open("/dev/urandom", O_RDONLY);
    NIMCP_CHECK_THROW_MSG(fd >= 0, NIMCP_ERROR_INVALID_PARAM, "failed to open /dev/urandom");

    ssize_t ret = read(fd, salt, salt_len);
    close(fd);

    NIMCP_CHECK_THROW_MSG(ret >= 0 && (size_t)ret == salt_len, NIMCP_ERROR_INVALID_PARAM, "failed to read from /dev/urandom");
#endif

    LOG_DEBUG("Generated %zu-byte random salt", salt_len);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Statistics and Utility Functions
//=============================================================================

nimcp_result_t nimcp_kdf_get_stats(nimcp_kdf_context_t ctx, nimcp_kdf_stats_t* stats)
{
    NIMCP_CHECK_THROW(ctx && ctx->magic == NIMCP_KDF_MAGIC, NIMCP_ERROR_INVALID_PARAM, "invalid KDF context");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");

    memcpy(stats, &ctx->stats, sizeof(nimcp_kdf_stats_t));
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_kdf_reset_stats(nimcp_kdf_context_t ctx)
{
    NIMCP_CHECK_THROW(ctx && ctx->magic == NIMCP_KDF_MAGIC, NIMCP_ERROR_INVALID_PARAM, "invalid KDF context");

    // Preserve algorithm info
    nimcp_kdf_algorithm_t algo = ctx->stats.algorithm;
    uint32_t mem = ctx->stats.current_memory_kb;
    uint32_t iter = ctx->stats.current_iterations;

    memset(&ctx->stats, 0, sizeof(nimcp_kdf_stats_t));

    ctx->stats.algorithm = algo;
    ctx->stats.current_memory_kb = mem;
    ctx->stats.current_iterations = iter;

    LOG_DEBUG("KDF statistics reset");

    return NIMCP_SUCCESS;
}

const char* nimcp_kdf_algorithm_name(nimcp_kdf_algorithm_t algorithm)
{
    switch (algorithm) {
        case NIMCP_KDF_ARGON2ID:
            return "Argon2id";
        case NIMCP_KDF_PBKDF2_SHA256:
            return "PBKDF2-HMAC-SHA256";
        default:
            return "Unknown";
    }
}

bool nimcp_kdf_verify_params(const nimcp_kdf_config_t* config, size_t salt_len)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_kdf_verify_params: config is NULL");
        return false;
    }

    if (salt_len < NIMCP_KDF_MIN_SALT_LEN) {
        LOG_WARN("Salt too short: %zu < %d", salt_len, NIMCP_KDF_MIN_SALT_LEN);
        return false;
    }

    if (config->algorithm == NIMCP_KDF_ARGON2ID) {
        if (config->memory_kb < NIMCP_KDF_DEFAULT_MEMORY_KB) {
            LOG_WARN("Argon2 memory below recommended: %u < %u KB",
                     config->memory_kb, NIMCP_KDF_DEFAULT_MEMORY_KB);
            return false;
        }

        if (config->iterations < NIMCP_KDF_DEFAULT_ITERATIONS) {
            LOG_WARN("Argon2 iterations below recommended: %u < %u",
                     config->iterations, NIMCP_KDF_DEFAULT_ITERATIONS);
            return false;
        }

        if (config->parallelism < 1) {
            LOG_WARN("Argon2 parallelism too low");
            return false;
        }
    } else if (config->algorithm == NIMCP_KDF_PBKDF2_SHA256) {
        if (config->iterations < NIMCP_KDF_PBKDF2_RECOMMENDED_ITERATIONS) {
            LOG_WARN("PBKDF2 iterations below recommended: %u < %u",
                     config->iterations, NIMCP_KDF_PBKDF2_RECOMMENDED_ITERATIONS);
            return false;
        }
    }

    return true;
}

nimcp_result_t nimcp_kdf_estimate_time(nimcp_kdf_context_t ctx, double* estimated_ms)
{
    NIMCP_CHECK_THROW(ctx && ctx->magic == NIMCP_KDF_MAGIC, NIMCP_ERROR_INVALID_PARAM, "invalid KDF context");
    NIMCP_CHECK_THROW(estimated_ms, NIMCP_ERROR_NULL_POINTER, "estimated_ms is NULL");

    // If we have statistics, use average
    if (ctx->stats.derivations_performed > 0) {
        *estimated_ms = ctx->stats.avg_derivation_time_ms;
        LOG_DEBUG("Estimated time from stats: %.2f ms", *estimated_ms);
        return NIMCP_SUCCESS;
    }

    // Otherwise, run a quick benchmark
    LOG_INFO("Running benchmark to estimate derivation time...");

    uint8_t test_salt[32];
    uint8_t test_key[32];
    nimcp_kdf_generate_salt(test_salt, sizeof(test_salt));

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    nimcp_result_t result = nimcp_kdf_derive(ctx, "benchmark", 9,
                                             test_salt, sizeof(test_salt),
                                             test_key, sizeof(test_key));

    clock_gettime(CLOCK_MONOTONIC, &end);

    nimcp_secure_zero(test_salt, sizeof(test_salt));
    nimcp_secure_zero(test_key, sizeof(test_key));

    if (result != NIMCP_SUCCESS) {
        return result;
    }

    *estimated_ms = (end.tv_sec - start.tv_sec) * 1000.0 +
                   (end.tv_nsec - start.tv_nsec) / 1e6;

    LOG_INFO("Benchmark complete: %.2f ms", *estimated_ms);

    return NIMCP_SUCCESS;
}
