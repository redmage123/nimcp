/**
 * @file nimcp_mesh_thalamus_integration.c
 * @brief Thalamus Mesh Network Integration Implementation
 *
 * WHAT: Connects the thalamus module to the mesh network
 * WHY:  Enable coordinated sensory relay and attention gating via distributed consensus
 * HOW:  Register as SUBCORTICAL participant, handle relay/attention transactions
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 * @version 1.0.0
 */

#include "mesh/nimcp_mesh_thalamus_integration.h"
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

#define LOG_MODULE "MESH_THALAMUS"

/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define MESH_THALAMUS_MAGIC 0x5448414C  /* "THAL" */

/* Participant type for thalamus */
#define MESH_PARTICIPANT_THALAMUS 0x0019

/* ============================================================================
 * String Conversion Tables
 * ============================================================================ */

static const char* firing_mode_names[] = {
    [MESH_THALAMUS_MODE_TONIC] = "tonic",
    [MESH_THALAMUS_MODE_BURST] = "burst",
};

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct mesh_thalamus_integration {
    uint32_t magic;

    /* Configuration */
    mesh_thalamus_config_t config;

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

    /* Connected thalamus module */
    thalamus_t* thalamus;

    /* Current state */
    float current_arousal;
    float current_attention;
    mesh_thalamus_firing_mode_t current_mode;

    /* Health agent */
    nimcp_health_agent_t* health_agent;

    /* Statistics */
    mesh_thalamus_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Transaction sequence counter */
    uint64_t tx_sequence;
};

/* ============================================================================
 * Forward Declarations for Participant Callbacks
 * ============================================================================ */

static nimcp_error_t thalamus_on_proposal(void* ctx, const mesh_transaction_t* tx);
static nimcp_error_t thalamus_on_endorse_request(void* ctx, const mesh_transaction_t* tx,
                                                  mesh_endorsement_t* endorsement);
static nimcp_error_t thalamus_on_commit(void* ctx, const mesh_transaction_t* tx);
static float thalamus_get_free_energy(void* ctx);
static void thalamus_get_health_metrics(void* ctx, health_metrics_t* metrics);

/* ============================================================================
 * Internal Helper: Send Heartbeat
 * ============================================================================ */

static void thalamus_heartbeat(mesh_thalamus_integration_t* integration,
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

static nimcp_error_t thalamus_on_proposal(void* ctx, const mesh_transaction_t* tx) {
    mesh_thalamus_integration_t* integration = (mesh_thalamus_integration_t*)ctx;

    if (!integration || integration->magic != MESH_THALAMUS_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!tx) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    integration->stats.transactions_received++;

    if (!mesh_thalamus_is_thalamus_transaction(tx->type)) {
        return NIMCP_SUCCESS;
    }

    thalamus_heartbeat(integration, "proposal_received", 0.5f);

    return NIMCP_SUCCESS;
}

static nimcp_error_t thalamus_on_endorse_request(void* ctx, const mesh_transaction_t* tx,
                                                  mesh_endorsement_t* endorsement) {
    mesh_thalamus_integration_t* integration = (mesh_thalamus_integration_t*)ctx;

    if (!integration || integration->magic != MESH_THALAMUS_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!tx || !endorsement) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    integration->stats.endorsement_requests_handled++;

    memset(endorsement, 0, sizeof(*endorsement));
    endorsement->endorser_id = integration->participant_id;
    endorsement->timestamp_ns = nimcp_time_now_ns();

    if (mesh_thalamus_is_thalamus_transaction(tx->type)) {
        endorsement->result = ENDORSEMENT_APPROVED;
        integration->stats.transactions_endorsed++;
    } else {
        /* Thalamus can gate transactions based on attention/arousal */
        if (integration->current_attention < 0.3f &&
            integration->current_arousal < 0.3f) {
            /* Low attention + low arousal = gate closed */
            endorsement->result = ENDORSEMENT_ABSTAIN;
        } else {
            endorsement->result = ENDORSEMENT_APPROVED;
            integration->stats.transactions_endorsed++;
        }
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t thalamus_on_commit(void* ctx, const mesh_transaction_t* tx) {
    mesh_thalamus_integration_t* integration = (mesh_thalamus_integration_t*)ctx;

    if (!integration || integration->magic != MESH_THALAMUS_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!tx) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!mesh_thalamus_is_thalamus_transaction(tx->type)) {
        return NIMCP_SUCCESS;
    }

    nimcp_mutex_lock(integration->mutex);

    integration->stats.last_relay_ns = nimcp_time_now_ns();

    switch ((mesh_thalamus_tx_type_t)tx->type) {
        case MESH_TX_THALAMUS_RELAY_VISUAL:
            integration->stats.visual_relays++;
            break;

        case MESH_TX_THALAMUS_RELAY_AUDITORY:
            integration->stats.auditory_relays++;
            break;

        case MESH_TX_THALAMUS_RELAY_SOMATO:
            integration->stats.somatosensory_relays++;
            break;

        case MESH_TX_THALAMUS_RELAY_MOTOR:
            integration->stats.motor_relays++;
            break;

        case MESH_TX_THALAMUS_RELAY_EXECUTIVE:
            integration->stats.executive_relays++;
            break;

        case MESH_TX_THALAMUS_ATTENTION_UPDATE:
            integration->stats.attention_updates++;
            if (tx->payload_size >= sizeof(float)) {
                integration->current_attention = *(const float*)tx->payload;
                integration->stats.current_attention = integration->current_attention;
            }
            break;

        case MESH_TX_THALAMUS_TRN_INHIBIT:
            integration->stats.trn_inhibitions++;
            break;

        case MESH_TX_THALAMUS_AROUSAL_CHANGE:
            integration->stats.arousal_changes++;
            integration->stats.last_arousal_change_ns = nimcp_time_now_ns();
            if (tx->payload_size >= sizeof(mesh_thalamus_arousal_payload_t)) {
                const mesh_thalamus_arousal_payload_t* payload =
                    (const mesh_thalamus_arousal_payload_t*)tx->payload;
                integration->current_arousal = payload->new_arousal;
                integration->current_mode = payload->resulting_mode;
                integration->stats.current_arousal = payload->new_arousal;
                integration->stats.current_mode = payload->resulting_mode;
            }
            break;

        case MESH_TX_THALAMUS_MODE_CHANGE:
            integration->stats.mode_changes++;
            break;

        case MESH_TX_THALAMUS_BURST_TRIGGER:
            integration->stats.burst_triggers++;
            break;

        default:
            break;
    }

    nimcp_mutex_unlock(integration->mutex);

    thalamus_heartbeat(integration, "commit_processed", 1.0f);

    return NIMCP_SUCCESS;
}

static float thalamus_get_free_energy(void* ctx) {
    mesh_thalamus_integration_t* integration = (mesh_thalamus_integration_t*)ctx;

    if (!integration || integration->magic != MESH_THALAMUS_MAGIC) {
        return -1.0f;
    }

    /* Free energy based on gating efficiency */
    /* Optimal: high attention + moderate arousal */
    float attention_energy = 1.0f - integration->current_attention;
    float arousal_energy = 0.0f;

    /* Arousal energy is U-shaped - optimal around 0.5-0.7 */
    float arousal = integration->current_arousal;
    if (arousal < 0.3f) {
        arousal_energy = 0.3f - arousal;  /* Too low */
    } else if (arousal > 0.8f) {
        arousal_energy = arousal - 0.8f;  /* Too high */
    } else {
        arousal_energy = 0.1f;  /* Optimal range */
    }

    return (attention_energy * 0.6f + arousal_energy * 0.4f);
}

static void thalamus_get_health_metrics(void* ctx, health_metrics_t* metrics) {
    mesh_thalamus_integration_t* integration = (mesh_thalamus_integration_t*)ctx;

    if (!integration || integration->magic != MESH_THALAMUS_MAGIC || !metrics) {
        return;
    }

    memset(metrics, 0, sizeof(*metrics));
    metrics->participant = integration->participant_id;
    metrics->last_heartbeat_ns = nimcp_time_now_ns();
    metrics->is_healthy = integration->current_arousal > 0.1f;  /* Not comatose */
    metrics->avg_latency_ms = 0.8f;  /* Thalamus is fast relay */
    metrics->transactions_processed = integration->stats.transactions_received;
    metrics->transactions_failed = 0;
}

/* ============================================================================
 * Configuration Defaults
 * ============================================================================ */

nimcp_error_t mesh_thalamus_default_config(mesh_thalamus_config_t* config) {
    if (!config) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    memset(config, 0, sizeof(*config));

    config->broadcast_relay_activity = false;  /* Would be too noisy */
    config->enable_distributed_gating = true;
    config->sync_arousal_with_mesh = true;

    config->relay_timeout_ms = 50;     /* Fast relay */
    config->attention_timeout_ms = 100;

    config->enable_health_monitoring = true;
    config->heartbeat_interval_ms = 500;  /* Faster heartbeat */

    config->verbose_logging = false;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

mesh_thalamus_integration_t* mesh_thalamus_create(
    mesh_bootstrap_t* bootstrap,
    thalamus_t* thalamus,
    const mesh_thalamus_config_t* config
) {
    if (!bootstrap) {
        LOG_ERROR("Cannot create thalamus integration without bootstrap");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "mesh_thalamus_create: bootstrap is NULL");
        return NULL;
    }

    mesh_thalamus_config_t default_config;
    if (!config) {
        mesh_thalamus_default_config(&default_config);
        config = &default_config;
    }

    mesh_thalamus_integration_t* integration = nimcp_calloc(1, sizeof(*integration));
    if (!integration) {
        LOG_ERROR("Failed to allocate thalamus integration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_MEMORY,
            "mesh_thalamus_create: allocation failed");
        return NULL;
    }

    integration->magic = MESH_THALAMUS_MAGIC;
    integration->config = *config;
    integration->bootstrap = bootstrap;
    integration->mesh_integration = mesh_bootstrap_get_integration(bootstrap);
    integration->registry = mesh_bootstrap_get_registry(bootstrap);
    integration->health_bridge = mesh_bootstrap_get_health_bridge(bootstrap);
    integration->exception_bridge = mesh_bootstrap_get_exception_bridge(bootstrap);
    integration->thalamus = thalamus;

    /* Set initial state */
    integration->current_arousal = 0.5f;
    integration->current_attention = 0.5f;
    integration->current_mode = MESH_THALAMUS_MODE_TONIC;

    integration->stats.current_arousal = 0.5f;
    integration->stats.current_attention = 0.5f;
    integration->stats.current_mode = MESH_THALAMUS_MODE_TONIC;

    mutex_attr_t attr = {0};
    integration->mutex = nimcp_mutex_create(&attr);
    if (!integration->mutex) {
        LOG_ERROR("Failed to create thalamus integration mutex");
        nimcp_free(integration);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_thalamus_create: integration->mutex is NULL");
        return NULL;
    }

    LOG_INFO("Created thalamus mesh integration");

    return integration;
}

void mesh_thalamus_destroy(mesh_thalamus_integration_t* integration) {
    if (!integration || integration->magic != MESH_THALAMUS_MAGIC) {
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

    LOG_INFO("Destroyed thalamus mesh integration");
}

/* ============================================================================
 * Participant Registration API
 * ============================================================================ */

nimcp_error_t mesh_thalamus_register_participant(mesh_thalamus_integration_t* integration) {
    if (!integration || integration->magic != MESH_THALAMUS_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);

    if (integration->registered) {
        nimcp_mutex_unlock(integration->mutex);
        LOG_WARN("Thalamus already registered with mesh");
        return NIMCP_ERROR_ALREADY_EXISTS;
    }

    if (!integration->registry) {
        nimcp_mutex_unlock(integration->mutex);
        LOG_ERROR("No participant registry available");
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    thalamus_heartbeat(integration, "registering", 0.0f);

    mesh_participant_interface_init(&integration->participant_interface);

    strncpy(integration->participant_interface.module_name, "thalamus",
            MESH_MAX_NAME_LEN - 1);
    integration->participant_interface.type = MESH_PARTICIPANT_MODULE;
    integration->participant_interface.home_channel = MESH_CHANNEL_SUBCORTICAL;

    integration->participant_interface.on_proposal = thalamus_on_proposal;
    integration->participant_interface.on_endorse_request = thalamus_on_endorse_request;
    integration->participant_interface.on_commit = thalamus_on_commit;
    integration->participant_interface.get_free_energy = thalamus_get_free_energy;
    integration->participant_interface.get_health_metrics = thalamus_get_health_metrics;
    integration->participant_interface.user_context = integration;

    mesh_participant_config_t reg_config;
    mesh_participant_config_init(&reg_config);
    reg_config.module_name = "thalamus";
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
        LOG_ERROR("Failed to register thalamus with mesh: %d", err);
        NIMCP_THROW_TO_IMMUNE(err, "mesh_thalamus_register_participant: registration failed");
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

    LOG_INFO("Thalamus registered with mesh: participant_id=0x%016lX",
             (unsigned long)integration->participant_id);

    thalamus_heartbeat(integration, "registered", 1.0f);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_thalamus_unregister_participant(mesh_thalamus_integration_t* integration) {
    if (!integration || integration->magic != MESH_THALAMUS_MAGIC) {
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

    LOG_INFO("Thalamus unregistered from mesh");

    return NIMCP_SUCCESS;
}

mesh_participant_id_t mesh_thalamus_get_participant_id(
    const mesh_thalamus_integration_t* integration
) {
    if (!integration || integration->magic != MESH_THALAMUS_MAGIC) {
        return 0;
    }
    return integration->participant_id;
}

bool mesh_thalamus_is_registered(const mesh_thalamus_integration_t* integration) {
    if (!integration || integration->magic != MESH_THALAMUS_MAGIC) {
        return false;
    }
    return integration->registered;
}

/* ============================================================================
 * Relay Operations API
 * ============================================================================ */

nimcp_error_t mesh_thalamus_report_relay(
    mesh_thalamus_integration_t* integration,
    uint32_t nucleus_type,
    uint32_t channel_count
) {
    if (!integration || integration->magic != MESH_THALAMUS_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!integration->registered) {
        return NIMCP_ERROR_NOT_INITIALIZED;
    }
    if (!integration->config.broadcast_relay_activity) {
        return NIMCP_SUCCESS;  /* Broadcasting disabled */
    }

    nimcp_mutex_lock(integration->mutex);

    mesh_thalamus_relay_payload_t payload = {
        .nucleus_type = nucleus_type,
        .channel_count = channel_count,
        .attention_weight = integration->current_attention,
        .arousal_level = integration->current_arousal
    };

    /* Determine transaction type based on nucleus */
    mesh_thalamus_tx_type_t tx_type = MESH_TX_THALAMUS_BASE;
    if (nucleus_type == 0) tx_type = MESH_TX_THALAMUS_RELAY_VISUAL;
    else if (nucleus_type == 1) tx_type = MESH_TX_THALAMUS_RELAY_AUDITORY;
    else if (nucleus_type == 2) tx_type = MESH_TX_THALAMUS_RELAY_SOMATO;
    else if (nucleus_type == 3) tx_type = MESH_TX_THALAMUS_RELAY_MOTOR;
    else if (nucleus_type == 4) tx_type = MESH_TX_THALAMUS_RELAY_EXECUTIVE;

    mesh_transaction_t* tx = mesh_transaction_create(
        (mesh_tx_type_t)tx_type,
        integration->participant_id,
        MESH_CHANNEL_SUBCORTICAL
    );

    if (!tx) {
        nimcp_mutex_unlock(integration->mutex);
        return NIMCP_ERROR_OUT_OF_MEMORY;
    }

    mesh_transaction_set_payload(tx, &payload, sizeof(payload));
    mesh_transaction_set_timeout(tx, integration->config.relay_timeout_ms);

    nimcp_error_t err = NIMCP_SUCCESS;
    if (integration->mesh_integration) {
        err = mesh_integration_submit_transaction(integration->mesh_integration, tx);
    }

    mesh_transaction_destroy(tx);
    nimcp_mutex_unlock(integration->mutex);

    return err;
}

nimcp_error_t mesh_thalamus_update_attention(
    mesh_thalamus_integration_t* integration,
    float new_attention
) {
    if (!integration || integration->magic != MESH_THALAMUS_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!integration->registered) {
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    nimcp_mutex_lock(integration->mutex);

    mesh_transaction_t* tx = mesh_transaction_create(
        (mesh_tx_type_t)MESH_TX_THALAMUS_ATTENTION_UPDATE,
        integration->participant_id,
        MESH_CHANNEL_SUBCORTICAL
    );

    if (!tx) {
        nimcp_mutex_unlock(integration->mutex);
        return NIMCP_ERROR_OUT_OF_MEMORY;
    }

    mesh_transaction_set_payload(tx, &new_attention, sizeof(new_attention));
    mesh_transaction_set_timeout(tx, integration->config.attention_timeout_ms);

    nimcp_error_t err = NIMCP_SUCCESS;
    if (integration->mesh_integration) {
        err = mesh_integration_submit_transaction(integration->mesh_integration, tx);
    }

    mesh_transaction_destroy(tx);
    nimcp_mutex_unlock(integration->mutex);

    return err;
}

nimcp_error_t mesh_thalamus_update_arousal(
    mesh_thalamus_integration_t* integration,
    float new_arousal
) {
    if (!integration || integration->magic != MESH_THALAMUS_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!integration->registered) {
        return NIMCP_ERROR_NOT_INITIALIZED;
    }
    if (!integration->config.sync_arousal_with_mesh) {
        /* Update locally only */
        nimcp_mutex_lock(integration->mutex);
        integration->current_arousal = new_arousal;
        integration->stats.current_arousal = new_arousal;
        integration->current_mode = (new_arousal < 0.3f) ?
            MESH_THALAMUS_MODE_BURST : MESH_THALAMUS_MODE_TONIC;
        integration->stats.current_mode = integration->current_mode;
        nimcp_mutex_unlock(integration->mutex);
        return NIMCP_SUCCESS;
    }

    nimcp_mutex_lock(integration->mutex);

    mesh_thalamus_arousal_payload_t payload = {
        .old_arousal = integration->current_arousal,
        .new_arousal = new_arousal,
        .resulting_mode = (new_arousal < 0.3f) ?
            MESH_THALAMUS_MODE_BURST : MESH_THALAMUS_MODE_TONIC
    };

    mesh_transaction_t* tx = mesh_transaction_create(
        (mesh_tx_type_t)MESH_TX_THALAMUS_AROUSAL_CHANGE,
        integration->participant_id,
        MESH_CHANNEL_SUBCORTICAL
    );

    if (!tx) {
        nimcp_mutex_unlock(integration->mutex);
        return NIMCP_ERROR_OUT_OF_MEMORY;
    }

    mesh_transaction_set_payload(tx, &payload, sizeof(payload));

    nimcp_error_t err = NIMCP_SUCCESS;
    if (integration->mesh_integration) {
        err = mesh_integration_submit_transaction(integration->mesh_integration, tx);
    }

    mesh_transaction_destroy(tx);
    nimcp_mutex_unlock(integration->mutex);

    return err;
}

/* ============================================================================
 * State Query API
 * ============================================================================ */

float mesh_thalamus_get_arousal(const mesh_thalamus_integration_t* integration) {
    if (!integration || integration->magic != MESH_THALAMUS_MAGIC) {
        return 0.0f;
    }
    return integration->current_arousal;
}

float mesh_thalamus_get_attention(const mesh_thalamus_integration_t* integration) {
    if (!integration || integration->magic != MESH_THALAMUS_MAGIC) {
        return 0.0f;
    }
    return integration->current_attention;
}

mesh_thalamus_firing_mode_t mesh_thalamus_get_mode(const mesh_thalamus_integration_t* integration) {
    if (!integration || integration->magic != MESH_THALAMUS_MAGIC) {
        return MESH_THALAMUS_MODE_TONIC;
    }
    return integration->current_mode;
}

/* ============================================================================
 * Statistics API
 * ============================================================================ */

nimcp_error_t mesh_thalamus_get_stats(
    const mesh_thalamus_integration_t* integration,
    mesh_thalamus_stats_t* stats
) {
    if (!integration || integration->magic != MESH_THALAMUS_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!stats) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(((mesh_thalamus_integration_t*)integration)->mutex);
    *stats = integration->stats;
    nimcp_mutex_unlock(((mesh_thalamus_integration_t*)integration)->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_thalamus_reset_stats(mesh_thalamus_integration_t* integration) {
    if (!integration || integration->magic != MESH_THALAMUS_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);

    /* Preserve current state */
    float arousal = integration->stats.current_arousal;
    float attention = integration->stats.current_attention;
    mesh_thalamus_firing_mode_t mode = integration->stats.current_mode;

    memset(&integration->stats, 0, sizeof(integration->stats));

    integration->stats.current_arousal = arousal;
    integration->stats.current_attention = attention;
    integration->stats.current_mode = mode;

    nimcp_mutex_unlock(integration->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Health Agent Integration API
 * ============================================================================ */

nimcp_error_t mesh_thalamus_set_health_agent(
    mesh_thalamus_integration_t* integration,
    nimcp_health_agent_t* agent
) {
    if (!integration || integration->magic != MESH_THALAMUS_MAGIC) {
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

bool mesh_thalamus_is_thalamus_transaction(mesh_tx_type_t tx_type) {
    return tx_type >= MESH_TX_THALAMUS_BASE &&
           tx_type <= MESH_TX_THALAMUS_BURST_TRIGGER;
}

const char* mesh_thalamus_mode_to_string(mesh_thalamus_firing_mode_t mode) {
    if (mode >= sizeof(firing_mode_names) / sizeof(firing_mode_names[0])) {
        return "unknown";
    }
    return firing_mode_names[mode];
}
