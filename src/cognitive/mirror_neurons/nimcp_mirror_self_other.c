/**
 * @file nimcp_mirror_self_other.c
 * @brief Mirror Neuron Self-Other Distinction Implementation
 * @version 1.0.0
 * @date 2025-01-05
 *
 * WHAT: Implements self-other distinction for mirror neuron modulation
 * WHY:  Discriminate own actions from observed actions to modulate mirroring
 * HOW:  Efference copy comparison, body schema, temporal/spatial contingency
 */

#include "cognitive/mirror_neurons/nimcp_mirror_self_other.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(mirror_self_other, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Internal Structure
//=============================================================================

struct self_other_system {
    self_other_config_t config;
    self_other_state_t state;

    /** Body schema */
    body_schema_t body_schema;

    /** Efference copy buffer (ring buffer) */
    efference_copy_t efference_buffer[SELF_OTHER_EFFERENCE_BUFFER];
    uint32_t efference_head;
    uint32_t efference_count;

    /** Decision history */
    agency_decision_t decision_history[SELF_OTHER_HISTORY_SIZE];
    uint32_t history_head;
    uint32_t history_count;

    /** Statistics */
    self_other_stats_t stats;

    /** Thread safety */
    nimcp_mutex_t* mutex;

    /** Bio-async */
    bool bio_async_registered;
};

static inline float pose_distance(const body_pose_t* a, const body_pose_t* b) {
    float dx = a->x - b->x;
    float dy = a->y - b->y;
    float dz = a->z - b->z;
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

static inline float pose_rotation_distance(const body_pose_t* a, const body_pose_t* b) {
    float drx = a->rx - b->rx;
    float dry = a->ry - b->ry;
    float drz = a->rz - b->rz;
    return sqrtf(drx*drx + dry*dry + drz*drz);
}

/**
 * @brief Compute pose similarity [0-1]
 */
static float compute_pose_similarity(const body_pose_t* a, const body_pose_t* b) {
    float pos_dist = pose_distance(a, b);
    float rot_dist = pose_rotation_distance(a, b);

    /* Exponential decay similarity */
    float pos_sim = expf(-pos_dist * 2.0f);  /* ~0.37 at 0.5m */
    float rot_sim = expf(-rot_dist * 0.5f);  /* ~0.37 at 2 radians */

    return 0.7f * pos_sim + 0.3f * rot_sim;
}

/**
 * @brief Find efference copy for action
 */
static efference_copy_t* find_efference_for_action(
    self_other_system_t* system,
    body_part_id_t effector,
    uint64_t action_time_us
) {
    uint64_t window = SELF_OTHER_CONTINGENCY_WINDOW_US;

    for (uint32_t i = 0; i < system->efference_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->efference_count > 256) {
            mirror_self_other_heartbeat("mirror_self__loop",
                             (float)(i + 1) / (float)system->efference_count);
        }

        uint32_t idx = (system->efference_head - 1 - i + SELF_OTHER_EFFERENCE_BUFFER)
                       % SELF_OTHER_EFFERENCE_BUFFER;
        efference_copy_t* ec = &system->efference_buffer[idx];

        if (ec->consumed) continue;
        if (ec->effector != effector) continue;

        /* Check temporal proximity */
        int64_t dt = (int64_t)action_time_us - (int64_t)ec->command_time_us;
        if (dt < 0) continue;  /* Action before command */
        if ((uint64_t)dt > window + (uint64_t)(ec->predicted_duration_ms * 1000.0f)) continue;

        return ec;
    }
    return NULL;
}

/**
 * @brief Compute body center position
 */
static void compute_body_center(const body_schema_t* schema, float* x, float* y, float* z) {
    /* Use torso as body center */
    for (uint32_t i = 0; i < schema->active_part_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && schema->active_part_count > 256) {
            mirror_self_other_heartbeat("mirror_self__loop",
                             (float)(i + 1) / (float)schema->active_part_count);
        }

        if (schema->parts[i].part_id == BODY_PART_TORSO) {
            *x = schema->parts[i].pose.x;
            *y = schema->parts[i].pose.y;
            *z = schema->parts[i].pose.z;
            return;
        }
    }
    /* Fallback to origin */
    *x = 0.0f;
    *y = 0.0f;
    *z = 0.0f;
}

//=============================================================================
// Public API Implementation
//=============================================================================

self_other_config_t self_other_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    mirror_self_other_heartbeat("mirror_self__self_other_config_de", 0.0f);


    self_other_config_t config = {
        .efference_match_threshold = 0.7f,
        .temporal_contingency_ms = 200.0f,
        .spatial_self_radius = 0.5f,

        .weight_efference = 0.4f,
        .weight_temporal = 0.25f,
        .weight_spatial = 0.2f,
        .weight_proprioceptive = 0.1f,
        .weight_visual = 0.05f,

        .self_threshold = 0.6f,
        .other_threshold = 0.4f,

        .self_suppression_gain = 0.2f,
        .enable_automatic_suppression = true,

        .enable_simd = true,
        .bio_async_enabled = true
    };
    return config;
}

self_other_system_t* self_other_create(const self_other_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    mirror_self_other_heartbeat("mirror_self__self_other_create", 0.0f);


    self_other_system_t* system = nimcp_calloc(1, sizeof(self_other_system_t));
    if (!system) {
        nimcp_log(LOG_LEVEL_ERROR, "Self-Other: Failed to allocate system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "self_other_create: system is NULL");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        system->config = *config;
    } else {
        system->config = self_other_config_default();
    }

    /* Initialize state */
    system->state = SELF_OTHER_STATE_IDLE;
    system->efference_head = 0;
    system->efference_count = 0;
    system->history_head = 0;
    system->history_count = 0;

    /* Initialize body schema with defaults */
    system->body_schema.active_part_count = 0;
    system->body_schema.reach_distance = 0.8f;  /* ~80cm arm reach */
    system->body_schema.personal_space_radius = 1.5f;

    for (int i = 0; i < BODY_PART_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && BODY_PART_COUNT > 256) {
            mirror_self_other_heartbeat("mirror_self__loop",
                             (float)(i + 1) / (float)BODY_PART_COUNT);
        }

        system->body_schema.max_joint_velocities[i] = 3.0f;  /* rad/s default */
    }

    /* Create mutex */
    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_NORMAL;
    system->mutex = nimcp_mutex_create(&attr);
    if (!system->mutex) {
        nimcp_log(LOG_LEVEL_ERROR, "Self-Other: Failed to create mutex");
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "self_other_create: system->mutex is NULL");
        return NULL;
    }

    /* Register with bio-async if enabled */
    if (system->config.bio_async_enabled) {
        self_other_register_bio_async(system);
    }

    nimcp_log(LOG_LEVEL_INFO, "Self-Other: Created system (efference_threshold=%.2f)",
              system->config.efference_match_threshold);

    return system;
}

void self_other_destroy(self_other_system_t* system) {
    if (!system) return;

    /* Phase 8: Heartbeat at operation start */
    mirror_self_other_heartbeat("mirror_self__self_other_destroy", 0.0f);


    if (system->bio_async_registered) {
        self_other_unregister_bio_async(system);
    }

    if (system->mutex) {
        nimcp_mutex_destroy(system->mutex);
    }

    nimcp_free(system);
    system = NULL;
    nimcp_log(LOG_LEVEL_DEBUG, "Self-Other: Destroyed system");
}

//=============================================================================
// Efference Copy API
//=============================================================================

bool self_other_register_efference(
    self_other_system_t* system,
    uint32_t action_id,
    body_part_id_t effector,
    const body_pose_t* intended_pose,
    float predicted_duration_ms
) {
    if (!system || !intended_pose) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_other_register_efference: required parameter is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_self_other_heartbeat("mirror_self__self_other_register_", 0.0f);


    nimcp_mutex_lock(system->mutex);

    /* Store in ring buffer */
    efference_copy_t* ec = &system->efference_buffer[system->efference_head];
    ec->action_id = action_id;
    ec->effector = effector;
    ec->intended_pose = *intended_pose;
    ec->predicted_sensory = *intended_pose;  /* Simple forward model */
    ec->predicted_duration_ms = predicted_duration_ms;
    ec->command_time_us = nimcp_time_now_us();
    ec->awaiting_feedback = true;
    ec->consumed = false;

    system->efference_head = (system->efference_head + 1) % SELF_OTHER_EFFERENCE_BUFFER;
    if (system->efference_count < SELF_OTHER_EFFERENCE_BUFFER) {
        system->efference_count++;
    }

    nimcp_mutex_unlock(system->mutex);
    return true;
}

efference_copy_t* self_other_get_efference(
    self_other_system_t* system,
    uint32_t action_id
) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    mirror_self_other_heartbeat("mirror_self__self_other_get_effer", 0.0f);


    for (uint32_t i = 0; i < system->efference_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->efference_count > 256) {
            mirror_self_other_heartbeat("mirror_self__loop",
                             (float)(i + 1) / (float)system->efference_count);
        }

        uint32_t idx = (system->efference_head - 1 - i + SELF_OTHER_EFFERENCE_BUFFER)
                       % SELF_OTHER_EFFERENCE_BUFFER;
        if (system->efference_buffer[idx].action_id == action_id) {
            return &system->efference_buffer[idx];
        }
    }
    return NULL;
}

void self_other_clear_expired_efference(self_other_system_t* system) {
    if (!system) return;

    /* Phase 8: Heartbeat at operation start */
    mirror_self_other_heartbeat("mirror_self__self_other_clear_exp", 0.0f);


    nimcp_mutex_lock(system->mutex);

    uint64_t now = nimcp_time_now_us();
    uint64_t expiry = 2000000;  /* 2 second expiry */

    for (uint32_t i = 0; i < system->efference_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->efference_count > 256) {
            mirror_self_other_heartbeat("mirror_self__loop",
                             (float)(i + 1) / (float)system->efference_count);
        }

        uint32_t idx = (system->efference_head - 1 - i + SELF_OTHER_EFFERENCE_BUFFER)
                       % SELF_OTHER_EFFERENCE_BUFFER;
        efference_copy_t* ec = &system->efference_buffer[idx];

        if (now - ec->command_time_us > expiry) {
            ec->consumed = true;
        }
    }

    nimcp_mutex_unlock(system->mutex);
}

//=============================================================================
// Body Schema API
//=============================================================================

void self_other_update_body_part(
    self_other_system_t* system,
    body_part_id_t part,
    const body_pose_t* pose,
    const body_pose_t* velocity,
    float confidence
) {
    if (!system || !pose || part >= BODY_PART_COUNT) {
        if (!system || !pose) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_other_update_body_part: required parameter is NULL");
            return;
        }
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_self_other_heartbeat("mirror_self__self_other_update_bo", 0.0f);


    nimcp_mutex_lock(system->mutex);

    /* Find or create entry */
    body_part_state_t* found = NULL;
    for (uint32_t i = 0; i < system->body_schema.active_part_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && system->body_schema.active_part_count > 256) {
            mirror_self_other_heartbeat("mirror_self__loop",
                             (float)(i + 1) / (float)system->body_schema.active_part_count);
        }

        if (system->body_schema.parts[i].part_id == part) {
            found = &system->body_schema.parts[i];
            break;
        }
    }

    if (!found && system->body_schema.active_part_count < SELF_OTHER_MAX_BODY_PARTS) {
        found = &system->body_schema.parts[system->body_schema.active_part_count++];
        found->part_id = part;
    }

    if (found) {
        found->pose = *pose;
        if (velocity) {
            found->velocity = *velocity;
        }
        found->confidence = confidence;
        found->timestamp_us = nimcp_time_now_us();
    }

    system->body_schema.last_update_us = nimcp_time_now_us();

    nimcp_mutex_unlock(system->mutex);
}

const body_schema_t* self_other_get_body_schema(const self_other_system_t* system) {
    /* Phase 8: Heartbeat at operation start */
    mirror_self_other_heartbeat("mirror_self__self_other_get_body_", 0.0f);


    return system ? &system->body_schema : NULL;
}

bool self_other_in_peripersonal_space(
    const self_other_system_t* system,
    float x, float y, float z
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_other_in_peripersonal_space: system is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_self_other_heartbeat("mirror_self__self_other_in_peripe", 0.0f);


    float dist = self_other_distance_from_body(system, x, y, z);
    return dist <= system->body_schema.personal_space_radius;
}

float self_other_distance_from_body(
    const self_other_system_t* system,
    float x, float y, float z
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_other_distance_from_body: system is NULL");
        return 999.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_self_other_heartbeat("mirror_self__self_other_distance_", 0.0f);


    float cx, cy, cz;
    compute_body_center(&system->body_schema, &cx, &cy, &cz);

    float dx = x - cx;
    float dy = y - cy;
    float dz = z - cz;

    return sqrtf(dx*dx + dy*dy + dz*dz);
}

//=============================================================================
// Agency Classification API
//=============================================================================

bool self_other_classify_agency(
    self_other_system_t* system,
    const action_observation_t* observation,
    const sensory_feedback_t* sensory,
    agency_decision_t* decision
) {
    if (!system || !observation || !decision) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_other_classify_agency: required parameter is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_self_other_heartbeat("mirror_self__self_other_classify_", 0.0f);


    nimcp_mutex_lock(system->mutex);

    uint64_t start_time = nimcp_time_now_us();
    memset(decision, 0, sizeof(agency_decision_t));
    decision->decision_time_us = start_time;

    system->state = SELF_OTHER_STATE_COMPARING;

    /* Find matching efference copy */
    efference_copy_t* efference = find_efference_for_action(
        system, observation->effector, observation->onset_time_us);

    /* Compute evidence scores */
    float efference_score = 0.0f;
    float temporal_score = 0.0f;
    float spatial_score = 0.0f;
    float proprioceptive_score = 0.0f;
    float visual_score = 0.0f;

    /* 1. Efference copy match */
    if (efference) {
        float pose_sim = compute_pose_similarity(&efference->intended_pose,
                                                  &observation->end_pose);
        efference_score = pose_sim;
        decision->efference_match = pose_sim;

        /* Prediction error */
        decision->prediction_error = 1.0f - pose_sim;

        /* Mark as consumed if good match */
        if (pose_sim > system->config.efference_match_threshold) {
            efference->consumed = true;
        }
    }

    /* 2. Temporal contingency */
    if (observation->preceded_by_intent) {
        temporal_score = self_other_compute_temporal_contingency(
            observation->intent_time_us,
            observation->onset_time_us,
            &system->config
        );
    }
    decision->temporal_match = temporal_score;

    /* 3. Spatial self-space */
    if (observation->in_peripersonal_space) {
        float dist_factor = 1.0f - (observation->distance_from_self /
                                     system->body_schema.personal_space_radius);
        spatial_score = nimcp_clampf(dist_factor, 0.0f, 1.0f);
    }
    decision->spatial_match = spatial_score;

    /* 4. Proprioceptive feedback */
    if (sensory && sensory->observation_confidence > 0.5f) {
        /* Find body part in schema */
        for (uint32_t i = 0; i < system->body_schema.active_part_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && system->body_schema.active_part_count > 256) {
                mirror_self_other_heartbeat("mirror_self__loop",
                                 (float)(i + 1) / (float)system->body_schema.active_part_count);
            }

            if (system->body_schema.parts[i].part_id == sensory->effector) {
                float sim = compute_pose_similarity(
                    &system->body_schema.parts[i].pose,
                    &sensory->observed_pose);
                proprioceptive_score = sim;
                break;
            }
        }
    }
    decision->proprioceptive_match = proprioceptive_score;

    /* 5. Visual perspective */
    if (sensory && sensory->is_first_person) {
        visual_score = 0.8f;  /* First-person strongly suggests self */
    } else if (sensory) {
        /* Third-person view, angle-dependent */
        visual_score = 0.2f * (1.0f - fabsf(sensory->visual_angle) / NIMCP_PI_F);
    }

    /* Combine evidence with weights */
    float self_evidence =
        system->config.weight_efference * efference_score +
        system->config.weight_temporal * temporal_score +
        system->config.weight_spatial * spatial_score +
        system->config.weight_proprioceptive * proprioceptive_score +
        system->config.weight_visual * visual_score;

    /* Normalize */
    float total_weight = system->config.weight_efference +
                         system->config.weight_temporal +
                         system->config.weight_spatial +
                         system->config.weight_proprioceptive +
                         system->config.weight_visual;
    self_evidence /= total_weight;

    /* Determine primary evidence source */
    float max_evidence = efference_score;
    decision->primary_evidence = AGENCY_EVIDENCE_EFFERENCE_COPY;

    if (temporal_score > max_evidence) {
        max_evidence = temporal_score;
        decision->primary_evidence = AGENCY_EVIDENCE_TEMPORAL;
    }
    if (spatial_score > max_evidence) {
        max_evidence = spatial_score;
        decision->primary_evidence = AGENCY_EVIDENCE_SPATIAL;
    }
    if (proprioceptive_score > max_evidence) {
        max_evidence = proprioceptive_score;
        decision->primary_evidence = AGENCY_EVIDENCE_PROPRIOCEPTIVE;
    }

    /* Make agency decision */
    system->state = SELF_OTHER_STATE_DECIDING;

    if (self_evidence >= system->config.self_threshold) {
        decision->agency = AGENCY_SELF;
        decision->confidence = self_evidence;
        system->stats.self_attributions++;
    } else if (self_evidence <= system->config.other_threshold) {
        decision->agency = AGENCY_OTHER;
        decision->confidence = 1.0f - self_evidence;
        system->stats.other_attributions++;
    } else {
        /* Ambiguous - could be shared or undetermined */
        if (efference && sensory) {
            decision->agency = AGENCY_SHARED;
            decision->confidence = 0.5f;
            system->stats.shared_attributions++;
        } else {
            decision->agency = AGENCY_UNDETERMINED;
            decision->confidence = 0.3f;
            system->stats.undetermined++;
        }
    }

    /* Update statistics */
    decision->processing_latency_ms =
        (float)(nimcp_time_now_us() - start_time) / 1000.0f;

    system->stats.total_classifications++;
    system->stats.avg_confidence =
        system->stats.avg_confidence * 0.95f + decision->confidence * 0.05f;
    system->stats.avg_prediction_error =
        system->stats.avg_prediction_error * 0.95f + decision->prediction_error * 0.05f;
    system->stats.avg_processing_latency_ms =
        system->stats.avg_processing_latency_ms * 0.95f +
        decision->processing_latency_ms * 0.05f;

    /* Track evidence usage */
    switch (decision->primary_evidence) {
        case AGENCY_EVIDENCE_EFFERENCE_COPY:
            system->stats.efference_based_decisions++;
            break;
        case AGENCY_EVIDENCE_TEMPORAL:
            system->stats.temporal_based_decisions++;
            break;
        case AGENCY_EVIDENCE_SPATIAL:
            system->stats.spatial_based_decisions++;
            break;
        default:
            break;
    }

    /* Store in history */
    system->decision_history[system->history_head] = *decision;
    system->history_head = (system->history_head + 1) % SELF_OTHER_HISTORY_SIZE;
    if (system->history_count < SELF_OTHER_HISTORY_SIZE) {
        system->history_count++;
    }

    system->state = SELF_OTHER_STATE_MONITORING;

    nimcp_mutex_unlock(system->mutex);
    return true;
}

bool self_other_classify_with_efference(
    self_other_system_t* system,
    const action_observation_t* observation,
    const efference_copy_t* efference,
    const sensory_feedback_t* sensory,
    agency_decision_t* decision
) {
    if (!system || !observation || !efference || !decision) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_other_classify_with_efference: required parameter is NULL");
        return false;
    }

    /* Direct comparison with provided efference copy */
    /* Phase 8: Heartbeat at operation start */
    mirror_self_other_heartbeat("mirror_self__self_other_classify_", 0.0f);


    float match = self_other_compare_efference_sensory(efference, sensory);

    decision->efference_match = match;
    decision->prediction_error = 1.0f - match;
    decision->decision_time_us = nimcp_time_now_us();

    if (match >= system->config.efference_match_threshold) {
        decision->agency = AGENCY_SELF;
        decision->confidence = match;
        decision->primary_evidence = AGENCY_EVIDENCE_EFFERENCE_COPY;
    } else {
        decision->agency = AGENCY_OTHER;
        decision->confidence = 1.0f - match;
        decision->primary_evidence = AGENCY_EVIDENCE_EFFERENCE_COPY;
    }

    return true;
}

uint32_t self_other_classify_batch(
    self_other_system_t* system,
    const action_observation_t* observations,
    const sensory_feedback_t* sensory,
    agency_decision_t* decisions,
    uint32_t count
) {
    if (!system || !observations || !decisions || count == 0) {
        if (!system || !observations || !decisions) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_other_classify_batch: required parameter is NULL");
            return 0;
        }
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_self_other_heartbeat("mirror_self__self_other_classify_", 0.0f);


    uint32_t processed = 0;
    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            mirror_self_other_heartbeat("mirror_self__loop",
                             (float)(i + 1) / (float)count);
        }

        const sensory_feedback_t* s = sensory ? &sensory[i] : NULL;
        if (self_other_classify_agency(system, &observations[i], s, &decisions[i])) {
            processed++;
        }
    }

    if (system->config.enable_simd && count >= SELF_OTHER_SIMD_THRESHOLD) {
        system->stats.simd_operations++;
    }

    return processed;
}

//=============================================================================
// Mirror Modulation API
//=============================================================================

float self_other_get_mirror_suppression(
    const self_other_system_t* system,
    agency_type_t agency
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_other_get_mirror_suppression: system is NULL");
        return 1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_self_other_heartbeat("mirror_self__self_other_get_mirro", 0.0f);


    switch (agency) {
        case AGENCY_SELF:
            return system->config.self_suppression_gain;
        case AGENCY_SHARED:
            return 0.5f + 0.5f * system->config.self_suppression_gain;
        case AGENCY_IMAGINED:
            return 0.7f;
        case AGENCY_OTHER:
        case AGENCY_UNDETERMINED:
        default:
            return 1.0f;  /* No suppression for others */
    }
}

float self_other_compute_mirror_gain(
    const self_other_system_t* system,
    const agency_decision_t* decision
) {
    if (!system || !decision) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_other_compute_mirror_gain: required parameter is NULL");
        return 1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_self_other_heartbeat("mirror_self__self_other_compute_m", 0.0f);


    float base_gain = self_other_get_mirror_suppression(system, decision->agency);

    /* Modulate by confidence */
    float confidence_factor = decision->confidence;

    if (decision->agency == AGENCY_SELF) {
        /* More confident self = more suppression */
        return base_gain * (2.0f - confidence_factor);
    } else {
        /* More confident other = full mirroring */
        return base_gain * confidence_factor;
    }
}

//=============================================================================
// Comparator Functions
//=============================================================================

float self_other_compare_efference_sensory(
    const efference_copy_t* efference,
    const sensory_feedback_t* sensory
) {
    if (!efference || !sensory) return 0.0f;

    /* Compare predicted pose with observed pose */
    /* Phase 8: Heartbeat at operation start */
    mirror_self_other_heartbeat("mirror_self__self_other_compare_e", 0.0f);


    float pose_sim = compute_pose_similarity(&efference->predicted_sensory,
                                              &sensory->observed_pose);

    /* Weight by sensory confidence */
    return pose_sim * sensory->observation_confidence;
}

float self_other_compute_temporal_contingency(
    uint64_t intent_time,
    uint64_t action_time,
    const self_other_config_t* config
) {
    if (!config) return 0.0f;
    if (action_time < intent_time) return 0.0f;  /* Action before intent */

    /* Phase 8: Heartbeat at operation start */
    mirror_self_other_heartbeat("mirror_self__self_other_compute_t", 0.0f);


    int64_t delay_us = (int64_t)(action_time - intent_time);
    float delay_ms = (float)delay_us / 1000.0f;

    if (delay_ms > config->temporal_contingency_ms) {
        return 0.0f;  /* Too long delay */
    }

    /* Linear decay with delay */
    return 1.0f - (delay_ms / config->temporal_contingency_ms);
}

void self_other_simd_compare_poses(
    const float* poses_a,
    const float* poses_b,
    float* similarities,
    uint32_t count
) {
    /* Simple scalar implementation - could be vectorized */
    /* Phase 8: Heartbeat at operation start */
    mirror_self_other_heartbeat("mirror_self__self_other_simd_comp", 0.0f);


    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            mirror_self_other_heartbeat("mirror_self__loop",
                             (float)(i + 1) / (float)count);
        }

        float dx = poses_a[i*6 + 0] - poses_b[i*6 + 0];
        float dy = poses_a[i*6 + 1] - poses_b[i*6 + 1];
        float dz = poses_a[i*6 + 2] - poses_b[i*6 + 2];
        float drx = poses_a[i*6 + 3] - poses_b[i*6 + 3];
        float dry = poses_a[i*6 + 4] - poses_b[i*6 + 4];
        float drz = poses_a[i*6 + 5] - poses_b[i*6 + 5];

        float pos_dist = sqrtf(dx*dx + dy*dy + dz*dz);
        float rot_dist = sqrtf(drx*drx + dry*dry + drz*drz);

        float pos_sim = expf(-pos_dist * 2.0f);
        float rot_sim = expf(-rot_dist * 0.5f);

        similarities[i] = 0.7f * pos_sim + 0.3f * rot_sim;
    }
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

bool self_other_register_bio_async(self_other_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_other_register_bio_async: system is NULL");
        return false;
    }

    /* Bio-async registration would go here */
    /* Phase 8: Heartbeat at operation start */
    mirror_self_other_heartbeat("mirror_self__self_other_register_", 0.0f);


    system->bio_async_registered = true;
    nimcp_log(LOG_LEVEL_DEBUG, "Self-Other: Registered with bio-async");
    return true;
}

void self_other_unregister_bio_async(self_other_system_t* system) {
    if (!system) return;
    /* Phase 8: Heartbeat at operation start */
    mirror_self_other_heartbeat("mirror_self__self_other_unregiste", 0.0f);


    system->bio_async_registered = false;
    nimcp_log(LOG_LEVEL_DEBUG, "Self-Other: Unregistered from bio-async");
}

//=============================================================================
// Statistics
//=============================================================================

bool self_other_get_stats(
    const self_other_system_t* system,
    self_other_stats_t* stats
) {
    if (!system || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_other_get_stats: required parameter is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_self_other_heartbeat("mirror_self__self_other_get_stats", 0.0f);


    nimcp_mutex_lock(((self_other_system_t*)system)->mutex);
    *stats = system->stats;
    nimcp_mutex_unlock(((self_other_system_t*)system)->mutex);

    return true;
}

void self_other_reset_stats(self_other_system_t* system) {
    if (!system) return;

    /* Phase 8: Heartbeat at operation start */
    mirror_self_other_heartbeat("mirror_self__self_other_reset_sta", 0.0f);


    nimcp_mutex_lock(system->mutex);
    memset(&system->stats, 0, sizeof(self_other_stats_t));
    nimcp_mutex_unlock(system->mutex);
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void mirror_self_other_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_mirror_self_other_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training stubs
 * ============================================================================ */
int mirror_self_other_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_self_other_training_begin: NULL argument");
        return -1;
    }
    mirror_self_other_heartbeat_instance(NULL, "mirror_self_other_training_begin", 0.0f);
    return 0;
}

int mirror_self_other_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_self_other_training_end: NULL argument");
        return -1;
    }
    mirror_self_other_heartbeat_instance(NULL, "mirror_self_other_training_end", 1.0f);
    return 0;
}

int mirror_self_other_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_self_other_training_step: NULL argument");
        return -1;
    }
    mirror_self_other_heartbeat_instance(NULL, "mirror_self_other_training_step", progress);
    return 0;
}
