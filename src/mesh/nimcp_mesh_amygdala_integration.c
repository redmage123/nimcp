/**
 * @file nimcp_mesh_amygdala_integration.c
 * @brief Amygdala Mesh Network Integration Implementation
 *
 * WHAT: Connects the amygdala module to the mesh network
 * WHY:  Enable coordinated emotional processing via distributed consensus
 * HOW:  Register as SUBCORTICAL participant with VETO capability
 *
 * BIOLOGICAL CONTEXT:
 * The amygdala can veto dangerous actions via the mesh network, similar to
 * how it overrides cortical decisions in fear situations.
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 * @version 1.0.0
 */

#include "mesh/nimcp_mesh_amygdala_integration.h"
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

#define LOG_MODULE "MESH_AMYGDALA"

/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define MESH_AMYGDALA_MAGIC 0x414D5947  /* "AMYG" */

/* Participant type for amygdala */
#define MESH_PARTICIPANT_AMYGDALA 0x0017

/* ============================================================================
 * String Conversion Tables
 * ============================================================================ */

static const char* threat_level_names[] = {
    [MESH_AMYGDALA_THREAT_NONE] = "none",
    [MESH_AMYGDALA_THREAT_LOW] = "low",
    [MESH_AMYGDALA_THREAT_MODERATE] = "moderate",
    [MESH_AMYGDALA_THREAT_HIGH] = "high",
    [MESH_AMYGDALA_THREAT_SEVERE] = "severe",
};

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct mesh_amygdala_integration {
    uint32_t magic;

    /* Configuration */
    mesh_amygdala_config_t config;

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

    /* Connected amygdala module */
    amygdala_t* amygdala;

    /* Current state */
    mesh_amygdala_threat_level_t current_threat_level;
    float current_anxiety;
    float current_fear;

    /* Callbacks */
    mesh_amygdala_threat_callback_t threat_callback;
    void* threat_callback_ctx;

    mesh_amygdala_veto_callback_t veto_callback;
    void* veto_callback_ctx;

    /* Health agent */
    nimcp_health_agent_t* health_agent;

    /* Statistics */
    mesh_amygdala_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Transaction sequence counter */
    uint64_t tx_sequence;
};

/* ============================================================================
 * Forward Declarations for Participant Callbacks
 * ============================================================================ */

static nimcp_error_t amygdala_on_proposal(void* ctx, const mesh_transaction_t* tx);
static nimcp_error_t amygdala_on_endorse_request(void* ctx, const mesh_transaction_t* tx,
                                                  mesh_endorsement_t* endorsement);
static nimcp_error_t amygdala_on_commit(void* ctx, const mesh_transaction_t* tx);
static float amygdala_get_free_energy(void* ctx);
static void amygdala_get_health_metrics(void* ctx, health_metrics_t* metrics);

/* ============================================================================
 * Internal Helper: Send Heartbeat
 * ============================================================================ */

static void amygdala_heartbeat(mesh_amygdala_integration_t* integration,
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

static void update_threat_level(mesh_amygdala_integration_t* integration,
                                mesh_amygdala_threat_level_t new_level) {
    mesh_amygdala_threat_level_t old_level = integration->current_threat_level;

    if (old_level == new_level) {
        return;
    }

    integration->current_threat_level = new_level;
    integration->stats.current_threat_level = new_level;
    integration->stats.last_threat_detection_ns = nimcp_time_now_ns();
    integration->stats.threats_detected++;

    if (new_level < 5) {
        integration->stats.threats_by_level[new_level]++;
    }

    LOG_INFO("Threat level changed: %s -> %s",
             mesh_amygdala_threat_to_string(old_level),
             mesh_amygdala_threat_to_string(new_level));

    /* Invoke callback */
    if (integration->threat_callback) {
        integration->threat_callback(old_level, new_level,
                                     integration->threat_callback_ctx);
    }
}

/* ============================================================================
 * Internal Helper: Evaluate Threat for Veto Decision
 * ============================================================================ */

static bool should_veto_transaction(mesh_amygdala_integration_t* integration,
                                    const mesh_transaction_t* tx) {
    if (!integration->config.enable_veto_capability) {
        return false;
    }

    /* Auto-veto on severe threat if configured */
    if (integration->config.auto_veto_on_severe_threat &&
        integration->current_threat_level >= MESH_AMYGDALA_THREAT_SEVERE) {

        /* Don't veto emergency-related transactions */
        if (tx->is_emergency) {
            return false;
        }

        LOG_WARN("Auto-vetoing transaction during severe threat: type=%d", tx->type);
        return true;
    }

    /* Veto if threat level exceeds threshold */
    if (integration->current_threat_level >= integration->config.veto_threshold) {
        /* Allow emergency transactions through */
        if (tx->is_emergency) {
            return false;
        }

        /* Veto non-critical transactions during high threat */
        if (tx->type < 0x1000) {  /* Assume critical types are < 0x1000 */
            return false;
        }

        return true;
    }

    return false;
}

/* ============================================================================
 * Internal Helper: Handle Transaction by Type
 * ============================================================================ */

static nimcp_error_t handle_threat_detected(mesh_amygdala_integration_t* integration,
                                            const mesh_transaction_t* tx) {
    if (tx->payload_size < sizeof(mesh_amygdala_threat_payload_t)) {
        LOG_WARN("Threat detected payload too small");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const mesh_amygdala_threat_payload_t* payload =
        (const mesh_amygdala_threat_payload_t*)tx->payload;

    update_threat_level(integration, payload->threat_level);

    LOG_INFO("Threat detected via mesh: level=%s intensity=%.2f",
             mesh_amygdala_threat_to_string(payload->threat_level),
             payload->threat_intensity);

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_fear_response(mesh_amygdala_integration_t* integration,
                                          const mesh_transaction_t* tx) {
    (void)tx;

    integration->stats.fear_responses_triggered++;

    LOG_INFO("Fear response triggered via mesh");

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_veto(mesh_amygdala_integration_t* integration,
                                 const mesh_transaction_t* tx) {
    if (tx->payload_size < sizeof(mesh_amygdala_veto_payload_t)) {
        LOG_WARN("Veto payload too small");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const mesh_amygdala_veto_payload_t* payload =
        (const mesh_amygdala_veto_payload_t*)tx->payload;

    integration->stats.veto_requests_applied++;
    integration->stats.last_veto_ns = nimcp_time_now_ns();

    LOG_WARN("VETO applied: target=0x%016lX reason=%s auto=%d",
             (unsigned long)payload->target_transaction,
             payload->reason,
             payload->is_automatic);

    /* Invoke callback */
    if (integration->veto_callback) {
        integration->veto_callback(payload->target_transaction,
                                   payload->reason,
                                   integration->veto_callback_ctx);
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_anxiety_change(mesh_amygdala_integration_t* integration,
                                           const mesh_transaction_t* tx) {
    if (tx->payload_size < sizeof(float)) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    float new_anxiety = *(const float*)tx->payload;

    integration->current_anxiety = new_anxiety;
    integration->stats.current_anxiety = new_anxiety;

    LOG_DEBUG("Anxiety level changed via mesh: %.2f", new_anxiety);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Participant Callback Implementations
 * ============================================================================ */

static nimcp_error_t amygdala_on_proposal(void* ctx, const mesh_transaction_t* tx) {
    mesh_amygdala_integration_t* integration = (mesh_amygdala_integration_t*)ctx;

    if (!integration || integration->magic != MESH_AMYGDALA_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!tx) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    integration->stats.transactions_received++;

    /* Check if this is an amygdala-related transaction */
    if (!mesh_amygdala_is_amygdala_transaction(tx->type)) {
        /* Not our transaction - no opinion yet (will evaluate in endorsement) */
        return NIMCP_SUCCESS;
    }

    amygdala_heartbeat(integration, "proposal_received", 0.5f);

    /* Accept the proposal */
    return NIMCP_SUCCESS;
}

static nimcp_error_t amygdala_on_endorse_request(void* ctx, const mesh_transaction_t* tx,
                                                  mesh_endorsement_t* endorsement) {
    mesh_amygdala_integration_t* integration = (mesh_amygdala_integration_t*)ctx;

    if (!integration || integration->magic != MESH_AMYGDALA_MAGIC) {
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

    /* Check if we should veto this transaction */
    if (should_veto_transaction(integration, tx)) {
        endorsement->result = ENDORSEMENT_REJECTED;
        integration->stats.transactions_vetoed++;
        integration->stats.veto_requests_issued++;

        LOG_WARN("Amygdala vetoing transaction: type=%d threat_level=%s",
                 tx->type, mesh_amygdala_threat_to_string(integration->current_threat_level));

        return NIMCP_SUCCESS;
    }

    /* Endorse amygdala-related transactions after validation */
    if (mesh_amygdala_is_amygdala_transaction(tx->type)) {
        /* Validate payload based on type */
        bool valid = true;

        switch ((mesh_amygdala_tx_type_t)tx->type) {
            case MESH_TX_AMYGDALA_THREAT_DETECTED:
                if (tx->payload_size < sizeof(mesh_amygdala_threat_payload_t)) {
                    valid = false;
                }
                break;

            case MESH_TX_AMYGDALA_VETO:
                if (tx->payload_size < sizeof(mesh_amygdala_veto_payload_t)) {
                    valid = false;
                }
                break;

            default:
                /* Accept other amygdala transactions */
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
        /* Not an amygdala transaction - approve unless in high threat state */
        if (integration->current_threat_level >= MESH_AMYGDALA_THREAT_HIGH) {
            /* Be cautious during high threat */
            endorsement->result = ENDORSEMENT_ABSTAIN;
        } else {
            endorsement->result = ENDORSEMENT_APPROVED;
            integration->stats.transactions_endorsed++;
        }
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t amygdala_on_commit(void* ctx, const mesh_transaction_t* tx) {
    mesh_amygdala_integration_t* integration = (mesh_amygdala_integration_t*)ctx;

    if (!integration || integration->magic != MESH_AMYGDALA_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!tx) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Handle amygdala-specific transactions */
    if (!mesh_amygdala_is_amygdala_transaction(tx->type)) {
        return NIMCP_SUCCESS;
    }

    nimcp_mutex_lock(integration->mutex);

    nimcp_error_t result = NIMCP_SUCCESS;

    switch ((mesh_amygdala_tx_type_t)tx->type) {
        case MESH_TX_AMYGDALA_THREAT_DETECTED:
            result = handle_threat_detected(integration, tx);
            break;

        case MESH_TX_AMYGDALA_FEAR_RESPONSE:
            result = handle_fear_response(integration, tx);
            break;

        case MESH_TX_AMYGDALA_FEAR_CONDITIONING:
            integration->stats.fear_conditioning_events++;
            break;

        case MESH_TX_AMYGDALA_FEAR_EXTINCTION:
            integration->stats.fear_extinction_events++;
            break;

        case MESH_TX_AMYGDALA_ANXIETY_CHANGE:
            result = handle_anxiety_change(integration, tx);
            break;

        case MESH_TX_AMYGDALA_VETO:
            result = handle_veto(integration, tx);
            break;

        case MESH_TX_AMYGDALA_VALENCE_UPDATE:
        case MESH_TX_AMYGDALA_MEMORY_TAG:
            /* Track but no special handling */
            break;

        default:
            break;
    }

    nimcp_mutex_unlock(integration->mutex);

    amygdala_heartbeat(integration, "commit_processed", 1.0f);

    return result;
}

static float amygdala_get_free_energy(void* ctx) {
    mesh_amygdala_integration_t* integration = (mesh_amygdala_integration_t*)ctx;

    if (!integration || integration->magic != MESH_AMYGDALA_MAGIC) {
        return -1.0f;
    }

    /* Free energy based on threat level and anxiety */
    float threat_energy = 0.0f;
    float anxiety_energy = integration->current_anxiety;

    /* Threat level contribution */
    switch (integration->current_threat_level) {
        case MESH_AMYGDALA_THREAT_NONE:
            threat_energy = 0.1f;  /* Optimal */
            break;
        case MESH_AMYGDALA_THREAT_LOW:
            threat_energy = 0.2f;
            break;
        case MESH_AMYGDALA_THREAT_MODERATE:
            threat_energy = 0.4f;
            break;
        case MESH_AMYGDALA_THREAT_HIGH:
            threat_energy = 0.7f;
            break;
        case MESH_AMYGDALA_THREAT_SEVERE:
            threat_energy = 1.0f;  /* Maximum energy = high uncertainty */
            break;
    }

    return (threat_energy + anxiety_energy) / 2.0f;
}

static void amygdala_get_health_metrics(void* ctx, health_metrics_t* metrics) {
    mesh_amygdala_integration_t* integration = (mesh_amygdala_integration_t*)ctx;

    if (!integration || integration->magic != MESH_AMYGDALA_MAGIC || !metrics) {
        return;
    }

    memset(metrics, 0, sizeof(*metrics));
    metrics->participant = integration->participant_id;
    metrics->last_heartbeat_ns = nimcp_time_now_ns();

    /* Health based on threat level */
    metrics->is_healthy = integration->current_threat_level < MESH_AMYGDALA_THREAT_SEVERE;

    /* Compute average latency (placeholder) */
    metrics->avg_latency_ms = 0.5f;  /* Amygdala is fast! */

    /* Transaction stats */
    metrics->transactions_processed = integration->stats.transactions_received;
    metrics->transactions_failed = integration->stats.transactions_vetoed;
}

/* ============================================================================
 * Configuration Defaults
 * ============================================================================ */

nimcp_error_t mesh_amygdala_default_config(mesh_amygdala_config_t* config) {
    if (!config) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    memset(config, 0, sizeof(*config));

    config->initial_threat_level = MESH_AMYGDALA_THREAT_NONE;
    config->initial_anxiety = 0.0f;

    config->enable_veto_capability = true;
    config->veto_threshold = MESH_AMYGDALA_THREAT_HIGH;
    config->auto_veto_on_severe_threat = true;

    config->threat_response_timeout_ms = 100;  /* Fast response */
    config->veto_timeout_ms = 50;              /* Very fast veto */

    config->enable_health_monitoring = true;
    config->heartbeat_interval_ms = 500;       /* Faster heartbeat for amygdala */

    config->verbose_logging = false;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

mesh_amygdala_integration_t* mesh_amygdala_create(
    mesh_bootstrap_t* bootstrap,
    amygdala_t* amygdala,
    const mesh_amygdala_config_t* config
) {
    if (!bootstrap) {
        LOG_ERROR("Cannot create amygdala integration without bootstrap");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "mesh_amygdala_create: bootstrap is NULL");
        return NULL;
    }

    mesh_amygdala_config_t default_config;
    if (!config) {
        mesh_amygdala_default_config(&default_config);
        config = &default_config;
    }

    mesh_amygdala_integration_t* integration = nimcp_calloc(1, sizeof(*integration));
    if (!integration) {
        LOG_ERROR("Failed to allocate amygdala integration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_MEMORY,
            "mesh_amygdala_create: allocation failed");
        return NULL;
    }

    integration->magic = MESH_AMYGDALA_MAGIC;
    integration->config = *config;
    integration->bootstrap = bootstrap;
    integration->mesh_integration = mesh_bootstrap_get_integration(bootstrap);
    integration->registry = mesh_bootstrap_get_registry(bootstrap);
    integration->health_bridge = mesh_bootstrap_get_health_bridge(bootstrap);
    integration->exception_bridge = mesh_bootstrap_get_exception_bridge(bootstrap);
    integration->amygdala = amygdala;

    /* Set initial state */
    integration->current_threat_level = config->initial_threat_level;
    integration->current_anxiety = config->initial_anxiety;
    integration->current_fear = 0.0f;

    /* Initialize stats */
    integration->stats.current_threat_level = config->initial_threat_level;
    integration->stats.current_anxiety = config->initial_anxiety;
    integration->stats.current_fear = 0.0f;

    /* Create mutex */
    mutex_attr_t attr = {0};
    integration->mutex = nimcp_mutex_create(&attr);
    if (!integration->mutex) {
        LOG_ERROR("Failed to create amygdala integration mutex");
        nimcp_free(integration);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_amygdala_create: integration->mutex is NULL");
        return NULL;
    }

    LOG_INFO("Created amygdala mesh integration (veto=%s)",
             config->enable_veto_capability ? "enabled" : "disabled");

    return integration;
}

void mesh_amygdala_destroy(mesh_amygdala_integration_t* integration) {
    if (!integration || integration->magic != MESH_AMYGDALA_MAGIC) {
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

    LOG_INFO("Destroyed amygdala mesh integration");
}

/* ============================================================================
 * Participant Registration API
 * ============================================================================ */

nimcp_error_t mesh_amygdala_register_participant(mesh_amygdala_integration_t* integration) {
    if (!integration || integration->magic != MESH_AMYGDALA_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);

    if (integration->registered) {
        nimcp_mutex_unlock(integration->mutex);
        LOG_WARN("Amygdala already registered with mesh");
        return NIMCP_ERROR_ALREADY_EXISTS;
    }

    if (!integration->registry) {
        nimcp_mutex_unlock(integration->mutex);
        LOG_ERROR("No participant registry available");
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    amygdala_heartbeat(integration, "registering", 0.0f);

    /* Initialize participant interface */
    mesh_participant_interface_init(&integration->participant_interface);

    strncpy(integration->participant_interface.module_name, "amygdala",
            MESH_MAX_NAME_LEN - 1);
    integration->participant_interface.type = MESH_PARTICIPANT_MODULE;
    integration->participant_interface.home_channel = MESH_CHANNEL_SUBCORTICAL;

    /* Set callbacks */
    integration->participant_interface.on_proposal = amygdala_on_proposal;
    integration->participant_interface.on_endorse_request = amygdala_on_endorse_request;
    integration->participant_interface.on_commit = amygdala_on_commit;
    integration->participant_interface.get_free_energy = amygdala_get_free_energy;
    integration->participant_interface.get_health_metrics = amygdala_get_health_metrics;
    integration->participant_interface.user_context = integration;

    /* Register with mesh */
    mesh_participant_config_t reg_config;
    mesh_participant_config_init(&reg_config);
    reg_config.module_name = "amygdala";
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
        LOG_ERROR("Failed to register amygdala with mesh: %d", err);
        NIMCP_THROW_TO_IMMUNE(err, "mesh_amygdala_register_participant: registration failed");
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

    LOG_INFO("Amygdala registered with mesh: participant_id=0x%016lX (veto=%s)",
             (unsigned long)integration->participant_id,
             integration->config.enable_veto_capability ? "enabled" : "disabled");

    amygdala_heartbeat(integration, "registered", 1.0f);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_amygdala_unregister_participant(mesh_amygdala_integration_t* integration) {
    if (!integration || integration->magic != MESH_AMYGDALA_MAGIC) {
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

    LOG_INFO("Amygdala unregistered from mesh");

    return NIMCP_SUCCESS;
}

mesh_participant_id_t mesh_amygdala_get_participant_id(
    const mesh_amygdala_integration_t* integration
) {
    if (!integration || integration->magic != MESH_AMYGDALA_MAGIC) {
        return 0;
    }
    return integration->participant_id;
}

bool mesh_amygdala_is_registered(const mesh_amygdala_integration_t* integration) {
    if (!integration || integration->magic != MESH_AMYGDALA_MAGIC) {
        return false;
    }
    return integration->registered;
}

/* ============================================================================
 * Threat and Emotional API
 * ============================================================================ */

nimcp_error_t mesh_amygdala_report_threat(
    mesh_amygdala_integration_t* integration,
    mesh_amygdala_threat_level_t level,
    float intensity,
    const char* description
) {
    if (!integration || integration->magic != MESH_AMYGDALA_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!integration->registered) {
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    nimcp_mutex_lock(integration->mutex);

    /* Create payload */
    mesh_amygdala_threat_payload_t payload = {
        .threat_level = level,
        .threat_intensity = intensity,
        .threat_source = 0
    };
    if (description) {
        strncpy(payload.description, description, sizeof(payload.description) - 1);
    }

    /* Create transaction */
    mesh_transaction_t* tx = mesh_transaction_create(
        (mesh_tx_type_t)MESH_TX_AMYGDALA_THREAT_DETECTED,
        integration->participant_id,
        MESH_CHANNEL_SUBCORTICAL
    );

    if (!tx) {
        nimcp_mutex_unlock(integration->mutex);
        return NIMCP_ERROR_OUT_OF_MEMORY;
    }

    /* Set as urgent if threat is high */
    if (level >= MESH_AMYGDALA_THREAT_HIGH) {
        tx->is_emergency = true;
    }

    mesh_transaction_set_payload(tx, &payload, sizeof(payload));
    mesh_transaction_set_timeout(tx, integration->config.threat_response_timeout_ms);

    /* Submit transaction */
    nimcp_error_t err = NIMCP_SUCCESS;
    if (integration->mesh_integration) {
        err = mesh_integration_submit_transaction(integration->mesh_integration, tx);
    }

    mesh_transaction_destroy(tx);

    nimcp_mutex_unlock(integration->mutex);

    if (err == NIMCP_SUCCESS) {
        LOG_INFO("Threat reported via mesh: level=%s intensity=%.2f",
                 mesh_amygdala_threat_to_string(level), intensity);
    } else {
        LOG_ERROR("Failed to report threat: %d", err);
    }

    amygdala_heartbeat(integration, "threat_reported", 1.0f);

    return err;
}

nimcp_error_t mesh_amygdala_issue_veto(
    mesh_amygdala_integration_t* integration,
    mesh_participant_id_t target_tx,
    const char* reason
) {
    if (!integration || integration->magic != MESH_AMYGDALA_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!integration->registered) {
        return NIMCP_ERROR_NOT_INITIALIZED;
    }
    if (!integration->config.enable_veto_capability) {
        return NIMCP_ERROR_NOT_IMPLEMENTED;
    }

    nimcp_mutex_lock(integration->mutex);

    integration->stats.veto_requests_issued++;

    /* Create payload */
    mesh_amygdala_veto_payload_t payload = {
        .target_transaction = target_tx,
        .threat_level = integration->current_threat_level,
        .is_automatic = false
    };
    if (reason) {
        strncpy(payload.reason, reason, sizeof(payload.reason) - 1);
    }

    /* Create transaction */
    mesh_transaction_t* tx = mesh_transaction_create(
        (mesh_tx_type_t)MESH_TX_AMYGDALA_VETO,
        integration->participant_id,
        MESH_CHANNEL_SUBCORTICAL
    );

    if (!tx) {
        nimcp_mutex_unlock(integration->mutex);
        return NIMCP_ERROR_OUT_OF_MEMORY;
    }

    tx->is_emergency = true;  /* Vetoes are always urgent */
    mesh_transaction_set_payload(tx, &payload, sizeof(payload));
    mesh_transaction_set_timeout(tx, integration->config.veto_timeout_ms);

    /* Submit transaction */
    nimcp_error_t err = NIMCP_SUCCESS;
    if (integration->mesh_integration) {
        err = mesh_integration_submit_transaction(integration->mesh_integration, tx);
    }

    mesh_transaction_destroy(tx);

    nimcp_mutex_unlock(integration->mutex);

    if (err == NIMCP_SUCCESS) {
        LOG_WARN("Veto issued: target=0x%016lX reason=%s",
                 (unsigned long)target_tx, reason ? reason : "(none)");
    } else {
        LOG_ERROR("Failed to issue veto: %d", err);
    }

    amygdala_heartbeat(integration, "veto_issued", 1.0f);

    return err;
}

nimcp_error_t mesh_amygdala_update_anxiety(
    mesh_amygdala_integration_t* integration,
    float new_anxiety
) {
    if (!integration || integration->magic != MESH_AMYGDALA_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!integration->registered) {
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    nimcp_mutex_lock(integration->mutex);

    /* Create transaction */
    mesh_transaction_t* tx = mesh_transaction_create(
        (mesh_tx_type_t)MESH_TX_AMYGDALA_ANXIETY_CHANGE,
        integration->participant_id,
        MESH_CHANNEL_SUBCORTICAL
    );

    if (!tx) {
        nimcp_mutex_unlock(integration->mutex);
        return NIMCP_ERROR_OUT_OF_MEMORY;
    }

    mesh_transaction_set_payload(tx, &new_anxiety, sizeof(new_anxiety));

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

mesh_amygdala_threat_level_t mesh_amygdala_get_threat_level(
    const mesh_amygdala_integration_t* integration
) {
    if (!integration || integration->magic != MESH_AMYGDALA_MAGIC) {
        return MESH_AMYGDALA_THREAT_NONE;
    }
    return integration->current_threat_level;
}

float mesh_amygdala_get_anxiety(const mesh_amygdala_integration_t* integration) {
    if (!integration || integration->magic != MESH_AMYGDALA_MAGIC) {
        return 0.0f;
    }
    return integration->current_anxiety;
}

float mesh_amygdala_get_fear(const mesh_amygdala_integration_t* integration) {
    if (!integration || integration->magic != MESH_AMYGDALA_MAGIC) {
        return 0.0f;
    }
    return integration->current_fear;
}

/* ============================================================================
 * Callback Registration API
 * ============================================================================ */

nimcp_error_t mesh_amygdala_set_threat_callback(
    mesh_amygdala_integration_t* integration,
    mesh_amygdala_threat_callback_t callback,
    void* ctx
) {
    if (!integration || integration->magic != MESH_AMYGDALA_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);
    integration->threat_callback = callback;
    integration->threat_callback_ctx = ctx;
    nimcp_mutex_unlock(integration->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_amygdala_set_veto_callback(
    mesh_amygdala_integration_t* integration,
    mesh_amygdala_veto_callback_t callback,
    void* ctx
) {
    if (!integration || integration->magic != MESH_AMYGDALA_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);
    integration->veto_callback = callback;
    integration->veto_callback_ctx = ctx;
    nimcp_mutex_unlock(integration->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics API
 * ============================================================================ */

nimcp_error_t mesh_amygdala_get_stats(
    const mesh_amygdala_integration_t* integration,
    mesh_amygdala_stats_t* stats
) {
    if (!integration || integration->magic != MESH_AMYGDALA_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!stats) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(((mesh_amygdala_integration_t*)integration)->mutex);
    *stats = integration->stats;
    nimcp_mutex_unlock(((mesh_amygdala_integration_t*)integration)->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_amygdala_reset_stats(mesh_amygdala_integration_t* integration) {
    if (!integration || integration->magic != MESH_AMYGDALA_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);

    /* Preserve current state */
    mesh_amygdala_threat_level_t threat = integration->stats.current_threat_level;
    float anxiety = integration->stats.current_anxiety;
    float fear = integration->stats.current_fear;

    memset(&integration->stats, 0, sizeof(integration->stats));

    /* Restore current state */
    integration->stats.current_threat_level = threat;
    integration->stats.current_anxiety = anxiety;
    integration->stats.current_fear = fear;

    nimcp_mutex_unlock(integration->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Health Agent Integration API
 * ============================================================================ */

nimcp_error_t mesh_amygdala_set_health_agent(
    mesh_amygdala_integration_t* integration,
    nimcp_health_agent_t* agent
) {
    if (!integration || integration->magic != MESH_AMYGDALA_MAGIC) {
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

const char* mesh_amygdala_threat_to_string(mesh_amygdala_threat_level_t level) {
    if (level >= sizeof(threat_level_names) / sizeof(threat_level_names[0])) {
        return "unknown";
    }
    return threat_level_names[level];
}

bool mesh_amygdala_is_amygdala_transaction(mesh_tx_type_t tx_type) {
    return tx_type >= MESH_TX_AMYGDALA_BASE &&
           tx_type <= MESH_TX_AMYGDALA_MEMORY_TAG;
}
