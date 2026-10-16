/**
 * @file nimcp_mirror_stdp.c
 * @brief Spike-Timing Dependent Plasticity Implementation for Mirror Neurons
 * @version 1.0.0
 * @date 2025-11-25
 *
 * WHAT: Implementation of biologically accurate STDP learning rules
 * WHY:  Enable timing-dependent plasticity for mirror neuron associations
 * HOW:  Track spike times, compute weight changes based on relative timing
 *
 * Implementation Details:
 * 1. Pair-based STDP: Standard nearest-neighbor spike pairing
 * 2. Triplet STDP: Accounts for burst patterns (Pfister & Gerstner 2006)
 * 3. Homeostatic scaling: Prevent runaway potentiation/depression
 * 4. Metaplasticity: BCM-like sliding threshold
 * 5. Neuromodulator gating: Dopamine and ACh modulation
 *
 * @see nimcp_mirror_stdp.h for API documentation
 */

#include "cognitive/mirror_neurons/nimcp_mirror_stdp.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define LOG_MODULE "mirror_stdp"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(mirror_stdp, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Internal Structure Definition
//=============================================================================

/**
 * @brief Internal STDP system state
 */
struct mirror_stdp_system {
    // Configuration
    mirror_stdp_config_t config;

    // Synapse storage
    mirror_stdp_synapse_t* synapses;
    uint32_t max_synapses;
    uint32_t num_synapses;

    // Action-to-synapse mapping (simple hash table)
    uint32_t* action_map;           /**< Map action_id to synapse_id */
    uint32_t action_map_size;

    // Global neuromodulator levels
    float dopamine_level;           /**< Current DA level (0-1) */
    float ach_level;                /**< Current ACh level (0-1) */

    // Global statistics
    uint32_t total_ltp_events;
    uint32_t total_ltd_events;
    float sum_ltp_magnitude;
    float sum_ltd_magnitude;
    float sum_delta_t_ltp;
    float sum_delta_t_ltd;
    uint32_t homeostatic_adjustments;
    float global_scale_factor;

    // Time tracking
    uint64_t current_time_us;
    uint64_t last_homeostasis_us;
    uint64_t last_metaplasticity_us;

    // Bio-async integration
    bio_module_context_t bio_ctx;   /**< Bio-async module context */
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Simple hash function for action mapping
 */
static inline uint32_t hash_action(uint32_t action_id, uint32_t map_size) {
    // MurmurHash-inspired mixing
    uint32_t h = action_id;
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h % map_size;
}

/**
 * @brief Add spike to history buffer
 */
static void add_spike_to_history(stdp_spike_t* history, uint8_t* count,
                                  uint64_t timestamp_us, float strength, bool is_obs) {
    // Shift old spikes down (FIFO)
    if (*count >= NIMCP_STDP_MAX_SPIKE_HISTORY) {
        memmove(&history[0], &history[1],
                (NIMCP_STDP_MAX_SPIKE_HISTORY - 1) * sizeof(stdp_spike_t));
        *count = NIMCP_STDP_MAX_SPIKE_HISTORY - 1;
    }

    // Add new spike
    uint8_t idx = *count;
    history[idx].timestamp_us = timestamp_us;
    history[idx].strength = strength;
    history[idx].is_observation = is_obs;
    (*count)++;
}

/**
 * @brief Find most recent spike within time window
 */
static const stdp_spike_t* find_recent_spike(const stdp_spike_t* history, uint8_t count,
                                              uint64_t current_us, float window_ms) {
    if (count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hash_action: count is zero");
        return NULL;
    }

    uint64_t window_us = (uint64_t)(window_ms * 1000.0F);

    // Search from most recent (end of array)
    for (int i = count - 1; i >= 0; i--) {
        uint64_t age = current_us - history[i].timestamp_us;
        if (age <= window_us) {
            return &history[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hash_action: validation failed");
    return NULL;
}

/**
 * @brief Compute exponential decay
 */
static inline float exp_decay(float dt_ms, float tau_ms) {
    return expf(-dt_ms / tau_ms);
}

//=============================================================================
// Bio-Async Message Handlers
//=============================================================================

/**
 * @brief Handle BIO_MSG_MIRROR_NEURON_ACTIVATION message
 *
 * WHAT: Processes mirror neuron activation messages
 * WHY:  Receive observation/execution spike events from mirror neurons
 * HOW:  Extract spike data, call appropriate spike handler
 */
static nimcp_error_t handle_mirror_neuron_activation(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    LOG_TRACE("handle_mirror_neuron_activation: entered");

    mirror_stdp_t stdp = (mirror_stdp_t)user_data;
    if (!stdp || !msg) {
        LOG_ERROR("handle_mirror_neuron_activation: invalid parameters");
        if (response_promise) {
            nimcp_bio_promise_fail(response_promise, -1);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "handle_mirror_neuron_activation: validation failed");
        return -1;
    }

    // Parse message (simplified - assumes custom message structure)
    // In production, define proper message type in nimcp_bio_messages.h
    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    LOG_DEBUG("handle_mirror_neuron_activation: processing activation from module 0x%x",
              header->source_module);

    // Complete with success
    if (response_promise) {
        nimcp_bio_promise_complete(response_promise, NULL);
    }

    LOG_TRACE("handle_mirror_neuron_activation: completed");
    return NIMCP_SUCCESS;
}

/**
 * @brief Handle BIO_MSG_STDP_EVENT message
 *
 * WHAT: Processes STDP spike timing events
 * WHY:  Receive spike pairs for STDP computation
 * HOW:  Extract pre/post spike times, update synaptic weights
 */
static nimcp_error_t handle_stdp_event(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    LOG_TRACE("handle_stdp_event: entered");

    mirror_stdp_t stdp = (mirror_stdp_t)user_data;
    if (!stdp || !msg || msg_size < sizeof(bio_msg_stdp_event_t)) {
        LOG_ERROR("handle_stdp_event: invalid parameters");
        if (response_promise) {
            nimcp_bio_promise_fail(response_promise, -1);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "handle_stdp_event: validation failed");
        return -1;
    }

    const bio_msg_stdp_event_t* event = (const bio_msg_stdp_event_t*)msg;

    LOG_DEBUG("handle_stdp_event: pre=%u post=%u delta_t=%.2fms",
              event->pre_neuron_id, event->post_neuron_id, event->delta_t_ms);

    // Find synapse for this neuron pair
    uint32_t synapse_id = mirror_stdp_find_synapse(stdp, event->pre_neuron_id);
    if (synapse_id == UINT32_MAX) {
        LOG_WARN("handle_stdp_event: synapse not found for neuron %u", event->pre_neuron_id);
        if (response_promise) {
            nimcp_bio_promise_fail(response_promise, -2);
        }
        return -2;
    }

    // Compute weight change
    mirror_stdp_synapse_t* syn = &stdp->synapses[synapse_id];
    float delta_w = mirror_stdp_compute_delta_w(
        stdp, event->delta_t_ms, syn->weight,
        stdp->dopamine_level, stdp->ach_level
    );

    // Apply weight change
    syn->weight = nimcp_clampf(syn->weight + delta_w, stdp->config.w_min, stdp->config.w_max);

    LOG_DEBUG("handle_stdp_event: synapse %u weight %.4f -> %.4f (delta=%.4f)",
              synapse_id, syn->weight - delta_w, syn->weight, delta_w);

    // Complete with response
    if (response_promise) {
        bio_msg_weight_update_response_t response = {0};
        bio_msg_init_header(&response.header, BIO_MSG_WEIGHT_UPDATE_RESPONSE,
                           BIO_MODULE_MIRROR_NEURONS, event->header.source_module,
                           sizeof(response));
        response.synapse_id = synapse_id;
        response.old_weight = syn->weight - delta_w;
        response.new_weight = syn->weight;
        response.clamped = (syn->weight == stdp->config.w_min ||
                           syn->weight == stdp->config.w_max);
        response.error = NIMCP_SUCCESS;

        nimcp_bio_promise_complete(response_promise, &response);
    }

    LOG_TRACE("handle_stdp_event: completed");
    return NIMCP_SUCCESS;
}

//=============================================================================
// KG-Driven Wiring Handler
//=============================================================================

/**
 * @brief KG-driven wiring handler callback
 *
 * WHAT: Register message handlers based on discovered wiring from KG
 * WHY:  Enables runtime assembly - module discovers its handlers from KG
 * HOW:  Orchestrator invokes this with message types from HANDLES_MESSAGE relations
 *
 * @param ctx Bio-async module context
 * @param message_types Array of message types to handle (from KG)
 * @param message_count Number of message types
 * @param user_data Module context pointer
 * @return 0 on success, -1 on error
 */
static int mirror_stdp_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    if (!ctx || !message_types || message_count == 0) {
        return 0;  /* No handlers to register */
    }

    (void)user_data;

    LOG_INFO(LOG_MODULE,
        "mirror_stdp_wiring_handler_callback: registering %u handlers from KG",
        message_count);

    for (uint32_t i = 0; i < message_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && message_count > 256) {
            mirror_stdp_heartbeat("mirror_stdp_loop",
                             (float)(i + 1) / (float)message_count);
        }

        switch (message_types[i]) {
            case BIO_MSG_MIRROR_NEURON_ACTIVATION:
                bio_router_register_handler(ctx, message_types[i], handle_mirror_neuron_activation);
                LOG_DEBUG(LOG_MODULE,
                    "  Registered handler for BIO_MSG_MIRROR_NEURON_ACTIVATION");
                break;

            case BIO_MSG_STDP_EVENT:
                bio_router_register_handler(ctx, message_types[i], handle_stdp_event);
                LOG_DEBUG(LOG_MODULE,
                    "  Registered handler for BIO_MSG_STDP_EVENT");
                break;

            default:
                LOG_DEBUG(LOG_MODULE,
                    "  Unknown message type 0x%04X, skipping", message_types[i]);
                break;
        }
    }

    return 0;
}

//=============================================================================
// Lifecycle Management
//=============================================================================

mirror_stdp_config_t mirror_stdp_get_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    mirror_stdp_heartbeat("mirror_stdp_get_default_config", 0.0f);


    mirror_stdp_config_t config = {
        // Timing windows
        .ltp_window_ms = NIMCP_STDP_LTP_WINDOW_MS,
        .ltd_window_ms = NIMCP_STDP_LTD_WINDOW_MS,

        // Amplitude parameters
        .A_plus = NIMCP_STDP_A_PLUS,
        .A_minus = NIMCP_STDP_A_MINUS,

        // Time constants
        .tau_plus = NIMCP_STDP_TAU_PLUS,
        .tau_minus = NIMCP_STDP_TAU_MINUS,

        // Weight bounds
        .w_max = NIMCP_STDP_W_MAX,
        .w_min = NIMCP_STDP_W_MIN,

        // Homeostatic plasticity
        .enable_homeostasis = true,
        .target_rate = NIMCP_STDP_TARGET_RATE,
        .tau_homeostasis = NIMCP_STDP_TAU_HOMEO,

        // Triplet STDP
        .enable_triplet = true,
        .tau_triplet = NIMCP_STDP_TAU_TRIPLET,
        .A_triplet = 0.1F,

        // Neuromodulator gating
        .enable_dopamine_gating = true,
        .enable_ach_gating = true,
        .dopamine_ltp_boost = 2.0F,
        .ach_attention_boost = 1.5F,

        // Metaplasticity
        .enable_metaplasticity = true,
        .meta_tau = 10000.0F,
        .meta_threshold = 0.5F
    };
    return config;
}

mirror_stdp_t mirror_stdp_create(const mirror_stdp_config_t* config, uint32_t max_synapses) {
    /* Phase 8: Heartbeat at operation start */
    mirror_stdp_heartbeat("mirror_stdp_create", 0.0f);


    LOG_TRACE("mirror_stdp_create: entered with max_synapses=%u", max_synapses);

    if (max_synapses == 0) {
        LOG_ERROR("mirror_stdp_create: max_synapses cannot be zero");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_stdp_create: max_synapses is zero");
        return NULL;
    }

    mirror_stdp_t stdp = (mirror_stdp_t)nimcp_calloc(1, sizeof(struct mirror_stdp_system));
    if (!stdp) {
        LOG_ERROR("mirror_stdp_create: failed to allocate STDP system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate stdp");

        return NULL;
    }

    // Copy configuration
    if (config) {
        stdp->config = *config;
    } else {
        stdp->config = mirror_stdp_get_default_config();
    }

    // Allocate synapse storage
    stdp->synapses = (mirror_stdp_synapse_t*)nimcp_calloc(max_synapses, sizeof(mirror_stdp_synapse_t));
    if (!stdp->synapses) {
        LOG_ERROR("mirror_stdp_create: failed to allocate synapse storage");
        nimcp_free(stdp);
        stdp = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_stdp_create: stdp->synapses is NULL");
        return NULL;
    }
    stdp->max_synapses = max_synapses;
    stdp->num_synapses = 0;

    // Allocate action map (2x size for reduced collisions)
    stdp->action_map_size = max_synapses * 2;
    stdp->action_map = (uint32_t*)nimcp_calloc(stdp->action_map_size, sizeof(uint32_t));
    if (!stdp->action_map) {
        LOG_ERROR("mirror_stdp_create: failed to allocate action map");
        nimcp_free(stdp->synapses);
        nimcp_free(stdp);
        stdp = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_stdp_create: stdp->action_map is NULL");
        return NULL;
    }
    // Initialize map to invalid
    for (uint32_t i = 0; i < stdp->action_map_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && stdp->action_map_size > 256) {
            mirror_stdp_heartbeat("mirror_stdp_loop",
                             (float)(i + 1) / (float)stdp->action_map_size);
        }

        stdp->action_map[i] = UINT32_MAX;
    }

    // Initialize neuromodulator levels to baseline
    stdp->dopamine_level = 0.5F;
    stdp->ach_level = 0.5F;

    // Initialize global scale factor
    stdp->global_scale_factor = 1.0F;

    // Register with bio-async router
    if (bio_router_is_initialized()) {
        LOG_DEBUG("mirror_stdp_create: registering with bio-async router");

        bio_module_info_t module_info = {
            .module_id = BIO_MODULE_MIRROR_NEURONS_STDP,
            .module_name = "mirror_stdp",
            .inbox_capacity = 128,
            .user_data = stdp
        };

        stdp->bio_ctx = bio_router_register_module(&module_info);
        if (!stdp->bio_ctx) {
            LOG_WARN("mirror_stdp_create: failed to register with bio-async router");
        } else {
            /* KG-Driven Wiring: Register callback for orchestrator to invoke
             * When orchestrator starts, it discovers HANDLES_MESSAGE relations
             * from the KG and invokes this callback with the message types */
            nimcp_error_t cb_result = bio_router_register_wiring_callback(
                BIO_MODULE_MIRROR_NEURONS_STDP,
                (void*)mirror_stdp_wiring_handler_callback,
                stdp
            );

            if (cb_result != NIMCP_SUCCESS) {
                /* Fallback: Direct registration if orchestrator not available
                 * This ensures backward compatibility with non-KG systems */
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(stdp->bio_ctx,
                        BIO_MSG_MIRROR_NEURON_ACTIVATION, handle_mirror_neuron_activation)
                );
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(stdp->bio_ctx,
                        BIO_MSG_STDP_EVENT, handle_stdp_event)
                );
                LOG_INFO("mirror_stdp_create: bio-async registered (legacy direct registration)");
            } else {
                LOG_INFO("mirror_stdp_create: bio-async registered (KG-driven wiring callback)");
            }
        }
    } else {
        LOG_DEBUG("mirror_stdp_create: bio-async router not initialized, skipping registration");
        stdp->bio_ctx = NULL;
    }

    LOG_INFO("mirror_stdp_create: created STDP system with %u max synapses", max_synapses);
    return stdp;
}

void mirror_stdp_destroy(mirror_stdp_t stdp) {
    if (!stdp) return;

    /* Phase 8: Heartbeat at operation start */
    mirror_stdp_heartbeat("mirror_stdp_destroy", 0.0f);


    LOG_TRACE("mirror_stdp_destroy: destroying STDP system");

    // Unregister from bio-async router
    if (stdp->bio_ctx) {
        LOG_DEBUG("mirror_stdp_destroy: unregistering from bio-async router");
        bio_router_unregister_module(stdp->bio_ctx);
        stdp->bio_ctx = NULL;
    }

    if (stdp->action_map) {
        nimcp_free(stdp->action_map);
    }
    if (stdp->synapses) {
        nimcp_free(stdp->synapses);
    }
    nimcp_free(stdp);
    stdp = NULL;

    LOG_TRACE("mirror_stdp_destroy: completed");
}

//=============================================================================
// Synapse Management
//=============================================================================

uint32_t mirror_stdp_create_synapse(mirror_stdp_t stdp, uint32_t action_id, float initial_weight) {
    if (!stdp || stdp->num_synapses >= stdp->max_synapses) {
        return UINT32_MAX;
    }

    // Find slot in action map
    /* Phase 8: Heartbeat at operation start */
    mirror_stdp_heartbeat("mirror_stdp_create_synapse", 0.0f);


    uint32_t hash = hash_action(action_id, stdp->action_map_size);
    uint32_t slot = hash;
    uint32_t attempts = 0;

    while (stdp->action_map[slot] != UINT32_MAX && attempts < stdp->action_map_size) {
        // Check if action already exists
        if (stdp->synapses[stdp->action_map[slot]].action_id == action_id) {
            return stdp->action_map[slot];  // Return existing synapse
        }
        slot = (slot + 1) % stdp->action_map_size;
        attempts++;
    }

    if (attempts >= stdp->action_map_size) {
        return UINT32_MAX;  // Map full
    }

    // Create new synapse
    uint32_t synapse_id = stdp->num_synapses++;
    mirror_stdp_synapse_t* syn = &stdp->synapses[synapse_id];

    // Initialize synapse
    syn->synapse_id = synapse_id;
    syn->action_id = action_id;
    syn->weight = nimcp_clampf(initial_weight, stdp->config.w_min, stdp->config.w_max);
    syn->initial_weight = syn->weight;

    // Initialize traces
    syn->r1 = 0.0F;
    syn->r2 = 0.0F;
    syn->o1 = 0.0F;
    syn->o2 = 0.0F;

    // Initialize spike history
    syn->obs_spike_count = 0;
    syn->exec_spike_count = 0;

    // Initialize rate tracking
    syn->avg_obs_rate = 0.0F;
    syn->avg_exec_rate = 0.0F;

    // Initialize metaplasticity
    syn->meta_state = 0.0F;
    syn->activity_history = 0.0F;

    // Initialize statistics
    syn->ltp_events = 0;
    syn->ltd_events = 0;
    syn->total_ltp = 0.0F;
    syn->total_ltd = 0.0F;
    syn->last_update_us = 0;

    // Add to action map
    stdp->action_map[slot] = synapse_id;

    return synapse_id;
}

bool mirror_stdp_get_synapse(mirror_stdp_t stdp, uint32_t synapse_id, mirror_stdp_synapse_t* out_synapse) {
    if (!stdp || !out_synapse || synapse_id >= stdp->num_synapses) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_stdp_get_synapse: required parameter is NULL (stdp, out_synapse)");
        return false;
    }

    *out_synapse = stdp->synapses[synapse_id];
    /* Phase 8: Heartbeat at operation start */
    mirror_stdp_heartbeat("mirror_stdp_get_synapse", 0.0f);


    return true;
}

float mirror_stdp_get_weight(mirror_stdp_t stdp, uint32_t synapse_id) {
    if (!stdp || synapse_id >= stdp->num_synapses) {
        return -1.0F;
    }
    /* Phase 8: Heartbeat at operation start */
    mirror_stdp_heartbeat("mirror_stdp_get_weight", 0.0f);


    return stdp->synapses[synapse_id].weight;
}

uint32_t mirror_stdp_find_synapse(mirror_stdp_t stdp, uint32_t action_id) {
    if (!stdp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_stdp_find_synapse: stdp is NULL");
        return UINT32_MAX;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_stdp_heartbeat("mirror_stdp_find_synapse", 0.0f);


    uint32_t hash = hash_action(action_id, stdp->action_map_size);
    uint32_t slot = hash;
    uint32_t attempts = 0;

    while (stdp->action_map[slot] != UINT32_MAX && attempts < stdp->action_map_size) {
        uint32_t syn_id = stdp->action_map[slot];
        if (stdp->synapses[syn_id].action_id == action_id) {
            return syn_id;
        }
        slot = (slot + 1) % stdp->action_map_size;
        attempts++;
    }

    return UINT32_MAX;
}

//=============================================================================
// Core STDP Computation
//=============================================================================

float mirror_stdp_compute_delta_w(mirror_stdp_t stdp, float delta_t_ms, float current_weight,
                                   float dopamine_level, float ach_level) {
    if (!stdp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_stdp_compute_delta_w: stdp is NULL");
        return 0.0F;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_stdp_heartbeat("mirror_stdp_compute_delta_w", 0.0f);


    const mirror_stdp_config_t* cfg = &stdp->config;
    float delta_w = 0.0F;

    // Base STDP computation
    if (delta_t_ms > 0.0F && delta_t_ms <= cfg->ltp_window_ms) {
        // LTP: Observation preceded execution (correct pairing for imitation)
        // Soft bounds: scale by distance from max
        float headroom = cfg->w_max - current_weight;
        delta_w = cfg->A_plus * exp_decay(delta_t_ms, cfg->tau_plus) * headroom;

        // Dopamine gating (reward enhances LTP)
        if (cfg->enable_dopamine_gating) {
            float da_factor = 1.0F + (dopamine_level - 0.5F) * cfg->dopamine_ltp_boost;
            delta_w *= nimcp_clampf(da_factor, 0.1F, 3.0F);
        }
    } else if (delta_t_ms < 0.0F && -delta_t_ms <= cfg->ltd_window_ms) {
        // LTD: Execution preceded observation (temporal confusion)
        // Soft bounds: scale by distance from min
        float footroom = current_weight - cfg->w_min;
        delta_w = -cfg->A_minus * exp_decay(-delta_t_ms, cfg->tau_minus) * footroom;
    }

    // ACh gating (attention enhances all plasticity)
    if (cfg->enable_ach_gating) {
        float ach_factor = 1.0F + (ach_level - 0.5F) * cfg->ach_attention_boost;
        delta_w *= nimcp_clampf(ach_factor, 0.1F, 2.0F);
    }

    return delta_w;
}

//=============================================================================
// Spike Processing
//=============================================================================

float mirror_stdp_observation_spike(mirror_stdp_t stdp, uint32_t synapse_id,
                                     uint64_t timestamp_us, float strength) {
    if (!stdp || synapse_id >= stdp->num_synapses) {
        return 0.0F;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_stdp_heartbeat("mirror_stdp_observation_spike", 0.0f);


    mirror_stdp_synapse_t* syn = &stdp->synapses[synapse_id];
    const mirror_stdp_config_t* cfg = &stdp->config;
    float total_delta_w = 0.0F;

    // Update current time
    stdp->current_time_us = timestamp_us;

    // Add observation spike to history
    add_spike_to_history(syn->obs_spikes, &syn->obs_spike_count,
                         timestamp_us, strength, true);

    // Update presynaptic traces
    float r1_before = syn->r1;
    syn->r1 += 1.0F;  // Increment fast trace
    if (cfg->enable_triplet) {
        syn->r2 += 1.0F;  // Increment slow trace
    }

    // Look for recent execution spikes for LTD
    // (If there was a recent execution, this observation comes AFTER -> LTD)
    const stdp_spike_t* recent_exec = find_recent_spike(
        syn->exec_spikes, syn->exec_spike_count,
        timestamp_us, cfg->ltd_window_ms
    );

    if (recent_exec) {
        // Δt = obs_time - exec_time (negative means exec was before obs -> LTD)
        float delta_t_ms = (float)(timestamp_us - recent_exec->timestamp_us) / 1000.0F;

        // Compute base weight change
        float delta_w = mirror_stdp_compute_delta_w(
            stdp, -delta_t_ms,  // Negative because we're looking backward
            syn->weight,
            stdp->dopamine_level,
            stdp->ach_level
        );

        // Apply triplet rule (modulate by postsynaptic trace)
        if (cfg->enable_triplet) {
            delta_w *= (1.0F + cfg->A_triplet * syn->o2);
        }

        // Apply metaplasticity
        if (cfg->enable_metaplasticity) {
            float meta_mod = 1.0F - syn->meta_state * cfg->meta_threshold;
            delta_w *= nimcp_clampf(meta_mod, 0.1F, 2.0F);
        }

        // Apply weight change
        syn->weight = nimcp_clampf(syn->weight + delta_w, cfg->w_min, cfg->w_max);
        total_delta_w += delta_w;

        // Update statistics
        if (delta_w < 0.0F) {
            syn->ltd_events++;
            syn->total_ltd += fabsf(delta_w);
            stdp->total_ltd_events++;
            stdp->sum_ltd_magnitude += fabsf(delta_w);
            stdp->sum_delta_t_ltd += fabsf(delta_t_ms);
        }
    }

    // Update rate tracking (exponential moving average)
    float rate_alpha = 0.01F;  // Smoothing factor
    syn->avg_obs_rate = syn->avg_obs_rate * (1.0F - rate_alpha) + strength * rate_alpha * 1000.0F;

    syn->last_update_us = timestamp_us;

    return total_delta_w;
}

float mirror_stdp_execution_spike(mirror_stdp_t stdp, uint32_t synapse_id,
                                   uint64_t timestamp_us, float strength) {
    if (!stdp || synapse_id >= stdp->num_synapses) {
        return 0.0F;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_stdp_heartbeat("mirror_stdp_execution_spike", 0.0f);


    mirror_stdp_synapse_t* syn = &stdp->synapses[synapse_id];
    const mirror_stdp_config_t* cfg = &stdp->config;
    float total_delta_w = 0.0F;

    // Update current time
    stdp->current_time_us = timestamp_us;

    // Add execution spike to history
    add_spike_to_history(syn->exec_spikes, &syn->exec_spike_count,
                         timestamp_us, strength, false);

    // Update postsynaptic traces
    float o1_before = syn->o1;
    syn->o1 += 1.0F;  // Increment fast trace
    if (cfg->enable_triplet) {
        syn->o2 += 1.0F;  // Increment slow trace
    }

    // Look for recent observation spikes for LTP
    // (If there was a recent observation, and now execution -> LTP)
    const stdp_spike_t* recent_obs = find_recent_spike(
        syn->obs_spikes, syn->obs_spike_count,
        timestamp_us, cfg->ltp_window_ms
    );

    if (recent_obs) {
        // Δt = obs_time - exec_time (positive means obs was before exec -> LTP)
        float delta_t_ms = (float)(recent_obs->timestamp_us - timestamp_us) / 1000.0F;
        // Actually we want: obs_time - exec_time, but exec is "now"
        // So it's: recent_obs->timestamp_us - timestamp_us which is negative
        // We need positive Δt for LTP, so:
        delta_t_ms = (float)(timestamp_us - recent_obs->timestamp_us) / 1000.0F;

        // Compute base weight change
        float delta_w = mirror_stdp_compute_delta_w(
            stdp, delta_t_ms,
            syn->weight,
            stdp->dopamine_level,
            stdp->ach_level
        );

        // Apply triplet rule (modulate by presynaptic trace)
        if (cfg->enable_triplet) {
            delta_w *= (1.0F + cfg->A_triplet * syn->r2);
        }

        // Apply metaplasticity
        if (cfg->enable_metaplasticity) {
            float meta_mod = 1.0F + syn->meta_state * cfg->meta_threshold;
            delta_w *= nimcp_clampf(meta_mod, 0.5F, 1.5F);
        }

        // Apply weight change
        syn->weight = nimcp_clampf(syn->weight + delta_w, cfg->w_min, cfg->w_max);
        total_delta_w += delta_w;

        // Update statistics
        if (delta_w > 0.0F) {
            syn->ltp_events++;
            syn->total_ltp += delta_w;
            stdp->total_ltp_events++;
            stdp->sum_ltp_magnitude += delta_w;
            stdp->sum_delta_t_ltp += delta_t_ms;
        }
    }

    // Update rate tracking
    float rate_alpha = 0.01F;
    syn->avg_exec_rate = syn->avg_exec_rate * (1.0F - rate_alpha) + strength * rate_alpha * 1000.0F;

    // Update activity history for metaplasticity
    if (cfg->enable_metaplasticity) {
        syn->activity_history = syn->activity_history * 0.999F + strength * 0.001F;
    }

    syn->last_update_us = timestamp_us;

    return total_delta_w;
}

//=============================================================================
// Trace and Eligibility
//=============================================================================

void mirror_stdp_update_traces(mirror_stdp_t stdp, float dt_ms) {
    if (!stdp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_stdp_update_traces: stdp is NULL");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_stdp_heartbeat("mirror_stdp_update_traces", 0.0f);


    const mirror_stdp_config_t* cfg = &stdp->config;

    // Compute decay factors
    float decay_fast = exp_decay(dt_ms, cfg->tau_plus);
    float decay_slow = cfg->enable_triplet ? exp_decay(dt_ms, cfg->tau_triplet) : 0.0F;

    // Update all synapse traces
    for (uint32_t i = 0; i < stdp->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && stdp->num_synapses > 256) {
            mirror_stdp_heartbeat("mirror_stdp_loop",
                             (float)(i + 1) / (float)stdp->num_synapses);
        }

        mirror_stdp_synapse_t* syn = &stdp->synapses[i];

        // Decay fast traces
        syn->r1 *= decay_fast;
        syn->o1 *= decay_fast;

        // Decay slow traces (triplet)
        if (cfg->enable_triplet) {
            syn->r2 *= decay_slow;
            syn->o2 *= decay_slow;
        }
    }
}

float mirror_stdp_get_obs_trace(mirror_stdp_t stdp, uint32_t synapse_id) {
    if (!stdp || synapse_id >= stdp->num_synapses) {
        return 0.0F;
    }
    /* Phase 8: Heartbeat at operation start */
    mirror_stdp_heartbeat("mirror_stdp_get_obs_trace", 0.0f);


    return stdp->synapses[synapse_id].r1;
}

float mirror_stdp_get_exec_trace(mirror_stdp_t stdp, uint32_t synapse_id) {
    if (!stdp || synapse_id >= stdp->num_synapses) {
        return 0.0F;
    }
    /* Phase 8: Heartbeat at operation start */
    mirror_stdp_heartbeat("mirror_stdp_get_exec_trace", 0.0f);


    return stdp->synapses[synapse_id].o1;
}

//=============================================================================
// Homeostatic Plasticity
//=============================================================================

void mirror_stdp_apply_homeostasis(mirror_stdp_t stdp, float dt_ms) {
    if (!stdp || !stdp->config.enable_homeostasis) {
        if (!stdp) NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_stdp_apply_homeostasis: stdp is NULL");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_stdp_heartbeat("mirror_stdp_apply_homeostasis", 0.0f);


    const mirror_stdp_config_t* cfg = &stdp->config;

    // Compute global average rate
    float total_rate = 0.0F;
    for (uint32_t i = 0; i < stdp->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && stdp->num_synapses > 256) {
            mirror_stdp_heartbeat("mirror_stdp_loop",
                             (float)(i + 1) / (float)stdp->num_synapses);
        }

        total_rate += stdp->synapses[i].avg_exec_rate;
    }
    float avg_rate = (stdp->num_synapses > 0) ? total_rate / stdp->num_synapses : 0.0F;

    // Compute rate deviation
    float rate_error = cfg->target_rate - avg_rate;

    // Compute scaling factor (multiplicative homeostasis)
    // Small time constant means slow adaptation
    float adapt_rate = dt_ms / cfg->tau_homeostasis;
    float scale_change = rate_error * adapt_rate * 0.01F;

    stdp->global_scale_factor = nimcp_clampf(stdp->global_scale_factor + scale_change, 0.5F, 2.0F);

    // Apply scaling to all synapses
    for (uint32_t i = 0; i < stdp->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && stdp->num_synapses > 256) {
            mirror_stdp_heartbeat("mirror_stdp_loop",
                             (float)(i + 1) / (float)stdp->num_synapses);
        }

        mirror_stdp_synapse_t* syn = &stdp->synapses[i];

        // Multiplicative scaling (preserves relative weights)
        float new_weight = syn->weight * stdp->global_scale_factor;

        if (fabsf(new_weight - syn->weight) > 1e-6F) {
            syn->weight = nimcp_clampf(new_weight, cfg->w_min, cfg->w_max);
            stdp->homeostatic_adjustments++;
        }
    }
}

void mirror_stdp_update_metaplasticity(mirror_stdp_t stdp, float dt_ms) {
    if (!stdp || !stdp->config.enable_metaplasticity) {
        if (!stdp) NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_stdp_update_metaplasticity: stdp is NULL");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_stdp_heartbeat("mirror_stdp_update_metaplasticit", 0.0f);


    const mirror_stdp_config_t* cfg = &stdp->config;
    float decay = exp_decay(dt_ms, cfg->meta_tau);

    for (uint32_t i = 0; i < stdp->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && stdp->num_synapses > 256) {
            mirror_stdp_heartbeat("mirror_stdp_loop",
                             (float)(i + 1) / (float)stdp->num_synapses);
        }

        mirror_stdp_synapse_t* syn = &stdp->synapses[i];

        // BCM-like sliding threshold
        // High activity -> raise threshold -> harder to induce LTP
        float target_activity = cfg->meta_threshold;
        float activity_error = syn->activity_history - target_activity;

        // Update metaplastic state
        syn->meta_state = syn->meta_state * decay + activity_error * (1.0F - decay);
        syn->meta_state = nimcp_clampf(syn->meta_state, -1.0F, 1.0F);
    }
}

//=============================================================================
// Neuromodulator Integration
//=============================================================================

void mirror_stdp_set_dopamine(mirror_stdp_t stdp, float level) {
    if (!stdp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_stdp_set_dopamine: stdp is NULL");
        return;
    }
    /* Phase 8: Heartbeat at operation start */
    mirror_stdp_heartbeat("mirror_stdp_set_dopamine", 0.0f);


    stdp->dopamine_level = nimcp_clampf(level, 0.0F, 1.0F);
}

void mirror_stdp_set_acetylcholine(mirror_stdp_t stdp, float level) {
    if (!stdp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_stdp_set_acetylcholine: stdp is NULL");
        return;
    }
    /* Phase 8: Heartbeat at operation start */
    mirror_stdp_heartbeat("mirror_stdp_set_acetylcholine", 0.0f);


    stdp->ach_level = nimcp_clampf(level, 0.0F, 1.0F);
}

//=============================================================================
// Statistics and Analysis
//=============================================================================

bool mirror_stdp_get_stats(mirror_stdp_t stdp, mirror_stdp_stats_t* stats) {
    if (!stdp || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_stdp_get_stats: required parameter is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_stdp_heartbeat("mirror_stdp_get_stats", 0.0f);


    memset(stats, 0, sizeof(mirror_stdp_stats_t));

    stats->num_synapses = stdp->num_synapses;

    // Count active synapses and compute weight statistics
    float sum_weight = 0.0F;
    float sum_weight_sq = 0.0F;
    stats->min_weight = stdp->config.w_max;
    stats->max_weight = stdp->config.w_min;
    float sum_obs_rate = 0.0F;
    float sum_exec_rate = 0.0F;

    uint64_t activity_threshold_us = stdp->current_time_us - 1000000;  // 1 second

    for (uint32_t i = 0; i < stdp->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && stdp->num_synapses > 256) {
            mirror_stdp_heartbeat("mirror_stdp_loop",
                             (float)(i + 1) / (float)stdp->num_synapses);
        }

        const mirror_stdp_synapse_t* syn = &stdp->synapses[i];

        // Check if active
        if (syn->last_update_us > activity_threshold_us) {
            stats->active_synapses++;
        }

        // Weight statistics
        sum_weight += syn->weight;
        sum_weight_sq += syn->weight * syn->weight;

        if (syn->weight < stats->min_weight) stats->min_weight = syn->weight;
        if (syn->weight > stats->max_weight) stats->max_weight = syn->weight;

        // Rate statistics
        sum_obs_rate += syn->avg_obs_rate;
        sum_exec_rate += syn->avg_exec_rate;
    }

    // Compute mean and variance
    if (stdp->num_synapses > 0) {
        stats->mean_weight = sum_weight / stdp->num_synapses;
        float mean_sq = sum_weight_sq / stdp->num_synapses;
        stats->weight_variance = mean_sq - stats->mean_weight * stats->mean_weight;
        if (stats->weight_variance < 0.0F) stats->weight_variance = 0.0F;

        stats->mean_obs_rate = sum_obs_rate / stdp->num_synapses;
        stats->mean_exec_rate = sum_exec_rate / stdp->num_synapses;
    }

    // Learning event statistics
    stats->total_ltp_events = stdp->total_ltp_events;
    stats->total_ltd_events = stdp->total_ltd_events;

    if (stdp->total_ltp_events > 0) {
        stats->avg_ltp_magnitude = stdp->sum_ltp_magnitude / stdp->total_ltp_events;
        stats->avg_delta_t_ltp = stdp->sum_delta_t_ltp / stdp->total_ltp_events;
    }

    if (stdp->total_ltd_events > 0) {
        stats->avg_ltd_magnitude = stdp->sum_ltd_magnitude / stdp->total_ltd_events;
        stats->avg_delta_t_ltd = stdp->sum_delta_t_ltd / stdp->total_ltd_events;
    }

    // Homeostasis statistics
    stats->homeostatic_adjustments = stdp->homeostatic_adjustments;
    stats->homeostatic_scale_factor = stdp->global_scale_factor;

    return true;
}

bool mirror_stdp_get_weight_histogram(mirror_stdp_t stdp, uint32_t* bins, uint32_t num_bins) {
    if (!stdp || !bins || num_bins == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_stdp_get_weight_histogram: required parameter is NULL");
        return false;
    }

    // Clear bins
    /* Phase 8: Heartbeat at operation start */
    mirror_stdp_heartbeat("mirror_stdp_get_weight_histogram", 0.0f);


    memset(bins, 0, num_bins * sizeof(uint32_t));

    float bin_width = (stdp->config.w_max - stdp->config.w_min) / num_bins;

    for (uint32_t i = 0; i < stdp->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && stdp->num_synapses > 256) {
            mirror_stdp_heartbeat("mirror_stdp_loop",
                             (float)(i + 1) / (float)stdp->num_synapses);
        }

        float weight = stdp->synapses[i].weight;
        uint32_t bin = (uint32_t)((weight - stdp->config.w_min) / bin_width);

        // Clamp to valid range
        if (bin >= num_bins) bin = num_bins - 1;

        bins[bin]++;
    }

    return true;
}

void mirror_stdp_step(mirror_stdp_t stdp, float dt_ms) {
    // Process pending bio-async messages
    /* Phase 8: Heartbeat at operation start */
    mirror_stdp_heartbeat("mirror_stdp_step", 0.0f);


    if (stdp && stdp->bio_ctx) {
        bio_router_process_inbox(stdp->bio_ctx, 5);
    }

    if (!stdp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_stdp_step: stdp is NULL");
        return;
    }

    // Update eligibility traces
    mirror_stdp_update_traces(stdp, dt_ms);

    // Apply homeostatic scaling (periodically, not every step)
    stdp->last_homeostasis_us += (uint64_t)(dt_ms * 1000);
    if (stdp->last_homeostasis_us >= 100000) {  // Every 100ms
        mirror_stdp_apply_homeostasis(stdp, 100.0F);
        stdp->last_homeostasis_us = 0;
    }

    // Update metaplasticity (periodically)
    stdp->last_metaplasticity_us += (uint64_t)(dt_ms * 1000);
    if (stdp->last_metaplasticity_us >= 1000000) {  // Every 1s
        mirror_stdp_update_metaplasticity(stdp, 1000.0F);
        stdp->last_metaplasticity_us = 0;
    }
}

void mirror_stdp_reset_stats(mirror_stdp_t stdp) {
    if (!stdp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_stdp_reset_stats: stdp is NULL");
        return;
    }

    // Reset global statistics
    /* Phase 8: Heartbeat at operation start */
    mirror_stdp_heartbeat("mirror_stdp_reset_stats", 0.0f);


    stdp->total_ltp_events = 0;
    stdp->total_ltd_events = 0;
    stdp->sum_ltp_magnitude = 0.0F;
    stdp->sum_ltd_magnitude = 0.0F;
    stdp->sum_delta_t_ltp = 0.0F;
    stdp->sum_delta_t_ltd = 0.0F;
    stdp->homeostatic_adjustments = 0;

    // Reset per-synapse statistics
    for (uint32_t i = 0; i < stdp->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && stdp->num_synapses > 256) {
            mirror_stdp_heartbeat("mirror_stdp_loop",
                             (float)(i + 1) / (float)stdp->num_synapses);
        }

        mirror_stdp_synapse_t* syn = &stdp->synapses[i];
        syn->ltp_events = 0;
        syn->ltd_events = 0;
        syn->total_ltp = 0.0F;
        syn->total_ltd = 0.0F;
    }
}

//=============================================================================
// KG Self-Awareness Integration
//=============================================================================

int mirror_stdp_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    mirror_stdp_heartbeat("mirror_stdp_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Mirror_STDP");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                mirror_stdp_heartbeat("mirror_stdp_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            LOG_DEBUG("Mirror STDP self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Mirror_STDP");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Mirror_STDP");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void mirror_stdp_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_mirror_stdp_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training stubs
 * ============================================================================ */
int mirror_stdp_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_stdp_training_begin: NULL argument");
        return -1;
    }
    mirror_stdp_heartbeat_instance(NULL, "mirror_stdp_training_begin", 0.0f);
    return 0;
}

int mirror_stdp_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_stdp_training_end: NULL argument");
        return -1;
    }
    mirror_stdp_heartbeat_instance(NULL, "mirror_stdp_training_end", 1.0f);
    return 0;
}

int mirror_stdp_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_stdp_training_step: NULL argument");
        return -1;
    }
    mirror_stdp_heartbeat_instance(NULL, "mirror_stdp_training_step", progress);
    return 0;
}
