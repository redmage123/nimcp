/**
 * @file nimcp_entity_tracker.h
 * @brief Entity Tracker — persistent object identity and permanence
 *
 * WHAT: Tracks objects across time with persistent IDs, handles occlusion,
 *       and maintains belief about hidden objects (object permanence).
 * WHY:  Core Knowledge (Spelke): infants expect objects to continue existing
 *       when hidden. The tracker maintains probabilistic beliefs about
 *       occluded objects and uses the physics engine to predict their state.
 * HOW:  Hungarian-style greedy assignment for observation-to-entity matching,
 *       Kalman-like position/velocity updates, confidence decay for hidden objects.
 */

#ifndef NIMCP_ENTITY_TRACKER_H
#define NIMCP_ENTITY_TRACKER_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/omni/bridges/nimcp_omni_wm_parietal_bridge.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration */
typedef struct intuitive_physics_engine intuitive_physics_engine_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

#define ET_MAX_ENTITIES             128
#define ET_MAX_OBSERVATIONS         64
#define ET_DEFAULT_OCCLUSION_DECAY  0.02f   /* confidence loss per second when hidden */
#define ET_DEFAULT_PERMANENCE_THRESH 0.1f   /* below this, entity is "lost" */
#define ET_DEFAULT_ASSOC_THRESH     0.5f    /* min score to match observation */
#define ET_POSITION_VARIANCE_INIT   1.0f    /* initial position uncertainty (m^2) */

/* ============================================================================
 * Visibility State
 * ============================================================================ */

typedef enum {
    ENTITY_VISIBLE      = 0,    /* currently observed */
    ENTITY_OCCLUDED     = 1,    /* behind something — predicted by physics */
    ENTITY_OUT_OF_VIEW  = 2,    /* outside field of view */
    ENTITY_PREDICTED    = 3,    /* never directly seen, inferred */
    ENTITY_LOST         = 4     /* confidence too low */
} entity_visibility_t;

/* ============================================================================
 * Observation (from perception pipeline)
 * ============================================================================ */

typedef struct {
    wm_parietal_vec3_t      position;
    wm_parietal_velocity_t  velocity;       /* may be zero if unknown */
    float                   mass_hint;      /* 0 = unknown */
    float                   bounding_radius;
    float                   confidence;
    uint32_t                semantic_label; /* optional category id */
} entity_observation_t;

/* ============================================================================
 * Entity Belief
 * ============================================================================ */

typedef struct {
    uint32_t                entity_id;      /* persistent unique ID */
    bool                    active;         /* slot in use */

    /* Physical state (estimated) */
    wm_parietal_vec3_t      pos_mean;
    wm_parietal_vec3_t      pos_variance;   /* per-axis uncertainty */
    wm_parietal_velocity_t  vel_mean;
    float                   mass_estimate;
    float                   mass_confidence;
    float                   bounding_radius;

    /* Tracking state */
    entity_visibility_t     visibility;
    float                   existence_confidence;  /* [0..1] */
    float                   identity_confidence;   /* [0..1] */
    double                  last_observed_time;
    double                  first_observed_time;
    uint32_t                observation_count;
    uint32_t                semantic_label;

    /* Physics prediction (while hidden) */
    wm_parietal_vec3_t      predicted_position;
    wm_parietal_velocity_t  predicted_velocity;
    float                   prediction_age;        /* seconds since last observation */
} entity_belief_t;

/* ============================================================================
 * Config
 * ============================================================================ */

typedef struct {
    uint32_t    max_entities;
    float       occlusion_decay_rate;   /* confidence/sec lost when hidden */
    float       permanence_threshold;   /* below this → LOST */
    float       association_threshold;  /* min score to match obs → entity */
    float       merge_distance;         /* merge entities closer than this (m) */
    float       position_process_noise; /* Kalman process noise */
} entity_tracker_config_t;

/* ============================================================================
 * Tracker
 * ============================================================================ */

typedef struct entity_tracker {
    entity_belief_t*    entities;
    uint32_t            num_entities;
    uint32_t            capacity;
    uint32_t            next_id;

    entity_tracker_config_t config;

    /* Scratch space for association (avoids per-frame alloc) */
    float*              assoc_scores;   /* [ET_MAX_OBSERVATIONS * capacity] */

    /* Statistics */
    uint64_t            total_observations;
    uint64_t            total_new_entities;
    uint64_t            total_lost_entities;
    uint64_t            total_occlusions;
    uint64_t            total_reappearances;

    bool                initialized;
} entity_tracker_t;

/* ============================================================================
 * API
 * ============================================================================ */

/** Create entity tracker */
entity_tracker_t* entity_tracker_create(const entity_tracker_config_t* config);

/** Destroy entity tracker */
void entity_tracker_destroy(entity_tracker_t* tracker);

/** Update with new observations. Matches to existing entities or creates new ones */
int entity_tracker_update(entity_tracker_t* tracker,
                           const entity_observation_t* observations,
                           uint32_t num_obs,
                           double timestamp);

/** Predict positions of all hidden entities using physics engine */
int entity_tracker_predict_hidden(entity_tracker_t* tracker,
                                   intuitive_physics_engine_t* physics,
                                   float dt);

/** Get entity belief by id (NULL if not found or inactive) */
const entity_belief_t* entity_tracker_get(const entity_tracker_t* tracker, uint32_t id);

/** Count entities by visibility state */
uint32_t entity_tracker_count_visible(const entity_tracker_t* tracker);
uint32_t entity_tracker_count_occluded(const entity_tracker_t* tracker);
uint32_t entity_tracker_count_total(const entity_tracker_t* tracker);

/** Check if brain believes object is permanent (exists even when hidden) */
bool entity_tracker_object_permanent(const entity_tracker_t* tracker, uint32_t id);

/** Get all active entity IDs. Returns count written to buf */
uint32_t entity_tracker_get_active_ids(const entity_tracker_t* tracker,
                                        uint32_t* buf, uint32_t buf_size);

/** Decay confidence for hidden entities. Called each frame */
void entity_tracker_decay(entity_tracker_t* tracker, float dt);

/** Default config */
entity_tracker_config_t entity_tracker_default_config(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ENTITY_TRACKER_H */
