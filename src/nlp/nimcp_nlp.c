#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_nlp.c - NLP Integration Module Implementation
//=============================================================================
/**
 * @file nimcp_nlp.c
 * @brief NLP integration combining synapses, attention, and neuromodulation
 *
 * ARCHITECTURE:
 * This module acts as a Facade over three major NIMCP subsystems:
 * 1. Neural Network: Programmable synapses with custom compute functions
 * 2. Multihead Attention: Query-key-value semantic routing
 * 3. Neuromodulators: Reward-based learning and gating
 *
 * DESIGN PATTERN: Facade Pattern
 * - Provides unified interface to complex subsystem interactions
 * - Hides internal wiring complexity from users
 * - Manages lifecycle of multiple interdependent components
 *
 * CRITICAL INTEGRATION POINTS:
 * - synapse_compute_context_t bridges synapses ↔ attention ↔ neuromodulation
 * - Attention output becomes global_state for synapse computation
 * - Neuromodulator levels modulate synaptic transmission and learning
 *
 * REFACTOR (2025-11-28):
 * - Replaced all malloc/free with unified memory (nimcp_malloc/nimcp_free)
 * - Added extensive logging using LOG_MODULE_* macros
 * - Made hyperparameters configurable via config system
 * - Registered with security system
 * - Added async event publishing for loose coupling
 *
 * @author Claude Code + NIMCP Development Team
 * @date 2025-11-28
 * @version 3.0.0
 */

#include "nlp/nimcp_nlp.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_unified_memory.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/config/nimcp_dynamic_config.h"
#include "security/nimcp_security_integration.h"
#include "core/synapse_compute/nimcp_synapse_compute.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#define LOG_MODULE "NLP"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/thread/nimcp_thread_rand.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(nlp)

//=============================================================================
// Module Constants
//=============================================================================

#define NLP_MODULE_NAME "nlp"
#define NLP_SECURITY_MODULE_ID 0x4E4C5000  // 'NLP\0'

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal NLP network structure
 *
 * WHAT: Complete state for integrated NLP system
 * WHY: Encapsulates all subsystems and their interconnections
 * HOW: Aggregates handles to neural net, attention, neuromodulators
 *
 * DESIGN: Composite Pattern - treats network as unified entity
 */
struct nlp_network_struct {
    // Core subsystems
    neural_network_t network;              /**< Base neural network */
    multihead_attention_t attention;       /**< Multihead attention system */
    neuromodulator_system_t neuromodulators; /**< Neuromodulator system */

    // Configuration
    nlp_network_config_t config;           /**< Configuration (copy) */

    // Embedding matrix: vocab_size × embedding_dim
    float* embeddings;                     /**< Word embeddings [vocab_size × embedding_dim] */

    // Attention context (shared with synapses via global_state)
    float* attention_output;               /**< Last attention output [sequence_length × output_dim] */
    uint32_t attention_output_size;        /**< Size of attention output buffer */

    // Sequence processing state
    uint32_t last_sequence_length;         /**< Length of last processed sequence */

    // Security integration
    uint32_t security_module_id;           /**< Security module registration ID */
    bool security_registered;              /**< Security registration flag */

    // Bio-async integration
    bio_module_context_t bio_ctx;          /**< Bio-async module context */
    bool bio_async_enabled;                /**< Bio-async enabled flag */
};

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Initialize embedding matrix
 *
 * WHAT: Allocate and randomly initialize word embeddings
 * WHY: Need continuous representations for discrete tokens
 * HOW: Xavier/Glorot initialization for stable gradients
 *
 * COMPLEXITY: O(v × d) where v=vocab_size, d=embedding_dim
 *
 * @param network NLP network
 * @return true on success, false on allocation failure
 */
static bool init_embeddings(nlp_network_t network) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_embeddings: NULL network");
        LOG_ERROR(LOG_MODULE, "init_embeddings: NULL network");
        return false;
    }

    // Get configurable parameters
    uint32_t vocab_size = (uint32_t)config_get_int("nlp.vocab_size", network->config.vocab_size);
    uint32_t embedding_dim = (uint32_t)config_get_int("nlp.embedding_dim", network->config.embedding_dim);

    LOG_INFO(LOG_MODULE, "Initializing embeddings: vocab_size=%u, embedding_dim=%u",
                    vocab_size, embedding_dim);

    // Allocate embedding matrix using unified memory
    // Overflow check: vocab_size * embedding_dim * sizeof(float)
    if (vocab_size > 0 && embedding_dim > SIZE_MAX / sizeof(float) / vocab_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT,
            "Embedding allocation overflow: vocab_size=%u, embedding_dim=%u", vocab_size, embedding_dim);
        LOG_ERROR(LOG_MODULE, "Embedding allocation would overflow: vocab_size=%u, embedding_dim=%u",
                  vocab_size, embedding_dim);
        return false;
    }
    size_t size = (size_t)vocab_size * embedding_dim * sizeof(float);
    network->embeddings = (float*)nimcp_malloc(size);
    if (!network->embeddings) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate %zu bytes for embeddings", size);
        LOG_ERROR(LOG_MODULE, "Failed to allocate %zu bytes for embeddings", size);
        return false;
    }

    // Xavier initialization: uniform(-sqrt(6/(vocab+dim)), +sqrt(6/(vocab+dim)))
    float range = sqrtf(6.0F / (vocab_size + embedding_dim));
    float init_scale = config_get_float("nlp.embedding_init_scale", 1.0);
    range *= init_scale;

    LOG_DEBUG(LOG_MODULE, "Xavier initialization range: +/- %f (scale=%f)", range, init_scale);

    for (uint32_t i = 0; i < vocab_size * embedding_dim; i++) {
        float r = (float)nimcp_tl_rand() / RAND_MAX;  // [0,1]
        network->embeddings[i] = 2.0F * range * r - range;  // [-range, +range]
    }

    LOG_INFO(LOG_MODULE, "Successfully initialized %u embeddings", vocab_size);
    return true;
}

/**
 * @brief Configure synapses with NLP-specific compute functions
 *
 * WHAT: Attach attention/neuromodulation compute functions to synapses
 * WHY: Enable context-dependent transmission for language
 * HOW: Iterate through neurons, set compute_function pointers
 *
 * COMPLEXITY: O(n × s) where n=neurons, s=synapses_per_neuron
 *
 * @param network NLP network
 * @return Number of synapses configured
 */
static uint32_t configure_synapses(nlp_network_t network) {
    if (!network || !network->network) {
        LOG_WARN(LOG_MODULE, "configure_synapses: Invalid network");
        return 0;
    }

    uint32_t configured = 0;

    // Get configuration flags
    bool use_attention = config_get_bool("nlp.use_attention_synapses",
                                         network->config.use_attention_synapses);
    bool use_semantic = config_get_bool("nlp.use_semantic_synapses",
                                        network->config.use_semantic_synapses);
    bool use_gating = config_get_bool("nlp.use_gating_synapses",
                                      network->config.use_gating_synapses);
    bool use_neuromodulated = config_get_bool("nlp.use_neuromodulated_synapses",
                                              network->config.use_neuromodulated_synapses);

    LOG_DEBUG(LOG_MODULE, "Synapse config: attention=%d, semantic=%d, gating=%d, neuromod=%d",
                     use_attention, use_semantic, use_gating, use_neuromodulated);

    // NOTE: This requires access to internal network structure
    // We'll implement this after updating neural_network_struct
    //
    // TODO: Iterate through neurons and configure synapses based on config flags
    // For now, just log that configuration is skipped

    LOG_INFO(LOG_MODULE, "Synapse configuration: %u synapses configured", configured);
    return configured;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

nlp_network_t nlp_network_create(const nlp_network_config_t* config) {
    LOG_INFO(LOG_MODULE, "Creating NLP network");

    // Guard: Validate input
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlp_network_create: NULL configuration provided");
        LOG_ERROR(LOG_MODULE, "NULL configuration provided");
        return NULL;
    }
    if (config->vocab_size == 0 || config->embedding_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAMETER, "Invalid config: vocab_size=%u, embedding_dim=%u", config->vocab_size, config->embedding_dim);
        LOG_ERROR(LOG_MODULE, "Invalid config: vocab_size=%u, embedding_dim=%u",
                         config->vocab_size, config->embedding_dim);
        return NULL;
    }

    // Allocate network structure using unified memory
    nlp_network_t network = (nlp_network_t)nimcp_calloc(1, sizeof(struct nlp_network_struct));
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nlp_network_create: Failed to allocate network structure");
        LOG_ERROR(LOG_MODULE, "Failed to allocate network structure");
        return NULL;
    }

    // Copy configuration
    memcpy(&network->config, config, sizeof(nlp_network_config_t));
    network->security_registered = false;

    // Create base neural network - fail only if config is completely invalid
    network->network = neural_network_create(&config->network_config);
    if (!network->network) {
        LOG_WARN(LOG_MODULE, "Failed to create with provided config, trying defaults");

        // Try with a minimal default config
        network_config_t default_config = {0};
        default_config.num_neurons = config->embedding_dim * 2;  // Reasonable default
        default_config.input_size = config->embedding_dim;
        default_config.output_size = config->embedding_dim;
        default_config.num_layers = 2;
        default_config.ei_ratio = 0.8F;
        default_config.learning_rate = config_get_float("nlp.learning_rate", 0.01F);
        default_config.stdp_window = config_get_float("nlp.stdp_window", 20.0F);
        default_config.refractory_period = config_get_float("nlp.refractory_period", 2.0F);
        default_config.min_weight = 0.0F;
        default_config.max_weight = 1.0F;

        network->network = neural_network_create(&default_config);
        if (!network->network) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "nlp_network_create: Failed to create neural network with defaults");
            LOG_ERROR(LOG_MODULE, "Failed to create neural network with defaults");
            nimcp_free(network);
            return NULL;
        }
        LOG_INFO(LOG_MODULE, "Created neural network with default config");
    }

    // Create multihead attention system - optional, don't fail if it can't be created
    network->attention = multihead_attention_create(&config->attention_config);
    if (!network->attention) {
        LOG_WARN(LOG_MODULE, "Failed to create attention system (graceful degradation)");
    } else {
        LOG_INFO(LOG_MODULE, "Created attention system with %u heads",
                        config->attention_config.num_heads);
    }

    // Create neuromodulator system - optional, don't fail if it can't be created
    network->neuromodulators = neuromodulator_system_create(&config->neuromod_config);
    if (!network->neuromodulators) {
        LOG_WARN(LOG_MODULE, "Failed to create neuromodulator system (graceful degradation)");
    } else {
        LOG_INFO(LOG_MODULE, "Created neuromodulator system");
    }

    // CRITICAL: Wire neuromodulator system to neural network
    // This enables synapses to access neuromodulator levels during computation
    if (network->neuromodulators) {
        neural_network_set_neuromodulator_system(network->network, network->neuromodulators);
        LOG_DEBUG(LOG_MODULE, "Wired neuromodulators to neural network");
    }

    // Initialize embeddings
    if (!init_embeddings(network)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "nlp_network_create: Failed to initialize embeddings");
        LOG_ERROR(LOG_MODULE, "Failed to initialize embeddings");
        if (network->neuromodulators) neuromodulator_system_destroy(network->neuromodulators);
        if (network->attention) multihead_attention_destroy(network->attention);
        neural_network_destroy(network->network);
        nimcp_free(network);
        return NULL;
    }

    // Allocate attention output buffer using unified memory
    uint32_t max_seq_len = (uint32_t)config_get_int("nlp.max_sequence_length",
                                                     config->max_sequence_length);
    network->attention_output_size = max_seq_len * config->attention_config.output_dim;
    network->attention_output = (float*)nimcp_calloc(network->attention_output_size, sizeof(float));
    if (!network->attention_output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nlp_network_create: Failed to allocate attention output buffer");
        LOG_ERROR(LOG_MODULE, "Failed to allocate attention output buffer");
        nimcp_free(network->embeddings);
        if (network->neuromodulators) neuromodulator_system_destroy(network->neuromodulators);
        if (network->attention) multihead_attention_destroy(network->attention);
        neural_network_destroy(network->network);
        nimcp_free(network);
        return NULL;
    }
    LOG_DEBUG(LOG_MODULE, "Allocated attention output buffer: %u elements",
                     network->attention_output_size);

    // Configure synapses with NLP compute functions
    uint32_t configured = configure_synapses(network);
    LOG_INFO(LOG_MODULE, "Configured %u synapses", configured);

    // Bio-async registration
    network->bio_ctx = NULL;
    network->bio_async_enabled = false;
    if (config->enable_bio_async && bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_NLP,
            .module_name = "nlp",
            .inbox_capacity = 64,
            .user_data = network
        };
        network->bio_ctx = bio_router_register_module(&bio_info);
        if (network->bio_ctx) {
            network->bio_async_enabled = true;
            LOG_INFO(LOG_MODULE, "Bio-async communication enabled");
        } else {
            LOG_WARN(LOG_MODULE, "Bio-async registration failed (router may not be initialized)");
        }
    } else {
        LOG_DEBUG(LOG_MODULE, "Bio-async not enabled (config=%d, router_init=%d)",
                  config->enable_bio_async, bio_router_is_initialized());
    }

    LOG_INFO(LOG_MODULE, "NLP network created successfully");
    return network;
}

void nlp_network_destroy(nlp_network_t network) {
    if (!network) {
        LOG_DEBUG(LOG_MODULE, "nlp_network_destroy: NULL network (no-op)");
        return;
    }

    LOG_INFO(LOG_MODULE, "Destroying NLP network");

    // Unregister from bio-async
    if (network->bio_async_enabled && network->bio_ctx) {
        bio_router_unregister_module(network->bio_ctx);
        network->bio_ctx = NULL;
        network->bio_async_enabled = false;
        LOG_DEBUG(LOG_MODULE, "Bio-async unregistered");
    }

    // Destroy in reverse creation order using unified memory
    if (network->attention_output) {
        nimcp_free(network->attention_output);
        LOG_DEBUG(LOG_MODULE, "Freed attention output buffer");
    }

    if (network->embeddings) {
        nimcp_free(network->embeddings);
        LOG_DEBUG(LOG_MODULE, "Freed embeddings");
    }

    if (network->neuromodulators) {
        neuromodulator_system_destroy(network->neuromodulators);
        LOG_DEBUG(LOG_MODULE, "Destroyed neuromodulator system");
    }

    if (network->attention) {
        multihead_attention_destroy(network->attention);
        LOG_DEBUG(LOG_MODULE, "Destroyed attention system");
    }

    if (network->network) {
        neural_network_destroy(network->network);
        LOG_DEBUG(LOG_MODULE, "Destroyed neural network");
    }

    nimcp_free(network);
    LOG_INFO(LOG_MODULE, "NLP network destroyed");
}

//=============================================================================
// Security Integration
//=============================================================================

bool nlp_network_register_security(nlp_network_t network, void* security_ctx) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlp_network_register_security: NULL network");
        LOG_ERROR(LOG_MODULE, "register_security: NULL network");
        return false;
    }

    if (!security_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlp_network_register_security: NULL security context");
        LOG_WARN(LOG_MODULE, "register_security: NULL security context");
        return false;
    }

    if (network->security_registered) {
        LOG_WARN(LOG_MODULE, "NLP network already registered with security");
        return true;
    }

    nimcp_sec_integration_t* sec = (nimcp_sec_integration_t*)security_ctx;
    nimcp_result_t result = nimcp_sec_register_module(sec, NLP_MODULE_NAME, NIMCP_SEC_CAT_COGNITIVE, &network->security_module_id);

    if (result != NIMCP_SUCCESS || network->security_module_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "nlp_network_register_security: Failed to register with security system");
        LOG_ERROR(LOG_MODULE, "Failed to register with security system");
        return false;
    }

    network->security_registered = true;
    LOG_INFO(LOG_MODULE, "Registered with security (module_id=0x%08X)",
                    network->security_module_id);
    return true;
}

//=============================================================================
// Forward Pass Functions
//=============================================================================

bool nlp_network_forward(
    nlp_network_t network,
    const uint32_t* token_ids,
    uint32_t sequence_length,
    float* output,
    uint32_t output_dim
) {
    // Process pending bio-async messages
    if (network && network->bio_ctx) {
        bio_router_process_inbox(network->bio_ctx, 5);
    }

    // Guard: Validate inputs
    if (!network || !token_ids || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlp_network_forward: NULL parameter (network=%p, tokens=%p, output=%p)", (void*)network, (const void*)token_ids, (void*)output);
        LOG_ERROR(LOG_MODULE, "forward: NULL parameter (network=%p, tokens=%p, output=%p)",
                         (void*)network, (const void*)token_ids, (void*)output);
        return false;
    }

    uint32_t max_seq_len = (uint32_t)config_get_int("nlp.max_sequence_length",
                                                     network->config.max_sequence_length);
    if (sequence_length == 0 || sequence_length > max_seq_len) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAMETER, "nlp_network_forward: Invalid sequence_length=%u (max=%u)", sequence_length, max_seq_len);
        LOG_ERROR(LOG_MODULE, "forward: Invalid sequence_length=%u (max=%u)",
                         sequence_length, max_seq_len);
        return false;
    }

    if (output_dim != network->config.attention_config.output_dim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAMETER, "nlp_network_forward: output_dim mismatch (got=%u, expected=%u)", output_dim, network->config.attention_config.output_dim);
        LOG_ERROR(LOG_MODULE, "forward: output_dim mismatch (got=%u, expected=%u)",
                         output_dim, network->config.attention_config.output_dim);
        return false;
    }

    LOG_DEBUG(LOG_MODULE, "Forward pass: sequence_length=%u, output_dim=%u",
                     sequence_length, output_dim);

    // Step 1: Convert tokens to embeddings
    uint32_t embedding_dim = network->config.embedding_dim;
    float* sequence_embeddings = (float*)nimcp_malloc(sequence_length * embedding_dim * sizeof(float));
    if (!sequence_embeddings) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nlp_network_forward: Failed to allocate sequence embeddings");
        LOG_ERROR(LOG_MODULE, "forward: Failed to allocate sequence embeddings");
        return false;
    }

    for (uint32_t i = 0; i < sequence_length; i++) {
        uint32_t token = token_ids[i];
        if (token >= network->config.vocab_size) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAMETER, "nlp_network_forward: Invalid token_id=%u at position %u (vocab_size=%u)", token, i, network->config.vocab_size);
            LOG_ERROR(LOG_MODULE, "forward: Invalid token_id=%u at position %u (vocab_size=%u)",
                             token, i, network->config.vocab_size);
            nimcp_free(sequence_embeddings);
            return false;
        }

        // Copy embedding for this token
        memcpy(
            &sequence_embeddings[i * embedding_dim],
            &network->embeddings[token * embedding_dim],
            embedding_dim * sizeof(float)
        );
    }
    LOG_DEBUG(LOG_MODULE, "Converted %u tokens to embeddings", sequence_length);

    // Step 2: Run multihead attention over embeddings
    bool attention_success = true;
    if (network->attention) {
        attention_success = multihead_attention_forward(
            network->attention,
            sequence_embeddings,
            sequence_length,
            NULL,  // No salience weighting (can be added later)
            network->attention_output
        );

        if (!attention_success) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "nlp_network_forward: Attention forward pass failed");
            LOG_ERROR(LOG_MODULE, "forward: Attention forward pass failed");
        } else {
            LOG_DEBUG(LOG_MODULE, "Attention forward pass completed");
        }
    } else {
        LOG_DEBUG(LOG_MODULE, "No attention system, copying embeddings directly");
        // Copy embeddings directly to output if no attention
        // Cap copy size to attention_output_size to prevent buffer overflow
        size_t copy_elems = sequence_length * embedding_dim;
        if (copy_elems > network->attention_output_size) {
            copy_elems = network->attention_output_size;
        }
        memcpy(network->attention_output, sequence_embeddings,
               copy_elems * sizeof(float));
    }

    nimcp_free(sequence_embeddings);

    if (!attention_success) {
        return false;  /* Already reported by first throw above */
    }

    // Store sequence length for context
    network->last_sequence_length = sequence_length;

    // Step 3: Wire attention output into network as global state
    // CRITICAL: This makes attention output available to all synapses
    neural_network_set_global_state(
        network->network,
        network->attention_output,
        network->attention_output_size
    );
    LOG_DEBUG(LOG_MODULE, "Set global state with attention output");

    // Step 4: Extract output from network
    // For now, copy attention output directly
    // In full implementation, this would come from output neurons
    uint32_t output_size = sequence_length * output_dim;
    if (output_size > network->attention_output_size) {
        output_size = network->attention_output_size;
        LOG_WARN(LOG_MODULE, "Output size clamped to %u", output_size);
    }

    memcpy(output, network->attention_output, output_size * sizeof(float));
    LOG_INFO(LOG_MODULE, "Forward pass completed successfully");

    return true;
}

bool nlp_network_get_embedding(
    nlp_network_t network,
    uint32_t token_id,
    float* embedding
) {
    // Guard: Validate inputs
    if (!network || !embedding) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlp_network_get_embedding: NULL parameter");
        LOG_ERROR(LOG_MODULE, "get_embedding: NULL parameter");
        return false;
    }
    if (token_id >= network->config.vocab_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAMETER, "nlp_network_get_embedding: token_id=%u >= vocab_size=%u", token_id, network->config.vocab_size);
        LOG_ERROR(LOG_MODULE, "get_embedding: token_id=%u >= vocab_size=%u",
                         token_id, network->config.vocab_size);
        return false;
    }

    // Copy embedding
    uint32_t offset = token_id * network->config.embedding_dim;
    memcpy(
        embedding,
        &network->embeddings[offset],
        network->config.embedding_dim * sizeof(float)
    );

    LOG_DEBUG(LOG_MODULE, "Retrieved embedding for token %u", token_id);
    return true;
}

bool nlp_network_set_embedding(
    nlp_network_t network,
    uint32_t token_id,
    const float* embedding
) {
    // Guard: Validate inputs
    if (!network || !embedding) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlp_network_set_embedding: NULL parameter");
        LOG_ERROR(LOG_MODULE, "set_embedding: NULL parameter");
        return false;
    }
    if (token_id >= network->config.vocab_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAMETER, "nlp_network_set_embedding: token_id=%u >= vocab_size=%u", token_id, network->config.vocab_size);
        LOG_ERROR(LOG_MODULE, "set_embedding: token_id=%u >= vocab_size=%u",
                         token_id, network->config.vocab_size);
        return false;
    }

    // Update embedding
    uint32_t offset = token_id * network->config.embedding_dim;
    memcpy(
        &network->embeddings[offset],
        embedding,
        network->config.embedding_dim * sizeof(float)
    );

    LOG_DEBUG(LOG_MODULE, "Updated embedding for token %u", token_id);
    return true;
}

//=============================================================================
// Attention Control Functions
//=============================================================================

bool nlp_network_set_attention_gate(nlp_network_t network, float gate_signal) {
    // Guard: Validate inputs
    if (!network || !network->attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlp_network_set_attention_gate: Invalid network or no attention");
        LOG_ERROR(LOG_MODULE, "set_attention_gate: Invalid network or no attention");
        return false;
    }

    // Forward to attention system
    bool success = multihead_attention_set_gate(network->attention, gate_signal);
    LOG_DEBUG(LOG_MODULE, "Set attention gate to %f: %s",
                     gate_signal, success ? "success" : "failed");
    return success;
}

bool nlp_network_get_attention_weights(
    nlp_network_t network,
    uint32_t head_idx,
    float* weights
) {
    // Guard: Validate inputs
    if (!network || !network->attention || !weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlp_network_get_attention_weights: Invalid parameters");
        LOG_ERROR(LOG_MODULE, "get_attention_weights: Invalid parameters");
        return false;
    }
    if (head_idx >= network->config.attention_config.num_heads) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAMETER, "nlp_network_get_attention_weights: head_idx=%u >= num_heads=%u", head_idx, network->config.attention_config.num_heads);
        LOG_ERROR(LOG_MODULE, "get_attention_weights: head_idx=%u >= num_heads=%u",
                         head_idx, network->config.attention_config.num_heads);
        return false;
    }

    // IMPLEMENTATION NOTE:
    // Attention weights are computed dynamically during forward pass
    // To implement this properly, we would need to modify the attention module
    // For now, return false to indicate feature not available

    LOG_WARN(LOG_MODULE, "get_attention_weights: Feature not yet implemented");
    return false;  /* Not implemented - normal behavior */
}

//=============================================================================
// Neuromodulation Functions
//=============================================================================

float nlp_network_release_dopamine(
    nlp_network_t network,
    float reward_magnitude,
    float predicted_reward
) {
    // Guard: Validate inputs
    if (!network || !network->neuromodulators) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlp_network_release_dopamine: Invalid network or no neuromodulators");
        LOG_ERROR(LOG_MODULE, "release_dopamine: Invalid network or no neuromodulators");
        return 0.0F;
    }

    // Release dopamine and return prediction error
    float prediction_error = neuromodulator_release_dopamine(
        network->neuromodulators,
        reward_magnitude,
        predicted_reward
    );

    LOG_DEBUG(LOG_MODULE, "Released dopamine: reward=%f, predicted=%f, error=%f",
                     reward_magnitude, predicted_reward, prediction_error);
    return prediction_error;
}

float nlp_network_release_acetylcholine(
    nlp_network_t network,
    float salience
) {
    // Guard: Validate inputs
    if (!network || !network->neuromodulators) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlp_network_release_acetylcholine: Invalid network or no neuromodulators");
        LOG_ERROR(LOG_MODULE, "release_acetylcholine: Invalid network or no neuromodulators");
        return 0.0F;
    }

    // Release acetylcholine
    float level = neuromodulator_release_acetylcholine(network->neuromodulators, salience);
    LOG_DEBUG(LOG_MODULE, "Released acetylcholine: salience=%f, level=%f", salience, level);
    return level;
}

bool nlp_network_get_neuromodulator_levels(
    nlp_network_t network,
    float* dopamine,
    float* serotonin,
    float* acetylcholine,
    float* norepinephrine
) {
    // Guard: Validate inputs
    if (!network || !network->neuromodulators) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlp_network_get_neuromodulator_levels: Invalid network or no neuromodulators");
        LOG_ERROR(LOG_MODULE, "get_neuromodulator_levels: Invalid network or no neuromodulators");
        return false;
    }

    // Get neuromodulator pool
    neuromodulator_pool_t pool = neuromodulator_pool_create();
    bool success = neuromodulator_get_levels(network->neuromodulators, &pool);
    if (!success) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "nlp_network_get_neuromodulator_levels: Failed to get neuromodulator levels");
        LOG_ERROR(LOG_MODULE, "Failed to get neuromodulator levels");
        neuromodulator_pool_destroy(&pool);
        return false;
    }

    // Extract individual levels if requested
    if (dopamine) *dopamine = neuromodulator_pool_get_dopamine(&pool);
    if (serotonin) *serotonin = neuromodulator_pool_get_serotonin(&pool);
    if (acetylcholine) *acetylcholine = neuromodulator_pool_get_acetylcholine(&pool);
    if (norepinephrine) *norepinephrine = neuromodulator_pool_get_norepinephrine(&pool);

    LOG_DEBUG(LOG_MODULE, "Neuromodulator levels: DA=%f, 5HT=%f, ACh=%f, NE=%f",
                     neuromodulator_pool_get_dopamine(&pool),
                     neuromodulator_pool_get_serotonin(&pool),
                     neuromodulator_pool_get_acetylcholine(&pool),
                     neuromodulator_pool_get_norepinephrine(&pool));

    neuromodulator_pool_destroy(&pool);
    return true;
}

//=============================================================================
// Training Functions
//=============================================================================

float nlp_network_train(
    nlp_network_t network,
    const uint32_t* token_ids,
    uint32_t sequence_length,
    const float* target,
    uint32_t output_dim,
    float learning_rate
) {
    // Guard: Validate inputs
    if (!network || !token_ids || !target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlp_network_train: NULL parameter");
        LOG_ERROR(LOG_MODULE, "train: NULL parameter");
        return -1.0F;
    }

    LOG_INFO(LOG_MODULE, "Training: sequence_length=%u, lr=%f", sequence_length, learning_rate);

    // Step 1: Forward pass
    float* output = (float*)nimcp_malloc(sequence_length * output_dim * sizeof(float));
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nlp_network_train: Failed to allocate output buffer");
        LOG_ERROR(LOG_MODULE, "train: Failed to allocate output buffer");
        return -1.0F;
    }

    bool forward_success = nlp_network_forward(
        network,
        token_ids,
        sequence_length,
        output,
        output_dim
    );

    if (!forward_success) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "nlp_network_train: Forward pass failed");
        LOG_ERROR(LOG_MODULE, "train: Forward pass failed");
        nimcp_free(output);
        return -1.0F;
    }

    // Step 2: Compute loss (mean squared error)
    float loss = 0.0F;
    uint32_t total_elements = sequence_length * output_dim;
    for (uint32_t i = 0; i < total_elements; i++) {
        float error = output[i] - target[i];
        loss += error * error;
    }
    loss /= total_elements;
    LOG_DEBUG(LOG_MODULE, "MSE loss: %f", loss);

    // Step 3: Compute reward signal based on prediction quality
    float reward = expf(-loss);  // Exponential decay: perfect prediction = 1.0, bad = ~0
    LOG_DEBUG(LOG_MODULE, "Reward signal: %f", reward);

    // Step 4: Release dopamine based on reward
    nlp_network_release_dopamine(network, reward, 0.0F);

    // Step 5: Apply reward-modulated STDP to neural network
    uint64_t current_time = 0;  // TODO: Get from platform timer when available
    neural_network_apply_reward_learning(
        network->network,
        reward,
        learning_rate,
        current_time
    );
    LOG_DEBUG(LOG_MODULE, "Applied reward-modulated STDP");

    // Step 6: Update embeddings using reward-modulated Hebbian learning
    float embedding_lr = learning_rate * config_get_float("nlp.embedding_lr_scale", 0.1F);
    float regularization = config_get_float("nlp.embedding_regularization", 0.0001F);

    for (uint32_t seq_idx = 0; seq_idx < sequence_length; seq_idx++) {
        uint32_t token_id = token_ids[seq_idx];
        if (token_id >= network->config.vocab_size) continue;

        float* embedding = &network->embeddings[token_id * network->config.embedding_dim];

        for (uint32_t dim = 0; dim < network->config.embedding_dim; dim++) {
            // Hebbian component: strengthen active patterns when reward is high
            float hebbian_update = reward * embedding[dim] * embedding_lr;

            // Exploration component: small random perturbation when reward is low
            float exploration = (1.0F - reward) * ((float)nimcp_tl_rand() / RAND_MAX - 0.5F) * 0.01F;

            // L2 regularization to prevent unbounded growth
            float reg_term = regularization * embedding[dim];

            // Apply combined update
            embedding[dim] += hebbian_update + exploration - reg_term;
        }
    }
    LOG_DEBUG(LOG_MODULE, "Updated embeddings with lr=%f", embedding_lr);

    nimcp_free(output);
    LOG_INFO(LOG_MODULE, "Training completed: loss=%f, reward=%f", loss, reward);
    return loss;
}

//=============================================================================
// Utility Functions
//=============================================================================

bool nlp_network_get_stats(nlp_network_t network, network_stats_t* stats) {
    // Guard: Validate inputs
    if (!network || !stats || !network->network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlp_network_get_stats: Invalid parameters");
        LOG_ERROR(LOG_MODULE, "get_stats: Invalid parameters");
        return false;
    }

    // Forward to base network
    bool success = neural_network_get_stats(network->network, stats);
    LOG_DEBUG(LOG_MODULE, "Retrieved network statistics: %s", success ? "success" : "failed");
    return success;
}

bool nlp_network_save(nlp_network_t network, const char* filepath) {
    // UNIMPLEMENTED: NLP network serialization not needed
    // Rationale: Brain-level persistence is handled at higher architectural layers.
    (void)network;
    (void)filepath;
    LOG_WARN(LOG_MODULE, "save: Not implemented (use brain-level persistence)");
    return false;  /* Not implemented - use brain-level persistence */
}

nlp_network_t nlp_network_load(const char* filepath) {
    // UNIMPLEMENTED: NLP network deserialization not needed
    // Rationale: Brain-level persistence is handled at higher architectural layers.
    (void)filepath;
    LOG_WARN(LOG_MODULE, "load: Not implemented (use brain-level persistence)");
    return NULL;  /* Not implemented - use brain-level persistence */
}
