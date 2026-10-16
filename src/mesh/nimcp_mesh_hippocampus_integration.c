/**
 * @file nimcp_mesh_hippocampus_integration.c
 * @brief Hippocampus Mesh Network Integration Implementation
 *
 * WHAT: Connects the hippocampus module to the mesh network
 * WHY:  Enable coordinated memory operations via distributed consensus
 * HOW:  Register as MEMORY category participant, handle encoding/retrieval transactions
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 * @version 1.0.0
 */

#include "mesh/nimcp_mesh_hippocampus_integration.h"
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

#define LOG_MODULE "MESH_HIPPOCAMPUS"

/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define MESH_HIPPOCAMPUS_MAGIC 0x48495050  /* "HIPP" */

/* Participant type for hippocampus */
#define MESH_PARTICIPANT_HIPPOCAMPUS 0x0016

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct mesh_hippocampus_integration {
    uint32_t magic;

    /* Configuration */
    mesh_hippocampus_config_t config;

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

    /* Connected hippocampus module */
    hippocampus_adapter_t* hippocampus;

    /* Health agent */
    nimcp_health_agent_t* health_agent;

    /* Statistics */
    mesh_hippocampus_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Transaction sequence counter */
    uint64_t tx_sequence;
};

/* ============================================================================
 * Forward Declarations for Participant Callbacks
 * ============================================================================ */

static nimcp_error_t hippocampus_on_proposal(void* ctx, const mesh_transaction_t* tx);
static nimcp_error_t hippocampus_on_endorse_request(void* ctx, const mesh_transaction_t* tx,
                                                     mesh_endorsement_t* endorsement);
static nimcp_error_t hippocampus_on_commit(void* ctx, const mesh_transaction_t* tx);
static float hippocampus_get_free_energy(void* ctx);
static void hippocampus_get_health_metrics(void* ctx, health_metrics_t* metrics);

/* ============================================================================
 * Internal Helper: Send Heartbeat
 * ============================================================================ */

static void hippocampus_heartbeat(mesh_hippocampus_integration_t* integration,
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

static nimcp_error_t handle_encoding_commit(mesh_hippocampus_integration_t* integration,
                                            const mesh_transaction_t* tx) {
    (void)tx;  /* Payload handling would go here */

    integration->stats.encodings_committed++;
    integration->stats.last_encoding_ns = nimcp_time_now_ns();

    LOG_INFO("Memory encoding committed via mesh");

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_retrieval_commit(mesh_hippocampus_integration_t* integration,
                                             const mesh_transaction_t* tx) {
    (void)tx;

    integration->stats.retrievals_committed++;
    integration->stats.last_retrieval_ns = nimcp_time_now_ns();

    LOG_INFO("Memory retrieval committed via mesh");

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_consolidation_commit(mesh_hippocampus_integration_t* integration,
                                                 const mesh_transaction_t* tx) {
    (void)tx;

    integration->stats.consolidations_triggered++;
    integration->stats.last_consolidation_ns = nimcp_time_now_ns();

    LOG_INFO("Memory consolidation committed via mesh");

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_replay_commit(mesh_hippocampus_integration_t* integration,
                                          const mesh_transaction_t* tx) {
    (void)tx;

    integration->stats.replay_events++;

    LOG_DEBUG("Memory replay event committed via mesh");

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Participant Callback Implementations
 * ============================================================================ */

static nimcp_error_t hippocampus_on_proposal(void* ctx, const mesh_transaction_t* tx) {
    mesh_hippocampus_integration_t* integration = (mesh_hippocampus_integration_t*)ctx;

    if (!integration || integration->magic != MESH_HIPPOCAMPUS_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!tx) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    integration->stats.transactions_received++;

    /* Check if this is a hippocampus-related transaction */
    if (!mesh_hippocampus_is_hippocampus_transaction(tx->type)) {
        /* Not our transaction - no opinion */
        return NIMCP_SUCCESS;
    }

    hippocampus_heartbeat(integration, "proposal_received", 0.5f);

    /* Accept the proposal */
    return NIMCP_SUCCESS;
}

static nimcp_error_t hippocampus_on_endorse_request(void* ctx, const mesh_transaction_t* tx,
                                                     mesh_endorsement_t* endorsement) {
    mesh_hippocampus_integration_t* integration = (mesh_hippocampus_integration_t*)ctx;

    if (!integration || integration->magic != MESH_HIPPOCAMPUS_MAGIC) {
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

    /* Endorse hippocampus-related transactions after validation */
    if (mesh_hippocampus_is_hippocampus_transaction(tx->type)) {
        /* Validate based on type */
        bool valid = true;

        switch ((mesh_hippocampus_tx_type_t)tx->type) {
            case MESH_TX_HIPPOCAMPUS_ENCODE:
                /* Encoding requires payload */
                if (tx->payload_size == 0) {
                    valid = false;
                }
                break;

            case MESH_TX_HIPPOCAMPUS_RETRIEVE:
                /* Retrieval requires query cue */
                if (tx->payload_size == 0) {
                    valid = false;
                }
                break;

            case MESH_TX_HIPPOCAMPUS_CONSOLIDATE:
            case MESH_TX_HIPPOCAMPUS_REPLAY:
            case MESH_TX_HIPPOCAMPUS_SYSTEMS_CONSOLIDATE:
                /* These don't require specific payloads */
                break;

            default:
                /* Accept other hippocampus transactions */
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
        /* Not a hippocampus transaction - abstain */
        endorsement->result = ENDORSEMENT_ABSTAIN;
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t hippocampus_on_commit(void* ctx, const mesh_transaction_t* tx) {
    mesh_hippocampus_integration_t* integration = (mesh_hippocampus_integration_t*)ctx;

    if (!integration || integration->magic != MESH_HIPPOCAMPUS_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!tx) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Handle hippocampus-specific transactions */
    if (!mesh_hippocampus_is_hippocampus_transaction(tx->type)) {
        return NIMCP_SUCCESS;
    }

    nimcp_mutex_lock(integration->mutex);

    nimcp_error_t result = NIMCP_SUCCESS;

    switch ((mesh_hippocampus_tx_type_t)tx->type) {
        case MESH_TX_HIPPOCAMPUS_ENCODE:
            result = handle_encoding_commit(integration, tx);
            break;

        case MESH_TX_HIPPOCAMPUS_RETRIEVE:
            result = handle_retrieval_commit(integration, tx);
            break;

        case MESH_TX_HIPPOCAMPUS_CONSOLIDATE:
        case MESH_TX_HIPPOCAMPUS_SYSTEMS_CONSOLIDATE:
            result = handle_consolidation_commit(integration, tx);
            break;

        case MESH_TX_HIPPOCAMPUS_REPLAY:
            result = handle_replay_commit(integration, tx);
            break;

        case MESH_TX_HIPPOCAMPUS_PLACE_UPDATE:
        case MESH_TX_HIPPOCAMPUS_GRID_UPDATE:
        case MESH_TX_HIPPOCAMPUS_PATTERN_COMPLETE:
        case MESH_TX_HIPPOCAMPUS_PATTERN_SEPARATE:
            /* Track spatial/pattern operations */
            break;

        default:
            break;
    }

    nimcp_mutex_unlock(integration->mutex);

    hippocampus_heartbeat(integration, "commit_processed", 1.0f);

    return result;
}

static float hippocampus_get_free_energy(void* ctx) {
    mesh_hippocampus_integration_t* integration = (mesh_hippocampus_integration_t*)ctx;

    if (!integration || integration->magic != MESH_HIPPOCAMPUS_MAGIC) {
        return -1.0f;
    }

    /* Free energy based on memory operations success rate */
    uint64_t total_encodings = integration->stats.encodings_proposed;
    uint64_t successful_encodings = integration->stats.encodings_committed;

    uint64_t total_retrievals = integration->stats.retrievals_proposed;
    uint64_t successful_retrievals = integration->stats.retrievals_committed;

    float encoding_energy = 0.3f;  /* Default */
    float retrieval_energy = 0.3f;

    if (total_encodings > 0) {
        float success_rate = (float)successful_encodings / (float)total_encodings;
        encoding_energy = 1.0f - success_rate;  /* Higher success = lower energy */
    }

    if (total_retrievals > 0) {
        float success_rate = (float)successful_retrievals / (float)total_retrievals;
        retrieval_energy = 1.0f - success_rate;
    }

    return (encoding_energy + retrieval_energy) / 2.0f;
}

static void hippocampus_get_health_metrics(void* ctx, health_metrics_t* metrics) {
    mesh_hippocampus_integration_t* integration = (mesh_hippocampus_integration_t*)ctx;

    if (!integration || integration->magic != MESH_HIPPOCAMPUS_MAGIC || !metrics) {
        return;
    }

    memset(metrics, 0, sizeof(*metrics));
    metrics->participant = integration->participant_id;
    metrics->last_heartbeat_ns = nimcp_time_now_ns();

    /* Health based on memory operation success */
    uint64_t total_ops = integration->stats.encodings_proposed +
                         integration->stats.retrievals_proposed;
    uint64_t failed_ops = integration->stats.encodings_rejected +
                          integration->stats.retrievals_rejected;

    metrics->is_healthy = (total_ops == 0) || (failed_ops < total_ops / 2);

    /* Compute average latency (placeholder) */
    metrics->avg_latency_ms = 2.0f;

    /* Transaction stats */
    metrics->transactions_processed = integration->stats.transactions_received;
    metrics->transactions_failed = failed_ops;
}

/* ============================================================================
 * Configuration Defaults
 * ============================================================================ */

nimcp_error_t mesh_hippocampus_default_config(mesh_hippocampus_config_t* config) {
    if (!config) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    memset(config, 0, sizeof(*config));

    config->require_consensus_for_encoding = true;
    config->require_consensus_for_retrieval = false;  /* Retrieval is usually fast */
    config->enable_distributed_consolidation = true;

    config->encoding_timeout_ms = 5000;      /* 5 seconds for encoding */
    config->retrieval_timeout_ms = 1000;     /* 1 second for retrieval */
    config->consolidation_timeout_ms = 30000; /* 30 seconds for consolidation */

    config->enable_health_monitoring = true;
    config->heartbeat_interval_ms = 1000;

    config->verbose_logging = false;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

mesh_hippocampus_integration_t* mesh_hippocampus_create(
    mesh_bootstrap_t* bootstrap,
    hippocampus_adapter_t* hippocampus,
    const mesh_hippocampus_config_t* config
) {
    if (!bootstrap) {
        LOG_ERROR("Cannot create hippocampus integration without bootstrap");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "mesh_hippocampus_create: bootstrap is NULL");
        return NULL;
    }

    mesh_hippocampus_config_t default_config;
    if (!config) {
        mesh_hippocampus_default_config(&default_config);
        config = &default_config;
    }

    mesh_hippocampus_integration_t* integration = nimcp_calloc(1, sizeof(*integration));
    if (!integration) {
        LOG_ERROR("Failed to allocate hippocampus integration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_MEMORY,
            "mesh_hippocampus_create: allocation failed");
        return NULL;
    }

    integration->magic = MESH_HIPPOCAMPUS_MAGIC;
    integration->config = *config;
    integration->bootstrap = bootstrap;
    integration->mesh_integration = mesh_bootstrap_get_integration(bootstrap);
    integration->registry = mesh_bootstrap_get_registry(bootstrap);
    integration->health_bridge = mesh_bootstrap_get_health_bridge(bootstrap);
    integration->exception_bridge = mesh_bootstrap_get_exception_bridge(bootstrap);
    integration->hippocampus = hippocampus;

    /* Create mutex */
    mutex_attr_t attr = {0};
    integration->mutex = nimcp_mutex_create(&attr);
    if (!integration->mutex) {
        LOG_ERROR("Failed to create hippocampus integration mutex");
        nimcp_free(integration);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_hippocampus_create: integration->mutex is NULL");
        return NULL;
    }

    LOG_INFO("Created hippocampus mesh integration");

    return integration;
}

void mesh_hippocampus_destroy(mesh_hippocampus_integration_t* integration) {
    if (!integration || integration->magic != MESH_HIPPOCAMPUS_MAGIC) {
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

    LOG_INFO("Destroyed hippocampus mesh integration");
}

/* ============================================================================
 * Participant Registration API
 * ============================================================================ */

nimcp_error_t mesh_hippocampus_register_participant(mesh_hippocampus_integration_t* integration) {
    if (!integration || integration->magic != MESH_HIPPOCAMPUS_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(integration->mutex);

    if (integration->registered) {
        nimcp_mutex_unlock(integration->mutex);
        LOG_WARN("Hippocampus already registered with mesh");
        return NIMCP_ERROR_ALREADY_EXISTS;
    }

    if (!integration->registry) {
        nimcp_mutex_unlock(integration->mutex);
        LOG_ERROR("No participant registry available");
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    hippocampus_heartbeat(integration, "registering", 0.0f);

    /* Initialize participant interface */
    mesh_participant_interface_init(&integration->participant_interface);

    strncpy(integration->participant_interface.module_name, "hippocampus",
            MESH_MAX_NAME_LEN - 1);
    integration->participant_interface.type = MESH_PARTICIPANT_MODULE;
    integration->participant_interface.home_channel = MESH_CHANNEL_SUBCORTICAL;

    /* Set callbacks */
    integration->participant_interface.on_proposal = hippocampus_on_proposal;
    integration->participant_interface.on_endorse_request = hippocampus_on_endorse_request;
    integration->participant_interface.on_commit = hippocampus_on_commit;
    integration->participant_interface.get_free_energy = hippocampus_get_free_energy;
    integration->participant_interface.get_health_metrics = hippocampus_get_health_metrics;
    integration->participant_interface.user_context = integration;

    /* Register with mesh */
    mesh_participant_config_t reg_config;
    mesh_participant_config_init(&reg_config);
    reg_config.module_name = "hippocampus";
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
        LOG_ERROR("Failed to register hippocampus with mesh: %d", err);
        NIMCP_THROW_TO_IMMUNE(err, "mesh_hippocampus_register_participant: registration failed");
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

    LOG_INFO("Hippocampus registered with mesh: participant_id=0x%016lX",
             (unsigned long)integration->participant_id);

    hippocampus_heartbeat(integration, "registered", 1.0f);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_hippocampus_unregister_participant(mesh_hippocampus_integration_t* integration) {
    if (!integration || integration->magic != MESH_HIPPOCAMPUS_MAGIC) {
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

    LOG_INFO("Hippocampus unregistered from mesh");

    return NIMCP_SUCCESS;
}

mesh_participant_id_t mesh_hippocampus_get_participant_id(
    const mesh_hippocampus_integration_t* integration
) {
    if (!integration || integration->magic != MESH_HIPPOCAMPUS_MAGIC) {
        return 0;
    }
    return integration->participant_id;
}

bool mesh_hippocampus_is_registered(const mesh_hippocampus_integration_t* integration) {
    if (!integration || integration->magic != MESH_HIPPOCAMPUS_MAGIC) {
        return false;
    }
    return integration->registered;
}

/* ============================================================================
 * Memory Operations API
 * ============================================================================ */

nimcp_error_t mesh_hippocampus_propose_encoding(
    mesh_hippocampus_integration_t* integration,
    const void* memory_data,
    size_t data_size,
    const char* context
) {
    if (!integration || integration->magic != MESH_HIPPOCAMPUS_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!memory_data || data_size == 0) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!integration->registered) {
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    nimcp_mutex_lock(integration->mutex);

    integration->stats.encodings_proposed++;

    /* Create transaction */
    mesh_transaction_t* tx = mesh_transaction_create(
        (mesh_tx_type_t)MESH_TX_HIPPOCAMPUS_ENCODE,
        integration->participant_id,
        MESH_CHANNEL_SUBCORTICAL
    );

    if (!tx) {
        nimcp_mutex_unlock(integration->mutex);
        integration->stats.encodings_rejected++;
        return NIMCP_ERROR_OUT_OF_MEMORY;
    }

    /* Set payload (limited to reasonable size) */
    size_t payload_size = data_size > 4096 ? 4096 : data_size;
    mesh_transaction_set_payload(tx, memory_data, payload_size);
    mesh_transaction_set_timeout(tx, integration->config.encoding_timeout_ms);

    if (context) {
        /* Could add context as metadata */
        (void)context;
    }

    /* Submit transaction */
    nimcp_error_t err = NIMCP_SUCCESS;
    if (integration->mesh_integration) {
        err = mesh_integration_submit_transaction(integration->mesh_integration, tx);
    }

    mesh_transaction_destroy(tx);

    nimcp_mutex_unlock(integration->mutex);

    if (err != NIMCP_SUCCESS) {
        integration->stats.encodings_rejected++;
        LOG_ERROR("Failed to propose encoding: %d", err);
    }

    hippocampus_heartbeat(integration, "encoding_proposed", 1.0f);

    return err;
}

nimcp_error_t mesh_hippocampus_propose_retrieval(
    mesh_hippocampus_integration_t* integration,
    const void* query_cue,
    size_t cue_size
) {
    if (!integration || integration->magic != MESH_HIPPOCAMPUS_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!query_cue || cue_size == 0) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!integration->registered) {
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    nimcp_mutex_lock(integration->mutex);

    integration->stats.retrievals_proposed++;

    /* Create transaction */
    mesh_transaction_t* tx = mesh_transaction_create(
        (mesh_tx_type_t)MESH_TX_HIPPOCAMPUS_RETRIEVE,
        integration->participant_id,
        MESH_CHANNEL_SUBCORTICAL
    );

    if (!tx) {
        nimcp_mutex_unlock(integration->mutex);
        integration->stats.retrievals_rejected++;
        return NIMCP_ERROR_OUT_OF_MEMORY;
    }

    /* Set payload */
    mesh_transaction_set_payload(tx, query_cue, cue_size);
    mesh_transaction_set_timeout(tx, integration->config.retrieval_timeout_ms);

    /* Submit transaction */
    nimcp_error_t err = NIMCP_SUCCESS;
    if (integration->mesh_integration) {
        err = mesh_integration_submit_transaction(integration->mesh_integration, tx);
    }

    mesh_transaction_destroy(tx);

    nimcp_mutex_unlock(integration->mutex);

    if (err != NIMCP_SUCCESS) {
        integration->stats.retrievals_rejected++;
        LOG_ERROR("Failed to propose retrieval: %d", err);
    }

    hippocampus_heartbeat(integration, "retrieval_proposed", 1.0f);

    return err;
}

nimcp_error_t mesh_hippocampus_trigger_consolidation(mesh_hippocampus_integration_t* integration) {
    if (!integration || integration->magic != MESH_HIPPOCAMPUS_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!integration->registered) {
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    nimcp_mutex_lock(integration->mutex);

    /* Create transaction */
    mesh_transaction_t* tx = mesh_transaction_create(
        (mesh_tx_type_t)MESH_TX_HIPPOCAMPUS_CONSOLIDATE,
        integration->participant_id,
        MESH_CHANNEL_SUBCORTICAL
    );

    if (!tx) {
        nimcp_mutex_unlock(integration->mutex);
        return NIMCP_ERROR_OUT_OF_MEMORY;
    }

    mesh_transaction_set_timeout(tx, integration->config.consolidation_timeout_ms);

    /* Submit transaction */
    nimcp_error_t err = NIMCP_SUCCESS;
    if (integration->mesh_integration) {
        err = mesh_integration_submit_transaction(integration->mesh_integration, tx);
    }

    mesh_transaction_destroy(tx);

    nimcp_mutex_unlock(integration->mutex);

    if (err == NIMCP_SUCCESS) {
        LOG_INFO("Memory consolidation triggered via mesh");
    } else {
        LOG_ERROR("Failed to trigger consolidation: %d", err);
    }

    hippocampus_heartbeat(integration, "consolidation_triggered", 1.0f);

    return err;
}

/* ============================================================================
 * Statistics API
 * ============================================================================ */

nimcp_error_t mesh_hippocampus_get_stats(
    const mesh_hippocampus_integration_t* integration,
    mesh_hippocampus_stats_t* stats
) {
    if (!integration || integration->magic != MESH_HIPPOCAMPUS_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!stats) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(((mesh_hippocampus_integration_t*)integration)->mutex);
    *stats = integration->stats;
    nimcp_mutex_unlock(((mesh_hippocampus_integration_t*)integration)->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_hippocampus_reset_stats(mesh_hippocampus_integration_t* integration) {
    if (!integration || integration->magic != MESH_HIPPOCAMPUS_MAGIC) {
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

nimcp_error_t mesh_hippocampus_set_health_agent(
    mesh_hippocampus_integration_t* integration,
    nimcp_health_agent_t* agent
) {
    if (!integration || integration->magic != MESH_HIPPOCAMPUS_MAGIC) {
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

bool mesh_hippocampus_is_hippocampus_transaction(mesh_tx_type_t tx_type) {
    return tx_type >= MESH_TX_HIPPOCAMPUS_BASE &&
           tx_type <= MESH_TX_HIPPOCAMPUS_SYSTEMS_CONSOLIDATE;
}
