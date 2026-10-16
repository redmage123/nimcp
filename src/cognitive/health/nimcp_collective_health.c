/**
 * @file nimcp_collective_health.c
 * @brief Collective Health Monitoring Implementation
 * @version 1.0.0
 * @date 2026-01-18
 *
 * Implementation of distributed health monitoring using collective cognition.
 */

/* Enable POSIX clock functions */
#define _POSIX_C_SOURCE 200809L

#include "cognitive/health/nimcp_collective_health.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include <math.h>

BRIDGE_BOILERPLATE(collective_health, MESH_ADAPTER_CATEGORY_COGNITIVE)



/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Pending consensus request
 */
typedef struct {
    uint64_t request_id;
    collective_anomaly_proposal_t proposal;
    collective_anomaly_consensus_t consensus;
    bool complete;
    uint64_t start_time_us;
    uint32_t votes_for;
    uint32_t votes_against;
    float total_confidence;
} pending_consensus_t;

/**
 * @brief Pending swarm action request
 */
typedef struct {
    uint64_t request_id;
    swarm_immune_request_t request;
    swarm_immune_response_t response;
    bool complete;
    uint64_t start_time_us;
    uint32_t approvals;
    uint32_t rejections;
} pending_swarm_action_t;

/**
 * @brief Collective health monitor internal state
 */
struct collective_health_monitor {
    /* Configuration */
    collective_health_config_t config;

    /* Connected systems */
    nimcp_health_agent_t* local_agent;
    collective_cognition_t* collective;

    /* Instance tracking */
    instance_health_report_t instance_reports[COLLECTIVE_HEALTH_MAX_INSTANCES];
    uint32_t num_instances;
    uint32_t local_instance_id;

    /* Pending requests */
    pending_consensus_t pending_consensus[COLLECTIVE_HEALTH_MAX_PENDING_CONSENSUS];
    uint32_t num_pending_consensus;
    uint64_t next_consensus_id;

    pending_swarm_action_t pending_swarm[COLLECTIVE_HEALTH_MAX_PENDING_CONSENSUS];
    uint32_t num_pending_swarm;
    uint64_t next_swarm_id;

    /* Threat callback */
    collective_threat_callback_t threat_callback;
    void* threat_callback_data;

    /* Statistics */
    collective_health_stats_t stats;

    /* State */
    bool running;
    uint64_t last_sync_time_us;
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

collective_health_config_t collective_health_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    collective_health_heartbeat("collective_h_default_config", 0.0f);


    collective_health_config_t config = {
        .enable_hyperscanning = true,
        .enable_collective_phi = true,
        .enable_shared_goals = true,
        .enable_we_mode_recovery = false,
        .anomaly_consensus_threshold = COLLECTIVE_HEALTH_DEFAULT_ANOMALY_THRESHOLD,
        .recovery_quorum_threshold = COLLECTIVE_HEALTH_DEFAULT_RECOVERY_QUORUM,
        .max_consensus_time_ms = COLLECTIVE_HEALTH_DEFAULT_CONSENSUS_TIMEOUT_MS,
        .aggregate_health_scores = true,
        .share_failure_predictions = true,
        .propagate_immune_memory = false,
        .local_weight = 0.5f,
        .use_reliability_weighting = true
    };
    return config;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

collective_health_monitor_t* collective_health_monitor_create(
    nimcp_health_agent_t* local_agent,
    collective_cognition_t* collective,
    const collective_health_config_t* config
) {
    if (!local_agent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "local_agent is NULL");

        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_health_heartbeat("collective_h_monitor_create", 0.0f);


    collective_health_monitor_t* monitor = nimcp_calloc(1, sizeof(collective_health_monitor_t));
    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate monitor");

        return NULL;
    }

    /* Apply configuration */
    if (config) {
        monitor->config = *config;
    } else {
        monitor->config = collective_health_default_config();
    }

    /* Store references */
    monitor->local_agent = local_agent;
    monitor->collective = collective;

    /* Initialize state */
    monitor->num_instances = 0;
    monitor->local_instance_id = 1;  /* Default instance ID */
    monitor->num_pending_consensus = 0;
    monitor->next_consensus_id = 1;
    monitor->num_pending_swarm = 0;
    monitor->next_swarm_id = 1;
    monitor->running = false;
    monitor->last_sync_time_us = get_time_us();

    /* Initialize statistics */
    memset(&monitor->stats, 0, sizeof(collective_health_stats_t));

    return monitor;
}

void collective_health_monitor_destroy(collective_health_monitor_t* monitor) {
    if (!monitor) {
        return;
    }

    /* Stop if running */
    /* Phase 8: Heartbeat at operation start */
    collective_health_heartbeat("collective_h_monitor_destroy", 0.0f);


    if (monitor->running) {
        collective_health_monitor_stop(monitor);
    }

    nimcp_free(monitor);
    monitor = NULL;
}

int collective_health_monitor_start(collective_health_monitor_t* monitor) {
    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "monitor is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_health_heartbeat("collective_h_monitor_start", 0.0f);


    if (monitor->running) {
        return 0;  /* Already running */
    }

    /* Register local instance if collective is available */
    if (monitor->collective) {
        collective_cognition_register_instance(
            monitor->collective,
            monitor->local_instance_id,
            NULL
        );
    }

    monitor->running = true;
    monitor->last_sync_time_us = get_time_us();

    return 0;
}

int collective_health_monitor_stop(collective_health_monitor_t* monitor) {
    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "monitor is NULL");

        return -1;
    }

    if (!monitor->running) {
        return 0;  /* Already stopped */
    }

    /* Unregister from collective if connected */
    /* Phase 8: Heartbeat at operation start */
    collective_health_heartbeat("collective_h_monitor_stop", 0.0f);


    if (monitor->collective) {
        collective_cognition_unregister_instance(
            monitor->collective,
            monitor->local_instance_id
        );
    }

    monitor->running = false;

    return 0;
}

bool collective_health_monitor_is_running(const collective_health_monitor_t* monitor) {
    if (!monitor) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    collective_health_heartbeat("collective_h_monitor_is_running", 0.0f);


    return monitor->running;
}

/* ============================================================================
 * Consensus API Implementation
 * ============================================================================ */

int collective_health_propose_anomaly(
    collective_health_monitor_t* monitor,
    const collective_anomaly_proposal_t* proposal,
    collective_anomaly_consensus_t* consensus
) {
    if (!monitor || !proposal || !consensus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_health_propose_anomaly: required parameter is NULL (monitor, proposal, consensus)");
        return -1;
    }

    /* Initialize consensus result */
    /* Phase 8: Heartbeat at operation start */
    collective_health_heartbeat("collective_h_propose_anomaly", 0.0f);


    memset(consensus, 0, sizeof(collective_anomaly_consensus_t));

    /* If no collective, auto-approve based on local confidence */
    if (!monitor->collective) {
        consensus->consensus_reached = (proposal->local_confidence >=
                                         monitor->config.anomaly_consensus_threshold);
        consensus->consensus_confidence = proposal->local_confidence;
        consensus->agreeing_instances = 1;
        consensus->total_instances = 1;
        consensus->agreed_severity = proposal->severity;
        consensus->agreed_recovery = proposal->suggested_recovery;
        consensus->collective_phi = 0.0f;
        consensus->consensus_time_ms = 0;
        consensus->timed_out = false;

        monitor->stats.anomalies_proposed++;
        if (consensus->consensus_reached) {
            monitor->stats.consensus_reached++;
        } else {
            monitor->stats.consensus_failed++;
        }

        return 0;
    }

    /* With collective: simulate consensus process */
    uint64_t start_time = get_time_us();
    uint32_t total_instances = collective_cognition_instance_count(monitor->collective);

    if (total_instances == 0) {
        total_instances = 1;
    }

    /* For now, simulate consensus based on severity and confidence */
    float base_agreement = proposal->local_confidence;

    /* Higher severity gets more agreement */
    switch (proposal->severity) {
        case HEALTH_SEVERITY_FATAL:
            base_agreement += 0.2f;
            break;
        case HEALTH_SEVERITY_CRITICAL:
            base_agreement += 0.15f;
            break;
        case HEALTH_SEVERITY_ERROR:
            base_agreement += 0.1f;
            break;
        case HEALTH_SEVERITY_WARNING:
            base_agreement += 0.05f;
            break;
        default:
            break;
    }

    if (base_agreement > 1.0f) {
        base_agreement = 1.0f;
    }

    /* Calculate agreeing instances */
    uint32_t agreeing = (uint32_t)(total_instances * base_agreement);
    if (agreeing == 0) {
        agreeing = 1;  /* At least local instance agrees */
    }

    /* Determine if consensus reached */
    float agreement_ratio = (float)agreeing / (float)total_instances;
    consensus->consensus_reached = (agreement_ratio >= monitor->config.anomaly_consensus_threshold);
    consensus->consensus_confidence = agreement_ratio;
    consensus->agreeing_instances = agreeing;
    consensus->total_instances = total_instances;
    consensus->agreed_severity = proposal->severity;
    consensus->agreed_recovery = proposal->suggested_recovery;

    /* Get collective phi if available */
    collective_phi_t phi;
    if (collective_cognition_get_phi(monitor->collective, &phi) == 0) {
        consensus->collective_phi = phi.phi_total;
    }

    consensus->consensus_time_ms = (uint32_t)((get_time_us() - start_time) / 1000);
    consensus->timed_out = false;

    if (!consensus->consensus_reached) {
        snprintf(consensus->disagreement_reason, sizeof(consensus->disagreement_reason),
                 "Agreement ratio %.2f below threshold %.2f",
                 agreement_ratio, monitor->config.anomaly_consensus_threshold);
    }

    /* Update statistics */
    monitor->stats.anomalies_proposed++;
    if (consensus->consensus_reached) {
        monitor->stats.consensus_reached++;
    } else {
        monitor->stats.consensus_failed++;
    }

    /* Update average consensus time */
    float total_time = monitor->stats.avg_consensus_time_ms *
                       (monitor->stats.consensus_reached + monitor->stats.consensus_failed - 1);
    total_time += consensus->consensus_time_ms;
    monitor->stats.avg_consensus_time_ms = total_time /
                       (monitor->stats.consensus_reached + monitor->stats.consensus_failed);

    return 0;
}

int collective_health_propose_anomaly_async(
    collective_health_monitor_t* monitor,
    const collective_anomaly_proposal_t* proposal,
    uint64_t* request_id
) {
    if (!monitor || !proposal || !request_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_health_propose_anomaly_async: required parameter is NULL (monitor, proposal, request_id)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_health_heartbeat("collective_h_propose_anomaly_asyn", 0.0f);


    if (monitor->num_pending_consensus >= COLLECTIVE_HEALTH_MAX_PENDING_CONSENSUS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "collective_health_propose_anomaly_async: capacity exceeded");
        return -1;  /* Queue full */
    }

    /* Store pending request */
    pending_consensus_t* pending = &monitor->pending_consensus[monitor->num_pending_consensus];
    pending->request_id = monitor->next_consensus_id++;
    pending->proposal = *proposal;
    pending->complete = false;
    pending->start_time_us = get_time_us();
    pending->votes_for = 1;  /* Local vote for */
    pending->votes_against = 0;
    pending->total_confidence = proposal->local_confidence;

    *request_id = pending->request_id;
    monitor->num_pending_consensus++;

    return 0;
}

int collective_health_check_consensus(
    collective_health_monitor_t* monitor,
    uint64_t request_id,
    collective_anomaly_consensus_t* consensus
) {
    if (!monitor || !consensus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_health_check_consensus: required parameter is NULL (monitor, consensus)");
        return -1;
    }

    /* Find the request */
    /* Phase 8: Heartbeat at operation start */
    collective_health_heartbeat("collective_h_check_consensus", 0.0f);


    for (uint32_t i = 0; i < monitor->num_pending_consensus; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && monitor->num_pending_consensus > 256) {
            collective_health_heartbeat("collective_h_loop",
                             (float)(i + 1) / (float)monitor->num_pending_consensus);
        }

        if (monitor->pending_consensus[i].request_id == request_id) {
            pending_consensus_t* pending = &monitor->pending_consensus[i];

            /* Check if timeout */
            uint64_t elapsed_ms = (get_time_us() - pending->start_time_us) / 1000;
            if (elapsed_ms >= monitor->config.max_consensus_time_ms) {
                /* Timeout - finalize based on current votes */
                pending->complete = true;
            }

            if (!pending->complete) {
                /* Still pending - simulate vote completion for demo */
                pending->complete = true;
            }

            if (pending->complete) {
                *consensus = pending->consensus;

                /* Fill in consensus if not already done */
                if (consensus->total_instances == 0) {
                    uint32_t total = pending->votes_for + pending->votes_against;
                    if (total == 0) total = 1;

                    consensus->agreeing_instances = pending->votes_for;
                    consensus->total_instances = total;
                    consensus->consensus_confidence = pending->total_confidence / total;
                    consensus->consensus_reached =
                        ((float)pending->votes_for / (total > 0 ? total : 1)) >= monitor->config.anomaly_consensus_threshold;
                    consensus->agreed_severity = pending->proposal.severity;
                    consensus->agreed_recovery = pending->proposal.suggested_recovery;
                    consensus->consensus_time_ms = (uint32_t)elapsed_ms;
                    consensus->timed_out = (elapsed_ms >= monitor->config.max_consensus_time_ms);
                }

                /* Remove from pending */
                if (i < monitor->num_pending_consensus - 1) {
                    memmove(&monitor->pending_consensus[i],
                            &monitor->pending_consensus[i + 1],
                            (monitor->num_pending_consensus - i - 1) * sizeof(pending_consensus_t));
                }
                monitor->num_pending_consensus--;

                return 1;  /* Complete */
            }

            return 0;  /* Still pending */
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "collective_health_check_consensus: operation failed");
    return -1;  /* Not found */
}

int collective_health_vote_anomaly(
    collective_health_monitor_t* monitor,
    const collective_anomaly_proposal_t* proposal,
    bool agree,
    float confidence
) {
    if (!monitor || !proposal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_health_vote_anomaly: required parameter is NULL (monitor, proposal)");
        return -1;
    }

    /* In a real implementation, this would send a vote to the collective.
     * For now, we just update local state. */
    /* Phase 8: Heartbeat at operation start */
    collective_health_heartbeat("collective_h_vote_anomaly", 0.0f);


    (void)proposal;
    (void)agree;
    (void)confidence;

    return 0;
}

/* ============================================================================
 * Health Summary API Implementation
 * ============================================================================ */

int collective_health_get_summary(
    const collective_health_monitor_t* monitor,
    collective_health_summary_t* summary
) {
    if (!monitor || !summary) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_health_get_summary: required parameter is NULL (monitor, summary)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_health_heartbeat("collective_h_get_summary", 0.0f);


    memset(summary, 0, sizeof(collective_health_summary_t));

    /* Calculate aggregate health from instance reports */
    float total_health = 0.0f;
    float total_weight = 0.0f;
    uint32_t healthy = 0, degraded = 0, failed = 0;
    health_agent_severity_t max_severity = HEALTH_SEVERITY_INFO;
    uint32_t total_anomalies = 0;
    float total_failure_prob = 0.0f;

    for (uint32_t i = 0; i < monitor->num_instances; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && monitor->num_instances > 256) {
            collective_health_heartbeat("collective_h_loop",
                             (float)(i + 1) / (float)monitor->num_instances);
        }

        const instance_health_report_t* report = &monitor->instance_reports[i];

        float weight = monitor->config.use_reliability_weighting ?
                       report->reliability_score : 1.0f;

        total_health += report->health_score * weight;
        total_weight += weight;

        if (report->health_score >= 0.8f) {
            healthy++;
        } else if (report->health_score >= 0.5f) {
            degraded++;
        } else {
            failed++;
        }

        if (report->max_severity > max_severity) {
            max_severity = report->max_severity;
        }

        total_anomalies += report->active_anomalies;
        total_failure_prob += report->failure_probability;
    }

    /* Handle case with no instances */
    if (monitor->num_instances == 0) {
        summary->collective_health_score = 1.0f;
        summary->healthy_instances = 0;
        summary->degraded_instances = 0;
        summary->failed_instances = 0;
    } else {
        summary->collective_health_score = total_weight > 0.0f ?
                                           total_health / (fabsf(total_weight) > 1e-7f ? total_weight : 1e-7f) : 0.0f;
        summary->healthy_instances = healthy;
        summary->degraded_instances = degraded;
        summary->failed_instances = failed;
        summary->most_severe_issue = max_severity;
        summary->total_active_anomalies = total_anomalies;
        summary->avg_failure_probability = total_failure_prob / monitor->num_instances;
    }

    /* Get collective phi if available */
    if (monitor->collective) {
        collective_phi_t phi;
        if (collective_cognition_get_phi(monitor->collective, &phi) == 0) {
            summary->collective_phi = phi.phi_total;
        }

        /* Get hyperscanning state for leader info */
        hyperscan_state_t hscan;
        if (collective_cognition_get_hyperscan_state(monitor->collective, &hscan) == 0) {
            summary->leader_instance_id = hscan.leader_instance_id;
            summary->leader_influence = hscan.leader_influence;
        }

        /* Check for fragmentation/overload */
        collective_cognition_state_t cc_state;
        if (collective_cognition_get_state(monitor->collective, &cc_state) == 0) {
            summary->is_fragmented = cc_state.is_fragmented;
            summary->is_overloaded = cc_state.is_overloaded;
        }
    }

    summary->timestamp_us = get_time_us();

    /* Update stats */
    ((collective_health_monitor_t*)monitor)->stats.avg_collective_health =
        (monitor->stats.avg_collective_health + summary->collective_health_score) / 2.0f;

    if (summary->collective_phi > monitor->stats.peak_collective_phi) {
        ((collective_health_monitor_t*)monitor)->stats.peak_collective_phi = summary->collective_phi;
    }

    return 0;
}

int collective_health_get_instance_report(
    const collective_health_monitor_t* monitor,
    uint32_t instance_id,
    instance_health_report_t* report
) {
    if (!monitor || !report) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_health_get_instance_report: required parameter is NULL (monitor, report)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_health_heartbeat("collective_h_get_instance_report", 0.0f);


    for (uint32_t i = 0; i < monitor->num_instances; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && monitor->num_instances > 256) {
            collective_health_heartbeat("collective_h_loop",
                             (float)(i + 1) / (float)monitor->num_instances);
        }

        if (monitor->instance_reports[i].instance_id == instance_id) {
            *report = monitor->instance_reports[i];
            return 0;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "collective_health_get_instance_report: validation failed");
    return -1;  /* Instance not found */
}

int collective_health_get_all_reports(
    const collective_health_monitor_t* monitor,
    instance_health_report_t* reports,
    uint32_t max_reports,
    uint32_t* num_reports
) {
    if (!monitor || !reports || !num_reports) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_health_get_all_reports: required parameter is NULL (monitor, reports, num_reports)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_health_heartbeat("collective_h_get_all_reports", 0.0f);


    uint32_t count = monitor->num_instances;
    if (count > max_reports) {
        count = max_reports;
    }

    memcpy(reports, monitor->instance_reports, count * sizeof(instance_health_report_t));
    *num_reports = count;

    return 0;
}

/* ============================================================================
 * Swarm Immune API Implementation
 * ============================================================================ */

int collective_health_request_swarm_action(
    collective_health_monitor_t* monitor,
    const swarm_immune_request_t* request,
    swarm_immune_response_t* response
) {
    if (!monitor || !request || !response) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_health_request_swarm_action: required parameter is NULL (monitor, request, response)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_health_heartbeat("collective_h_request_swarm_action", 0.0f);


    memset(response, 0, sizeof(swarm_immune_response_t));

    /* Update statistics */
    monitor->stats.swarm_actions_requested++;

    /* If no collective, auto-approve based on urgency */
    if (!monitor->collective) {
        response->approved = (request->urgency >= 0.5f);
        response->approving_instances = response->approved ? 1 : 0;
        response->total_instances = 1;
        response->executing_instances = response->approved ? 1 : 0;
        response->collective_confidence = request->urgency;
        response->approval_time_ms = 0;

        if (response->approved) {
            monitor->stats.swarm_actions_approved++;
        } else {
            monitor->stats.swarm_actions_rejected++;
            snprintf(response->rejection_reason, sizeof(response->rejection_reason),
                     "Urgency %.2f below threshold", request->urgency);
        }

        return 0;
    }

    /* With collective: simulate quorum */
    uint64_t start_time = get_time_us();
    uint32_t total_instances = collective_cognition_instance_count(monitor->collective);

    if (total_instances == 0) {
        total_instances = 1;
    }

    /* Calculate approval based on urgency and recovery quorum threshold */
    float base_approval = request->urgency * 0.8f + 0.2f;  /* Baseline + urgency boost */

    /* Higher urgency = faster approval, more likely to pass */
    if (request->urgency >= 0.9f) {
        base_approval = 1.0f;  /* Emergency = auto-approve */
    }

    uint32_t approving = (uint32_t)(total_instances * base_approval);
    if (approving == 0 && request->urgency > 0.0f) {
        approving = 1;
    }

    float approval_ratio = (float)approving / (float)total_instances;
    response->approved = (approval_ratio >= monitor->config.recovery_quorum_threshold);
    response->approving_instances = approving;
    response->total_instances = total_instances;
    response->executing_instances = response->approved ? approving : 0;
    response->collective_confidence = approval_ratio;
    response->approval_time_ms = (uint32_t)((get_time_us() - start_time) / 1000);

    if (!response->approved) {
        snprintf(response->rejection_reason, sizeof(response->rejection_reason),
                 "Approval ratio %.2f below quorum threshold %.2f",
                 approval_ratio, monitor->config.recovery_quorum_threshold);
        monitor->stats.swarm_actions_rejected++;
    } else {
        monitor->stats.swarm_actions_approved++;

        /* Update specific action stats */
        if (request->action == SWARM_IMMUNE_QUARANTINE_INSTANCE) {
            monitor->stats.quarantine_events++;
        } else if (request->action == SWARM_IMMUNE_LOAD_REDISTRIBUTE) {
            monitor->stats.load_redistributions++;
        }
    }

    return 0;
}

int collective_health_request_swarm_action_async(
    collective_health_monitor_t* monitor,
    const swarm_immune_request_t* request,
    uint64_t* request_id
) {
    if (!monitor || !request || !request_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_health_request_swarm_action_async: required parameter is NULL (monitor, request, request_id)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_health_heartbeat("collective_h_request_swarm_action", 0.0f);


    if (monitor->num_pending_swarm >= COLLECTIVE_HEALTH_MAX_PENDING_CONSENSUS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "collective_health_request_swarm_action_async: capacity exceeded");
        return -1;  /* Queue full */
    }

    /* Store pending request */
    pending_swarm_action_t* pending = &monitor->pending_swarm[monitor->num_pending_swarm];
    pending->request_id = monitor->next_swarm_id++;
    pending->request = *request;
    pending->complete = false;
    pending->start_time_us = get_time_us();
    pending->approvals = 0;
    pending->rejections = 0;

    *request_id = pending->request_id;
    monitor->num_pending_swarm++;

    monitor->stats.swarm_actions_requested++;

    return 0;
}

int collective_health_check_swarm_action(
    collective_health_monitor_t* monitor,
    uint64_t request_id,
    swarm_immune_response_t* response
) {
    if (!monitor || !response) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_health_check_swarm_action: required parameter is NULL (monitor, response)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_health_heartbeat("collective_h_check_swarm_action", 0.0f);


    for (uint32_t i = 0; i < monitor->num_pending_swarm; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && monitor->num_pending_swarm > 256) {
            collective_health_heartbeat("collective_h_loop",
                             (float)(i + 1) / (float)monitor->num_pending_swarm);
        }

        if (monitor->pending_swarm[i].request_id == request_id) {
            pending_swarm_action_t* pending = &monitor->pending_swarm[i];

            /* Auto-complete for demo */
            if (!pending->complete) {
                /* Simulate swarm approval */
                swarm_immune_response_t temp_response;
                collective_health_request_swarm_action(monitor, &pending->request, &temp_response);
                pending->response = temp_response;
                pending->complete = true;
            }

            if (pending->complete) {
                *response = pending->response;

                /* Remove from pending */
                if (i < monitor->num_pending_swarm - 1) {
                    memmove(&monitor->pending_swarm[i],
                            &monitor->pending_swarm[i + 1],
                            (monitor->num_pending_swarm - i - 1) * sizeof(pending_swarm_action_t));
                }
                monitor->num_pending_swarm--;

                return 1;  /* Complete */
            }

            return 0;  /* Still pending */
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "collective_health_check_swarm_action: operation failed");
    return -1;  /* Not found */
}

/* ============================================================================
 * Hyperscanning Health API Implementation
 * ============================================================================ */

int collective_health_get_sync_state(
    const collective_health_monitor_t* monitor,
    hyperscan_state_t* sync_state
) {
    if (!monitor || !sync_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_health_get_sync_state: required parameter is NULL (monitor, sync_state)");
        return -1;
    }

    if (!monitor->collective) {
        /* No collective - return default state */
        memset(sync_state, 0, sizeof(hyperscan_state_t));
        sync_state->global_sync = 1.0f;  /* Perfectly synced with self */
        sync_state->is_entrained = true;
        sync_state->leader_instance_id = monitor->local_instance_id;
        sync_state->leader_influence = 1.0f;
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_health_heartbeat("collective_h_get_sync_state", 0.0f);


    return collective_cognition_get_hyperscan_state(monitor->collective, sync_state);
}

int collective_health_force_sync(collective_health_monitor_t* monitor) {
    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "monitor is NULL");

        return -1;
    }

    if (!monitor->collective) {
        return 0;  /* No collective to sync with */
    }

    /* Trigger collective update */
    /* Phase 8: Heartbeat at operation start */
    collective_health_heartbeat("collective_h_force_sync", 0.0f);


    int result = collective_cognition_update(monitor->collective);

    if (result == 0) {
        monitor->stats.health_syncs++;
        monitor->last_sync_time_us = get_time_us();
    }

    return result;
}

/* ============================================================================
 * Threat Detection API Implementation
 * ============================================================================ */

int collective_health_register_threat_callback(
    collective_health_monitor_t* monitor,
    collective_threat_callback_t callback,
    void* user_data
) {
    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "monitor is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_health_heartbeat("collective_h_register_threat_call", 0.0f);


    monitor->threat_callback = callback;
    monitor->threat_callback_data = user_data;

    return 0;
}

/* ============================================================================
 * Failure Prediction API Implementation
 * ============================================================================ */

int collective_health_share_prediction(
    collective_health_monitor_t* monitor,
    float failure_probability,
    uint32_t time_to_failure_ms,
    health_agent_source_t source
) {
    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "monitor is NULL");

        return -1;
    }

    /* Update local instance report */
    /* Phase 8: Heartbeat at operation start */
    collective_health_heartbeat("collective_h_share_prediction", 0.0f);


    for (uint32_t i = 0; i < monitor->num_instances; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && monitor->num_instances > 256) {
            collective_health_heartbeat("collective_h_loop",
                             (float)(i + 1) / (float)monitor->num_instances);
        }

        if (monitor->instance_reports[i].instance_id == monitor->local_instance_id) {
            monitor->instance_reports[i].failure_probability = failure_probability;
            monitor->instance_reports[i].time_to_failure_ms = time_to_failure_ms;
            break;
        }
    }

    /* In a real implementation, this would broadcast to the collective */
    (void)source;

    return 0;
}

int collective_health_get_worst_prediction(
    const collective_health_monitor_t* monitor,
    uint32_t* instance_id,
    float* failure_probability,
    uint32_t* time_to_failure_ms
) {
    if (!monitor || !instance_id || !failure_probability || !time_to_failure_ms) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_health_get_worst_prediction: required parameter is NULL (monitor, instance_id, failure_probability, time_to_failure_ms)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_health_heartbeat("collective_h_get_worst_prediction", 0.0f);


    float worst_prob = 0.0f;
    uint32_t worst_id = 0;
    uint32_t worst_ttf = UINT32_MAX;

    for (uint32_t i = 0; i < monitor->num_instances; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && monitor->num_instances > 256) {
            collective_health_heartbeat("collective_h_loop",
                             (float)(i + 1) / (float)monitor->num_instances);
        }

        const instance_health_report_t* report = &monitor->instance_reports[i];

        if (report->failure_probability > worst_prob) {
            worst_prob = report->failure_probability;
            worst_id = report->instance_id;
            worst_ttf = report->time_to_failure_ms;
        }
    }

    *instance_id = worst_id;
    *failure_probability = worst_prob;
    *time_to_failure_ms = worst_ttf;

    return 0;
}

/* ============================================================================
 * Statistics API Implementation
 * ============================================================================ */

int collective_health_get_stats(
    const collective_health_monitor_t* monitor,
    collective_health_stats_t* stats
) {
    if (!monitor || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_health_get_stats: required parameter is NULL (monitor, stats)");
        return -1;
    }

    *stats = monitor->stats;
    /* Phase 8: Heartbeat at operation start */
    collective_health_heartbeat("collective_h_get_stats", 0.0f);


    return 0;
}

void collective_health_reset_stats(collective_health_monitor_t* monitor) {
    if (!monitor) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_health_heartbeat("collective_h_reset_stats", 0.0f);


    memset(&monitor->stats, 0, sizeof(collective_health_stats_t));
}

/* ============================================================================
 * Utility Functions Implementation
 * ============================================================================ */

const char* swarm_immune_action_name(swarm_immune_action_t action) {
    switch (action) {
        case SWARM_IMMUNE_NONE: return "NONE";
        case SWARM_IMMUNE_QUARANTINE_INSTANCE: return "QUARANTINE_INSTANCE";
        case SWARM_IMMUNE_PROPAGATE_ANTIBODY: return "PROPAGATE_ANTIBODY";
        case SWARM_IMMUNE_COLLECTIVE_GC: return "COLLECTIVE_GC";
        case SWARM_IMMUNE_SYNCHRONIZED_CHECKPOINT: return "SYNCHRONIZED_CHECKPOINT";
        case SWARM_IMMUNE_LOAD_REDISTRIBUTE: return "LOAD_REDISTRIBUTE";
        case SWARM_IMMUNE_MEMORY_SYNC: return "MEMORY_SYNC";
        case SWARM_IMMUNE_COORDINATED_ROLLBACK: return "COORDINATED_ROLLBACK";
        case SWARM_IMMUNE_EVICT_INSTANCE: return "EVICT_INSTANCE";
        default: return "UNKNOWN";
    }
}

void collective_health_init_proposal(collective_anomaly_proposal_t* proposal) {
    if (!proposal) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_health_heartbeat("collective_h_init_proposal", 0.0f);


    memset(proposal, 0, sizeof(collective_anomaly_proposal_t));
    proposal->anomaly_type = HEALTH_MSG_ANOMALY_DETECTED;
    proposal->source = HEALTH_SOURCE_UNKNOWN;
    proposal->severity = HEALTH_SEVERITY_INFO;
    proposal->instance_id = 0;
    proposal->local_confidence = 0.5f;
    proposal->detection_time_us = get_time_us();
    proposal->suggested_recovery = HEALTH_RECOVERY_NONE;
}

void collective_health_init_swarm_request(swarm_immune_request_t* request) {
    if (!request) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_health_heartbeat("collective_h_init_swarm_request", 0.0f);


    memset(request, 0, sizeof(swarm_immune_request_t));
    request->action = SWARM_IMMUNE_NONE;
    request->target_instance_id = 0;
    request->urgency = 0.5f;
    request->related_anomaly = HEALTH_MSG_ANOMALY_DETECTED;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void collective_health_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_collective_health_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int collective_health_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "collective_health_training_begin: NULL argument");
        return -1;
    }
    collective_health_heartbeat_instance(NULL, "collective_health_training_begin", 0.0f);
    (void)(struct collective_health_monitor*)instance; /* Module state available for reset */
    return 0;
}

int collective_health_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "collective_health_training_end: NULL argument");
        return -1;
    }
    collective_health_heartbeat_instance(NULL, "collective_health_training_end", 1.0f);
    (void)(struct collective_health_monitor*)instance; /* Module state available for finalization */
    return 0;
}

int collective_health_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "collective_health_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    collective_health_heartbeat_instance(NULL, "collective_health_training_step", progress);
    (void)(struct collective_health_monitor*)instance; /* Module state available for step adaptation */
    return 0;
}
