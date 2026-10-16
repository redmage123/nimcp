/**
 * @file nimcp_affordance_processing.c
 * @brief Implementation of Affordance Detection and Processing
 *
 * This implementation provides embodied cognition capabilities for detecting
 * and processing action possibilities (affordances) in the environment.
 *
 * Biological basis:
 * - Dorsal visual stream processes object properties for action
 * - Premotor cortex encodes object-action associations
 * - Competition resolved through lateral inhibition
 */

#include "embodiment/nimcp_affordance_processing.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#define LOG_MODULE "affordance_processing"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/thread/nimcp_thread_rand.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(affordance_processing)

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Internal affordance context structure
 */
struct nimcp_affordance_context {
    nimcp_affordance_config_t config;  /**< Configuration */
    bool initialized;                   /**< Initialization flag */

    /* Object tracking */
    nimcp_object_properties_t objects[NIMCP_AFFORDANCE_MAX_OBJECTS];
    bool object_active[NIMCP_AFFORDANCE_MAX_OBJECTS];
    uint32_t num_objects;

    /* Affordance storage */
    nimcp_affordance_t affordances[NIMCP_AFFORDANCE_MAX_OBJECTS * NIMCP_AFFORDANCE_MAX_PER_OBJECT];
    uint32_t num_affordances;
    uint32_t next_affordance_id;

    /* Goal state */
    nimcp_affordance_goal_t current_goal;
    bool has_goal;

    /* History */
    nimcp_affordance_history_t history[NIMCP_AFFORDANCE_HISTORY_SIZE];
    uint32_t history_index;
    uint32_t history_count;

    /* Statistics */
    nimcp_affordance_stats_t stats;

    /* Timing */
    uint64_t last_update_time;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in nanoseconds
 */
static inline uint64_t get_timestamp_ns(void) {
    return nimcp_time_get_us() * 1000ULL;
}

/**
 * @brief Calculate distance between two 3D points
 */
static double calculate_distance(const double* p1, const double* p2) {
    double dx = p2[0] - p1[0];
    double dy = p2[1] - p1[1];
    double dz = p2[2] - p1[2];
    return sqrt(dx * dx + dy * dy + dz * dz);
}

/**
 * @brief Normalize a 3D vector
 */
static void normalize_vector(double* v) {
    double mag = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (mag > 1e-9) {
        v[0] /= mag;
        v[1] /= mag;
        v[2] /= mag;
    }
}

/**
 * @brief Find object by ID
 */
static nimcp_object_properties_t* find_object(
    nimcp_affordance_context_t* ctx,
    uint32_t object_id
) {
    for (uint32_t i = 0; i < NIMCP_AFFORDANCE_MAX_OBJECTS; i++) {
        if (ctx->object_active[i] && ctx->objects[i].object_id == object_id) {
            return &ctx->objects[i];
        }
    }
    return NULL;
}

/**
 * @brief Find affordance by ID
 */
static nimcp_affordance_t* find_affordance(
    nimcp_affordance_context_t* ctx,
    uint32_t affordance_id
) {
    for (uint32_t i = 0; i < ctx->num_affordances; i++) {
        if (ctx->affordances[i].affordance_id == affordance_id) {
            return &ctx->affordances[i];
        }
    }
    return NULL;
}

/**
 * @brief Add a new affordance to storage
 */
static nimcp_affordance_t* add_affordance(nimcp_affordance_context_t* ctx) {
    if (ctx->num_affordances >= NIMCP_AFFORDANCE_MAX_OBJECTS * NIMCP_AFFORDANCE_MAX_PER_OBJECT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "add_affordance: capacity exceeded");
        return NULL;
    }

    nimcp_affordance_t* aff = &ctx->affordances[ctx->num_affordances];
    memset(aff, 0, sizeof(*aff));
    aff->affordance_id = ctx->next_affordance_id++;
    ctx->num_affordances++;
    ctx->stats.total_detections++;

    return aff;
}

/**
 * @brief Calculate reachability based on distance and arm length
 */
static double calculate_reachability(double distance, double arm_length) {
    if (distance <= arm_length * 0.5) {
        return 1.0;  /* Easily reachable */
    } else if (distance <= arm_length) {
        return 1.0 - (distance - arm_length * 0.5) / (arm_length * 0.5);
    }
    return 0.0;  /* Out of reach */
}

/**
 * @brief Calculate manipulability based on object properties
 */
static double calculate_manipulability(const nimcp_object_properties_t* obj) {
    double score = 0.5;  /* Base score */

    /* Size factor */
    double max_dim = fmax(obj->dimensions[0], fmax(obj->dimensions[1], obj->dimensions[2]));
    if (max_dim < 0.05) {
        score *= 0.7;  /* Too small */
    } else if (max_dim > 0.5) {
        score *= 0.6;  /* Too large */
    } else {
        score *= 1.0;  /* Good size */
    }

    /* Mass factor */
    if (obj->estimated_mass < 0.1) {
        score *= 1.0;  /* Light */
    } else if (obj->estimated_mass < 1.0) {
        score *= 0.9;
    } else if (obj->estimated_mass < 5.0) {
        score *= 0.7;
    } else {
        score *= 0.4;  /* Heavy */
    }

    /* Handle bonus */
    if (obj->has_handle) {
        score *= 1.3;
    }

    /* Graspability */
    if (obj->is_graspable) {
        score *= 1.2;
    }

    return fmin(1.0, score);
}

/**
 * @brief Detect affordances for object based on category
 */
static void detect_affordances_for_object(
    nimcp_affordance_context_t* ctx,
    const nimcp_object_properties_t* obj
) {
    const double arm_length = 0.7;  /* Approximate arm length in meters */
    double reachability = calculate_reachability(obj->distance, arm_length);
    double manipulability = calculate_manipulability(obj);
    uint64_t now = get_timestamp_ns();

    /* Category-based affordance detection */
    switch (obj->category) {
        case NIMCP_OBJECT_CATEGORY_TOOL:
            if (obj->is_graspable) {
                nimcp_affordance_t* aff = add_affordance(ctx);
                if (aff) {
                    aff->object_id = obj->object_id;
                    aff->action = NIMCP_ACTION_GRASP;
                    aff->state = NIMCP_AFFORDANCE_STATE_DETECTED;
                    aff->saliency = 0.8 * manipulability;
                    aff->reachability = reachability;
                    aff->manipulability = manipulability;
                    aff->detection_time = now;
                    aff->decay_rate = ctx->config.decay_rate;

                    /* Set approach vector */
                    aff->approach_vector[0] = -obj->position[0];
                    aff->approach_vector[1] = -obj->position[1];
                    aff->approach_vector[2] = -obj->position[2];
                    normalize_vector(aff->approach_vector);
                }
            }
            break;

        case NIMCP_OBJECT_CATEGORY_CONTAINER:
            /* Grasp affordance */
            if (obj->is_graspable && obj->is_movable) {
                nimcp_affordance_t* aff = add_affordance(ctx);
                if (aff) {
                    aff->object_id = obj->object_id;
                    aff->action = NIMCP_ACTION_GRASP;
                    aff->state = NIMCP_AFFORDANCE_STATE_DETECTED;
                    aff->saliency = 0.7 * manipulability;
                    aff->reachability = reachability;
                    aff->manipulability = manipulability;
                    aff->detection_time = now;
                    aff->decay_rate = ctx->config.decay_rate;
                }
            }
            /* Pour affordance */
            {
                nimcp_affordance_t* aff = add_affordance(ctx);
                if (aff) {
                    aff->object_id = obj->object_id;
                    aff->action = NIMCP_ACTION_POUR;
                    aff->state = NIMCP_AFFORDANCE_STATE_DETECTED;
                    aff->saliency = 0.6;
                    aff->reachability = reachability;
                    aff->manipulability = manipulability * 0.8;
                    aff->detection_time = now;
                    aff->decay_rate = ctx->config.decay_rate;
                }
            }
            /* Contain affordance */
            if (obj->is_container) {
                nimcp_affordance_t* aff = add_affordance(ctx);
                if (aff) {
                    aff->object_id = obj->object_id;
                    aff->action = NIMCP_ACTION_CONTAIN;
                    aff->state = NIMCP_AFFORDANCE_STATE_DETECTED;
                    aff->saliency = 0.5;
                    aff->reachability = reachability;
                    aff->manipulability = 1.0;  /* Container doesn't need manipulation */
                    aff->detection_time = now;
                    aff->decay_rate = ctx->config.decay_rate;
                }
            }
            break;

        case NIMCP_OBJECT_CATEGORY_SURFACE:
            /* Place affordance */
            {
                nimcp_affordance_t* aff = add_affordance(ctx);
                if (aff) {
                    aff->object_id = obj->object_id;
                    aff->action = NIMCP_ACTION_PLACE;
                    aff->state = NIMCP_AFFORDANCE_STATE_DETECTED;
                    aff->saliency = 0.6;
                    aff->reachability = reachability;
                    aff->manipulability = 1.0;
                    aff->detection_time = now;
                    aff->decay_rate = ctx->config.decay_rate;
                }
            }
            /* Support affordance */
            {
                nimcp_affordance_t* aff = add_affordance(ctx);
                if (aff) {
                    aff->object_id = obj->object_id;
                    aff->action = NIMCP_ACTION_SUPPORT;
                    aff->state = NIMCP_AFFORDANCE_STATE_DETECTED;
                    aff->saliency = 0.5;
                    aff->reachability = 1.0;  /* Always "reachable" for support */
                    aff->manipulability = 1.0;
                    aff->detection_time = now;
                    aff->decay_rate = ctx->config.decay_rate;
                }
            }
            break;

        case NIMCP_OBJECT_CATEGORY_BUTTON:
            {
                nimcp_affordance_t* aff = add_affordance(ctx);
                if (aff) {
                    aff->object_id = obj->object_id;
                    aff->action = NIMCP_ACTION_PRESS;
                    aff->state = NIMCP_AFFORDANCE_STATE_DETECTED;
                    aff->saliency = 0.9;  /* Buttons are very salient */
                    aff->reachability = reachability;
                    aff->manipulability = 1.0;  /* Easy to press */
                    aff->effort_estimate = 0.1;  /* Low effort */
                    aff->detection_time = now;
                    aff->decay_rate = ctx->config.decay_rate;
                }
            }
            break;

        case NIMCP_OBJECT_CATEGORY_HANDLE:
            /* Grasp handle */
            {
                nimcp_affordance_t* aff = add_affordance(ctx);
                if (aff) {
                    aff->object_id = obj->object_id;
                    aff->action = NIMCP_ACTION_GRASP;
                    aff->state = NIMCP_AFFORDANCE_STATE_DETECTED;
                    aff->saliency = 0.85;
                    aff->reachability = reachability;
                    aff->manipulability = 0.9;
                    aff->detection_time = now;
                    aff->decay_rate = ctx->config.decay_rate;
                }
            }
            /* Pull handle */
            {
                nimcp_affordance_t* aff = add_affordance(ctx);
                if (aff) {
                    aff->object_id = obj->object_id;
                    aff->action = NIMCP_ACTION_PULL;
                    aff->state = NIMCP_AFFORDANCE_STATE_DETECTED;
                    aff->saliency = 0.8;
                    aff->reachability = reachability;
                    aff->manipulability = 0.85;
                    aff->detection_time = now;
                    aff->decay_rate = ctx->config.decay_rate;
                }
            }
            break;

        case NIMCP_OBJECT_CATEGORY_OPENING:
            /* Open */
            {
                nimcp_affordance_t* aff = add_affordance(ctx);
                if (aff) {
                    aff->object_id = obj->object_id;
                    aff->action = NIMCP_ACTION_OPEN;
                    aff->state = NIMCP_AFFORDANCE_STATE_DETECTED;
                    aff->saliency = 0.75;
                    aff->reachability = reachability;
                    aff->manipulability = manipulability;
                    aff->detection_time = now;
                    aff->decay_rate = ctx->config.decay_rate;
                }
            }
            /* Close */
            {
                nimcp_affordance_t* aff = add_affordance(ctx);
                if (aff) {
                    aff->object_id = obj->object_id;
                    aff->action = NIMCP_ACTION_CLOSE;
                    aff->state = NIMCP_AFFORDANCE_STATE_DETECTED;
                    aff->saliency = 0.7;
                    aff->reachability = reachability;
                    aff->manipulability = manipulability;
                    aff->detection_time = now;
                    aff->decay_rate = ctx->config.decay_rate;
                }
            }
            /* Traverse (if door-sized opening) */
            if (obj->dimensions[0] > 0.6 && obj->dimensions[1] > 1.5) {
                nimcp_affordance_t* aff = add_affordance(ctx);
                if (aff) {
                    aff->object_id = obj->object_id;
                    aff->action = NIMCP_ACTION_TRAVERSE;
                    aff->state = NIMCP_AFFORDANCE_STATE_DETECTED;
                    aff->saliency = 0.6;
                    aff->reachability = 1.0;
                    aff->manipulability = 1.0;
                    aff->detection_time = now;
                    aff->decay_rate = ctx->config.decay_rate;
                }
            }
            break;

        case NIMCP_OBJECT_CATEGORY_MANIPULANDUM:
            /* General manipulation */
            if (obj->is_graspable) {
                nimcp_affordance_t* aff = add_affordance(ctx);
                if (aff) {
                    aff->object_id = obj->object_id;
                    aff->action = NIMCP_ACTION_MANIPULATE;
                    aff->state = NIMCP_AFFORDANCE_STATE_DETECTED;
                    aff->saliency = 0.7 * manipulability;
                    aff->reachability = reachability;
                    aff->manipulability = manipulability;
                    aff->detection_time = now;
                    aff->decay_rate = ctx->config.decay_rate;
                }
            }
            /* Push */
            if (obj->is_movable) {
                nimcp_affordance_t* aff = add_affordance(ctx);
                if (aff) {
                    aff->object_id = obj->object_id;
                    aff->action = NIMCP_ACTION_PUSH;
                    aff->state = NIMCP_AFFORDANCE_STATE_DETECTED;
                    aff->saliency = 0.5;
                    aff->reachability = reachability;
                    aff->manipulability = manipulability * 0.9;
                    aff->detection_time = now;
                    aff->decay_rate = ctx->config.decay_rate;
                }
            }
            break;

        case NIMCP_OBJECT_CATEGORY_SUPPORT:
            /* Sit affordance */
            if (obj->dimensions[1] > 0.3 && obj->dimensions[1] < 0.7) {
                nimcp_affordance_t* aff = add_affordance(ctx);
                if (aff) {
                    aff->object_id = obj->object_id;
                    aff->action = NIMCP_ACTION_SIT;
                    aff->state = NIMCP_AFFORDANCE_STATE_DETECTED;
                    aff->saliency = 0.65;
                    aff->reachability = 1.0;
                    aff->manipulability = 1.0;
                    aff->detection_time = now;
                    aff->decay_rate = ctx->config.decay_rate;
                }
            }
            break;

        case NIMCP_OBJECT_CATEGORY_PROJECTILE:
            /* Grasp */
            if (obj->is_graspable) {
                nimcp_affordance_t* aff = add_affordance(ctx);
                if (aff) {
                    aff->object_id = obj->object_id;
                    aff->action = NIMCP_ACTION_GRASP;
                    aff->state = NIMCP_AFFORDANCE_STATE_DETECTED;
                    aff->saliency = 0.75;
                    aff->reachability = reachability;
                    aff->manipulability = manipulability;
                    aff->detection_time = now;
                    aff->decay_rate = ctx->config.decay_rate;
                }
            }
            /* Throw */
            if (obj->is_graspable && obj->estimated_mass < 2.0) {
                nimcp_affordance_t* aff = add_affordance(ctx);
                if (aff) {
                    aff->object_id = obj->object_id;
                    aff->action = NIMCP_ACTION_THROW;
                    aff->state = NIMCP_AFFORDANCE_STATE_DETECTED;
                    aff->saliency = 0.6;
                    aff->reachability = reachability;
                    aff->manipulability = manipulability;
                    aff->effort_estimate = obj->estimated_mass / 2.0;
                    aff->detection_time = now;
                    aff->decay_rate = ctx->config.decay_rate;
                }
            }
            break;

        default:
            /* Unknown category - detect basic affordances */
            if (obj->is_graspable && obj->is_movable) {
                nimcp_affordance_t* aff = add_affordance(ctx);
                if (aff) {
                    aff->object_id = obj->object_id;
                    aff->action = NIMCP_ACTION_GRASP;
                    aff->state = NIMCP_AFFORDANCE_STATE_DETECTED;
                    aff->saliency = 0.5 * manipulability;
                    aff->reachability = reachability;
                    aff->manipulability = manipulability;
                    aff->detection_time = now;
                    aff->decay_rate = ctx->config.decay_rate;
                }
            }
            break;
    }
}

/**
 * @brief Calculate competition score for affordance
 */
static double calculate_competition_score(
    const nimcp_affordance_context_t* ctx,
    const nimcp_affordance_t* aff
) {
    double score = aff->saliency;

    /* Apply goal relevance weight */
    if (ctx->has_goal) {
        score += ctx->config.goal_weight * aff->goal_relevance;
    }

    /* Apply motor readiness weight */
    score += ctx->config.motor_weight * aff->motor_readiness;

    /* Modulate by reachability and manipulability */
    score *= aff->reachability;
    score *= aff->manipulability;

    return fmax(0.0, fmin(1.0, score));
}

/**
 * @brief Apply softmax selection
 */
static uint32_t softmax_select(
    nimcp_affordance_context_t* ctx,
    double temperature
) {
    if (ctx->num_affordances == 0) {
        return 0;
    }

    /* Calculate softmax probabilities */
    double max_score = -DBL_MAX;
    for (uint32_t i = 0; i < ctx->num_affordances; i++) {
        if (ctx->affordances[i].state == NIMCP_AFFORDANCE_STATE_DETECTED ||
            ctx->affordances[i].state == NIMCP_AFFORDANCE_STATE_COMPETING) {
            double score = calculate_competition_score(ctx, &ctx->affordances[i]);
            if (score > max_score) {
                max_score = score;
            }
        }
    }

    double sum_exp = 0.0;
    double* probs = nimcp_malloc(ctx->num_affordances * sizeof(double));
    if (!probs) {
        return 0;
    }

    for (uint32_t i = 0; i < ctx->num_affordances; i++) {
        if (ctx->affordances[i].state == NIMCP_AFFORDANCE_STATE_DETECTED ||
            ctx->affordances[i].state == NIMCP_AFFORDANCE_STATE_COMPETING) {
            double score = calculate_competition_score(ctx, &ctx->affordances[i]);
            probs[i] = exp((score - max_score) / temperature);
            sum_exp += probs[i];
        } else {
            probs[i] = 0.0;
        }
    }

    /* Normalize */
    for (uint32_t i = 0; i < ctx->num_affordances; i++) {
        probs[i] /= sum_exp;
    }

    /* Sample */
    double r = (double)nimcp_tl_rand() / RAND_MAX;
    double cumsum = 0.0;
    uint32_t selected = 0;

    for (uint32_t i = 0; i < ctx->num_affordances; i++) {
        cumsum += probs[i];
        if (r <= cumsum) {
            selected = i;
            break;
        }
    }

    nimcp_free(probs);
    return ctx->affordances[selected].affordance_id;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

void nimcp_affordance_default_config(nimcp_affordance_config_t* config) {
    if (!config) {
        return;
    }

    memset(config, 0, sizeof(*config));

    config->detection_threshold = 0.3;
    config->goal_weight = 0.4;
    config->motor_weight = 0.2;
    config->decay_rate = 0.1;

    config->strategy = NIMCP_COMPETITION_WINNER_TAKE_ALL;
    config->competition_threshold = 0.5;
    config->softmax_temperature = 0.1;
    config->max_competition_cycles = 100;

    config->enable_motor_coupling = true;
    config->motor_readiness_threshold = 0.6;

    config->max_objects = NIMCP_AFFORDANCE_MAX_OBJECTS;
    config->max_affordances_per_object = NIMCP_AFFORDANCE_MAX_PER_OBJECT;

    config->update_rate_hz = 30.0;
    config->enable_history = true;
}

nimcp_affordance_context_t* nimcp_affordance_create(
    const nimcp_affordance_config_t* config
) {
    /* Use defaults if config is NULL */
    nimcp_affordance_config_t default_config;
    if (!config) {
        nimcp_affordance_default_config(&default_config);
        config = &default_config;
    }

    nimcp_affordance_context_t* ctx = nimcp_malloc(sizeof(nimcp_affordance_context_t));
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate affordance context");
        return NULL;
    }

    nimcp_affordance_error_t err = nimcp_affordance_init(ctx, config);
    if (err != NIMCP_AFFORDANCE_OK) {
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_affordance_create: validation failed");
        return NULL;
    }

    return ctx;
}

nimcp_affordance_error_t nimcp_affordance_init(
    nimcp_affordance_context_t* ctx,
    const nimcp_affordance_config_t* config
) {
    if (!ctx || !config) {
        return NIMCP_AFFORDANCE_ERROR_NULL_PARAM;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->config = *config;
    ctx->initialized = true;
    ctx->next_affordance_id = 1;
    ctx->stats.creation_time = get_timestamp_ns();
    ctx->last_update_time = ctx->stats.creation_time;

    LOG_INFO("Initialized affordance processing context");

    return NIMCP_AFFORDANCE_OK;
}

nimcp_affordance_error_t nimcp_affordance_reset(nimcp_affordance_context_t* ctx) {
    if (!ctx) {
        return NIMCP_AFFORDANCE_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_AFFORDANCE_ERROR_NOT_INITIALIZED;
    }

    /* Clear objects */
    memset(ctx->objects, 0, sizeof(ctx->objects));
    memset(ctx->object_active, 0, sizeof(ctx->object_active));
    ctx->num_objects = 0;

    /* Clear affordances */
    memset(ctx->affordances, 0, sizeof(ctx->affordances));
    ctx->num_affordances = 0;
    ctx->next_affordance_id = 1;

    /* Clear goal */
    memset(&ctx->current_goal, 0, sizeof(ctx->current_goal));
    ctx->has_goal = false;

    /* Clear history */
    memset(ctx->history, 0, sizeof(ctx->history));
    ctx->history_index = 0;
    ctx->history_count = 0;

    /* Reset statistics */
    uint64_t creation_time = ctx->stats.creation_time;
    memset(&ctx->stats, 0, sizeof(ctx->stats));
    ctx->stats.creation_time = creation_time;

    ctx->last_update_time = get_timestamp_ns();

    LOG_INFO("Reset affordance processing context");

    return NIMCP_AFFORDANCE_OK;
}

void nimcp_affordance_destroy(nimcp_affordance_context_t* ctx) {
    if (!ctx) {
        return;
    }

    LOG_INFO("Destroying affordance context (detections: %llu, selections: %llu, success rate: %.2f%%)",
             (unsigned long long)ctx->stats.total_detections,
             (unsigned long long)ctx->stats.total_selections,
             ctx->stats.action_success_rate * 100.0);

    nimcp_free(ctx);
}

/* ============================================================================
 * Object Management
 * ============================================================================ */

nimcp_affordance_error_t nimcp_affordance_register_object(
    nimcp_affordance_context_t* ctx,
    const nimcp_object_properties_t* properties
) {
    if (!ctx || !properties) {
        return NIMCP_AFFORDANCE_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_AFFORDANCE_ERROR_NOT_INITIALIZED;
    }

    /* Check if object already exists */
    if (find_object(ctx, properties->object_id)) {
        return nimcp_affordance_update_object(ctx, properties);
    }

    /* Find free slot */
    int free_slot = -1;
    for (uint32_t i = 0; i < NIMCP_AFFORDANCE_MAX_OBJECTS; i++) {
        if (!ctx->object_active[i]) {
            free_slot = (int)i;
            break;
        }
    }

    if (free_slot < 0) {
        LOG_WARN("Object limit reached (%u)", NIMCP_AFFORDANCE_MAX_OBJECTS);
        return NIMCP_AFFORDANCE_ERROR_OBJECT_LIMIT;
    }

    /* Add object */
    ctx->objects[free_slot] = *properties;
    ctx->objects[free_slot].first_seen_time = get_timestamp_ns();
    ctx->objects[free_slot].last_update_time = ctx->objects[free_slot].first_seen_time;
    ctx->object_active[free_slot] = true;
    ctx->num_objects++;
    ctx->stats.objects_tracked = ctx->num_objects;

    /* Detect affordances */
    detect_affordances_for_object(ctx, &ctx->objects[free_slot]);

    LOG_DEBUG("Registered object %u (category: %s)",
              properties->object_id,
              nimcp_affordance_category_name(properties->category));

    return NIMCP_AFFORDANCE_OK;
}

nimcp_affordance_error_t nimcp_affordance_update_object(
    nimcp_affordance_context_t* ctx,
    const nimcp_object_properties_t* properties
) {
    if (!ctx || !properties) {
        return NIMCP_AFFORDANCE_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_AFFORDANCE_ERROR_NOT_INITIALIZED;
    }

    nimcp_object_properties_t* obj = find_object(ctx, properties->object_id);
    if (!obj) {
        return nimcp_affordance_register_object(ctx, properties);
    }

    /* Update properties */
    uint64_t first_seen = obj->first_seen_time;
    *obj = *properties;
    obj->first_seen_time = first_seen;
    obj->last_update_time = get_timestamp_ns();

    /* Update existing affordances for this object */
    for (uint32_t i = 0; i < ctx->num_affordances; i++) {
        if (ctx->affordances[i].object_id == properties->object_id) {
            /* Update reachability based on new distance */
            ctx->affordances[i].reachability = calculate_reachability(properties->distance, 0.7);
            ctx->affordances[i].manipulability = calculate_manipulability(properties);
        }
    }

    return NIMCP_AFFORDANCE_OK;
}

nimcp_affordance_error_t nimcp_affordance_remove_object(
    nimcp_affordance_context_t* ctx,
    uint32_t object_id
) {
    if (!ctx) {
        return NIMCP_AFFORDANCE_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_AFFORDANCE_ERROR_NOT_INITIALIZED;
    }

    /* Find and remove object */
    for (uint32_t i = 0; i < NIMCP_AFFORDANCE_MAX_OBJECTS; i++) {
        if (ctx->object_active[i] && ctx->objects[i].object_id == object_id) {
            ctx->object_active[i] = false;
            ctx->num_objects--;
            ctx->stats.objects_tracked = ctx->num_objects;

            /* Remove associated affordances */
            uint32_t write_idx = 0;
            for (uint32_t j = 0; j < ctx->num_affordances; j++) {
                if (ctx->affordances[j].object_id != object_id) {
                    if (write_idx != j) {
                        ctx->affordances[write_idx] = ctx->affordances[j];
                    }
                    write_idx++;
                }
            }
            ctx->num_affordances = write_idx;

            LOG_DEBUG("Removed object %u", object_id);
            return NIMCP_AFFORDANCE_OK;
        }
    }

    return NIMCP_AFFORDANCE_ERROR_INVALID_OBJECT;
}

nimcp_affordance_error_t nimcp_affordance_get_object(
    const nimcp_affordance_context_t* ctx,
    uint32_t object_id,
    nimcp_object_properties_t* properties
) {
    if (!ctx || !properties) {
        return NIMCP_AFFORDANCE_ERROR_NULL_PARAM;
    }

    for (uint32_t i = 0; i < NIMCP_AFFORDANCE_MAX_OBJECTS; i++) {
        if (ctx->object_active[i] && ctx->objects[i].object_id == object_id) {
            *properties = ctx->objects[i];
            return NIMCP_AFFORDANCE_OK;
        }
    }

    return NIMCP_AFFORDANCE_ERROR_INVALID_OBJECT;
}

/* ============================================================================
 * Affordance Detection
 * ============================================================================ */

nimcp_affordance_error_t nimcp_affordance_detect(
    nimcp_affordance_context_t* ctx,
    uint32_t object_id,
    nimcp_affordance_t* affordances,
    uint32_t max_affordances,
    uint32_t* num_detected
) {
    if (!ctx || !affordances || !num_detected) {
        return NIMCP_AFFORDANCE_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_AFFORDANCE_ERROR_NOT_INITIALIZED;
    }

    *num_detected = 0;

    for (uint32_t i = 0; i < ctx->num_affordances && *num_detected < max_affordances; i++) {
        if (ctx->affordances[i].object_id == object_id) {
            affordances[*num_detected] = ctx->affordances[i];
            (*num_detected)++;
        }
    }

    if (*num_detected == 0) {
        return NIMCP_AFFORDANCE_ERROR_NO_AFFORDANCES;
    }

    return NIMCP_AFFORDANCE_OK;
}

nimcp_affordance_error_t nimcp_affordance_detect_all(
    nimcp_affordance_context_t* ctx,
    nimcp_affordance_t* affordances,
    uint32_t max_affordances,
    uint32_t* num_detected
) {
    if (!ctx || !affordances || !num_detected) {
        return NIMCP_AFFORDANCE_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_AFFORDANCE_ERROR_NOT_INITIALIZED;
    }

    *num_detected = 0;

    for (uint32_t i = 0; i < ctx->num_affordances && *num_detected < max_affordances; i++) {
        if (ctx->affordances[i].saliency >= ctx->config.detection_threshold) {
            affordances[*num_detected] = ctx->affordances[i];
            (*num_detected)++;
        }
    }

    ctx->stats.active_affordances = *num_detected;

    return NIMCP_AFFORDANCE_OK;
}

nimcp_affordance_error_t nimcp_affordance_get(
    const nimcp_affordance_context_t* ctx,
    uint32_t affordance_id,
    nimcp_affordance_t* affordance
) {
    if (!ctx || !affordance) {
        return NIMCP_AFFORDANCE_ERROR_NULL_PARAM;
    }

    for (uint32_t i = 0; i < ctx->num_affordances; i++) {
        if (ctx->affordances[i].affordance_id == affordance_id) {
            *affordance = ctx->affordances[i];
            return NIMCP_AFFORDANCE_OK;
        }
    }

    return NIMCP_AFFORDANCE_ERROR_NO_AFFORDANCES;
}

nimcp_affordance_error_t nimcp_affordance_get_by_action(
    const nimcp_affordance_context_t* ctx,
    nimcp_action_type_t action,
    nimcp_affordance_t* affordances,
    uint32_t max_affordances,
    uint32_t* num_found
) {
    if (!ctx || !affordances || !num_found) {
        return NIMCP_AFFORDANCE_ERROR_NULL_PARAM;
    }

    *num_found = 0;

    for (uint32_t i = 0; i < ctx->num_affordances && *num_found < max_affordances; i++) {
        if (ctx->affordances[i].action == action) {
            affordances[*num_found] = ctx->affordances[i];
            (*num_found)++;
        }
    }

    return NIMCP_AFFORDANCE_OK;
}

/* ============================================================================
 * Competition and Selection
 * ============================================================================ */

nimcp_affordance_error_t nimcp_affordance_compete(
    nimcp_affordance_context_t* ctx,
    nimcp_competition_result_t* result
) {
    if (!ctx || !result) {
        return NIMCP_AFFORDANCE_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_AFFORDANCE_ERROR_NOT_INITIALIZED;
    }

    memset(result, 0, sizeof(*result));

    if (ctx->num_affordances == 0) {
        return NIMCP_AFFORDANCE_ERROR_NO_AFFORDANCES;
    }

    uint64_t start_time = get_timestamp_ns();

    /* Set all detected affordances to competing state */
    uint32_t num_competitors = 0;
    for (uint32_t i = 0; i < ctx->num_affordances; i++) {
        if (ctx->affordances[i].state == NIMCP_AFFORDANCE_STATE_DETECTED) {
            ctx->affordances[i].state = NIMCP_AFFORDANCE_STATE_COMPETING;
            ctx->affordances[i].competition_score = calculate_competition_score(ctx, &ctx->affordances[i]);
            num_competitors++;
        }
    }

    if (num_competitors == 0) {
        return NIMCP_AFFORDANCE_ERROR_NO_AFFORDANCES;
    }

    uint32_t winner_id = 0;
    double winner_score = -1.0;

    switch (ctx->config.strategy) {
        case NIMCP_COMPETITION_WINNER_TAKE_ALL: {
            /* Find highest score */
            for (uint32_t i = 0; i < ctx->num_affordances; i++) {
                if (ctx->affordances[i].state == NIMCP_AFFORDANCE_STATE_COMPETING) {
                    if (ctx->affordances[i].competition_score > winner_score) {
                        winner_score = ctx->affordances[i].competition_score;
                        winner_id = ctx->affordances[i].affordance_id;
                    }
                }
            }
            break;
        }

        case NIMCP_COMPETITION_SOFTMAX: {
            winner_id = softmax_select(ctx, ctx->config.softmax_temperature);
            nimcp_affordance_t* winner = find_affordance(ctx, winner_id);
            if (winner) {
                winner_score = winner->competition_score;
            }
            break;
        }

        case NIMCP_COMPETITION_THRESHOLD: {
            /* Find highest above threshold */
            for (uint32_t i = 0; i < ctx->num_affordances; i++) {
                if (ctx->affordances[i].state == NIMCP_AFFORDANCE_STATE_COMPETING &&
                    ctx->affordances[i].competition_score >= ctx->config.competition_threshold) {
                    if (ctx->affordances[i].competition_score > winner_score) {
                        winner_score = ctx->affordances[i].competition_score;
                        winner_id = ctx->affordances[i].affordance_id;
                    }
                }
            }
            break;
        }

        case NIMCP_COMPETITION_GOAL_DIRECTED: {
            if (!ctx->has_goal) {
                /* Fall back to winner-take-all */
                for (uint32_t i = 0; i < ctx->num_affordances; i++) {
                    if (ctx->affordances[i].state == NIMCP_AFFORDANCE_STATE_COMPETING) {
                        if (ctx->affordances[i].competition_score > winner_score) {
                            winner_score = ctx->affordances[i].competition_score;
                            winner_id = ctx->affordances[i].affordance_id;
                        }
                    }
                }
            } else {
                /* Prioritize goal-relevant affordances */
                for (uint32_t i = 0; i < ctx->num_affordances; i++) {
                    if (ctx->affordances[i].state == NIMCP_AFFORDANCE_STATE_COMPETING) {
                        double score = ctx->affordances[i].competition_score;
                        if (ctx->affordances[i].action == ctx->current_goal.target_action) {
                            score *= 1.5;  /* Boost goal-matching actions */
                        }
                        if (score > winner_score) {
                            winner_score = score;
                            winner_id = ctx->affordances[i].affordance_id;
                        }
                    }
                }
            }
            break;
        }

        default:
            break;
    }

    if (winner_id == 0) {
        return NIMCP_AFFORDANCE_ERROR_COMPETITION;
    }

    /* Update states */
    for (uint32_t i = 0; i < ctx->num_affordances; i++) {
        if (ctx->affordances[i].state == NIMCP_AFFORDANCE_STATE_COMPETING) {
            if (ctx->affordances[i].affordance_id == winner_id) {
                ctx->affordances[i].state = NIMCP_AFFORDANCE_STATE_SELECTED;
                ctx->affordances[i].selection_time = get_timestamp_ns();
            } else {
                ctx->affordances[i].state = NIMCP_AFFORDANCE_STATE_INHIBITED;
            }
        }
    }

    /* Fill result */
    nimcp_affordance_t* winner = find_affordance(ctx, winner_id);
    if (winner) {
        result->winner_affordance_id = winner_id;
        result->winner_object_id = winner->object_id;
        result->winner_action = winner->action;
        result->winner_score = winner_score;
    }
    result->num_competitors = num_competitors;
    result->competition_duration = (get_timestamp_ns() - start_time) / 1e9;
    result->strategy_used = ctx->config.strategy;

    ctx->stats.total_competitions++;
    ctx->stats.total_selections++;
    ctx->stats.avg_competition_time = (ctx->stats.avg_competition_time * (ctx->stats.total_competitions - 1) +
                                        result->competition_duration) / ctx->stats.total_competitions;
    ctx->stats.avg_saliency = (ctx->stats.avg_saliency * (ctx->stats.total_selections - 1) +
                               winner_score) / ctx->stats.total_selections;

    LOG_DEBUG("Competition resolved: winner=%u, action=%s, score=%.3f",
              winner_id, nimcp_affordance_action_name(result->winner_action), winner_score);

    return NIMCP_AFFORDANCE_OK;
}

nimcp_affordance_error_t nimcp_affordance_compete_for_goal(
    nimcp_affordance_context_t* ctx,
    const nimcp_affordance_goal_t* goal,
    nimcp_competition_result_t* result
) {
    if (!ctx || !goal || !result) {
        return NIMCP_AFFORDANCE_ERROR_NULL_PARAM;
    }

    /* Temporarily set goal and run competition */
    nimcp_affordance_goal_t prev_goal = ctx->current_goal;
    bool had_goal = ctx->has_goal;

    ctx->current_goal = *goal;
    ctx->has_goal = true;

    /* Update goal relevance for all affordances */
    for (uint32_t i = 0; i < ctx->num_affordances; i++) {
        double relevance = 0.0;
        if (ctx->affordances[i].action == goal->target_action) {
            relevance += 0.5;
        }
        if (goal->target_object_id > 0 &&
            ctx->affordances[i].object_id == goal->target_object_id) {
            relevance += 0.5;
        }
        ctx->affordances[i].goal_relevance = relevance;
    }

    nimcp_affordance_error_t err = nimcp_affordance_compete(ctx, result);

    /* Restore previous goal state */
    ctx->current_goal = prev_goal;
    ctx->has_goal = had_goal;

    return err;
}

nimcp_affordance_error_t nimcp_affordance_select(
    nimcp_affordance_context_t* ctx,
    uint32_t affordance_id
) {
    if (!ctx) {
        return NIMCP_AFFORDANCE_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_AFFORDANCE_ERROR_NOT_INITIALIZED;
    }

    nimcp_affordance_t* aff = find_affordance(ctx, affordance_id);
    if (!aff) {
        return NIMCP_AFFORDANCE_ERROR_NO_AFFORDANCES;
    }

    /* Inhibit all others */
    for (uint32_t i = 0; i < ctx->num_affordances; i++) {
        if (ctx->affordances[i].affordance_id != affordance_id) {
            if (ctx->affordances[i].state == NIMCP_AFFORDANCE_STATE_DETECTED ||
                ctx->affordances[i].state == NIMCP_AFFORDANCE_STATE_COMPETING ||
                ctx->affordances[i].state == NIMCP_AFFORDANCE_STATE_SELECTED) {
                ctx->affordances[i].state = NIMCP_AFFORDANCE_STATE_INHIBITED;
            }
        }
    }

    aff->state = NIMCP_AFFORDANCE_STATE_SELECTED;
    aff->selection_time = get_timestamp_ns();
    ctx->stats.total_selections++;

    LOG_DEBUG("Manually selected affordance %u", affordance_id);

    return NIMCP_AFFORDANCE_OK;
}

nimcp_affordance_error_t nimcp_affordance_inhibit(
    nimcp_affordance_context_t* ctx,
    uint32_t affordance_id
) {
    if (!ctx) {
        return NIMCP_AFFORDANCE_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_AFFORDANCE_ERROR_NOT_INITIALIZED;
    }

    nimcp_affordance_t* aff = find_affordance(ctx, affordance_id);
    if (!aff) {
        return NIMCP_AFFORDANCE_ERROR_NO_AFFORDANCES;
    }

    aff->state = NIMCP_AFFORDANCE_STATE_INHIBITED;

    LOG_DEBUG("Inhibited affordance %u", affordance_id);

    return NIMCP_AFFORDANCE_OK;
}

/* ============================================================================
 * Motor Coupling
 * ============================================================================ */

nimcp_affordance_error_t nimcp_affordance_get_motor_programs(
    const nimcp_affordance_context_t* ctx,
    uint32_t affordance_id,
    nimcp_motor_program_t* programs,
    uint32_t max_programs,
    uint32_t* num_programs
) {
    if (!ctx || !programs || !num_programs) {
        return NIMCP_AFFORDANCE_ERROR_NULL_PARAM;
    }

    for (uint32_t i = 0; i < ctx->num_affordances; i++) {
        if (ctx->affordances[i].affordance_id == affordance_id) {
            *num_programs = 0;
            for (uint32_t j = 0; j < ctx->affordances[i].num_motor_programs && *num_programs < max_programs; j++) {
                programs[*num_programs] = ctx->affordances[i].motor_programs[j];
                (*num_programs)++;
            }
            return NIMCP_AFFORDANCE_OK;
        }
    }

    return NIMCP_AFFORDANCE_ERROR_NO_AFFORDANCES;
}

nimcp_affordance_error_t nimcp_affordance_update_motor_readiness(
    nimcp_affordance_context_t* ctx,
    uint32_t affordance_id,
    double readiness
) {
    if (!ctx) {
        return NIMCP_AFFORDANCE_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_AFFORDANCE_ERROR_NOT_INITIALIZED;
    }

    nimcp_affordance_t* aff = find_affordance(ctx, affordance_id);
    if (!aff) {
        return NIMCP_AFFORDANCE_ERROR_NO_AFFORDANCES;
    }

    aff->motor_readiness = fmax(0.0, fmin(1.0, readiness));

    return NIMCP_AFFORDANCE_OK;
}

nimcp_affordance_error_t nimcp_affordance_report_outcome(
    nimcp_affordance_context_t* ctx,
    uint32_t affordance_id,
    bool success,
    double actual_effort,
    double actual_duration
) {
    if (!ctx) {
        return NIMCP_AFFORDANCE_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_AFFORDANCE_ERROR_NOT_INITIALIZED;
    }

    nimcp_affordance_t* aff = find_affordance(ctx, affordance_id);
    if (!aff) {
        return NIMCP_AFFORDANCE_ERROR_NO_AFFORDANCES;
    }

    /* Update affordance state */
    aff->state = NIMCP_AFFORDANCE_STATE_COMPLETED;

    /* Update success probability (exponential moving average) */
    double alpha = 0.1;
    aff->success_probability = aff->success_probability * (1.0 - alpha) +
                               (success ? 1.0 : 0.0) * alpha;

    /* Record in history */
    if (ctx->config.enable_history) {
        nimcp_affordance_history_t* hist = &ctx->history[ctx->history_index];
        hist->affordance_id = affordance_id;
        hist->object_id = aff->object_id;
        hist->action = aff->action;
        hist->success = success;
        hist->actual_effort = actual_effort;
        hist->actual_duration = actual_duration;
        hist->timestamp = get_timestamp_ns();

        ctx->history_index = (ctx->history_index + 1) % NIMCP_AFFORDANCE_HISTORY_SIZE;
        if (ctx->history_count < NIMCP_AFFORDANCE_HISTORY_SIZE) {
            ctx->history_count++;
        }
    }

    /* Update statistics */
    ctx->stats.total_executions++;
    if (success) {
        ctx->stats.successful_actions++;
    } else {
        ctx->stats.failed_actions++;
    }
    ctx->stats.action_success_rate = (double)ctx->stats.successful_actions /
                                     ctx->stats.total_executions;

    LOG_DEBUG("Action outcome: affordance=%u, success=%s, effort=%.2f, duration=%.2f",
              affordance_id, success ? "true" : "false", actual_effort, actual_duration);

    return NIMCP_AFFORDANCE_OK;
}

/* ============================================================================
 * Update and Processing
 * ============================================================================ */

nimcp_affordance_error_t nimcp_affordance_update(
    nimcp_affordance_context_t* ctx,
    double delta_time
) {
    if (!ctx) {
        return NIMCP_AFFORDANCE_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_AFFORDANCE_ERROR_NOT_INITIALIZED;
    }

    /* Apply saliency decay */
    for (uint32_t i = 0; i < ctx->num_affordances; i++) {
        if (ctx->affordances[i].state == NIMCP_AFFORDANCE_STATE_DETECTED ||
            ctx->affordances[i].state == NIMCP_AFFORDANCE_STATE_COMPETING) {
            ctx->affordances[i].saliency -= ctx->affordances[i].decay_rate * delta_time;
            if (ctx->affordances[i].saliency < 0.0) {
                ctx->affordances[i].saliency = 0.0;
                ctx->affordances[i].state = NIMCP_AFFORDANCE_STATE_INACTIVE;
            }
        }
    }

    /* Remove very old inactive affordances */
    uint64_t now = get_timestamp_ns();
    uint64_t timeout = 10000000000ULL;  /* 10 seconds */

    uint32_t write_idx = 0;
    for (uint32_t i = 0; i < ctx->num_affordances; i++) {
        bool should_keep = true;

        if (ctx->affordances[i].state == NIMCP_AFFORDANCE_STATE_INACTIVE ||
            ctx->affordances[i].state == NIMCP_AFFORDANCE_STATE_COMPLETED) {
            uint64_t age = now - ctx->affordances[i].detection_time;
            if (age > timeout) {
                should_keep = false;
            }
        }

        if (should_keep) {
            if (write_idx != i) {
                ctx->affordances[write_idx] = ctx->affordances[i];
            }
            write_idx++;
        }
    }
    ctx->num_affordances = write_idx;

    ctx->last_update_time = now;

    return NIMCP_AFFORDANCE_OK;
}

nimcp_affordance_error_t nimcp_affordance_set_goal(
    nimcp_affordance_context_t* ctx,
    const nimcp_affordance_goal_t* goal
) {
    if (!ctx || !goal) {
        return NIMCP_AFFORDANCE_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_AFFORDANCE_ERROR_NOT_INITIALIZED;
    }

    ctx->current_goal = *goal;
    ctx->has_goal = true;

    /* Update goal relevance for all affordances */
    for (uint32_t i = 0; i < ctx->num_affordances; i++) {
        double relevance = 0.0;
        if (ctx->affordances[i].action == goal->target_action) {
            relevance += 0.5;
        }
        if (goal->target_object_id > 0 &&
            ctx->affordances[i].object_id == goal->target_object_id) {
            relevance += 0.5;
        }
        ctx->affordances[i].goal_relevance = relevance;
    }

    LOG_DEBUG("Set goal: action=%s, urgency=%.2f",
              nimcp_affordance_action_name(goal->target_action), goal->urgency);

    return NIMCP_AFFORDANCE_OK;
}

nimcp_affordance_error_t nimcp_affordance_clear_goal(nimcp_affordance_context_t* ctx) {
    if (!ctx) {
        return NIMCP_AFFORDANCE_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_AFFORDANCE_ERROR_NOT_INITIALIZED;
    }

    memset(&ctx->current_goal, 0, sizeof(ctx->current_goal));
    ctx->has_goal = false;

    /* Clear goal relevance */
    for (uint32_t i = 0; i < ctx->num_affordances; i++) {
        ctx->affordances[i].goal_relevance = 0.0;
    }

    LOG_DEBUG("Cleared goal");

    return NIMCP_AFFORDANCE_OK;
}

/* ============================================================================
 * Statistics and Utility
 * ============================================================================ */

nimcp_affordance_error_t nimcp_affordance_get_stats(
    const nimcp_affordance_context_t* ctx,
    nimcp_affordance_stats_t* stats
) {
    if (!ctx || !stats) {
        return NIMCP_AFFORDANCE_ERROR_NULL_PARAM;
    }

    *stats = ctx->stats;
    stats->objects_tracked = ctx->num_objects;

    /* Count active affordances */
    uint32_t active = 0;
    for (uint32_t i = 0; i < ctx->num_affordances; i++) {
        if (ctx->affordances[i].state == NIMCP_AFFORDANCE_STATE_DETECTED ||
            ctx->affordances[i].state == NIMCP_AFFORDANCE_STATE_COMPETING ||
            ctx->affordances[i].state == NIMCP_AFFORDANCE_STATE_SELECTED) {
            active++;
        }
    }
    stats->active_affordances = active;

    return NIMCP_AFFORDANCE_OK;
}

const char* nimcp_affordance_action_name(nimcp_action_type_t action) {
    static const char* names[] = {
        "None", "Grasp", "Push", "Pull", "Lift", "Rotate",
        "Press", "Throw", "Place", "Insert", "Extract",
        "Cut", "Pour", "Open", "Close", "Reach",
        "Manipulate", "Support", "Contain", "Traverse", "Sit"
    };

    if (action >= 0 && action < NIMCP_ACTION_COUNT) {
        return names[action];
    }
    return "Unknown";
}

const char* nimcp_affordance_state_name(nimcp_affordance_state_t state) {
    static const char* names[] = {
        "Inactive", "Detected", "Competing", "Selected",
        "Executing", "Completed", "Inhibited"
    };

    if (state >= 0 && state < NIMCP_AFFORDANCE_STATE_COUNT) {
        return names[state];
    }
    return "Unknown";
}

const char* nimcp_affordance_category_name(nimcp_object_category_t category) {
    static const char* names[] = {
        "Unknown", "Tool", "Container", "Surface", "Support",
        "Manipulandum", "Button", "Handle", "Opening", "Projectile"
    };

    if (category >= 0 && category < NIMCP_OBJECT_CATEGORY_COUNT) {
        return names[category];
    }
    return "Unknown";
}
