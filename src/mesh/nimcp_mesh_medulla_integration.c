/**
 * @file nimcp_mesh_medulla_integration.c
 * @brief Medulla Oblongata Mesh Network Integration Implementation
 *
 * WHAT: Connects the medulla oblongata module to the mesh network
 * WHY:  Enable coordinated brainstem control via distributed consensus
 * HOW:  Register as subcortical participant, handle arousal/protection transactions
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 * @version 1.0.0
 */

#include "mesh/nimcp_mesh_medulla_integration.h"
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
#include <math.h>

#define LOG_MODULE "MESH_MEDULLA"

/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define MESH_MEDULLA_MAGIC 0x4D454455  /* "MEDU" */

/* Participant type for medulla (add to types if needed) */
#define MESH_PARTICIPANT_MEDULLA 0x0015

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct mesh_medulla_integration {
    uint32_t magic;

    /* Configuration */
    mesh_medulla_config_t config;

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

    /* Connected medulla module */
    medulla_oblongata_t* medulla;

    /* Current state */
    mesh_medulla_arousal_state_t arousal_state;
    mesh_medulla_protection_level_t protection_level;
    bool in_emergency_mode;

    /* Callbacks */
    mesh_medulla_arousal_callback_t arousal_callback;
    void* arousal_callback_ctx;

    mesh_medulla_protection_callback_t protection_callback;
    void* protection_callback_ctx;

    mesh_medulla_emergency_callback_t emergency_callback;
    void* emergency_callback_ctx;

    /* Health agent */
    nimcp_health_agent_t* health_agent;

    /* Statistics */
    mesh_medulla_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Transaction sequence counter */
    uint64_t tx_sequence;
};

/* ============================================================================
 * String Conversion Tables
 * ============================================================================ */

static const char* arousal_state_names[] = {
    [MEDULLA_AROUSAL_COMA] = "coma",
    [MEDULLA_AROUSAL_DEEP_SLEEP] = "deep_sleep",
    [MEDULLA_AROUSAL_LIGHT_SLEEP] = "light_sleep",
    [MEDULLA_AROUSAL_DROWSY] = "drowsy",
    [MEDULLA_AROUSAL_RELAXED] = "relaxed",
    [MEDULLA_AROUSAL_ALERT] = "alert",
    [MEDULLA_AROUSAL_VIGILANT] = "vigilant",
    [MEDULLA_AROUSAL_HYPERVIGILANT] = "hypervigilant",
    [MEDULLA_AROUSAL_EMERGENCY] = "emergency",
};

static const char* protection_level_names[] = {
    [MEDULLA_PROTECT_DISABLED] = "disabled",
    [MEDULLA_PROTECT_MINIMAL] = "minimal",
    [MEDULLA_PROTECT_NORMAL] = "normal",
    [MEDULLA_PROTECT_HEIGHTENED] = "heightened",
    [MEDULLA_PROTECT_MAXIMUM] = "maximum",
};

/* ============================================================================
 * Forward Declarations for Participant Callbacks
 * ============================================================================ */

static nimcp_error_t medulla_on_proposal(void* ctx, const mesh_transaction_t* tx);
static nimcp_error_t medulla_on_endorse_request(void* ctx, const mesh_transaction_t* tx,
                                                 mesh_endorsement_t* endorsement);
static nimcp_error_t medulla_on_commit(void* ctx, const mesh_transaction_t* tx);
static float medulla_get_free_energy(void* ctx);
static void medulla_get_health_metrics(void* ctx, health_metrics_t* metrics);

/* ============================================================================
 * Internal Helper: Send Heartbeat
 * ============================================================================ */

static void medulla_heartbeat(mesh_medulla_integration_t* integration,
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
 * Internal Helper: Update State with Callback
 * ============================================================================ */

static void update_arousal_state(mesh_medulla_integration_t* integration,
                                 mesh_medulla_arousal_state_t new_state) {
    mesh_medulla_arousal_state_t old_state = integration->arousal_state;

    if (old_state == new_state) {
        return;
    }

    integration->arousal_state = new_state;
    integration->stats.current_arousal = new_state;
    integration->stats.last_arousal_change_ns = nimcp_time_now_ns();

    LOG_INFO("Arousal state changed: %s -> %s",
             mesh_medulla_arousal_to_string(old_state),
             mesh_medulla_arousal_to_string(new_state));

    /* Invoke callback */
    if (integration->arousal_callback) {
        integration->arousal_callback(old_state, new_state,
                                      integration->arousal_callback_ctx);
    }
}

static void update_protection_level(mesh_medulla_integration_t* integration,
                                    mesh_medulla_protection_level_t new_level) {
    mesh_medulla_protection_level_t old_level = integration->protection_level;

    if (old_level == new_level) {
        return;
    }

    integration->protection_level = new_level;
    integration->stats.current_protection = new_level;
    integration->stats.last_protection_change_ns = nimcp_time_now_ns();

    LOG_INFO("Protection level changed: %s -> %s",
             mesh_medulla_protection_to_string(old_level),
             mesh_medulla_protection_to_string(new_level));

    /* Invoke callback */
    if (integration->protection_callback) {
        integration->protection_callback(old_level, new_level,
                                         integration->protection_callback_ctx);
    }
}

/* ============================================================================
 * Internal Helper: Handle Transaction by Type
 * ============================================================================ */

static nimcp_error_t handle_arousal_change(mesh_medulla_integration_t* integration,
                                           const mesh_transaction_t* tx) {
    if (tx->payload_size < sizeof(mesh_medulla_arousal_payload_t)) {
        LOG_WARN("Arousal change payload too small");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const mesh_medulla_arousal_payload_t* payload =
        (const mesh_medulla_arousal_payload_t*)tx->payload;

    integration->stats.arousal_changes_committed++;
    update_arousal_state(integration, payload->target_state);

    /* Enter emergency mode if arousal is EMERGENCY */
    if (payload->target_state == MEDULLA_AROUSAL_EMERGENCY) {
        integration->in_emergency_mode = true;
        integration->stats.in_emergency_mode = true;
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_protection_change(mesh_medulla_integration_t* integration,
                                              const mesh_transaction_t* tx) {
    if (tx->payload_size < sizeof(mesh_medulla_protection_payload_t)) {
        LOG_WARN("Protection change payload too small");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const mesh_medulla_protection_payload_t* payload =
        (const mesh_medulla_protection_payload_t*)tx->payload;

    integration->stats.protection_changes_committed++;
    update_protection_level(integration, payload->target_level);

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_emergency_shutdown(mesh_medulla_integration_t* integration,
                                               const mesh_transaction_t* tx) {
    if (tx->payload_size < sizeof(mesh_medulla_emergency_payload_t)) {
        LOG_WARN("Emergency shutdown payload too small");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const mesh_medulla_emergency_payload_t* payload =
        (const mesh_medulla_emergency_payload_t*)tx->payload;

    LOG_ERROR("EMERGENCY SHUTDOWN triggered: code=%u reason=%s",
              payload->emergency_code, payload->reason);

    integration->in_emergency_mode = true;
    integration->stats.in_emergency_mode = true;
    integration->stats.emergency_shutdowns_triggered++;
    integration->stats.last_emergency_ns = nimcp_time_now_ns();

    /* Set arousal to emergency */
    update_arousal_state(integration, MEDULLA_AROUSAL_EMERGENCY);

    /* Set protection to maximum */
    update_protection_level(integration, MEDULLA_PROTECT_MAXIMUM);

    /* Invoke callback */
    if (integration->emergency_callback) {
        integration->emergency_callback(payload->emergency_code, payload->reason,
                                        integration->emergency_callback_ctx);
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_emergency_recovery(mesh_medulla_integration_t* integration,
                                               const mesh_transaction_t* tx) {
    (void)tx;

    LOG_INFO("Emergency recovery initiated");

    integration->in_emergency_mode = false;
    integration->stats.in_emergency_mode = false;
    integration->stats.emergency_recoveries_completed++;

    /* Return to alert state */
    update_arousal_state(integration, MEDULLA_AROUSAL_ALERT);

    /* Return to normal protection */
    update_protection_level(integration, MEDULLA_PROTECT_NORMAL);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Participant Callback Implementations
 * ============================================================================ */

static nimcp_error_t medulla_on_proposal(void* ctx, const mesh_transaction_t* tx) {
    mesh_medulla_integration_t* integration = (mesh_medulla_integration_t*)ctx;

    if (!integration || integration->magic != MESH_MEDULLA_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!tx) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    integration->stats.transactions_received++;

    /* Check if this is a medulla-related transaction */
    if (!mesh_medulla_is_medulla_transaction(tx->type)) {
        /* Not our transaction - no opinion */
        return NIMCP_SUCCESS;
    }

    medulla_heartbeat(integration, "proposal_received", 0.5f);

    /* Accept the proposal */
    return NIMCP_SUCCESS;
}

static nimcp_error_t medulla_on_endorse_request(void* ctx, const mesh_transaction_t* tx,
                                                 mesh_endorsement_t* endorsement) {
    mesh_medulla_integration_t* integration = (mesh_medulla_integration_t*)ctx;

    if (!integration || integration->magic != MESH_MEDULLA_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!tx || !endorsement) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    integration->stats.endorsement_requests_handled++;

    /* Initialize endorsement */
    memset(endorsement, 0, sizeof(*endorsement));
    endorsement->endorser_id = integration->participant_id;
    endorsement->timestamp_ns = nimcp_time_now_ns();

    /* Check if in emergency mode - veto all non-emergency transactions */
    if (integration->in_emergency_mode &&
        tx->type != MESH_TX_MEDULLA_EMERGENCY_SHUTDOWN &&
        tx->type != MESH_TX_MEDULLA_EMERGENCY_RECOVERY &&
        tx->type != MESH_TX_EMERGENCY_OVERRIDE) {

        LOG_WARN("Vetoing transaction during emergency mode: type=%d", tx->type);
        endorsement->result = ENDORSEMENT_REJECTED;
        integration->stats.transactions_vetoed++;
        return NIMCP_SUCCESS;
    }

    /* Endorse medulla-related transactions after validation */
    if (mesh_medulla_is_medulla_transaction(tx->type)) {
        /* Validate payload based on type */
        bool valid = true;

        switch ((mesh_medulla_tx_type_t)tx->type) {
            case MESH_TX_MEDULLA_AROUSAL_CHANGE:
                if (tx->payload_size < sizeof(mesh_medulla_arousal_payload_t)) {
                    valid = false;
                }
                break;

            case MESH_TX_MEDULLA_PROTECTION_CHANGE:
                if (tx->payload_size < sizeof(mesh_medulla_protection_payload_t)) {
                    valid = false;
                }
                break;

            case MESH_TX_MEDULLA_EMERGENCY_SHUTDOWN:
                if (tx->payload_size < sizeof(mesh_medulla_emergency_payload_t)) {
                    valid = false;
                }
                break;

            default:
                /* Accept other medulla transactions */
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
        /* Not a medulla transaction - abstain */
        endorsement->result = ENDORSEMENT_ABSTAIN;
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t medulla_on_commit(void* ctx, const mesh_transaction_t* tx) {
    mesh_medulla_integration_t* integration = (mesh_medulla_integration_t*)ctx;

    if (!integration || integration->magic != MESH_MEDULLA_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!tx) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Handle medulla-specific transactions */
    if (!mesh_medulla_is_medulla_transaction(tx->type)) {
        return NIMCP_SUCCESS;
    }

    nimcp_mutex_lock(integration->mutex);

    nimcp_error_t result = NIMCP_SUCCESS;

    switch ((mesh_medulla_tx_type_t)tx->type) {
        case MESH_TX_MEDULLA_AROUSAL_CHANGE:
            result = handle_arousal_change(integration, tx);
            break;

        case MESH_TX_MEDULLA_PROTECTION_CHANGE:
            result = handle_protection_change(integration, tx);
            break;

        case MESH_TX_MEDULLA_EMERGENCY_SHUTDOWN:
            result = handle_emergency_shutdown(integration, tx);
            break;

        case MESH_TX_MEDULLA_EMERGENCY_RECOVERY:
            result = handle_emergency_recovery(integration, tx);
            break;

        case MESH_TX_MEDULLA_REFLEX_TRIGGER:
            integration->stats.reflex_triggers_notified++;
            break;

        default:
            /* Other transaction types - just track */
            break;
    }

    nimcp_mutex_unlock(integration->mutex);

    medulla_heartbeat(integration, "commit_processed", 1.0f);

    return result;
}

static float medulla_get_free_energy(void* ctx) {
    mesh_medulla_integration_t* integration = (mesh_medulla_integration_t*)ctx;

    if (!integration || integration->magic != MESH_MEDULLA_MAGIC) {
        return -1.0f;
    }

    /* Free energy based on continuous arousal level and protection level.
     * Map discrete state to continuous level, then compute energy via
     * inverted-U curve centered at alert (optimal). This replaces the
     * former discrete switch with smooth continuous scaling. */
    float arousal_energy = 0.0f;
    float protection_energy = 0.0f;

    /* Map discrete arousal enum to continuous level (0-1) for energy calc.
     * Uses midpoint of each state's typical range for smooth interpolation. */
    static const float MEDULLA_AROUSAL_LEVELS[] = {
        0.02f,  /* COMA */
        0.10f,  /* DEEP_SLEEP */
        0.22f,  /* LIGHT_SLEEP */
        0.35f,  /* DROWSY */
        0.47f,  /* RELAXED */
        0.62f,  /* ALERT (optimal) */
        0.77f,  /* VIGILANT */
        0.90f,  /* HYPERVIGILANT */
        0.97f,  /* EMERGENCY */
    };
    float arousal_level = MEDULLA_AROUSAL_LEVELS[(int)integration->arousal_state];

    /* Arousal energy: inverted-U centered at alert (~0.62), sigma=0.3.
     * Deviation from optimal = higher free energy. */
    float optimal_arousal = 0.62f;
    float dev = arousal_level - optimal_arousal;
    float optimality = expf(-(dev * dev) / (2.0f * 0.3f * 0.3f));
    arousal_energy = 1.0f - optimality;  /* 0.0 at optimal, ~1.0 at extremes */

    /* Protection energy - normal is optimal */
    switch (integration->protection_level) {
        case MEDULLA_PROTECT_NORMAL:
            protection_energy = 0.1f;
            break;
        case MEDULLA_PROTECT_HEIGHTENED:
        case MEDULLA_PROTECT_MINIMAL:
            protection_energy = 0.3f;
            break;
        case MEDULLA_PROTECT_MAXIMUM:
            protection_energy = 0.5f;
            break;
        case MEDULLA_PROTECT_DISABLED:
            protection_energy = 0.9f;
            break;
    }

    /* Emergency mode adds energy */
    if (integration->in_emergency_mode) {
        return (arousal_energy + protection_energy) / 2.0f + 0.3f;
    }

    return (arousal_energy + protection_energy) / 2.0f;
}

static void medulla_get_health_metrics(void* ctx, health_metrics_t* metrics) {
    mesh_medulla_integration_t* integration = (mesh_medulla_integration_t*)ctx;

    if (!integration || integration->magic != MESH_MEDULLA_MAGIC || !metrics) {
        return;
    }

    memset(metrics, 0, sizeof(*metrics));
    metrics->participant = integration->participant_id;
    metrics->last_heartbeat_ns = nimcp_time_now_ns();

    /* Health based on state */
    metrics->is_healthy = !integration->in_emergency_mode &&
                          integration->arousal_state != MEDULLA_AROUSAL_COMA;

    /* Compute average latency (placeholder) */
    metrics->avg_latency_ms = 1.0f;

    /* Transaction stats */
    metrics->transactions_processed = integration->stats.transactions_received;
    metrics->transactions_failed = integration->stats.arousal_changes_rejected +
                                   integration->stats.protection_changes_rejected;
}

/* ============================================================================
 * Configuration Defaults
 * ============================================================================ */

nimcp_error_t mesh_medulla_default_config(mesh_medulla_config_t* config) {
    if (!config) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    memset(config, 0, sizeof(*config));

    config->initial_arousal = MEDULLA_AROUSAL_ALERT;
    config->initial_protection = MEDULLA_PROTECT_NORMAL;

    config->auto_emergency_on_critical_health = true;
    config->require_consensus_for_arousal = true;
    config->require_consensus_for_protection = false;
    config->allow_external_shutdown = true;

    config->arousal_change_timeout_ms = 1000;
    config->emergency_response_timeout_ms = 100;

    config->enable_health_monitoring = true;
    config->heartbeat_interval_ms = 1000;

    config->verbose_logging = false;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

mesh_medulla_integration_t* mesh_medulla_create(
    mesh_bootstrap_t* bootstrap,
    medulla_oblongata_t* medulla,
    const mesh_medulla_config_t* config
) {
    if (!bootstrap) {
        LOG_ERROR("Cannot create medulla integration without bootstrap");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "mesh_medulla_create: bootstrap is NULL");
        return NULL;
    }

    mesh_medulla_config_t default_config;
    if (!config) {
        mesh_medulla_default_config(&default_config);
        config = &default_config;
    }

    mesh_medulla_integration_t* integration = nimcp_calloc(1, sizeof(*integration));
    if (!integration) {
        LOG_ERROR("Failed to allocate medulla integration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_MEMORY,
            "mesh_medulla_create: allocation failed");
        return NULL;
    }

    integration->magic = MESH_MEDULLA_MAGIC;
    integration->config = *config;
    integration->bootstrap = bootstrap;
    integration->mesh_integration = mesh_bootstrap_get_integration(bootstrap);
    integration->registry = mesh_bootstrap_get_registry(bootstrap);
    integration->health_bridge = mesh_bootstrap_get_health_bridge(bootstrap);
    integration->exception_bridge = mesh_bootstrap_get_exception_bridge(bootstrap);
    integration->medulla = medulla;

    /* Set initial state */
    integration->arousal_state = config->initial_arousal;
    integration->protection_level = config->initial_protection;
    integration->in_emergency_mode = false;

    /* Initialize stats */
    integration->stats.current_arousal = config->initial_arousal;
    integration->stats.current_protection = config->initial_protection;
    integration->stats.in_emergency_mode = false;

    /* Create mutex */
    mutex_attr_t attr = {0};
    integration->mutex = nimcp_mutex_create(&attr);
    if (!integration->mutex) {
        LOG_ERROR("Failed to create medulla integration mutex");
        nimcp_free(integration);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_medulla_create: integration->mutex is NULL");
        return NULL;
    }

    LOG_INFO("Created medulla mesh integration");

    return integration;
}

void mesh_medulla_destroy(mesh_medulla_integration_t* integration) {
    if (!integration || integration->magic != MESH_MEDULLA_MAGIC) {
        return;
    }

    nimcp_mutex_lock(integration->mutex);

    /* Unregister from mesh if registered */
    if (integration->registered && integration->registry) {
        mesh_participant_unregister(integration->registry, integration->participant_id);
        integration->registered = false;
    }

    /* Clear magic to prevent reuse */
    integration->magic = 0;

    nimcp_mutex_unlock(integration->mutex);
    nimcp_mutex_destroy(integration->mutex);

    nimcp_free(integration);

    LOG_INFO("Destroyed medulla mesh integration");
}

/* ============================================================================
 * Participant Registration API
 * ============================================================================ */

nimcp_error_t mesh_medulla_register_participant(mesh_medulla_integration_t* integration) {
    if (!integration || integration->magic != MESH_MEDULLA_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);

    if (integration->registered) {
        nimcp_mutex_unlock(integration->mutex);
        LOG_WARN("Medulla already registered with mesh");
        return NIMCP_ERROR_ALREADY_EXISTS;
    }

    if (!integration->registry) {
        nimcp_mutex_unlock(integration->mutex);
        LOG_ERROR("No participant registry available");
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    medulla_heartbeat(integration, "registering", 0.0f);

    /* Initialize participant interface */
    mesh_participant_interface_init(&integration->participant_interface);

    strncpy(integration->participant_interface.module_name, "medulla_oblongata",
            MESH_MAX_NAME_LEN - 1);
    integration->participant_interface.type = MESH_PARTICIPANT_MODULE;
    integration->participant_interface.home_channel = MESH_CHANNEL_SUBCORTICAL;

    /* Set callbacks */
    integration->participant_interface.on_proposal = medulla_on_proposal;
    integration->participant_interface.on_endorse_request = medulla_on_endorse_request;
    integration->participant_interface.on_commit = medulla_on_commit;
    integration->participant_interface.get_free_energy = medulla_get_free_energy;
    integration->participant_interface.get_health_metrics = medulla_get_health_metrics;
    integration->participant_interface.user_context = integration;

    /* Register with mesh */
    mesh_participant_config_t reg_config;
    mesh_participant_config_init(&reg_config);
    reg_config.module_name = "medulla_oblongata";
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
        LOG_ERROR("Failed to register medulla with mesh: %d", err);
        NIMCP_THROW_TO_IMMUNE(err, "mesh_medulla_register_participant: registration failed");
        return err;
    }

    integration->registered = true;

    /* Register health agent with health bridge if available */
    if (integration->health_bridge && integration->health_agent) {
        mesh_health_bridge_register_agent(
            integration->health_bridge,
            integration->participant_id,
            integration->health_agent
        );
    }

    nimcp_mutex_unlock(integration->mutex);

    LOG_INFO("Medulla registered with mesh: participant_id=0x%016lX",
             (unsigned long)integration->participant_id);

    medulla_heartbeat(integration, "registered", 1.0f);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_medulla_unregister_participant(mesh_medulla_integration_t* integration) {
    if (!integration || integration->magic != MESH_MEDULLA_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);

    if (!integration->registered) {
        nimcp_mutex_unlock(integration->mutex);
        return NIMCP_SUCCESS;
    }

    /* Unregister health agent */
    if (integration->health_bridge) {
        mesh_health_bridge_unregister_agent(
            integration->health_bridge,
            integration->participant_id
        );
    }

    /* Unregister from mesh */
    if (integration->registry) {
        mesh_participant_unregister(integration->registry, integration->participant_id);
    }

    integration->registered = false;
    integration->participant_id = 0;

    nimcp_mutex_unlock(integration->mutex);

    LOG_INFO("Medulla unregistered from mesh");

    return NIMCP_SUCCESS;
}

mesh_participant_id_t mesh_medulla_get_participant_id(
    const mesh_medulla_integration_t* integration
) {
    if (!integration || integration->magic != MESH_MEDULLA_MAGIC) {
        return 0;
    }
    return integration->participant_id;
}

bool mesh_medulla_is_registered(const mesh_medulla_integration_t* integration) {
    if (!integration || integration->magic != MESH_MEDULLA_MAGIC) {
        return false;
    }
    return integration->registered;
}

/* ============================================================================
 * Transaction Handling API
 * ============================================================================ */

nimcp_error_t mesh_medulla_on_transaction(
    mesh_medulla_integration_t* integration,
    const mesh_transaction_t* tx
) {
    if (!integration || integration->magic != MESH_MEDULLA_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!tx) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Delegate to on_commit handler */
    return medulla_on_commit(integration, tx);
}

nimcp_error_t mesh_medulla_endorse_transaction(
    mesh_medulla_integration_t* integration,
    const mesh_transaction_t* tx,
    mesh_endorsement_t* endorsement
) {
    if (!integration || integration->magic != MESH_MEDULLA_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!tx || !endorsement) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Delegate to on_endorse_request handler */
    return medulla_on_endorse_request(integration, tx, endorsement);
}

/* ============================================================================
 * State Change Proposal API
 * ============================================================================ */

nimcp_error_t mesh_medulla_propose_arousal_change(
    mesh_medulla_integration_t* integration,
    mesh_medulla_arousal_state_t target_state,
    const char* reason,
    bool urgent
) {
    if (!integration || integration->magic != MESH_MEDULLA_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!integration->registered) {
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    nimcp_mutex_lock(integration->mutex);

    integration->stats.arousal_changes_proposed++;

    /* Create payload */
    mesh_medulla_arousal_payload_t payload = {
        .target_state = target_state,
        .current_state = integration->arousal_state,
        .urgent = urgent
    };
    if (reason) {
        strncpy(payload.reason, reason, sizeof(payload.reason) - 1);
    }

    /* Create transaction */
    mesh_tx_type_t tx_type = urgent ? MESH_TX_EMERGENCY_OVERRIDE :
                             (mesh_tx_type_t)MESH_TX_MEDULLA_AROUSAL_CHANGE;

    mesh_transaction_t* tx = mesh_transaction_create(
        tx_type,
        integration->participant_id,
        MESH_CHANNEL_SUBCORTICAL
    );

    if (!tx) {
        nimcp_mutex_unlock(integration->mutex);
        integration->stats.arousal_changes_rejected++;
        return NIMCP_ERROR_OUT_OF_MEMORY;
    }

    /* Set payload */
    mesh_transaction_set_payload(tx, &payload, sizeof(payload));
    mesh_transaction_set_timeout(tx, integration->config.arousal_change_timeout_ms);

    /* Submit transaction */
    nimcp_error_t err = NIMCP_SUCCESS;
    if (integration->mesh_integration) {
        err = mesh_integration_submit_transaction(integration->mesh_integration, tx);
    }

    mesh_transaction_destroy(tx);

    nimcp_mutex_unlock(integration->mutex);

    if (err != NIMCP_SUCCESS) {
        integration->stats.arousal_changes_rejected++;
        LOG_ERROR("Failed to propose arousal change: %d", err);
    }

    medulla_heartbeat(integration, "arousal_proposal", 1.0f);

    return err;
}

nimcp_error_t mesh_medulla_propose_protection_change(
    mesh_medulla_integration_t* integration,
    mesh_medulla_protection_level_t target_level,
    const char* reason
) {
    if (!integration || integration->magic != MESH_MEDULLA_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!integration->registered) {
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    nimcp_mutex_lock(integration->mutex);

    integration->stats.protection_changes_proposed++;

    /* Create payload */
    mesh_medulla_protection_payload_t payload = {
        .target_level = target_level,
        .current_level = integration->protection_level
    };
    if (reason) {
        strncpy(payload.reason, reason, sizeof(payload.reason) - 1);
    }

    /* Create transaction */
    mesh_transaction_t* tx = mesh_transaction_create(
        (mesh_tx_type_t)MESH_TX_MEDULLA_PROTECTION_CHANGE,
        integration->participant_id,
        MESH_CHANNEL_SUBCORTICAL
    );

    if (!tx) {
        nimcp_mutex_unlock(integration->mutex);
        integration->stats.protection_changes_rejected++;
        return NIMCP_ERROR_OUT_OF_MEMORY;
    }

    /* Set payload */
    mesh_transaction_set_payload(tx, &payload, sizeof(payload));

    /* Submit transaction */
    nimcp_error_t err = NIMCP_SUCCESS;
    if (integration->mesh_integration) {
        err = mesh_integration_submit_transaction(integration->mesh_integration, tx);
    }

    mesh_transaction_destroy(tx);

    nimcp_mutex_unlock(integration->mutex);

    if (err != NIMCP_SUCCESS) {
        integration->stats.protection_changes_rejected++;
        LOG_ERROR("Failed to propose protection change: %d", err);
    }

    return err;
}

nimcp_error_t mesh_medulla_emergency_shutdown(
    mesh_medulla_integration_t* integration,
    uint32_t emergency_code,
    const char* reason,
    bool force_immediate
) {
    if (!integration || integration->magic != MESH_MEDULLA_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    LOG_ERROR("EMERGENCY SHUTDOWN requested: code=%u reason=%s force=%d",
              emergency_code, reason ? reason : "(none)", force_immediate);

    nimcp_mutex_lock(integration->mutex);

    /* If force immediate, apply directly without mesh consensus */
    if (force_immediate) {
        integration->in_emergency_mode = true;
        integration->stats.in_emergency_mode = true;
        integration->stats.emergency_shutdowns_triggered++;
        integration->stats.last_emergency_ns = nimcp_time_now_ns();

        update_arousal_state(integration, MEDULLA_AROUSAL_EMERGENCY);
        update_protection_level(integration, MEDULLA_PROTECT_MAXIMUM);

        if (integration->emergency_callback) {
            integration->emergency_callback(emergency_code,
                                            reason ? reason : "Emergency shutdown",
                                            integration->emergency_callback_ctx);
        }

        nimcp_mutex_unlock(integration->mutex);
        return NIMCP_SUCCESS;
    }

    /* Create payload */
    mesh_medulla_emergency_payload_t payload = {
        .emergency_code = emergency_code,
        .requesting_module = integration->participant_id,
        .force_immediate = force_immediate
    };
    if (reason) {
        strncpy(payload.reason, reason, sizeof(payload.reason) - 1);
    }

    /* Create emergency transaction */
    mesh_transaction_t* tx = mesh_transaction_create(
        (mesh_tx_type_t)MESH_TX_MEDULLA_EMERGENCY_SHUTDOWN,
        integration->participant_id,
        MESH_CHANNEL_SUBCORTICAL
    );

    if (!tx) {
        nimcp_mutex_unlock(integration->mutex);
        return NIMCP_ERROR_OUT_OF_MEMORY;
    }

    tx->is_emergency = true;
    mesh_transaction_set_payload(tx, &payload, sizeof(payload));
    mesh_transaction_set_timeout(tx, integration->config.emergency_response_timeout_ms);

    /* Submit transaction */
    nimcp_error_t err = NIMCP_SUCCESS;
    if (integration->mesh_integration) {
        err = mesh_integration_submit_transaction(integration->mesh_integration, tx);
    }

    mesh_transaction_destroy(tx);

    nimcp_mutex_unlock(integration->mutex);

    return err;
}

nimcp_error_t mesh_medulla_emergency_recovery(mesh_medulla_integration_t* integration) {
    if (!integration || integration->magic != MESH_MEDULLA_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    LOG_INFO("Emergency recovery requested");

    nimcp_mutex_lock(integration->mutex);

    if (!integration->in_emergency_mode) {
        nimcp_mutex_unlock(integration->mutex);
        LOG_INFO("Not in emergency mode - no recovery needed");
        return NIMCP_SUCCESS;
    }

    /* Create recovery transaction */
    mesh_transaction_t* tx = mesh_transaction_create(
        (mesh_tx_type_t)MESH_TX_MEDULLA_EMERGENCY_RECOVERY,
        integration->participant_id,
        MESH_CHANNEL_SUBCORTICAL
    );

    if (!tx) {
        nimcp_mutex_unlock(integration->mutex);
        return NIMCP_ERROR_OUT_OF_MEMORY;
    }

    /* Submit transaction */
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

mesh_medulla_arousal_state_t mesh_medulla_get_arousal_state(
    const mesh_medulla_integration_t* integration
) {
    if (!integration || integration->magic != MESH_MEDULLA_MAGIC) {
        return MEDULLA_AROUSAL_COMA;
    }
    return integration->arousal_state;
}

mesh_medulla_protection_level_t mesh_medulla_get_protection_level(
    const mesh_medulla_integration_t* integration
) {
    if (!integration || integration->magic != MESH_MEDULLA_MAGIC) {
        return MEDULLA_PROTECT_DISABLED;
    }
    return integration->protection_level;
}

bool mesh_medulla_is_emergency_mode(const mesh_medulla_integration_t* integration) {
    if (!integration || integration->magic != MESH_MEDULLA_MAGIC) {
        return false;
    }
    return integration->in_emergency_mode;
}

/* ============================================================================
 * Callback Registration API
 * ============================================================================ */

nimcp_error_t mesh_medulla_set_arousal_callback(
    mesh_medulla_integration_t* integration,
    mesh_medulla_arousal_callback_t callback,
    void* ctx
) {
    if (!integration || integration->magic != MESH_MEDULLA_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);
    integration->arousal_callback = callback;
    integration->arousal_callback_ctx = ctx;
    nimcp_mutex_unlock(integration->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_medulla_set_protection_callback(
    mesh_medulla_integration_t* integration,
    mesh_medulla_protection_callback_t callback,
    void* ctx
) {
    if (!integration || integration->magic != MESH_MEDULLA_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);
    integration->protection_callback = callback;
    integration->protection_callback_ctx = ctx;
    nimcp_mutex_unlock(integration->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_medulla_set_emergency_callback(
    mesh_medulla_integration_t* integration,
    mesh_medulla_emergency_callback_t callback,
    void* ctx
) {
    if (!integration || integration->magic != MESH_MEDULLA_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);
    integration->emergency_callback = callback;
    integration->emergency_callback_ctx = ctx;
    nimcp_mutex_unlock(integration->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics API
 * ============================================================================ */

nimcp_error_t mesh_medulla_get_stats(
    const mesh_medulla_integration_t* integration,
    mesh_medulla_stats_t* stats
) {
    if (!integration || integration->magic != MESH_MEDULLA_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!stats) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(((mesh_medulla_integration_t*)integration)->mutex);
    *stats = integration->stats;
    nimcp_mutex_unlock(((mesh_medulla_integration_t*)integration)->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_medulla_reset_stats(mesh_medulla_integration_t* integration) {
    if (!integration || integration->magic != MESH_MEDULLA_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);

    /* Preserve current state */
    mesh_medulla_arousal_state_t arousal = integration->stats.current_arousal;
    mesh_medulla_protection_level_t protection = integration->stats.current_protection;
    bool emergency = integration->stats.in_emergency_mode;

    memset(&integration->stats, 0, sizeof(integration->stats));

    /* Restore current state */
    integration->stats.current_arousal = arousal;
    integration->stats.current_protection = protection;
    integration->stats.in_emergency_mode = emergency;

    nimcp_mutex_unlock(integration->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Health Agent Integration API
 * ============================================================================ */

nimcp_error_t mesh_medulla_set_health_agent(
    mesh_medulla_integration_t* integration,
    nimcp_health_agent_t* agent
) {
    if (!integration || integration->magic != MESH_MEDULLA_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);
    integration->health_agent = agent;
    nimcp_mutex_unlock(integration->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_medulla_heartbeat(
    mesh_medulla_integration_t* integration,
    const char* operation,
    float progress
) {
    if (!integration || integration->magic != MESH_MEDULLA_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    medulla_heartbeat(integration, operation, progress);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* mesh_medulla_arousal_to_string(mesh_medulla_arousal_state_t state) {
    if (state >= sizeof(arousal_state_names) / sizeof(arousal_state_names[0])) {
        return "unknown";
    }
    return arousal_state_names[state];
}

const char* mesh_medulla_protection_to_string(mesh_medulla_protection_level_t level) {
    if (level >= sizeof(protection_level_names) / sizeof(protection_level_names[0])) {
        return "unknown";
    }
    return protection_level_names[level];
}

bool mesh_medulla_is_medulla_transaction(mesh_tx_type_t tx_type) {
    return tx_type >= MESH_TX_MEDULLA_BASE &&
           tx_type <= MESH_TX_MEDULLA_VITALS_UPDATE;
}
