/**
 * @file nimcp_physics_prior.h
 * @brief Physics Prior — constrains world model predictions with physical laws
 *
 * WHAT: Post-correction layer that blends learned RSSM predictions with
 *       deterministic physics simulation to produce physically plausible forecasts.
 * WHY:  Pure learned models can predict impossible outcomes (objects teleporting,
 *       floating, interpenetrating). The physics prior enforces conservation laws
 *       and contact constraints as an inductive bias.
 * HOW:  1. Decode spatial components from RSSM latent state
 *       2. Run physics engine from same initial conditions
 *       3. Blend: corrected = (1-w)*learned + w*physics
 *       4. Enforce hard constraints (no interpenetration, energy bounds)
 *       5. Compute physics loss for training: L_physics = ||pred - physics_pred||²
 *
 * The blending weight adapts: increases when the learned model has high error
 * relative to physics, decreases as the learned model improves.
 */

#ifndef NIMCP_PHYSICS_PRIOR_H
#define NIMCP_PHYSICS_PRIOR_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/physics/nimcp_intuitive_physics.h"
#include "cognitive/physics/nimcp_entity_tracker.h"
#include "cognitive/physics/nimcp_scene_graph.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct omni_world_model omni_world_model_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

#define PP_DEFAULT_WEIGHT           0.3f    /* default physics blend weight */
#define PP_MIN_WEIGHT               0.05f   /* never fully trust learned model */
#define PP_MAX_WEIGHT               0.95f   /* never fully trust physics */
#define PP_EMA_ALPHA                0.01f   /* error EMA smoothing */
#define PP_MAX_PREDICTION_HORIZON   5.0f    /* max seconds to predict forward */

/* ============================================================================
 * Violation Types
 * ============================================================================ */

typedef enum {
    PP_VIOLATION_NONE           = 0,
    PP_VIOLATION_INTERPENETRATION = (1 << 0),
    PP_VIOLATION_ENERGY_INCREASE = (1 << 1),
    PP_VIOLATION_TELEPORT       = (1 << 2),   /* position jump > threshold */
    PP_VIOLATION_IMPOSSIBLE_VEL = (1 << 3),   /* velocity exceeds physical limit */
    PP_VIOLATION_FLOATING       = (1 << 4),   /* unsupported object not falling */
    PP_VIOLATION_DISCONTINUITY  = (1 << 5),   /* non-smooth trajectory */
} pp_violation_flags_t;

/* ============================================================================
 * Spatial State (decoded from RSSM latent)
 * ============================================================================ */

typedef struct {
    uint32_t            num_objects;
    uint32_t            capacity;
    wm_parietal_vec3_t* positions;
    wm_parietal_velocity_t* velocities;
    float*              masses;
    uint32_t*           object_ids;     /* maps to entity tracker IDs */
} pp_spatial_state_t;

/* ============================================================================
 * Config
 * ============================================================================ */

typedef struct {
    float       physics_weight;         /* initial blend weight */
    float       weight_adapt_rate;      /* how fast weight adapts to error ratio */
    float       energy_tolerance;       /* fractional energy change before flagging */
    float       teleport_threshold;     /* max position change per step (m) */
    float       velocity_limit;         /* max velocity magnitude (m/s) */
    bool        hard_interpenetration;  /* enforce no-interpenetration as hard constraint */
    bool        adaptive_weight;        /* auto-adapt physics_weight from error ratio */
} pp_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

typedef struct {
    uint64_t    total_predictions;
    uint64_t    total_corrections;
    uint64_t    violations_detected;
    uint64_t    interpenetrations_fixed;
    float       learned_error_ema;      /* EMA of learned model prediction error */
    float       physics_error_ema;      /* EMA of physics model prediction error */
    float       current_weight;         /* current adaptive blend weight */
    float       avg_correction_magnitude;
} pp_stats_t;

/* ============================================================================
 * Physics Prior
 * ============================================================================ */

typedef struct physics_prior {
    intuitive_physics_engine_t* physics;    /* NOT owned — shared with brain */
    entity_tracker_t*           tracker;    /* NOT owned */
    scene_graph_t*              scene;      /* NOT owned */

    pp_config_t                 config;
    pp_stats_t                  stats;

    /* Adaptive blend weight */
    float                       physics_weight;
    float                       learned_error_ema;
    float                       physics_error_ema;

    /* Scratch buffers (avoid per-call allocation) */
    pp_spatial_state_t          scratch_learned;
    pp_spatial_state_t          scratch_physics;

    bool                        initialized;
} physics_prior_t;

/* ============================================================================
 * API
 * ============================================================================ */

/** Create physics prior */
physics_prior_t* physics_prior_create(const pp_config_t* config);

/** Destroy physics prior */
void physics_prior_destroy(physics_prior_t* prior);

/** Connect to physics subsystems (non-owning pointers) */
void physics_prior_connect(physics_prior_t* prior,
                            intuitive_physics_engine_t* physics,
                            entity_tracker_t* tracker,
                            scene_graph_t* scene);

/**
 * Constrain a set of predicted spatial states using the physics prior.
 * Modifies states in-place. Returns violation flags (0 = no violations).
 */
uint32_t physics_prior_constrain(physics_prior_t* prior,
                                  pp_spatial_state_t* predicted,
                                  float dt);

/**
 * Compute physics-informed loss term for world model training.
 * Returns L_physics = weighted sum of violation penalties.
 */
float physics_prior_compute_loss(physics_prior_t* prior,
                                  const pp_spatial_state_t* predicted,
                                  const pp_spatial_state_t* observed);

/**
 * Update error statistics from a prediction/observation pair.
 * Call this after each training step to adapt the blend weight.
 */
void physics_prior_update_errors(physics_prior_t* prior,
                                  float learned_error,
                                  float physics_error);

/** Check if a single spatial state violates physics laws */
pp_violation_flags_t physics_prior_check_violations(const physics_prior_t* prior,
                                                     const pp_spatial_state_t* state,
                                                     float dt);

/** Get current stats */
pp_stats_t physics_prior_get_stats(const physics_prior_t* prior);

/** Default config */
pp_config_t physics_prior_default_config(void);

/** Allocate/free spatial state buffers */
int pp_spatial_state_alloc(pp_spatial_state_t* state, uint32_t capacity);
void pp_spatial_state_free(pp_spatial_state_t* state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PHYSICS_PRIOR_H */
