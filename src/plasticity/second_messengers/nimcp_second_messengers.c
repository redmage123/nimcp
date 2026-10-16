/**
 * @file nimcp_second_messengers.c
 * @brief Implementation of intracellular second messenger cascade signaling
 *
 * WHAT: Implements cAMP, IP3/DAG, calcium, and gene expression cascades
 * WHY:  Bridges receptor activation to long-term plasticity changes
 * HOW:  Euler integration of cascade ODEs with bio-async messaging
 *
 * IMPLEMENTATION NOTES:
 * - All dynamics use first-order ODEs with exponential decay
 * - Kinase activations use Hill functions for sigmoidality
 * - Gene expression has longer time constants than kinase cascades
 * - Bio-async messages sent on significant state changes
 *
 * CODING STANDARDS:
 * - Guard clauses (no nested ifs)
 * - Helper functions (<50 lines)
 * - WHAT-WHY-HOW documentation
 * - Single Responsibility Principle
 *
 * @author NIMCP Development Team
 * @date 2025-12-09
 * @version 1.0.0
 */

#include "plasticity/nimcp_second_messengers.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_time.h"
#include "security/nimcp_security.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "utils/math/nimcp_math_helpers.h"

/*=============================================================================
 * MODULE LOGGING
 *============================================================================*/

#undef LOG_MODULE
#define LOG_MODULE "SECOND_MESSENGER"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(second_messengers)

/*=============================================================================
 * INTERNAL STRUCTURES
 *============================================================================*/

/**
 * @brief Internal second messenger system structure
 */
struct second_messenger_system {
    /* Configuration */
    second_messenger_config_t config;
    uint32_t max_neurons;

    /* State storage */
    second_messenger_state_t* states;   /**< Per-neuron cascade states */
    bool* neuron_active;                /**< Whether neuron has active cascade */
    uint32_t active_count;              /**< Number of active cascades */

    /* Bio-async integration */
    bio_module_context_t bio_ctx;       /**< Bio-async context */
    bool bio_async_enabled;             /**< Whether bio-async is active */

    /* Neuromodulator integration */
    neuromodulator_system_t neuromod;   /**< Connected neuromodulator system */
    bool neuromod_connected;            /**< Whether neuromod is connected */

    /* Statistics */
    second_messenger_stats_t stats;

    /* Thread safety */
    nimcp_platform_mutex_t mutex;
    bool mutex_initialized;

    /* Security */
    bool security_registered;
};

/*=============================================================================
 * HELPER FUNCTION DECLARATIONS
 *============================================================================*/

static void init_state_baseline(second_messenger_state_t* state, uint32_t neuron_id,
                                uint64_t timestamp_ms);
static void update_camp_pathway(second_messenger_state_t* state, float dt_ms);
static void update_ip3_dag_pathway(second_messenger_state_t* state, float dt_ms);
static void update_calcium_signaling(second_messenger_state_t* state, float dt_ms,
                                     const second_messenger_config_t* config);
static void update_gene_expression(second_messenger_state_t* state, float dt_ms,
                                   const second_messenger_config_t* config);
static void update_integration_state(second_messenger_state_t* state);
static float hill_function(float x, float k, float n);
static nimcp_result_t bio_message_handler(const void* msg, size_t msg_size,
                                          nimcp_bio_promise_t response_promise,
                                          void* user_data);

/*=============================================================================
 * LIFECYCLE IMPLEMENTATION
 *============================================================================*/

/**
 * @brief Get default configuration
 */
second_messenger_config_t second_messenger_default_config(void) {
    second_messenger_config_t config = {
        /* Kinetics */
        .dt_ms = SM_DEFAULT_DT_MS,
        .camp_synthesis_rate = 10.0F,
        .camp_degradation_rate = 0.5F,
        .ip3_synthesis_rate = 20.0F,
        .ip3_degradation_rate = 0.1F,
        .ca_release_rate = 100.0F,
        .ca_reuptake_rate = 10.0F,

        /* Thresholds */
        .pka_activation_threshold = 1.0F,   /* cAMP in micromolar */
        .pkc_activation_threshold = 0.3F,   /* DAG + Ca2+ combined */
        .camkii_activation_threshold = 0.5F,/* Ca2+/CaM level */
        .creb_phosphorylation_threshold = 0.4F, /* kinase activity */

        /* Gene expression */
        .ieg_induction_threshold = 0.6F,
        .protein_synthesis_delay_ms = 30000.0F, /* 30 seconds */

        /* Bio-async */
        .enable_bio_async = true,
        .broadcast_threshold = 0.1F,

        /* Security */
        .enable_security = true
    };
    return config;
}

/**
 * @brief Validate configuration
 */
nimcp_result_t second_messenger_validate_config(const second_messenger_config_t* config) {
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");
    NIMCP_CHECK_THROW(config->dt_ms > 0.0F && config->dt_ms <= 100.0F,
                      NIMCP_ERROR_INVALID_PARAM, "Invalid dt_ms: %.2f", config->dt_ms);
    NIMCP_CHECK_THROW(config->camp_synthesis_rate >= 0.0F && config->camp_degradation_rate >= 0.0F,
                      NIMCP_ERROR_INVALID_PARAM, "camp synthesis/degradation rates must be non-negative");
    NIMCP_CHECK_THROW(config->pka_activation_threshold > 0.0F,
                      NIMCP_ERROR_INVALID_PARAM, "pka_activation_threshold must be positive");
    return NIMCP_SUCCESS;
}

/**
 * @brief Create second messenger system
 */
second_messenger_system_t* second_messenger_create(
    uint32_t max_neurons,
    const second_messenger_config_t* config
) {
    /* Validate parameters */
    if (max_neurons == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "second_messenger_create: max_neurons must be > 0");
        LOG_MODULE_ERROR(LOG_MODULE, "max_neurons must be > 0");
        return NULL;
    }

    /* Use defaults if no config provided */
    second_messenger_config_t cfg = config ? *config : second_messenger_default_config();

    /* Validate configuration */
    if (second_messenger_validate_config(&cfg) != NIMCP_SUCCESS) {
        LOG_MODULE_ERROR(LOG_MODULE, "Invalid configuration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "second_messenger_create: validation failed");
        return NULL;
    }

    /* Allocate system structure */
    second_messenger_system_t* sys = nimcp_calloc(1, sizeof(second_messenger_system_t));
    if (!sys) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "second_messenger_create: failed to allocate system");
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to allocate system structure");
        return NULL;
    }

    /* Store configuration */
    sys->config = cfg;
    sys->max_neurons = max_neurons;

    /* Allocate state arrays */
    sys->states = nimcp_calloc(max_neurons, sizeof(second_messenger_state_t));
    if (!sys->states) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "second_messenger_create: failed to allocate states array");
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to allocate states array");
        nimcp_free(sys);
        return NULL;
    }

    sys->neuron_active = nimcp_calloc(max_neurons, sizeof(bool));
    if (!sys->neuron_active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "second_messenger_create: failed to allocate active flags");
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to allocate active flags");
        nimcp_free(sys->states);
        nimcp_free(sys);
        return NULL;
    }

    /* Initialize mutex */
    if (nimcp_mutex_init(&sys->mutex, NULL) == 0) {
        sys->mutex_initialized = true;
    } else {
        LOG_MODULE_WARN(LOG_MODULE, "Failed to initialize mutex - single-threaded mode");
    }

    /* Initialize all states to baseline */
    uint64_t now_ms = nimcp_platform_time_monotonic_ms();
    for (uint32_t i = 0; i < max_neurons; i++) {
        init_state_baseline(&sys->states[i], i, now_ms);
    }

    /* Mark security as enabled if requested */
    if (cfg.enable_security) {
        sys->security_registered = true;
        LOG_MODULE_DEBUG(LOG_MODULE, "Security validation enabled for second messengers");
    }

    LOG_MODULE_INFO(LOG_MODULE, "Created second messenger system for %u neurons", max_neurons);
    return sys;
}

/**
 * @brief Destroy second messenger system
 */
void second_messenger_destroy(second_messenger_system_t* system) {
    if (!system) {
        return;
    }

    /* Unregister bio-async if registered */
    if (system->bio_async_enabled && system->bio_ctx) {
        bio_router_unregister_module(system->bio_ctx);
        system->bio_async_enabled = false;
    }

    /* Destroy mutex */
    if (system->mutex_initialized) {
        nimcp_platform_mutex_destroy(&system->mutex);
    }

    /* Free arrays */
    if (system->neuron_active) {
        nimcp_free(system->neuron_active);
    }
    if (system->states) {
        nimcp_free(system->states);
    }

    LOG_MODULE_INFO(LOG_MODULE, "Destroyed second messenger system");
    nimcp_free(system);
}

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *============================================================================*/

/* ============================================================================
 * KG-Driven Wiring Callback
 * ============================================================================ */

/**
 * @brief Wiring callback for KG-driven handler registration
 *
 * WHAT: Register message handlers based on discovered wiring from KG
 * WHY:  Enables runtime assembly - module discovers its handlers from KG
 * HOW:  Orchestrator invokes this with message types from HANDLES_MESSAGE relations
 */
static int second_messenger_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    (void)user_data;

    if (!ctx || !message_types || message_count == 0) {
        return 0;
    }

    int registered = 0;
    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            case BIO_MSG_SECOND_MESSENGER_UPDATE:
                bio_router_register_handler(ctx, message_types[i], bio_message_handler);
                registered++;
                LOG_MODULE_DEBUG(LOG_MODULE, "  Registered handler for BIO_MSG_SECOND_MESSENGER_UPDATE");
                break;

            case BIO_MSG_NEUROMODULATOR_RELEASE:
                bio_router_register_handler(ctx, message_types[i], bio_message_handler);
                registered++;
                LOG_MODULE_DEBUG(LOG_MODULE, "  Registered handler for BIO_MSG_NEUROMODULATOR_RELEASE");
                break;

            default:
                LOG_MODULE_DEBUG(LOG_MODULE, "Second messenger: unknown message type %d in wiring callback",
                                 message_types[i]);
                break;
        }
    }

    return (registered > 0) ? 0 : -1;
}

/**
 * @brief Register with bio-async router
 */
nimcp_result_t second_messenger_register_bioasync(
    second_messenger_system_t* system,
    bio_router_t router
) {
    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    NIMCP_CHECK_THROW(router, NIMCP_ERROR_NULL_POINTER, "router is NULL");
    if (!bio_router_is_initialized()) {
        LOG_MODULE_DEBUG(LOG_MODULE, "Bio-router not initialized, skipping registration");
        return NIMCP_SUCCESS;
    }

    /* Create module info */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_SECOND_MESSENGER,
        .module_name = "second_messengers",
        .inbox_capacity = 64,
        .user_data = system
    };

    /* Register module */
    system->bio_ctx = bio_router_register_module(&info);
    if (!system->bio_ctx) {
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to register with bio-router");
        return NIMCP_ERROR_NOT_SUPPORTED;
    }

    /* Try KG-driven wiring callback registration first */
    nimcp_result_t wiring_result = bio_router_register_wiring_callback(
        BIO_MODULE_SECOND_MESSENGER,
        (void*)second_messenger_wiring_handler_callback,
        system
    );

    if (wiring_result == NIMCP_SUCCESS) {
        LOG_MODULE_INFO(LOG_MODULE, "Second messenger: KG-driven wiring callback registered");
    } else {
        /* Legacy fallback - register handlers directly */
        LEGACY_HANDLER_REGISTRATION(
            nimcp_result_t result = bio_router_register_handler(
                system->bio_ctx,
                BIO_MSG_SECOND_MESSENGER_UPDATE,
                bio_message_handler
            )
        );
        if (result != NIMCP_SUCCESS) {
            LOG_MODULE_WARN(LOG_MODULE, "Handler registration failed: %d", result);
        }

        /* Subscribe to neuromodulator release messages */
        LEGACY_HANDLER_REGISTRATION(
            bio_router_register_handler(system->bio_ctx, BIO_MSG_NEUROMODULATOR_RELEASE, bio_message_handler)
        );

        LOG_MODULE_INFO(LOG_MODULE, "Second messenger: legacy handler registration");
    }

    system->bio_async_enabled = true;
    LOG_MODULE_INFO(LOG_MODULE, "Registered with bio-async router");
    return NIMCP_SUCCESS;
}

/**
 * @brief Process bio-async inbox
 */
uint32_t second_messenger_process_inbox(second_messenger_system_t* system) {
    if (!system || !system->bio_async_enabled) {
        return 0;
    }

    uint32_t processed = bio_router_process_inbox(system->bio_ctx, 0);
    if (processed > 0) {
        system->stats.bio_async_messages_recv += processed;
    }
    return processed;
}

/**
 * @brief Broadcast cascade state
 */
nimcp_result_t second_messenger_broadcast_state(
    second_messenger_system_t* system,
    uint32_t neuron_id
) {
    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    if (!system->bio_async_enabled) {
        return NIMCP_SUCCESS; /* Silent success if bio-async disabled */
    }

    /* Build message */
    bio_msg_second_messenger_update_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header, BIO_MSG_SECOND_MESSENGER_UPDATE,
                        BIO_MODULE_SECOND_MESSENGER, BIO_MODULE_ALL,
                        sizeof(msg) - sizeof(bio_message_header_t));

    if (neuron_id == 0) {
        /* Broadcast aggregate state */
        msg.neuron_id = 0;
        msg.pka_activity = system->stats.avg_pka_activity;
        msg.pkc_activity = 0.0F;
        msg.camkii_activity = system->stats.avg_camkii_activity;
        msg.creb_phosphorylation = system->stats.avg_creb_phosphorylation;
    } else {
        NIMCP_CHECK_THROW(neuron_id < system->max_neurons && system->neuron_active[neuron_id],
                          NIMCP_ERROR_INVALID_PARAM, "Invalid neuron_id %u or neuron not active", neuron_id);
        /* Broadcast specific neuron state */
        second_messenger_state_t* state = &system->states[neuron_id];
        msg.neuron_id = neuron_id;
        msg.pka_activity = state->camp.pka_activity;
        msg.pkc_activity = state->ip3_dag.pkc_activity;
        msg.camkii_activity = state->calcium.camkii_activity;
        msg.creb_phosphorylation = state->gene_expr.creb_phosphorylation;
        msg.camp_concentration = state->camp.camp_concentration;
        msg.calcium_concentration = state->calcium.ca_cytoplasmic;
    }

    msg.header.timestamp_us = nimcp_platform_time_monotonic_us();

    /* Send broadcast */
    nimcp_result_t result = bio_router_broadcast(
        system->bio_ctx,
        &msg,
        sizeof(msg)
    );

    if (result == NIMCP_SUCCESS) {
        system->stats.bio_async_messages_sent++;
    }
    return result;
}

/**
 * @brief Bio-async message handler
 */
static nimcp_result_t bio_message_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    (void)response_promise; /* Unused for now */

    NIMCP_CHECK_THROW(msg, NIMCP_ERROR_NULL_POINTER, "msg is NULL");
    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    second_messenger_system_t* system = (second_messenger_system_t*)user_data;
    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    switch (header->type) {
        case BIO_MSG_NEUROMODULATOR_RELEASE: {
            /* Handle neuromodulator release -> cascade activation */
            NIMCP_CHECK_THROW(msg_size >= sizeof(bio_msg_neuromodulator_release_t),
                              NIMCP_ERROR_INVALID_SIZE, "msg_size too small for neuromodulator_release");
            const bio_msg_neuromodulator_release_t* nm_msg =
                (const bio_msg_neuromodulator_release_t*)msg;

            /* Map neuromodulator to receptor activation */
            uint64_t now_ms = header->timestamp_us / 1000;
            float occupancy = nm_msg->release_amount;

            /* Dopamine -> D1 (Gs) or D2 (Gi) depending on receptor density */
            if (nm_msg->neuromodulator == BIO_CHANNEL_DOPAMINE) {
                /* Assume predominantly D1 for now */
                second_messenger_activate_gs(system, nm_msg->source_region, occupancy, now_ms);
            }
            /* Serotonin -> 5-HT2A (Gq) */
            else if (nm_msg->neuromodulator == BIO_CHANNEL_SEROTONIN) {
                second_messenger_activate_gq(system, nm_msg->source_region, occupancy, now_ms);
            }
            /* Norepinephrine -> beta-AR (Gs) */
            else if (nm_msg->neuromodulator == BIO_CHANNEL_NOREPINEPHRINE) {
                second_messenger_activate_gs(system, nm_msg->source_region, occupancy * 0.5F, now_ms);
            }
            break;
        }

        case BIO_MSG_SECOND_MESSENGER_QUERY: {
            /* Handle state query */
            /* TODO: Implement query response */
            break;
        }

        default:
            break;
    }

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * RECEPTOR ACTIVATION IMPLEMENTATION
 *============================================================================*/

/**
 * @brief Activate receptor and trigger cascade
 */
nimcp_result_t second_messenger_activate_receptor(
    second_messenger_system_t* system,
    const receptor_activation_event_t* event
) {
    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    NIMCP_CHECK_THROW(event, NIMCP_ERROR_NULL_POINTER, "event is NULL");
    NIMCP_CHECK_THROW(event->neuron_id < system->max_neurons,
                      NIMCP_ERROR_INVALID_PARAM, "neuron_id %u >= max_neurons %u",
                      event->neuron_id, system->max_neurons);
    NIMCP_CHECK_THROW(event->occupancy >= 0.0F && event->occupancy <= 1.0F,
                      NIMCP_ERROR_INVALID_PARAM, "occupancy %.2f out of range [0,1]",
                      event->occupancy);

    /* Route to appropriate pathway based on coupling */
    switch (event->coupling) {
        case GPCR_GS_COUPLED:
            return second_messenger_activate_gs(system, event->neuron_id,
                                                event->occupancy, event->timestamp_ms);
        case GPCR_GI_COUPLED:
            return second_messenger_activate_gi(system, event->neuron_id,
                                                event->occupancy, event->timestamp_ms);
        case GPCR_GQ_COUPLED:
            return second_messenger_activate_gq(system, event->neuron_id,
                                                event->occupancy, event->timestamp_ms);
        default:
            LOG_MODULE_WARN(LOG_MODULE, "Unknown GPCR coupling type: %d", event->coupling);
            return NIMCP_ERROR_INVALID_PARAM;
    }
}

/**
 * @brief Activate Gs-coupled receptor
 */
nimcp_result_t second_messenger_activate_gs(
    second_messenger_system_t* system,
    uint32_t neuron_id,
    float occupancy,
    uint64_t timestamp_ms
) {
    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    NIMCP_CHECK_THROW(neuron_id < system->max_neurons,
                      NIMCP_ERROR_INVALID_PARAM, "neuron_id %u >= max_neurons %u",
                      neuron_id, system->max_neurons);

    /* Lock for thread safety */
    if (system->mutex_initialized) {
        nimcp_platform_mutex_lock(&system->mutex);
    }

    /* Ensure neuron state is initialized */
    if (!system->neuron_active[neuron_id]) {
        init_state_baseline(&system->states[neuron_id], neuron_id, timestamp_ms);
        system->neuron_active[neuron_id] = true;
        system->active_count++;
    }

    second_messenger_state_t* state = &system->states[neuron_id];

    /* Gs activation increases adenylyl cyclase activity */
    float ac_increase = occupancy * 0.2F; /* Scale factor */
    state->camp.adenylyl_cyclase_activity = nimcp_clampf(
        state->camp.adenylyl_cyclase_activity + ac_increase, 0.0F, 1.0F
    );

    state->camp.last_update_ms = timestamp_ms;
    state->last_update_ms = timestamp_ms;

    system->stats.receptor_activations++;

    if (system->mutex_initialized) {
        nimcp_platform_mutex_unlock(&system->mutex);
    }

    LOG_MODULE_DEBUG(LOG_MODULE, "Gs activation: neuron=%u, occupancy=%.2f, AC=%.3f",
                     neuron_id, occupancy, state->camp.adenylyl_cyclase_activity);
    return NIMCP_SUCCESS;
}

/**
 * @brief Activate Gi-coupled receptor
 */
nimcp_result_t second_messenger_activate_gi(
    second_messenger_system_t* system,
    uint32_t neuron_id,
    float occupancy,
    uint64_t timestamp_ms
) {
    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    NIMCP_CHECK_THROW(neuron_id < system->max_neurons,
                      NIMCP_ERROR_INVALID_PARAM, "neuron_id %u >= max_neurons %u",
                      neuron_id, system->max_neurons);

    if (system->mutex_initialized) {
        nimcp_platform_mutex_lock(&system->mutex);
    }

    if (!system->neuron_active[neuron_id]) {
        init_state_baseline(&system->states[neuron_id], neuron_id, timestamp_ms);
        system->neuron_active[neuron_id] = true;
        system->active_count++;
    }

    second_messenger_state_t* state = &system->states[neuron_id];

    /* Gi activation decreases adenylyl cyclase activity */
    float ac_decrease = occupancy * 0.15F;
    state->camp.adenylyl_cyclase_activity = nimcp_clampf(
        state->camp.adenylyl_cyclase_activity - ac_decrease, 0.0F, 1.0F
    );

    state->camp.last_update_ms = timestamp_ms;
    state->last_update_ms = timestamp_ms;

    system->stats.receptor_activations++;

    if (system->mutex_initialized) {
        nimcp_platform_mutex_unlock(&system->mutex);
    }

    LOG_MODULE_DEBUG(LOG_MODULE, "Gi activation: neuron=%u, occupancy=%.2f, AC=%.3f",
                     neuron_id, occupancy, state->camp.adenylyl_cyclase_activity);
    return NIMCP_SUCCESS;
}

/**
 * @brief Activate Gq-coupled receptor
 */
nimcp_result_t second_messenger_activate_gq(
    second_messenger_system_t* system,
    uint32_t neuron_id,
    float occupancy,
    uint64_t timestamp_ms
) {
    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    NIMCP_CHECK_THROW(neuron_id < system->max_neurons,
                      NIMCP_ERROR_INVALID_PARAM, "neuron_id %u >= max_neurons %u",
                      neuron_id, system->max_neurons);

    if (system->mutex_initialized) {
        nimcp_platform_mutex_lock(&system->mutex);
    }

    if (!system->neuron_active[neuron_id]) {
        init_state_baseline(&system->states[neuron_id], neuron_id, timestamp_ms);
        system->neuron_active[neuron_id] = true;
        system->active_count++;
    }

    second_messenger_state_t* state = &system->states[neuron_id];

    /* Gq activation increases phospholipase C activity */
    float plc_increase = occupancy * 0.25F;
    state->ip3_dag.phospholipase_c_activity = nimcp_clampf(
        state->ip3_dag.phospholipase_c_activity + plc_increase, 0.0F, 1.0F
    );

    state->ip3_dag.last_update_ms = timestamp_ms;
    state->last_update_ms = timestamp_ms;

    system->stats.receptor_activations++;

    if (system->mutex_initialized) {
        nimcp_platform_mutex_unlock(&system->mutex);
    }

    LOG_MODULE_DEBUG(LOG_MODULE, "Gq activation: neuron=%u, occupancy=%.2f, PLC=%.3f",
                     neuron_id, occupancy, state->ip3_dag.phospholipase_c_activity);
    return NIMCP_SUCCESS;
}

/*=============================================================================
 * CASCADE DYNAMICS IMPLEMENTATION
 *============================================================================*/

/**
 * @brief Update cascade dynamics for all neurons
 */
nimcp_result_t second_messenger_update(
    second_messenger_system_t* system,
    float dt_ms,
    uint64_t timestamp_ms
) {
    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    NIMCP_CHECK_THROW(dt_ms > 0.0F, NIMCP_ERROR_INVALID_PARAM, "dt_ms must be positive, got %.2f", dt_ms);

    if (system->mutex_initialized) {
        nimcp_platform_mutex_lock(&system->mutex);
    }

    /* Update aggregate statistics */
    float total_pka = 0.0F;
    float total_camkii = 0.0F;
    float total_creb = 0.0F;
    uint32_t active_count = 0;

    /* Update all active cascades */
    for (uint32_t i = 0; i < system->max_neurons; i++) {
        if (!system->neuron_active[i]) {
            continue;
        }

        second_messenger_state_t* state = &system->states[i];

        /* Update each pathway */
        update_camp_pathway(state, dt_ms);
        update_ip3_dag_pathway(state, dt_ms);
        update_calcium_signaling(state, dt_ms, &system->config);
        update_gene_expression(state, dt_ms, &system->config);
        update_integration_state(state);

        state->last_update_ms = timestamp_ms;

        /* Accumulate statistics */
        total_pka += state->camp.pka_activity;
        total_camkii += state->calcium.camkii_activity;
        total_creb += state->gene_expr.creb_phosphorylation;
        active_count++;

        /* Check for gene expression events */
        if (state->gene_expr.creb_phosphorylation > system->config.ieg_induction_threshold) {
            system->stats.gene_expression_events++;
        }

        /* Check for plasticity tag */
        if (state->gene_expr.ieg_levels[IEG_ARC] > 0.5F) {
            system->stats.plasticity_tags_set++;
        }
    }

    /* Update average statistics */
    if (active_count > 0) {
        system->stats.avg_pka_activity = total_pka / active_count;
        system->stats.avg_camkii_activity = total_camkii / active_count;
        system->stats.avg_creb_phosphorylation = total_creb / active_count;
    }

    system->stats.cascade_updates++;

    if (system->mutex_initialized) {
        nimcp_platform_mutex_unlock(&system->mutex);
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Update cascade for single neuron
 */
nimcp_result_t second_messenger_update_neuron(
    second_messenger_system_t* system,
    uint32_t neuron_id,
    float dt_ms,
    uint64_t timestamp_ms
) {
    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    NIMCP_CHECK_THROW(neuron_id < system->max_neurons,
                      NIMCP_ERROR_INVALID_PARAM, "neuron_id %u >= max_neurons %u",
                      neuron_id, system->max_neurons);
    if (!system->neuron_active[neuron_id]) {
        return NIMCP_SUCCESS; /* Nothing to update */
    }

    if (system->mutex_initialized) {
        nimcp_platform_mutex_lock(&system->mutex);
    }

    second_messenger_state_t* state = &system->states[neuron_id];

    update_camp_pathway(state, dt_ms);
    update_ip3_dag_pathway(state, dt_ms);
    update_calcium_signaling(state, dt_ms, &system->config);
    update_gene_expression(state, dt_ms, &system->config);
    update_integration_state(state);

    state->last_update_ms = timestamp_ms;
    system->stats.cascade_updates++;

    if (system->mutex_initialized) {
        nimcp_platform_mutex_unlock(&system->mutex);
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Inject external calcium
 */
nimcp_result_t second_messenger_inject_calcium(
    second_messenger_system_t* system,
    uint32_t neuron_id,
    float ca_nm,
    uint64_t timestamp_ms
) {
    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    NIMCP_CHECK_THROW(neuron_id < system->max_neurons,
                      NIMCP_ERROR_INVALID_PARAM, "neuron_id %u >= max_neurons %u",
                      neuron_id, system->max_neurons);
    NIMCP_CHECK_THROW(ca_nm >= 0.0F, NIMCP_ERROR_INVALID_PARAM, "ca_nm must be non-negative, got %.2f", ca_nm);

    if (system->mutex_initialized) {
        nimcp_platform_mutex_lock(&system->mutex);
    }

    if (!system->neuron_active[neuron_id]) {
        init_state_baseline(&system->states[neuron_id], neuron_id, timestamp_ms);
        system->neuron_active[neuron_id] = true;
        system->active_count++;
    }

    second_messenger_state_t* state = &system->states[neuron_id];

    /* Add calcium to cytoplasmic pool */
    state->calcium.ca_cytoplasmic = nimcp_clampf(
        state->calcium.ca_cytoplasmic + ca_nm,
        SM_CA_BASELINE_NM,
        SM_CA_MAX_NM
    );

    state->calcium.last_update_ms = timestamp_ms;
    state->last_update_ms = timestamp_ms;

    if (system->mutex_initialized) {
        nimcp_platform_mutex_unlock(&system->mutex);
    }

    LOG_MODULE_DEBUG(LOG_MODULE, "Calcium injection: neuron=%u, added=%.1fnM, total=%.1fnM",
                     neuron_id, ca_nm, state->calcium.ca_cytoplasmic);
    return NIMCP_SUCCESS;
}

/*=============================================================================
 * STATE QUERY IMPLEMENTATION
 *============================================================================*/

/**
 * @brief Get cascade state for neuron
 */
nimcp_result_t second_messenger_get_state(
    const second_messenger_system_t* system,
    uint32_t neuron_id,
    second_messenger_state_t* state
) {
    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_NULL_POINTER, "state is NULL");
    NIMCP_CHECK_THROW(neuron_id < system->max_neurons,
                      NIMCP_ERROR_INVALID_PARAM, "neuron_id %u >= max_neurons %u",
                      neuron_id, system->max_neurons);

    *state = system->states[neuron_id];
    return NIMCP_SUCCESS;
}

/**
 * @brief Get cascade effects on plasticity
 */
nimcp_result_t second_messenger_get_effects(
    const second_messenger_system_t* system,
    uint32_t neuron_id,
    cascade_effects_t* effects
) {
    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    NIMCP_CHECK_THROW(effects, NIMCP_ERROR_NULL_POINTER, "effects is NULL");
    NIMCP_CHECK_THROW(neuron_id < system->max_neurons,
                      NIMCP_ERROR_INVALID_PARAM, "neuron_id %u >= max_neurons %u",
                      neuron_id, system->max_neurons);

    memset(effects, 0, sizeof(cascade_effects_t));

    if (!system->neuron_active[neuron_id]) {
        /* Return baseline values */
        effects->ltp_threshold_modulation = 1.0F;
        effects->ltd_threshold_modulation = 1.0F;
        effects->stdp_window_modulation = 1.0F;
        effects->eligibility_trace_decay = 1.0F;
        effects->spike_threshold_modulation = 1.0F;
        effects->input_resistance_modulation = 1.0F;
        effects->afterhyperpolarization = 1.0F;
        effects->release_probability_mod = 1.0F;
        effects->postsynaptic_gain_mod = 1.0F;
        effects->nmda_modulation = 1.0F;
        return NIMCP_SUCCESS;
    }

    const second_messenger_state_t* state = &system->states[neuron_id];

    /* PKA: lowers LTP threshold, enhances L-type Ca channels */
    float pka = state->camp.pka_activity;
    effects->ltp_threshold_modulation = 1.0F - 0.3F * pka;  /* 0.7-1.0 */
    effects->nmda_modulation = 1.0F + 0.2F * pka;           /* 1.0-1.2 */

    /* PKC: affects release probability and postsynaptic gain */
    float pkc = state->ip3_dag.pkc_activity;
    effects->release_probability_mod = 1.0F + 0.3F * pkc;   /* 1.0-1.3 */
    effects->postsynaptic_gain_mod = 1.0F + 0.2F * pkc;     /* 1.0-1.2 */

    /* CaMKII: critical for LTP, affects AMPAR trafficking */
    float camkii = state->calcium.camkii_activity;
    effects->ltp_threshold_modulation -= 0.2F * camkii;     /* Further reduction */
    effects->stdp_window_modulation = 1.0F + 0.5F * camkii; /* Wider window */

    /* Clamp LTP threshold modulation */
    effects->ltp_threshold_modulation = nimcp_clampf(effects->ltp_threshold_modulation, 0.5F, 1.0F);

    /* LTD threshold: opposite direction from LTP */
    effects->ltd_threshold_modulation = 1.0F + 0.2F * (pka + camkii);

    /* Eligibility trace decay: PKA slows decay */
    effects->eligibility_trace_decay = 1.0F - 0.3F * pka;

    /* Excitability modulation */
    effects->spike_threshold_modulation = 1.0F - 0.1F * pka + 0.05F * pkc;
    effects->input_resistance_modulation = 1.0F + 0.1F * pkc;
    effects->afterhyperpolarization = 1.0F - 0.2F * pka;

    /* Plasticity tag based on Arc expression */
    effects->plasticity_tag_set = state->gene_expr.ieg_levels[IEG_ARC] > 0.5F;
    effects->bdnf_availability = state->gene_expr.ieg_levels[IEG_BDNF];

    return NIMCP_SUCCESS;
}

/**
 * @brief Get kinase activity
 */
float second_messenger_get_kinase_activity(
    const second_messenger_system_t* system,
    uint32_t neuron_id,
    kinase_type_t kinase
) {
    if (!system || neuron_id >= system->max_neurons) {
        return -1.0F;
    }
    if (!system->neuron_active[neuron_id]) {
        return 0.0F;
    }

    const second_messenger_state_t* state = &system->states[neuron_id];

    switch (kinase) {
        case KINASE_PKA:
            return state->camp.pka_activity;
        case KINASE_PKC:
            return state->ip3_dag.pkc_activity;
        case KINASE_CAMKII:
            return state->calcium.camkii_activity;
        case KINASE_MAPK:
            /* MAPK activity derived from combined inputs */
            return (state->camp.pka_activity + state->ip3_dag.pkc_activity) * 0.5F;
        default:
            return -1.0F;
    }
}

/**
 * @brief Get IEG expression level
 */
float second_messenger_get_ieg_level(
    const second_messenger_system_t* system,
    uint32_t neuron_id,
    ieg_type_t ieg
) {
    if (!system || neuron_id >= system->max_neurons) {
        return -1.0F;
    }
    if (ieg >= IEG_COUNT) {
        return -1.0F;
    }
    if (!system->neuron_active[neuron_id]) {
        return 0.0F;
    }

    return system->states[neuron_id].gene_expr.ieg_levels[ieg];
}

/**
 * @brief Check if plasticity tag is set
 */
bool second_messenger_is_tagged(
    const second_messenger_system_t* system,
    uint32_t neuron_id
) {
    if (!system || neuron_id >= system->max_neurons) {
        return false;
    }
    if (!system->neuron_active[neuron_id]) {
        return false;
    }

    /* Plasticity tag set when Arc and BDNF are sufficiently expressed */
    const gene_expression_t* ge = &system->states[neuron_id].gene_expr;
    return (ge->ieg_levels[IEG_ARC] > 0.5F) && (ge->creb_phosphorylation > 0.4F);
}

/*=============================================================================
 * INTEGRATION API IMPLEMENTATION
 *============================================================================*/

/**
 * @brief Integrate with neuromodulator system
 */
nimcp_result_t second_messenger_integrate_neuromodulator(
    second_messenger_system_t* system,
    neuromodulator_system_t neuromod
) {
    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_POINTER, "system is NULL");

    system->neuromod = neuromod;
    system->neuromod_connected = (neuromod != NULL);

    if (system->neuromod_connected) {
        LOG_MODULE_INFO(LOG_MODULE, "Integrated with neuromodulator system");
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Get plasticity modulation factor
 */
float second_messenger_get_plasticity_modulation(
    const second_messenger_system_t* system,
    uint32_t neuron_id
) {
    if (!system || neuron_id >= system->max_neurons) {
        return 1.0F;
    }
    if (!system->neuron_active[neuron_id]) {
        return 1.0F;
    }

    return system->states[neuron_id].plasticity_modulation;
}

/**
 * @brief Get excitability modulation factor
 */
float second_messenger_get_excitability_modulation(
    const second_messenger_system_t* system,
    uint32_t neuron_id
) {
    if (!system || neuron_id >= system->max_neurons) {
        return 1.0F;
    }
    if (!system->neuron_active[neuron_id]) {
        return 1.0F;
    }

    return system->states[neuron_id].excitability_modulation;
}

/*=============================================================================
 * RESET AND STATISTICS IMPLEMENTATION
 *============================================================================*/

/**
 * @brief Reset cascade state for neuron
 */
nimcp_result_t second_messenger_reset(
    second_messenger_system_t* system,
    uint32_t neuron_id
) {
    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_POINTER, "system is NULL");

    uint64_t now_ms = nimcp_platform_time_monotonic_ms();

    if (system->mutex_initialized) {
        nimcp_platform_mutex_lock(&system->mutex);
    }

    if (neuron_id == 0) {
        /* Reset all neurons */
        for (uint32_t i = 0; i < system->max_neurons; i++) {
            init_state_baseline(&system->states[i], i, now_ms);
            system->neuron_active[i] = false;
        }
        system->active_count = 0;
        LOG_MODULE_INFO(LOG_MODULE, "Reset all cascade states");
    } else {
        /* Validate neuron_id before reset */
        if (neuron_id >= system->max_neurons) {
            if (system->mutex_initialized) {
                nimcp_platform_mutex_unlock(&system->mutex);
            }
            NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "neuron_id %u >= max_neurons %u",
                        neuron_id, system->max_neurons);
            return NIMCP_ERROR_INVALID_PARAM;
        }
        /* Reset specific neuron */
        init_state_baseline(&system->states[neuron_id], neuron_id, now_ms);
        if (system->neuron_active[neuron_id]) {
            system->neuron_active[neuron_id] = false;
            system->active_count--;
        }
        LOG_MODULE_DEBUG(LOG_MODULE, "Reset cascade state for neuron %u", neuron_id);
    }

    if (system->mutex_initialized) {
        nimcp_platform_mutex_unlock(&system->mutex);
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Get system statistics
 */
nimcp_result_t second_messenger_get_stats(
    const second_messenger_system_t* system,
    second_messenger_stats_t* stats
) {
    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");

    *stats = system->stats;
    return NIMCP_SUCCESS;
}

/**
 * @brief Reset statistics
 */
nimcp_result_t second_messenger_reset_stats(second_messenger_system_t* system) {
    NIMCP_CHECK_THROW(system, NIMCP_ERROR_NULL_POINTER, "system is NULL");

    memset(&system->stats, 0, sizeof(second_messenger_stats_t));
    return NIMCP_SUCCESS;
}

/*=============================================================================
 * UTILITY FUNCTION IMPLEMENTATION
 *============================================================================*/

/**
 * @brief Get receptor coupling type
 */
gpcr_coupling_t second_messenger_receptor_coupling(receptor_type_t receptor) {
    switch (receptor) {
        case RECEPTOR_D1:
            return GPCR_GS_COUPLED;
        case RECEPTOR_D2:
            return GPCR_GI_COUPLED;
        case RECEPTOR_5HT1A:
            return GPCR_GI_COUPLED;
        case RECEPTOR_5HT2A:
            return GPCR_GQ_COUPLED;
        case RECEPTOR_NICOTINIC:
            return GPCR_GS_COUPLED; /* Technically ionotropic, but model as Gs */
        case RECEPTOR_MUSCARINIC:
            return GPCR_GQ_COUPLED; /* M1/M3 are Gq */
        case RECEPTOR_ALPHA1:
            return GPCR_GQ_COUPLED;
        case RECEPTOR_ALPHA2:
            return GPCR_GI_COUPLED;
        case RECEPTOR_BETA:
            return GPCR_GS_COUPLED;
        default:
            return GPCR_GS_COUPLED;
    }
}

/**
 * @brief Get kinase name
 */
const char* second_messenger_kinase_name(kinase_type_t kinase) {
    switch (kinase) {
        case KINASE_PKA:
            return "PKA";
        case KINASE_PKC:
            return "PKC";
        case KINASE_CAMKII:
            return "CaMKII";
        case KINASE_MAPK:
            return "MAPK/ERK";
        default:
            return "Unknown";
    }
}

/**
 * @brief Get IEG name
 */
const char* second_messenger_ieg_name(ieg_type_t ieg) {
    switch (ieg) {
        case IEG_CFOS:
            return "c-Fos";
        case IEG_ARC:
            return "Arc/Arg3.1";
        case IEG_BDNF:
            return "BDNF";
        case IEG_EGR1:
            return "Egr-1/Zif268";
        case IEG_HOMER1A:
            return "Homer1a";
        default:
            return "Unknown";
    }
}

/*=============================================================================
 * INTERNAL HELPER IMPLEMENTATIONS
 *============================================================================*/

/**
 * @brief Initialize state to baseline values
 */
static void init_state_baseline(second_messenger_state_t* state, uint32_t neuron_id,
                                uint64_t timestamp_ms) {
    memset(state, 0, sizeof(second_messenger_state_t));

    /* cAMP pathway baseline */
    state->camp.adenylyl_cyclase_activity = 0.1F;
    state->camp.camp_concentration = SM_CAMP_BASELINE_UM;
    state->camp.pde_activity = 0.5F;
    state->camp.pka_activity = 0.0F;
    state->camp.pka_catalytic_free = 0.0F;
    state->camp.last_update_ms = timestamp_ms;

    /* IP3/DAG pathway baseline */
    state->ip3_dag.phospholipase_c_activity = 0.05F;
    state->ip3_dag.ip3_concentration = SM_IP3_BASELINE_UM;
    state->ip3_dag.dag_concentration = SM_DAG_BASELINE;
    state->ip3_dag.ip3_receptor_open_prob = 0.0F;
    state->ip3_dag.pkc_activity = 0.0F;
    state->ip3_dag.last_update_ms = timestamp_ms;

    /* Calcium signaling baseline */
    state->calcium.ca_cytoplasmic = SM_CA_BASELINE_NM;
    state->calcium.ca_er_store = 0.8F;  /* ER mostly full */
    state->calcium.calmodulin_activation = 0.0F;
    state->calcium.camkii_activity = 0.0F;
    state->calcium.camkii_autophosphorylation = 0.0F;
    state->calcium.serca_activity = 0.5F;
    state->calcium.last_update_ms = timestamp_ms;

    /* Gene expression baseline */
    state->gene_expr.creb_phosphorylation = 0.0F;
    state->gene_expr.creb_activity = 0.0F;
    state->gene_expr.protein_synthesis_rate = 1.0F;
    state->gene_expr.last_expression_ms = timestamp_ms;
    for (int i = 0; i < IEG_COUNT; i++) {
        state->gene_expr.ieg_levels[i] = 0.0F;
    }

    /* Integration state */
    state->total_kinase_activity = 0.0F;
    state->plasticity_modulation = 1.0F;
    state->excitability_modulation = 1.0F;

    /* Metadata */
    state->neuron_id = neuron_id;
    state->created_ms = timestamp_ms;
    state->last_update_ms = timestamp_ms;
}

/**
 * @brief Update cAMP pathway dynamics
 */
static void update_camp_pathway(second_messenger_state_t* state, float dt_ms) {
    camp_pathway_t* camp = &state->camp;

    /* cAMP synthesis: AC activity -> cAMP production */
    float camp_synthesis = camp->adenylyl_cyclase_activity * 10.0F;  /* micromolar/s */

    /* cAMP degradation: PDE activity -> cAMP breakdown */
    float camp_degradation = camp->pde_activity * camp->camp_concentration * 0.5F;

    /* Update cAMP concentration (Euler integration) */
    float dcAMP = (camp_synthesis - camp_degradation) * (dt_ms / 1000.0F);
    camp->camp_concentration = nimcp_clampf(
        camp->camp_concentration + dcAMP,
        SM_CAMP_BASELINE_UM,
        SM_CAMP_MAX_UM
    );

    /* PKA activation (Hill function) */
    float camp_norm = camp->camp_concentration / 1.0F;  /* K_d = 1 micromolar */
    camp->pka_activity = hill_function(camp_norm, 1.0F, SM_HILL_PKA);

    /* Free catalytic subunits */
    camp->pka_catalytic_free = camp->pka_activity * 0.8F;

    /* AC decay back to baseline */
    float ac_decay = (camp->adenylyl_cyclase_activity - 0.1F) * dt_ms / SM_TAU_CAMP_SYNTHESIS;
    camp->adenylyl_cyclase_activity = nimcp_clampf(
        camp->adenylyl_cyclase_activity - ac_decay, 0.0F, 1.0F
    );
}

/**
 * @brief Update IP3/DAG pathway dynamics
 */
static void update_ip3_dag_pathway(second_messenger_state_t* state, float dt_ms) {
    ip3_dag_pathway_t* ip3dag = &state->ip3_dag;

    /* IP3 synthesis from PLC activity */
    float ip3_synthesis = ip3dag->phospholipase_c_activity * 20.0F;  /* micromolar/s */

    /* IP3 degradation */
    float ip3_degradation = ip3dag->ip3_concentration * 0.1F;

    /* Update IP3 concentration */
    float dIP3 = (ip3_synthesis - ip3_degradation) * (dt_ms / 1000.0F);
    ip3dag->ip3_concentration = nimcp_clampf(
        ip3dag->ip3_concentration + dIP3,
        SM_IP3_BASELINE_UM,
        SM_IP3_MAX_UM
    );

    /* DAG synthesis (parallels IP3) */
    float dag_synthesis = ip3dag->phospholipase_c_activity * 0.5F;
    float dag_degradation = ip3dag->dag_concentration * 0.1F;

    float dDAG = (dag_synthesis - dag_degradation) * (dt_ms / 1000.0F);
    ip3dag->dag_concentration = nimcp_clampf(
        ip3dag->dag_concentration + dDAG,
        SM_DAG_BASELINE,
        SM_DAG_MAX
    );

    /* IP3 receptor open probability */
    float ip3_norm = ip3dag->ip3_concentration / 0.5F;  /* K_d = 0.5 micromolar */
    ip3dag->ip3_receptor_open_prob = hill_function(ip3_norm, 1.0F, 2.0F);

    /* PKC activation (requires DAG + Ca2+) */
    float ca_norm = state->calcium.ca_cytoplasmic / 200.0F;  /* Ca2+ contribution */
    float pkc_input = ip3dag->dag_concentration * 0.7F + ca_norm * 0.3F;
    ip3dag->pkc_activity = hill_function(pkc_input, 0.3F, SM_HILL_PKC);

    /* PLC decay back to baseline */
    float plc_decay = (ip3dag->phospholipase_c_activity - 0.05F) * dt_ms / SM_TAU_IP3_SYNTHESIS;
    ip3dag->phospholipase_c_activity = nimcp_clampf(
        ip3dag->phospholipase_c_activity - plc_decay, 0.0F, 1.0F
    );
}

/**
 * @brief Update calcium signaling dynamics
 */
static void update_calcium_signaling(second_messenger_state_t* state, float dt_ms,
                                     const second_messenger_config_t* config) {
    calcium_signaling_t* ca = &state->calcium;
    (void)config;  /* Use config for future customization */

    /* Ca2+ release from ER via IP3 receptors */
    float ca_release = state->ip3_dag.ip3_receptor_open_prob * ca->ca_er_store * 100.0F;

    /* SERCA reuptake */
    float ca_reuptake = ca->serca_activity * (ca->ca_cytoplasmic - SM_CA_BASELINE_NM) * 0.1F;

    /* Update cytoplasmic calcium */
    float dCa = (ca_release - ca_reuptake) * (dt_ms / 1000.0F);
    ca->ca_cytoplasmic = nimcp_clampf(
        ca->ca_cytoplasmic + dCa,
        SM_CA_BASELINE_NM,
        SM_CA_MAX_NM
    );

    /* Update ER store (inverse of release) */
    float dER = (-ca_release * 0.01F + ca_reuptake * 0.01F) * (dt_ms / 1000.0F);
    ca->ca_er_store = nimcp_clampf(ca->ca_er_store + dER, 0.0F, 1.0F);

    /* Calmodulin activation */
    float ca_norm = (ca->ca_cytoplasmic - SM_CA_BASELINE_NM) /
                    (SM_CA_MAX_NM - SM_CA_BASELINE_NM);
    ca_norm = nimcp_clampf(ca_norm, 0.0F, 1.0F);
    float cam_target = hill_function(ca_norm, 0.3F, 4.0F);  /* Cooperative binding */

    float dCaM = (cam_target - ca->calmodulin_activation) * dt_ms / SM_TAU_CALMODULIN;
    ca->calmodulin_activation = nimcp_clampf(ca->calmodulin_activation + dCaM, 0.0F, 1.0F);

    /* CaMKII activation */
    float camkii_target = hill_function(ca->calmodulin_activation, 0.5F, SM_HILL_CAMKII);

    /* CaMKII autophosphorylation provides positive feedback */
    float autop_contribution = ca->camkii_autophosphorylation * 0.3F;
    camkii_target = nimcp_clampf(camkii_target + autop_contribution, 0.0F, 1.0F);

    float dCaMKII = (camkii_target - ca->camkii_activity) * dt_ms / SM_TAU_CAMKII;
    ca->camkii_activity = nimcp_clampf(ca->camkii_activity + dCaMKII, 0.0F, 1.0F);

    /* Autophosphorylation (bistable switch) */
    if (ca->camkii_activity > 0.6F) {
        float autop_rate = 0.01F * (ca->camkii_activity - 0.6F);
        ca->camkii_autophosphorylation = nimcp_clampf(
            ca->camkii_autophosphorylation + autop_rate * (dt_ms / 1000.0F),
            0.0F, 1.0F
        );
    } else {
        /* Slow decay of autophosphorylation */
        ca->camkii_autophosphorylation *= expf(-dt_ms / 60000.0F);  /* 1 minute decay */
    }
}

/**
 * @brief Update gene expression dynamics
 */
static void update_gene_expression(second_messenger_state_t* state, float dt_ms,
                                   const second_messenger_config_t* config) {
    gene_expression_t* ge = &state->gene_expr;

    /* Combined kinase input to CREB */
    float kinase_input = state->camp.pka_activity * 0.5F +
                         state->calcium.camkii_activity * 0.3F +
                         state->ip3_dag.pkc_activity * 0.2F;

    /* CREB phosphorylation (slow dynamics) */
    float creb_target = hill_function(kinase_input, config->creb_phosphorylation_threshold, 2.0F);
    float dCREB = (creb_target - ge->creb_phosphorylation) * dt_ms / SM_TAU_CREB_PHOS;
    ge->creb_phosphorylation = nimcp_clampf(ge->creb_phosphorylation + dCREB, 0.0F, 1.0F);

    /* CREB transcriptional activity (slightly delayed from phosphorylation) */
    float creb_delay = 0.01F;  /* Small delay factor */
    ge->creb_activity = ge->creb_phosphorylation * (1.0F - expf(-dt_ms * creb_delay));

    /* IEG induction (very slow dynamics - minutes scale) */
    if (ge->creb_phosphorylation > config->ieg_induction_threshold) {
        float induction_rate = (ge->creb_phosphorylation - config->ieg_induction_threshold) *
                               dt_ms / SM_TAU_IEG_EXPRESSION;

        /* c-Fos: fastest IEG */
        ge->ieg_levels[IEG_CFOS] = nimcp_clampf(
            ge->ieg_levels[IEG_CFOS] + induction_rate * 1.2F, 0.0F, 1.0F
        );

        /* Arc: slower, critical for plasticity */
        ge->ieg_levels[IEG_ARC] = nimcp_clampf(
            ge->ieg_levels[IEG_ARC] + induction_rate * 0.8F, 0.0F, 1.0F
        );

        /* BDNF: slowest, but most important for late-LTP */
        ge->ieg_levels[IEG_BDNF] = nimcp_clampf(
            ge->ieg_levels[IEG_BDNF] + induction_rate * 0.5F, 0.0F, 1.0F
        );

        /* Egr-1: memory consolidation */
        ge->ieg_levels[IEG_EGR1] = nimcp_clampf(
            ge->ieg_levels[IEG_EGR1] + induction_rate * 0.7F, 0.0F, 1.0F
        );

        /* Homer1a: mGluR modulation */
        ge->ieg_levels[IEG_HOMER1A] = nimcp_clampf(
            ge->ieg_levels[IEG_HOMER1A] + induction_rate * 0.6F, 0.0F, 1.0F
        );
    }

    /* IEG decay (mRNA half-life ~30-60 minutes) */
    float ieg_decay_rate = dt_ms / 1800000.0F;  /* 30 minute half-life */
    for (int i = 0; i < IEG_COUNT; i++) {
        ge->ieg_levels[i] *= (1.0F - ieg_decay_rate);
    }

    /* Protein synthesis rate affected by activity */
    ge->protein_synthesis_rate = 1.0F + 0.5F * ge->creb_phosphorylation;
}

/**
 * @brief Update integration state (combined effects)
 */
static void update_integration_state(second_messenger_state_t* state) {
    /* Total kinase activity */
    state->total_kinase_activity =
        state->camp.pka_activity * 0.35F +
        state->ip3_dag.pkc_activity * 0.30F +
        state->calcium.camkii_activity * 0.35F;

    /* Plasticity modulation: higher kinase activity = lower threshold for plasticity */
    /* Range: 0.5 (very enhanced) to 1.5 (suppressed) */
    state->plasticity_modulation = 1.0F - 0.5F * state->total_kinase_activity +
                                   0.2F * state->gene_expr.ieg_levels[IEG_ARC];
    state->plasticity_modulation = nimcp_clampf(state->plasticity_modulation, 0.5F, 1.5F);

    /* Excitability modulation: PKA increases, PKC can decrease */
    state->excitability_modulation = 1.0F +
                                     0.1F * state->camp.pka_activity -
                                     0.05F * state->ip3_dag.pkc_activity;
    state->excitability_modulation = nimcp_clampf(state->excitability_modulation, 0.8F, 1.2F);
}

/**
 * @brief Hill function for sigmoidal kinetics
 */
static float hill_function(float x, float k, float n) {
    if (x <= 0.0F) {
        return 0.0F;
    }
    float xn = powf(x, n);
    float kn = powf(k, n);
    return xn / (xn + kn);
}

