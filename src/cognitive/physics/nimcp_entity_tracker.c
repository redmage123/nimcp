/**
 * @file nimcp_entity_tracker.c
 * @brief Entity Tracker — persistent object identity and permanence
 *
 * WHAT: Tracks objects across time with persistent IDs, handles occlusion
 * WHY:  Object permanence is a core intuition (Spelke, 1990)
 * HOW:  Greedy nearest-neighbor assignment with confidence decay
 */

#include "cognitive/physics/nimcp_entity_tracker.h"
#include "cognitive/physics/nimcp_intuitive_physics.h"
#include "cognitive/world_model/nimcp_world_model_kg_events.h"  /* W8 */
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>
#include <float.h>

#define LOG_TAG "ENTITY_TRACKER"

/* ============================================================================
 * Helpers
 * ============================================================================ */

static inline float et_distance(wm_parietal_vec3_t a, wm_parietal_vec3_t b) {
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

/* Compute association score between observation and entity (higher = better match) */
static float compute_association_score(const entity_observation_t* obs,
                                        const entity_belief_t* ent) {
    /* Position-based distance (primary) */
    wm_parietal_vec3_t ent_pos = ent->visibility == ENTITY_VISIBLE
                                  ? ent->pos_mean
                                  : ent->predicted_position;
    float dist = et_distance(obs->position, ent_pos);

    /* Score = 1 / (1 + dist/scale) with radius-based normalization */
    float scale = ent->bounding_radius > 0 ? ent->bounding_radius * 3.0f : 1.0f;
    float pos_score = 1.0f / (1.0f + dist / scale);

    /* Size similarity bonus */
    float size_score = 1.0f;
    if (obs->bounding_radius > 0 && ent->bounding_radius > 0) {
        float ratio = obs->bounding_radius / ent->bounding_radius;
        if (ratio > 1.0f) ratio = 1.0f / ratio;
        size_score = ratio;  /* 1.0 = perfect match, <1.0 = mismatch */
    }

    /* Semantic label match bonus */
    float label_score = 1.0f;
    if (obs->semantic_label != 0 && ent->semantic_label != 0) {
        label_score = (obs->semantic_label == ent->semantic_label) ? 1.5f : 0.5f;
    }

    return pos_score * size_score * label_score * ent->existence_confidence;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

entity_tracker_config_t entity_tracker_default_config(void) {
    return (entity_tracker_config_t){
        .max_entities = ET_MAX_ENTITIES,
        .occlusion_decay_rate = ET_DEFAULT_OCCLUSION_DECAY,
        .permanence_threshold = ET_DEFAULT_PERMANENCE_THRESH,
        .association_threshold = ET_DEFAULT_ASSOC_THRESH,
        .merge_distance = 0.1f,
        .position_process_noise = 0.01f,
    };
}

entity_tracker_t* entity_tracker_create(const entity_tracker_config_t* config) {
    entity_tracker_config_t cfg = config ? *config : entity_tracker_default_config();

    entity_tracker_t* t = nimcp_calloc(1, sizeof(*t));
    if (!t) return NULL;

    t->config = cfg;
    t->capacity = cfg.max_entities;
    t->entities = nimcp_calloc(t->capacity, sizeof(entity_belief_t));
    t->assoc_scores = nimcp_calloc(ET_MAX_OBSERVATIONS * t->capacity, sizeof(float));

    if (!t->entities || !t->assoc_scores) {
        entity_tracker_destroy(t);
        return NULL;
    }

    t->next_id = 1;
    t->initialized = true;

    LOG_INFO(LOG_TAG, "Entity tracker created: max_entities=%u, decay=%.3f, "
             "permanence_thresh=%.2f",
             cfg.max_entities, cfg.occlusion_decay_rate, cfg.permanence_threshold);
    return t;
}

void entity_tracker_destroy(entity_tracker_t* tracker) {
    if (!tracker) return;
    nimcp_free(tracker->entities);
    nimcp_free(tracker->assoc_scores);
    nimcp_free(tracker);
}

int entity_tracker_update(entity_tracker_t* tracker,
                           const entity_observation_t* observations,
                           uint32_t num_obs,
                           double timestamp) {
    if (!tracker || !tracker->initialized) return -1;
    if (!observations && num_obs > 0) return -1;
    if (num_obs > ET_MAX_OBSERVATIONS) num_obs = ET_MAX_OBSERVATIONS;

    tracker->total_observations += num_obs;

    /* Compute association scores: [num_obs x num_entities] */
    bool obs_matched[ET_MAX_OBSERVATIONS] = {0};
    bool ent_matched[ET_MAX_ENTITIES] = {0};

    /* Greedy nearest-neighbor assignment (simple and fast for small N) */
    for (uint32_t round = 0; round < num_obs; round++) {
        float best_score = 0;
        uint32_t best_obs = UINT32_MAX, best_ent = UINT32_MAX;

        for (uint32_t o = 0; o < num_obs; o++) {
            if (obs_matched[o]) continue;
            for (uint32_t e = 0; e < tracker->capacity; e++) {
                if (!tracker->entities[e].active || ent_matched[e]) continue;
                float score = compute_association_score(&observations[o], &tracker->entities[e]);
                if (score > best_score) {
                    best_score = score;
                    best_obs = o;
                    best_ent = e;
                }
            }
        }

        if (best_score < tracker->config.association_threshold || best_obs == UINT32_MAX)
            break;

        /* Match found: update entity with observation */
        obs_matched[best_obs] = true;
        ent_matched[best_ent] = true;

        entity_belief_t* ent = &tracker->entities[best_ent];
        const entity_observation_t* obs = &observations[best_obs];

        /* Kalman-like update: blend observation with prediction */
        float alpha = 0.7f;  /* observation weight */
        ent->pos_mean.x = alpha * obs->position.x + (1 - alpha) * ent->pos_mean.x;
        ent->pos_mean.y = alpha * obs->position.y + (1 - alpha) * ent->pos_mean.y;
        ent->pos_mean.z = alpha * obs->position.z + (1 - alpha) * ent->pos_mean.z;
        ent->vel_mean = obs->velocity;
        ent->bounding_radius = obs->bounding_radius;

        /* Reduce position variance on observation */
        ent->pos_variance.x *= (1 - alpha);
        ent->pos_variance.y *= (1 - alpha);
        ent->pos_variance.z *= (1 - alpha);

        if (obs->mass_hint > 0) {
            ent->mass_estimate = 0.8f * obs->mass_hint + 0.2f * ent->mass_estimate;
            ent->mass_confidence = fminf(1.0f, ent->mass_confidence + 0.1f);
        }
        if (obs->semantic_label != 0) {
            ent->semantic_label = obs->semantic_label;
        }

        /* Visibility update */
        if (ent->visibility != ENTITY_VISIBLE) {
            tracker->total_reappearances++;
        }
        ent->visibility = ENTITY_VISIBLE;
        ent->existence_confidence = fminf(1.0f, ent->existence_confidence + 0.2f);
        ent->identity_confidence = fminf(1.0f, best_score);
        ent->last_observed_time = timestamp;
        ent->observation_count++;
        ent->prediction_age = 0;
    }

    /* Create new entities for unmatched observations */
    for (uint32_t o = 0; o < num_obs; o++) {
        if (obs_matched[o]) continue;

        /* Find empty slot */
        uint32_t slot = UINT32_MAX;
        for (uint32_t e = 0; e < tracker->capacity; e++) {
            if (!tracker->entities[e].active) { slot = e; break; }
        }
        if (slot == UINT32_MAX) {
            LOG_WARN(LOG_TAG, "Entity tracker full (%u entities)", tracker->capacity);
            break;
        }

        entity_belief_t* ent = &tracker->entities[slot];
        memset(ent, 0, sizeof(*ent));
        ent->entity_id = tracker->next_id++;
        ent->active = true;
        ent->pos_mean = observations[o].position;
        ent->vel_mean = observations[o].velocity;
        ent->pos_variance = (wm_parietal_vec3_t){
            ET_POSITION_VARIANCE_INIT, ET_POSITION_VARIANCE_INIT, ET_POSITION_VARIANCE_INIT
        };
        ent->bounding_radius = observations[o].bounding_radius;
        ent->mass_estimate = observations[o].mass_hint;
        ent->mass_confidence = observations[o].mass_hint > 0 ? 0.5f : 0.0f;
        ent->visibility = ENTITY_VISIBLE;
        ent->existence_confidence = observations[o].confidence;
        ent->identity_confidence = 1.0f;
        ent->last_observed_time = timestamp;
        ent->first_observed_time = timestamp;
        ent->observation_count = 1;
        ent->semantic_label = observations[o].semantic_label;
        ent->predicted_position = observations[o].position;
        ent->predicted_velocity = observations[o].velocity;

        if (slot >= tracker->num_entities) tracker->num_entities = slot + 1;
        tracker->total_new_entities++;

        /* W8: emit spawn event for the new entity. */
        {
            struct brain_struct* _kg_brain =
                world_model_kg_events_get_registered_brain();
            if (_kg_brain) {
                world_model_kg_emit_entity_event(_kg_brain,
                    ent->entity_id, 0 /* spawn */,
                    ent->existence_confidence);
            }
        }
    }

    /* Mark unmatched visible entities as occluded */
    for (uint32_t e = 0; e < tracker->capacity; e++) {
        if (!tracker->entities[e].active) continue;
        if (ent_matched[e]) continue;
        if (tracker->entities[e].visibility == ENTITY_VISIBLE) {
            tracker->entities[e].visibility = ENTITY_OCCLUDED;
            tracker->total_occlusions++;

            /* W8: emit occlude event + read-back world-sim partner check. */
            struct brain_struct* _kg_brain =
                world_model_kg_events_get_registered_brain();
            if (_kg_brain) {
                world_model_kg_emit_entity_event(_kg_brain,
                    tracker->entities[e].entity_id, 1 /* occlude */,
                    tracker->entities[e].existence_confidence);
                (void)world_model_kg_has_partner(_kg_brain,
                    "cog_physics_world_simulator");
            }
        }
    }

    return 0;
}

int entity_tracker_predict_hidden(entity_tracker_t* tracker,
                                   intuitive_physics_engine_t* physics,
                                   float dt) {
    if (!tracker || !physics) return -1;

    for (uint32_t e = 0; e < tracker->capacity; e++) {
        entity_belief_t* ent = &tracker->entities[e];
        if (!ent->active) continue;
        if (ent->visibility == ENTITY_VISIBLE) continue;
        if (ent->visibility == ENTITY_LOST) continue;

        /* Simple physics prediction: apply gravity */
        wm_parietal_vec3_t g = {0, -9.81f * dt, 0};  /* gravity impulse */
        ent->predicted_velocity.vy += g.y;

        ent->predicted_position.x += ent->predicted_velocity.vx * dt;
        ent->predicted_position.y += ent->predicted_velocity.vy * dt;
        ent->predicted_position.z += ent->predicted_velocity.vz * dt;

        /* Ground clamp */
        float ground_y = ent->bounding_radius;
        if (ent->predicted_position.y < ground_y) {
            ent->predicted_position.y = ground_y;
            ent->predicted_velocity.vy = 0;
        }

        /* Grow uncertainty */
        float noise = tracker->config.position_process_noise * dt;
        ent->pos_variance.x += noise;
        ent->pos_variance.y += noise;
        ent->pos_variance.z += noise;

        ent->prediction_age += dt;
    }
    return 0;
}

void entity_tracker_decay(entity_tracker_t* tracker, float dt) {
    if (!tracker) return;

    for (uint32_t e = 0; e < tracker->capacity; e++) {
        entity_belief_t* ent = &tracker->entities[e];
        if (!ent->active) continue;
        if (ent->visibility == ENTITY_VISIBLE) continue;

        /* Decay confidence for hidden entities */
        ent->existence_confidence -= tracker->config.occlusion_decay_rate * dt;

        if (ent->existence_confidence < tracker->config.permanence_threshold) {
            ent->visibility = ENTITY_LOST;
            /* Don't immediately deactivate — keep for potential reappearance */
            if (ent->existence_confidence < 0.01f) {
                ent->active = false;
                tracker->total_lost_entities++;
            }
        }
    }
}

const entity_belief_t* entity_tracker_get(const entity_tracker_t* tracker, uint32_t id) {
    if (!tracker) return NULL;
    for (uint32_t e = 0; e < tracker->capacity; e++) {
        if (tracker->entities[e].active && tracker->entities[e].entity_id == id)
            return &tracker->entities[e];
    }
    return NULL;
}

uint32_t entity_tracker_count_visible(const entity_tracker_t* tracker) {
    if (!tracker) return 0;
    uint32_t count = 0;
    for (uint32_t e = 0; e < tracker->capacity; e++) {
        if (tracker->entities[e].active && tracker->entities[e].visibility == ENTITY_VISIBLE)
            count++;
    }
    return count;
}

uint32_t entity_tracker_count_occluded(const entity_tracker_t* tracker) {
    if (!tracker) return 0;
    uint32_t count = 0;
    for (uint32_t e = 0; e < tracker->capacity; e++) {
        if (tracker->entities[e].active && tracker->entities[e].visibility == ENTITY_OCCLUDED)
            count++;
    }
    return count;
}

uint32_t entity_tracker_count_total(const entity_tracker_t* tracker) {
    if (!tracker) return 0;
    uint32_t count = 0;
    for (uint32_t e = 0; e < tracker->capacity; e++) {
        if (tracker->entities[e].active) count++;
    }
    return count;
}

bool entity_tracker_object_permanent(const entity_tracker_t* tracker, uint32_t id) {
    const entity_belief_t* ent = entity_tracker_get(tracker, id);
    if (!ent) return false;
    /* An object is "permanent" if we believe it exists even when hidden */
    return ent->existence_confidence > tracker->config.permanence_threshold;
}

uint32_t entity_tracker_get_active_ids(const entity_tracker_t* tracker,
                                        uint32_t* buf, uint32_t buf_size) {
    if (!tracker || !buf) return 0;
    uint32_t count = 0;
    for (uint32_t e = 0; e < tracker->capacity && count < buf_size; e++) {
        if (tracker->entities[e].active)
            buf[count++] = tracker->entities[e].entity_id;
    }
    return count;
}
