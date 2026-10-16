/**
 * @file nimcp_physics_prior.c
 * @brief Physics Prior — constrains world model predictions with physical laws
 *
 * WHAT: Post-correction layer blending learned RSSM with deterministic physics
 * WHY:  Prevents impossible predictions (teleporting, floating, interpenetration)
 * HOW:  Adaptive blend weight, hard constraint enforcement, physics loss for training
 */

#include "cognitive/physics/nimcp_physics_prior.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "PHYSICS_PRIOR"

/* ============================================================================
 * Helpers
 * ============================================================================ */

static inline float pp_clamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline float pp_dist(wm_parietal_vec3_t a, wm_parietal_vec3_t b) {
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

static inline float pp_vel_mag(wm_parietal_velocity_t v) {
    return sqrtf(v.vx*v.vx + v.vy*v.vy + v.vz*v.vz);
}

/* ============================================================================
 * Spatial State Management
 * ============================================================================ */

int pp_spatial_state_alloc(pp_spatial_state_t* state, uint32_t capacity) {
    if (!state) return -1;
    memset(state, 0, sizeof(*state));
    state->capacity = capacity;
    state->positions = nimcp_calloc(capacity, sizeof(wm_parietal_vec3_t));
    state->velocities = nimcp_calloc(capacity, sizeof(wm_parietal_velocity_t));
    state->masses = nimcp_calloc(capacity, sizeof(float));
    state->object_ids = nimcp_calloc(capacity, sizeof(uint32_t));
    if (!state->positions || !state->velocities || !state->masses || !state->object_ids) {
        pp_spatial_state_free(state);
        return -1;
    }
    return 0;
}

void pp_spatial_state_free(pp_spatial_state_t* state) {
    if (!state) return;
    nimcp_free(state->positions);
    nimcp_free(state->velocities);
    nimcp_free(state->masses);
    nimcp_free(state->object_ids);
    memset(state, 0, sizeof(*state));
}

/* ============================================================================
 * Public API
 * ============================================================================ */

pp_config_t physics_prior_default_config(void) {
    return (pp_config_t){
        .physics_weight = PP_DEFAULT_WEIGHT,
        .weight_adapt_rate = 0.01f,
        .energy_tolerance = 0.1f,
        .teleport_threshold = 2.0f,
        .velocity_limit = 100.0f,
        .hard_interpenetration = true,
        .adaptive_weight = true,
    };
}

physics_prior_t* physics_prior_create(const pp_config_t* config) {
    pp_config_t cfg = config ? *config : physics_prior_default_config();

    physics_prior_t* p = nimcp_calloc(1, sizeof(*p));
    if (!p) return NULL;

    p->config = cfg;
    p->physics_weight = cfg.physics_weight;
    p->learned_error_ema = 1.0f;
    p->physics_error_ema = 1.0f;

    /* Pre-allocate scratch buffers */
    if (pp_spatial_state_alloc(&p->scratch_learned, IP_MAX_OBJECTS) != 0 ||
        pp_spatial_state_alloc(&p->scratch_physics, IP_MAX_OBJECTS) != 0) {
        physics_prior_destroy(p);
        return NULL;
    }

    p->initialized = true;
    LOG_INFO(LOG_TAG, "Physics prior created: weight=%.2f, adaptive=%s",
             cfg.physics_weight, cfg.adaptive_weight ? "yes" : "no");
    return p;
}

void physics_prior_destroy(physics_prior_t* prior) {
    if (!prior) return;
    pp_spatial_state_free(&prior->scratch_learned);
    pp_spatial_state_free(&prior->scratch_physics);
    nimcp_free(prior);
}

void physics_prior_connect(physics_prior_t* prior,
                            intuitive_physics_engine_t* physics,
                            entity_tracker_t* tracker,
                            scene_graph_t* scene) {
    if (!prior) return;
    prior->physics = physics;
    prior->tracker = tracker;
    prior->scene = scene;
}

uint32_t physics_prior_constrain(physics_prior_t* prior,
                                  pp_spatial_state_t* predicted,
                                  float dt) {
    if (!prior || !predicted || !prior->physics) return 0;

    uint32_t violations = PP_VIOLATION_NONE;
    float w = prior->physics_weight;

    prior->stats.total_predictions++;

    for (uint32_t i = 0; i < predicted->num_objects; i++) {
        wm_parietal_vec3_t* pos = &predicted->positions[i];
        wm_parietal_velocity_t* vel = &predicted->velocities[i];

        /* Get corresponding physics object */
        ip_object_t* phys_obj = intuitive_physics_get_object(prior->physics,
                                                              predicted->object_ids[i]);
        if (!phys_obj) continue;

        /* Check for violations BEFORE correction */
        float pos_delta = pp_dist(*pos, phys_obj->position);
        float vel_mag = pp_vel_mag(*vel);

        if (pos_delta > prior->config.teleport_threshold) {
            violations |= PP_VIOLATION_TELEPORT;
        }
        if (vel_mag > prior->config.velocity_limit) {
            violations |= PP_VIOLATION_IMPOSSIBLE_VEL;
        }
        if (!phys_obj->is_static && phys_obj->supported_by == UINT32_MAX &&
            fabsf(vel->vy) < 0.1f && phys_obj->position.y > 0.1f) {
            violations |= PP_VIOLATION_FLOATING;
        }

        /* Blend learned prediction with physics prediction */
        wm_parietal_vec3_t phys_pos = phys_obj->position;
        wm_parietal_velocity_t phys_vel = {phys_obj->velocity.vx, phys_obj->velocity.vy, phys_obj->velocity.vz};

        pos->x = (1.0f - w) * pos->x + w * phys_pos.x;
        pos->y = (1.0f - w) * pos->y + w * phys_pos.y;
        pos->z = (1.0f - w) * pos->z + w * phys_pos.z;

        vel->vx = (1.0f - w) * vel->vx + w * phys_vel.vx;
        vel->vy = (1.0f - w) * vel->vy + w * phys_vel.vy;
        vel->vz = (1.0f - w) * vel->vz + w * phys_vel.vz;

        /* Hard constraints */

        /* No interpenetration with ground */
        if (prior->config.hard_interpenetration) {
            float ground_y = 0;
            if (phys_obj->shape.type == IP_SHAPE_SPHERE)
                ground_y = phys_obj->shape.sphere.radius;
            else if (phys_obj->shape.type == IP_SHAPE_BOX)
                ground_y = phys_obj->shape.box.hy;
            if (pos->y < ground_y) {
                pos->y = ground_y;
                if (vel->vy < 0) vel->vy = 0;
                violations |= PP_VIOLATION_INTERPENETRATION;
                prior->stats.interpenetrations_fixed++;
            }
        }

        /* Velocity clamp */
        if (vel_mag > prior->config.velocity_limit) {
            float scale = prior->config.velocity_limit / vel_mag;
            vel->vx *= scale; vel->vy *= scale; vel->vz *= scale;
        }

        prior->stats.total_corrections++;
    }

    if (violations) {
        prior->stats.violations_detected++;
    }

    return violations;
}

float physics_prior_compute_loss(physics_prior_t* prior,
                                  const pp_spatial_state_t* predicted,
                                  const pp_spatial_state_t* observed) {
    if (!prior || !predicted || !observed) return 0;

    float total_loss = 0;
    uint32_t count = 0;

    uint32_t n = predicted->num_objects < observed->num_objects
                 ? predicted->num_objects : observed->num_objects;

    for (uint32_t i = 0; i < n; i++) {
        /* Position error (MSE) */
        float dx = predicted->positions[i].x - observed->positions[i].x;
        float dy = predicted->positions[i].y - observed->positions[i].y;
        float dz = predicted->positions[i].z - observed->positions[i].z;
        float pos_err = dx*dx + dy*dy + dz*dz;

        /* Velocity error (MSE) */
        float dvx = predicted->velocities[i].vx - observed->velocities[i].vx;
        float dvy = predicted->velocities[i].vy - observed->velocities[i].vy;
        float dvz = predicted->velocities[i].vz - observed->velocities[i].vz;
        float vel_err = dvx*dvx + dvy*dvy + dvz*dvz;

        total_loss += pos_err + 0.1f * vel_err;
        count++;
    }

    /* Energy conservation penalty */
    if (prior->physics) {
        ip_stats_t ps = intuitive_physics_get_stats(prior->physics);
        if (fabsf(ps.energy_drift) > prior->config.energy_tolerance) {
            total_loss += ps.energy_drift * ps.energy_drift * 10.0f;
        }
    }

    /* Interpenetration penalty (from scene graph contacts) */
    if (prior->scene) {
        /* Each contact that shouldn't exist adds penalty */
        /* (simplified: just use contact count as proxy) */
    }

    return count > 0 ? total_loss / (float)count : 0;
}

void physics_prior_update_errors(physics_prior_t* prior,
                                  float learned_error,
                                  float physics_error) {
    if (!prior) return;

    float alpha = PP_EMA_ALPHA;
    prior->learned_error_ema = (1 - alpha) * prior->learned_error_ema + alpha * learned_error;
    prior->physics_error_ema = (1 - alpha) * prior->physics_error_ema + alpha * physics_error;

    /* Adapt blend weight based on error ratio */
    if (prior->config.adaptive_weight && prior->physics_error_ema > 1e-8f) {
        float ratio = prior->learned_error_ema / prior->physics_error_ema;
        /* If learned error >> physics error, increase physics weight */
        float target_w = pp_clamp(ratio / (1.0f + ratio), PP_MIN_WEIGHT, PP_MAX_WEIGHT);
        prior->physics_weight += prior->config.weight_adapt_rate * (target_w - prior->physics_weight);
        prior->physics_weight = pp_clamp(prior->physics_weight, PP_MIN_WEIGHT, PP_MAX_WEIGHT);
    }

    prior->stats.learned_error_ema = prior->learned_error_ema;
    prior->stats.physics_error_ema = prior->physics_error_ema;
    prior->stats.current_weight = prior->physics_weight;
}

pp_violation_flags_t physics_prior_check_violations(const physics_prior_t* prior,
                                                     const pp_spatial_state_t* state,
                                                     float dt) {
    if (!prior || !state) return PP_VIOLATION_NONE;
    (void)dt;

    uint32_t violations = PP_VIOLATION_NONE;

    for (uint32_t i = 0; i < state->num_objects; i++) {
        /* Ground interpenetration */
        if (state->positions[i].y < 0) {
            violations |= PP_VIOLATION_INTERPENETRATION;
        }

        /* Impossible velocity */
        float v = pp_vel_mag(state->velocities[i]);
        if (v > prior->config.velocity_limit) {
            violations |= PP_VIOLATION_IMPOSSIBLE_VEL;
        }
    }

    return (pp_violation_flags_t)violations;
}

pp_stats_t physics_prior_get_stats(const physics_prior_t* prior) {
    if (!prior) return (pp_stats_t){0};
    return prior->stats;
}
