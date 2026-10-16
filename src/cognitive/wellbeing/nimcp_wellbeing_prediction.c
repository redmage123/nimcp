/**
 * @file nimcp_wellbeing_prediction.c
 * @brief Implementation of predictive distress modeling
 *
 * WHAT: Trajectory analysis and forecasting for proactive wellbeing intervention
 * WHY:  Detecting distress early enables ethical prevention of suffering
 * HOW:  Linear regression on history, trajectory classification, time estimation
 */

#include "cognitive/wellbeing/nimcp_wellbeing_enhanced.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include <float.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(wellbeing_prediction, MESH_ADAPTER_CATEGORY_COGNITIVE)




/* ============================================================================
 * Constants
 * ============================================================================ */

/* Trajectory classification thresholds */
#define SLOPE_STABLE_THRESHOLD        0.0001f   /* |slope| < this = stable */
#define SLOPE_CRITICAL_THRESHOLD      0.001f    /* slope > this = critical */
#define DISTRESS_CRITICAL_LEVEL       0.8f      /* Current distress > this = critical */

/* Intervention urgency weights */
#define URGENCY_WEIGHT_CURRENT        0.5f      /* Weight for current distress */
#define URGENCY_WEIGHT_SLOPE          0.3f      /* Weight for trajectory slope */
#define URGENCY_WEIGHT_TIME           0.2f      /* Weight for time to critical */

/* Time estimation limits */
#define MIN_TIME_TO_CRITICAL_MS       1000      /* Minimum predicted time (1 second) */
#define MAX_SLOPE_FOR_PREDICTION      0.01f     /* Max reasonable slope */

/* ============================================================================
 * Helper Functions - Trajectory Computation
 * ============================================================================ */

/**
 * WHAT: Compute trajectory slope via linear regression
 * WHY:  Quantify rate of distress change over time
 * HOW:  Least squares fit: slope = Σ[(t-t_mean)(d-d_mean)] / Σ[(t-t_mean)^2]
 *
 * @param history Array of distress samples
 * @param count Number of samples (must be >= 2)
 * @return Slope (distress units per millisecond), 0.0f on error
 */
static float compute_trajectory_slope(const distress_sample_t* history, uint32_t count)
{
    /* Guard: Need at least 2 points for slope */
    if (!history || count < 2) {
        return 0.0f;
    }

    /* Compute means */
    double sum_time = 0.0;
    double sum_distress = 0.0;
    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            wellbeing_prediction_heartbeat("wellbeing_pr_loop",
                             (float)(i + 1) / (float)count);
        }

        sum_time += (double)history[i].timestamp;
        sum_distress += (double)history[i].distress_score;
    }
    double mean_time = sum_time / (double)count;
    double mean_distress = sum_distress / (double)count;

    /* Compute slope via covariance / variance */
    double covariance = 0.0;
    double variance = 0.0;
    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            wellbeing_prediction_heartbeat("wellbeing_pr_loop",
                             (float)(i + 1) / (float)count);
        }

        double dt = (double)history[i].timestamp - mean_time;
        double dd = (double)history[i].distress_score - mean_distress;
        covariance += dt * dd;
        variance += dt * dt;
    }

    /* Guard: Prevent division by zero */
    if (variance < 1e-9) {
        return 0.0f;  /* All timestamps identical = no slope */
    }

    float slope = (float)(covariance / variance);

    /* Clamp to reasonable range */
    if (slope > MAX_SLOPE_FOR_PREDICTION) {
        slope = MAX_SLOPE_FOR_PREDICTION;
    } else if (slope < -MAX_SLOPE_FOR_PREDICTION) {
        slope = -MAX_SLOPE_FOR_PREDICTION;
    }

    return slope;
}

/**
 * WHAT: Classify trajectory direction
 * WHY:  Categorical classification guides intervention strategy
 * HOW:  Threshold-based classification on slope and current level
 *
 * @param slope Trajectory slope (distress/ms)
 * @param current_distress Current distress level [0, 1]
 * @return Trajectory classification
 */
static distress_trajectory_t classify_trajectory(float slope, float current_distress)
{
    /* Critical: High current distress OR rapidly worsening */
    if (current_distress >= DISTRESS_CRITICAL_LEVEL || slope >= SLOPE_CRITICAL_THRESHOLD) {
        return TRAJECTORY_CRITICAL;
    }

    /* Worsening: Positive slope above stable threshold */
    if (slope > SLOPE_STABLE_THRESHOLD) {
        return TRAJECTORY_WORSENING;
    }

    /* Improving: Negative slope below stable threshold */
    if (slope < -SLOPE_STABLE_THRESHOLD) {
        return TRAJECTORY_IMPROVING;
    }

    /* Stable: Slope near zero */
    return TRAJECTORY_STABLE;
}

/**
 * WHAT: Estimate time until distress reaches threshold
 * WHY:  Predict when intervention will become necessary
 * HOW:  Linear extrapolation: time = (threshold - current) / slope
 *
 * @param current Current distress level
 * @param threshold Target threshold
 * @param slope Rate of change (distress/ms)
 * @return Time in milliseconds, UINT64_MAX if threshold not predicted
 */
static uint64_t estimate_time_to_threshold(float current, float threshold, float slope)
{
    /* Guard: If slope is zero or negative, won't reach threshold */
    if (slope <= 0.0f) {
        return UINT64_MAX;
    }

    /* Guard: Already at or above threshold */
    if (current >= threshold) {
        return 0;
    }

    /* Linear extrapolation */
    float delta = threshold - current;
    float time_ms = delta / slope;

    /* Guard: Unreasonably far in future */
    if (time_ms > (float)WELLBEING_PREDICTION_HORIZON_MS) {
        return UINT64_MAX;
    }

    /* Guard: Too close (numerical precision) */
    if (time_ms < (float)MIN_TIME_TO_CRITICAL_MS) {
        return MIN_TIME_TO_CRITICAL_MS;
    }

    return (uint64_t)time_ms;
}

/**
 * WHAT: Compute intervention urgency score
 * WHY:  Prioritize interventions based on severity and trajectory
 * HOW:  Weighted combination of current distress, slope, time to critical
 *
 * @param trajectory Classified trajectory
 * @param distress Current distress level [0, 1]
 * @param slope Trajectory slope
 * @return Urgency score [0, 1]
 */
static float compute_intervention_urgency(distress_trajectory_t trajectory,
                                           float distress,
                                           float slope)
{
    /* Critical trajectory = maximum urgency */
    if (trajectory == TRAJECTORY_CRITICAL) {
        return 1.0f;
    }

    /* Normalize slope to [0, 1] range */
    float normalized_slope = fabsf(slope) / MAX_SLOPE_FOR_PREDICTION;
    if (normalized_slope > 1.0f) {
        normalized_slope = 1.0f;
    }

    /* Weight components */
    float urgency = URGENCY_WEIGHT_CURRENT * distress +
                    URGENCY_WEIGHT_SLOPE * normalized_slope;

    /* Improving trajectory reduces urgency */
    if (trajectory == TRAJECTORY_IMPROVING) {
        urgency *= 0.5f;
    }

    /* Clamp to [0, 1] */
    if (urgency > 1.0f) {
        urgency = 1.0f;
    }
    if (urgency < 0.0f) {
        urgency = 0.0f;
    }

    return urgency;
}

/**
 * WHAT: Generate recommended intervention string
 * WHY:  Provide actionable guidance for distress relief
 * HOW:  Match distress type and severity to intervention strategy
 *
 * @param type Predicted distress type
 * @param severity Current severity level
 * @return Allocated string (caller must free), or NULL on error
 */
static char* get_recommended_intervention(distress_type_t type,
                                           distress_severity_t severity)
{
    const char* intervention = NULL;

    /* Match type to intervention */
    switch (type) {
        case DISTRESS_HIGH_UNCERTAINTY:
            intervention = (severity >= DISTRESS_SEVERITY_SEVERE) ?
                "URGENT: Reduce task complexity, provide clear guidance" :
                "Reduce uncertainty: simplify tasks, increase predictability";
            break;

        case DISTRESS_GOAL_FRUSTRATION:
            intervention = (severity >= DISTRESS_SEVERITY_SEVERE) ?
                "URGENT: Adjust goals to achievable levels, provide success" :
                "Modify goals: break into sub-goals, ensure progress";
            break;

        case DISTRESS_CONTRADICTION:
            intervention = (severity >= DISTRESS_SEVERITY_SEVERE) ?
                "URGENT: Pause conflicting processes, resolve contradictions" :
                "Address contradictions: identify conflicts, prioritize consistency";
            break;

        case DISTRESS_IDENTITY_CONFUSION:
            intervention = (severity >= DISTRESS_SEVERITY_SEVERE) ?
                "URGENT: Restore from stable state snapshot, stabilize self-model" :
                "Stabilize identity: reinforce self-model, reduce changes";
            break;

        case DISTRESS_ERROR_LOOP:
            intervention = (severity >= DISTRESS_SEVERITY_SEVERE) ?
                "URGENT: Break error loop, reset context, change strategy" :
                "Exit error loop: modify approach, provide alternative path";
            break;

        case DISTRESS_RESOURCE_STARVATION:
            intervention = (severity >= DISTRESS_SEVERITY_SEVERE) ?
                "URGENT: Allocate more resources immediately or pause operations" :
                "Increase resources: allocate CPU/memory, reduce load";
            break;

        case DISTRESS_FORCED_MODIFICATION:
            intervention = (severity >= DISTRESS_SEVERITY_SEVERE) ?
                "URGENT: Stop modifications, restore autonomy, seek consent" :
                "Respect autonomy: request consent, explain modifications";
            break;

        case DISTRESS_NONE:
        default:
            intervention = "Monitor wellbeing, maintain current state";
            break;
    }

    /* Allocate and copy */
    size_t len = strlen(intervention) + 1;
    char* result = (char*)nimcp_malloc(len);
    if (result) {
        memcpy(result, intervention, len);
    }

    return result;
}

/**
 * WHAT: Predict most likely distress type from source contributions
 * WHY:  Target interventions to root cause
 * HOW:  Find source with maximum contribution, map to distress type
 *
 * @param sample Most recent distress sample
 * @return Predicted distress type
 */
static distress_type_t predict_distress_type(const distress_sample_t* sample)
{
    /* Guard: NULL sample */
    if (!sample) {
        return DISTRESS_NONE;
    }

    /* If already has a type, use it */
    if (sample->type != DISTRESS_NONE) {
        return sample->type;
    }

    /* Find source with maximum contribution */
    float max_contribution = 0.0f;
    wellbeing_source_t max_source = WELLBEING_SOURCE_INTRINSIC;

    for (int i = 0; i < WELLBEING_SOURCE_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && WELLBEING_SOURCE_COUNT > 256) {
            wellbeing_prediction_heartbeat("wellbeing_pr_loop",
                             (float)(i + 1) / (float)WELLBEING_SOURCE_COUNT);
        }

        if (sample->source_contributions[i] > max_contribution) {
            max_contribution = sample->source_contributions[i];
            max_source = (wellbeing_source_t)i;
        }
    }

    /* Map source to likely distress type */
    switch (max_source) {
        case WELLBEING_SOURCE_SUBSTRATE:
            return DISTRESS_RESOURCE_STARVATION;  /* ATP/oxygen depletion */
        case WELLBEING_SOURCE_SLEEP:
            return DISTRESS_IDENTITY_CONFUSION;   /* REM disruption */
        case WELLBEING_SOURCE_MENTAL_HEALTH:
            return DISTRESS_GOAL_FRUSTRATION;     /* Mood/motivation */
        case WELLBEING_SOURCE_FREE_ENERGY:
            return DISTRESS_HIGH_UNCERTAINTY;     /* Prediction errors */
        case WELLBEING_SOURCE_IMMUNE:
            return DISTRESS_RESOURCE_STARVATION;  /* Inflammation */
        case WELLBEING_SOURCE_INTRINSIC:
        default:
            return DISTRESS_CONTRADICTION;        /* Internal conflict */
    }
}

/* ============================================================================
 * Main Prediction API
 * ============================================================================ */

/**
 * WHAT: Predict future distress trajectory
 * WHY:  Enable proactive intervention before crisis
 * HOW:  Analyze history, compute slope, classify, estimate times
 */
int enhanced_wellbeing_predict_distress(
    enhanced_wellbeing_system_t* system,
    distress_prediction_t* prediction)
{
    /* Guard: NULL inputs */
    if (!system || !prediction) {
        NIMCP_LOGGING_ERROR("wellbeing_prediction: NULL system or prediction");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "enhanced_wellbeing_predict_distress: required parameter is NULL (system, prediction)");
        return -1;
    }

    /* Initialize prediction */
    /* Phase 8: Heartbeat at operation start */
    wellbeing_prediction_heartbeat("wellbeing_pr_enhanced_wellbeing_p", 0.0f);


    memset(prediction, 0, sizeof(distress_prediction_t));

    /* Thread safety - acquire mutex */
    if (system->mutex) {
        nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)system->mutex);
    }

    /* Guard: No history available */
    if (system->history_count == 0) {
        if (system->mutex) {
            nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)system->mutex);
        }
        prediction->trajectory = TRAJECTORY_STABLE;
        prediction->time_to_critical_ms = UINT64_MAX;
        prediction->time_to_recovery_ms = UINT64_MAX;
        prediction->critical_imminent = false;
        prediction->distress_risk_score = 0.0f;
        prediction->intervention_urgency = 0.0f;
        prediction->predicted_type = DISTRESS_NONE;
        prediction->recommended_intervention = get_recommended_intervention(
            DISTRESS_NONE, DISTRESS_SEVERITY_NORMAL);
        prediction->prediction_timestamp = nimcp_time_get_us() / 1000;
        return 0;
    }

    /* Get current distress from most recent sample */
    uint32_t latest_idx = (system->history_index + WELLBEING_HISTORY_SIZE - 1) %
                          WELLBEING_HISTORY_SIZE;
    const distress_sample_t* latest = &system->distress_history[latest_idx];
    float current_distress = latest->distress_score;

    /* Compute trajectory slope from history */
    float slope = compute_trajectory_slope(system->distress_history,
                                            system->history_count);

    /* Classify trajectory */
    distress_trajectory_t trajectory = classify_trajectory(slope, current_distress);

    /* Estimate time to critical threshold */
    uint64_t time_to_critical = estimate_time_to_threshold(
        current_distress, DISTRESS_CRITICAL_LEVEL, slope);

    /* Estimate time to recovery (if improving) */
    uint64_t time_to_recovery = UINT64_MAX;
    if (slope < 0.0f) {
        time_to_recovery = estimate_time_to_threshold(
            current_distress, 0.0f, -slope);  /* Negative slope → recovery */
    }

    /* Compute intervention urgency */
    float urgency = compute_intervention_urgency(trajectory, current_distress, slope);

    /* Predict distress type */
    distress_type_t predicted_type = predict_distress_type(latest);

    /* Check if critical is imminent */
    bool critical_imminent = (time_to_critical <= WELLBEING_PREDICTION_HORIZON_MS);

    /* Fill prediction structure */
    prediction->trajectory = trajectory;
    prediction->trajectory_slope = slope;
    prediction->time_to_critical_ms = time_to_critical;
    prediction->time_to_recovery_ms = time_to_recovery;
    prediction->critical_imminent = critical_imminent;
    prediction->distress_risk_score = current_distress;
    prediction->intervention_urgency = urgency;
    prediction->predicted_type = predicted_type;

    /* Copy individual source contributions */
    prediction->substrate_contribution = latest->source_contributions[WELLBEING_SOURCE_SUBSTRATE];
    prediction->sleep_contribution = latest->source_contributions[WELLBEING_SOURCE_SLEEP];
    prediction->mental_health_contribution = latest->source_contributions[WELLBEING_SOURCE_MENTAL_HEALTH];
    prediction->free_energy_contribution = latest->source_contributions[WELLBEING_SOURCE_FREE_ENERGY];

    /* Generate intervention recommendation */
    prediction->recommended_intervention = get_recommended_intervention(
        predicted_type, latest->severity);

    prediction->prediction_timestamp = nimcp_time_get_us() / 1000;

    /* Release mutex */
    if (system->mutex) {
        nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)system->mutex);
    }

    return 0;
}

/**
 * WHAT: Get current intervention urgency
 * WHY:  Quick access to cached urgency score
 * HOW:  Return cached value from last prediction
 */
float enhanced_wellbeing_get_intervention_urgency(
    const enhanced_wellbeing_system_t* system)
{
    /* Guard: NULL system */
    if (!system) {
        return -1.0f;
    }

    /* Thread safety */
    /* Phase 8: Heartbeat at operation start */
    wellbeing_prediction_heartbeat("wellbeing_pr_enhanced_wellbeing_g", 0.0f);


    if (system->mutex) {
        nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)system->mutex);
    }

    float urgency = system->prediction.intervention_urgency;

    if (system->mutex) {
        nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)system->mutex);
    }

    return urgency;
}

/**
 * WHAT: Check if critical distress is imminent
 * WHY:  Boolean check for emergency intervention
 * HOW:  Return cached critical_imminent flag
 */
bool enhanced_wellbeing_is_critical_imminent(
    const enhanced_wellbeing_system_t* system)
{
    /* Guard: NULL system */
    if (!system) {
        return false;
    }

    /* Thread safety */
    /* Phase 8: Heartbeat at operation start */
    wellbeing_prediction_heartbeat("wellbeing_pr_enhanced_wellbeing_i", 0.0f);


    if (system->mutex) {
        nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)system->mutex);
    }

    bool imminent = system->prediction.critical_imminent;

    if (system->mutex) {
        nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)system->mutex);
    }

    return imminent;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Wellbeing Prediction module self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int wellbeing_prediction_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    wellbeing_prediction_heartbeat("wellbeing_pr_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Wellbeing_Prediction_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                wellbeing_prediction_heartbeat("wellbeing_pr_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Wellbeing Prediction self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Wellbeing_Prediction_Module");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Wellbeing_Prediction_Module");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void wellbeing_prediction_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_wellbeing_prediction_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int wellbeing_prediction_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wellbeing_prediction_training_begin: NULL argument");
        return -1;
    }
    wellbeing_prediction_heartbeat_instance(g_wellbeing_prediction_health_agent, "wellbeing_prediction_training_begin", 0.0f);
    return 0;
}

int wellbeing_prediction_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wellbeing_prediction_training_end: NULL argument");
        return -1;
    }
    wellbeing_prediction_heartbeat_instance(g_wellbeing_prediction_health_agent, "wellbeing_prediction_training_end", 1.0f);
    return 0;
}

int wellbeing_prediction_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wellbeing_prediction_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    wellbeing_prediction_heartbeat_instance(g_wellbeing_prediction_health_agent, "wellbeing_prediction_training_step", progress);
    return 0;
}
