/**
 * @file nimcp_mesh_pattern_routing.c
 * @brief Brain-Inspired Pattern-Based Transaction Routing Implementation
 *
 * WHAT: Routes transactions based on pattern similarity, not discrete types
 * WHY:  Mirrors how the brain routes information without predefined categories
 * HOW:  Modules have receptive fields, transactions have patterns,
 *       activation = similarity × threshold × neuromodulation
 */

#include "mesh/nimcp_mesh_pattern_routing.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "constants/nimcp_learning_constants.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define MESH_PATTERN_ROUTER_MAGIC 0x50415452  /* "PATR" */
#define MAX_REGISTERED_MODULES 512

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Registered module entry
 */
typedef struct registered_module {
    mesh_participant_id_t id;
    mesh_receptive_field_t field;
    bool active;

    /* Learning state */
    float success_rate;
    uint32_t activation_count;
    uint32_t success_count;
} registered_module_t;

/**
 * @brief Pattern router structure
 */
struct mesh_pattern_router {
    uint32_t magic;
    mesh_pattern_router_config_t config;

    /* Registered modules */
    registered_module_t modules[MAX_REGISTERED_MODULES];
    size_t module_count;

    /* Global neuromodulator state */
    float neuromod_levels[4];  /* DA, NE, ACh, 5-HT */

    /* Statistics */
    uint64_t total_routings;
    uint64_t successful_routings;

    /* P1: Thread safety */
    nimcp_mutex_t* mutex;
};

/* ============================================================================
 * Pattern Utilities
 * ============================================================================ */

void mesh_pattern_init(mesh_pattern_t* pattern) {
    if (!pattern) return;
    memset(pattern, 0, sizeof(*pattern));
    pattern->magnitude = 0.0f;
    pattern->active_dims = 0;
}

void mesh_receptive_field_init(mesh_receptive_field_t* field) {
    if (!field) return;
    memset(field, 0, sizeof(*field));
    field->threshold = MESH_DEFAULT_ACTIVATION_THRESHOLD;
    field->sharpness = 1.0f;
    field->learned_bias = 0.0f;
    field->neuromod_gain = 1.0f;
}

float mesh_pattern_similarity(
    const mesh_pattern_t* a,
    const mesh_pattern_t* b
) {
    if (!a || !b) return 0.0f;

    /* Cosine similarity */
    float dot = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;

    for (int i = 0; i < MESH_PATTERN_DIM; i++) {
        dot += a->vector[i] * b->vector[i];
        norm_a += a->vector[i] * a->vector[i];
        norm_b += b->vector[i] * b->vector[i];
    }

    norm_a = sqrtf(norm_a);
    norm_b = sqrtf(norm_b);

    if (norm_a < 1e-6f || norm_b < 1e-6f) {
        return 0.0f;
    }

    float similarity = dot / (norm_a * norm_b);

    /* Clamp to [0, 1] - we only care about positive similarity */
    if (similarity < 0.0f) similarity = 0.0f;
    if (similarity > 1.0f) similarity = 1.0f;

    return similarity;
}

nimcp_error_t mesh_pattern_blend(
    const mesh_pattern_t* patterns,
    const float* weights,
    size_t count,
    mesh_pattern_t* result
) {
    if (!patterns || !weights || !result || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_pattern_routing: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    mesh_pattern_init(result);

    float total_weight = 0.0f;
    for (size_t i = 0; i < count; i++) {
        total_weight += weights[i];
    }

    if (total_weight < 1e-6f) {
        return NIMCP_SUCCESS;
    }

    /* Weighted average of patterns */
    for (size_t i = 0; i < count; i++) {
        float w = weights[i] / total_weight;
        for (int d = 0; d < MESH_PATTERN_DIM; d++) {
            result->vector[d] += patterns[i].vector[d] * w;
        }
        result->magnitude += patterns[i].magnitude * w;
    }

    /* Count active dimensions */
    for (int d = 0; d < MESH_PATTERN_DIM; d++) {
        if (fabsf(result->vector[d]) > 0.01f) {
            result->active_dims++;
        }
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_pattern_from_semantics(
    const char* description,
    mesh_pattern_t* pattern_out
) {
    if (!description || !pattern_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_pattern_routing: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    mesh_pattern_init(pattern_out);

    /* Simple hash-based encoding for demo purposes */
    /* In production, would use learned embeddings */
    size_t len = strlen(description);
    uint32_t hash = 5381;

    for (size_t i = 0; i < len; i++) {
        hash = ((hash << 5) + hash) + description[i];

        /* Distribute across pattern dimensions */
        int dim = (hash + i) % MESH_PATTERN_DIM;
        pattern_out->vector[dim] += 0.1f;
    }

    /* Normalize */
    float norm = 0.0f;
    for (int i = 0; i < MESH_PATTERN_DIM; i++) {
        norm += pattern_out->vector[i] * pattern_out->vector[i];
    }
    norm = sqrtf(norm);

    if (norm > 1e-6f) {
        for (int i = 0; i < MESH_PATTERN_DIM; i++) {
            pattern_out->vector[i] /= norm;
            if (fabsf(pattern_out->vector[i]) > 0.01f) {
                pattern_out->active_dims++;
            }
        }
    }

    pattern_out->magnitude = 1.0f;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Router Lifecycle
 * ============================================================================ */

mesh_pattern_router_t* mesh_pattern_router_create(
    const mesh_pattern_router_config_t* config
) {
    mesh_pattern_router_t* router = nimcp_calloc(1, sizeof(*router));
    if (!router) {
        LOG_ERROR("Failed to allocate pattern router");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_pattern_router_create: router is NULL");
        return NULL;
    }

    router->magic = MESH_PATTERN_ROUTER_MAGIC;

    if (config) {
        router->config = *config;
    } else {
        router->config.default_threshold = MESH_DEFAULT_ACTIVATION_THRESHOLD;
        router->config.competition_strength = 0.1f;
        router->config.enable_learning = true;
        router->config.learning_rate = NIMCP_LEARNING_RATE_DEFAULT;
        router->config.max_endorsers = 16;
    }

    /* Initialize neuromodulator levels to baseline */
    for (int i = 0; i < 4; i++) {
        router->neuromod_levels[i] = 0.5f;
    }

    /* P1: Create mutex for thread safety */
    router->mutex = nimcp_mutex_create(NULL);
    if (!router->mutex) {
        nimcp_free(router);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_pattern_router_create: mutex creation failed");
        return NULL;
    }

    LOG_INFO("Created pattern router with threshold=%.2f",
             router->config.default_threshold);

    return router;
}

void mesh_pattern_router_destroy(mesh_pattern_router_t* router) {
    if (!router || router->magic != MESH_PATTERN_ROUTER_MAGIC) return;

    /* P1: Destroy mutex */
    if (router->mutex) {
        nimcp_mutex_destroy(router->mutex);
    }

    router->magic = 0;
    nimcp_free(router);

    LOG_INFO("Destroyed pattern router");
}

/* ============================================================================
 * Module Registration
 * ============================================================================ */

nimcp_error_t mesh_pattern_router_register_receptive_field(
    mesh_pattern_router_t* router,
    mesh_participant_id_t module_id,
    const mesh_receptive_field_t* field
) {
    if (!router || router->magic != MESH_PATTERN_ROUTER_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_pattern_routing: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!field) return NIMCP_ERROR_NULL_POINTER;

    /* P1: Mutex protection for module list modification */
    nimcp_mutex_lock(router->mutex);

    /* Check if already registered */
    for (size_t i = 0; i < router->module_count; i++) {
        if (router->modules[i].id == module_id) {
            /* Update existing */
            router->modules[i].field = *field;
            nimcp_mutex_unlock(router->mutex);
            return NIMCP_SUCCESS;
        }
    }

    /* Add new */
    if (router->module_count >= MAX_REGISTERED_MODULES) {
        nimcp_mutex_unlock(router->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_CAPACITY_EXCEEDED, "mesh_pattern_routing: error condition");
        return NIMCP_ERROR_CAPACITY_EXCEEDED;
    }

    registered_module_t* mod = &router->modules[router->module_count++];
    mod->id = module_id;
    mod->field = *field;
    mod->active = true;
    mod->success_rate = 0.5f;
    mod->activation_count = 0;
    mod->success_count = 0;

    nimcp_mutex_unlock(router->mutex);

    LOG_DEBUG("Registered receptive field for module 0x%llx (threshold=%.2f)",
              (unsigned long long)module_id, field->threshold);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_pattern_router_update_receptive_field(
    mesh_pattern_router_t* router,
    mesh_participant_id_t module_id,
    const mesh_pattern_t* new_preferred,
    float learning_rate
) {
    if (!router || router->magic != MESH_PATTERN_ROUTER_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_pattern_routing: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!new_preferred) return NIMCP_ERROR_NULL_POINTER;

    /* P1: Mutex protection for module state modification */
    nimcp_mutex_lock(router->mutex);

    /* Find module */
    for (size_t i = 0; i < router->module_count; i++) {
        if (router->modules[i].id == module_id) {
            mesh_receptive_field_t* field = &router->modules[i].field;

            /* Update preferred pattern (simple moving average) */
            if (field->pattern_count > 0) {
                mesh_pattern_t* pref = &field->preferred[0];
                for (int d = 0; d < MESH_PATTERN_DIM; d++) {
                    pref->vector[d] = (1.0f - learning_rate) * pref->vector[d] +
                                      learning_rate * new_preferred->vector[d];
                }
            } else {
                field->preferred[0] = *new_preferred;
                field->pattern_count = 1;
            }

            nimcp_mutex_unlock(router->mutex);
            return NIMCP_SUCCESS;
        }
    }

    nimcp_mutex_unlock(router->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_pattern_routing: error condition");
    return NIMCP_ERROR_NOT_FOUND;
}

/* ============================================================================
 * Routing
 * ============================================================================ */

/**
 * @brief Compute activation for a single module
 */
static float compute_module_activation(
    const registered_module_t* mod,
    const mesh_pattern_transaction_t* tx,
    const float* neuromod_levels
) {
    const mesh_receptive_field_t* field = &mod->field;

    /* Compute max similarity to any preferred pattern */
    float max_sim = 0.0f;
    for (size_t i = 0; i < field->pattern_count; i++) {
        float sim = mesh_pattern_similarity(&tx->content_pattern, &field->preferred[i]);
        if (sim > max_sim) max_sim = sim;
    }

    /* Apply sharpness (tuning curve) */
    /* Higher sharpness = more selective (narrower tuning) */
    float tuned_sim = powf(max_sim, field->sharpness);

    /* Apply neuromodulation */
    float neuromod_effect = field->neuromod_gain;

    /* Norepinephrine broadens receptive fields (lower threshold) */
    float ne_level = neuromod_levels[MESH_NEUROMOD_NOREPINEPHRINE];
    neuromod_effect *= (1.0f + 0.5f * (ne_level - 0.5f));

    /* Dopamine increases salience for high-urgency transactions */
    float da_level = neuromod_levels[MESH_NEUROMOD_DOPAMINE];
    if (tx->urgency > 0.5f) {
        neuromod_effect *= (1.0f + 0.3f * da_level * tx->urgency);
    }

    /* Acetylcholine sharpens attention (increases sharpness) */
    float ach_level = neuromod_levels[MESH_NEUROMOD_ACETYLCHOLINE];
    tuned_sim = powf(tuned_sim, 1.0f + 0.5f * (ach_level - 0.5f));

    /* Apply learned bias */
    float activation = tuned_sim * neuromod_effect + field->learned_bias;

    /* Clamp */
    if (activation < 0.0f) activation = 0.0f;
    if (activation > 1.0f) activation = 1.0f;

    return activation;
}

nimcp_error_t mesh_pattern_router_compute_activations(
    mesh_pattern_router_t* router,
    const mesh_pattern_transaction_t* tx,
    mesh_activation_t* activations,
    size_t max_activations,
    size_t* count_out
) {
    if (!router || router->magic != MESH_PATTERN_ROUTER_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_pattern_routing: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!tx || !activations || !count_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_pattern_routing: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    *count_out = 0;

    /* P1: Mutex protection for module state read and stats update */
    nimcp_mutex_lock(router->mutex);

    /* Compute activation for all modules */
    float all_activations[MAX_REGISTERED_MODULES];
    for (size_t i = 0; i < router->module_count; i++) {
        if (!router->modules[i].active) {
            all_activations[i] = 0.0f;
            continue;
        }

        all_activations[i] = compute_module_activation(
            &router->modules[i], tx, router->neuromod_levels
        );
    }

    /* Apply lateral inhibition (competition) */
    /* Winner-take-all dynamics: strong activations suppress weak ones */
    if (router->config.competition_strength > 0.0f) {
        float max_act = 0.0f;
        for (size_t i = 0; i < router->module_count; i++) {
            if (all_activations[i] > max_act) max_act = all_activations[i];
        }

        for (size_t i = 0; i < router->module_count; i++) {
            if (all_activations[i] < max_act) {
                float suppression = router->config.competition_strength *
                                    (max_act - all_activations[i]);
                all_activations[i] -= suppression;
                if (all_activations[i] < 0.0f) all_activations[i] = 0.0f;
            }
        }
    }

    /* Collect modules above threshold */
    for (size_t i = 0; i < router->module_count && *count_out < max_activations; i++) {
        float threshold = router->modules[i].field.threshold;
        if (threshold <= 0.0f) threshold = router->config.default_threshold;

        if (all_activations[i] >= threshold) {
            mesh_activation_t* act = &activations[*count_out];
            act->module_id = router->modules[i].id;
            act->activation_level = all_activations[i];
            act->confidence = router->modules[i].success_rate;
            act->pattern_similarity = mesh_pattern_similarity(
                &tx->content_pattern,
                &router->modules[i].field.preferred[0]
            );
            act->should_endorse = true;
            (*count_out)++;
        }
    }

    router->total_routings++;

    nimcp_mutex_unlock(router->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_pattern_router_get_endorsers(
    mesh_pattern_router_t* router,
    const mesh_pattern_transaction_t* tx,
    mesh_participant_id_t* endorsers,
    size_t max_endorsers,
    size_t* count_out
) {
    if (!router || !tx || !endorsers || !count_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_pattern_routing: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    mesh_activation_t activations[MAX_REGISTERED_MODULES];
    size_t activation_count = 0;

    nimcp_error_t err = mesh_pattern_router_compute_activations(
        router, tx, activations, MAX_REGISTERED_MODULES, &activation_count
    );

    if (err != NIMCP_SUCCESS) return err;

    /* Sort by activation level (descending) and take top N */
    /* Simple bubble sort for small N */
    for (size_t i = 0; i < activation_count; i++) {
        for (size_t j = i + 1; j < activation_count; j++) {
            if (activations[j].activation_level > activations[i].activation_level) {
                mesh_activation_t tmp = activations[i];
                activations[i] = activations[j];
                activations[j] = tmp;
            }
        }
    }

    /* Take top endorsers up to limit */
    size_t limit = router->config.max_endorsers;
    if (limit > max_endorsers) limit = max_endorsers;
    if (limit > activation_count) limit = activation_count;

    for (size_t i = 0; i < limit; i++) {
        endorsers[i] = activations[i].module_id;
    }
    *count_out = limit;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Neuromodulation
 * ============================================================================ */

nimcp_error_t mesh_pattern_router_apply_neuromodulation(
    mesh_pattern_router_t* router,
    mesh_neuromodulator_t neuromod,
    float level
) {
    if (!router || router->magic != MESH_PATTERN_ROUTER_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_pattern_routing: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (neuromod > MESH_NEUROMOD_SEROTONIN) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_pattern_routing: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Clamp level */
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;

    /* P1: Mutex protection for neuromodulator state modification */
    nimcp_mutex_lock(router->mutex);
    router->neuromod_levels[neuromod] = level;
    nimcp_mutex_unlock(router->mutex);

    const char* names[] = {"Dopamine", "Norepinephrine", "Acetylcholine", "Serotonin"};
    LOG_DEBUG("Applied %s level=%.2f", names[neuromod], level);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Learning
 * ============================================================================ */

nimcp_error_t mesh_pattern_router_learn_outcome(
    mesh_pattern_router_t* router,
    const mesh_pattern_transaction_t* tx,
    const mesh_participant_id_t* endorsers,
    size_t endorser_count,
    bool success,
    float reward_signal
) {
    if (!router || router->magic != MESH_PATTERN_ROUTER_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_pattern_routing: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!router->config.enable_learning) {
        return NIMCP_SUCCESS;
    }

    /* P1: Mutex protection for module state and stats modification */
    nimcp_mutex_lock(router->mutex);

    float lr = router->config.learning_rate;

    /* Update stats and learned bias for each endorser */
    for (size_t e = 0; e < endorser_count; e++) {
        for (size_t i = 0; i < router->module_count; i++) {
            if (router->modules[i].id == endorsers[e]) {
                registered_module_t* mod = &router->modules[i];
                mod->activation_count++;

                if (success) {
                    mod->success_count++;
                    /* Strengthen association: increase bias for this pattern */
                    mod->field.learned_bias += lr * reward_signal;
                } else {
                    /* Weaken association */
                    mod->field.learned_bias -= lr * (1.0f - reward_signal);
                }

                /* Clamp bias */
                if (mod->field.learned_bias > 0.5f) mod->field.learned_bias = 0.5f;
                if (mod->field.learned_bias < -0.5f) mod->field.learned_bias = -0.5f;

                /* Update success rate */
                mod->success_rate = (float)mod->success_count /
                                    (float)mod->activation_count;

                break;
            }
        }
    }

    if (success) {
        router->successful_routings++;
    }

    nimcp_mutex_unlock(router->mutex);
    return NIMCP_SUCCESS;
}
