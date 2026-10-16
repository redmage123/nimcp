/**
 * @file nimcp_lnn_network.c
 * @brief LNN Network Implementation
 *
 * WHAT: Multi-layer Liquid Neural Network implementation
 * WHY:  Provides continuous-time temporal processing for NIMCP
 * HOW:  Sequential layer propagation, state history management, training integration
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 */

#include "lnn/nimcp_lnn_network.h"
#include "lnn/nimcp_lnn_layer.h"
#include "lnn/nimcp_lnn_config.h"
#include "lnn/nimcp_lnn_training.h"        /* lnn_tune_get_substrate_* / _thalamic_* */
#include "lnn/nimcp_lnn_hamiltonian.h"     /* hnn_tune_get_dissipation_* */
#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_atomic.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/lnn/nimcp_lnn_gpu.h"
/* W5 KG-integration: brain_internal MUST be included before thalamic_channel.h
 * to avoid a cognitive/parietal/nimcp_parietal.h vs nimcp_thalamic_router.h
 * double-typedef of thalamic_router_t. */
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"
#include "core/substrate/nimcp_substrate_effects.h"  /* substrate_compute_effects, debit */
#include "core/thalamic/nimcp_thalamic_channel.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

/*=============================================================================
 * Health Agent Forward Declarations (Phase 8: Heartbeat for Long Operations)
 *============================================================================*/
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(lnn_network)

#include <stddef.h>  /* for NULL */
// Thread-safe network ID counter
static nimcp_atomic_uint32_t g_lnn_network_id_counter = {0};

/*=============================================================================
 * W5 KG-integration — network-level aggregator event emitter (2026-04-24)
 *
 * File-scope backpointer to the owning brain, installed once at brain init
 * via net_lnn_kg_register_brain(). The structural root node "net_lnn" is
 * created by nimcp_brain_init_network_kg_wiring.c (Wave 32). Emits
 * state-transition events (gradient_explosion, mode_transition, phase_shift)
 * only on anomaly — never every tick, to avoid KG firehose.
 *
 * Admin-token self-elevation pattern per kg-node-naming-registry.md §7.
 * (brain_internal.h + brain_kg.h are included above.)
 *===========================================================================*/

static brain_t s_net_lnn_kg_brain = NULL;

/* Public setter (called from nimcp_brain_init_network_kg_wiring.c). */
void net_lnn_kg_register_brain(brain_t brain) {
    s_net_lnn_kg_brain = brain;
}

static void net_lnn_kg_emit_event(brain_t brain, const char* kind,
                                  float magnitude, uint64_t ts_us) {
    if (!brain || !kind) return;
    if (!brain->internal_kg_enabled || !brain->internal_kg) return;

    brain_kg_t* kg = brain->internal_kg;
    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN,
                              brain->internal_kg_admin_token);

    char ev_name[160];
    snprintf(ev_name, sizeof(ev_name),
             "net_lnn_event_%s_%llu", kind, (unsigned long long)ts_us);
    char desc[240];
    snprintf(desc, sizeof(desc),
             "LNN network state transition: kind=%s magnitude=%.4f",
             kind, magnitude);

    brain_kg_node_id_t ev = brain_kg_add_node(kg, ev_name,
        BRAIN_KG_NODE_CORE, desc);
    brain_kg_node_id_t owner = brain_kg_find_node(kg, "net_lnn");
    if (owner != BRAIN_KG_INVALID_NODE && ev != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, owner, ev, BRAIN_KG_EDGE_SENDS_TO,
            "produced_by", magnitude);
    }
    if (ev != BRAIN_KG_INVALID_NODE) {
        char buf[40];
        snprintf(buf, sizeof(buf), "%.6f", magnitude);
        brain_kg_add_metadata(kg, ev, "magnitude", buf);
        snprintf(buf, sizeof(buf), "%llu", (unsigned long long)ts_us);
        brain_kg_add_metadata(kg, ev, "ts_us", buf);
    }

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);
}

/* Public test-facing trigger — allows unit tests to verify emit behavior
 * without having to induce a real gradient explosion. Pure forwarding. */
void net_lnn_kg_trigger_event(brain_t brain, const char* kind,
                              float magnitude, uint64_t ts_us) {
    net_lnn_kg_emit_event(brain, kind, magnitude, ts_us);
}

/*=============================================================================
 * Network Lifecycle
 *===========================================================================*/

/**
 * @brief Create LNN network from configuration
 *
 * WHAT: Allocate and initialize multi-layer LNN network
 * WHY:  Primary network creation method
 * HOW:  Validate config, create layers, allocate history buffer, initialize mutex
 */
lnn_network_t* lnn_network_create(const lnn_config_t* config) {
    // Guard: Validate inputs
    if (!config) {
        NIMCP_LOGGING_ERROR("lnn_network_create: NULL config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lnn_network_create: config is NULL");
        return NULL;
    }

    if (lnn_config_validate(config) != LNN_SUCCESS) {
        NIMCP_LOGGING_ERROR("lnn_network_create: Invalid config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "lnn_network_create: validation failed");
        return NULL;
    }

    // Allocate network structure
    lnn_network_t* network = (lnn_network_t*)nimcp_calloc(1, sizeof(lnn_network_t));
    if (!network) {
        NIMCP_LOGGING_ERROR("lnn_network_create: Failed to allocate network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_network_create: failed to allocate network");
        return NULL;
    }

    // Initialize basic fields with thread-safe ID generation
    network->id = nimcp_atomic_fetch_add_u32(&g_lnn_network_id_counter, 1, NIMCP_MEMORY_ORDER_SEQ_CST);
    snprintf(network->name, sizeof(network->name), "lnn_network_%u", network->id);
    network->n_layers = config->n_layers;
    network->n_inputs = config->n_inputs;
    network->n_outputs = config->n_outputs;
    network->train_mode = config->train_mode;
    network->is_training = false;

    // Deep copy configuration (shallow copy would leave shared pointers)
    network->config = (lnn_config_t*)nimcp_calloc(1, sizeof(lnn_config_t));
    if (!network->config) {
        NIMCP_LOGGING_ERROR("lnn_network_create: Failed to allocate config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_network_create: failed to allocate config");
        nimcp_free(network);
        return NULL;
    }
    memcpy(network->config, config, sizeof(lnn_config_t));

    // Deep copy layer_configs if present
    if (config->layer_configs && config->n_layers > 0) {
        network->config->layer_configs = (lnn_layer_config_t*)nimcp_calloc(
            config->n_layers, sizeof(lnn_layer_config_t));
        if (!network->config->layer_configs) {
            NIMCP_LOGGING_ERROR("lnn_network_create: Failed to allocate layer configs");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_network_create: failed to allocate layer configs");
            nimcp_free(network->config);
            nimcp_free(network);
            return NULL;
        }
        memcpy(network->config->layer_configs, config->layer_configs,
               config->n_layers * sizeof(lnn_layer_config_t));
    }

    // Allocate layer array
    network->layers = (lnn_layer_t**)nimcp_calloc(network->n_layers, sizeof(lnn_layer_t*));
    if (!network->layers) {
        NIMCP_LOGGING_ERROR("lnn_network_create: Failed to allocate layer array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_network_create: failed to allocate layer array");
        lnn_config_destroy(network->config);
        nimcp_free(network->config);
        nimcp_free(network);
        return NULL;
    }

    // Create layers
    for (uint32_t i = 0; i < network->n_layers; i++) {
        uint32_t layer_input_size = (i == 0) ? config->n_inputs :
                                     config->layer_configs[i-1].n_neurons;

        network->layers[i] = lnn_layer_create(&config->layer_configs[i], layer_input_size);
        if (!network->layers[i]) {
            NIMCP_LOGGING_ERROR("lnn_network_create: Failed to create layer %u", i);
            // Cleanup previously created layers
            for (uint32_t j = 0; j < i; j++) {
                lnn_layer_destroy(network->layers[j]);
            }
            nimcp_free(network->layers);
            lnn_config_destroy(network->config);
            nimcp_free(network->config);
            nimcp_free(network);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_network_create: failed to create layer");
            return NULL;
        }
    }

    // Allocate state history if preallocate enabled
    if (config->preallocate_history && config->bptt_truncation > 0) {
        network->history_capacity = config->bptt_truncation;
        network->state_history = (nimcp_tensor_t**)nimcp_calloc(
            network->history_capacity, sizeof(nimcp_tensor_t*)
        );
        if (!network->state_history) {
            NIMCP_LOGGING_WARN("lnn_network_create: Failed to preallocate history");
            network->history_capacity = 0;
        }
    }

    // Create mutex
    network->mutex = nimcp_platform_mutex_create();
    if (!network->mutex) {
        NIMCP_LOGGING_WARN("lnn_network_create: Failed to create mutex");
    }

    // Initialize GPU LNN layers if GPU is available
    network->gpu_ctx = NULL;
    {
        nimcp_gpu_context_t* gpu = nimcp_gpu_context_create_auto();
        if (gpu) {
            network->gpu_ctx = gpu;
            for (uint32_t i = 0; i < network->n_layers; i++) {
                if (network->layers[i]) {
                    network->layers[i]->gpu_lnn_layer =
                        nimcp_lnn_layer_gpu_create(gpu, network->layers[i]);
                    if (network->layers[i]->gpu_lnn_layer) {
                        network->layers[i]->gpu_ctx = gpu;
                        NIMCP_LOGGING_INFO("GPU LNN layer %u created for network '%s'",
                                           i, network->name);
                    } else {
                        NIMCP_LOGGING_WARN("Failed to create GPU LNN layer %u, using CPU fallback",
                                           i);
                    }
                }
            }
        } else {
            NIMCP_LOGGING_INFO("No GPU available for LNN network '%s', using CPU path",
                               network->name);
        }
    }

    // Initialize statistics
    memset(&network->stats, 0, sizeof(lnn_network_stats_t));
    network->stats.health = LNN_STATE_VALID;

    NIMCP_LOGGING_INFO("Created LNN network '%s' with %u layers (%u inputs, %u outputs)",
                       network->name, network->n_layers, network->n_inputs, network->n_outputs);

    return network;
}

/**
 * @brief Create NCP (Neural Circuit Policy) network
 *
 * WHAT: Convenience constructor for NCP architecture
 * WHY:  NCP is standard LNN architecture from original paper
 * HOW:  Create config with lnn_config_ncp, then call lnn_network_create
 */
lnn_network_t* lnn_network_create_ncp(
    uint32_t n_inputs,
    uint32_t n_inter,
    uint32_t n_command,
    uint32_t n_outputs
) {
    // Guard: Validate inputs
    if (n_inputs == 0 || n_inter == 0 || n_command == 0 || n_outputs == 0) {
        NIMCP_LOGGING_ERROR("lnn_network_create_ncp: Invalid dimensions");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "lnn_network_create_ncp: n_inputs is zero");
        return NULL;
    }

    // Create NCP configuration
    lnn_config_t config;
    if (lnn_config_ncp(&config, n_inputs, n_inter, n_command, n_outputs) != LNN_SUCCESS) {
        NIMCP_LOGGING_ERROR("lnn_network_create_ncp: Failed to create NCP config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_network_create_ncp: validation failed");
        return NULL;
    }

    // Create network from config
    lnn_network_t* network = lnn_network_create(&config);

    // Cleanup config (network owns a copy)
    lnn_config_destroy(&config);

    return network;
}

/**
 * @brief Destroy network and free all resources
 *
 * WHAT: Clean shutdown of network
 * WHY:  Prevent memory leaks
 * HOW:  Free layers, history, config, mutex, network struct
 */
void lnn_network_destroy(lnn_network_t* network) {
    // Guard: NULL-safe
    if (!network) {
        return;
    }

    /* Destroy the thalamic channel (owned). Router is borrowed — do
     * NOT destroy it here. NULL-safe by channel contract. */
    if (network->thalamic_channel) {
        thalamic_channel_destroy(network->thalamic_channel);
        network->thalamic_channel = NULL;
    }

    // Destroy GPU LNN layers before CPU layers
    if (network->layers) {
        for (uint32_t i = 0; i < network->n_layers; i++) {
            if (network->layers[i] && network->layers[i]->gpu_lnn_layer) {
                nimcp_lnn_layer_gpu_destroy(
                    (nimcp_lnn_layer_gpu_t*)network->layers[i]->gpu_lnn_layer);
                network->layers[i]->gpu_lnn_layer = NULL;
            }
        }
    }

    // Destroy GPU context
    if (network->gpu_ctx) {
        nimcp_gpu_context_destroy((nimcp_gpu_context_t*)network->gpu_ctx);
        network->gpu_ctx = NULL;
    }

    // Destroy layers
    if (network->layers) {
        for (uint32_t i = 0; i < network->n_layers; i++) {
            lnn_layer_destroy(network->layers[i]);
            network->layers[i] = NULL;  // Prevent use-after-free
        }
        nimcp_free(network->layers);
        network->layers = NULL;  // Prevent use-after-free
    }

    // Clear and free state history
    lnn_network_clear_history(network);
    if (network->state_history) {
        nimcp_free(network->state_history);
        network->state_history = NULL;  // Prevent use-after-free
    }

    // Destroy config
    if (network->config) {
        lnn_config_destroy(network->config);
        nimcp_free(network->config);
        network->config = NULL;  // Prevent use-after-free
    }

    // Destroy mutex
    if (network->mutex) {
        nimcp_platform_mutex_destroy((nimcp_platform_mutex_t*)network->mutex);
        nimcp_free(network->mutex);
        network->mutex = NULL;
        network->mutex = NULL;  // Prevent use-after-free
    }

    // Free network structure
    nimcp_free(network);
}

/*=============================================================================
 * Thalamic attention adapter (Phase 2 of multi-network thalamic rollout)
 *===========================================================================*/

/**
 * @brief Attach (or detach) a thalamic router
 *
 * WHAT: Create/destroy an internal thalamic_channel_t bound to this LNN.
 * WHY:  Lets the LNN participate in attention-gated cross-network routing.
 * HOW:  Destroy any existing channel, optionally build a new one with
 *       source_id=net->id and destination 0 pre-registered.
 */
nimcp_error_t lnn_network_attach_thalamic_router(lnn_network_t* net,
                                                 struct thalamic_router* router)
{
    if (!net) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Single-attach: always destroy the existing channel first so a
     * re-attach (same or different router) cannot leak. Safe on NULL. */
    if (net->thalamic_channel) {
        thalamic_channel_destroy(net->thalamic_channel);
        net->thalamic_channel = NULL;
    }

    /* Detach path — caller passed router=NULL. */
    if (!router) {
        return NIMCP_SUCCESS;
    }

    /* Build a fresh channel bound to this network's id. */
    thalamic_channel_t* ch = thalamic_channel_create((thalamic_router_t*)router,
                                                     net->id);
    if (!ch) {
        NIMCP_LOGGING_ERROR("lnn_network_attach_thalamic_router: "
                            "channel allocation failed");
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Pre-register destination 0 (the downstream decoder) so get_gate
     * and submit have a valid slot from the first forward step. Failure
     * here is unexpected but should not leak the channel. */
    nimcp_error_t rc = thalamic_channel_add_destination(ch, 0);
    if (rc != NIMCP_SUCCESS) {
        thalamic_channel_destroy(ch);
        NIMCP_LOGGING_ERROR("lnn_network_attach_thalamic_router: "
                            "add_destination(0) failed (%d)", (int)rc);
        return rc;
    }

    net->thalamic_channel = ch;
    return NIMCP_SUCCESS;
}

/**
 * @brief Attach a neural substrate to the LNN network (borrowed pointer).
 *
 * WHAT: Sets network->substrate and zeroes the cached effect structs +
 *       update counter, so the next forward step recomputes fresh.
 * WHY:  LNN substrate adapter needs a handle to read ATP/temperature/ion/
 *       membrane state each tick. The adapter owns nothing — the substrate
 *       lifecycle is the caller's responsibility.
 * HOW:  NULL-tolerant. Passing sub=NULL detaches cleanly.
 */
void lnn_network_attach_substrate(lnn_network_t* net,
                                  struct neural_substrate* sub) {
    if (!net) return;  /* null-tolerant: silent drop */
    net->substrate = sub;
    /* Reset cache + counter so the next step recomputes fresh effects. */
    memset(&net->cached_axon_effects, 0, sizeof(net->cached_axon_effects));
    memset(&net->cached_dend_effects, 0, sizeof(net->cached_dend_effects));
    net->substrate_steps_since_update = 0;
    /* F1 CRITICAL mirror: populate cache IMMEDIATELY so downstream consumers
     * that read cached_dend_effects.plasticity_mod before the first forward
     * step don't multiply LR by 0. Without this, learning silently dies on
     * any pre-step inference or training hop that skips the refresh block. */
    if (sub) {
        substrate_compute_effects(sub,
                                  &net->cached_axon_effects,
                                  &net->cached_dend_effects);
        NIMCP_LOGGING_DEBUG("lnn_network_attach_substrate: eager-populated substrate cache after attach");
    }
    NIMCP_LOGGING_DEBUG("lnn_network_attach_substrate: substrate=%p",
                        (void*)sub);
}

/* Accessors for wire-up regression tests. NULL-tolerant.
 * Returns the currently-attached substrate / thalamic_channel pointers. */
struct neural_substrate* lnn_network_get_substrate(lnn_network_t* net) {
    return net ? net->substrate : NULL;
}

void* lnn_network_get_thalamic_channel(lnn_network_t* net) {
    return net ? (void*)net->thalamic_channel : NULL;
}

/**
 * @brief Initialize network weights
 *
 * WHAT: Initialize all layer weights with random values
 * WHY:  Proper initialization critical for training convergence
 * HOW:  Call lnn_layer_init_weights on each layer
 */
int lnn_network_init_weights(lnn_network_t* network, uint64_t seed) {
    // Guard: Validate inputs
    if (!network) {
        NIMCP_LOGGING_ERROR("lnn_network_init_weights: NULL network");
        return LNN_ERROR_NULL_POINTER;
    }

    // Use current time if seed is 0
    if (seed == 0) {
        seed = (uint64_t)time(NULL);
    }

    // Initialize each layer with unique seed
    for (uint32_t i = 0; i < network->n_layers; i++) {
        float std = network->config->layer_configs[i].weight_init_std;
        uint64_t layer_seed = seed + i;

        int result = lnn_layer_init_weights(network->layers[i], std, layer_seed);
        if (result != LNN_SUCCESS) {
            NIMCP_LOGGING_ERROR("lnn_network_init_weights: Failed to init layer %u", i);
            return result;
        }
    }

    NIMCP_LOGGING_INFO("Initialized weights for network '%s' with seed %lu",
                       network->name, seed);
    return LNN_SUCCESS;
}

/*=============================================================================
 * Forward Pass
 *===========================================================================*/

/**
 * @brief Single forward step with given input
 *
 * WHAT: Propagate input through all layers for one time step
 * WHY:  Core inference operation
 * HOW:  Sequential layer forward, record state if training
 */
int lnn_network_forward_step(
    lnn_network_t* network,
    const nimcp_tensor_t* input,
    nimcp_tensor_t* output,
    float dt
) {
    // Guard: Validate inputs
    if (!network || !input || !output) {
        NIMCP_LOGGING_ERROR("lnn_network_forward_step: NULL argument");
        return LNN_ERROR_NULL_POINTER;
    }

    /* Phase 8: Send heartbeat at start of forward step */
    lnn_network_heartbeat("lnn_forward_step", 0.0f);

    // Use default dt if not specified
    if (dt <= 0.0f) {
        dt = network->config->default_dt;
    }

    // Lock for thread safety
    if (network->mutex) {
        nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)network->mutex);
    }

    /* Substrate adapter: refresh cached effect structs at most once per
     * forward step (not per layer). */
    const dendrite_substrate_effects_t* dend_effects_ptr = NULL;
    const axon_substrate_effects_t*     axon_effects_ptr = NULL;
    if (network->substrate && lnn_tune_get_substrate_enabled() != 0.0f) {
        const uint32_t period = (uint32_t)lnn_tune_get_substrate_update_period();
        if (network->substrate_steps_since_update == 0 || period == 0) {
            substrate_compute_effects(network->substrate,
                                      &network->cached_axon_effects,
                                      &network->cached_dend_effects);
        }
        network->substrate_steps_since_update++;
        if (period > 0 && network->substrate_steps_since_update >= period) {
            network->substrate_steps_since_update = 0;
        }
        if (lnn_tune_get_substrate_tau_compose_on() != 0.0f) {
            dend_effects_ptr = &network->cached_dend_effects;
        }
        /* HNN Rayleigh dissipation consumes axon effects (overall_capacity
         * ↔ ATP/ion state). Only publish the pointer when HNN dissipation
         * is enabled so non-HNN paths aren't paying for an unused read. */
        if (hnn_tune_get_dissipation_enabled() != 0.0f) {
            axon_effects_ptr = &network->cached_axon_effects;
        }
    }

    /* Thalamic attention gate — scales input before the ODE integration.
     * Burst mode additionally clamps per-layer tau to speed transient
     * response. gated_input is cloned only when gate != 1.0 && gain_on. */
    float thal_gate = 1.0f;
    bool  thal_channel_active = false;
    bool  thal_burst_clamp    = false;
    if (network->thalamic_channel && lnn_tune_get_thalamic_enabled() != 0.0f) {
        thal_channel_active = true;
        thal_gate = thalamic_channel_get_gate(network->thalamic_channel, 0);
        if (lnn_tune_get_thalamic_burst_tau_clamp_on() != 0.0f &&
            network->thalamic_channel->mode == THALAMIC_CHAN_BURST) {
            thal_burst_clamp = true;
        }
    }

    nimcp_tensor_t* gated_input = NULL;
    if (thal_channel_active &&
        lnn_tune_get_thalamic_input_gain_on() != 0.0f &&
        thal_gate != 1.0f) {
        gated_input = nimcp_tensor_clone(input);
        if (gated_input) {
            float* gi = (float*)nimcp_tensor_data(gated_input);
            size_t n  = nimcp_tensor_numel(gated_input);
            for (size_t j = 0; j < n; j++) {
                gi[j] *= thal_gate;
            }
        }
    }


    // Temporary storage for layer outputs
    const nimcp_tensor_t* layer_input =
        gated_input ? gated_input : input;
    nimcp_tensor_t* layer_output = NULL;
    int forward_rc = LNN_SUCCESS;

    // Forward through each layer
    for (uint32_t i = 0; i < network->n_layers; i++) {
        /* Propagate the per-step cached dend effects pointer. Layers pick
         * it up inside lnn_layer_forward (compute_tau → substrate wrap →
         * compute_derivatives). Reset after the step so stale pointers
         * can't leak into a subsequent forward that happens to be called
         * with no substrate attached. */
        network->layers[i]->substrate_dend_effects = dend_effects_ptr;
        network->layers[i]->substrate_axon_effects = axon_effects_ptr;
        // For last layer, write directly to output
        if (i == network->n_layers - 1) {
            layer_output = output;
        } else {
            // Allocate temporary output for intermediate layers
            uint32_t next_size = network->layers[i]->n_neurons;
            uint32_t dims[1] = {next_size};
            layer_output = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
            if (!layer_output) {
                NIMCP_LOGGING_ERROR("lnn_network_forward_step: Failed to allocate layer output");
                forward_rc = LNN_ERROR_OUT_OF_MEMORY;
                /* layer_output is NULL; goto cleanup will free any
                 * intermediate layer_input still owned by this function. */
                goto cleanup;
            }
        }

        // Forward through layer
        int result = lnn_layer_forward(network->layers[i], layer_input, layer_output, dt);
        if (result != LNN_SUCCESS) {
            NIMCP_LOGGING_ERROR("lnn_network_forward_step: Layer %u forward failed", i);
            forward_rc = result;
            goto cleanup;
        }

        /* Thalamic burst-tau clamp — when the channel is in BURST mode,
         * clamp each neuron's tau to <= 1 ms so subsequent steps respond
         * faster (biological thalamic burst firing). This operates on
         * the layer's tau tensor in-place; the next forward step's
         * lnn_layer_compute_tau will still recompute from inputs, but
         * the visible tau for this step is clamped for any downstream
         * observer (lnn_network_get_tau, tests, introspection). */
        if (thal_burst_clamp && network->layers[i] && network->layers[i]->tau) {
            float* tau_data = (float*)nimcp_tensor_data(network->layers[i]->tau);
            uint32_t n_tau  = network->layers[i]->n_neurons;
            const float tau_cap = 1.0f; /* 1 ms — fast burst timescale */
            for (uint32_t j = 0; j < n_tau; j++) {
                if (tau_data[j] > tau_cap) {
                    tau_data[j] = tau_cap;
                }
            }
        }

        /* Free previous layer input if it was an intermediate tensor —
         * never free the original `input` or the locally-owned
         * `gated_input` here (gated_input is freed after the loop). */
        if (i > 0 && layer_input != input && layer_input != gated_input) {
            nimcp_tensor_destroy((nimcp_tensor_t*)layer_input);
        }

        // Move output to input for next layer
        layer_input = layer_output;
    }

    // Record state if training
    if (network->is_training) {
        lnn_network_record_state(network);
    }

    // Update statistics
    network->stats.forward_steps++;

    /* Substrate adapter: report activity back to the substrate using an
     * L1-norm proxy on the output (LNN has no discrete spikes). */
    if (network->substrate && lnn_tune_get_substrate_enabled() != 0.0f) {
        uint32_t activity = 0;
        if (output && network->n_outputs > 0) {
            const float* out_data = (const float*)nimcp_tensor_data_const(output);
            if (out_data) {
                float sum_abs = 0.0f;
                for (uint32_t i = 0; i < network->n_outputs; i++) {
                    float v = out_data[i];
                    if (isfinite(v)) sum_abs += fabsf(v);
                }
                activity = (uint32_t)(sum_abs * 100.0f);
            }
        }
        substrate_debit_activity(network->substrate,
                                 0 /* region_id */,
                                 activity,
                                 0 /* n_plasticity — optimizer step debits separately */);
    }

    /* W5 KG: anomaly-trigger emit — scan output for NaN/explosion only on
     * networks known to the KG. Sampled: roughly once per forward_step but
     * gated by the anomaly threshold so normal training never writes to the
     * KG. See kg-integration-audit-2026-04-24.md §2.12. */
    if (s_net_lnn_kg_brain && output && network->n_outputs > 0) {
        const float* lnn_out = (const float*)nimcp_tensor_data_const(output);
        if (lnn_out) {
            float l2 = 0.0f;
            int   bad = 0;
            for (uint32_t i = 0; i < network->n_outputs; i++) {
                float v = lnn_out[i];
                if (!isfinite(v)) { bad = 1; break; }
                l2 += v * v;
            }
            if (bad || l2 > 1.0e12f) {
                uint64_t ts_us = (uint64_t)time(NULL) * 1000000ULL;
                net_lnn_kg_emit_event(s_net_lnn_kg_brain,
                    bad ? "gradient_explosion" : "phase_shift",
                    bad ? INFINITY : sqrtf(l2), ts_us);
            }
        }
    }

    /* Thalamic end-of-step hook — submit the network output and tick.
     * Only runs on the success path (before cleanup). */
    if (thal_channel_active) {
        const float* out_data = (const float*)nimcp_tensor_data_const(output);
        size_t        out_n    = nimcp_tensor_numel(output);
        if (out_data && out_n > 0) {
            (void)thalamic_channel_submit(network->thalamic_channel,
                                          0 /* default dest */,
                                          out_data, out_n,
                                          THALAMIC_PRIORITY_NORMAL);
        }
        thalamic_channel_tick(network->thalamic_channel);
    }

cleanup:
    /* Unified cleanup — reachable via both the success fall-through and
     * `goto cleanup` from any error branch above. Every exit path must
     * leave the network in a clean state:
     *   1. No leaked intermediate layer tensors (the per-loop temp that
     *      was going to become the next layer_input, and the freshly
     *      allocated layer_output on the iteration that failed).
     *   2. No stale substrate_{dend,axon}_effects pointers on any layer
     *      (they are borrowed from network->cached_* which may change
     *      next step, or disappear entirely if the substrate is
     *      detached).
     *   3. No leaked gated_input clone.
     *   4. Mutex released (if held). */

    /* (1a) Free layer_output if it's an intermediate tensor that we
     * allocated but never handed off to layer_input. On success, the
     * loop's final iteration assigns layer_input = layer_output where
     * layer_output == output (the caller's buffer), so the pointer
     * equality check below keeps us from double-freeing the caller's
     * output. On the allocation failure path, layer_output is NULL. */
    if (layer_output && layer_output != output && layer_output != layer_input) {
        nimcp_tensor_destroy(layer_output);
    }
    /* (1b) Free layer_input if it's an intermediate (neither the
     * caller's input nor our locally-owned gated_input). On success,
     * layer_input == output (the last layer wrote directly into it),
     * so this branch is a no-op. On an error mid-loop, layer_input
     * may be a prior iteration's temp tensor that must be released. */
    if (layer_input && layer_input != input &&
        layer_input != gated_input && layer_input != output) {
        nimcp_tensor_destroy((nimcp_tensor_t*)layer_input);
    }

    /* (2) Clear per-layer substrate pointers on ALL layers — not just
     * those we touched — so a subsequent forward without substrate
     * attached can't read stale cache from an earlier step. This was
     * previously only done on the success path; it is now
     * unconditional so error paths don't leave dangling pointers. */
    for (uint32_t i = 0; i < network->n_layers; i++) {
        if (network->layers[i]) {
            network->layers[i]->substrate_dend_effects = NULL;
            network->layers[i]->substrate_axon_effects = NULL;
        }
    }

    /* (3) Release the locally-owned gated input, if any. */
    if (gated_input) {
        nimcp_tensor_destroy(gated_input);
    }

    /* (4) Release the mutex we acquired at the top of the function. */
    if (network->mutex) {
        nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)network->mutex);
    }

    return forward_rc;
}

/**
 * @brief Forward pass over sequence
 *
 * WHAT: Process sequence of inputs, producing sequence of outputs
 * WHY:  Temporal sequence processing
 * HOW:  Loop over sequence, calling forward_step for each timestep
 */
int lnn_network_forward_sequence(
    lnn_network_t* network,
    const nimcp_tensor_t* inputs,
    nimcp_tensor_t* outputs,
    uint32_t seq_len,
    float dt
) {
    // Guard: Validate inputs
    if (!network || !inputs || !outputs) {
        NIMCP_LOGGING_ERROR("lnn_network_forward_sequence: NULL argument");
        return LNN_ERROR_NULL_POINTER;
    }

    if (seq_len == 0) {
        NIMCP_LOGGING_ERROR("lnn_network_forward_sequence: Zero sequence length");
        return LNN_ERROR_INVALID_DIMENSION;
    }

    // Process each time step
    for (uint32_t t = 0; t < seq_len; t++) {
        // Extract input at time t
        uint32_t input_dims[1] = {network->n_inputs};
        nimcp_tensor_t* input_t = nimcp_tensor_create(input_dims, 1, NIMCP_DTYPE_F32);
        if (!input_t) {
            NIMCP_LOGGING_ERROR("lnn_network_forward_sequence: Failed to allocate input");
            return LNN_ERROR_OUT_OF_MEMORY;
        }

        // Copy input slice [t, :] to input_t
        for (uint32_t i = 0; i < network->n_inputs; i++) {
            float val = nimcp_tensor_get_flat(inputs, t * network->n_inputs + i);
            nimcp_tensor_set_flat(input_t, i, val);
        }

        // Allocate output for this timestep
        uint32_t output_dims[1] = {network->n_outputs};
        nimcp_tensor_t* output_t = nimcp_tensor_create(output_dims, 1, NIMCP_DTYPE_F32);
        if (!output_t) {
            NIMCP_LOGGING_ERROR("lnn_network_forward_sequence: Failed to allocate output");
            nimcp_tensor_destroy(input_t);
            return LNN_ERROR_OUT_OF_MEMORY;
        }

        // Forward step
        int result = lnn_network_forward_step(network, input_t, output_t, dt);
        nimcp_tensor_destroy(input_t);

        if (result != LNN_SUCCESS) {
            NIMCP_LOGGING_ERROR("lnn_network_forward_sequence: Failed at timestep %u", t);
            nimcp_tensor_destroy(output_t);
            return result;
        }

        // Copy output_t to outputs[t, :]
        for (uint32_t i = 0; i < network->n_outputs; i++) {
            float val = nimcp_tensor_get_flat(output_t, i);
            nimcp_tensor_set_flat(outputs, t * network->n_outputs + i, val);
        }

        nimcp_tensor_destroy(output_t);
    }

    return LNN_SUCCESS;
}

/**
 * @brief Parallel forward over batch of sequences
 *
 * WHAT: Process batch of sequences in parallel
 * WHY:  Efficient batch processing
 * HOW:  Distribute batch across threads (if thread pool connected)
 *
 * NOTE: Simplified implementation - sequential for now (parallel requires thread pool integration)
 */
int lnn_network_forward_batch(
    lnn_network_t* network,
    const nimcp_tensor_t* inputs,
    nimcp_tensor_t* outputs,
    uint32_t batch_size,
    uint32_t seq_len,
    float dt
) {
    // Guard: Validate inputs
    if (!network || !inputs || !outputs) {
        NIMCP_LOGGING_ERROR("lnn_network_forward_batch: NULL argument");
        return LNN_ERROR_NULL_POINTER;
    }

    // For now, process sequentially (TODO: parallelize with thread pool)
    for (uint32_t b = 0; b < batch_size; b++) {
        // Extract batch element [b, :, :]
        uint32_t batch_input_dims[1] = {seq_len * network->n_inputs};
        nimcp_tensor_t* batch_input = nimcp_tensor_create(batch_input_dims, 1, NIMCP_DTYPE_F32);

        uint32_t batch_output_dims[1] = {seq_len * network->n_outputs};
        nimcp_tensor_t* batch_output = nimcp_tensor_create(batch_output_dims, 1, NIMCP_DTYPE_F32);

        if (!batch_input || !batch_output) {
            NIMCP_LOGGING_ERROR("lnn_network_forward_batch: Failed to allocate batch tensors");
            if (batch_input) nimcp_tensor_destroy(batch_input);
            if (batch_output) nimcp_tensor_destroy(batch_output);
            return LNN_ERROR_OUT_OF_MEMORY;
        }

        // Copy batch slice
        size_t batch_offset = b * seq_len * network->n_inputs;
        for (uint32_t i = 0; i < seq_len * network->n_inputs; i++) {
            float val = nimcp_tensor_get_flat(inputs, batch_offset + i);
            nimcp_tensor_set_flat(batch_input, i, val);
        }

        // Process sequence
        int result = lnn_network_forward_sequence(network, batch_input, batch_output, seq_len, dt);

        if (result != LNN_SUCCESS) {
            NIMCP_LOGGING_ERROR("lnn_network_forward_batch: Failed at batch %u", b);
            nimcp_tensor_destroy(batch_input);
            nimcp_tensor_destroy(batch_output);
            return result;
        }

        // Copy result back
        size_t output_offset = b * seq_len * network->n_outputs;
        for (uint32_t i = 0; i < seq_len * network->n_outputs; i++) {
            float val = nimcp_tensor_get_flat(batch_output, i);
            nimcp_tensor_set_flat(outputs, output_offset + i, val);
        }

        nimcp_tensor_destroy(batch_input);
        nimcp_tensor_destroy(batch_output);
    }

    return LNN_SUCCESS;
}

/*=============================================================================
 * State Management
 *===========================================================================*/

/**
 * @brief Get current network state (all layers)
 *
 * WHAT: Concatenate state from all layers
 * WHY:  State checkpointing, introspection
 * HOW:  Gather x from each layer, concatenate
 */
/**
 * Internal unlocked helper — caller MUST hold network->mutex.
 */
static int lnn_network_get_state_unlocked(const lnn_network_t* network, nimcp_tensor_t** state) {
    // Calculate total state size
    uint32_t total_size = 0;
    for (uint32_t i = 0; i < network->n_layers; i++) {
        total_size += network->layers[i]->n_neurons;
    }

    // Allocate output tensor
    uint32_t dims[1] = {total_size};
    *state = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    if (!*state) {
        NIMCP_LOGGING_ERROR("lnn_network_get_state: Failed to allocate state tensor");
        return LNN_ERROR_OUT_OF_MEMORY;
    }

    // Concatenate layer states
    uint32_t offset = 0;
    for (uint32_t i = 0; i < network->n_layers; i++) {
        nimcp_tensor_t* layer_state = NULL;
        int result = lnn_layer_get_state(network->layers[i], &layer_state);
        if (result != LNN_SUCCESS) {
            NIMCP_LOGGING_ERROR("lnn_network_get_state: Failed to get layer %u state", i);
            nimcp_tensor_destroy(*state);
            *state = NULL;
            return result;
        }

        // Copy layer state to output
        for (uint32_t j = 0; j < network->layers[i]->n_neurons; j++) {
            float val = nimcp_tensor_get_flat(layer_state, j);
            nimcp_tensor_set_flat(*state, offset + j, val);
        }

        offset += network->layers[i]->n_neurons;
        nimcp_tensor_destroy(layer_state);
    }

    return LNN_SUCCESS;
}

int lnn_network_get_state(const lnn_network_t* network, nimcp_tensor_t** state) {
    // Guard: Validate inputs
    if (!network || !state) {
        NIMCP_LOGGING_ERROR("lnn_network_get_state: NULL argument");
        return LNN_ERROR_NULL_POINTER;
    }

    /* Lock mutex to prevent race with forward_step which destroys/replaces
     * layer->x tensors.  Without this lock, a concurrent forward_step can
     * free layer->x while we are cloning it, leading to use-after-free
     * (manifests as "nimcp_tensor_create: t->shape.numel is zero"). */
    if (network->mutex) {
        nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)network->mutex);
    }

    int result = lnn_network_get_state_unlocked(network, state);

    if (network->mutex) {
        nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)network->mutex);
    }

    return result;
}

/**
 * @brief Set network state (all layers)
 *
 * WHAT: Restore network state from tensor
 * WHY:  State restoration from checkpoint
 * HOW:  Split state tensor, set each layer's state
 */
int lnn_network_set_state(lnn_network_t* network, const nimcp_tensor_t* state) {
    // Guard: Validate inputs
    if (!network || !state) {
        NIMCP_LOGGING_ERROR("lnn_network_set_state: NULL argument");
        return LNN_ERROR_NULL_POINTER;
    }

    /* Lock mutex — set_state writes to layer->x which forward_step also modifies. */
    if (network->mutex) {
        nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)network->mutex);
    }

    // Split state and set each layer
    uint32_t offset = 0;
    for (uint32_t i = 0; i < network->n_layers; i++) {
        uint32_t layer_size = network->layers[i]->n_neurons;

        // Extract layer state slice
        uint32_t dims[1] = {layer_size};
        nimcp_tensor_t* layer_state = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
        if (!layer_state) {
            NIMCP_LOGGING_ERROR("lnn_network_set_state: Failed to allocate layer state");
            if (network->mutex) {
                nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)network->mutex);
            }
            return LNN_ERROR_OUT_OF_MEMORY;
        }

        // Copy slice
        for (uint32_t j = 0; j < layer_size; j++) {
            float val = nimcp_tensor_get_flat(state, offset + j);
            nimcp_tensor_set_flat(layer_state, j, val);
        }

        // Set layer state
        int result = lnn_layer_set_state(network->layers[i], layer_state);
        nimcp_tensor_destroy(layer_state);

        if (result != LNN_SUCCESS) {
            NIMCP_LOGGING_ERROR("lnn_network_set_state: Failed to set layer %u state", i);
            if (network->mutex) {
                nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)network->mutex);
            }
            return result;
        }

        offset += layer_size;
    }

    if (network->mutex) {
        nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)network->mutex);
    }

    return LNN_SUCCESS;
}

/**
 * @brief Get current time constants (all layers)
 *
 * WHAT: Concatenate τ from all layers
 * WHY:  Introspection of learned dynamics
 * HOW:  Gather tau from each layer, concatenate
 */
int lnn_network_get_tau(const lnn_network_t* network, nimcp_tensor_t** tau) {
    // Guard: Validate inputs
    if (!network || !tau) {
        NIMCP_LOGGING_ERROR("lnn_network_get_tau: NULL argument");
        return LNN_ERROR_NULL_POINTER;
    }

    /* Lock mutex — tau tensors are replaced during lnn_layer_compute_tau
     * which runs inside forward_step under the same mutex. */
    if (network->mutex) {
        nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)network->mutex);
    }

    // Calculate total size
    uint32_t total_size = 0;
    for (uint32_t i = 0; i < network->n_layers; i++) {
        total_size += network->layers[i]->n_neurons;
    }

    // Allocate output tensor
    uint32_t dims[1] = {total_size};
    *tau = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    if (!*tau) {
        NIMCP_LOGGING_ERROR("lnn_network_get_tau: Failed to allocate tau tensor");
        if (network->mutex) {
            nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)network->mutex);
        }
        return LNN_ERROR_OUT_OF_MEMORY;
    }

    // Concatenate layer tau values
    uint32_t offset = 0;
    for (uint32_t i = 0; i < network->n_layers; i++) {
        nimcp_tensor_t* layer_tau = NULL;
        int result = lnn_layer_get_tau(network->layers[i], &layer_tau);
        if (result != LNN_SUCCESS) {
            NIMCP_LOGGING_ERROR("lnn_network_get_tau: Failed to get layer %u tau", i);
            nimcp_tensor_destroy(*tau);
            *tau = NULL;
            if (network->mutex) {
                nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)network->mutex);
            }
            return result;
        }

        // Copy layer tau to output
        for (uint32_t j = 0; j < network->layers[i]->n_neurons; j++) {
            float val = nimcp_tensor_get_flat(layer_tau, j);
            nimcp_tensor_set_flat(*tau, offset + j, val);
        }

        offset += network->layers[i]->n_neurons;
        nimcp_tensor_destroy(layer_tau);
    }

    if (network->mutex) {
        nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)network->mutex);
    }

    return LNN_SUCCESS;
}

/**
 * @brief Reset network state to zero
 *
 * WHAT: Zero out all layer states
 * WHY:  Between sequences
 * HOW:  Call lnn_layer_reset on each layer
 */
void lnn_network_reset_state(lnn_network_t* network) {
    // Guard: NULL-safe
    if (!network) {
        return;
    }

    // Reset each layer
    for (uint32_t i = 0; i < network->n_layers; i++) {
        lnn_layer_reset(network->layers[i]);
    }
}

/**
 * @brief Reset accumulated gradients to zero
 *
 * WHAT: Zero all gradient tensors
 * WHY:  Between training batches
 * HOW:  Call lnn_layer_reset_gradients on each layer
 */
void lnn_network_reset_gradients(lnn_network_t* network) {
    // Guard: NULL-safe
    if (!network) {
        return;
    }

    // Reset each layer
    for (uint32_t i = 0; i < network->n_layers; i++) {
        lnn_layer_reset_gradients(network->layers[i]);
    }
}

/*=============================================================================
 * Training Mode
 *===========================================================================*/

/**
 * @brief Set training mode
 *
 * WHAT: Enable/disable training mode
 * WHY:  Training mode records state history for BPTT
 * HOW:  Set flag, allocate history if needed
 */
void lnn_network_set_training(lnn_network_t* network, bool training) {
    // Guard: Validate input
    if (!network) {
        return;
    }

    network->is_training = training;

    // Allocate history buffer if entering training mode and not preallocated
    if (training && !network->state_history && network->config->bptt_truncation > 0) {
        network->history_capacity = network->config->bptt_truncation;
        network->state_history = (nimcp_tensor_t**)nimcp_calloc(
            network->history_capacity, sizeof(nimcp_tensor_t*)
        );
        if (!network->state_history) {
            NIMCP_LOGGING_WARN("lnn_network_set_training: Failed to allocate history");
            network->history_capacity = 0;
        }
    }

    // Clear history when exiting training mode
    if (!training) {
        lnn_network_clear_history(network);
    }
}

/**
 * @brief Check if in training mode
 *
 * WHAT: Query current training mode
 * WHY:  Conditional behavior based on train/inference
 * HOW:  Return is_training flag
 */
bool lnn_network_is_training(const lnn_network_t* network) {
    // Guard: NULL-safe
    if (!network) {
        return false;
    }

    return network->is_training;
}

/*=============================================================================
 * History Management (for BPTT)
 *===========================================================================*/

/**
 * @brief Record current state in history
 *
 * WHAT: Save current network state to history buffer
 * WHY:  BPTT requires state at each time step
 * HOW:  Get state, store in circular buffer
 */
int lnn_network_record_state(lnn_network_t* network) {
    // Guard: Validate input
    if (!network) {
        NIMCP_LOGGING_ERROR("lnn_network_record_state: NULL network");
        return LNN_ERROR_NULL_POINTER;
    }

    // Check if history is allocated
    if (!network->state_history || network->history_capacity == 0) {
        return LNN_SUCCESS;  // No history buffer, skip silently
    }

    // Get current state — use unlocked helper since caller (forward_step)
    // already holds network->mutex.
    nimcp_tensor_t* state = NULL;
    int result = lnn_network_get_state_unlocked(network, &state);
    if (result != LNN_SUCCESS) {
        NIMCP_LOGGING_ERROR("lnn_network_record_state: Failed to get state");
        return result;
    }

    // Free old state if buffer is full
    if (network->state_history[network->history_write_idx]) {
        nimcp_tensor_destroy(network->state_history[network->history_write_idx]);
    }

    // Store state in circular buffer
    network->state_history[network->history_write_idx] = state;
    network->history_write_idx = (network->history_write_idx + 1) % network->history_capacity;

    // Update length
    if (network->history_len < network->history_capacity) {
        network->history_len++;
    }

    return LNN_SUCCESS;
}

/**
 * @brief Get state from history at given time step
 *
 * WHAT: Retrieve previously recorded state
 * WHY:  Access intermediate states for gradient computation
 * HOW:  Index into circular buffer
 */
int lnn_network_get_history(
    const lnn_network_t* network,
    uint32_t step,
    nimcp_tensor_t** state
) {
    // Guard: Validate inputs
    if (!network || !state) {
        NIMCP_LOGGING_ERROR("lnn_network_get_history: NULL argument");
        return LNN_ERROR_NULL_POINTER;
    }

    if (step >= network->history_len) {
        NIMCP_LOGGING_ERROR("lnn_network_get_history: Invalid step %u (max %u)",
                           step, network->history_len - 1);
        return LNN_ERROR_INVALID_DIMENSION;
    }

    if (!network->state_history) {
        NIMCP_LOGGING_ERROR("lnn_network_get_history: No history buffer");
        return LNN_ERROR_NOT_INITIALIZED;
    }

    // Calculate circular buffer index
    uint32_t idx = (network->history_write_idx + network->history_capacity - network->history_len + step)
                   % network->history_capacity;

    // Clone state tensor
    *state = nimcp_tensor_clone(network->state_history[idx]);
    if (!*state) {
        NIMCP_LOGGING_ERROR("lnn_network_get_history: Failed to clone state");
        return LNN_ERROR_OUT_OF_MEMORY;
    }

    return LNN_SUCCESS;
}

/**
 * @brief Clear state history
 *
 * WHAT: Reset history buffer to empty
 * WHY:  Between training epochs, free memory
 * HOW:  Free all history tensors, reset indices
 */
void lnn_network_clear_history(lnn_network_t* network) {
    // Guard: NULL-safe
    if (!network || !network->state_history) {
        return;
    }

    // Free all stored states
    for (uint32_t i = 0; i < network->history_capacity; i++) {
        if (network->state_history[i]) {
            nimcp_tensor_destroy(network->state_history[i]);
            network->state_history[i] = NULL;
        }
    }

    // Reset indices
    network->history_len = 0;
    network->history_write_idx = 0;
}

/*=============================================================================
 * Integration Connections
 *===========================================================================*/

/**
 * @brief Connect to NIMCP optimizer
 *
 * WHAT: Link network to optimizer for parameter updates
 * WHY:  Enable training with Adam, SGD, etc.
 * HOW:  Store optimizer handle
 */
int lnn_network_connect_optimizer(lnn_network_t* network, void* optimizer) {
    // Guard: Validate inputs
    if (!network) {
        NIMCP_LOGGING_ERROR("lnn_network_connect_optimizer: NULL network");
        return LNN_ERROR_NULL_POINTER;
    }

    network->optimizer = optimizer;

    NIMCP_LOGGING_INFO("Connected optimizer to network '%s'", network->name);
    return LNN_SUCCESS;
}

/**
 * @brief Connect to gradient manager
 *
 * WHAT: Link network to gradient manager
 * WHY:  Enable gradient accumulation, clipping
 * HOW:  Store gradient manager handle
 */
int lnn_network_connect_gradient_manager(lnn_network_t* network, void* grad_mgr) {
    // Guard: Validate inputs
    if (!network) {
        NIMCP_LOGGING_ERROR("lnn_network_connect_gradient_manager: NULL network");
        return LNN_ERROR_NULL_POINTER;
    }

    network->gradient_manager = grad_mgr;

    NIMCP_LOGGING_INFO("Connected gradient manager to network '%s'", network->name);
    return LNN_SUCCESS;
}

/**
 * @brief Connect to thread pool
 *
 * WHAT: Link network to thread pool for parallel execution
 * WHY:  Enable batch parallelism
 * HOW:  Store thread pool handle
 */
int lnn_network_connect_thread_pool(lnn_network_t* network, void* pool) {
    // Guard: Validate inputs
    if (!network) {
        NIMCP_LOGGING_ERROR("lnn_network_connect_thread_pool: NULL network");
        return LNN_ERROR_NULL_POINTER;
    }

    network->thread_pool = pool;

    NIMCP_LOGGING_INFO("Connected thread pool to network '%s'", network->name);
    return LNN_SUCCESS;
}

/*=============================================================================
 * Statistics
 *===========================================================================*/

/**
 * @brief Get network statistics
 *
 * WHAT: Compute current network stats
 * WHY:  Monitor training, detect issues
 * HOW:  Aggregate stats from all layers
 */
int lnn_network_get_stats(const lnn_network_t* network, lnn_network_stats_t* stats) {
    // Guard: Validate inputs
    if (!network || !stats) {
        NIMCP_LOGGING_ERROR("lnn_network_get_stats: NULL argument");
        return LNN_ERROR_NULL_POINTER;
    }

    // Copy current stats
    memcpy(stats, &network->stats, sizeof(lnn_network_stats_t));

    // Compute average tau across all layers
    float total_tau = 0.0f;
    uint32_t total_neurons = 0;

    for (uint32_t i = 0; i < network->n_layers; i++) {
        float avg_tau, min_tau, max_tau, state_norm;
        lnn_layer_get_stats(network->layers[i], &avg_tau, &min_tau, &max_tau, &state_norm);

        total_tau += avg_tau * network->layers[i]->n_neurons;
        total_neurons += network->layers[i]->n_neurons;
    }

    stats->avg_tau_network = (total_neurons > 0) ? (total_tau / total_neurons) : 0.0f;

    // Compute state norm
    nimcp_tensor_t* state = NULL;
    if (lnn_network_get_state(network, &state) == LNN_SUCCESS) {
        stats->state_norm = nimcp_tensor_norm_p(state, 2.0f);
        nimcp_tensor_destroy(state);
    }

    // Compute memory usage
    stats->memory_usage_bytes = lnn_network_memory_usage(network);

    return LNN_SUCCESS;
}

/**
 * @brief Get total number of trainable parameters
 *
 * WHAT: Count all learnable weights across all layers
 * WHY:  Model capacity reporting
 * HOW:  Sum lnn_layer_param_count across layers
 */
size_t lnn_network_param_count(const lnn_network_t* network) {
    // Guard: NULL-safe
    if (!network) {
        return 0;
    }

    size_t total = 0;
    for (uint32_t i = 0; i < network->n_layers; i++) {
        total += lnn_layer_param_count(network->layers[i]);
    }

    return total;
}

/**
 * @brief Get total memory usage
 *
 * WHAT: Compute total memory consumed by network
 * WHY:  Memory profiling
 * HOW:  Sum memory from layers, history, metadata
 */
size_t lnn_network_memory_usage(const lnn_network_t* network) {
    // Guard: NULL-safe
    if (!network) {
        return 0;
    }

    size_t total = sizeof(lnn_network_t);

    // Config
    total += sizeof(lnn_config_t);
    if (network->config && network->config->layer_configs) {
        total += network->n_layers * sizeof(lnn_layer_config_t);
    }

    // Layers array
    total += network->n_layers * sizeof(lnn_layer_t*);

    // Layer memory (simplified - would need layer-specific memory calculation)
    for (uint32_t i = 0; i < network->n_layers; i++) {
        total += sizeof(lnn_layer_t);
        // Add tensor memory (rough estimate)
        total += network->layers[i]->n_neurons * sizeof(float) * 10;  // State + weights
    }

    // History buffer
    total += network->history_capacity * sizeof(nimcp_tensor_t*);

    return total;
}

/*=============================================================================
 * Parameter Access
 *===========================================================================*/

/**
 * @brief Helper to copy tensor data to flat buffer
 */
static size_t copy_tensor_to_buffer(const nimcp_tensor_t* t, float* buf, size_t offset) {
    if (!t) return 0;
    size_t n = nimcp_tensor_numel(t);
    const float* data = (const float*)nimcp_tensor_data_const(t);
    if (data) {
        memcpy(buf + offset, data, n * sizeof(float));
    }
    return n;
}

/**
 * @brief Helper to copy flat buffer data to tensor
 */
static size_t copy_buffer_to_tensor(nimcp_tensor_t* t, const float* buf, size_t offset) {
    if (!t) return 0;
    size_t n = nimcp_tensor_numel(t);
    float* data = (float*)nimcp_tensor_data(t);
    if (data) {
        memcpy(data, buf + offset, n * sizeof(float));
    }
    return n;
}

/**
 * @brief Get all network parameters as flat array
 *
 * WHAT: Extract all trainable weights into contiguous buffer
 * WHY:  Interface with optimizers
 * HOW:  Iterate layers, copy W_in, W_rec, W_tau, b_in, b_tau, tau_base
 */
int lnn_network_get_params(
    const lnn_network_t* network,
    float* params,
    size_t* n_params
) {
    /* Guard clauses */
    if (!network) {
        NIMCP_LOGGING_ERROR("lnn_network_get_params: NULL network");
        return LNN_ERROR_NULL_POINTER;
    }
    if (!params || !n_params) {
        NIMCP_LOGGING_ERROR("lnn_network_get_params: NULL output pointer");
        return LNN_ERROR_NULL_POINTER;
    }

    size_t offset = 0;

    /* Copy parameters from each layer */
    for (uint32_t i = 0; i < network->n_layers; i++) {
        lnn_layer_t* layer = network->layers[i];
        if (!layer) continue;

        /* Order: W_in, W_rec, W_tau, b_in, b_tau, tau_base */
        offset += copy_tensor_to_buffer(layer->W_in, params, offset);
        offset += copy_tensor_to_buffer(layer->W_rec, params, offset);
        offset += copy_tensor_to_buffer(layer->W_tau, params, offset);
        offset += copy_tensor_to_buffer(layer->b_in, params, offset);
        offset += copy_tensor_to_buffer(layer->b_tau, params, offset);
        offset += copy_tensor_to_buffer(layer->tau_base, params, offset);
    }

    *n_params = offset;
    return LNN_SUCCESS;
}

/**
 * @brief Set all network parameters from flat array
 *
 * WHAT: Load trainable weights from contiguous buffer
 * WHY:  Apply optimizer updates
 * HOW:  Iterate layers, write to weight tensors
 */
int lnn_network_set_params(
    lnn_network_t* network,
    const float* params,
    size_t n_params
) {
    /* Guard clauses */
    if (!network) {
        NIMCP_LOGGING_ERROR("lnn_network_set_params: NULL network");
        return LNN_ERROR_NULL_POINTER;
    }
    if (!params) {
        NIMCP_LOGGING_ERROR("lnn_network_set_params: NULL params");
        return LNN_ERROR_NULL_POINTER;
    }

    size_t expected = lnn_network_param_count(network);
    if (n_params != expected) {
        NIMCP_LOGGING_ERROR("lnn_network_set_params: param count mismatch (%zu vs %zu)",
                           n_params, expected);
        return LNN_ERROR_INVALID_PARAM;
    }

    size_t offset = 0;

    /* Copy parameters to each layer */
    for (uint32_t i = 0; i < network->n_layers; i++) {
        lnn_layer_t* layer = network->layers[i];
        if (!layer) continue;

        /* Order: W_in, W_rec, W_tau, b_in, b_tau, tau_base */
        offset += copy_buffer_to_tensor(layer->W_in, params, offset);
        offset += copy_buffer_to_tensor(layer->W_rec, params, offset);
        offset += copy_buffer_to_tensor(layer->W_tau, params, offset);
        offset += copy_buffer_to_tensor(layer->b_in, params, offset);
        offset += copy_buffer_to_tensor(layer->b_tau, params, offset);
        offset += copy_buffer_to_tensor(layer->tau_base, params, offset);
    }

    return LNN_SUCCESS;
}

/**
 * @brief Get all gradient values as flat array
 *
 * WHAT: Extract accumulated gradients from all layers
 * WHY:  Interface with optimizers
 * HOW:  Iterate layers, copy grad_W_in, grad_W_rec, etc.
 */
int lnn_network_get_gradients(
    const lnn_network_t* network,
    float* gradients,
    size_t* n_grads
) {
    /* Guard clauses */
    if (!network) {
        NIMCP_LOGGING_ERROR("lnn_network_get_gradients: NULL network");
        return LNN_ERROR_NULL_POINTER;
    }
    if (!gradients || !n_grads) {
        NIMCP_LOGGING_ERROR("lnn_network_get_gradients: NULL output pointer");
        return LNN_ERROR_NULL_POINTER;
    }

    size_t offset = 0;

    /* Copy gradients from each layer */
    for (uint32_t i = 0; i < network->n_layers; i++) {
        lnn_layer_t* layer = network->layers[i];
        if (!layer) continue;

        /* Order: same as params */
        offset += copy_tensor_to_buffer(layer->grad_W_in, gradients, offset);
        offset += copy_tensor_to_buffer(layer->grad_W_rec, gradients, offset);
        offset += copy_tensor_to_buffer(layer->grad_W_tau, gradients, offset);
        offset += copy_tensor_to_buffer(layer->grad_b_in, gradients, offset);
        offset += copy_tensor_to_buffer(layer->grad_b_tau, gradients, offset);
        offset += copy_tensor_to_buffer(layer->grad_tau_base, gradients, offset);
    }

    *n_grads = offset;
    return LNN_SUCCESS;
}

/**
 * @brief Zero all gradient tensors in network
 *
 * WHAT: Reset gradients to zero
 * WHY:  Prevent gradient accumulation
 * HOW:  Call nimcp_tensor_zero on all grad_* tensors
 */
int lnn_network_zero_gradients(lnn_network_t* network) {
    /* Guard clause */
    if (!network) {
        NIMCP_LOGGING_ERROR("lnn_network_zero_gradients: NULL network");
        return LNN_ERROR_NULL_POINTER;
    }

    for (uint32_t i = 0; i < network->n_layers; i++) {
        lnn_layer_t* layer = network->layers[i];
        if (!layer) continue;

        if (layer->grad_W_in) nimcp_tensor_zero_grad(layer->grad_W_in);
        if (layer->grad_W_rec) nimcp_tensor_zero_grad(layer->grad_W_rec);
        if (layer->grad_W_tau) nimcp_tensor_zero_grad(layer->grad_W_tau);
        if (layer->grad_b_in) nimcp_tensor_zero_grad(layer->grad_b_in);
        if (layer->grad_b_tau) nimcp_tensor_zero_grad(layer->grad_b_tau);
        if (layer->grad_tau_base) nimcp_tensor_zero_grad(layer->grad_tau_base);
    }

    return LNN_SUCCESS;
}

/*=============================================================================
 * Serialization
 *===========================================================================*/

/**
 * @brief Save network to file
 *
 * WHAT: Serialize network to disk
 * WHY:  Model persistence
 * HOW:  Write magic number, config, layer weights
 *
 * NOTE: Placeholder implementation - full serialization requires binary format
 */
int lnn_network_save(const lnn_network_t* network, const char* path) {
    if (!network || !path) {
        NIMCP_LOGGING_ERROR("lnn_network_save: NULL argument");
        return LNN_ERROR_NULL_POINTER;
    }

    /* Atomic write: write to temp file, then rename */
    char tmp_path[1024];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    FILE* f = fopen(tmp_path, "wb");
    if (!f) {
        NIMCP_LOGGING_ERROR("lnn_network_save: failed to open %s", tmp_path);
        return -1;
    }

    /* Checked fwrite helper macro */
    #define LNN_FWRITE(ptr, size, count) \
        do { if (fwrite((ptr), (size), (count), f) != (count)) { \
            NIMCP_LOGGING_ERROR("lnn_network_save: fwrite failed"); \
            fclose(f); remove(tmp_path); return -1; \
        } } while (0)

    /* Header */
    uint32_t magic = 0x4C4E4E53; /* "LNNS" */
    uint32_t version = 2;  /* v2: adds layer->p (momentum) + use_hamiltonian */
    LNN_FWRITE(&magic, sizeof(uint32_t), 1);
    LNN_FWRITE(&version, sizeof(uint32_t), 1);

    /* Network metadata */
    LNN_FWRITE(&network->n_layers, sizeof(uint32_t), 1);
    LNN_FWRITE(&network->n_inputs, sizeof(uint32_t), 1);
    LNN_FWRITE(&network->n_outputs, sizeof(uint32_t), 1);
    LNN_FWRITE(&network->is_training, sizeof(bool), 1);

    /* Per-layer weights and state */
    for (uint32_t i = 0; i < network->n_layers; i++) {
        lnn_layer_t* layer = network->layers[i];
        if (!layer) {
            uint32_t zero = 0;
            LNN_FWRITE(&zero, sizeof(uint32_t), 1);
            continue;
        }
        LNN_FWRITE(&layer->n_neurons, sizeof(uint32_t), 1);
        LNN_FWRITE(&layer->ode_method, sizeof(uint32_t), 1);
        LNN_FWRITE(&layer->dt, sizeof(float), 1);
        LNN_FWRITE(&layer->use_layer_norm, sizeof(bool), 1);
        LNN_FWRITE(&layer->layer_norm_eps, sizeof(float), 1);

        /* Save weight tensors (NULL-safe via nimcp_tensor_save) */
        nimcp_tensor_save(layer->W_in, f);
        nimcp_tensor_save(layer->W_rec, f);
        nimcp_tensor_save(layer->W_tau, f);
        nimcp_tensor_save(layer->b_in, f);
        nimcp_tensor_save(layer->b_tau, f);
        nimcp_tensor_save(layer->tau_base, f);

        /* Save state tensors */
        nimcp_tensor_save(layer->x, f);
        nimcp_tensor_save(layer->tau, f);

        /* Save Hamiltonian momentum (NULL-safe: nimcp_tensor_save writes 0 for NULL) */
        nimcp_tensor_save(layer->p, f);
        LNN_FWRITE(&layer->use_hamiltonian, sizeof(bool), 1);
    }

    #undef LNN_FWRITE

    fclose(f);

    /* Atomic rename: temp → final path */
    if (rename(tmp_path, path) != 0) {
        NIMCP_LOGGING_ERROR("lnn_network_save: rename %s → %s failed", tmp_path, path);
        remove(tmp_path);
        return -1;
    }

    NIMCP_LOGGING_INFO("LNN network saved to %s (%u layers)", path, network->n_layers);
    return 0;
}

/**
 * @brief Load network from file
 *
 * WHAT: Deserialize network from disk
 * WHY:  Restore saved model
 * HOW:  Read file, reconstruct config, create network, load weights
 *
 * NOTE: Placeholder implementation
 */
lnn_network_t* lnn_network_load(const char* path) {
    if (!path) {
        NIMCP_LOGGING_ERROR("lnn_network_load: NULL path");
        return NULL;
    }

    /* Ensure LNN library is initialized — checkpoint load may bypass
     * the normal brain_init LNN wave that calls lnn_init(). Without
     * this, g_lnn_initialized is false and all forward/backward calls
     * return LNN_ERROR_INVALID_STATE. */
    extern int lnn_init(uint32_t n_threads);
    lnn_init(1);

    FILE* f = fopen(path, "rb");
    if (!f) {
        NIMCP_LOGGING_WARN("lnn_network_load: file not found %s", path);
        return NULL;
    }

    /* Header */
    uint32_t magic = 0, version = 0;
    if (fread(&magic, sizeof(uint32_t), 1, f) != 1 || magic != 0x4C4E4E53) {
        NIMCP_LOGGING_ERROR("lnn_network_load: invalid magic");
        fclose(f);
        return NULL;
    }
    if (fread(&version, sizeof(uint32_t), 1, f) != 1 || version > 2) {
        NIMCP_LOGGING_ERROR("lnn_network_load: unsupported version %u", version);
        fclose(f); return NULL;
    }

    /* Network metadata */
    uint32_t n_layers = 0, n_inputs = 0, n_outputs = 0;
    bool is_training = false;
    if (fread(&n_layers, sizeof(uint32_t), 1, f) != 1 ||
        fread(&n_inputs, sizeof(uint32_t), 1, f) != 1 ||
        fread(&n_outputs, sizeof(uint32_t), 1, f) != 1 ||
        fread(&is_training, sizeof(bool), 1, f) != 1) {
        NIMCP_LOGGING_ERROR("lnn_network_load: truncated metadata");
        fclose(f); return NULL;
    }

    /* Recreate network using NCP topology (matches brain_enable_multi_network_training) */
    uint32_t n_inter = (n_inputs + n_outputs) / 2;
    if (n_inter < 8) n_inter = 8;
    uint32_t n_command = n_outputs * 2;
    if (n_command < 8) n_command = 8;

    lnn_network_t* net = lnn_network_create_ncp(n_inputs, n_inter, n_command, n_outputs);
    if (!net) {
        NIMCP_LOGGING_ERROR("lnn_network_load: failed to create NCP network");
        fclose(f);
        return NULL;
    }

    /* Restore per-layer weights from file */
    uint32_t restore_layers = (n_layers < net->n_layers) ? n_layers : net->n_layers;
    for (uint32_t i = 0; i < n_layers; i++) {
        uint32_t n_neurons = 0;
        uint32_t ode_method = 0;
        float dt = 0.0f;
        bool use_layer_norm = false;
        float layer_norm_eps = 0.0f;

        if (fread(&n_neurons, sizeof(uint32_t), 1, f) != 1) {
            NIMCP_LOGGING_WARN("lnn_network_load: truncated at layer %u", i);
            break;
        }
        if (n_neurons == 0) continue;

        if (fread(&ode_method, sizeof(uint32_t), 1, f) != 1 ||
            fread(&dt, sizeof(float), 1, f) != 1 ||
            fread(&use_layer_norm, sizeof(bool), 1, f) != 1 ||
            fread(&layer_norm_eps, sizeof(float), 1, f) != 1) {
            NIMCP_LOGGING_WARN("lnn_network_load: truncated layer %u config", i);
            break;
        }

        /* Load weight tensors */
        nimcp_tensor_t* W_in = nimcp_tensor_load(f);
        nimcp_tensor_t* W_rec = nimcp_tensor_load(f);
        nimcp_tensor_t* W_tau = nimcp_tensor_load(f);
        nimcp_tensor_t* b_in = nimcp_tensor_load(f);
        nimcp_tensor_t* b_tau = nimcp_tensor_load(f);
        nimcp_tensor_t* tau_base = nimcp_tensor_load(f);
        nimcp_tensor_t* x = nimcp_tensor_load(f);
        nimcp_tensor_t* tau = nimcp_tensor_load(f);

        /* Apply to matching layer in recreated network */
        if (i < restore_layers && net->layers[i]) {
            lnn_layer_t* layer = net->layers[i];

            /* Replace weight tensors if dimensions match */
            #define SWAP_TENSOR(dst, src) do { \
                if ((src) && (dst) && nimcp_tensor_numel(src) == nimcp_tensor_numel(dst)) { \
                    nimcp_tensor_destroy(dst); \
                    (dst) = (src); \
                    (src) = NULL; \
                } \
            } while(0)

            SWAP_TENSOR(layer->W_in, W_in);
            SWAP_TENSOR(layer->W_rec, W_rec);
            SWAP_TENSOR(layer->W_tau, W_tau);
            SWAP_TENSOR(layer->b_in, b_in);
            SWAP_TENSOR(layer->b_tau, b_tau);
            SWAP_TENSOR(layer->tau_base, tau_base);
            SWAP_TENSOR(layer->x, x);
            SWAP_TENSOR(layer->tau, tau);
            #undef SWAP_TENSOR

            layer->ode_method = (lnn_ode_method_t)ode_method;
            layer->dt = dt;
            layer->use_layer_norm = use_layer_norm;
            layer->layer_norm_eps = layer_norm_eps;

            /* Post-restore stability: spectral-normalize W_rec.
             * Checkpoints from before spectral normalization was added may
             * contain W_rec with σ_max >> 1, causing ODE divergence → NaN
             * in the very first forward step. */
            {
                uint32_t n = layer->n_neurons;
                float* W = (float*)nimcp_tensor_data(layer->W_rec);
                if (W && n > 0) {
                    float* u_vec = (float*)nimcp_calloc(n, sizeof(float));
                    float* v_vec = (float*)nimcp_calloc(n, sizeof(float));
                    if (u_vec && v_vec) {
                        float iv = 1.0f / sqrtf((float)n);
                        for (uint32_t ui = 0; ui < n; ui++) u_vec[ui] = iv;
                        float sigma = 0.0f;
                        for (int iter = 0; iter < 10; iter++) {
                            float vn = 0.0f;
                            for (uint32_t j = 0; j < n; j++) {
                                float s = 0.0f;
                                for (uint32_t k = 0; k < n; k++)
                                    s += W[k * n + j] * u_vec[k];
                                v_vec[j] = s; vn += s * s;
                            }
                            vn = sqrtf(vn);
                            if (vn < 1e-12f) break;
                            for (uint32_t j = 0; j < n; j++) v_vec[j] /= vn;
                            float un = 0.0f;
                            for (uint32_t k = 0; k < n; k++) {
                                float s = 0.0f;
                                for (uint32_t j = 0; j < n; j++)
                                    s += W[k * n + j] * v_vec[j];
                                u_vec[k] = s; un += s * s;
                            }
                            un = sqrtf(un);
                            if (un < 1e-12f) break;
                            sigma = un;
                            for (uint32_t k = 0; k < n; k++) u_vec[k] /= un;
                        }
                        if (sigma > 1.0f) {
                            float scale = 1.0f / sigma;
                            size_t sz = (size_t)n * n;
                            for (size_t wi = 0; wi < sz; wi++) W[wi] *= scale;
                            NIMCP_LOGGING_INFO("lnn_network_load: layer %u W_rec spectral norm %.2f → 1.0", i, sigma);
                        }
                    }
                    if (u_vec) nimcp_free(u_vec);
                    if (v_vec) nimcp_free(v_vec);
                }
            }

            /* Post-restore: sanitize loaded state x — NaN/Inf from old
             * checkpoints would immediately produce NaN on the first
             * forward step and propagate to all feature vectors. */
            {
                float* xd = (float*)nimcp_tensor_data(layer->x);
                uint32_t n = layer->n_neurons;
                bool had_nan = false;
                for (uint32_t xi = 0; xi < n; xi++) {
                    if (!isfinite(xd[xi])) {
                        xd[xi] = 0.0f;
                        had_nan = true;
                    }
                }
                if (had_nan) {
                    NIMCP_LOGGING_WARN("lnn_network_load: layer %u had NaN/Inf in state — reset to 0", i);
                }
            }

            /* Post-restore: sanitize loaded tau — NaN/Inf from old
             * checkpoints would cause 1/tau division to produce NaN/Inf
             * on the first forward step. Reset to tau_base_init. */
            {
                float* td = (float*)nimcp_tensor_data(layer->tau);
                uint32_t n = layer->n_neurons;
                bool had_nan = false;
                for (uint32_t ti = 0; ti < n; ti++) {
                    if (!isfinite(td[ti]) || td[ti] < 0.01f) {
                        td[ti] = 10.0f;  /* Default tau_base_init */
                        had_nan = true;
                    }
                }
                if (had_nan) {
                    NIMCP_LOGGING_WARN("lnn_network_load: layer %u had NaN/Inf in tau — reset to 10.0", i);
                }
            }

            /* Post-restore: sanitize all weight/bias tensors for NaN/Inf.
             * Any NaN in W_tau or b_tau → NaN logits → NaN tau at runtime. */
            {
                struct { nimcp_tensor_t* t; const char* name; } tensors[] = {
                    {layer->W_in, "W_in"}, {layer->W_rec, "W_rec"},
                    {layer->W_tau, "W_tau"}, {layer->b_in, "b_in"},
                    {layer->b_tau, "b_tau"},
                };
                for (int ti = 0; ti < 5; ti++) {
                    if (!tensors[ti].t) continue;
                    float* data = (float*)nimcp_tensor_data(tensors[ti].t);
                    uint32_t numel = nimcp_tensor_numel(tensors[ti].t);
                    uint32_t fixed = 0;
                    for (uint32_t fi = 0; fi < numel; fi++) {
                        if (!isfinite(data[fi])) {
                            data[fi] = 0.0f;
                            fixed++;
                        }
                    }
                    if (fixed > 0) {
                        NIMCP_LOGGING_WARN("lnn_network_load: layer %u %s had %u NaN/Inf values — reset to 0",
                                           i, tensors[ti].name, fixed);
                    }
                }
            }

            /* Post-restore: enforce tau_base floor.
             * tau = tau_base * sigmoid(...), so if tau_base < 0.01,
             * the decay term -x/tau can explode even with sigmoid ≈ 1. */
            {
                float* tb = (float*)nimcp_tensor_data(layer->tau_base);
                uint32_t n = layer->n_neurons;
                uint32_t fixed = 0;
                for (uint32_t ti = 0; ti < n; ti++) {
                    if (!isfinite(tb[ti]) || tb[ti] < 0.01f) {
                        tb[ti] = 1.0f;  /* Reset to default 1.0 */
                        fixed++;
                    }
                }
                if (fixed > 0) {
                    NIMCP_LOGGING_WARN("lnn_network_load: layer %u fixed %u tau_base values (< 0.01 or NaN)", i, fixed);
                }
            }
        }

        /* v2: Load Hamiltonian momentum and flag */
        nimcp_tensor_t* p_loaded = NULL;
        bool use_hamiltonian_loaded = false;
        if (version >= 2) {
            p_loaded = nimcp_tensor_load(f);
            if (fread(&use_hamiltonian_loaded, sizeof(bool), 1, f) != 1) {
                use_hamiltonian_loaded = false;
            }
        }

        if (i < restore_layers && net->layers[i]) {
            lnn_layer_t* layer = net->layers[i];

            /* Restore momentum if saved */
            if (p_loaded && layer->n_neurons > 0) {
                if (!layer->p || nimcp_tensor_numel(layer->p) != nimcp_tensor_numel(p_loaded)) {
                    nimcp_tensor_destroy(layer->p);
                    layer->p = p_loaded;
                    p_loaded = NULL;
                } else {
                    memcpy(nimcp_tensor_data(layer->p),
                           nimcp_tensor_data_const(p_loaded),
                           nimcp_tensor_numel(p_loaded) * sizeof(float));
                }
            }

            /* Restore use_hamiltonian flag — but don't enable without H_net */
            if (use_hamiltonian_loaded && layer->H_net) {
                layer->use_hamiltonian = true;
            }
        }
        nimcp_tensor_destroy(p_loaded);

        /* Clean up any tensors not consumed */
        nimcp_tensor_destroy(W_in);
        nimcp_tensor_destroy(W_rec);
        nimcp_tensor_destroy(W_tau);
        nimcp_tensor_destroy(b_in);
        nimcp_tensor_destroy(b_tau);
        nimcp_tensor_destroy(tau_base);
        nimcp_tensor_destroy(x);
        nimcp_tensor_destroy(tau);
    }

    net->is_training = is_training;
    fclose(f);
    NIMCP_LOGGING_INFO("LNN network loaded from %s (%u layers restored)", path, restore_layers);
    return net;
}
