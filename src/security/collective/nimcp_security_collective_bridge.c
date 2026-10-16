/**
 * @file nimcp_security_collective_bridge.c
 * @brief Security-Collective Cognition Bridge Implementation
 * @version 1.0.0
 * @date 2025-01-10
 *
 * WHAT: Bidirectional integration between security subsystem and collective cognition
 * WHY:  Protect swarm/collective systems from Byzantine agents, consensus manipulation,
 *       and emergent behavior exploitation
 * HOW:  Byzantine detection, consensus verification, swarm monitoring, pattern validation
 *
 * @author NIMCP Development Team
 */

#include "security/collective/nimcp_security_collective_bridge.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_collective_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


#define LOG_MODULE "SECURITY_COLLECTIVE_BRIDGE"


/* ============================================================================
 * Internal Constants
 * ============================================================================ */

/** Default agent tracking capacity */
#define DEFAULT_AGENT_CAPACITY         64

/** Module ID for bio-async */
#define SECURITY_COLLECTIVE_MODULE_ID  0x53434F4C  /* 'SCOL' */

/** Minimum trust score */
#define MIN_TRUST_SCORE                0.0f

/** Maximum trust score */
#define MAX_TRUST_SCORE                1.0f

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in milliseconds
 */
static uint64_t get_timestamp_ms(void) {
    return nimcp_time_get_us() / 1000;
}

/**
 * @brief Get current timestamp in nanoseconds
 */
static uint64_t get_timestamp_ns(void) {
    return nimcp_time_get_us() * 1000;
}

/**
 * @brief Update running average
 */
static float update_running_avg(float current, uint64_t count, float new_val) {
    if (count == 0) return new_val;
    return (current * (float)(count - 1) + new_val) / (float)count;
}

/**
 * @brief Convert trust score to level
 */
static trust_level_t score_to_trust_level(float score) {
    if (score < 0.1f) return TRUST_LEVEL_UNTRUSTED;
    if (score < 0.3f) return TRUST_LEVEL_MINIMAL;
    if (score < 0.5f) return TRUST_LEVEL_LOW;
    if (score < 0.7f) return TRUST_LEVEL_MODERATE;
    if (score < 0.9f) return TRUST_LEVEL_HIGH;
    return TRUST_LEVEL_VERIFIED;
}

/**
 * @brief Find agent index in tracking arrays
 */
static int find_agent_index(
    const security_collective_bridge_t* bridge,
    uint32_t agent_id
) {
    for (uint32_t i = 0; i < bridge->tracked_agent_count; i++) {
        if (bridge->agent_trust_scores[i].agent_id == agent_id) {
            return (int)i;
        }
    }
    /* Agent not found - normal case during registration checks, not an error */
    return -1;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int security_collective_default_config(security_collective_config_t* config) {
    if (!config) return NIMCP_ERROR_NULL_POINTER;

    memset(config, 0, sizeof(security_collective_config_t));

    /* Byzantine Detection */
    config->enable_byzantine_detection = true;
    config->byzantine_threshold = SECURITY_COLLECTIVE_DEFAULT_BYZANTINE_THRESHOLD;
    config->min_conflicts_for_byzantine = 3;
    config->enable_automatic_quarantine = true;

    /* Consensus Verification */
    config->enable_consensus_verification = true;
    config->min_quorum_ratio = 0.67f;
    config->enable_sybil_detection = true;
    config->consensus_timeout_ms = 5000;

    /* Swarm Monitoring */
    config->enable_swarm_monitoring = true;
    config->monitoring_interval_ms = SECURITY_COLLECTIVE_DEFAULT_VERIFY_INTERVAL;
    config->anomaly_threshold = 0.7f;

    /* Emergent Pattern Validation */
    config->enable_pattern_validation = true;
    config->pattern_confidence_threshold = 0.6f;
    config->pattern_history_size = SECURITY_COLLECTIVE_MAX_PATTERNS;

    /* Agent Trust Scoring */
    config->enable_trust_scoring = true;
    config->initial_trust_score = 0.5f;
    config->trust_decay_rate = SECURITY_COLLECTIVE_DEFAULT_TRUST_DECAY;
    config->trust_boost_rate = 0.05f;

    /* Sensitivity Factors */
    config->security_sensitivity = 1.0f;
    config->collective_sensitivity = 1.0f;

    return 0;
}

security_collective_bridge_t* security_collective_bridge_create(
    const security_collective_config_t* config
) {
    security_collective_bridge_t* bridge = nimcp_malloc(sizeof(security_collective_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_collective_bridge_create: failed to allocate bridge");
        return NULL;
    }
    memset(bridge, 0, sizeof(security_collective_bridge_t));

    /* Initialize config */
    if (config) {
        bridge->config = *config;
    } else {
        security_collective_default_config(&bridge->config);
    }

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, SECURITY_COLLECTIVE_MODULE_ID,
                         "security_collective_bridge") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "security_collective_bridge_create: failed to init bridge base");
        nimcp_free(bridge);
        return NULL;
    }

    /* Allocate agent tracking arrays */
    bridge->agent_byzantine_status = nimcp_malloc(
        sizeof(byzantine_detection_result_t) * DEFAULT_AGENT_CAPACITY);
    if (!bridge->agent_byzantine_status) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_collective_bridge_create: failed to allocate byzantine status");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        return NULL;
    }
    memset(bridge->agent_byzantine_status, 0,
           sizeof(byzantine_detection_result_t) * DEFAULT_AGENT_CAPACITY);

    bridge->agent_trust_scores = nimcp_malloc(
        sizeof(agent_trust_result_t) * DEFAULT_AGENT_CAPACITY);
    if (!bridge->agent_trust_scores) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_collective_bridge_create: failed to allocate trust scores");
        nimcp_free(bridge->agent_byzantine_status);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        return NULL;
    }
    memset(bridge->agent_trust_scores, 0,
           sizeof(agent_trust_result_t) * DEFAULT_AGENT_CAPACITY);

    bridge->tracked_agent_count = 0;

    /* Initialize state */
    bridge->state.swarm_behavior = SWARM_BEHAVIOR_IDLE;
    bridge->state.swarm_health = 1.0f;
    bridge->state.avg_trust_score = bridge->config.initial_trust_score;
    bridge->state.last_update_time = get_timestamp_ms();

    return bridge;
}

void security_collective_bridge_destroy(security_collective_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "security_collective");

    if (bridge->agent_byzantine_status) {
        nimcp_free(bridge->agent_byzantine_status);
        bridge->agent_byzantine_status = NULL;
    }

    if (bridge->agent_trust_scores) {
        nimcp_free(bridge->agent_trust_scores);
        bridge->agent_trust_scores = NULL;
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API Implementation
 * ============================================================================ */

int security_collective_bridge_connect_collective(
    security_collective_bridge_t* bridge,
    collective_cognition_t* collective
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(collective);

    BRIDGE_LOCK(bridge);
    bridge->collective = collective;
    bridge->base.system_a = collective;
    bridge->base.system_a_connected = true;
    bridge->base.bridge_active = bridge->base.system_a_connected &&
                                  bridge->base.system_b_connected;
    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_collective_bridge_connect_policy_engine(
    security_collective_bridge_t* bridge,
    nimcp_policy_engine_t policy_engine
) {
    BRIDGE_NULL_CHECK(bridge);
    if (!policy_engine) return NIMCP_ERROR_NULL_POINTER;

    BRIDGE_LOCK(bridge);
    bridge->policy_engine = policy_engine;
    bridge->base.system_b = policy_engine;
    bridge->base.system_b_connected = true;
    bridge->base.bridge_active = bridge->base.system_a_connected &&
                                  bridge->base.system_b_connected;
    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_collective_bridge_disconnect(security_collective_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);
    bridge->collective = NULL;
    bridge->policy_engine = NULL;
    bridge->base.system_a = NULL;
    bridge->base.system_b = NULL;
    bridge->base.system_a_connected = false;
    bridge->base.system_b_connected = false;
    bridge->base.bridge_active = false;
    BRIDGE_UNLOCK(bridge);

    return 0;
}

bool security_collective_bridge_is_connected(
    const security_collective_bridge_t* bridge
) {
    BRIDGE_NULL_CHECK_BOOL(bridge);
    return bridge->base.bridge_active;
}

/* ============================================================================
 * Security -> Collective Direction
 * ============================================================================ */

int security_collective_detect_byzantine(
    security_collective_bridge_t* bridge,
    uint32_t agent_id,
    byzantine_detection_result_t* result
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(result);

    uint64_t start_time = get_timestamp_ns();

    BRIDGE_LOCK(bridge);

    memset(result, 0, sizeof(byzantine_detection_result_t));
    result->agent_id = agent_id;

    bridge->stats.total_byzantine_checks++;

    int idx = find_agent_index(bridge, agent_id);
    if (idx < 0) {
        result->status = BYZANTINE_STATUS_NORMAL;
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    /* Copy existing Byzantine status */
    *result = bridge->agent_byzantine_status[idx];

    /* Check if threshold exceeded */
    if (result->message_count > 0) {
        float conflict_ratio = (float)result->conflict_count / (float)result->message_count;
        if (conflict_ratio >= bridge->config.byzantine_threshold) {
            if (result->conflict_count >= bridge->config.min_conflicts_for_byzantine) {
                result->status = BYZANTINE_STATUS_CONFIRMED;
                result->confidence = conflict_ratio;
                bridge->stats.byzantine_detections++;

                if (bridge->config.enable_automatic_quarantine &&
                    !result->is_quarantined) {
                    result->is_quarantined = true;
                    result->status = BYZANTINE_STATUS_QUARANTINED;
                    bridge->state.quarantined_count++;
                    bridge->stats.agents_quarantined++;

                    /* Apply trust penalty for auto-quarantine */
                    bridge->agent_trust_scores[idx].trust_score *= 0.1f;
                    bridge->agent_trust_scores[idx].level = TRUST_LEVEL_UNTRUSTED;
                }
            } else {
                result->status = BYZANTINE_STATUS_SUSPECTED;
                result->confidence = conflict_ratio * 0.5f;
            }
        }
    }

    /* Update tracking */
    bridge->agent_byzantine_status[idx] = *result;

    /* Recalculate swarm health if quarantine state changed */
    if (result->is_quarantined && bridge->state.total_agents > 0) {
        float total_trust = 0.0f;
        for (uint32_t j = 0; j < bridge->tracked_agent_count; j++) {
            total_trust += bridge->agent_trust_scores[j].trust_score;
        }
        bridge->state.avg_trust_score = total_trust / (float)bridge->tracked_agent_count;

        float health = 1.0f;
        health -= (float)bridge->state.quarantined_count / (float)bridge->state.total_agents;
        health *= bridge->state.avg_trust_score;
        bridge->state.swarm_health = nimcp_clampf(health, 0.0f, 1.0f);
    }

    uint64_t elapsed = get_timestamp_ns() - start_time;
    bridge->stats.avg_detection_time_ns = update_running_avg(
        bridge->stats.avg_detection_time_ns,
        bridge->stats.total_byzantine_checks,
        (float)elapsed);

    bridge->state.last_byzantine_check = get_timestamp_ms();

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_collective_verify_consensus(
    security_collective_bridge_t* bridge,
    uint32_t consensus_id,
    const uint32_t* participants,
    uint32_t num_participants,
    consensus_verification_result_t* result
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(result);

    BRIDGE_LOCK(bridge);

    memset(result, 0, sizeof(consensus_verification_result_t));
    result->consensus_id = consensus_id;
    result->participant_count = num_participants;

    bridge->stats.consensus_verifications++;

    /* Check quorum */
    if (num_participants < 2) {
        result->validity = CONSENSUS_INVALID_QUORUM;
        result->quorum_ratio = 0.0f;
        bridge->stats.invalid_consensuses++;
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    /* Count valid and invalid votes based on trust */
    uint32_t valid_votes = 0;
    uint32_t invalid_votes = 0;
    uint32_t suspected_sybil = 0;

    if (participants) {
        for (uint32_t i = 0; i < num_participants; i++) {
            int idx = find_agent_index(bridge, participants[i]);
            if (idx >= 0) {
                if (bridge->agent_byzantine_status[idx].is_quarantined) {
                    invalid_votes++;
                } else if (bridge->agent_trust_scores[idx].trust_score <
                           bridge->config.initial_trust_score * 0.5f) {
                    suspected_sybil++;
                    invalid_votes++;
                } else {
                    valid_votes++;
                }
            } else {
                suspected_sybil++;
            }
        }
    } else {
        valid_votes = num_participants;
    }

    result->valid_votes = valid_votes;
    result->invalid_votes = invalid_votes;
    result->suspected_sybil_count = suspected_sybil;

    /* Calculate quorum ratio */
    float total_agents = (float)bridge->state.total_agents;
    if (total_agents < 1.0f) total_agents = 1.0f;

    result->quorum_ratio = (float)valid_votes / total_agents;

    /* Determine validity */
    if (result->quorum_ratio < bridge->config.min_quorum_ratio) {
        result->validity = CONSENSUS_INVALID_QUORUM;
        bridge->stats.invalid_consensuses++;
    } else if (suspected_sybil > num_participants / 3) {
        result->validity = CONSENSUS_INVALID_SYBIL;
        bridge->stats.sybil_attacks_detected++;
        bridge->stats.invalid_consensuses++;
    } else if (invalid_votes > valid_votes) {
        result->validity = CONSENSUS_INVALID_MANIPULATION;
        result->manipulation_detected = true;
        bridge->stats.invalid_consensuses++;
    } else {
        result->validity = CONSENSUS_VALID;
        bridge->stats.valid_consensuses++;
    }

    bridge->stats.avg_quorum_ratio = update_running_avg(
        bridge->stats.avg_quorum_ratio,
        bridge->stats.consensus_verifications,
        result->quorum_ratio);

    bridge->state.last_consensus_verify = get_timestamp_ms();

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_collective_quarantine_agent(
    security_collective_bridge_t* bridge,
    uint32_t agent_id,
    const char* reason
) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);

    int idx = find_agent_index(bridge, agent_id);
    if (idx < 0) {
        BRIDGE_UNLOCK(bridge);
        return NIMCP_ERROR_NOT_FOUND;
    }

    if (bridge->agent_byzantine_status[idx].is_quarantined) {
        BRIDGE_UNLOCK(bridge);
        return 0;  /* Already quarantined */
    }

    bridge->agent_byzantine_status[idx].is_quarantined = true;
    bridge->agent_byzantine_status[idx].status = BYZANTINE_STATUS_QUARANTINED;
    bridge->state.quarantined_count++;
    bridge->stats.agents_quarantined++;

    /* Add to effects */
    if (bridge->security_effects.quarantined_agent_count <
        SECURITY_COLLECTIVE_MAX_QUARANTINE) {
        uint32_t q_idx = bridge->security_effects.quarantined_agent_count;
        bridge->security_effects.quarantined_agents[q_idx] = agent_id;
        bridge->security_effects.quarantined_agent_count++;
    }

    /* Apply trust penalty */
    bridge->agent_trust_scores[idx].trust_score *= 0.1f;
    bridge->agent_trust_scores[idx].negative_actions++;
    bridge->agent_trust_scores[idx].level = TRUST_LEVEL_UNTRUSTED;

    /* Recalculate swarm health after quarantine */
    if (bridge->state.total_agents > 0) {
        float total_trust = 0.0f;
        for (uint32_t i = 0; i < bridge->tracked_agent_count; i++) {
            total_trust += bridge->agent_trust_scores[i].trust_score;
        }
        bridge->state.avg_trust_score = total_trust / (float)bridge->tracked_agent_count;

        float health = 1.0f;
        health -= (float)bridge->state.quarantined_count / (float)bridge->state.total_agents;
        health *= bridge->state.avg_trust_score;
        bridge->state.swarm_health = nimcp_clampf(health, 0.0f, 1.0f);
    }

    (void)reason;  /* Used for logging in full implementation */

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_collective_release_agent(
    security_collective_bridge_t* bridge,
    uint32_t agent_id
) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);

    int idx = find_agent_index(bridge, agent_id);
    if (idx < 0) {
        BRIDGE_UNLOCK(bridge);
        return NIMCP_ERROR_NOT_FOUND;
    }

    if (!bridge->agent_byzantine_status[idx].is_quarantined) {
        BRIDGE_UNLOCK(bridge);
        return 0;  /* Not quarantined */
    }

    bridge->agent_byzantine_status[idx].is_quarantined = false;
    bridge->agent_byzantine_status[idx].status = BYZANTINE_STATUS_NORMAL;
    bridge->agent_byzantine_status[idx].conflict_count = 0;

    if (bridge->state.quarantined_count > 0) {
        bridge->state.quarantined_count--;
    }

    bridge->stats.agents_cleared++;

    /* Remove from effects quarantine list */
    for (uint32_t i = 0; i < bridge->security_effects.quarantined_agent_count; i++) {
        if (bridge->security_effects.quarantined_agents[i] == agent_id) {
            for (uint32_t j = i; j < bridge->security_effects.quarantined_agent_count - 1; j++) {
                bridge->security_effects.quarantined_agents[j] =
                    bridge->security_effects.quarantined_agents[j + 1];
            }
            bridge->security_effects.quarantined_agent_count--;
            break;
        }
    }

    /* Set trust to minimal */
    bridge->agent_trust_scores[idx].trust_score = 0.1f;
    bridge->agent_trust_scores[idx].level = TRUST_LEVEL_MINIMAL;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_collective_validate_emergent(
    security_collective_bridge_t* bridge,
    uint32_t pattern_id,
    emergent_pattern_result_t* result
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(result);

    BRIDGE_LOCK(bridge);

    memset(result, 0, sizeof(emergent_pattern_result_t));
    result->pattern_id = pattern_id;
    result->emergence_time = get_timestamp_ms();

    bridge->stats.patterns_validated++;

    /* Calculate pattern authenticity based on contributing agents */
    float total_trust = 0.0f;
    uint32_t contributing = 0;

    for (uint32_t i = 0; i < bridge->tracked_agent_count; i++) {
        if (!bridge->agent_byzantine_status[i].is_quarantined) {
            total_trust += bridge->agent_trust_scores[i].trust_score;
            contributing++;
        }
    }

    result->contributing_agents = contributing;

    if (contributing > 0 && bridge->tracked_agent_count > 0) {
        float avg_trust = total_trust / (float)contributing;
        /* Scale by ratio of contributing agents to total agents */
        float participation_ratio = (float)contributing / (float)bridge->tracked_agent_count;
        result->authenticity_score = avg_trust * participation_ratio;
    } else {
        result->authenticity_score = 0.0f;
    }

    result->confidence = result->authenticity_score;

    /* Determine status */
    if (result->authenticity_score >= bridge->config.pattern_confidence_threshold) {
        result->status = EMERGENT_PATTERN_VALID;
        result->is_stable = true;
    } else if (result->authenticity_score >= 0.3f) {
        result->status = EMERGENT_PATTERN_SUSPICIOUS;
        result->is_stable = false;
    } else if (result->authenticity_score < 0.1f) {
        result->status = EMERGENT_PATTERN_MANIPULATED;
        result->is_stable = false;
        bridge->stats.patterns_rejected++;
    } else {
        result->status = EMERGENT_PATTERN_UNKNOWN;
        result->is_stable = false;
    }

    bridge->stats.avg_pattern_confidence = update_running_avg(
        bridge->stats.avg_pattern_confidence,
        bridge->stats.patterns_validated,
        result->confidence);

    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Collective -> Security Direction
 * ============================================================================ */

int security_collective_monitor_swarm(
    security_collective_bridge_t* bridge,
    swarm_monitoring_result_t* result
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(result);

    BRIDGE_LOCK(bridge);

    memset(result, 0, sizeof(swarm_monitoring_result_t));

    bridge->stats.swarm_monitoring_updates++;

    result->current_behavior = bridge->state.swarm_behavior;
    result->active_agents = bridge->tracked_agent_count - bridge->state.quarantined_count;
    result->last_update_time = get_timestamp_ms();

    /* Calculate synchronization and coherence */
    float total_trust = 0.0f;
    uint32_t active_count = 0;

    for (uint32_t i = 0; i < bridge->tracked_agent_count; i++) {
        if (!bridge->agent_byzantine_status[i].is_quarantined) {
            total_trust += bridge->agent_trust_scores[i].trust_score;
            active_count++;
        }
    }

    if (active_count > 0) {
        result->synchronization_level = total_trust / (float)active_count;
        result->coherence_level = result->synchronization_level;
    }

    /* Calculate fragmentation */
    if (bridge->state.total_agents > 0) {
        result->fragmentation_index = (float)bridge->state.quarantined_count /
                                       (float)bridge->state.total_agents;
    }

    /* Check for anomalies */
    if (result->fragmentation_index > bridge->config.anomaly_threshold ||
        result->synchronization_level < (1.0f - bridge->config.anomaly_threshold)) {
        result->anomaly_detected = true;
        result->anomaly_score = fmaxf(result->fragmentation_index,
                                       1.0f - result->synchronization_level);
        bridge->stats.anomalies_detected++;
    }

    bridge->stats.avg_synchronization = update_running_avg(
        bridge->stats.avg_synchronization,
        bridge->stats.swarm_monitoring_updates,
        result->synchronization_level);

    if (result->fragmentation_index > bridge->stats.max_fragmentation) {
        bridge->stats.max_fragmentation = result->fragmentation_index;
    }

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_collective_score_agent(
    security_collective_bridge_t* bridge,
    uint32_t agent_id,
    agent_trust_result_t* result
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(result);

    BRIDGE_LOCK(bridge);

    memset(result, 0, sizeof(agent_trust_result_t));
    result->agent_id = agent_id;

    int idx = find_agent_index(bridge, agent_id);
    if (idx < 0) {
        result->level = TRUST_LEVEL_UNTRUSTED;
        result->trust_score = 0.0f;
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    *result = bridge->agent_trust_scores[idx];

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_collective_report_action(
    security_collective_bridge_t* bridge,
    uint32_t agent_id,
    bool positive,
    float weight
) {
    BRIDGE_NULL_CHECK(bridge);

    weight = nimcp_clampf(weight, 0.0f, 1.0f);

    BRIDGE_LOCK(bridge);

    int idx = find_agent_index(bridge, agent_id);
    if (idx < 0) {
        BRIDGE_UNLOCK(bridge);
        return NIMCP_ERROR_NOT_FOUND;
    }

    agent_trust_result_t* trust = &bridge->agent_trust_scores[idx];
    float old_score = trust->trust_score;

    if (positive) {
        trust->positive_actions++;
        trust->trust_score += bridge->config.trust_boost_rate * weight;
    } else {
        trust->negative_actions++;
        trust->trust_score -= bridge->config.trust_boost_rate * weight * 2.0f;

        /* Record conflict for Byzantine detection */
        bridge->agent_byzantine_status[idx].conflict_count++;
        bridge->agent_byzantine_status[idx].message_count++;
    }

    trust->trust_score = nimcp_clampf(trust->trust_score, MIN_TRUST_SCORE, MAX_TRUST_SCORE);
    trust->last_action_time = get_timestamp_ms();

    /* Update trend */
    trust->trend = trust->trust_score - old_score;

    /* Update level */
    trust_level_t old_level = trust->level;
    trust->level = score_to_trust_level(trust->trust_score);

    if (trust->level > old_level) {
        bridge->stats.trust_promotions++;
    } else if (trust->level < old_level) {
        bridge->stats.trust_demotions++;
    }

    bridge->stats.trust_updates++;
    bridge->stats.avg_trust_change = update_running_avg(
        bridge->stats.avg_trust_change,
        bridge->stats.trust_updates,
        fabsf(trust->trend));

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_collective_register_agent(
    security_collective_bridge_t* bridge,
    uint32_t agent_id
) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);

    /* Check if already registered */
    if (find_agent_index(bridge, agent_id) >= 0) {
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    /* Check capacity */
    if (bridge->tracked_agent_count >= DEFAULT_AGENT_CAPACITY) {
        BRIDGE_UNLOCK(bridge);
        return NIMCP_ERROR_OUT_OF_RANGE;  /* Capacity exceeded */
    }

    uint32_t idx = bridge->tracked_agent_count;
    uint64_t now = get_timestamp_ms();

    /* Initialize Byzantine status */
    memset(&bridge->agent_byzantine_status[idx], 0, sizeof(byzantine_detection_result_t));
    bridge->agent_byzantine_status[idx].agent_id = agent_id;
    bridge->agent_byzantine_status[idx].status = BYZANTINE_STATUS_NORMAL;
    bridge->agent_byzantine_status[idx].last_activity_time = now;

    /* Initialize trust score */
    memset(&bridge->agent_trust_scores[idx], 0, sizeof(agent_trust_result_t));
    bridge->agent_trust_scores[idx].agent_id = agent_id;
    bridge->agent_trust_scores[idx].trust_score = bridge->config.initial_trust_score;
    bridge->agent_trust_scores[idx].level = score_to_trust_level(bridge->config.initial_trust_score);
    bridge->agent_trust_scores[idx].first_seen_time = now;
    bridge->agent_trust_scores[idx].last_action_time = now;

    bridge->tracked_agent_count++;
    bridge->state.total_agents = bridge->tracked_agent_count;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_collective_unregister_agent(
    security_collective_bridge_t* bridge,
    uint32_t agent_id
) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);

    int idx = find_agent_index(bridge, agent_id);
    if (idx < 0) {
        BRIDGE_UNLOCK(bridge);
        return NIMCP_ERROR_NOT_FOUND;
    }

    /* If quarantined, update count */
    if (bridge->agent_byzantine_status[idx].is_quarantined &&
        bridge->state.quarantined_count > 0) {
        bridge->state.quarantined_count--;
    }

    /* Shift remaining entries */
    for (uint32_t i = (uint32_t)idx; i < bridge->tracked_agent_count - 1; i++) {
        bridge->agent_byzantine_status[i] = bridge->agent_byzantine_status[i + 1];
        bridge->agent_trust_scores[i] = bridge->agent_trust_scores[i + 1];
    }

    bridge->tracked_agent_count--;
    bridge->state.total_agents = bridge->tracked_agent_count;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

int security_collective_bridge_update(
    security_collective_bridge_t* bridge,
    uint64_t delta_ms
) {
    BRIDGE_NULL_CHECK(bridge);

    uint64_t start_time = get_timestamp_ns();

    BRIDGE_LOCK(bridge);

    bridge->stats.bridge_updates++;
    bridge->state.last_update_time = get_timestamp_ms();

    /* Apply trust decay */
    if (bridge->config.enable_trust_scoring) {
        float decay = bridge->config.trust_decay_rate * (float)delta_ms / 1000.0f;

        for (uint32_t i = 0; i < bridge->tracked_agent_count; i++) {
            bridge->agent_trust_scores[i].trust_score -= decay;
            bridge->agent_trust_scores[i].trust_score = nimcp_clampf(
                bridge->agent_trust_scores[i].trust_score,
                MIN_TRUST_SCORE, MAX_TRUST_SCORE);
            bridge->agent_trust_scores[i].level = score_to_trust_level(
                bridge->agent_trust_scores[i].trust_score);
        }
    }

    /* Update aggregate trust */
    float total_trust = 0.0f;
    for (uint32_t i = 0; i < bridge->tracked_agent_count; i++) {
        total_trust += bridge->agent_trust_scores[i].trust_score;
    }
    if (bridge->tracked_agent_count > 0) {
        bridge->state.avg_trust_score = total_trust / (float)bridge->tracked_agent_count;
    }

    /* Update trust distribution */
    memset(bridge->state.trust_distribution, 0, sizeof(bridge->state.trust_distribution));
    for (uint32_t i = 0; i < bridge->tracked_agent_count; i++) {
        trust_level_t level = bridge->agent_trust_scores[i].level;
        if (level <= TRUST_LEVEL_VERIFIED) {
            bridge->state.trust_distribution[level]++;
        }
    }

    /* Normalize distribution */
    if (bridge->tracked_agent_count > 0) {
        for (int i = 0; i < 5; i++) {
            bridge->state.trust_distribution[i] /= (float)bridge->tracked_agent_count;
        }
    }

    /* Update swarm health */
    float health = 1.0f;
    if (bridge->state.total_agents > 0) {
        health -= (float)bridge->state.quarantined_count / (float)bridge->state.total_agents;
        health *= bridge->state.avg_trust_score;
    }
    bridge->state.swarm_health = nimcp_clampf(health, 0.0f, 1.0f);

    /* Record update time */
    uint64_t update_time = get_timestamp_ns() - start_time;
    bridge->stats.avg_update_time_ns = update_running_avg(
        bridge->stats.avg_update_time_ns,
        bridge->stats.bridge_updates,
        (float)update_time);

    bridge_base_record_update(&bridge->base);

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_collective_apply_security_effects(
    security_collective_bridge_t* bridge
) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);

    float sensitivity = nimcp_clampf(bridge->config.security_sensitivity, 0.5f, 2.0f);

    /* Update security effects */
    bridge->security_effects.avg_swarm_trust = bridge->state.avg_trust_score;
    bridge->security_effects.allowed_synchronization = bridge->state.swarm_health;

    /* Count untrusted agents */
    uint32_t untrusted = 0;
    for (uint32_t i = 0; i < bridge->tracked_agent_count; i++) {
        if (bridge->agent_trust_scores[i].level <= TRUST_LEVEL_MINIMAL) {
            untrusted++;
        }
    }
    bridge->security_effects.untrusted_agent_count = untrusted;

    /* Scale effects by sensitivity */
    bridge->security_effects.allowed_synchronization *= sensitivity;
    if (bridge->security_effects.allowed_synchronization > 1.0f) {
        bridge->security_effects.allowed_synchronization = 1.0f;
    }

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_collective_apply_collective_effects(
    security_collective_bridge_t* bridge
) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);

    float sensitivity = nimcp_clampf(bridge->config.collective_sensitivity, 0.5f, 2.0f);

    /* Update collective effects from state */
    bridge->collective_effects.current_behavior = bridge->state.swarm_behavior;
    bridge->collective_effects.active_agent_count = bridge->tracked_agent_count -
                                                     bridge->state.quarantined_count;
    bridge->collective_effects.synchronization_level = bridge->state.avg_trust_score;

    /* Detect unusual behavior */
    if (bridge->state.quarantined_count > bridge->tracked_agent_count / 4) {
        bridge->collective_effects.unusual_behavior = true;
        bridge->collective_effects.behavior_anomaly_score = 0.8f;
    }

    /* Calculate trust variation */
    float min_trust = 1.0f, max_trust = 0.0f;
    for (uint32_t i = 0; i < bridge->tracked_agent_count; i++) {
        float score = bridge->agent_trust_scores[i].trust_score;
        if (score < min_trust) min_trust = score;
        if (score > max_trust) max_trust = score;
    }
    bridge->collective_effects.trust_variation = max_trust - min_trust;

    (void)sensitivity;  /* Used for scaling in full implementation */

    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

int security_collective_bridge_get_state(
    const security_collective_bridge_t* bridge,
    security_collective_state_t* state
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(state);

    security_collective_bridge_t* mutable_bridge = (security_collective_bridge_t*)bridge;

    BRIDGE_LOCK(mutable_bridge);
    *state = bridge->state;
    BRIDGE_UNLOCK(mutable_bridge);

    return 0;
}

int security_collective_bridge_get_stats(
    const security_collective_bridge_t* bridge,
    security_collective_stats_t* stats
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(stats);

    security_collective_bridge_t* mutable_bridge = (security_collective_bridge_t*)bridge;

    BRIDGE_LOCK(mutable_bridge);
    *stats = bridge->stats;
    BRIDGE_UNLOCK(mutable_bridge);

    return 0;
}

int security_collective_get_security_effects(
    const security_collective_bridge_t* bridge,
    security_to_collective_effects_t* effects
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(effects);

    security_collective_bridge_t* mutable_bridge = (security_collective_bridge_t*)bridge;

    BRIDGE_LOCK(mutable_bridge);
    *effects = bridge->security_effects;
    BRIDGE_UNLOCK(mutable_bridge);

    return 0;
}

int security_collective_get_collective_effects(
    const security_collective_bridge_t* bridge,
    collective_to_security_effects_t* effects
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(effects);

    security_collective_bridge_t* mutable_bridge = (security_collective_bridge_t*)bridge;

    BRIDGE_LOCK(mutable_bridge);
    *effects = bridge->collective_effects;
    BRIDGE_UNLOCK(mutable_bridge);

    return 0;
}

int security_collective_get_quarantined_agents(
    const security_collective_bridge_t* bridge,
    uint32_t* agent_ids,
    uint32_t max_agents,
    uint32_t* num_agents
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(agent_ids);
    BRIDGE_NULL_CHECK(num_agents);

    if (max_agents == 0) {
        *num_agents = 0;
        return 0;
    }

    security_collective_bridge_t* mutable_bridge = (security_collective_bridge_t*)bridge;

    BRIDGE_LOCK(mutable_bridge);

    uint32_t count = 0;
    for (uint32_t i = 0; i < bridge->tracked_agent_count && count < max_agents; i++) {
        if (bridge->agent_byzantine_status[i].is_quarantined) {
            agent_ids[count++] = bridge->agent_byzantine_status[i].agent_id;
        }
    }

    *num_agents = count;

    BRIDGE_UNLOCK(mutable_bridge);

    return 0;
}

int security_collective_get_agents_by_trust(
    const security_collective_bridge_t* bridge,
    trust_level_t level,
    uint32_t* agent_ids,
    uint32_t max_agents,
    uint32_t* num_agents
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(agent_ids);
    BRIDGE_NULL_CHECK(num_agents);

    if (max_agents == 0) {
        *num_agents = 0;
        return 0;
    }

    security_collective_bridge_t* mutable_bridge = (security_collective_bridge_t*)bridge;

    BRIDGE_LOCK(mutable_bridge);

    uint32_t count = 0;
    for (uint32_t i = 0; i < bridge->tracked_agent_count && count < max_agents; i++) {
        if (bridge->agent_trust_scores[i].level == level) {
            agent_ids[count++] = bridge->agent_trust_scores[i].agent_id;
        }
    }

    *num_agents = count;

    BRIDGE_UNLOCK(mutable_bridge);

    return 0;
}

int security_collective_bridge_reset_stats(security_collective_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);
    memset(&bridge->stats, 0, sizeof(security_collective_stats_t));
    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int security_collective_bridge_connect_bio_async(
    security_collective_bridge_t* bridge
) {
    BRIDGE_NULL_CHECK(bridge);
    return bridge_base_connect_bio_async(&bridge->base);
}

int security_collective_bridge_disconnect_bio_async(
    security_collective_bridge_t* bridge
) {
    BRIDGE_NULL_CHECK(bridge);
    return bridge_base_disconnect_bio_async(&bridge->base);
}

bool security_collective_bridge_is_bio_async_connected(
    const security_collective_bridge_t* bridge
) {
    BRIDGE_NULL_CHECK_BOOL(bridge);
    return bridge_base_is_bio_async_connected(&bridge->base);
}
