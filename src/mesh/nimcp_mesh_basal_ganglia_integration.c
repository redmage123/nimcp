/**
 * @file nimcp_mesh_basal_ganglia_integration.c
 * @brief Basal Ganglia Mesh Network Integration Implementation
 *
 * WHAT: Connects the basal ganglia module to the mesh network
 * WHY:  Enable coordinated action selection via distributed consensus
 * HOW:  Register as SUBCORTICAL participant, handle action/RL transactions
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 * @version 1.0.0
 */

#include "mesh/nimcp_mesh_basal_ganglia_integration.h"
#include "mesh/nimcp_mesh_bootstrap.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_coordinator_pool.h"
#include "mesh/nimcp_mesh_receptive_fields.h"
#include "mesh/nimcp_mesh_health_bridge.h"
#include "mesh/nimcp_mesh_exception_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>

#define LOG_MODULE "MESH_BASAL_GANGLIA"

/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define MESH_BG_MAGIC 0x42475354  /* "BGST" (Basal Ganglia STructure) */

/* Participant type for basal ganglia */
#define MESH_PARTICIPANT_BASAL_GANGLIA 0x0018

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct mesh_basal_ganglia_integration {
    uint32_t magic;

    /* Configuration */
    mesh_basal_ganglia_config_t config;

    /* Mesh components */
    mesh_bootstrap_t* bootstrap;
    mesh_integration_t* mesh_integration;
    mesh_participant_registry_t* registry;
    mesh_health_bridge_t* health_bridge;
    mesh_exception_bridge_t* exception_bridge;

    /* Participant registration */
    mesh_participant_id_t participant_id;
    mesh_participant_interface_t participant_interface;
    bool registered;

    /* Connected basal ganglia module */
    bg_enhanced_t* basal_ganglia;

    /* Health agent */
    nimcp_health_agent_t* health_agent;

    /* Statistics */
    mesh_basal_ganglia_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Transaction sequence counter */
    uint64_t tx_sequence;
};

/* ============================================================================
 * Forward Declarations for Participant Callbacks
 * ============================================================================ */

static nimcp_error_t bg_on_proposal(void* ctx, const mesh_transaction_t* tx);
static nimcp_error_t bg_on_endorse_request(void* ctx, const mesh_transaction_t* tx,
                                            mesh_endorsement_t* endorsement);
static nimcp_error_t bg_on_commit(void* ctx, const mesh_transaction_t* tx);
static float bg_get_free_energy(void* ctx);
static void bg_get_health_metrics(void* ctx, health_metrics_t* metrics);

/* ============================================================================
 * Internal Helper: Send Heartbeat
 * ============================================================================ */

static void bg_heartbeat(mesh_basal_ganglia_integration_t* integration,
                         const char* operation, float progress) {
    if (integration->health_agent) {
        extern void nimcp_health_agent_heartbeat_ex(nimcp_health_agent_t* agent,
                                                    const char* operation,
                                                    float progress);
        nimcp_health_agent_heartbeat_ex(integration->health_agent, operation, progress);
        integration->stats.health_heartbeats_sent++;
    }
}

/* ============================================================================
 * Participant Callback Implementations
 * ============================================================================ */

static nimcp_error_t bg_on_proposal(void* ctx, const mesh_transaction_t* tx) {
    mesh_basal_ganglia_integration_t* integration = (mesh_basal_ganglia_integration_t*)ctx;

    if (!integration || integration->magic != MESH_BG_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!tx) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    integration->stats.transactions_received++;

    if (!mesh_basal_ganglia_is_bg_transaction(tx->type)) {
        return NIMCP_SUCCESS;
    }

    bg_heartbeat(integration, "proposal_received", 0.5f);

    return NIMCP_SUCCESS;
}

static nimcp_error_t bg_on_endorse_request(void* ctx, const mesh_transaction_t* tx,
                                            mesh_endorsement_t* endorsement) {
    mesh_basal_ganglia_integration_t* integration = (mesh_basal_ganglia_integration_t*)ctx;

    if (!integration || integration->magic != MESH_BG_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!tx || !endorsement) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    integration->stats.endorsement_requests_handled++;

    memset(endorsement, 0, sizeof(*endorsement));
    endorsement->endorser_id = integration->participant_id;
    endorsement->timestamp_ns = nimcp_time_now_ns();

    if (mesh_basal_ganglia_is_bg_transaction(tx->type)) {
        endorsement->result = ENDORSEMENT_APPROVED;
        integration->stats.transactions_endorsed++;
    } else {
        endorsement->result = ENDORSEMENT_ABSTAIN;
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t bg_on_commit(void* ctx, const mesh_transaction_t* tx) {
    mesh_basal_ganglia_integration_t* integration = (mesh_basal_ganglia_integration_t*)ctx;

    if (!integration || integration->magic != MESH_BG_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!tx) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!mesh_basal_ganglia_is_bg_transaction(tx->type)) {
        return NIMCP_SUCCESS;
    }

    nimcp_mutex_lock(integration->mutex);

    switch ((mesh_basal_ganglia_tx_type_t)tx->type) {
        case MESH_TX_BG_ACTION_SELECT:
            integration->stats.actions_selected++;
            integration->stats.last_action_ns = nimcp_time_now_ns();
            break;

        case MESH_TX_BG_ACTION_INHIBIT:
            integration->stats.actions_inhibited++;
            break;

        case MESH_TX_BG_RPE_SIGNAL:
            integration->stats.rpe_signals_sent++;
            integration->stats.last_rpe_ns = nimcp_time_now_ns();
            if (tx->payload_size >= sizeof(float)) {
                integration->stats.current_rpe = *(const float*)tx->payload;
            }
            break;

        case MESH_TX_BG_DOPAMINE_BURST:
            integration->stats.dopamine_bursts++;
            if (tx->payload_size >= sizeof(float)) {
                integration->stats.current_dopamine = *(const float*)tx->payload;
            }
            break;

        case MESH_TX_BG_GO_PATHWAY:
            integration->stats.go_activations++;
            break;

        case MESH_TX_BG_NOGO_PATHWAY:
            integration->stats.nogo_activations++;
            break;

        case MESH_TX_BG_HABIT_UPDATE:
            integration->stats.learning_updates++;
            break;

        default:
            break;
    }

    nimcp_mutex_unlock(integration->mutex);

    bg_heartbeat(integration, "commit_processed", 1.0f);

    return NIMCP_SUCCESS;
}

static float bg_get_free_energy(void* ctx) {
    mesh_basal_ganglia_integration_t* integration = (mesh_basal_ganglia_integration_t*)ctx;

    if (!integration || integration->magic != MESH_BG_MAGIC) {
        return -1.0f;
    }

    /* Free energy based on action selection success and dopamine levels */
    float action_energy = 0.3f;
    float dopamine_energy = 0.3f;

    /* Higher dopamine = lower prediction error = lower free energy */
    float da = integration->stats.current_dopamine;
    if (da > 0.0f) {
        dopamine_energy = 0.5f - (da * 0.4f);
        if (dopamine_energy < 0.1f) dopamine_energy = 0.1f;
    }

    /* RPE magnitude indicates prediction error */
    float rpe = integration->stats.current_rpe;
    if (rpe < 0) rpe = -rpe;
    action_energy = rpe * 0.5f;
    if (action_energy > 0.9f) action_energy = 0.9f;

    return (action_energy + dopamine_energy) / 2.0f;
}

static void bg_get_health_metrics(void* ctx, health_metrics_t* metrics) {
    mesh_basal_ganglia_integration_t* integration = (mesh_basal_ganglia_integration_t*)ctx;

    if (!integration || integration->magic != MESH_BG_MAGIC || !metrics) {
        return;
    }

    memset(metrics, 0, sizeof(*metrics));
    metrics->participant = integration->participant_id;
    metrics->last_heartbeat_ns = nimcp_time_now_ns();
    metrics->is_healthy = true;
    metrics->avg_latency_ms = 1.5f;
    metrics->transactions_processed = integration->stats.transactions_received;
    metrics->transactions_failed = integration->stats.actions_inhibited;
}

/* ============================================================================
 * Configuration Defaults
 * ============================================================================ */

nimcp_error_t mesh_basal_ganglia_default_config(mesh_basal_ganglia_config_t* config) {
    if (!config) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    memset(config, 0, sizeof(*config));

    config->require_consensus_for_action = false;  /* Actions need to be fast */
    config->broadcast_rpe_signals = true;
    config->enable_distributed_learning = true;

    config->action_selection_timeout_ms = 100;  /* Fast action selection */
    config->learning_timeout_ms = 1000;

    config->enable_health_monitoring = true;
    config->heartbeat_interval_ms = 1000;

    config->verbose_logging = false;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

mesh_basal_ganglia_integration_t* mesh_basal_ganglia_create(
    mesh_bootstrap_t* bootstrap,
    bg_enhanced_t* basal_ganglia,
    const mesh_basal_ganglia_config_t* config
) {
    if (!bootstrap) {
        LOG_ERROR("Cannot create basal ganglia integration without bootstrap");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "mesh_basal_ganglia_create: bootstrap is NULL");
        return NULL;
    }

    mesh_basal_ganglia_config_t default_config;
    if (!config) {
        mesh_basal_ganglia_default_config(&default_config);
        config = &default_config;
    }

    mesh_basal_ganglia_integration_t* integration = nimcp_calloc(1, sizeof(*integration));
    if (!integration) {
        LOG_ERROR("Failed to allocate basal ganglia integration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_MEMORY,
            "mesh_basal_ganglia_create: allocation failed");
        return NULL;
    }

    integration->magic = MESH_BG_MAGIC;
    integration->config = *config;
    integration->bootstrap = bootstrap;
    integration->mesh_integration = mesh_bootstrap_get_integration(bootstrap);
    integration->registry = mesh_bootstrap_get_registry(bootstrap);
    integration->health_bridge = mesh_bootstrap_get_health_bridge(bootstrap);
    integration->exception_bridge = mesh_bootstrap_get_exception_bridge(bootstrap);
    integration->basal_ganglia = basal_ganglia;

    mutex_attr_t attr = {0};
    integration->mutex = nimcp_mutex_create(&attr);
    if (!integration->mutex) {
        LOG_ERROR("Failed to create basal ganglia integration mutex");
        nimcp_free(integration);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_basal_ganglia_create: integration->mutex is NULL");
        return NULL;
    }

    LOG_INFO("Created basal ganglia mesh integration");

    return integration;
}

void mesh_basal_ganglia_destroy(mesh_basal_ganglia_integration_t* integration) {
    if (!integration || integration->magic != MESH_BG_MAGIC) {
        return;
    }

    nimcp_mutex_lock(integration->mutex);

    if (integration->registered && integration->registry) {
        mesh_participant_unregister(integration->registry, integration->participant_id);
        integration->registered = false;
    }

    integration->magic = 0;

    nimcp_mutex_unlock(integration->mutex);
    nimcp_mutex_destroy(integration->mutex);

    nimcp_free(integration);

    LOG_INFO("Destroyed basal ganglia mesh integration");
}

/* ============================================================================
 * Participant Registration API
 * ============================================================================ */

nimcp_error_t mesh_basal_ganglia_register_participant(mesh_basal_ganglia_integration_t* integration) {
    if (!integration || integration->magic != MESH_BG_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);

    if (integration->registered) {
        nimcp_mutex_unlock(integration->mutex);
        LOG_WARN("Basal ganglia already registered with mesh");
        return NIMCP_ERROR_ALREADY_EXISTS;
    }

    if (!integration->registry) {
        nimcp_mutex_unlock(integration->mutex);
        LOG_ERROR("No participant registry available");
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    bg_heartbeat(integration, "registering", 0.0f);

    mesh_participant_interface_init(&integration->participant_interface);

    strncpy(integration->participant_interface.module_name, "basal_ganglia",
            MESH_MAX_NAME_LEN - 1);
    integration->participant_interface.type = MESH_PARTICIPANT_MODULE;
    integration->participant_interface.home_channel = MESH_CHANNEL_SUBCORTICAL;

    integration->participant_interface.on_proposal = bg_on_proposal;
    integration->participant_interface.on_endorse_request = bg_on_endorse_request;
    integration->participant_interface.on_commit = bg_on_commit;
    integration->participant_interface.get_free_energy = bg_get_free_energy;
    integration->participant_interface.get_health_metrics = bg_get_health_metrics;
    integration->participant_interface.user_context = integration;

    mesh_participant_config_t reg_config;
    mesh_participant_config_init(&reg_config);
    reg_config.module_name = "basal_ganglia";
    reg_config.type = MESH_PARTICIPANT_MODULE;
    reg_config.home_channel = MESH_CHANNEL_SUBCORTICAL;
    reg_config.user_context = integration;

    nimcp_error_t err = mesh_participant_register(
        integration->registry,
        &integration->participant_interface,
        &reg_config,
        &integration->participant_id
    );

    if (err != NIMCP_SUCCESS) {
        nimcp_mutex_unlock(integration->mutex);
        LOG_ERROR("Failed to register basal ganglia with mesh: %d", err);
        NIMCP_THROW_TO_IMMUNE(err, "mesh_basal_ganglia_register_participant: registration failed");
        return err;
    }

    integration->registered = true;

    if (integration->health_bridge && integration->health_agent) {
        mesh_health_bridge_register_agent(
            integration->health_bridge,
            integration->participant_id,
            integration->health_agent
        );
    }

    nimcp_mutex_unlock(integration->mutex);

    LOG_INFO("Basal ganglia registered with mesh: participant_id=0x%016lX",
             (unsigned long)integration->participant_id);

    bg_heartbeat(integration, "registered", 1.0f);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_basal_ganglia_unregister_participant(mesh_basal_ganglia_integration_t* integration) {
    if (!integration || integration->magic != MESH_BG_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);

    if (!integration->registered) {
        nimcp_mutex_unlock(integration->mutex);
        return NIMCP_SUCCESS;
    }

    if (integration->health_bridge) {
        mesh_health_bridge_unregister_agent(
            integration->health_bridge,
            integration->participant_id
        );
    }

    if (integration->registry) {
        mesh_participant_unregister(integration->registry, integration->participant_id);
    }

    integration->registered = false;
    integration->participant_id = 0;

    nimcp_mutex_unlock(integration->mutex);

    LOG_INFO("Basal ganglia unregistered from mesh");

    return NIMCP_SUCCESS;
}

mesh_participant_id_t mesh_basal_ganglia_get_participant_id(
    const mesh_basal_ganglia_integration_t* integration
) {
    if (!integration || integration->magic != MESH_BG_MAGIC) {
        return 0;
    }
    return integration->participant_id;
}

bool mesh_basal_ganglia_is_registered(const mesh_basal_ganglia_integration_t* integration) {
    if (!integration || integration->magic != MESH_BG_MAGIC) {
        return false;
    }
    return integration->registered;
}

/* ============================================================================
 * Action Selection API
 * ============================================================================ */

nimcp_error_t mesh_basal_ganglia_propose_action(
    mesh_basal_ganglia_integration_t* integration,
    uint32_t action_id,
    float confidence
) {
    if (!integration || integration->magic != MESH_BG_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!integration->registered) {
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    nimcp_mutex_lock(integration->mutex);

    integration->stats.actions_proposed++;

    struct {
        uint32_t action_id;
        float confidence;
    } payload = { action_id, confidence };

    mesh_transaction_t* tx = mesh_transaction_create(
        (mesh_tx_type_t)MESH_TX_BG_ACTION_SELECT,
        integration->participant_id,
        MESH_CHANNEL_SUBCORTICAL
    );

    if (!tx) {
        nimcp_mutex_unlock(integration->mutex);
        return NIMCP_ERROR_OUT_OF_MEMORY;
    }

    mesh_transaction_set_payload(tx, &payload, sizeof(payload));
    mesh_transaction_set_timeout(tx, integration->config.action_selection_timeout_ms);

    nimcp_error_t err = NIMCP_SUCCESS;
    if (integration->mesh_integration) {
        err = mesh_integration_submit_transaction(integration->mesh_integration, tx);
    }

    mesh_transaction_destroy(tx);
    nimcp_mutex_unlock(integration->mutex);

    bg_heartbeat(integration, "action_proposed", 1.0f);

    return err;
}

nimcp_error_t mesh_basal_ganglia_report_rpe(
    mesh_basal_ganglia_integration_t* integration,
    float rpe_value
) {
    if (!integration || integration->magic != MESH_BG_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!integration->registered) {
        return NIMCP_ERROR_NOT_INITIALIZED;
    }
    if (!integration->config.broadcast_rpe_signals) {
        return NIMCP_SUCCESS;  /* Broadcasting disabled */
    }

    nimcp_mutex_lock(integration->mutex);

    mesh_transaction_t* tx = mesh_transaction_create(
        (mesh_tx_type_t)MESH_TX_BG_RPE_SIGNAL,
        integration->participant_id,
        MESH_CHANNEL_SUBCORTICAL
    );

    if (!tx) {
        nimcp_mutex_unlock(integration->mutex);
        return NIMCP_ERROR_OUT_OF_MEMORY;
    }

    mesh_transaction_set_payload(tx, &rpe_value, sizeof(rpe_value));

    nimcp_error_t err = NIMCP_SUCCESS;
    if (integration->mesh_integration) {
        err = mesh_integration_submit_transaction(integration->mesh_integration, tx);
    }

    mesh_transaction_destroy(tx);
    nimcp_mutex_unlock(integration->mutex);

    return err;
}

nimcp_error_t mesh_basal_ganglia_report_dopamine(
    mesh_basal_ganglia_integration_t* integration,
    float dopamine_level
) {
    if (!integration || integration->magic != MESH_BG_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!integration->registered) {
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    nimcp_mutex_lock(integration->mutex);

    mesh_transaction_t* tx = mesh_transaction_create(
        (mesh_tx_type_t)MESH_TX_BG_DOPAMINE_BURST,
        integration->participant_id,
        MESH_CHANNEL_SUBCORTICAL
    );

    if (!tx) {
        nimcp_mutex_unlock(integration->mutex);
        return NIMCP_ERROR_OUT_OF_MEMORY;
    }

    mesh_transaction_set_payload(tx, &dopamine_level, sizeof(dopamine_level));

    nimcp_error_t err = NIMCP_SUCCESS;
    if (integration->mesh_integration) {
        err = mesh_integration_submit_transaction(integration->mesh_integration, tx);
    }

    mesh_transaction_destroy(tx);
    nimcp_mutex_unlock(integration->mutex);

    return err;
}

/* ============================================================================
 * Statistics API
 * ============================================================================ */

nimcp_error_t mesh_basal_ganglia_get_stats(
    const mesh_basal_ganglia_integration_t* integration,
    mesh_basal_ganglia_stats_t* stats
) {
    if (!integration || integration->magic != MESH_BG_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!stats) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(((mesh_basal_ganglia_integration_t*)integration)->mutex);
    *stats = integration->stats;
    nimcp_mutex_unlock(((mesh_basal_ganglia_integration_t*)integration)->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_basal_ganglia_reset_stats(mesh_basal_ganglia_integration_t* integration) {
    if (!integration || integration->magic != MESH_BG_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);
    memset(&integration->stats, 0, sizeof(integration->stats));
    nimcp_mutex_unlock(integration->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Health Agent Integration API
 * ============================================================================ */

nimcp_error_t mesh_basal_ganglia_set_health_agent(
    mesh_basal_ganglia_integration_t* integration,
    nimcp_health_agent_t* agent
) {
    if (!integration || integration->magic != MESH_BG_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);
    integration->health_agent = agent;
    nimcp_mutex_unlock(integration->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

bool mesh_basal_ganglia_is_bg_transaction(mesh_tx_type_t tx_type) {
    return tx_type >= MESH_TX_BG_BASE &&
           tx_type <= MESH_TX_BG_BETA_SYNC;
}
