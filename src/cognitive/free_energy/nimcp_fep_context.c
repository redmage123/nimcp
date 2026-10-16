/**
 * @file nimcp_fep_context.c
 * @brief Free Energy Principle Contextual Model Switching
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Implementation of context-dependent generative model switching
 * WHY:  Cognitive efficiency requires task-specific models
 * HOW:  Maintain context library, infer context, switch with hard/soft/gated strategies
 */

#include "cognitive/free_energy/nimcp_fep_context.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(fep_context_instance, MESH_ADAPTER_CATEGORY_COGNITIVE)

/* Alias: tests reference fep_context_set_health_agent (without _instance suffix) */
static nimcp_health_agent_t* g_fep_context_health_agent = NULL;
void fep_context_set_health_agent(struct nimcp_health_agent* agent) { g_fep_context_health_agent = agent; }


/* ============================================================================
 * Internal Structures
 * ============================================================================ */

struct fep_context_system {
    fep_context_config_t config;

    /* Context library */
    fep_context_t* contexts;
    uint32_t num_contexts;
    uint32_t max_contexts;

    /* Current state */
    uint32_t active_context_id;
    float active_confidence;
    bool switching_in_progress;
    float blend_alpha;              /* For soft switching */
    uint32_t blend_source_id;
    uint32_t blend_target_id;

    /* Connected FEP system */
    fep_system_t* fep_system;

    /* ID generation */
    uint32_t next_context_id;

    /* Training step counter (separate from num_contexts to avoid corruption) */
    uint32_t training_step_count;

    /* Bio-async */
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

static inline float safe_exp(float x) {
    x = nimcp_clampf(x, -88.0f, 88.0f);
    return expf(x);
}

static inline float safe_log(float x) {
    if (x <= 1e-10f) return -100.0f;
    return logf(x);
}

static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/* Find context by ID */
static fep_context_t* find_context(fep_context_system_t* sys, uint32_t id) {
    if (!sys) {
        return NULL;
    }

    for (uint32_t i = 0; i < sys->num_contexts; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sys->num_contexts > 256) {
            fep_context_instance_heartbeat("fep_context_loop",
                             (float)(i + 1) / (float)sys->num_contexts);
        }

        if (sys->contexts[i].context_id == id) {
            return &sys->contexts[i];
        }
    }
    /* Not found is normal control flow — no immune throw */
    return NULL;
}

/* Compute free energy under a specific context */
static float compute_context_free_energy(
    fep_context_system_t* sys,
    fep_system_t* fep,
    fep_context_t* ctx,
    const float* observation,
    size_t obs_dim
) {
    if (!ctx || !observation) return 1e10f;

    /* F = -log p(o) ≈ E_q[(o - pred)^2 / (2σ^2)] + 0.5 * log(σ^2) */
    float fe = 0.0f;
    size_t dim = obs_dim < ctx->belief_dim ? obs_dim : ctx->belief_dim;

    for (size_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            fep_context_instance_heartbeat("fep_context_loop",
                             (float)(i + 1) / (float)dim);
        }

        float pred = ctx->prior_beliefs[i];
        float diff = observation[i] - pred;
        fe += 0.5f * diff * diff;
    }

    return fe;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

void fep_context_default_config(fep_context_config_t* config) {
    if (!config) return;

    /* Phase 8: Heartbeat at operation start */
    fep_context_instance_heartbeat("fep_context_default_config", 0.0f);


    config->max_contexts = FEP_CONTEXT_DEFAULT_MAX;
    config->switch_mode = CONTEXT_SWITCH_SOFT;
    config->switch_threshold = FEP_CONTEXT_DEFAULT_THRESHOLD;
    config->interpolation_rate = FEP_CONTEXT_DEFAULT_INTERP;
    config->context_decay_rate = FEP_CONTEXT_DEFAULT_DECAY;
    config->enable_context_learning = true;
    config->enable_context_creation = true;
}

fep_context_system_t* fep_context_create(const fep_context_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    fep_context_instance_heartbeat("fep_context_create", 0.0f);


    fep_context_system_t* sys = (fep_context_system_t*)nimcp_calloc(
        1, sizeof(fep_context_system_t));
    NIMCP_API_CHECK_ALLOC(sys, "Failed to allocate FEP context system");

    /* Apply configuration */
    fep_context_config_t default_cfg;
    if (!config) {
        fep_context_default_config(&default_cfg);
        config = &default_cfg;
    }
    sys->config = *config;

    /* Allocate context array */
    sys->max_contexts = config->max_contexts;
    sys->contexts = (fep_context_t*)nimcp_calloc(
        sys->max_contexts, sizeof(fep_context_t));
    if (!sys->contexts) {
        fep_context_destroy(sys);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fep_context_create: sys->contexts is NULL");
        return NULL;
    }

    sys->next_context_id = 1;
    sys->active_context_id = 0;
    sys->blend_alpha = 1.0f;

    /* Create mutex */
    sys->mutex = nimcp_platform_mutex_create();
    if (!sys->mutex) {
        fep_context_destroy(sys);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fep_context_create: sys->mutex is NULL");
        return NULL;
    }

    NIMCP_LOGGING_INFO("Context system created: max=%u, mode=%d",
                      config->max_contexts, config->switch_mode);
    return sys;
}

void fep_context_destroy(fep_context_system_t* sys) {
    if (!sys) return;

    /* Phase 8: Heartbeat at operation start */
    fep_context_instance_heartbeat("fep_context_destroy", 0.0f);


    if (sys->bio_async_enabled) {
        fep_context_disconnect_bio_async(sys);
    }

    /* Free context beliefs */
    if (sys->contexts) {
        for (uint32_t i = 0; i < sys->num_contexts; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && sys->num_contexts > 256) {
                fep_context_instance_heartbeat("fep_context_loop",
                                 (float)(i + 1) / (float)sys->num_contexts);
            }

            if (sys->contexts[i].prior_beliefs) {
                nimcp_free(sys->contexts[i].prior_beliefs);
            }
            if (sys->contexts[i].transition_matrix) {
                nimcp_free(sys->contexts[i].transition_matrix);
            }
        }
        nimcp_free(sys->contexts);
    }

    if (sys->mutex) {
        nimcp_platform_mutex_destroy(sys->mutex);
        nimcp_free(sys->mutex);
        sys->mutex = NULL;
    }

    nimcp_free(sys);
    sys = NULL;
    NIMCP_LOGGING_INFO("Context system destroyed");
}

/* ============================================================================
 * Context Management Implementation
 * ============================================================================ */

int fep_context_add(
    fep_context_system_t* sys,
    const char* name,
    const float* prior_beliefs,
    size_t belief_dim,
    uint32_t* context_id
) {
    if (!sys || !name || !prior_beliefs || !context_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_context_add: required parameter is NULL (sys, name, prior_beliefs, context_id)");
        return -1;
    }
    if (belief_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "fep_context_add: belief_dim is zero");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_context_instance_heartbeat("fep_context_add", 0.0f);


    nimcp_platform_mutex_lock(sys->mutex);

    if (sys->num_contexts >= sys->max_contexts) {
        NIMCP_LOGGING_ERROR("Context library full");
        nimcp_platform_mutex_unlock(sys->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "fep_context_add: capacity exceeded");
        return -1;
    }

    /* Find empty slot */
    fep_context_t* ctx = &sys->contexts[sys->num_contexts];

    /* Initialize context */
    ctx->context_id = sys->next_context_id++;
    strncpy(ctx->name, name, FEP_CONTEXT_MAX_NAME_LEN - 1);
    ctx->name[FEP_CONTEXT_MAX_NAME_LEN - 1] = '\0';

    /* Allocate and copy beliefs */
    ctx->prior_beliefs = (float*)nimcp_calloc(belief_dim, sizeof(float));
    if (!ctx->prior_beliefs) {
        nimcp_platform_mutex_unlock(sys->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fep_context_add: ctx->prior_beliefs is NULL");
        return -1;
    }
    memcpy(ctx->prior_beliefs, prior_beliefs, belief_dim * sizeof(float));
    ctx->belief_dim = (uint32_t)belief_dim;

    ctx->activation = 0.0f;
    ctx->last_used = get_time_ms();
    ctx->use_count = 0;

    *context_id = ctx->context_id;
    sys->num_contexts++;

    /* If first context, make it active */
    if (sys->num_contexts == 1) {
        sys->active_context_id = ctx->context_id;
    }

    NIMCP_LOGGING_INFO("Context added: id=%u, name='%s', dim=%zu",
                      ctx->context_id, name, belief_dim);

    nimcp_platform_mutex_unlock(sys->mutex);
    return 0;
}

int fep_context_remove(fep_context_system_t* sys, uint32_t context_id) {
    if (!sys) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_context_remove: sys is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_context_instance_heartbeat("fep_context_remove", 0.0f);


    nimcp_platform_mutex_lock(sys->mutex);

    /* Find context index */
    int found_idx = -1;
    for (uint32_t i = 0; i < sys->num_contexts; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sys->num_contexts > 256) {
            fep_context_instance_heartbeat("fep_context_loop",
                             (float)(i + 1) / (float)sys->num_contexts);
        }

        if (sys->contexts[i].context_id == context_id) {
            found_idx = (int)i;
            break;
        }
    }

    if (found_idx < 0) {
        nimcp_platform_mutex_unlock(sys->mutex);
        NIMCP_LOGGING_ERROR("fep_context_remove: context id=%u not found", context_id);
        return -1;
    }

    /* Free context resources */
    fep_context_t* ctx = &sys->contexts[found_idx];
    if (ctx->prior_beliefs) nimcp_free(ctx->prior_beliefs);
    if (ctx->transition_matrix) nimcp_free(ctx->transition_matrix);

    /* Shift remaining contexts */
    for (uint32_t i = (uint32_t)found_idx; i < sys->num_contexts - 1; i++) {
        sys->contexts[i] = sys->contexts[i + 1];
    }
    sys->num_contexts--;

    /* Update active context if needed */
    if (sys->active_context_id == context_id && sys->num_contexts > 0) {
        sys->active_context_id = sys->contexts[0].context_id;
    }

    nimcp_platform_mutex_unlock(sys->mutex);
    return 0;
}

int fep_context_get(
    const fep_context_system_t* sys,
    uint32_t context_id,
    fep_context_t* context
) {
    if (!sys || !context) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_context_get: required parameter is NULL (sys, context)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_context_instance_heartbeat("fep_context_get", 0.0f);

    nimcp_platform_mutex_lock(((fep_context_system_t*)sys)->mutex);

    fep_context_t* ctx = find_context((fep_context_system_t*)sys, context_id);
    if (!ctx) {
        nimcp_platform_mutex_unlock(((fep_context_system_t*)sys)->mutex);
        return -1;
    }

    /* Deep copy: copy scalar fields first, then duplicate pointer fields */
    *context = *ctx;

    /* Duplicate prior_beliefs to avoid exposing internal pointer */
    if (ctx->prior_beliefs && ctx->belief_dim > 0) {
        context->prior_beliefs = (float*)nimcp_calloc(ctx->belief_dim, sizeof(float));
        if (!context->prior_beliefs) {
            nimcp_platform_mutex_unlock(((fep_context_system_t*)sys)->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                "fep_context_get: failed to allocate prior_beliefs copy");
            return -1;
        }
        memcpy(context->prior_beliefs, ctx->prior_beliefs, ctx->belief_dim * sizeof(float));
    } else {
        context->prior_beliefs = NULL;
    }

    /* Duplicate transition_matrix to avoid exposing internal pointer */
    if (ctx->transition_matrix && ctx->belief_dim > 0) {
        size_t matrix_size = (size_t)ctx->belief_dim * (size_t)ctx->belief_dim;
        context->transition_matrix = (float*)nimcp_calloc(matrix_size, sizeof(float));
        if (!context->transition_matrix) {
            nimcp_free(context->prior_beliefs);
            context->prior_beliefs = NULL;
            nimcp_platform_mutex_unlock(((fep_context_system_t*)sys)->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                "fep_context_get: failed to allocate transition_matrix copy");
            return -1;
        }
        memcpy(context->transition_matrix, ctx->transition_matrix, matrix_size * sizeof(float));
    } else {
        context->transition_matrix = NULL;
    }

    nimcp_platform_mutex_unlock(((fep_context_system_t*)sys)->mutex);
    return 0;
}

int fep_context_update(
    fep_context_system_t* sys,
    uint32_t context_id,
    const float* new_beliefs,
    size_t belief_dim
) {
    if (!sys || !new_beliefs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_context_update: required parameter is NULL (sys, new_beliefs)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_context_instance_heartbeat("fep_context_update", 0.0f);


    nimcp_platform_mutex_lock(sys->mutex);

    fep_context_t* ctx = find_context(sys, context_id);
    if (!ctx) {
        nimcp_platform_mutex_unlock(sys->mutex);
        /* find_context already throws to immune */
        return -1;
    }

    /* Reallocate if dimension changed (safe pattern: allocate new before freeing old) */
    if (ctx->belief_dim != belief_dim) {
        float* new_ptr = (float*)nimcp_calloc(belief_dim, sizeof(float));
        if (!new_ptr) {
            nimcp_platform_mutex_unlock(sys->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fep_context_update: new_ptr is NULL");
            return -1;
        }
        float* old_ptr = ctx->prior_beliefs;
        ctx->prior_beliefs = new_ptr;
        ctx->belief_dim = (uint32_t)belief_dim;
        if (old_ptr) nimcp_free(old_ptr);
    }

    memcpy(ctx->prior_beliefs, new_beliefs, belief_dim * sizeof(float));

    nimcp_platform_mutex_unlock(sys->mutex);
    return 0;
}

/* ============================================================================
 * Context Switching Implementation
 * ============================================================================ */

/* Internal helper to apply context priors (must be called with mutex held) */
static void apply_context_priors_nolock(
    fep_context_system_t* sys,
    fep_system_t* fep,
    fep_context_t* ctx
) {
    if (!sys || !fep || !ctx) return;

    /* Apply context priors to FEP level 0 */
    if (fep->num_levels > 0) {
        fep_hierarchy_level_t* level = &fep->levels[0];
        size_t dim = ctx->belief_dim < level->beliefs.dim ?
                     ctx->belief_dim : level->beliefs.dim;

        for (size_t i = 0; i < dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && dim > 256) {
                fep_context_instance_heartbeat("fep_context_loop",
                                 (float)(i + 1) / (float)dim);
            }

            level->prior_mean[i] = ctx->prior_beliefs[i];
            level->beliefs.mean[i] = ctx->prior_beliefs[i];
        }
    }
}

int fep_context_switch(
    fep_context_system_t* sys,
    fep_system_t* fep,
    uint32_t target_context_id
) {
    if (!sys || !fep) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_context_switch: required parameter is NULL (sys, fep)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_context_instance_heartbeat("fep_context_switch", 0.0f);


    nimcp_platform_mutex_lock(sys->mutex);

    fep_context_t* target = find_context(sys, target_context_id);
    if (!target) {
        nimcp_platform_mutex_unlock(sys->mutex);
        /* find_context already throws to immune */
        return -1;
    }

    /* Apply context based on switching mode */
    switch (sys->config.switch_mode) {
        case CONTEXT_SWITCH_HARD:
            /* Immediate full switch */
            sys->active_context_id = target_context_id;
            sys->blend_alpha = 1.0f;
            apply_context_priors_nolock(sys, fep, target);
            break;

        case CONTEXT_SWITCH_SOFT:
            /* Start gradual interpolation */
            sys->switching_in_progress = true;
            sys->blend_source_id = sys->active_context_id;
            sys->blend_target_id = target_context_id;
            sys->blend_alpha = 0.0f;
            /* Update active context to target (even though transition is gradual) */
            sys->active_context_id = target_context_id;
            break;

        case CONTEXT_SWITCH_GATED:
            /* Only switch if confidence exceeds threshold */
            if (sys->active_confidence >= sys->config.switch_threshold) {
                sys->active_context_id = target_context_id;
                apply_context_priors_nolock(sys, fep, target);
            }
            break;
    }

    /* Update context metadata */
    target->last_used = get_time_ms();
    target->use_count++;
    target->activation = 1.0f;

    NIMCP_LOGGING_INFO("Context switch to id=%u", target_context_id);

    nimcp_platform_mutex_unlock(sys->mutex);
    return 0;
}

int fep_context_infer(
    fep_context_system_t* sys,
    fep_system_t* fep,
    const float* observation,
    size_t obs_dim,
    uint32_t* inferred_context_id,
    float* confidence
) {
    if (!sys || !fep || !observation || !inferred_context_id || !confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_context_infer: required parameter is NULL (sys, fep, observation, inferred_context_id, confidence)");
        return -1;
    }
    if (sys->num_contexts == 0) {
        NIMCP_LOGGING_DEBUG("fep_context_infer: no contexts available");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_context_instance_heartbeat("fep_context_infer", 0.0f);


    nimcp_platform_mutex_lock(sys->mutex);

    /* Compute free energy for each context */
    float* free_energies = (float*)nimcp_calloc(sys->num_contexts, sizeof(float));
    if (!free_energies) {
        nimcp_platform_mutex_unlock(sys->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fep_context_infer: free_energies is NULL");
        return -1;
    }

    float min_fe = 1e10f;
    uint32_t best_idx = 0;

    for (uint32_t i = 0; i < sys->num_contexts; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sys->num_contexts > 256) {
            fep_context_instance_heartbeat("fep_context_loop",
                             (float)(i + 1) / (float)sys->num_contexts);
        }

        free_energies[i] = compute_context_free_energy(
            sys, fep, &sys->contexts[i], observation, obs_dim);

        if (free_energies[i] < min_fe) {
            min_fe = free_energies[i];
            best_idx = i;
        }
    }

    /* Convert to probabilities via softmax over negative free energies */
    float sum_exp = 0.0f;
    for (uint32_t i = 0; i < sys->num_contexts; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sys->num_contexts > 256) {
            fep_context_instance_heartbeat("fep_context_loop",
                             (float)(i + 1) / (float)sys->num_contexts);
        }

        free_energies[i] = safe_exp(-(free_energies[i] - min_fe));
        sum_exp += free_energies[i];
    }

    float best_prob = free_energies[best_idx] / sum_exp;

    *inferred_context_id = sys->contexts[best_idx].context_id;
    *confidence = best_prob;

    sys->active_confidence = best_prob;

    nimcp_free(free_energies);
    free_energies = NULL;
    nimcp_platform_mutex_unlock(sys->mutex);
    return 0;
}

int fep_context_auto_switch(
    fep_context_system_t* sys,
    fep_system_t* fep,
    const float* observation,
    size_t obs_dim
) {
    if (!sys || !fep || !observation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_context_auto_switch: required parameter is NULL (sys, fep, observation)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_context_instance_heartbeat("fep_context_auto_switch", 0.0f);


    uint32_t inferred_id = 0;
    float confidence = 0.0f;

    int ret = fep_context_infer(sys, fep, observation, obs_dim, &inferred_id, &confidence);
    if (ret != 0) return ret;

    /* Switch if inferred context differs from active */
    if (inferred_id != sys->active_context_id) {
        return fep_context_switch(sys, fep, inferred_id);
    }

    return 0;
}

/* ============================================================================
 * Context Application Implementation
 * ============================================================================ */

int fep_context_apply(
    fep_context_system_t* sys,
    fep_system_t* fep,
    uint32_t context_id
) {
    if (!sys || !fep) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_context_apply: required parameter is NULL (sys, fep)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_context_instance_heartbeat("fep_context_apply", 0.0f);


    nimcp_platform_mutex_lock(sys->mutex);

    fep_context_t* ctx = find_context(sys, context_id);
    if (!ctx) {
        nimcp_platform_mutex_unlock(sys->mutex);
        /* find_context already throws to immune */
        return -1;
    }

    /* Apply context priors to FEP level 0 */
    if (fep->num_levels > 0) {
        fep_hierarchy_level_t* level = &fep->levels[0];
        size_t dim = ctx->belief_dim < level->beliefs.dim ?
                     ctx->belief_dim : level->beliefs.dim;

        for (size_t i = 0; i < dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && dim > 256) {
                fep_context_instance_heartbeat("fep_context_loop",
                                 (float)(i + 1) / (float)dim);
            }

            level->prior_mean[i] = ctx->prior_beliefs[i];
            level->beliefs.mean[i] = ctx->prior_beliefs[i];
        }
    }

    nimcp_platform_mutex_unlock(sys->mutex);
    return 0;
}

int fep_context_blend(
    fep_context_system_t* sys,
    fep_system_t* fep,
    uint32_t context1_id,
    uint32_t context2_id,
    float blend_factor
) {
    if (!sys || !fep) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_context_blend: required parameter is NULL (sys, fep)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_context_instance_heartbeat("fep_context_blend", 0.0f);


    nimcp_platform_mutex_lock(sys->mutex);

    fep_context_t* ctx1 = find_context(sys, context1_id);
    fep_context_t* ctx2 = find_context(sys, context2_id);

    if (!ctx1 || !ctx2) {
        nimcp_platform_mutex_unlock(sys->mutex);
        /* find_context already throws to immune */
        return -1;
    }

    blend_factor = nimcp_clampf(blend_factor, 0.0f, 1.0f);

    /* Blend and apply to FEP level 0 */
    if (fep->num_levels > 0) {
        fep_hierarchy_level_t* level = &fep->levels[0];
        size_t dim = ctx1->belief_dim < ctx2->belief_dim ?
                     ctx1->belief_dim : ctx2->belief_dim;
        dim = dim < level->beliefs.dim ? dim : level->beliefs.dim;

        for (size_t i = 0; i < dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && dim > 256) {
                fep_context_instance_heartbeat("fep_context_loop",
                                 (float)(i + 1) / (float)dim);
            }

            float blended = (1.0f - blend_factor) * ctx1->prior_beliefs[i] +
                           blend_factor * ctx2->prior_beliefs[i];
            level->prior_mean[i] = blended;
            level->beliefs.mean[i] = blended;
        }
    }

    nimcp_platform_mutex_unlock(sys->mutex);
    return 0;
}

/* ============================================================================
 * Context Learning Implementation
 * ============================================================================ */

int fep_context_learn_from_experience(
    fep_context_system_t* sys,
    fep_system_t* fep,
    uint32_t context_id
) {
    if (!sys || !fep) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_context_learn_from_experience: required parameter is NULL (sys, fep)");
        return -1;
    }
    if (!sys->config.enable_context_learning) return 0;

    /* Phase 8: Heartbeat at operation start */
    fep_context_instance_heartbeat("fep_context_learn_from_experience", 0.0f);


    nimcp_platform_mutex_lock(sys->mutex);

    fep_context_t* ctx = find_context(sys, context_id);
    if (!ctx) {
        nimcp_platform_mutex_unlock(sys->mutex);
        /* find_context already throws to immune */
        return -1;
    }

    /* Update context from current FEP beliefs */
    /* μ_ctx ← μ_ctx + η * (μ_current - μ_ctx) */
    if (fep->num_levels > 0) {
        fep_belief_t* beliefs = &fep->levels[0].beliefs;
        float eta = 0.1f;  /* Learning rate */

        size_t dim = ctx->belief_dim < beliefs->dim ?
                     ctx->belief_dim : beliefs->dim;

        for (size_t i = 0; i < dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && dim > 256) {
                fep_context_instance_heartbeat("fep_context_loop",
                                 (float)(i + 1) / (float)dim);
            }

            float diff = beliefs->mean[i] - ctx->prior_beliefs[i];
            ctx->prior_beliefs[i] += eta * diff;
        }
    }

    nimcp_platform_mutex_unlock(sys->mutex);
    return 0;
}

int fep_context_create_from_current(
    fep_context_system_t* sys,
    fep_system_t* fep,
    const char* name,
    uint32_t* new_context_id
) {
    if (!sys || !fep || !name || !new_context_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_context_create_from_current: required parameter is NULL (sys, fep, name, new_context_id)");
        return -1;
    }
    if (!sys->config.enable_context_creation) {
        NIMCP_LOGGING_DEBUG("fep_context_create_from_current: context creation disabled");
        return -1;
    }

    if (fep->num_levels == 0) {
        NIMCP_LOGGING_ERROR("fep_context_create_from_current: fep has no levels configured");
        return -1;
    }

    /* Extract current beliefs */
    /* Phase 8: Heartbeat at operation start */
    fep_context_instance_heartbeat("fep_context_create_from_current", 0.0f);


    fep_belief_t* beliefs = &fep->levels[0].beliefs;

    return fep_context_add(sys, name, beliefs->mean, beliefs->dim, new_context_id);
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int fep_context_get_state(
    const fep_context_system_t* sys,
    fep_context_state_t* state
) {
    if (!sys || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_context_get_state: required parameter is NULL (sys, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_context_instance_heartbeat("fep_context_get_state", 0.0f);

    nimcp_platform_mutex_lock(((fep_context_system_t*)sys)->mutex);
    state->active_context_id = sys->active_context_id;
    state->active_context_confidence = sys->active_confidence;
    state->num_contexts = sys->num_contexts;
    state->switching_in_progress = sys->switching_in_progress;
    nimcp_platform_mutex_unlock(((fep_context_system_t*)sys)->mutex);

    return 0;
}

int fep_context_get_active(
    const fep_context_system_t* sys,
    uint32_t* context_id
) {
    if (!sys || !context_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_context_get_active: required parameter is NULL (sys, context_id)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    fep_context_instance_heartbeat("fep_context_get_active", 0.0f);

    nimcp_platform_mutex_lock(((fep_context_system_t*)sys)->mutex);
    *context_id = sys->active_context_id;
    nimcp_platform_mutex_unlock(((fep_context_system_t*)sys)->mutex);

    return 0;
}

/* ============================================================================
 * Integration Implementation
 * ============================================================================ */

int fep_context_connect(fep_context_system_t* context, fep_system_t* fep) {
    if (!context || !fep) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_context_connect: required parameter is NULL (context, fep)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_context_instance_heartbeat("fep_context_connect", 0.0f);


    nimcp_platform_mutex_lock(context->mutex);
    context->fep_system = fep;
    nimcp_platform_mutex_unlock(context->mutex);

    NIMCP_LOGGING_INFO("Context system connected to FEP");
    return 0;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

int fep_context_connect_bio_async(fep_context_system_t* sys) {
    if (!sys) {
        return -1;
    }
    if (sys->bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    fep_context_instance_heartbeat("fep_context_connect_bio_async", 0.0f);


    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_CONTEXT,
        .module_name = "fep_context",
        .inbox_capacity = 32,
        .user_data = sys
    };

    sys->bio_ctx = bio_router_register_module(&info);
    if (sys->bio_ctx) {
        sys->bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Context connected to bio-async");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    }
    return 0;
}

int fep_context_disconnect_bio_async(fep_context_system_t* sys) {
    if (!sys) {
        return -1;
    }
    if (!sys->bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    fep_context_instance_heartbeat("fep_context_disconnect_bio_async", 0.0f);


    if (sys->bio_ctx) {
        bio_router_unregister_module(sys->bio_ctx);
        sys->bio_ctx = NULL;
    }
    sys->bio_async_enabled = false;
    return 0;
}

bool fep_context_is_bio_async_connected(const fep_context_system_t* sys) {
    /* Phase 8: Heartbeat at operation start */
    fep_context_instance_heartbeat("fep_context_is_bio_async_connect", 0.0f);


    return sys && sys->bio_async_enabled;
}

/* ============================================================================
 * KG Self-Awareness Integration
 * ============================================================================ */

int fep_context_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    fep_context_instance_heartbeat("fep_context_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "FEP_Context");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                fep_context_instance_heartbeat("fep_context_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("FEP Context self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "FEP_Context");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "FEP_Context");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void fep_context_set_instance_health_agent(void* ctx, nimcp_health_agent_t* agent) {
    (void)ctx;  /* context pointer not used for global singleton */
    g_fep_context_instance_health_agent = agent;
}

/* ============================================================================
 * Phase 8: Full Training Implementation
 * ============================================================================ */
int fep_context_training_begin(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_context_training_begin: ctx is NULL");
        return -1;
    }
    fep_context_instance_heartbeat_instance(g_fep_context_instance_health_agent, "fep_ctx_training_begin", 0.0f);
    struct fep_context_system* s = (struct fep_context_system*)ctx;
    /* Reset training step counter, NOT num_contexts (which tracks the context library) */
    s->training_step_count = 0;
    s->active_confidence = (s->active_confidence > 0.0f) ? s->active_confidence : 0.5f;
    s->blend_alpha = (s->blend_alpha > 0.0f) ? s->blend_alpha : 0.5f;
    NIMCP_LOGGING_INFO("fep_context: training begun, counters reset");
    return 0;
}

int fep_context_training_step(void* ctx, float progress) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_context_training_step: ctx is NULL");
        return -1;
    }
    float clamped = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    fep_context_instance_heartbeat_instance(g_fep_context_instance_health_agent, "fep_ctx_training_step", clamped);
    struct fep_context_system* s = (struct fep_context_system*)ctx;
    float p = clamped;
    s->active_confidence += (1.0f - p) * 0.001f;
    if (s->active_confidence > 2.0f) s->active_confidence = 2.0f;
    if (s->active_confidence < 0.0f) s->active_confidence = 0.0f;
    s->blend_alpha += (1.0f - p) * 0.001f;
    if (s->blend_alpha > 2.0f) s->blend_alpha = 2.0f;
    if (s->blend_alpha < 0.0f) s->blend_alpha = 0.0f;
    s->training_step_count++;
    return 0;
}

int fep_context_training_end(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_context_training_end: ctx is NULL");
        return -1;
    }
    fep_context_instance_heartbeat_instance(g_fep_context_instance_health_agent, "fep_ctx_training_end", 1.0f);
    struct fep_context_system* s = (struct fep_context_system*)ctx;
    float avg_metric = (s->active_confidence + s->blend_alpha) / 2.0f;
    NIMCP_LOGGING_INFO("fep_context: training complete, avg_metric=%.4f", avg_metric);
    return 0;
}
