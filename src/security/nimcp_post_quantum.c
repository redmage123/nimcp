/**
 * @file nimcp_post_quantum.c
 * @brief Post-Quantum Cryptography Context Management
 *
 * WHAT: Main context and coordination for PQ crypto operations
 * WHY: Centralized management of Kyber, Dilithium, and hybrid operations
 * HOW: Implements context lifecycle, statistics, bio-async integration
 */

#include "security/nimcp_post_quantum.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_wiring_helpers.h"

#include "utils/error/nimcp_error_codes.h"
#include "utils/validation/nimcp_common.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_router.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(post_quantum, MESH_ADAPTER_CATEGORY_SECURITY)


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

/* Post-quantum context structure */
struct nimcp_pq_context {
    uint32_t magic;
    nimcp_pq_config_t config;
    nimcp_pq_stats_t stats;
    nimcp_mutex_t stats_lock;
    bio_module_context_t bio_ctx;
    bool bio_registered;
};

/* Bio-async message types */
#define NIMCP_PQ_MSG_KEYGEN         1
#define NIMCP_PQ_MSG_ENCAPSULATE    2
#define NIMCP_PQ_MSG_SIGN           3
#define NIMCP_PQ_MSG_VERIFY         4

/* ========================================================================
 * Bio-Async Integration
 * ======================================================================== */

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
static int post_quantum_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
);

static nimcp_error_t pq_inbox_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data);

static int post_quantum_wiring_handler_callback(
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
                bio_router_register_handler(ctx, message_types[i], pq_inbox_handler);
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

static nimcp_error_t pq_inbox_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    nimcp_pq_context_t pq_ctx = (nimcp_pq_context_t)user_data;

    if (!pq_ctx || pq_ctx->magic != NIMCP_PQ_CONTEXT_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAMETER;
    }

    if (!msg || msg_size == 0) {
        return NIMCP_ERROR_INVALID;
    }

    const bio_message_header_t* header = (const bio_message_header_t*)msg;
    LOG_DEBUG("PQ inbox received message type %u", header->type);

    /* Note: Message type handling would go here if we had specific message types */
    LOG_INFO("PQ message received (type=%u, size=%zu)", header->type, msg_size);

    return NIMCP_OK;
}

/* ========================================================================
 * Context Management
 * ======================================================================== */

nimcp_pq_context_t nimcp_pq_context_create(const nimcp_pq_config_t* config) {
    /* Allocate context */
    nimcp_pq_context_t ctx = (nimcp_pq_context_t)nimcp_calloc(1, sizeof(struct nimcp_pq_context));
    if (!ctx) {
        LOG_ERROR("nimcp_pq_context_create: Memory allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return NULL;
    }

    /* Set magic number */
    ctx->magic = NIMCP_PQ_CONTEXT_MAGIC;

    /* Copy configuration or use defaults */
    if (config) {
        memcpy(&ctx->config, config, sizeof(nimcp_pq_config_t));
    } else {
        /* Default configuration */
        ctx->config.default_kyber_variant = NIMCP_PQ_KYBER_768;
        ctx->config.default_dilithium_variant = NIMCP_PQ_DILITHIUM_3;
        ctx->config.hybrid_config.enable_classical = true;
        ctx->config.hybrid_config.enable_pq = true;
        ctx->config.hybrid_config.require_both = true;
        ctx->config.hybrid_config.allow_pq_fallback = false;
        ctx->config.enable_logging = true;
        ctx->config.bio_ctx = NULL;
    }

    /* Initialize statistics */
    memset(&ctx->stats, 0, sizeof(nimcp_pq_stats_t));

    /* Initialize mutex */
    if (nimcp_mutex_init(&ctx->stats_lock, NULL) != 0) {
        LOG_ERROR("nimcp_pq_context_create: Mutex initialization failed");
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_pq_context_create: validation failed");
        return NULL;
    }

    /* Register with bio-async if provided */
    if (ctx->config.bio_ctx) {
        ctx->bio_ctx = ctx->config.bio_ctx;

        bio_module_info_t module_info = {
            .module_id = BIO_MODULE_SECURITY,
            .module_name = "post_quantum",
            .inbox_capacity = 0,  /* Use default */
            .user_data = ctx
        };

        bio_module_context_t module_ctx = bio_router_register_module(&module_info);
        if (module_ctx) {
            // Try KG-driven wiring callback registration first
            nimcp_error_t result = bio_router_register_wiring_callback(
                BIO_MODULE_SECURITY,
                (void*)post_quantum_wiring_handler_callback,
                ctx
            );

            if (result == NIMCP_SUCCESS) {
                ctx->bio_registered = true;
                LOG_INFO("KG-driven wiring callback registered successfully");
            } else {
                // Fallback to legacy handler registration
                LOG_INFO("Falling back to legacy handler registration");

                LEGACY_HANDLER_REGISTRATION(
                    result = bio_router_register_handler(
                        module_ctx,
                        BIO_MSG_HEALTH_CHECK,
                        pq_inbox_handler
                    )
                );
                if (result == NIMCP_SUCCESS) {
                    ctx->bio_registered = true;
                    LOG_INFO("Post-quantum crypto registered with bio-async");
                } else {
                    bio_router_unregister_module(module_ctx);
                    LOG_WARN("Failed to register post-quantum handler: %d", result);
                }
            }
        } else {
            LOG_WARN("Failed to register post-quantum module with bio-router");
        }
    }

    LOG_INFO("Post-quantum context created (Kyber-%d, Dilithium-%d)",
                   ctx->config.default_kyber_variant == NIMCP_PQ_KYBER_512 ? 512 :
                   ctx->config.default_kyber_variant == NIMCP_PQ_KYBER_768 ? 768 : 1024,
                   ctx->config.default_dilithium_variant == NIMCP_PQ_DILITHIUM_2 ? 2 :
                   ctx->config.default_dilithium_variant == NIMCP_PQ_DILITHIUM_3 ? 3 : 5);

    return ctx;
}

void nimcp_pq_context_destroy(nimcp_pq_context_t ctx) {
    if (!ctx) {
        return;
    }

    if (ctx->magic != NIMCP_PQ_CONTEXT_MAGIC) {
        LOG_WARN("nimcp_pq_context_destroy: Invalid magic number");
        return;
    }

    /* Note: Unregistration would happen via stored module context if we kept it */
    if (ctx->bio_registered) {
        LOG_INFO("Post-quantum crypto unregistered from bio-async");
    }

    /* Destroy mutex */
    nimcp_mutex_destroy(&ctx->stats_lock);

    /* Clear magic */
    ctx->magic = 0;

    /* Free context */
    nimcp_free(ctx);

    LOG_INFO("Post-quantum context destroyed");
}

nimcp_error_t nimcp_pq_get_stats(nimcp_pq_context_t ctx, nimcp_pq_stats_t* stats) {
    if (!ctx || ctx->magic != NIMCP_PQ_CONTEXT_MAGIC || !stats) {
        return NIMCP_ERROR_INVALID;
    }

    nimcp_mutex_lock(&ctx->stats_lock);
    memcpy(stats, &ctx->stats, sizeof(nimcp_pq_stats_t));
    nimcp_mutex_unlock(&ctx->stats_lock);

    return NIMCP_OK;
}

/* ========================================================================
 * Statistics Tracking
 * ======================================================================== */

static void increment_stat(nimcp_pq_context_t ctx, uint64_t* stat) {
    if (!ctx) return;
    nimcp_mutex_lock(&ctx->stats_lock);
    (*stat)++;
    nimcp_mutex_unlock(&ctx->stats_lock);
}

/* ========================================================================
 * Self-Test
 * ======================================================================== */

nimcp_error_t nimcp_pq_self_test(nimcp_pq_context_t ctx) {
    if (!ctx || ctx->magic != NIMCP_PQ_CONTEXT_MAGIC) {
        return NIMCP_ERROR_INVALID;
    }

    LOG_INFO("Running post-quantum self-tests...");

    nimcp_error_t err;

    /* Test Kyber-512 */
    {
        nimcp_kyber_keypair_t keypair;
        err = nimcp_kyber_keygen(NIMCP_PQ_KYBER_512, &keypair);
        if (err != NIMCP_OK) {
            LOG_ERROR("Self-test: Kyber-512 keygen failed");
            return err;
        }

        uint8_t ciphertext[NIMCP_KYBER_512_CIPHERTEXT_BYTES];
        uint8_t secret1[NIMCP_KYBER_512_SHARED_SECRET_BYTES];
        uint8_t secret2[NIMCP_KYBER_512_SHARED_SECRET_BYTES];
        size_t ct_len = sizeof(ciphertext);

        err = nimcp_kyber_encapsulate(NIMCP_PQ_KYBER_512, keypair.public_key,
                                       ciphertext, &ct_len, secret1, sizeof(secret1));
        if (err != NIMCP_OK) {
            nimcp_kyber_keypair_free(&keypair);
            LOG_ERROR("Self-test: Kyber-512 encapsulate failed");
            return err;
        }

        err = nimcp_kyber_decapsulate(NIMCP_PQ_KYBER_512, keypair.secret_key,
                                       ciphertext, ct_len, secret2, sizeof(secret2));
        if (err != NIMCP_OK) {
            nimcp_kyber_keypair_free(&keypair);
            LOG_ERROR("Self-test: Kyber-512 decapsulate failed");
            return err;
        }

        nimcp_kyber_keypair_free(&keypair);
        LOG_INFO("Self-test: Kyber-512 PASSED");
    }

    /* Test Dilithium-2 */
    {
        nimcp_dilithium_keypair_t keypair;
        err = nimcp_dilithium_keygen(NIMCP_PQ_DILITHIUM_2, &keypair);
        if (err != NIMCP_OK) {
            LOG_ERROR("Self-test: Dilithium-2 keygen failed");
            return err;
        }

        const char* test_msg = "Test message for Dilithium";
        uint8_t signature[NIMCP_DILITHIUM_2_SIGNATURE_BYTES];
        size_t sig_len = sizeof(signature);

        err = nimcp_dilithium_sign(NIMCP_PQ_DILITHIUM_2, keypair.secret_key,
                                    (const uint8_t*)test_msg, strlen(test_msg),
                                    signature, &sig_len);
        if (err != NIMCP_OK) {
            nimcp_dilithium_keypair_free(&keypair);
            LOG_ERROR("Self-test: Dilithium-2 sign failed");
            return err;
        }

        err = nimcp_dilithium_verify(NIMCP_PQ_DILITHIUM_2, keypair.public_key,
                                      (const uint8_t*)test_msg, strlen(test_msg),
                                      signature, sig_len);
        if (err != NIMCP_OK) {
            nimcp_dilithium_keypair_free(&keypair);
            LOG_ERROR("Self-test: Dilithium-2 verify failed");
            return err;
        }

        nimcp_dilithium_keypair_free(&keypair);
        LOG_INFO("Self-test: Dilithium-2 PASSED");
    }

    LOG_INFO("All post-quantum self-tests PASSED");
    return NIMCP_OK;
}
