/**
 * @file nimcp_mesh_global_workspace_integration.c
 * @brief Global Workspace Mesh Network Integration Implementation
 *
 * WHAT: Connects the global workspace module to the mesh network
 * WHY:  Enable coordinated conscious broadcasting via distributed consensus
 * HOW:  Register as SYSTEM participant with COORDINATOR role, handle broadcast transactions
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 * @version 1.0.0
 */

#include "mesh/nimcp_mesh_global_workspace_integration.h"
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

#define LOG_MODULE "MESH_GLOBAL_WORKSPACE"

/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define MESH_GW_MAGIC 0x47574B53  /* "GWKS" (Global WorKSpace) */

/* Participant type for global workspace */
#define MESH_PARTICIPANT_GLOBAL_WORKSPACE 0x001A

/* ============================================================================
 * String Conversion Tables
 * ============================================================================ */

static const char* strategy_names[] = {
    [MESH_GW_STRATEGY_WINNER_TAKE_ALL] = "winner_take_all",
    [MESH_GW_STRATEGY_SOFTMAX] = "softmax",
    [MESH_GW_STRATEGY_COALITION] = "coalition",
    [MESH_GW_STRATEGY_HIERARCHICAL] = "hierarchical",
};

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct mesh_global_workspace_integration {
    uint32_t magic;

    /* Configuration */
    mesh_gw_config_t config;

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

    /* Connected global workspace module */
    global_workspace_t* workspace;

    /* Callbacks */
    mesh_gw_broadcast_callback_t broadcast_callback;
    void* broadcast_callback_ctx;

    mesh_gw_ignition_callback_t ignition_callback;
    void* ignition_callback_ctx;

    /* Health agent */
    nimcp_health_agent_t* health_agent;

    /* Statistics */
    mesh_gw_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Transaction sequence counter */
    uint64_t tx_sequence;
};

/* ============================================================================
 * Forward Declarations for Participant Callbacks
 * ============================================================================ */

static nimcp_error_t gw_on_proposal(void* ctx, const mesh_transaction_t* tx);
static nimcp_error_t gw_on_endorse_request(void* ctx, const mesh_transaction_t* tx,
                                            mesh_endorsement_t* endorsement);
static nimcp_error_t gw_on_commit(void* ctx, const mesh_transaction_t* tx);
static float gw_get_free_energy(void* ctx);
static void gw_get_health_metrics(void* ctx, health_metrics_t* metrics);

/* ============================================================================
 * Internal Helper: Send Heartbeat
 * ============================================================================ */

static void gw_heartbeat(mesh_global_workspace_integration_t* integration,
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
 * Internal Helper: Handle Transaction by Type
 * ============================================================================ */

static nimcp_error_t handle_broadcast_commit(mesh_global_workspace_integration_t* integration,
                                             const mesh_transaction_t* tx) {
    if (tx->payload_size < sizeof(mesh_gw_broadcast_payload_t)) {
        LOG_WARN("Broadcast payload too small");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const mesh_gw_broadcast_payload_t* payload =
        (const mesh_gw_broadcast_payload_t*)tx->payload;

    integration->stats.broadcasts_completed++;
    integration->stats.last_broadcast_ns = nimcp_time_now_ns();
    integration->stats.current_winner_module = payload->source_module;
    integration->stats.current_winner_strength = payload->strength;

    LOG_INFO("Global workspace broadcast: module=%u strength=%.2f",
             payload->source_module, payload->strength);

    /* Invoke callback */
    if (integration->broadcast_callback && payload->content_size > 0) {
        integration->broadcast_callback(
            payload->source_module,
            payload->content,
            payload->content_size,
            integration->broadcast_callback_ctx
        );
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_ignition_commit(mesh_global_workspace_integration_t* integration,
                                            const mesh_transaction_t* tx) {
    if (tx->payload_size < sizeof(mesh_gw_ignition_payload_t)) {
        LOG_WARN("Ignition payload too small");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const mesh_gw_ignition_payload_t* payload =
        (const mesh_gw_ignition_payload_t*)tx->payload;

    integration->stats.ignition_events++;
    integration->stats.last_ignition_ns = nimcp_time_now_ns();

    LOG_INFO("Global workspace ignition: module=%u activation=%.2f sustained=%d",
             payload->source_module, payload->activation_level, payload->sustained);

    /* Invoke callback */
    if (integration->ignition_callback) {
        integration->ignition_callback(
            payload->source_module,
            payload->activation_level,
            integration->ignition_callback_ctx
        );
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Participant Callback Implementations
 * ============================================================================ */

static nimcp_error_t gw_on_proposal(void* ctx, const mesh_transaction_t* tx) {
    mesh_global_workspace_integration_t* integration = (mesh_global_workspace_integration_t*)ctx;

    if (!integration || integration->magic != MESH_GW_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!tx) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    integration->stats.transactions_received++;

    if (!mesh_gw_is_gw_transaction(tx->type)) {
        return NIMCP_SUCCESS;
    }

    gw_heartbeat(integration, "proposal_received", 0.5f);

    return NIMCP_SUCCESS;
}

static nimcp_error_t gw_on_endorse_request(void* ctx, const mesh_transaction_t* tx,
                                            mesh_endorsement_t* endorsement) {
    mesh_global_workspace_integration_t* integration = (mesh_global_workspace_integration_t*)ctx;

    if (!integration || integration->magic != MESH_GW_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!tx || !endorsement) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    integration->stats.endorsement_requests_handled++;

    memset(endorsement, 0, sizeof(*endorsement));
    endorsement->endorser_id = integration->participant_id;
    endorsement->timestamp_ns = nimcp_time_now_ns();

    if (mesh_gw_is_gw_transaction(tx->type)) {
        /* Validate global workspace transactions */
        bool valid = true;

        switch ((mesh_gw_tx_type_t)tx->type) {
            case MESH_TX_GW_BROADCAST:
                if (tx->payload_size < sizeof(mesh_gw_broadcast_payload_t)) {
                    valid = false;
                } else {
                    const mesh_gw_broadcast_payload_t* payload =
                        (const mesh_gw_broadcast_payload_t*)tx->payload;
                    /* Check if strength meets ignition threshold */
                    if (payload->strength < integration->config.ignition_threshold) {
                        valid = false;  /* Below threshold */
                    }
                }
                break;

            case MESH_TX_GW_IGNITION:
                if (tx->payload_size < sizeof(mesh_gw_ignition_payload_t)) {
                    valid = false;
                }
                break;

            default:
                break;
        }

        if (valid) {
            endorsement->result = ENDORSEMENT_APPROVED;
            integration->stats.transactions_endorsed++;
        } else {
            endorsement->result = ENDORSEMENT_REJECTED;
            integration->stats.transactions_vetoed++;
        }
    } else {
        /* Global workspace endorses all transactions (it's the coordinator) */
        endorsement->result = ENDORSEMENT_APPROVED;
        integration->stats.transactions_endorsed++;
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t gw_on_commit(void* ctx, const mesh_transaction_t* tx) {
    mesh_global_workspace_integration_t* integration = (mesh_global_workspace_integration_t*)ctx;

    if (!integration || integration->magic != MESH_GW_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!tx) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!mesh_gw_is_gw_transaction(tx->type)) {
        return NIMCP_SUCCESS;
    }

    nimcp_mutex_lock(integration->mutex);

    nimcp_error_t result = NIMCP_SUCCESS;

    switch ((mesh_gw_tx_type_t)tx->type) {
        case MESH_TX_GW_BROADCAST:
            result = handle_broadcast_commit(integration, tx);
            break;

        case MESH_TX_GW_WINNER:
            integration->stats.winners_announced++;
            integration->stats.competitions_held++;
            break;

        case MESH_TX_GW_IGNITION:
            result = handle_ignition_commit(integration, tx);
            break;

        case MESH_TX_GW_DECAY:
            /* Content fading */
            break;

        case MESH_TX_GW_ATTENTION_SHIFT:
            /* Attention spotlight moved */
            break;

        case MESH_TX_GW_MODULE_REGISTER:
            integration->stats.active_competitors++;
            break;

        case MESH_TX_GW_COALITION_FORM:
            integration->stats.coalitions_formed++;
            break;

        case MESH_TX_GW_BINDING:
            integration->stats.binding_events++;
            break;

        case MESH_TX_GW_WM_UPDATE:
        case MESH_TX_GW_METACOGNITION:
            /* Track but no special handling */
            break;

        default:
            break;
    }

    nimcp_mutex_unlock(integration->mutex);

    gw_heartbeat(integration, "commit_processed", 1.0f);

    return result;
}

static float gw_get_free_energy(void* ctx) {
    mesh_global_workspace_integration_t* integration = (mesh_global_workspace_integration_t*)ctx;

    if (!integration || integration->magic != MESH_GW_MAGIC) {
        return -1.0f;
    }

    /* Free energy based on broadcast success and competition activity */
    float broadcast_energy = 0.3f;
    float competition_energy = 0.3f;

    /* Successful broadcasts = low energy */
    uint64_t total = integration->stats.broadcasts_initiated;
    uint64_t success = integration->stats.broadcasts_completed;
    if (total > 0) {
        float success_rate = (float)success / (float)total;
        broadcast_energy = 1.0f - success_rate;
    }

    /* Active competitors = healthy competition = low energy */
    uint32_t competitors = integration->stats.active_competitors;
    if (competitors > 0 && competitors < 20) {
        competition_energy = 0.1f + (0.5f / competitors);
    } else if (competitors >= 20) {
        competition_energy = 0.1f;  /* Many competitors is healthy */
    } else {
        competition_energy = 0.8f;  /* No competitors is bad */
    }

    return (broadcast_energy + competition_energy) / 2.0f;
}

static void gw_get_health_metrics(void* ctx, health_metrics_t* metrics) {
    mesh_global_workspace_integration_t* integration = (mesh_global_workspace_integration_t*)ctx;

    if (!integration || integration->magic != MESH_GW_MAGIC || !metrics) {
        return;
    }

    memset(metrics, 0, sizeof(*metrics));
    metrics->participant = integration->participant_id;
    metrics->last_heartbeat_ns = nimcp_time_now_ns();
    metrics->is_healthy = integration->stats.broadcasts_failed <
                          integration->stats.broadcasts_completed;
    metrics->avg_latency_ms = 5.0f;  /* Broadcasts take time */
    metrics->transactions_processed = integration->stats.transactions_received;
    metrics->transactions_failed = integration->stats.broadcasts_failed;
}

/* ============================================================================
 * Configuration Defaults
 * ============================================================================ */

nimcp_error_t mesh_gw_default_config(mesh_gw_config_t* config) {
    if (!config) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    memset(config, 0, sizeof(*config));

    config->strategy = MESH_GW_STRATEGY_WINNER_TAKE_ALL;
    config->broadcast_to_all_channels = true;
    config->require_consensus_for_broadcast = true;

    config->ignition_threshold = 0.5f;
    config->decay_rate = 0.1f;
    config->max_coalition_size = 5;

    config->competition_timeout_ms = 100;
    config->broadcast_timeout_ms = 500;

    config->enable_health_monitoring = true;
    config->heartbeat_interval_ms = 1000;

    config->verbose_logging = false;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

mesh_global_workspace_integration_t* mesh_gw_create(
    mesh_bootstrap_t* bootstrap,
    global_workspace_t* workspace,
    const mesh_gw_config_t* config
) {
    if (!bootstrap) {
        LOG_ERROR("Cannot create global workspace integration without bootstrap");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "mesh_gw_create: bootstrap is NULL");
        return NULL;
    }

    mesh_gw_config_t default_config;
    if (!config) {
        mesh_gw_default_config(&default_config);
        config = &default_config;
    }

    mesh_global_workspace_integration_t* integration = nimcp_calloc(1, sizeof(*integration));
    if (!integration) {
        LOG_ERROR("Failed to allocate global workspace integration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_MEMORY,
            "mesh_gw_create: allocation failed");
        return NULL;
    }

    integration->magic = MESH_GW_MAGIC;
    integration->config = *config;
    integration->bootstrap = bootstrap;
    integration->mesh_integration = mesh_bootstrap_get_integration(bootstrap);
    integration->registry = mesh_bootstrap_get_registry(bootstrap);
    integration->health_bridge = mesh_bootstrap_get_health_bridge(bootstrap);
    integration->exception_bridge = mesh_bootstrap_get_exception_bridge(bootstrap);
    integration->workspace = workspace;

    mutex_attr_t attr = {0};
    integration->mutex = nimcp_mutex_create(&attr);
    if (!integration->mutex) {
        LOG_ERROR("Failed to create global workspace integration mutex");
        nimcp_free(integration);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_gw_create: integration->mutex is NULL");
        return NULL;
    }

    LOG_INFO("Created global workspace mesh integration (strategy=%s)",
             mesh_gw_strategy_to_string(config->strategy));

    return integration;
}

void mesh_gw_destroy(mesh_global_workspace_integration_t* integration) {
    if (!integration || integration->magic != MESH_GW_MAGIC) {
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

    LOG_INFO("Destroyed global workspace mesh integration");
}

/* ============================================================================
 * Participant Registration API
 * ============================================================================ */

nimcp_error_t mesh_gw_register_participant(mesh_global_workspace_integration_t* integration) {
    if (!integration || integration->magic != MESH_GW_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);

    if (integration->registered) {
        nimcp_mutex_unlock(integration->mutex);
        LOG_WARN("Global workspace already registered with mesh");
        return NIMCP_ERROR_ALREADY_EXISTS;
    }

    if (!integration->registry) {
        nimcp_mutex_unlock(integration->mutex);
        LOG_ERROR("No participant registry available");
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    gw_heartbeat(integration, "registering", 0.0f);

    mesh_participant_interface_init(&integration->participant_interface);

    strncpy(integration->participant_interface.module_name, "global_workspace",
            MESH_MAX_NAME_LEN - 1);
    integration->participant_interface.type = MESH_PARTICIPANT_MODULE;
    integration->participant_interface.home_channel = MESH_CHANNEL_SYSTEM;

    integration->participant_interface.on_proposal = gw_on_proposal;
    integration->participant_interface.on_endorse_request = gw_on_endorse_request;
    integration->participant_interface.on_commit = gw_on_commit;
    integration->participant_interface.get_free_energy = gw_get_free_energy;
    integration->participant_interface.get_health_metrics = gw_get_health_metrics;
    integration->participant_interface.user_context = integration;

    mesh_participant_config_t reg_config;
    mesh_participant_config_init(&reg_config);
    reg_config.module_name = "global_workspace";
    reg_config.type = MESH_PARTICIPANT_MODULE;
    reg_config.home_channel = MESH_CHANNEL_SYSTEM;
    reg_config.user_context = integration;

    nimcp_error_t err = mesh_participant_register(
        integration->registry,
        &integration->participant_interface,
        &reg_config,
        &integration->participant_id
    );

    if (err != NIMCP_SUCCESS) {
        nimcp_mutex_unlock(integration->mutex);
        LOG_ERROR("Failed to register global workspace with mesh: %d", err);
        NIMCP_THROW_TO_IMMUNE(err, "mesh_gw_register_participant: registration failed");
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

    LOG_INFO("Global workspace registered with mesh: participant_id=0x%016lX",
             (unsigned long)integration->participant_id);

    gw_heartbeat(integration, "registered", 1.0f);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_gw_unregister_participant(mesh_global_workspace_integration_t* integration) {
    if (!integration || integration->magic != MESH_GW_MAGIC) {
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

    LOG_INFO("Global workspace unregistered from mesh");

    return NIMCP_SUCCESS;
}

mesh_participant_id_t mesh_gw_get_participant_id(
    const mesh_global_workspace_integration_t* integration
) {
    if (!integration || integration->magic != MESH_GW_MAGIC) {
        return 0;
    }
    return integration->participant_id;
}

bool mesh_gw_is_registered(const mesh_global_workspace_integration_t* integration) {
    if (!integration || integration->magic != MESH_GW_MAGIC) {
        return false;
    }
    return integration->registered;
}

/* ============================================================================
 * Broadcast Operations API
 * ============================================================================ */

nimcp_error_t mesh_gw_initiate_broadcast(
    mesh_global_workspace_integration_t* integration,
    uint32_t source_module,
    const void* content,
    size_t content_size,
    float strength
) {
    if (!integration || integration->magic != MESH_GW_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!integration->registered) {
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    /* Check if strength meets threshold */
    if (strength < integration->config.ignition_threshold) {
        LOG_DEBUG("Broadcast rejected: strength %.2f below threshold %.2f",
                  strength, integration->config.ignition_threshold);
        return NIMCP_ERROR_OUT_OF_RANGE;
    }

    nimcp_mutex_lock(integration->mutex);

    integration->stats.broadcasts_initiated++;

    mesh_gw_broadcast_payload_t payload = {
        .source_module = source_module,
        .strength = strength,
        .content_type = 0,
        .content_size = (content_size > 256) ? 256 : content_size
    };

    if (content && content_size > 0) {
        memcpy(payload.content, content, payload.content_size);
    }

    mesh_transaction_t* tx = mesh_transaction_create(
        (mesh_tx_type_t)MESH_TX_GW_BROADCAST,
        integration->participant_id,
        MESH_CHANNEL_SYSTEM
    );

    if (!tx) {
        nimcp_mutex_unlock(integration->mutex);
        integration->stats.broadcasts_failed++;
        return NIMCP_ERROR_OUT_OF_MEMORY;
    }

    mesh_transaction_set_payload(tx, &payload, sizeof(payload));
    mesh_transaction_set_timeout(tx, integration->config.broadcast_timeout_ms);

    nimcp_error_t err = NIMCP_SUCCESS;
    if (integration->mesh_integration) {
        err = mesh_integration_submit_transaction(integration->mesh_integration, tx);
    }

    mesh_transaction_destroy(tx);

    if (err != NIMCP_SUCCESS) {
        integration->stats.broadcasts_failed++;
    }

    nimcp_mutex_unlock(integration->mutex);

    gw_heartbeat(integration, "broadcast_initiated", 1.0f);

    return err;
}

nimcp_error_t mesh_gw_report_ignition(
    mesh_global_workspace_integration_t* integration,
    uint32_t source_module,
    float activation_level
) {
    if (!integration || integration->magic != MESH_GW_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!integration->registered) {
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    nimcp_mutex_lock(integration->mutex);

    mesh_gw_ignition_payload_t payload = {
        .source_module = source_module,
        .activation_level = activation_level,
        .sustained = (activation_level > integration->config.ignition_threshold * 1.5f)
    };

    mesh_transaction_t* tx = mesh_transaction_create(
        (mesh_tx_type_t)MESH_TX_GW_IGNITION,
        integration->participant_id,
        MESH_CHANNEL_SYSTEM
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

nimcp_error_t mesh_gw_report_winner(
    mesh_global_workspace_integration_t* integration,
    uint32_t winner_module,
    float strength
) {
    if (!integration || integration->magic != MESH_GW_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!integration->registered) {
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    nimcp_mutex_lock(integration->mutex);

    struct {
        uint32_t winner_module;
        float strength;
    } payload = { winner_module, strength };

    mesh_transaction_t* tx = mesh_transaction_create(
        (mesh_tx_type_t)MESH_TX_GW_WINNER,
        integration->participant_id,
        MESH_CHANNEL_SYSTEM
    );

    if (!tx) {
        nimcp_mutex_unlock(integration->mutex);
        return NIMCP_ERROR_OUT_OF_MEMORY;
    }

    mesh_transaction_set_payload(tx, &payload, sizeof(payload));
    mesh_transaction_set_timeout(tx, integration->config.competition_timeout_ms);

    nimcp_error_t err = NIMCP_SUCCESS;
    if (integration->mesh_integration) {
        err = mesh_integration_submit_transaction(integration->mesh_integration, tx);
    }

    mesh_transaction_destroy(tx);
    nimcp_mutex_unlock(integration->mutex);

    return err;
}

/* ============================================================================
 * Callback Registration API
 * ============================================================================ */

nimcp_error_t mesh_gw_set_broadcast_callback(
    mesh_global_workspace_integration_t* integration,
    mesh_gw_broadcast_callback_t callback,
    void* ctx
) {
    if (!integration || integration->magic != MESH_GW_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);
    integration->broadcast_callback = callback;
    integration->broadcast_callback_ctx = ctx;
    nimcp_mutex_unlock(integration->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_gw_set_ignition_callback(
    mesh_global_workspace_integration_t* integration,
    mesh_gw_ignition_callback_t callback,
    void* ctx
) {
    if (!integration || integration->magic != MESH_GW_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);
    integration->ignition_callback = callback;
    integration->ignition_callback_ctx = ctx;
    nimcp_mutex_unlock(integration->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics API
 * ============================================================================ */

nimcp_error_t mesh_gw_get_stats(
    const mesh_global_workspace_integration_t* integration,
    mesh_gw_stats_t* stats
) {
    if (!integration || integration->magic != MESH_GW_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!stats) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(((mesh_global_workspace_integration_t*)integration)->mutex);
    *stats = integration->stats;
    nimcp_mutex_unlock(((mesh_global_workspace_integration_t*)integration)->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_gw_reset_stats(mesh_global_workspace_integration_t* integration) {
    if (!integration || integration->magic != MESH_GW_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);

    /* Preserve active competitors count */
    uint32_t competitors = integration->stats.active_competitors;

    memset(&integration->stats, 0, sizeof(integration->stats));

    integration->stats.active_competitors = competitors;

    nimcp_mutex_unlock(integration->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Health Agent Integration API
 * ============================================================================ */

nimcp_error_t mesh_gw_set_health_agent(
    mesh_global_workspace_integration_t* integration,
    nimcp_health_agent_t* agent
) {
    if (!integration || integration->magic != MESH_GW_MAGIC) {
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

bool mesh_gw_is_gw_transaction(mesh_tx_type_t tx_type) {
    return tx_type >= MESH_TX_GW_BASE &&
           tx_type <= MESH_TX_GW_METACOGNITION;
}

const char* mesh_gw_strategy_to_string(mesh_gw_competition_strategy_t strategy) {
    if (strategy >= sizeof(strategy_names) / sizeof(strategy_names[0])) {
        return "unknown";
    }
    return strategy_names[strategy];
}
