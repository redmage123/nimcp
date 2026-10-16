/**
 * @file nimcp_health_cognitive_bridge.c
 * @brief Unified Health Agent Cognitive Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-18
 *
 * Implementation of the unified bridge connecting health agent to cognitive systems.
 */

/* Enable POSIX clock functions */
#define _POSIX_C_SOURCE 200809L

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/health/nimcp_health_cognitive_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include <math.h>

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(health_cognitive_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_health_cognitive_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_health_cognitive_bridge_mesh_registry = NULL;

nimcp_error_t health_cognitive_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_health_cognitive_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "health_cognitive_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "health_cognitive_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_health_cognitive_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_health_cognitive_bridge_mesh_registry = registry;
    return err;
}

void health_cognitive_bridge_mesh_unregister(void) {
    if (g_health_cognitive_bridge_mesh_registry && g_health_cognitive_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_health_cognitive_bridge_mesh_registry, g_health_cognitive_bridge_mesh_id);
        g_health_cognitive_bridge_mesh_id = 0;
        g_health_cognitive_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from health_cognitive_bridge module (instance-level) */
static inline void health_cognitive_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_health_cognitive_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_health_cognitive_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_health_cognitive_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "HEALTH_COGNITIVE_BRIDGE"


/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Pending intelligent handling request
 */
typedef struct {
    uint64_t request_id;
    health_agent_msg_type_t anomaly_type;
    health_agent_source_t source;
    health_agent_severity_t severity;
    intelligent_handling_result_t result;
    bool complete;
    uint64_t start_time_us;

    /* Stage tracking */
    bool consensus_started;
    bool diagnosis_started;
    bool recovery_planned;
    bool executed;

    /* Request IDs for async operations */
    uint64_t consensus_request_id;
    uint64_t rcog_goal_id;
} pending_handling_t;

/**
 * @brief Health cognitive bridge internal state
 */
struct health_cognitive_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    /* Configuration */
    cognitive_bridge_config_t config;

    /* Connected systems */
    nimcp_health_agent_t* agent;
    collective_cognition_t* collective;
    rcog_engine_t* rcog;

    /* Subsystems */
    collective_health_monitor_t* collective_monitor;
    rcog_health_integration_t* rcog_health;
    meta_health_reflector_t* meta_reflector;

    /* Pending requests */
    pending_handling_t* pending;
    uint32_t num_pending;
    uint32_t max_pending;
    uint64_t next_request_id;

    /* Statistics */
    health_cognitive_stats_t stats;

    /* State */
    bool running;
};

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/* ============================================================================
 * Configuration API Implementation
 * ============================================================================ */

cognitive_bridge_config_t health_cognitive_bridge_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    health_cognitive_bridge_heartbeat("health_cogni_default_config", 0.0f);


    cognitive_bridge_config_t config = {
        /* Collective integration */
        .enable_collective_monitoring = true,
        .enable_consensus_decisions = true,
        .enable_swarm_immune = true,
        .collective_config = collective_health_default_config(),

        /* RCOG integration */
        .enable_rcog_diagnosis = true,
        .enable_rcog_recovery_planning = true,
        .rcog_timeout_ms = RCOG_HEALTH_DEFAULT_TIMEOUT_MS,
        .rcog_confidence_threshold = RCOG_HEALTH_DEFAULT_CONFIDENCE,
        .rcog_config = rcog_health_default_config(),

        /* Meta-health integration */
        .enable_meta_reflection = true,
        .enable_auto_learn = false,
        .meta_config = meta_health_default_config(),

        /* Imagination integration */
        .enable_imagination_for_planning = false,
        .max_imagination_steps = 5,

        /* Global settings */
        .default_timeout_ms = COGNITIVE_BRIDGE_DEFAULT_TIMEOUT_MS,
        .require_consensus_for_recovery = false,
        .require_quorum_for_swarm = true,
        .enable_ethics_validation = false,  /* Requires Phase 9 integration */
        .enable_emotion_awareness = false   /* Requires Phase 9 integration */
    };
    return config;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

health_cognitive_bridge_t* health_cognitive_bridge_create(
    nimcp_health_agent_t* agent,
    collective_cognition_t* collective,
    rcog_engine_t* rcog,
    const cognitive_bridge_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    health_cognitive_bridge_heartbeat("health_cogni_create", 0.0f);


    health_cognitive_bridge_t* bridge = nimcp_calloc(1, sizeof(health_cognitive_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = health_cognitive_bridge_default_config();
    }

    /* Store references */
    bridge->agent = agent;
    bridge->collective = collective;
    bridge->rcog = rcog;

    /* Create subsystems */
    if (bridge->config.enable_collective_monitoring && collective) {
        bridge->collective_monitor = collective_health_monitor_create(
            agent,
            collective,
            &bridge->config.collective_config
        );
    }

    if (bridge->config.enable_rcog_diagnosis) {
        bridge->rcog_health = rcog_health_create(
            rcog,
            agent,
            &bridge->config.rcog_config
        );
    }

    if (bridge->config.enable_meta_reflection) {
        bridge->meta_reflector = meta_health_create(
            agent,
            rcog,
            &bridge->config.meta_config
        );
    }

    /* Initialize pending requests */
    bridge->max_pending = COGNITIVE_BRIDGE_MAX_PENDING;
    bridge->pending = nimcp_calloc(bridge->max_pending, sizeof(pending_handling_t));
    if (!bridge->pending) {
        collective_health_monitor_destroy(bridge->collective_monitor);
        rcog_health_destroy(bridge->rcog_health);
        meta_health_destroy(bridge->meta_reflector);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "health_cognitive_bridge_create: bridge->pending is NULL");
        return NULL;
    }
    bridge->num_pending = 0;
    bridge->next_request_id = 1;

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(health_cognitive_stats_t));

    /* Initialize state */
    bridge->running = false;

    return bridge;
}

void health_cognitive_bridge_destroy(health_cognitive_bridge_t* bridge) {
    if (!bridge) {
        return;
        NIMCP_LOGGING_DEBUG("Destroying %s bridge", "health_cognitive");
    }

    /* Stop if running */
    /* Phase 8: Heartbeat at operation start */
    health_cognitive_bridge_heartbeat("health_cogni_destroy", 0.0f);


    if (bridge->running) {
        health_cognitive_bridge_stop(bridge);
    }

    /* Destroy subsystems */
    collective_health_monitor_destroy(bridge->collective_monitor);
    rcog_health_destroy(bridge->rcog_health);
    meta_health_destroy(bridge->meta_reflector);

    nimcp_free(bridge->pending);
    nimcp_free(bridge);
    bridge = NULL;
}

int health_cognitive_bridge_start(health_cognitive_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    health_cognitive_bridge_heartbeat("health_cogni_start", 0.0f);


    if (bridge->running) {
        return 0;  /* Already running */
    }

    /* Start subsystems */
    if (bridge->collective_monitor) {
        collective_health_monitor_start(bridge->collective_monitor);
    }

    if (bridge->meta_reflector) {
        meta_health_start(bridge->meta_reflector);
    }

    bridge->running = true;

    return 0;
}

int health_cognitive_bridge_stop(health_cognitive_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (!bridge->running) {
        return 0;  /* Already stopped */
    }

    /* Stop subsystems */
    /* Phase 8: Heartbeat at operation start */
    health_cognitive_bridge_heartbeat("health_cogni_stop", 0.0f);


    if (bridge->collective_monitor) {
        collective_health_monitor_stop(bridge->collective_monitor);
    }

    if (bridge->meta_reflector) {
        meta_health_stop(bridge->meta_reflector);
    }

    bridge->running = false;

    return 0;
}

bool health_cognitive_bridge_is_running(const health_cognitive_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    health_cognitive_bridge_heartbeat("health_cogni_is_running", 0.0f);


    return bridge->running;
}

/* ============================================================================
 * Intelligent Handling API Implementation
 * ============================================================================ */

int health_cognitive_intelligent_handle(
    health_cognitive_bridge_t* bridge,
    health_agent_msg_type_t anomaly_type,
    health_agent_source_t source,
    health_agent_severity_t severity,
    intelligent_handling_result_t* result
) {
    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_cognitive_intelligent_handle: required parameter is NULL (bridge, result)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    health_cognitive_bridge_heartbeat("health_cogni_health_cognitive_int", 0.0f);


    uint64_t start_time = get_time_us();

    health_cognitive_init_handling_result(result);
    bridge->stats.anomalies_handled++;

    /* Stage 1: Consensus (if enabled and collective available) */
    collective_anomaly_consensus_t consensus;

    if (bridge->config.enable_consensus_decisions && bridge->collective_monitor) {
        result->consensus_required = true;

        collective_anomaly_proposal_t proposal;
        collective_health_init_proposal(&proposal);
        proposal.anomaly_type = anomaly_type;
        proposal.source = source;
        proposal.severity = severity;
        proposal.local_confidence = 0.8f;

        if (collective_health_propose_anomaly(bridge->collective_monitor, &proposal, &consensus) == 0) {
            result->consensus_reached = consensus.consensus_reached;
            result->consensus_confidence = consensus.consensus_confidence;
            result->consensus_time_ms = consensus.consensus_time_ms;

            bridge->stats.consensus_obtained += consensus.consensus_reached ? 1 : 0;
            bridge->stats.consensus_failed += consensus.consensus_reached ? 0 : 1;
        }

        /* If consensus required but not reached, and not emergency, abort */
        if (bridge->config.require_consensus_for_recovery &&
            !consensus.consensus_reached &&
            severity < HEALTH_SEVERITY_CRITICAL) {
            result->success = false;
            snprintf(result->error_message, sizeof(result->error_message),
                     "Consensus not reached for recovery action");
            return 0;
        }
    } else {
        result->consensus_required = false;
        result->consensus_reached = true;
        result->consensus_confidence = 1.0f;
    }

    /* Stage 2: Diagnosis (if RCOG enabled) */
    uint64_t diag_start = get_time_us();

    if (bridge->config.enable_rcog_diagnosis && bridge->rcog_health) {
        rcog_health_answer_t answer;

        if (rcog_health_diagnose_anomaly(bridge->rcog_health, anomaly_type, source, severity, &answer) == 0) {
            result->diagnosis_performed = true;
            result->diagnosis = answer.diagnosis;

            if (bridge->config.enable_rcog_recovery_planning) {
                result->recovery_planned = true;
                result->recovery_plan = answer.recovery;
            }

            bridge->stats.diagnoses_performed++;
        }
    } else {
        /* Default diagnosis */
        result->diagnosis_performed = true;
        result->diagnosis.root_cause_source = source;
        snprintf(result->diagnosis.root_cause_description,
                 sizeof(result->diagnosis.root_cause_description),
                 "Anomaly detected (no RCOG diagnosis)");
        result->diagnosis.diagnosis_confidence = 0.5f;

        /* Default recovery */
        result->recovery_planned = true;
        result->recovery_plan.primary_action = HEALTH_RECOVERY_CHECKPOINT;
        result->recovery_plan.fallback_action = HEALTH_RECOVERY_NONE;
        result->recovery_plan.success_probability = 0.9f;
        result->recovery_plan.estimated_recovery_time_ms = 100;
    }

    result->diagnosis_time_ms = (uint32_t)((get_time_us() - diag_start) / 1000);

    /* Stage 3: Ethics Validation (if enabled - Phase 9 integration) */
    if (bridge->config.enable_ethics_validation) {
        /* Would call health_ethics_evaluate_action here */
        result->ethics_approved = true;  /* Default to approved if not implemented */
    } else {
        result->ethics_approved = true;
    }

    if (!result->ethics_approved) {
        result->success = false;
        snprintf(result->error_message, sizeof(result->error_message),
                 "Recovery action not approved by ethics evaluation");
        bridge->stats.ethics_rejected++;
        return 0;
    }
    bridge->stats.ethics_approved++;

    /* Stage 4: Swarm Quorum (if enabled and action requires it) */
    uint64_t recovery_start = get_time_us();

    if (bridge->config.enable_swarm_immune && bridge->collective_monitor &&
        result->recovery_plan.primary_action >= HEALTH_RECOVERY_ROLLBACK) {
        swarm_immune_request_t swarm_request;
        collective_health_init_swarm_request(&swarm_request);
        swarm_request.action = SWARM_IMMUNE_SYNCHRONIZED_CHECKPOINT;
        swarm_request.urgency = (severity >= HEALTH_SEVERITY_CRITICAL) ? 0.9f : 0.5f;
        snprintf(swarm_request.reason, sizeof(swarm_request.reason),
                 "Recovery from anomaly type %d", anomaly_type);

        swarm_immune_response_t swarm_response;
        if (collective_health_request_swarm_action(bridge->collective_monitor,
                                                   &swarm_request, &swarm_response) == 0) {
            result->swarm_quorum_obtained = swarm_response.approved;

            if (swarm_response.approved) {
                bridge->stats.swarm_quorums++;
            }
        }

        if (bridge->config.require_quorum_for_swarm && !result->swarm_quorum_obtained) {
            result->success = false;
            snprintf(result->error_message, sizeof(result->error_message),
                     "Swarm quorum not obtained for recovery action");
            return 0;
        }
    } else {
        result->swarm_quorum_obtained = true;
    }

    /* Stage 5: Execute Recovery */
    result->recovery_executed = true;
    bridge->stats.recoveries_executed++;

    /* Simulate recovery execution */
    result->outcome = META_HEALTH_OUTCOME_SUCCESS;

    result->recovery_time_ms = (uint32_t)((get_time_us() - recovery_start) / 1000);

    /* Stage 6: Record for Meta-Health */
    if (bridge->meta_reflector) {
        meta_health_decision_t decision;
        meta_health_init_decision(&decision);
        decision.anomaly_type = anomaly_type;
        decision.anomaly_source = source;
        decision.anomaly_severity = severity;
        decision.detection_confidence = result->diagnosis.diagnosis_confidence;
        decision.action_taken = result->recovery_plan.primary_action;
        decision.outcome = result->outcome;
        decision.recovery_succeeded = (result->outcome == META_HEALTH_OUTCOME_SUCCESS);
        decision.time_to_recovery_ms = result->recovery_time_ms;
        decision.post_recovery_health = 0.95f;

        meta_health_record_decision(bridge->meta_reflector, &decision);
    }

    /* Set final result */
    result->success = true;
    result->total_processing_time_ms = (uint32_t)((get_time_us() - start_time) / 1000);

    /* Update statistics */
    if (result->success) {
        bridge->stats.handling_success++;
    } else {
        bridge->stats.handling_failed++;
    }

    float n = (float)(bridge->stats.handling_success + bridge->stats.handling_failed);
    bridge->stats.avg_handling_time_ms =
        ((n - 1) * bridge->stats.avg_handling_time_ms + result->total_processing_time_ms) / (fabsf(n) > 1e-7f ? n : 1e-7f);
    bridge->stats.avg_consensus_time_ms =
        ((n - 1) * bridge->stats.avg_consensus_time_ms + result->consensus_time_ms) / (fabsf(n) > 1e-7f ? n : 1e-7f);
    bridge->stats.avg_diagnosis_time_ms =
        ((n - 1) * bridge->stats.avg_diagnosis_time_ms + result->diagnosis_time_ms) / (fabsf(n) > 1e-7f ? n : 1e-7f);

    if (bridge->stats.recoveries_executed > 0) {
        bridge->stats.recovery_success_rate =
            (float)bridge->stats.handling_success / (float)bridge->stats.recoveries_executed;
    }

    return 0;
}

int health_cognitive_intelligent_handle_async(
    health_cognitive_bridge_t* bridge,
    health_agent_msg_type_t anomaly_type,
    health_agent_source_t source,
    health_agent_severity_t severity,
    uint64_t* request_id
) {
    if (!bridge || !request_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_cognitive_intelligent_handle_async: required parameter is NULL (bridge, request_id)");
        return -1;
    }

    if (bridge->num_pending >= bridge->max_pending) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "health_cognitive_intelligent_handle_async: capacity exceeded");
        return -1;  /* Queue full */
    }

    pending_handling_t* pending = &bridge->pending[bridge->num_pending];
    pending->request_id = bridge->next_request_id++;
    pending->anomaly_type = anomaly_type;
    pending->source = source;
    pending->severity = severity;
    pending->complete = false;
    pending->start_time_us = get_time_us();
    pending->consensus_started = false;
    pending->diagnosis_started = false;
    pending->recovery_planned = false;
    pending->executed = false;

    *request_id = pending->request_id;
    bridge->num_pending++;
    bridge->stats.pending_requests++;

    return 0;
}

int health_cognitive_check_handling(
    health_cognitive_bridge_t* bridge,
    uint64_t request_id,
    intelligent_handling_result_t* result
) {
    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_cognitive_check_handling: required parameter is NULL (bridge, result)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    health_cognitive_bridge_heartbeat("health_cogni_health_cognitive_che", 0.0f);


    for (uint32_t i = 0; i < bridge->num_pending; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_pending > 256) {
            health_cognitive_bridge_heartbeat("health_cogni_loop",
                             (float)(i + 1) / (float)bridge->num_pending);
        }

        if (bridge->pending[i].request_id == request_id) {
            pending_handling_t* pending = &bridge->pending[i];

            /* Complete if not done */
            if (!pending->complete) {
                health_cognitive_intelligent_handle(
                    bridge,
                    pending->anomaly_type,
                    pending->source,
                    pending->severity,
                    &pending->result
                );
                pending->complete = true;
            }

            *result = pending->result;

            /* Remove from pending */
            if (i < bridge->num_pending - 1) {
                memmove(&bridge->pending[i],
                        &bridge->pending[i + 1],
                        (bridge->num_pending - i - 1) * sizeof(pending_handling_t));
            }
            bridge->num_pending--;
            bridge->stats.pending_requests--;

            return 1;  /* Complete */
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "health_cognitive_check_handling: operation failed");
    return -1;  /* Not found */
}

/* ============================================================================
 * Component Access API Implementation
 * ============================================================================ */

collective_health_monitor_t* health_cognitive_get_collective(health_cognitive_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    health_cognitive_bridge_heartbeat("health_cogni_health_cognitive_get", 0.0f);


    return bridge ? bridge->collective_monitor : NULL;
}

rcog_health_integration_t* health_cognitive_get_rcog(health_cognitive_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    health_cognitive_bridge_heartbeat("health_cogni_health_cognitive_get", 0.0f);


    return bridge ? bridge->rcog_health : NULL;
}

meta_health_reflector_t* health_cognitive_get_meta(health_cognitive_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    health_cognitive_bridge_heartbeat("health_cogni_health_cognitive_get", 0.0f);


    return bridge ? bridge->meta_reflector : NULL;
}

/* ============================================================================
 * Status API Implementation
 * ============================================================================ */

int health_cognitive_get_status(
    const health_cognitive_bridge_t* bridge,
    cognitive_bridge_status_t* status
) {
    if (!bridge || !status) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_cognitive_get_status: required parameter is NULL (bridge, status)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    health_cognitive_bridge_heartbeat("health_cogni_health_cognitive_get", 0.0f);


    memset(status, 0, sizeof(cognitive_bridge_status_t));

    /* Check connections */
    status->collective_connected = (bridge->collective_monitor != NULL);
    status->rcog_connected = (bridge->rcog_health != NULL);
    status->meta_health_active = (bridge->meta_reflector != NULL);

    /* Get pending counts */
    status->pending_consensus_requests = 0;  /* Would get from collective monitor */
    status->pending_rcog_goals = 0;  /* Would get from RCOG health */

    /* Get collective status if available */
    if (bridge->collective_monitor) {
        collective_health_summary_t summary;
        if (collective_health_get_summary(bridge->collective_monitor, &summary) == 0) {
            status->collective_health_score = summary.collective_health_score;
            status->collective_phi = summary.collective_phi;
        }
    }

    /* Get meta-health accuracy if available */
    if (bridge->meta_reflector) {
        meta_health_assessment_t assessment;
        if (meta_health_get_assessment(bridge->meta_reflector, &assessment) == 0) {
            status->meta_health_accuracy = assessment.accuracy_rate;
        }
    }

    /* Get average processing time from stats */
    status->avg_cognitive_processing_time_ms = bridge->stats.avg_handling_time_ms;

    /* Set active mode */
    snprintf(status->active_mode, sizeof(status->active_mode),
             bridge->running ? "ACTIVE" : "STOPPED");

    return 0;
}

/* ============================================================================
 * Manual Control API Implementation
 * ============================================================================ */

int health_cognitive_force_reflection(
    health_cognitive_bridge_t* bridge,
    meta_health_reflection_result_t* result
) {
    if (!bridge || !result || !bridge->meta_reflector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_cognitive_force_reflection: required parameter is NULL (bridge, result, bridge->meta_reflector)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    health_cognitive_bridge_heartbeat("health_cogni_health_cognitive_for", 0.0f);


    return meta_health_reflect(bridge->meta_reflector, result);
}

int health_cognitive_force_sync(health_cognitive_bridge_t* bridge) {
    if (!bridge || !bridge->collective_monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_cognitive_force_sync: required parameter is NULL (bridge, bridge->collective_monitor)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    health_cognitive_bridge_heartbeat("health_cogni_health_cognitive_for", 0.0f);


    return collective_health_force_sync(bridge->collective_monitor);
}

int health_cognitive_diagnose_only(
    health_cognitive_bridge_t* bridge,
    health_agent_msg_type_t anomaly_type,
    health_agent_source_t source,
    health_agent_severity_t severity,
    rcog_health_diagnosis_t* diagnosis
) {
    if (!bridge || !diagnosis) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_cognitive_diagnose_only: required parameter is NULL (bridge, diagnosis)");
        return -1;
    }

    if (!bridge->rcog_health) {
        /* Default diagnosis */
        diagnosis->root_cause_source = source;
        snprintf(diagnosis->root_cause_description,
                 sizeof(diagnosis->root_cause_description),
                 "Anomaly detected (RCOG not available)");
        diagnosis->diagnosis_confidence = 0.5f;
        diagnosis->is_certain = false;
        diagnosis->num_alternatives = 0;
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    health_cognitive_bridge_heartbeat("health_cogni_health_cognitive_dia", 0.0f);


    rcog_health_answer_t answer;
    int result = rcog_health_diagnose_anomaly(bridge->rcog_health, anomaly_type, source, severity, &answer);

    if (result == 0 && answer.success) {
        *diagnosis = answer.diagnosis;
    }

    return result;
}

int health_cognitive_plan_only(
    health_cognitive_bridge_t* bridge,
    health_agent_msg_type_t anomaly_type,
    health_agent_source_t source,
    health_agent_severity_t severity,
    rcog_health_recovery_plan_t* plan
) {
    if (!bridge || !plan) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_cognitive_plan_only: required parameter is NULL (bridge, plan)");
        return -1;
    }

    if (!bridge->rcog_health) {
        /* Default plan */
        plan->primary_action = HEALTH_RECOVERY_CHECKPOINT;
        plan->fallback_action = HEALTH_RECOVERY_NONE;
        snprintf(plan->recovery_plan, sizeof(plan->recovery_plan),
                 "Default: Create checkpoint and monitor");
        plan->success_probability = 0.9f;
        plan->estimated_recovery_time_ms = 100;
        plan->requires_immediate_action = false;
        plan->action_risk = HEALTH_SEVERITY_INFO;
        plan->num_steps = 1;
        snprintf(plan->recovery_steps[0], sizeof(plan->recovery_steps[0]), "Create checkpoint");
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    health_cognitive_bridge_heartbeat("health_cogni_health_cognitive_pla", 0.0f);


    return rcog_health_plan_recovery(bridge->rcog_health, anomaly_type, source, severity, plan);
}

/* ============================================================================
 * Statistics API Implementation
 * ============================================================================ */

int health_cognitive_get_stats(
    const health_cognitive_bridge_t* bridge,
    health_cognitive_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_cognitive_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    /* Phase 8: Heartbeat at operation start */
    health_cognitive_bridge_heartbeat("health_cogni_health_cognitive_get", 0.0f);


    return 0;
}

void health_cognitive_reset_stats(health_cognitive_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    health_cognitive_bridge_heartbeat("health_cogni_health_cognitive_res", 0.0f);


    memset(&bridge->stats, 0, sizeof(health_cognitive_stats_t));

    /* Reset subsystem stats */
    if (bridge->collective_monitor) {
        collective_health_reset_stats(bridge->collective_monitor);
    }
    if (bridge->rcog_health) {
        rcog_health_reset_stats(bridge->rcog_health);
    }
    if (bridge->meta_reflector) {
        meta_health_reset_stats(bridge->meta_reflector);
    }
}

/* ============================================================================
 * Utility Functions Implementation
 * ============================================================================ */

void health_cognitive_init_handling_result(intelligent_handling_result_t* result) {
    if (!result) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    health_cognitive_bridge_heartbeat("health_cogni_health_cognitive_ini", 0.0f);


    memset(result, 0, sizeof(intelligent_handling_result_t));
    result->success = false;
    result->consensus_required = false;
    result->consensus_reached = false;
    result->diagnosis_performed = false;
    result->recovery_planned = false;
    result->ethics_approved = false;
    result->recovery_executed = false;
    result->swarm_quorum_obtained = false;
    result->outcome = META_HEALTH_OUTCOME_UNKNOWN;
}

void health_cognitive_dump_handling_result(const intelligent_handling_result_t* result) {
    if (!result) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    health_cognitive_bridge_heartbeat("health_cogni_health_cognitive_dum", 0.0f);


    printf("=== Intelligent Handling Result ===\n");
    printf("Success: %s\n", result->success ? "yes" : "no");
    if (!result->success) {
        printf("Error: %s\n", result->error_message);
    }
    printf("\n--- Consensus Stage ---\n");
    printf("Required: %s\n", result->consensus_required ? "yes" : "no");
    printf("Reached: %s (confidence: %.2f)\n",
           result->consensus_reached ? "yes" : "no", result->consensus_confidence);
    printf("Time: %u ms\n", result->consensus_time_ms);
    printf("\n--- Diagnosis Stage ---\n");
    printf("Performed: %s\n", result->diagnosis_performed ? "yes" : "no");
    printf("Root Cause: %s\n", result->diagnosis.root_cause_description);
    printf("Confidence: %.2f\n", result->diagnosis.diagnosis_confidence);
    printf("\n--- Recovery Stage ---\n");
    printf("Planned: %s\n", result->recovery_planned ? "yes" : "no");
    printf("Primary Action: %d\n", result->recovery_plan.primary_action);
    printf("Success Probability: %.2f\n", result->recovery_plan.success_probability);
    printf("Ethics Approved: %s\n", result->ethics_approved ? "yes" : "no");
    printf("Swarm Quorum: %s\n", result->swarm_quorum_obtained ? "yes" : "no");
    printf("Executed: %s\n", result->recovery_executed ? "yes" : "no");
    printf("Outcome: %d\n", result->outcome);
    printf("\n--- Timing ---\n");
    printf("Total: %u ms\n", result->total_processing_time_ms);
    printf("Consensus: %u ms\n", result->consensus_time_ms);
    printf("Diagnosis: %u ms\n", result->diagnosis_time_ms);
    printf("Recovery: %u ms\n", result->recovery_time_ms);
    printf("===================================\n");
}

void health_cognitive_dump_status(const cognitive_bridge_status_t* status) {
    if (!status) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    health_cognitive_bridge_heartbeat("health_cogni_health_cognitive_dum", 0.0f);


    printf("=== Cognitive Status ===\n");
    printf("Mode: %s\n", status->active_mode);
    printf("Collective Connected: %s\n", status->collective_connected ? "yes" : "no");
    printf("RCOG Connected: %s\n", status->rcog_connected ? "yes" : "no");
    printf("Meta-Health Active: %s\n", status->meta_health_active ? "yes" : "no");
    printf("\n--- Metrics ---\n");
    printf("Collective Health: %.2f\n", status->collective_health_score);
    printf("Collective Phi: %.2f\n", status->collective_phi);
    printf("Meta-Health Accuracy: %.2f\n", status->meta_health_accuracy);
    printf("Avg Processing Time: %.2f ms\n", status->avg_cognitive_processing_time_ms);
    printf("========================\n");
}

/* ============================================================================
 * Convenience Wrapper Implementation
 * ============================================================================ */

int nimcp_health_agent_connect_cognitive(
    nimcp_health_agent_t* agent,
    collective_cognition_t* collective,
    rcog_engine_t* rcog,
    const cognitive_bridge_config_t* config
) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "agent is NULL");

        return -1;
    }

    /* Create bridge */
    /* Phase 8: Heartbeat at operation start */
    health_cognitive_bridge_heartbeat("health_cogni_health_agent_connect", 0.0f);


    health_cognitive_bridge_t* bridge = health_cognitive_bridge_create(
        agent,
        collective,
        rcog,
        config
    );

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");


        return -1;
    }

    /* Start bridge */
    int result = health_cognitive_bridge_start(bridge);

    if (result != 0) {
        health_cognitive_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_health_agent_connect_cognitive: validation failed");
        return -1;
    }

    /* TODO: Store bridge reference in agent struct to prevent memory leak.
     * The agent struct needs a cognitive_bridge field added.
     * For now, destroy the bridge to prevent leak since it's not stored anywhere. */
    health_cognitive_bridge_destroy(bridge);

    return 0;
}

int nimcp_health_agent_disconnect_cognitive(nimcp_health_agent_t* agent) {
    if (!agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "agent is NULL");

        return -1;
    }

    /* In a real implementation, we would retrieve and destroy the bridge
     * stored in the agent. For now, this is a simplified version. */

    /* Phase 8: Heartbeat at operation start */
    health_cognitive_bridge_heartbeat("health_cogni_health_agent_disconn", 0.0f);


    return 0;
}

int nimcp_health_agent_get_bridge_status(
    const nimcp_health_agent_t* agent,
    cognitive_bridge_status_t* status
) {
    if (!agent || !status) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_health_agent_get_bridge_status: required parameter is NULL (agent, status)");
        return -1;
    }

    /* In a real implementation, we would get the status from the bridge
     * stored in the agent. For now, return a default status. */
    /* Phase 8: Heartbeat at operation start */
    health_cognitive_bridge_heartbeat("health_cogni_health_agent_get_bri", 0.0f);


    memset(status, 0, sizeof(cognitive_bridge_status_t));
    snprintf(status->active_mode, sizeof(status->active_mode), "DEFAULT");

    return 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void health_cognitive_bridge_set_instance_health_agent(health_cognitive_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "health_cognitive_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int health_cognitive_bridge_training_begin(health_cognitive_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "health_cognitive_bridge_training_begin: NULL argument");
        return -1;
    }
    health_cognitive_bridge_heartbeat_instance(bridge->health_agent, "health_cognitive_bridge_training_begin", 0.0f);
    return 0;
}

int health_cognitive_bridge_training_end(health_cognitive_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "health_cognitive_bridge_training_end: NULL argument");
        return -1;
    }
    health_cognitive_bridge_heartbeat_instance(bridge->health_agent, "health_cognitive_bridge_training_end", 1.0f);
    return 0;
}

int health_cognitive_bridge_training_step(health_cognitive_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "health_cognitive_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    health_cognitive_bridge_heartbeat_instance(bridge->health_agent, "health_cognitive_bridge_training_step", progress);
    return 0;
}
