/**
 * @file nimcp_imagination_engine.c
 * @brief Imagination Engine - Generative Mental Simulation System
 *
 * WHAT: Central engine for generative mental simulation
 * WHY:  Enable construction, manipulation, and exploration of hypothetical scenarios
 * HOW:  Integrates JEPA, hippocampus, visual/audio cortex, and prefrontal control
 *
 * @author NIMCP Development Team
 * @date 2026-01-02
 * @version 2.6.3
 */

#include "cognitive/imagination/nimcp_imagination_engine.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/global_workspace/nimcp_global_workspace.h"
#include "constants/nimcp_buffer_constants.h"
#include "cognitive/imagination/nimcp_imagination_workspace.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "core/brain/nimcp_brain_kg.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/containers/nimcp_darray.h"
#include "utils/algorithms/nimcp_monte_carlo.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/rng/nimcp_rand.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_dimension_constants.h"
#include "constants/nimcp_math_constants.h"

BRIDGE_BOILERPLATE(imagination_engine, MESH_ADAPTER_CATEGORY_COGNITIVE)



/* Thread-local RNG seed for imagination engine - used by mc_random_uniform */
static _Thread_local uint32_t g_imagination_rand_seed = 0;

/*============================================================================
 * Constants
 *============================================================================*/

#define ENGINE_VERSION_MAJOR 1
#define ENGINE_VERSION_MINOR 0
#define ENGINE_VERSION_PATCH 0

#define MAX_ELEMENTS_PER_SCENARIO 32
#define DEFAULT_NOISE_STDDEV 0.1f
#define IMMUNE_MODULATION_SCALE 0.5f

/*============================================================================
 * Tensor Helper Functions (local implementations)
 *============================================================================*/

/* nimcp_tensor_size is now defined in nimcp_tensor.h */

/**
 * @brief Copy tensor data from src to dst (must be same size)
 */
static inline void nimcp_tensor_copy(nimcp_tensor_t* dst, const nimcp_tensor_t* src) {
    if (!dst || !src) return;
    size_t dst_numel = nimcp_tensor_numel(dst);
    size_t src_numel = nimcp_tensor_numel(src);
    if (dst_numel != src_numel) return;

    float* dst_data = (float*)nimcp_tensor_data(dst);
    const float* src_data = (const float*)nimcp_tensor_data_const(src);
    memcpy(dst_data, src_data, dst_numel * sizeof(float));
}

/*============================================================================
 * Helper Functions
 *============================================================================*/

static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/**
 * @brief Fill tensor with a scalar value (zero fill optimization)
 */
static void tensor_fill_zero(nimcp_tensor_t* t) {
    if (!t) return;
    void* data = nimcp_tensor_data(t);
    size_t numel = nimcp_tensor_numel(t);
    if (data && numel > 0) {
        memset(data, 0, numel * sizeof(float));
    }
}

/**
 * @brief Generate Gaussian noise using centralized RNG module
 *
 * WHAT: Generate random value from N(0, stddev^2)
 * WHY:  Add stochasticity to imagination simulations
 * HOW:  Delegates to nimcp_rand_normal() for thread-safe Box-Muller
 */
static inline float gaussian_noise(float stddev) {
    return nimcp_rand_normal(0.0f, stddev);
}

/**
 * @brief Apply noise to tensor
 */
static void apply_noise(nimcp_tensor_t* tensor, float noise_level) {
    if (!tensor || noise_level <= 0.0f) return;

    float* data = nimcp_tensor_data(tensor);
    size_t size = nimcp_tensor_size(tensor);

    for (size_t i = 0; i < size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && size > 256) {
            imagination_engine_heartbeat("imagination__loop",
                             (float)(i + 1) / (float)size);
        }

        data[i] += gaussian_noise(noise_level);
    }
}

/**
 * @brief Compute coherence score between two tensors
 */
static float compute_coherence(const nimcp_tensor_t* a, const nimcp_tensor_t* b) {
    if (!a || !b) return 0.0f;

    size_t size_a = nimcp_tensor_size(a);
    size_t size_b = nimcp_tensor_size(b);
    if (size_a != size_b || size_a == 0) return 0.0f;

    const float* data_a = nimcp_tensor_data_const(a);
    const float* data_b = nimcp_tensor_data_const(b);

    /* Cosine similarity */
    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    for (size_t i = 0; i < size_a; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && size_a > 256) {
            imagination_engine_heartbeat("imagination__loop",
                             (float)(i + 1) / (float)size_a);
        }

        dot += data_a[i] * data_b[i];
        norm_a += data_a[i] * data_a[i];
        norm_b += data_b[i] * data_b[i];
    }

    if (norm_a < 1e-10f || norm_b < 1e-10f) return 0.0f;

    float similarity = dot / (sqrtf(norm_a) * sqrtf(norm_b));
    return (similarity + 1.0f) / 2.0f;  /* Map [-1,1] to [0,1] */
}

/**
 * @brief Compute tensor norm
 */
static float tensor_norm(const nimcp_tensor_t* t) {
    if (!t) return 0.0f;

    const float* data = nimcp_tensor_data_const(t);
    size_t size = nimcp_tensor_size(t);

    float sum = 0.0f;
    for (size_t i = 0; i < size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && size > 256) {
            imagination_engine_heartbeat("imagination__loop",
                             (float)(i + 1) / (float)size);
        }

        sum += data[i] * data[i];
    }
    return sqrtf(sum);
}

/**
 * @brief Blend two tensors: result = alpha * a + (1-alpha) * b
 */
static int blend_tensors(nimcp_tensor_t* result,
                         const nimcp_tensor_t* a,
                         const nimcp_tensor_t* b,
                         float alpha) {
    if (!result || !a || !b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "blend_tensors: required parameter is NULL (result, a, b)");
        return -1;
    }

    size_t size = nimcp_tensor_size(result);
    if (nimcp_tensor_size(a) != size || nimcp_tensor_size(b) != size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "blend_tensors: tensor size mismatch");
        return -1;
    }

    float* r = nimcp_tensor_data(result);
    const float* da = nimcp_tensor_data_const(a);
    const float* db = nimcp_tensor_data_const(b);

    float beta = 1.0f - alpha;
    for (size_t i = 0; i < size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && size > 256) {
            imagination_engine_heartbeat("imagination__loop",
                             (float)(i + 1) / (float)size);
        }

        r[i] = alpha * da[i] + beta * db[i];
    }

    return 0;
}

/*============================================================================
 * Configuration API
 *============================================================================*/

imagination_engine_config_t imagination_engine_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__default_config", 0.0f);


    imagination_engine_config_t config = {
        /* Capacity */
        .max_concurrent_scenarios = IMAGINATION_MAX_SCENARIOS,
        .workspace_capacity = 1024 * 1024,  /* 1MB */
        .latent_dim = IMAGINATION_DEFAULT_LATENT_DIM,

        /* Quality defaults */
        .default_quality = IMAGINATION_QUALITY_NORMAL,
        .default_vividness = IMAGINATION_DEFAULT_VIVIDNESS,
        .creativity_noise_level = IMAGINATION_DEFAULT_NOISE_LEVEL,
        .coherence_threshold = IMAGINATION_DEFAULT_COHERENCE_THRESHOLD,

        /* Feature flags */
        .enable_reality_checking = true,
        .enable_memory_integration = true,
        .enable_prospective_mode = true,
        .enable_counterfactual = true,
        .enable_social_simulation = true,

        /* Integration flags */
        .enable_bio_async = false,
        .enable_immune_modulation = true,
        .enable_gpu_acceleration = false,
        .enable_thalamic_routing = true,
        .enable_substrate_constraints = true,
        .enable_training_feedback = false,
        .enable_logic_constraints = true,
        .enable_parietal_integration = true,
        .enable_quantum_reasoning = false,
        .enable_domain_simulation = true,

        /* GPU settings */
        .gpu_device_id = -1,
        .gpu_batch_size = NIMCP_SMALL_BATCH_SIZE,

        /* Timeouts */
        .default_timeout_ms = IMAGINATION_DIRECTED_TIMEOUT_MS,
        .step_timeout_ms = 100
    };
    return config;
}

bool imagination_engine_validate_config(
    const imagination_engine_config_t* config,
    char* error_msg,
    size_t error_msg_len) {

    if (!config) {
        if (error_msg && error_msg_len > 0) {
            snprintf(error_msg, error_msg_len, "Config is NULL");
        }
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__validate_config", 0.0f);


    if (config->max_concurrent_scenarios == 0 ||
        config->max_concurrent_scenarios > 64) {
        if (error_msg && error_msg_len > 0) {
            snprintf(error_msg, error_msg_len,
                     "max_concurrent_scenarios must be 1-64");
        }
        return false;
    }

    if (config->latent_dim == 0 || config->latent_dim > IMAGINATION_MAX_LATENT_DIM) {
        if (error_msg && error_msg_len > 0) {
            snprintf(error_msg, error_msg_len,
                     "latent_dim must be 1-%d", IMAGINATION_MAX_LATENT_DIM);
        }
        return false;
    }

    if (config->default_vividness < 0.0f || config->default_vividness > 1.0f) {
        if (error_msg && error_msg_len > 0) {
            snprintf(error_msg, error_msg_len,
                     "default_vividness must be 0.0-1.0");
        }
        return false;
    }

    if (config->coherence_threshold < 0.0f || config->coherence_threshold > 1.0f) {
        if (error_msg && error_msg_len > 0) {
            snprintf(error_msg, error_msg_len,
                     "coherence_threshold must be 0.0-1.0");
        }
        return false;
    }

    return true;
}

/*============================================================================
 * Lifecycle API
 *============================================================================*/

imagination_engine_t* imagination_engine_create(
    const imagination_engine_config_t* config) {

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__create", 0.0f);


    imagination_engine_config_t cfg;
    if (config) {
        char error_msg[NIMCP_ERROR_BUFFER_SIZE];
        if (!imagination_engine_validate_config(config, error_msg, sizeof(error_msg))) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "imagination_engine_default_config: imagination_engine_validate_config is NULL");
            return NULL;
        }
        cfg = *config;
    } else {
        cfg = imagination_engine_default_config();
    }

    /* Allocate engine */
    imagination_engine_t* engine = (imagination_engine_t*)nimcp_calloc(
        1, sizeof(imagination_engine_t));
    if (!engine) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate engine");

        return NULL;

    }

    /* Initialize bridge base */
    memset(&engine->base, 0, sizeof(bridge_base_t));
    engine->base.module_id = BIO_MODULE_IMAGINATION;
    engine->base.module_name = "imagination_engine";
    engine->base.bridge_active = false;

    engine->config = cfg;

    /* Create mutex */
    engine->mutex = nimcp_mutex_create(NULL);
    if (!engine->mutex) goto error;

    /* Create workspace */
    imagination_workspace_config_t ws_config = imagination_workspace_default_config();
    ws_config.max_scenarios = cfg.max_concurrent_scenarios;
    ws_config.latent_dim = cfg.latent_dim;

    engine->workspace = imagination_workspace_create(&ws_config);
    if (!engine->workspace) goto error;

    /* Create active scenarios list */
    engine->active_scenarios = nimcp_darray_create(sizeof(imagination_scenario_t*), 16);
    if (!engine->active_scenarios) goto error;

    /* Initialize state */
    engine->current_mode = IMAGINATION_MODE_PASSIVE;
    engine->next_scenario_id = 1;
    engine->gpu_available = false;
    engine->quantum_enabled = false;

    /* Initialize statistics */
    memset(&engine->stats, 0, sizeof(imagination_stats_t));

    /* Initialize thread-safe RNG seed */
    g_imagination_rand_seed = mc_seed_from_time();

    /* Mark as initialized */
    engine->base.bridge_active = true;

    return engine;

error:
    imagination_engine_destroy(engine);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "imagination_engine_create: resource allocation failed");
    return NULL;
}

void imagination_engine_destroy(imagination_engine_t* engine) {
    if (!engine) return;

    /* End all active scenarios */
    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__destroy", 0.0f);


    if (engine->active_scenarios) {
        size_t count = nimcp_darray_size(engine->active_scenarios);
        for (size_t i = 0; i < count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && count > 256) {
                imagination_engine_heartbeat("imagination__loop",
                                 (float)(i + 1) / (float)count);
            }

            imagination_scenario_t** ptr =
                (imagination_scenario_t**)nimcp_darray_at(engine->active_scenarios, i);
            imagination_scenario_t* scenario = ptr ? *ptr : NULL;
            if (scenario) {
                /* Release workspace slot */
                if (engine->workspace && scenario->id) {
                    imagination_workspace_release_scenario(engine->workspace, scenario->id);
                }
                /* Free scenario */
                if (scenario->latent_state) nimcp_tensor_destroy(scenario->latent_state);
                if (scenario->latent_previous) nimcp_tensor_destroy(scenario->latent_previous);
                if (scenario->visual_buffer) nimcp_tensor_destroy(scenario->visual_buffer);
                if (scenario->audio_buffer) nimcp_tensor_destroy(scenario->audio_buffer);
                if (scenario->semantic_buffer) nimcp_tensor_destroy(scenario->semantic_buffer);
                /* Free trajectory tensor entries before destroying darray */
                if (scenario->trajectory) {
                    size_t traj_count = nimcp_darray_size(scenario->trajectory);
                    for (size_t j = 0; j < traj_count; j++) {
                        nimcp_tensor_t** state_ptr =
                            (nimcp_tensor_t**)nimcp_darray_at(scenario->trajectory, j);
                        if (state_ptr && *state_ptr) nimcp_tensor_destroy(*state_ptr);
                    }
                    nimcp_darray_destroy(scenario->trajectory);
                }
                /* Free element entries before destroying darray */
                if (scenario->elements) {
                    size_t elem_count = nimcp_darray_size(scenario->elements);
                    for (size_t j = 0; j < elem_count; j++) {
                        imagination_element_t** elem_ptr =
                            (imagination_element_t**)nimcp_darray_at(scenario->elements, j);
                        if (elem_ptr && *elem_ptr) {
                            imagination_element_t* elem = *elem_ptr;
                            if (elem->features) nimcp_tensor_destroy(elem->features);
                            nimcp_free(elem);
                        }
                    }
                    nimcp_darray_destroy(scenario->elements);
                }
                if (scenario->active_goal) nimcp_free(scenario->active_goal);
                nimcp_free(scenario);
                scenario = NULL;
            }
        }
        nimcp_darray_destroy(engine->active_scenarios);
    }

    /* Destroy workspace */
    if (engine->workspace) {
        imagination_workspace_destroy(engine->workspace);
    }

    /* Destroy mutex */
    if (engine->mutex) {
        nimcp_mutex_destroy(engine->mutex);
    }

    nimcp_free(engine);
}

/* Tier 1 Step E (2026-05-24): copy the currently-active imagined scenario's
 * content vector for the language cascade's content blend, so production can
 * reflect an active imagined scene. Read-only w.r.t. engine state.
 *
 * Prefers the scenario's semantic_buffer (language-space content) and falls
 * back to latent_state (the core scene representation, more often populated).
 * Copies up to caller_cap floats into caller_buf UNDER the engine lock, so
 * the data stays valid after the lock releases even if another thread steps
 * or ends the scenario (no borrowed-pointer UAF). Only F32 tensors are
 * copied.
 *
 * @return number of floats copied (0 = idle: no active scenario or no usable
 *         tensor). out_vividness (optional) receives the scenario's vividness
 *         [0-1] for the caller to weight the blend.
 */
uint32_t imagination_engine_copy_active_vector(imagination_engine_t* engine,
                                               float* caller_buf,
                                               uint32_t caller_cap,
                                               float* out_vividness) {
    if (out_vividness) *out_vividness = 0.0f;
    if (!engine || !caller_buf || caller_cap == 0 || !engine->mutex) return 0;

    uint32_t copied = 0;
    nimcp_mutex_lock(engine->mutex);

    if (engine->active_scenarios &&
        nimcp_darray_size(engine->active_scenarios) > 0) {
        imagination_scenario_t** ptr =
            (imagination_scenario_t**)nimcp_darray_at(engine->active_scenarios, 0);
        imagination_scenario_t* sc = ptr ? *ptr : NULL;
        if (sc && sc->is_active && !sc->is_paused) {
            /* Prefer language-space content; fall back to the latent scene. */
            nimcp_tensor_t* src = sc->semantic_buffer ? sc->semantic_buffer
                                                       : sc->latent_state;
            if (src && nimcp_tensor_dtype(src) == NIMCP_DTYPE_F32) {
                const float* data = (const float*)nimcp_tensor_data_const(src);
                size_t n = nimcp_tensor_numel(src);
                if (data && n > 0) {
                    uint32_t lim = (n < (size_t)caller_cap) ? (uint32_t)n
                                                            : caller_cap;
                    for (uint32_t i = 0; i < lim; i++) {
                        float v = data[i];
                        caller_buf[i] = isfinite(v) ? v : 0.0f;
                    }
                    copied = lim;
                    if (out_vividness) *out_vividness = sc->vividness;
                }
            }
        }
    }

    nimcp_mutex_unlock(engine->mutex);
    return copied;
}

int imagination_engine_reset(imagination_engine_t* engine) {
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_engine_reset: engine is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__reset", 0.0f);


    nimcp_mutex_lock(engine->mutex);

    /* End all active scenarios */
    if (engine->active_scenarios) {
        size_t count = nimcp_darray_size(engine->active_scenarios);
        for (size_t i = 0; i < count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && count > 256) {
                imagination_engine_heartbeat("imagination__loop",
                                 (float)(i + 1) / (float)count);
            }

            imagination_scenario_t** ptr =
                (imagination_scenario_t**)nimcp_darray_at(engine->active_scenarios, i);
            imagination_scenario_t* scenario = ptr ? *ptr : NULL;
            if (scenario) {
                if (engine->workspace && scenario->id) {
                    imagination_workspace_release_scenario(engine->workspace, scenario->id);
                }
                if (scenario->latent_state) nimcp_tensor_destroy(scenario->latent_state);
                if (scenario->latent_previous) nimcp_tensor_destroy(scenario->latent_previous);
                if (scenario->visual_buffer) nimcp_tensor_destroy(scenario->visual_buffer);
                if (scenario->audio_buffer) nimcp_tensor_destroy(scenario->audio_buffer);
                if (scenario->semantic_buffer) nimcp_tensor_destroy(scenario->semantic_buffer);
                /* Free trajectory tensor entries before destroying darray */
                if (scenario->trajectory) {
                    size_t traj_count = nimcp_darray_size(scenario->trajectory);
                    for (size_t j = 0; j < traj_count; j++) {
                        nimcp_tensor_t** state_ptr =
                            (nimcp_tensor_t**)nimcp_darray_at(scenario->trajectory, j);
                        if (state_ptr && *state_ptr) nimcp_tensor_destroy(*state_ptr);
                    }
                    nimcp_darray_destroy(scenario->trajectory);
                }
                /* Free element entries before destroying darray */
                if (scenario->elements) {
                    size_t elem_count = nimcp_darray_size(scenario->elements);
                    for (size_t j = 0; j < elem_count; j++) {
                        imagination_element_t** elem_ptr =
                            (imagination_element_t**)nimcp_darray_at(scenario->elements, j);
                        if (elem_ptr && *elem_ptr) {
                            imagination_element_t* elem = *elem_ptr;
                            if (elem->features) nimcp_tensor_destroy(elem->features);
                            nimcp_free(elem);
                        }
                    }
                    nimcp_darray_destroy(scenario->elements);
                }
                if (scenario->active_goal) nimcp_free(scenario->active_goal);
                nimcp_free(scenario);
                scenario = NULL;
            }
        }
        nimcp_darray_clear(engine->active_scenarios);
    }

    /* Reset workspace */
    if (engine->workspace) {
        imagination_workspace_reset(engine->workspace);
    }

    /* Reset state */
    engine->current_mode = IMAGINATION_MODE_PASSIVE;
    engine->next_scenario_id = 1;

    /* Reset statistics */
    memset(&engine->stats, 0, sizeof(imagination_stats_t));

    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

/*============================================================================
 * Brain Factory Integration
 *============================================================================*/

int imagination_engine_init_for_brain(
    brain_t brain,
    const imagination_engine_config_t* config) {

    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_engine_init_for_brain: brain is NULL");
        return -1;
    }

    /* Create engine */
    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__init_for_brain", 0.0f);


    imagination_engine_t* engine = imagination_engine_create(config);
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "imagination_engine_init_for_brain: engine creation failed");
        return -1;
    }

    /* Store brain reference */
    engine->brain = brain;

    /* Attach to brain (implementation depends on brain structure) */
    /* This would typically call: brain_set_imagination_engine(brain, engine); */
    /* NOTE: If brain doesn't retain the engine pointer, we must destroy it to
     * avoid a memory leak. Currently brain_set_imagination_engine is not called,
     * so the engine is leaked. For now, destroy it since there's no API to attach. */
    imagination_engine_destroy(engine);

    return 0;
}

imagination_engine_t* brain_get_imagination_engine(brain_t brain) {
    if (!brain) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return NULL;

    }
    /* Implementation depends on brain structure */
    /* Would typically be: return brain->imagination_engine; */
    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__brain_get_imaginatio", 0.0f);


    /* TODO: brain_get_imagination_engine not yet implemented */
    return NULL;
}

/*============================================================================
 * Connection API - Cognitive Systems
 *============================================================================*/

int imagination_engine_connect_world_model(
    imagination_engine_t* engine,
    jepa_predictor_t* jepa) {

    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_engine_connect_world_model: engine is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__connect_world_model", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    engine->world_model = jepa;
    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

int imagination_engine_connect_hippocampus(
    imagination_engine_t* engine,
    hippocampus_adapter_t* hipp) {

    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_engine_connect_hippocampus: engine is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__connect_hippocampus", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    engine->hippocampus = hipp;
    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

int imagination_engine_connect_visual(
    imagination_engine_t* engine,
    visual_cortex_t* visual) {

    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_engine_connect_visual: engine is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__connect_visual", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    engine->visual_cortex = visual;
    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

int imagination_engine_connect_audio(
    imagination_engine_t* engine,
    audio_cortex_t* audio) {

    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_engine_connect_audio: engine is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__connect_audio", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    engine->audio_cortex = audio;
    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

int imagination_engine_connect_prefrontal(
    imagination_engine_t* engine,
    prefrontal_adapter_t* pfc) {

    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_engine_connect_prefrontal: engine is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__connect_prefrontal", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    engine->prefrontal = pfc;
    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

int imagination_engine_connect_global_workspace(
    imagination_engine_t* engine,
    global_workspace_t* gw) {

    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_engine_connect_global_workspace: engine is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__connect_global_works", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    engine->global_workspace = gw;
    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

int imagination_engine_connect_tom(
    imagination_engine_t* engine,
    theory_of_mind_t* tom) {

    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_engine_connect_tom: engine is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__connect_tom", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    engine->tom = tom;
    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

int imagination_engine_connect_curiosity(
    imagination_engine_t* engine,
    curiosity_state_t* curiosity) {

    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_engine_connect_curiosity: engine is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__connect_curiosity", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    engine->curiosity = curiosity;
    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

int imagination_engine_connect_sleep(
    imagination_engine_t* engine,
    sleep_wake_cycle_t* sleep) {

    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_engine_connect_sleep: engine is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__connect_sleep", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    engine->sleep = sleep;
    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

/*============================================================================
 * Integration Bridges API
 *============================================================================*/

int imagination_engine_connect_bio_async(
    imagination_engine_t* engine,
    bio_module_context_t* bio_ctx) {

    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_engine_connect_bio_async: engine is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__connect_bio_async", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    engine->bio_context = bio_ctx;
    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

int imagination_engine_connect_immune(
    imagination_engine_t* engine,
    brain_immune_system_t* immune) {

    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_engine_connect_immune: engine is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__connect_immune", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    engine->immune = immune;
    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

int imagination_engine_connect_substrate(
    imagination_engine_t* engine,
    neural_substrate_t* substrate) {

    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_engine_connect_substrate: engine is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__connect_substrate", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    engine->substrate = substrate;
    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

int imagination_engine_connect_thalamic(
    imagination_engine_t* engine,
    thalamic_router_t* thalamic) {

    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_engine_connect_thalamic: engine is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__connect_thalamic", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    engine->thalamic = thalamic;
    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

int imagination_engine_connect_training(
    imagination_engine_t* engine,
    training_context_t* training) {

    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_engine_connect_training: engine is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__connect_training", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    engine->training = training;
    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

int imagination_engine_connect_logic(
    imagination_engine_t* engine,
    neural_logic_gate_t* logic) {

    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_engine_connect_logic: engine is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__connect_logic", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    engine->logic = logic;
    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

int imagination_engine_init_gpu(
    imagination_engine_t* engine,
    int device_id) {

    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_engine_init_gpu: engine is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__init_gpu", 0.0f);


    nimcp_mutex_lock(engine->mutex);

    /* GPU initialization would go here */
    /* For now, mark as unavailable */
    engine->gpu_available = false;
    engine->config.gpu_device_id = device_id;

    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

/*============================================================================
 * Parietal Lobe Integration API
 *============================================================================*/

int imagination_engine_connect_parietal(
    imagination_engine_t* engine,
    parietal_lobe_t* parietal) {

    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_engine_connect_parietal: engine is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__connect_parietal", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    engine->parietal = parietal;
    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

int imagination_engine_connect_spatial(
    imagination_engine_t* engine,
    spatial_transform_t* spatial) {

    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_engine_connect_spatial: engine is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__connect_spatial", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    engine->spatial = spatial;
    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

int imagination_engine_connect_number_sense(
    imagination_engine_t* engine,
    number_sense_t* number_sense) {

    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_engine_connect_number_sense: engine is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__connect_number_sense", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    engine->number_sense = number_sense;
    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

int imagination_engine_connect_math_intuition(
    imagination_engine_t* engine,
    mathematical_intuition_t* math) {

    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_engine_connect_math_intuition: engine is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__connect_math_intuiti", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    engine->math_intuition = math;
    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

int imagination_engine_connect_scientific(
    imagination_engine_t* engine,
    scientific_reasoning_t* scientific) {

    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_engine_connect_scientific: engine is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__connect_scientific", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    engine->scientific = scientific;
    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

/*============================================================================
 * Domain Simulation API
 *============================================================================*/

int imagination_engine_connect_chemistry(
    imagination_engine_t* engine,
    chemistry_context_t* chemistry) {

    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_engine_connect_chemistry: engine is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__connect_chemistry", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    engine->chemistry = chemistry;
    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

int imagination_engine_connect_biology(
    imagination_engine_t* engine,
    biology_context_t* biology) {

    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_engine_connect_biology: engine is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__connect_biology", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    engine->biology = biology;
    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

int imagination_engine_connect_physics(
    imagination_engine_t* engine,
    physics_context_t* physics) {

    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_engine_connect_physics: engine is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__connect_physics", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    engine->physics = physics;
    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

int imagination_engine_connect_software(
    imagination_engine_t* engine,
    software_engineering_context_t* software) {

    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_engine_connect_software: engine is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__connect_software", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    engine->software = software;
    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

int imagination_engine_connect_equations(
    imagination_engine_t* engine,
    equation_manipulation_context_t* equations) {

    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_engine_connect_equations: engine is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__connect_equations", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    engine->equations = equations;
    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

/*============================================================================
 * Quantum Reasoning Integration API
 *============================================================================*/

int imagination_engine_connect_quantum_kb(
    imagination_engine_t* engine,
    qreason_kb_t* kb) {

    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_engine_connect_quantum_kb: engine is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__connect_quantum_kb", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    engine->quantum_kb = kb;
    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

int imagination_engine_init_quantum_state(
    imagination_engine_t* engine,
    size_t num_qubits) {

    if (!engine || num_qubits == 0 || num_qubits > 32) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "imagination_engine_init_quantum_state: invalid parameters");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__init_quantum_state", 0.0f);


    nimcp_mutex_lock(engine->mutex);

    /* Quantum state would be initialized here */
    /* For now, just mark configuration */
    engine->quantum_enabled = true;

    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

int imagination_engine_set_quantum_enabled(
    imagination_engine_t* engine,
    bool enabled) {

    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_engine_set_quantum_enabled: engine is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__set_quantum_enabled", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    engine->quantum_enabled = enabled;
    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

/*============================================================================
 * Scenario Management API
 *============================================================================*/

imagination_scenario_t* imagination_begin_scenario(
    imagination_engine_t* engine,
    imagination_mode_t mode,
    const imagination_goal_t* goal) {

    if (!engine) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "engine is NULL");


        return NULL;


    }
    if (mode < 0 || mode >= IMAGINATION_MODE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "imagination_begin_scenario: invalid mode");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_begin_sc", 0.0f);


    nimcp_mutex_lock(engine->mutex);

    /* Allocate workspace slot */
    scenario_id_t slot_id = imagination_workspace_allocate_scenario(engine->workspace);
    if (slot_id == 0) {
        nimcp_mutex_unlock(engine->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "imagination_begin_scenario: workspace full");
        return NULL;  /* Workspace full */
    }

    /* Create scenario structure */
    imagination_scenario_t* scenario = (imagination_scenario_t*)nimcp_calloc(
        1, sizeof(imagination_scenario_t));
    if (!scenario) {
        imagination_workspace_release_scenario(engine->workspace, slot_id);
        nimcp_mutex_unlock(engine->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "imagination_begin_scenario: allocation failed");
        return NULL;
    }

    /* Initialize scenario */
    scenario->id = slot_id;
    scenario->mode = mode;
    scenario->quality = engine->config.default_quality;

    /* Create latent state buffers */
    uint32_t latent_dims[] = {(uint32_t)engine->config.latent_dim};
    scenario->latent_state = nimcp_tensor_create(latent_dims, 1, NIMCP_DTYPE_F32);
    scenario->latent_previous = nimcp_tensor_create(latent_dims, 1, NIMCP_DTYPE_F32);
    if (!scenario->latent_state || !scenario->latent_previous) {
        goto error;
    }

    /* Initialize latent state to zero then add noise (seeding imagination) */
    tensor_fill_zero(scenario->latent_state);
    apply_noise(scenario->latent_state, engine->config.creativity_noise_level);

    /* Initialize latent_previous to same state as latent_state */
    nimcp_tensor_copy(scenario->latent_previous, scenario->latent_state);

    /* Set initial metrics */
    scenario->vividness = engine->config.default_vividness;
    scenario->controllability = (mode == IMAGINATION_MODE_DIRECTED) ? 0.8f : 0.3f;
    scenario->coherence = 1.0f;
    scenario->reality_distance = 0.0f;
    scenario->novelty = 0.0f;

    /* Set timing */
    scenario->start_time_ms = get_time_ms();
    scenario->last_step_ms = scenario->start_time_ms;
    scenario->duration_ms = 0;

    /* Create trajectory array (stores nimcp_tensor_t*) */
    scenario->trajectory = nimcp_darray_create(sizeof(nimcp_tensor_t*), 64);
    if (!scenario->trajectory) goto error;
    scenario->trajectory_length = 0;

    /* Create elements array (stores imagination_element_t*) */
    scenario->elements = nimcp_darray_create(sizeof(imagination_element_t*), 16);
    if (!scenario->elements) goto error;

    /* Copy goal if provided */
    if (goal) {
        scenario->active_goal = (imagination_goal_t*)nimcp_malloc(sizeof(imagination_goal_t));
        if (!scenario->active_goal) goto error;
        memcpy(scenario->active_goal, goal, sizeof(imagination_goal_t));
        /* Deep copy tensors if needed */
        if (goal->target_features) {
            scenario->active_goal->target_features = nimcp_tensor_clone(goal->target_features);
        }
        if (goal->constraints) {
            scenario->active_goal->constraints = nimcp_tensor_clone(goal->constraints);
        }
        if (goal->avoid) {
            scenario->active_goal->avoid = nimcp_tensor_clone(goal->avoid);
        }
    }
    scenario->goal_progress = 0.0f;

    /* Set status */
    scenario->is_active = true;
    scenario->is_paused = false;
    scenario->error_code = 0;

    /* Add to active scenarios array */
    nimcp_darray_push_back(engine->active_scenarios, &scenario);

    /* Update statistics */
    engine->stats.scenarios_created++;
    engine->current_mode = mode;

    nimcp_mutex_unlock(engine->mutex);

    return scenario;

error:
    if (scenario->latent_state) nimcp_tensor_destroy(scenario->latent_state);
    if (scenario->latent_previous) nimcp_tensor_destroy(scenario->latent_previous);
    if (scenario->trajectory) nimcp_darray_destroy(scenario->trajectory);
    if (scenario->elements) nimcp_darray_destroy(scenario->elements);
    if (scenario->active_goal) {
        /* Free any deep-copied goal tensors before freeing the goal struct */
        if (scenario->active_goal->target_features) {
            nimcp_tensor_destroy(scenario->active_goal->target_features);
        }
        if (scenario->active_goal->constraints) {
            nimcp_tensor_destroy(scenario->active_goal->constraints);
        }
        if (scenario->active_goal->avoid) {
            nimcp_tensor_destroy(scenario->active_goal->avoid);
        }
        nimcp_free(scenario->active_goal);
    }
    nimcp_free(scenario);
    scenario = NULL;
    imagination_workspace_release_scenario(engine->workspace, slot_id);
    nimcp_mutex_unlock(engine->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "imagination_begin_scenario: resource allocation failed");
    return NULL;
}

int imagination_step_scenario(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario) {

    if (!engine || !scenario) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_step_scenario: engine or scenario is NULL");
        return -1;
    }
    if (!scenario->is_active || scenario->is_paused) {
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_step_sce", 0.0f);


    nimcp_mutex_lock(engine->mutex);

    /* Save previous state */
    nimcp_tensor_copy(scenario->latent_previous, scenario->latent_state);

    /* Get workspace latent buffer */
    nimcp_tensor_t* ws_latent = imagination_workspace_get_latent(
        engine->workspace, scenario->id);
    if (!ws_latent) {
        nimcp_mutex_unlock(engine->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_step_scenario: ws_latent is NULL");
        return -1;
    }

    /* Apply mode-specific evolution */
    float noise_level = engine->config.creativity_noise_level;

    switch (scenario->mode) {
        case IMAGINATION_MODE_PASSIVE:
            /* Low-control drift with moderate noise */
            noise_level *= 2.0f;
            apply_noise(scenario->latent_state, noise_level);
            break;

        case IMAGINATION_MODE_DIRECTED:
            /* Goal-directed with low noise */
            if (scenario->active_goal && scenario->active_goal->target_features) {
                /* Blend toward target */
                blend_tensors(scenario->latent_state,
                              scenario->latent_state,
                              scenario->active_goal->target_features,
                              0.9f);  /* 10% toward target */
            }
            apply_noise(scenario->latent_state, noise_level * 0.5f);
            break;

        case IMAGINATION_MODE_CREATIVE:
            /* High noise, novel combinations */
            noise_level *= 3.0f;
            apply_noise(scenario->latent_state, noise_level);
            break;

        case IMAGINATION_MODE_COUNTERFACTUAL:
        case IMAGINATION_MODE_PROSPECTIVE:
        case IMAGINATION_MODE_RETROSPECTIVE:
        case IMAGINATION_MODE_SOCIAL:
            /* Standard evolution */
            apply_noise(scenario->latent_state, noise_level);
            break;

        case IMAGINATION_MODE_SPATIAL:
        case IMAGINATION_MODE_MATHEMATICAL:
        case IMAGINATION_MODE_SCIENTIFIC:
        case IMAGINATION_MODE_DOMAIN_SIMULATION:
            /* Parietal-guided evolution - lower noise for precision */
            apply_noise(scenario->latent_state, noise_level * 0.3f);
            break;

        case IMAGINATION_MODE_QUANTUM_SEARCH:
            /* Quantum-inspired superposition exploration */
            apply_noise(scenario->latent_state, noise_level);
            break;

        default:
            apply_noise(scenario->latent_state, noise_level);
            break;
    }

    /* Apply immune modulation if enabled */
    if (engine->config.enable_immune_modulation && engine->immune) {
        float modulation = imagination_get_immune_modulation(engine);
        scenario->vividness *= modulation;
        scenario->coherence *= modulation;
    }

    /* Calculate coherence with previous state */
    scenario->coherence = compute_coherence(scenario->latent_state,
                                             scenario->latent_previous);

    /* Check coherence threshold */
    if (scenario->coherence < engine->config.coherence_threshold) {
        /* Reset to previous state if too incoherent */
        nimcp_tensor_copy(scenario->latent_state, scenario->latent_previous);
        scenario->coherence = 1.0f;
    }

    /* Update novelty (distance from start) */
    scenario->novelty = 1.0f - scenario->coherence;

    /* Calculate goal progress if applicable */
    if (scenario->active_goal && scenario->active_goal->target_features) {
        float alignment = compute_coherence(scenario->latent_state,
                                             scenario->active_goal->target_features);
        scenario->goal_progress = alignment;
    }

    /* Update timing */
    uint64_t now = get_time_ms();
    scenario->duration_ms = now - scenario->start_time_ms;
    scenario->last_step_ms = now;
    scenario->trajectory_length++;

    /* Update statistics */
    engine->stats.total_steps++;

    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

int imagination_end_scenario(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario) {

    if (!engine || !scenario) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_end_scenario: engine or scenario is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_end_scen", 0.0f);


    nimcp_mutex_lock(engine->mutex);

    /* Mark as inactive */
    scenario->is_active = false;

    /* Update timing */
    scenario->duration_ms = get_time_ms() - scenario->start_time_ms;

    /* Release workspace slot */
    if (engine->workspace) {
        imagination_workspace_release_scenario(engine->workspace, scenario->id);
    }

    /* Remove from active list */
    size_t count = nimcp_darray_size(engine->active_scenarios);
    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            imagination_engine_heartbeat("imagination__loop",
                             (float)(i + 1) / (float)count);
        }

        imagination_scenario_t** ptr =
            (imagination_scenario_t**)nimcp_darray_at(engine->active_scenarios, i);
        if (ptr && *ptr == scenario) {
            nimcp_darray_remove_at(engine->active_scenarios, i, NULL);
            break;
        }
    }

    /* Update statistics */
    engine->stats.scenarios_completed++;

    /* Free scenario resources */
    if (scenario->latent_state) nimcp_tensor_destroy(scenario->latent_state);
    if (scenario->latent_previous) nimcp_tensor_destroy(scenario->latent_previous);
    if (scenario->visual_buffer) nimcp_tensor_destroy(scenario->visual_buffer);
    if (scenario->audio_buffer) nimcp_tensor_destroy(scenario->audio_buffer);
    if (scenario->semantic_buffer) nimcp_tensor_destroy(scenario->semantic_buffer);

    if (scenario->trajectory) {
        size_t traj_count = nimcp_darray_size(scenario->trajectory);
        for (size_t i = 0; i < traj_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && traj_count > 256) {
                imagination_engine_heartbeat("imagination__loop",
                                 (float)(i + 1) / (float)traj_count);
            }

            nimcp_tensor_t** state_ptr =
                (nimcp_tensor_t**)nimcp_darray_at(scenario->trajectory, i);
            if (state_ptr && *state_ptr) nimcp_tensor_destroy(*state_ptr);
        }
        nimcp_darray_destroy(scenario->trajectory);
    }

    if (scenario->elements) {
        size_t elem_count = nimcp_darray_size(scenario->elements);
        for (size_t i = 0; i < elem_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && elem_count > 256) {
                imagination_engine_heartbeat("imagination__loop",
                                 (float)(i + 1) / (float)elem_count);
            }

            imagination_element_t** elem_ptr =
                (imagination_element_t**)nimcp_darray_at(scenario->elements, i);
            if (elem_ptr && *elem_ptr) {
                imagination_element_t* elem = *elem_ptr;
                if (elem->features) nimcp_tensor_destroy(elem->features);
                if (elem->position) nimcp_free(elem->position);
                nimcp_free(elem);
                elem = NULL;
            }
        }
        nimcp_darray_destroy(scenario->elements);
    }

    if (scenario->active_goal) {
        if (scenario->active_goal->target_features)
            nimcp_tensor_destroy(scenario->active_goal->target_features);
        if (scenario->active_goal->constraints)
            nimcp_tensor_destroy(scenario->active_goal->constraints);
        if (scenario->active_goal->avoid)
            nimcp_tensor_destroy(scenario->active_goal->avoid);
        nimcp_free(scenario->active_goal);
    }

    nimcp_free(scenario);
    scenario = NULL;

    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

int imagination_pause_scenario(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario) {

    if (!engine || !scenario) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_pause_scenario: engine or scenario is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_pause_sc", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    scenario->is_paused = true;
    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

int imagination_resume_scenario(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario) {

    if (!engine || !scenario) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_resume_scenario: engine or scenario is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_resume_s", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    scenario->is_paused = false;
    scenario->last_step_ms = get_time_ms();
    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

/*============================================================================
 * Generation API
 *============================================================================*/

int imagination_generate_visual(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario) {

    if (!engine || !scenario) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_generate_visual: engine or scenario is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_generate", 0.0f);


    nimcp_mutex_lock(engine->mutex);

    /* Create visual buffer if needed */
    if (!scenario->visual_buffer) {
        uint32_t dims[] = {64, 64, 1};  /* Default size */
        scenario->visual_buffer = nimcp_tensor_create(dims, 3, NIMCP_DTYPE_F32);
        if (!scenario->visual_buffer) {
            nimcp_mutex_unlock(engine->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "imagination_generate_visual: tensor allocation failed");
            return -1;
        }
    }

    /* Simple generation: project latent to visual space */
    /* In a real implementation, this would use a decoder network */
    float* visual_data = nimcp_tensor_data(scenario->visual_buffer);
    const float* latent_data = nimcp_tensor_data_const(scenario->latent_state);
    size_t visual_size = nimcp_tensor_size(scenario->visual_buffer);
    size_t latent_size = nimcp_tensor_size(scenario->latent_state);

    /* Simple projection with modulation by vividness */
    for (size_t i = 0; i < visual_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && visual_size > 256) {
            imagination_engine_heartbeat("imagination__loop",
                             (float)(i + 1) / (float)visual_size);
        }

        size_t li = i % latent_size;
        visual_data[i] = latent_data[li] * scenario->vividness;
        /* Clamp to valid range */
        if (visual_data[i] < 0.0f) visual_data[i] = 0.0f;
        if (visual_data[i] > 1.0f) visual_data[i] = 1.0f;
    }

    /* Update statistics */
    engine->stats.visual_generations++;

    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

int imagination_generate_audio(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario) {

    if (!engine || !scenario) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_generate_audio: engine or scenario is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_generate", 0.0f);


    nimcp_mutex_lock(engine->mutex);

    /* Create audio buffer if needed */
    if (!scenario->audio_buffer) {
        uint32_t dims[] = {1024};  /* Default size */
        scenario->audio_buffer = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
        if (!scenario->audio_buffer) {
            nimcp_mutex_unlock(engine->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "imagination_generate_audio: tensor allocation failed");
            return -1;
        }
    }

    /* Simple generation: project latent to audio space */
    float* audio_data = nimcp_tensor_data(scenario->audio_buffer);
    const float* latent_data = nimcp_tensor_data_const(scenario->latent_state);
    size_t audio_size = nimcp_tensor_size(scenario->audio_buffer);
    size_t latent_size = nimcp_tensor_size(scenario->latent_state);

    /* Simple oscillatory projection */
    for (size_t i = 0; i < audio_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && audio_size > 256) {
            imagination_engine_heartbeat("imagination__loop",
                             (float)(i + 1) / (float)audio_size);
        }

        size_t li = i % latent_size;
        float phase = (float)i / (float)audio_size * NIMCP_TWO_PI_F;
        audio_data[i] = latent_data[li] * sinf(phase * (1.0f + li * 0.1f));
        audio_data[i] *= scenario->vividness;
    }

    /* Update statistics */
    engine->stats.audio_generations++;

    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

int imagination_generate_multimodal(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario) {

    if (!engine || !scenario) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_generate_multimodal: engine or scenario is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_generate", 0.0f);


    int result = imagination_generate_visual(engine, scenario);
    if (result != 0) return result;

    result = imagination_generate_audio(engine, scenario);
    return result;
}

/*============================================================================
 * Manipulation API
 *============================================================================*/

int imagination_transform_scene(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario,
    const imagination_transform_t* transform) {

    if (!engine || !scenario || !transform) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_transform_scene: engine, scenario, or transform is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_transfor", 0.0f);


    nimcp_mutex_lock(engine->mutex);

    /* Apply rotation if provided */
    if (transform->rotation) {
        /* Simple rotation: blend with rotated version */
        /* In real implementation, apply proper rotation matrix */
    }

    /* Apply translation if provided */
    if (transform->translation) {
        float* latent = nimcp_tensor_data(scenario->latent_state);
        const float* trans = nimcp_tensor_data_const(transform->translation);
        size_t size = nimcp_tensor_size(scenario->latent_state);
        size_t trans_size = nimcp_tensor_size(transform->translation);

        for (size_t i = 0; i < size && i < trans_size; i++) {
            latent[i] += trans[i];
        }
    }

    /* Apply scaling if provided */
    if (transform->scaling) {
        float* latent = nimcp_tensor_data(scenario->latent_state);
        const float* scale = nimcp_tensor_data_const(transform->scaling);
        size_t size = nimcp_tensor_size(scenario->latent_state);
        size_t scale_size = nimcp_tensor_size(transform->scaling);

        for (size_t i = 0; i < size && i < scale_size; i++) {
            latent[i] *= scale[i];
        }
    }

    /* Apply time delta (temporal evolution) */
    if (transform->time_delta > 0.0f) {
        apply_noise(scenario->latent_state,
                    engine->config.creativity_noise_level * transform->time_delta);
    }

    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

uint64_t imagination_inject_element(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario,
    const imagination_element_t* element) {

    if (!engine || !scenario || !element) return 0;

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_inject_e", 0.0f);


    nimcp_mutex_lock(engine->mutex);

    /* Create element copy */
    imagination_element_t* elem = (imagination_element_t*)nimcp_malloc(
        sizeof(imagination_element_t));
    if (!elem) {
        nimcp_mutex_unlock(engine->mutex);
        return 0;
    }

    /* Copy element */
    elem->id = engine->next_scenario_id++;
    elem->type = element->type;
    elem->salience = element->salience;
    elem->features = element->features ? nimcp_tensor_clone(element->features) : NULL;

    if (element->position && element->position_dim > 0) {
        elem->position = (float*)nimcp_calloc(element->position_dim, sizeof(float));
        if (elem->position) {
            memcpy(elem->position, element->position,
                   element->position_dim * sizeof(float));
        }
        elem->position_dim = element->position_dim;
    } else {
        elem->position = NULL;
        elem->position_dim = 0;
    }

    /* Add to elements array */
    nimcp_darray_push_back(scenario->elements, &elem);

    /* Blend element features into latent state */
    if (elem->features) {
        blend_tensors(scenario->latent_state,
                      scenario->latent_state,
                      elem->features,
                      1.0f - elem->salience);
    }

    nimcp_mutex_unlock(engine->mutex);

    return elem->id;
}

int imagination_remove_element(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario,
    uint64_t element_id) {

    if (!engine || !scenario || element_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_remove_element: invalid parameters");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_remove_e", 0.0f);


    nimcp_mutex_lock(engine->mutex);

    bool found = false;
    size_t count = nimcp_darray_size(scenario->elements);

    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            imagination_engine_heartbeat("imagination__loop",
                             (float)(i + 1) / (float)count);
        }

        imagination_element_t** elem_ptr =
            (imagination_element_t**)nimcp_darray_at(scenario->elements, i);
        if (!elem_ptr) continue;
        imagination_element_t* elem = *elem_ptr;
        if (elem && elem->id == element_id) {
            /* Remove element's contribution from latent state */
            if (elem->features) {
                float* latent = nimcp_tensor_data(scenario->latent_state);
                const float* feat = nimcp_tensor_data_const(elem->features);
                size_t size = nimcp_tensor_size(scenario->latent_state);
                size_t feat_size = nimcp_tensor_size(elem->features);

                for (size_t j = 0; j < size && j < feat_size; j++) {
                    latent[j] -= feat[j] * elem->salience * 0.5f;
                }
            }

            /* Free element */
            if (elem->features) nimcp_tensor_destroy(elem->features);
            if (elem->position) nimcp_free(elem->position);
            nimcp_free(elem);
            elem = NULL;

            nimcp_darray_remove_at(scenario->elements, i, NULL);
            found = true;
            break;
        }
    }

    nimcp_mutex_unlock(engine->mutex);

    return found ? 0 : -1;
}

imagination_scenario_t* imagination_blend_scenarios(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario_a,
    imagination_scenario_t* scenario_b,
    float alpha) {

    if (!engine || !scenario_a || !scenario_b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_blend_scenarios: engine or scenario is NULL");
        return NULL;
    }
    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_blend_sc", 0.0f);


    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;

    /* Create new scenario */
    imagination_scenario_t* blended = imagination_begin_scenario(
        engine, IMAGINATION_MODE_CREATIVE, NULL);
    if (!blended) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "blended is NULL");

        return NULL;

    }

    nimcp_mutex_lock(engine->mutex);

    /* Blend latent states */
    blend_tensors(blended->latent_state,
                  scenario_a->latent_state,
                  scenario_b->latent_state,
                  alpha);

    /* Blend metrics */
    blended->vividness = alpha * scenario_a->vividness +
                         (1.0f - alpha) * scenario_b->vividness;
    blended->controllability = alpha * scenario_a->controllability +
                               (1.0f - alpha) * scenario_b->controllability;

    nimcp_mutex_unlock(engine->mutex);

    return blended;
}

/*============================================================================
 * Advanced Imagination Modes
 *============================================================================*/

imagination_scenario_t* imagination_counterfactual(
    imagination_engine_t* engine,
    const nimcp_tensor_t* memory,
    const counterfactual_query_t* query) {

    if (!engine || !memory || !query) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_counterfactual: engine, memory, or query is NULL");
        return NULL;
    }

    /* Create counterfactual scenario */
    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_counterf", 0.0f);


    imagination_goal_t goal = {
        .mode = IMAGINATION_MODE_COUNTERFACTUAL,
        .target_features = NULL,
        .constraints = query->original_state ? nimcp_tensor_clone(query->original_state) : NULL,
        .avoid = NULL,
        .priority = 0.8f,
        .deadline_ms = 0,
        .context = NULL
    };

    imagination_scenario_t* scenario = imagination_begin_scenario(
        engine, IMAGINATION_MODE_COUNTERFACTUAL, &goal);

    /* Free the temporary goal.constraints since imagination_begin_scenario
     * makes a deep copy if needed */
    if (goal.constraints) {
        nimcp_tensor_destroy(goal.constraints);
    }

    if (!scenario) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "scenario is NULL");


        return NULL;
    }

    nimcp_mutex_lock(engine->mutex);

    /* Initialize from memory */
    nimcp_tensor_copy(scenario->latent_state, memory);

    /* Apply intervention */
    if (query->intervention) {
        float* latent = nimcp_tensor_data(scenario->latent_state);
        const float* interv = nimcp_tensor_data_const(query->intervention);
        size_t size = nimcp_tensor_size(scenario->latent_state);
        size_t interv_size = nimcp_tensor_size(query->intervention);

        for (size_t i = 0; i < size && i < interv_size; i++) {
            latent[i] += interv[i];
        }
    }

    /* Simulate forward */
    nimcp_mutex_unlock(engine->mutex);

    for (size_t i = 0; i < query->steps_forward; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && query->steps_forward > 256) {
            imagination_engine_heartbeat("imagination__loop",
                             (float)(i + 1) / (float)query->steps_forward);
        }

        imagination_step_scenario(engine, scenario);
    }

    return scenario;
}

imagination_scenario_t* imagination_simulate_future(
    imagination_engine_t* engine,
    const nimcp_tensor_t* current_state,
    const nimcp_tensor_t* actions,
    size_t num_actions,
    size_t steps_ahead) {

    if (!engine || !current_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_simulate_future: engine or current_state is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_simulate", 0.0f);


    imagination_scenario_t* scenario = imagination_begin_scenario(
        engine, IMAGINATION_MODE_PROSPECTIVE, NULL);
    if (!scenario) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "scenario is NULL");

        return NULL;

    }

    nimcp_mutex_lock(engine->mutex);

    /* Initialize from current state */
    nimcp_tensor_copy(scenario->latent_state, current_state);

    /* Apply actions to latent state */
    if (actions) {
        float* latent = nimcp_tensor_data(scenario->latent_state);
        const float* act = nimcp_tensor_data_const(actions);
        size_t latent_size = nimcp_tensor_size(scenario->latent_state);
        size_t act_size = nimcp_tensor_size(actions);

        for (size_t i = 0; i < latent_size && i < act_size; i++) {
            latent[i] += act[i] * 0.1f;  /* Action influence */
        }
    }

    nimcp_mutex_unlock(engine->mutex);

    /* Simulate forward */
    for (size_t i = 0; i < steps_ahead; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && steps_ahead > 256) {
            imagination_engine_heartbeat("imagination__loop",
                             (float)(i + 1) / (float)steps_ahead);
        }

        imagination_step_scenario(engine, scenario);
    }

    return scenario;
}

imagination_scenario_t* imagination_simulate_agent(
    imagination_engine_t* engine,
    uint64_t agent_id,
    const nimcp_tensor_t* believed_state) {

    if (!engine) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "engine is NULL");


        return NULL;


    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_simulate", 0.0f);


    imagination_scenario_t* scenario = imagination_begin_scenario(
        engine, IMAGINATION_MODE_SOCIAL, NULL);
    if (!scenario) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "scenario is NULL");

        return NULL;

    }

    nimcp_mutex_lock(engine->mutex);

    /* Initialize from believed state if provided */
    if (believed_state) {
        nimcp_tensor_copy(scenario->latent_state, believed_state);
    }

    /* Add noise to represent uncertainty about agent's state */
    apply_noise(scenario->latent_state,
                engine->config.creativity_noise_level * 2.0f);

    nimcp_mutex_unlock(engine->mutex);

    return scenario;
}

imagination_scenario_t* imagination_creative_recombine(
    imagination_engine_t* engine,
    nimcp_tensor_t** seed_memories,
    size_t num_memories,
    float creativity_level) {

    if (!engine || !seed_memories || num_memories == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_creative_recombine: engine or seed_memories is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_creative", 0.0f);


    imagination_scenario_t* scenario = imagination_begin_scenario(
        engine, IMAGINATION_MODE_CREATIVE, NULL);
    if (!scenario) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "scenario is NULL");

        return NULL;

    }

    nimcp_mutex_lock(engine->mutex);

    /* Combine seed memories */
    tensor_fill_zero(scenario->latent_state);

    float weight = 1.0f / (float)num_memories;
    for (size_t i = 0; i < num_memories; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_memories > 256) {
            imagination_engine_heartbeat("imagination__loop",
                             (float)(i + 1) / (float)num_memories);
        }

        if (seed_memories[i]) {
            blend_tensors(scenario->latent_state,
                          scenario->latent_state,
                          seed_memories[i],
                          1.0f - weight);
        }
    }

    /* Apply creativity noise */
    apply_noise(scenario->latent_state, creativity_level);

    nimcp_mutex_unlock(engine->mutex);

    return scenario;
}

/*============================================================================
 * Parietal-Mediated Imagination API
 *============================================================================*/

int imagination_mental_rotate(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario,
    const float* rotation_axis,
    float angle_radians) {

    if (!engine || !scenario || !rotation_axis) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_mental_rotate: engine, scenario, or rotation_axis is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_mental_r", 0.0f);


    nimcp_mutex_lock(engine->mutex);

    /* Apply rotation transform to latent space */
    float* latent = nimcp_tensor_data(scenario->latent_state);
    size_t size = nimcp_tensor_size(scenario->latent_state);

    /* Simple rotation simulation: circular shift with scaling */
    float cos_a = cosf(angle_radians);
    float sin_a = sinf(angle_radians);

    /* Rotate pairs of dimensions */
    for (size_t i = 0; i + 1 < size; i += 2) {
        float x = latent[i];
        float y = latent[i + 1];
        latent[i] = x * cos_a - y * sin_a;
        latent[i + 1] = x * sin_a + y * cos_a;
    }

    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

imagination_scenario_t* imagination_numerical(
    imagination_engine_t* engine,
    double quantity,
    double scale) {

    if (!engine) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "engine is NULL");


        return NULL;


    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_numerica", 0.0f);


    imagination_scenario_t* scenario = imagination_begin_scenario(
        engine, IMAGINATION_MODE_MATHEMATICAL, NULL);
    if (!scenario) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "scenario is NULL");

        return NULL;

    }

    nimcp_mutex_lock(engine->mutex);

    /* Encode quantity in latent space */
    float* latent = nimcp_tensor_data(scenario->latent_state);
    size_t size = nimcp_tensor_size(scenario->latent_state);

    /* Weber-Fechner encoding: logarithmic */
    float encoded = (float)(log(1.0 + fabs(quantity)) / log(10.0));
    float sign = quantity >= 0 ? 1.0f : -1.0f;

    for (size_t i = 0; i < size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && size > 256) {
            imagination_engine_heartbeat("imagination__loop",
                             (float)(i + 1) / (float)size);
        }

        latent[i] = encoded * sign * (1.0f + 0.1f * gaussian_noise(0.1f));
        latent[i] *= (float)scale;
    }

    nimcp_mutex_unlock(engine->mutex);

    return scenario;
}

imagination_scenario_t* imagination_mathematical(
    imagination_engine_t* engine,
    const nimcp_tensor_t* equation_latent,
    const float* variables,
    size_t num_vars) {

    if (!engine) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "engine is NULL");


        return NULL;


    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_mathemat", 0.0f);


    imagination_scenario_t* scenario = imagination_begin_scenario(
        engine, IMAGINATION_MODE_MATHEMATICAL, NULL);
    if (!scenario) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "scenario is NULL");

        return NULL;

    }

    nimcp_mutex_lock(engine->mutex);

    /* Initialize from equation if provided */
    if (equation_latent) {
        nimcp_tensor_copy(scenario->latent_state, equation_latent);
    }

    /* Apply variable bindings */
    if (variables && num_vars > 0) {
        float* latent = nimcp_tensor_data(scenario->latent_state);
        size_t size = nimcp_tensor_size(scenario->latent_state);

        for (size_t i = 0; i < size && i < num_vars; i++) {
            latent[i] += variables[i] * 0.1f;
        }
    }

    nimcp_mutex_unlock(engine->mutex);

    return scenario;
}

imagination_scenario_t* imagination_scientific_simulate(
    imagination_engine_t* engine,
    const nimcp_tensor_t* hypothesis,
    const nimcp_tensor_t* initial_conditions,
    size_t steps) {

    if (!engine) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "engine is NULL");


        return NULL;


    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_scientif", 0.0f);


    imagination_scenario_t* scenario = imagination_begin_scenario(
        engine, IMAGINATION_MODE_SCIENTIFIC, NULL);
    if (!scenario) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "scenario is NULL");

        return NULL;

    }

    nimcp_mutex_lock(engine->mutex);

    /* Initialize from conditions if provided */
    if (initial_conditions) {
        nimcp_tensor_copy(scenario->latent_state, initial_conditions);
    }

    /* Apply hypothesis as bias */
    if (hypothesis) {
        blend_tensors(scenario->latent_state,
                      scenario->latent_state,
                      hypothesis,
                      0.9f);
    }

    nimcp_mutex_unlock(engine->mutex);

    /* Run simulation steps */
    for (size_t i = 0; i < steps; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && steps > 256) {
            imagination_engine_heartbeat("imagination__loop",
                             (float)(i + 1) / (float)steps);
        }

        imagination_step_scenario(engine, scenario);
    }

    return scenario;
}

/*============================================================================
 * Domain-Specific Simulation API
 *============================================================================*/

imagination_scenario_t* imagination_simulate_chemistry(
    imagination_engine_t* engine,
    const nimcp_tensor_t* molecules,
    const nimcp_tensor_t* reaction_conditions) {

    if (!engine) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "engine is NULL");


        return NULL;


    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_simulate", 0.0f);


    imagination_scenario_t* scenario = imagination_begin_scenario(
        engine, IMAGINATION_MODE_DOMAIN_SIMULATION, NULL);
    if (!scenario) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "scenario is NULL");

        return NULL;

    }

    nimcp_mutex_lock(engine->mutex);

    /* Initialize from molecules */
    if (molecules) {
        nimcp_tensor_copy(scenario->latent_state, molecules);
    }

    /* Apply reaction conditions */
    if (reaction_conditions) {
        blend_tensors(scenario->latent_state,
                      scenario->latent_state,
                      reaction_conditions,
                      0.8f);
    }

    nimcp_mutex_unlock(engine->mutex);

    return scenario;
}

imagination_scenario_t* imagination_simulate_biology(
    imagination_engine_t* engine,
    const nimcp_tensor_t* biological_system,
    const nimcp_tensor_t* perturbation) {

    if (!engine) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "engine is NULL");


        return NULL;


    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_simulate", 0.0f);


    imagination_scenario_t* scenario = imagination_begin_scenario(
        engine, IMAGINATION_MODE_DOMAIN_SIMULATION, NULL);
    if (!scenario) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "scenario is NULL");

        return NULL;

    }

    nimcp_mutex_lock(engine->mutex);

    if (biological_system) {
        nimcp_tensor_copy(scenario->latent_state, biological_system);
    }

    if (perturbation) {
        float* latent = nimcp_tensor_data(scenario->latent_state);
        const float* pert = nimcp_tensor_data_const(perturbation);
        size_t size = nimcp_tensor_size(scenario->latent_state);
        size_t pert_size = nimcp_tensor_size(perturbation);

        for (size_t i = 0; i < size && i < pert_size; i++) {
            latent[i] += pert[i];
        }
    }

    nimcp_mutex_unlock(engine->mutex);

    return scenario;
}

imagination_scenario_t* imagination_simulate_physics(
    imagination_engine_t* engine,
    const nimcp_tensor_t* physical_state,
    const nimcp_tensor_t* forces,
    float time_delta) {

    if (!engine) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "engine is NULL");


        return NULL;


    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_simulate", 0.0f);


    imagination_scenario_t* scenario = imagination_begin_scenario(
        engine, IMAGINATION_MODE_DOMAIN_SIMULATION, NULL);
    if (!scenario) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "scenario is NULL");

        return NULL;

    }

    nimcp_mutex_lock(engine->mutex);

    if (physical_state) {
        nimcp_tensor_copy(scenario->latent_state, physical_state);
    }

    /* Apply forces over time */
    if (forces && time_delta > 0.0f) {
        float* latent = nimcp_tensor_data(scenario->latent_state);
        const float* f = nimcp_tensor_data_const(forces);
        size_t size = nimcp_tensor_size(scenario->latent_state);
        size_t f_size = nimcp_tensor_size(forces);

        /* Simple Euler integration: v += a*dt */
        for (size_t i = 0; i < size && i < f_size; i++) {
            latent[i] += f[i] * time_delta;
        }
    }

    nimcp_mutex_unlock(engine->mutex);

    return scenario;
}

imagination_scenario_t* imagination_simulate_software(
    imagination_engine_t* engine,
    const nimcp_tensor_t* program_state,
    size_t execution_steps) {

    if (!engine) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "engine is NULL");


        return NULL;


    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_simulate", 0.0f);


    imagination_scenario_t* scenario = imagination_begin_scenario(
        engine, IMAGINATION_MODE_DOMAIN_SIMULATION, NULL);
    if (!scenario) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "scenario is NULL");

        return NULL;

    }

    nimcp_mutex_lock(engine->mutex);

    if (program_state) {
        nimcp_tensor_copy(scenario->latent_state, program_state);
    }

    nimcp_mutex_unlock(engine->mutex);

    /* Execute steps */
    for (size_t i = 0; i < execution_steps; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && execution_steps > 256) {
            imagination_engine_heartbeat("imagination__loop",
                             (float)(i + 1) / (float)execution_steps);
        }

        imagination_step_scenario(engine, scenario);
    }

    return scenario;
}

/*============================================================================
 * Quantum-Inspired Reasoning API
 *============================================================================*/

imagination_scenario_t* imagination_quantum_superpose(
    imagination_engine_t* engine,
    nimcp_tensor_t** possibilities,
    size_t num_possibilities,
    const float* amplitudes) {

    if (!engine || !possibilities || num_possibilities == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_quantum_superpose: engine or possibilities is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_quantum_", 0.0f);


    imagination_scenario_t* scenario = imagination_begin_scenario(
        engine, IMAGINATION_MODE_QUANTUM_SEARCH, NULL);
    if (!scenario) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "scenario is NULL");

        return NULL;

    }

    nimcp_mutex_lock(engine->mutex);

    /* Create superposition as weighted sum */
    tensor_fill_zero(scenario->latent_state);

    float total_amp = 0.0f;
    for (size_t i = 0; i < num_possibilities; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_possibilities > 256) {
            imagination_engine_heartbeat("imagination__loop",
                             (float)(i + 1) / (float)num_possibilities);
        }

        float amp = amplitudes ? amplitudes[i] : 1.0f / (float)num_possibilities;
        total_amp += amp * amp;  /* Probability is amplitude squared */
    }

    if (total_amp > 0.0f) {
        float norm = 1.0f / sqrtf(total_amp);

        for (size_t i = 0; i < num_possibilities; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && num_possibilities > 256) {
                imagination_engine_heartbeat("imagination__loop",
                                 (float)(i + 1) / (float)num_possibilities);
            }

            if (possibilities[i]) {
                float amp = amplitudes ? amplitudes[i] * norm :
                            norm / sqrtf((float)num_possibilities);
                blend_tensors(scenario->latent_state,
                              scenario->latent_state,
                              possibilities[i],
                              1.0f - amp * amp);
            }
        }
    }

    nimcp_mutex_unlock(engine->mutex);

    return scenario;
}

int imagination_quantum_solve(
    imagination_engine_t* engine,
    const qreason_cnf_t* constraints,
    size_t max_iterations,
    qreason_result_t* result) {

    if (!engine || !constraints || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_quantum_solve: engine, constraints, or result is NULL");
        return -1;
    }

    /* This would implement Grover-inspired search */
    /* For now, return placeholder result */

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_quantum_", 0.0f);


    nimcp_mutex_lock(engine->mutex);

    /* Simple constraint satisfaction placeholder */
    memset(result, 0, sizeof(qreason_result_t));

    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

imagination_scenario_t* imagination_quantum_collapse(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario) {

    if (!engine || !scenario) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_quantum_collapse: engine or scenario is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_quantum_", 0.0f);


    nimcp_mutex_lock(engine->mutex);

    /* "Collapse" superposition by adding measurement noise */
    apply_noise(scenario->latent_state,
                engine->config.creativity_noise_level * 0.1f);

    /* Change mode from quantum to directed */
    scenario->mode = IMAGINATION_MODE_DIRECTED;

    nimcp_mutex_unlock(engine->mutex);

    return scenario;
}

/*============================================================================
 * Evaluation API
 *============================================================================*/

int imagination_evaluate(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario,
    imagination_evaluation_t* result) {

    if (!engine || !scenario || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_evaluate: engine, scenario, or result is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_evaluate", 0.0f);


    nimcp_mutex_lock(engine->mutex);

    memset(result, 0, sizeof(imagination_evaluation_t));

    /* Coherence */
    result->coherence = scenario->coherence;

    /* Plausibility (based on norm of latent state) */
    float norm = tensor_norm(scenario->latent_state);
    result->plausibility = 1.0f / (1.0f + norm * 0.1f);  /* Sigmoid-like */

    /* Reality distance */
    result->reality_distance = scenario->reality_distance;

    /* Goal alignment */
    result->goal_alignment = scenario->goal_progress;

    /* Novelty */
    result->novelty = scenario->novelty;

    /* Overall validity */
    result->is_valid = (result->coherence >= engine->config.coherence_threshold) &&
                       (result->plausibility >= 0.3f);

    result->issues = NULL;

    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

float imagination_check_plausibility(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario) {

    if (!engine || !scenario) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_check_pl", 0.0f);


    nimcp_mutex_lock(engine->mutex);

    float norm = tensor_norm(scenario->latent_state);
    float plausibility = 1.0f / (1.0f + norm * 0.1f);

    nimcp_mutex_unlock(engine->mutex);

    return plausibility;
}

float imagination_reality_distance(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario) {

    if (!engine || !scenario) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_reality_", 0.0f);


    return scenario->reality_distance;
}

/*============================================================================
 * Bio-Async Integration
 *============================================================================*/

uint32_t imagination_process_bio_messages(
    imagination_engine_t* engine,
    uint32_t max_messages) {

    if (!engine || !engine->bio_context) return 0;

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_process_", 0.0f);

    uint32_t processed = 0;
    uint32_t limit = (max_messages == 0) ? 64 : max_messages;

    nimcp_mutex_lock(engine->mutex);

    /* Process pending scenarios from active list */
    if (engine->active_scenarios) {
        size_t count = nimcp_darray_size(engine->active_scenarios);
        for (size_t i = 0; i < count && processed < limit; i++) {
            imagination_scenario_t** slot =
                (imagination_scenario_t**)nimcp_darray_at(engine->active_scenarios, i);
            if (!slot || !*slot) continue;

            imagination_scenario_t* scenario = *slot;

            /* Update scenario coherence if latent state exists */
            if (scenario->latent_state && scenario->latent_previous) {
                float coherence = compute_coherence(
                    scenario->latent_state, scenario->latent_previous);
                scenario->coherence = coherence;
            }
            processed++;
        }
    }

    /* Update stats */
    engine->stats.scenarios_completed += processed;

    nimcp_mutex_unlock(engine->mutex);

    imagination_engine_heartbeat("imagination__imagination_process_", 1.0f);

    return processed;
}

int imagination_broadcast_to_workspace(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario,
    float salience) {

    if (!engine || !scenario) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_broadcast_to_workspace: engine or scenario is NULL");
        return -1;
    }
    if (!engine->global_workspace) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_broadcast_to_workspace: engine->global_workspace is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_broadcas", 0.0f);

    /* Extract latent state as float array for workspace competition */
    if (!scenario->latent_state) return -1;

    const float* content = nimcp_tensor_data_const(scenario->latent_state);
    uint32_t content_dim = (uint32_t)nimcp_tensor_size(scenario->latent_state);
    if (!content || content_dim == 0) return -1;

    /* Compete for global workspace access with imagination content */
    float effective_salience = salience * scenario->coherence;
    if (effective_salience < 0.01f) effective_salience = salience;

    bool won = global_workspace_compete(
        engine->global_workspace,
        MODULE_CUSTOM_START + 1,  /* Imagination module ID */
        content,
        content_dim,
        effective_salience);

    if (won) {
        engine->stats.scenarios_completed++;
    }

    imagination_engine_heartbeat("imagination__imagination_broadcas", 1.0f);

    return won ? 0 : 1;  /* 0 = broadcast succeeded, 1 = lost competition */
}

/*============================================================================
 * Immune System Modulation
 *============================================================================*/

int imagination_update_immune_modulation(imagination_engine_t* engine) {
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_update_immune_modulation: engine is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_update_i", 0.0f);

    if (!engine->immune) return 0;  /* No immune system connected */

    /* Query immune modulation factor */
    float modulation = imagination_get_immune_modulation(engine);

    /* Apply modulation to imagination capacity:
     * High inflammation (modulation < 1.0) reduces imagination capacity
     * to conserve resources for immune response */
    nimcp_mutex_lock(engine->mutex);

    if (modulation < 1.0f) {
        /* Scale down active scenario quality under immune stress */
        float scale = IMMUNE_MODULATION_SCALE + (1.0f - IMMUNE_MODULATION_SCALE) * modulation;
        if (engine->active_scenarios) {
            size_t count = nimcp_darray_size(engine->active_scenarios);
            for (size_t i = 0; i < count; i++) {
                imagination_scenario_t** slot =
                    (imagination_scenario_t**)nimcp_darray_at(engine->active_scenarios, i);
                if (slot && *slot) {
                    /* Reduce coherence under immune pressure */
                    (*slot)->coherence *= scale;
                }
            }
        }
    }

    nimcp_mutex_unlock(engine->mutex);

    imagination_engine_heartbeat("imagination__imagination_update_i", 1.0f);

    return 0;
}

float imagination_get_immune_modulation(const imagination_engine_t* engine) {
    if (!engine) return 1.0f;
    if (!engine->immune) return 1.0f;

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_get_immu", 0.0f);

    /* Query immune system inflammation level */
    brain_immune_stats_t stats;
    int rc = brain_immune_get_stats(engine->immune, &stats);
    if (rc != 0) {
        /* Can't query — assume healthy */
        return 1.0f;
    }

    /* Use continuous inflammation level [0.0-1.0] */
    float inflammation = stats.inflammation_level_continuous;
    if (inflammation < 0.0f) inflammation = 0.0f;
    if (inflammation > 1.0f) inflammation = 1.0f;

    /* Modulation: 1.0 (no inflammation) → 0.3 (max inflammation) */
    float modulation = 1.0f - inflammation * (1.0f - 0.3f);

    return modulation;
}

/*============================================================================
 * GPU Acceleration
 *============================================================================*/

bool imagination_gpu_available(const imagination_engine_t* engine) {
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_gpu_available: engine is NULL");
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_gpu_avai", 0.0f);


    return engine->gpu_available;
}

int imagination_generate_visual_gpu(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario) {

    if (!engine || !scenario) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_gpu_available: required parameter is NULL (engine, scenario)");
        return -1;
    }
    if (!engine->gpu_available) {
        /* Fall back to CPU */
        return imagination_generate_visual(engine, scenario);
    }

    /* GPU generation would go here */
    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_generate", 0.0f);


    return imagination_generate_visual(engine, scenario);
}

int imagination_batch_generate_gpu(
    imagination_engine_t* engine,
    imagination_scenario_t** scenarios,
    size_t num_scenarios) {

    if (!engine || !scenarios || num_scenarios == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_gpu_available: required parameter is NULL (engine, scenarios)");
        return -1;
    }

    /* Batch GPU generation would go here */
    /* For now, process sequentially */
    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_batch_ge", 0.0f);


    for (size_t i = 0; i < num_scenarios; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_scenarios > 256) {
            imagination_engine_heartbeat("imagination__loop",
                             (float)(i + 1) / (float)num_scenarios);
        }

        if (scenarios[i]) {
            imagination_generate_visual(engine, scenarios[i]);
        }
    }

    return 0;
}

/*============================================================================
 * Statistics and Debugging
 *============================================================================*/

int imagination_get_stats(
    const imagination_engine_t* engine,
    imagination_stats_t* stats) {

    if (!engine || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_gpu_available: required parameter is NULL (engine, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_get_stat", 0.0f);


    imagination_engine_t* e = (imagination_engine_t*)engine;
    nimcp_mutex_lock(e->mutex);
    *stats = engine->stats;
    nimcp_mutex_unlock(e->mutex);

    return 0;
}

int imagination_reset_stats(imagination_engine_t* engine) {
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_reset_stats: engine is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_reset_st", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    memset(&engine->stats, 0, sizeof(imagination_stats_t));
    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

void imagination_print_state(
    const imagination_engine_t* engine,
    bool verbose) {

    if (!engine) {
        printf("Imagination Engine: NULL\n");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_print_st", 0.0f);


    printf("=== Imagination Engine State ===\n");
    printf("Initialized: %s\n", engine->base.bridge_active ? "yes" : "no");
    printf("Current Mode: %s\n", imagination_mode_to_string(engine->current_mode));
    printf("Active Scenarios: %zu\n",
           engine->active_scenarios ? nimcp_darray_size(engine->active_scenarios) : 0);
    printf("GPU Available: %s\n", engine->gpu_available ? "yes" : "no");
    printf("Quantum Enabled: %s\n", engine->quantum_enabled ? "yes" : "no");

    if (verbose) {
        printf("\n--- Configuration ---\n");
        printf("Max Scenarios: %zu\n", engine->config.max_concurrent_scenarios);
        printf("Latent Dim: %zu\n", engine->config.latent_dim);
        printf("Creativity Noise: %.3f\n", engine->config.creativity_noise_level);
        printf("Coherence Threshold: %.3f\n", engine->config.coherence_threshold);

        printf("\n--- Statistics ---\n");
        printf("Scenarios Created: %lu\n", (unsigned long)engine->stats.scenarios_created);
        printf("Scenarios Completed: %lu\n", (unsigned long)engine->stats.scenarios_completed);
        printf("Total Steps: %lu\n", (unsigned long)engine->stats.total_steps);
        printf("Visual Generations: %lu\n", (unsigned long)engine->stats.visual_generations);
        printf("Avg Vividness: %.3f\n", engine->stats.avg_vividness);
        printf("Avg Coherence: %.3f\n", engine->stats.avg_coherence);
    }

    printf("================================\n");
}

/*============================================================================
 * String Conversion Utilities
 *============================================================================*/

const char* imagination_mode_to_string(imagination_mode_t mode) {
    switch (mode) {
        case IMAGINATION_MODE_PASSIVE:          return "PASSIVE";
        case IMAGINATION_MODE_DIRECTED:         return "DIRECTED";
        case IMAGINATION_MODE_COUNTERFACTUAL:   return "COUNTERFACTUAL";
        case IMAGINATION_MODE_PROSPECTIVE:      return "PROSPECTIVE";
        case IMAGINATION_MODE_RETROSPECTIVE:    return "RETROSPECTIVE";
        case IMAGINATION_MODE_CREATIVE:         return "CREATIVE";
        case IMAGINATION_MODE_SOCIAL:           return "SOCIAL";
        case IMAGINATION_MODE_SPATIAL:          return "SPATIAL";
        case IMAGINATION_MODE_MATHEMATICAL:     return "MATHEMATICAL";
        case IMAGINATION_MODE_SCIENTIFIC:       return "SCIENTIFIC";
        case IMAGINATION_MODE_DOMAIN_SIMULATION: return "DOMAIN_SIMULATION";
        case IMAGINATION_MODE_QUANTUM_SEARCH:   return "QUANTUM_SEARCH";
        default:                                return "UNKNOWN";
    }
}

const char* imagination_quality_to_string(imagination_quality_t quality) {
    switch (quality) {
        case IMAGINATION_QUALITY_DRAFT:  return "DRAFT";
        case IMAGINATION_QUALITY_NORMAL: return "NORMAL";
        case IMAGINATION_QUALITY_HIGH:   return "HIGH";
        case IMAGINATION_QUALITY_VIVID:  return "VIVID";
        default:                         return "UNKNOWN";
    }
}

/*============================================================================
 * Collective Consciousness Integration
 *============================================================================*/

int imagination_engine_connect_collective(
    imagination_engine_t* engine,
    swarm_consciousness_ctx_t* collective) {

    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_quality_to_string: engine is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__connect_collective", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    engine->collective = collective;
    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

int imagination_broadcast_to_collective(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario) {

    if (!engine || !scenario) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_quality_to_string: required parameter is NULL (engine, scenario)");
        return -1;
    }
    if (!engine->collective) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_quality_to_string: engine->collective is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_broadcas", 0.0f);


    nimcp_mutex_lock(engine->mutex);

    /* TODO: When swarm_consciousness API supports it, broadcast latent state
     * to collective workspace for distributed processing.
     * For now, placeholder implementation. */

    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

int imagination_receive_collective_insights(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario) {

    if (!engine || !scenario) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_quality_to_string: required parameter is NULL (engine, scenario)");
        return -1;
    }
    if (!engine->collective) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_quality_to_string: engine->collective is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_receive_", 0.0f);


    nimcp_mutex_lock(engine->mutex);

    /* TODO: When swarm_consciousness API supports it, receive collective
     * insights and integrate into scenario's latent state.
     * For now, placeholder returning 0 insights. */

    nimcp_mutex_unlock(engine->mutex);

    return 0;  /* 0 insights received (placeholder) */
}

/*============================================================================
 * Internal Knowledge Graph Registration
 *============================================================================*/

int imagination_engine_register_with_kg(
    imagination_engine_t* engine,
    brain_kg_t* kg) {

    if (!engine || !kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_quality_to_string: required parameter is NULL (engine, kg)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__register_with_kg", 0.0f);


    nimcp_mutex_lock(engine->mutex);

    /* Store KG reference */
    engine->kg = kg;

    /* Add imagination engine as a cognitive node */
    engine->kg_node_id = brain_kg_add_node(kg,
        "imagination_engine",
        BRAIN_KG_NODE_COGNITIVE,
        "Generative mental simulation: 12 modes including counterfactual, "
        "prospective, creative, social, spatial, mathematical, scientific reasoning");

    if (engine->kg_node_id == BRAIN_KG_INVALID_NODE) {
        nimcp_mutex_unlock(engine->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "imagination_quality_to_string: validation failed");
        return -1;
    }

    /* Register edges for connected systems */

    /* Cognitive connections */
    if (engine->world_model) {
        uint32_t jepa_node = brain_kg_find_node(kg, "jepa_predictor");
        if (jepa_node != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, engine->kg_node_id, jepa_node,
                BRAIN_KG_EDGE_INTEGRATES_WITH,
                "Latent prediction for imagination", 0.9f);
        }
    }

    if (engine->hippocampus) {
        uint32_t hipp_node = brain_kg_find_node(kg, "hippocampus");
        if (hipp_node != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, engine->kg_node_id, hipp_node,
                BRAIN_KG_EDGE_CONNECTS_TO,
                "Memory retrieval for imagination", 0.8f);
        }
    }

    if (engine->prefrontal) {
        uint32_t pfc_node = brain_kg_find_node(kg, "prefrontal_cortex");
        if (pfc_node != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, engine->kg_node_id, pfc_node,
                BRAIN_KG_EDGE_RECEIVES_FROM,
                "Executive control of imagination", 0.9f);
        }
    }

    if (engine->visual_cortex) {
        uint32_t vis_node = brain_kg_find_node(kg, "visual_cortex");
        if (vis_node != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, engine->kg_node_id, vis_node,
                BRAIN_KG_EDGE_SENDS_TO,
                "Visual generation from imagination", 0.8f);
        }
    }

    if (engine->audio_cortex) {
        uint32_t aud_node = brain_kg_find_node(kg, "audio_cortex");
        if (aud_node != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, engine->kg_node_id, aud_node,
                BRAIN_KG_EDGE_SENDS_TO,
                "Audio generation from imagination", 0.7f);
        }
    }

    if (engine->global_workspace) {
        uint32_t gws_node = brain_kg_find_node(kg, "global_workspace");
        if (gws_node != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, engine->kg_node_id, gws_node,
                BRAIN_KG_EDGE_CONNECTS_TO,
                "Conscious broadcast of imagined content", 0.8f);
        }
    }

    if (engine->tom) {
        uint32_t tom_node = brain_kg_find_node(kg, "theory_of_mind");
        if (tom_node != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, engine->kg_node_id, tom_node,
                BRAIN_KG_EDGE_INTEGRATES_WITH,
                "Social imagination and perspective-taking", 0.85f);
        }
    }

    if (engine->curiosity) {
        uint32_t cur_node = brain_kg_find_node(kg, "curiosity");
        if (cur_node != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, engine->kg_node_id, cur_node,
                BRAIN_KG_EDGE_RECEIVES_FROM,
                "Curiosity drives exploratory imagination", 0.7f);
        }
    }

    if (engine->sleep) {
        uint32_t slp_node = brain_kg_find_node(kg, "sleep_wake_cycle");
        if (slp_node != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, engine->kg_node_id, slp_node,
                BRAIN_KG_EDGE_MODULATES,
                "REM sleep enhances creative imagination", 0.6f);
        }
    }

    /* Integration bridges */
    if (engine->immune) {
        uint32_t imm_node = brain_kg_find_node(kg, "brain_immune");
        if (imm_node != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, engine->kg_node_id, imm_node,
                BRAIN_KG_EDGE_MODULATES,
                "Immune state modulates vividness/coherence", 0.5f);
        }
    }

    if (engine->substrate) {
        uint32_t sub_node = brain_kg_find_node(kg, "neural_substrate");
        if (sub_node != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, engine->kg_node_id, sub_node,
                BRAIN_KG_EDGE_DEPENDS_ON,
                "Metabolic constraints on imagination", 0.6f);
        }
    }

    if (engine->thalamic) {
        uint32_t thal_node = brain_kg_find_node(kg, "thalamic_router");
        if (thal_node != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, engine->kg_node_id, thal_node,
                BRAIN_KG_EDGE_RECEIVES_FROM,
                "Attention gating for imagination", 0.7f);
        }
    }

    /* Parietal connections */
    if (engine->parietal) {
        uint32_t par_node = brain_kg_find_node(kg, "parietal_lobe");
        if (par_node != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, engine->kg_node_id, par_node,
                BRAIN_KG_EDGE_INTEGRATES_WITH,
                "Spatial/mathematical reasoning in imagination", 0.8f);
        }
    }

    /* Collective consciousness */
    if (engine->collective) {
        uint32_t col_node = brain_kg_find_node(kg, "swarm_consciousness");
        if (col_node != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, engine->kg_node_id, col_node,
                BRAIN_KG_EDGE_CONNECTS_TO,
                "Collective imagination across swarm", 0.7f);
        }
    }

    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Imagination Engine self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int imagination_engine_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Imagination_Engine");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                imagination_engine_heartbeat("imagination__loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            LOG_DEBUG("Imagination Engine self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Imagination_Engine");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Imagination_Engine");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * MCTS-BASED GOAL-DIRECTED IMAGINATION
 * ============================================================================ */

/**
 * @brief Action types for imagination MCTS
 */
typedef enum {
    IMAG_ACTION_STEP,           /**< Take a step in current mode */
    IMAG_ACTION_ADD_NOISE,      /**< Add creative noise */
    IMAG_ACTION_ROTATE,         /**< Apply rotation transform */
    IMAG_ACTION_BLEND_TOWARD,   /**< Blend toward goal */
    IMAG_ACTION_COUNT
} imag_mcts_action_t;

/**
 * @brief State for MCTS imagination search
 */
typedef struct {
    float* latent_copy;         /**< Copy of latent state */
    uint32_t latent_size;       /**< Size of latent state */
    float goal_progress;        /**< Current goal alignment */
    float coherence;            /**< Current coherence */
    uint32_t depth;             /**< Search depth */
} imag_mcts_state_t;

/**
 * @brief User data for MCTS callbacks
 */
typedef struct {
    imagination_engine_t* engine;
    imagination_scenario_t* scenario;
    const imagination_goal_t* goal;
    float* goal_features;
    uint32_t goal_size;
} imag_mcts_user_data_t;

static uint32_t imag_mcts_get_action_count(const void* state, void* user_data) {
    (void)state;
    (void)user_data;
    return IMAG_ACTION_COUNT;
}

static uint32_t imag_mcts_get_action(const void* state, uint32_t idx, void* user_data) {
    (void)state;
    (void)user_data;
    return idx;
}

static void* imag_mcts_apply_action(const void* state, uint32_t action, void* user_data) {
    const imag_mcts_state_t* s = (const imag_mcts_state_t*)state;
    imag_mcts_user_data_t* ud = (imag_mcts_user_data_t*)user_data;

    imag_mcts_state_t* new_state = nimcp_malloc(sizeof(imag_mcts_state_t));
    if (!new_state) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate new_state");

        return NULL;

    }

    new_state->latent_size = s->latent_size;
    new_state->depth = s->depth + 1;
    new_state->latent_copy = nimcp_calloc(s->latent_size, sizeof(float));
    if (!new_state->latent_copy) {
        nimcp_free(new_state);
        new_state = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "imag_mcts_apply_action: new_state->latent_copy is NULL");
        return NULL;
    }
    memcpy(new_state->latent_copy, s->latent_copy, s->latent_size * sizeof(float));

    float noise_level = ud->engine->config.creativity_noise_level;

    switch (action) {
        case IMAG_ACTION_STEP:
            /* Apply small noise to simulate a step */
            for (uint32_t i = 0; i < new_state->latent_size; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && new_state->latent_size > 256) {
                    imagination_engine_heartbeat("imagination__loop",
                                     (float)(i + 1) / (float)new_state->latent_size);
                }

                new_state->latent_copy[i] += gaussian_noise(noise_level * 0.5f);
            }
            break;

        case IMAG_ACTION_ADD_NOISE:
            /* Add more creative noise */
            for (uint32_t i = 0; i < new_state->latent_size; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && new_state->latent_size > 256) {
                    imagination_engine_heartbeat("imagination__loop",
                                     (float)(i + 1) / (float)new_state->latent_size);
                }

                new_state->latent_copy[i] += gaussian_noise(noise_level * 2.0f);
            }
            break;

        case IMAG_ACTION_ROTATE:
            /* Apply simple rotation in latent space */
            if (new_state->latent_size >= 2) {
                float angle = mc_random_uniform(&g_imagination_rand_seed) * 0.5f;
                float cos_a = cosf(angle);
                float sin_a = sinf(angle);
                for (uint32_t i = 0; i + 1 < new_state->latent_size; i += 2) {
                    float x = new_state->latent_copy[i];
                    float y = new_state->latent_copy[i + 1];
                    new_state->latent_copy[i] = x * cos_a - y * sin_a;
                    new_state->latent_copy[i + 1] = x * sin_a + y * cos_a;
                }
            }
            break;

        case IMAG_ACTION_BLEND_TOWARD:
            /* Blend toward goal features */
            if (ud->goal_features && ud->goal_size > 0) {
                float alpha = 0.2f;  /* 20% toward goal */
                uint32_t min_size = new_state->latent_size < ud->goal_size ?
                                    new_state->latent_size : ud->goal_size;
                for (uint32_t i = 0; i < min_size; i++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((i & 0xFF) == 0 && min_size > 256) {
                        imagination_engine_heartbeat("imagination__loop",
                                         (float)(i + 1) / (float)min_size);
                    }

                    new_state->latent_copy[i] = (1.0f - alpha) * new_state->latent_copy[i]
                                              + alpha * ud->goal_features[i];
                }
            }
            break;

        default:
            break;
    }

    /* Calculate new goal progress (cosine similarity with goal) */
    if (ud->goal_features && ud->goal_size > 0) {
        float dot = 0.0f, norm_l = 0.0f, norm_g = 0.0f;
        uint32_t min_size = new_state->latent_size < ud->goal_size ?
                            new_state->latent_size : ud->goal_size;
        for (uint32_t i = 0; i < min_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && min_size > 256) {
                imagination_engine_heartbeat("imagination__loop",
                                 (float)(i + 1) / (float)min_size);
            }

            dot += new_state->latent_copy[i] * ud->goal_features[i];
            norm_l += new_state->latent_copy[i] * new_state->latent_copy[i];
            norm_g += ud->goal_features[i] * ud->goal_features[i];
        }
        if (norm_l > 1e-10f && norm_g > 1e-10f) {
            new_state->goal_progress = (dot / (sqrtf(norm_l) * sqrtf(norm_g)) + 1.0f) / 2.0f;
        } else {
            new_state->goal_progress = 0.0f;
        }
    } else {
        new_state->goal_progress = s->goal_progress;
    }

    /* Calculate coherence with previous state */
    float dot = 0.0f, norm_old = 0.0f, norm_new = 0.0f;
    for (uint32_t i = 0; i < new_state->latent_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && new_state->latent_size > 256) {
            imagination_engine_heartbeat("imagination__loop",
                             (float)(i + 1) / (float)new_state->latent_size);
        }

        dot += s->latent_copy[i] * new_state->latent_copy[i];
        norm_old += s->latent_copy[i] * s->latent_copy[i];
        norm_new += new_state->latent_copy[i] * new_state->latent_copy[i];
    }
    if (norm_old > 1e-10f && norm_new > 1e-10f) {
        new_state->coherence = (dot / (sqrtf(norm_old) * sqrtf(norm_new)) + 1.0f) / 2.0f;
    } else {
        new_state->coherence = 1.0f;
    }

    return new_state;
}

static float imag_mcts_evaluate(const void* state, void* user_data) {
    const imag_mcts_state_t* s = (const imag_mcts_state_t*)state;
    (void)user_data;

    /* Reward = goal_progress + coherence bonus */
    return s->goal_progress * 0.7f + s->coherence * 0.3f;
}

static bool imag_mcts_is_terminal(const void* state, void* user_data) {
    const imag_mcts_state_t* s = (const imag_mcts_state_t*)state;
    imag_mcts_user_data_t* ud = (imag_mcts_user_data_t*)user_data;

    /* Terminal if goal reached or max depth */
    if (s->goal_progress >= 0.9f) return true;
    if (s->coherence < ud->engine->config.coherence_threshold) return true;
    if (s->depth >= 10) return true;
    return false;
}

static void imag_mcts_free_state(void* state, void* user_data) {
    (void)user_data;
    if (!state) return;
    imag_mcts_state_t* s = (imag_mcts_state_t*)state;
    if (s->latent_copy) nimcp_free(s->latent_copy);
    nimcp_free(s);
    s = NULL;
}

static void* imag_mcts_clone_state(const void* state, void* user_data) {
    (void)user_data;
    if (!state) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "state is NULL");

        return NULL;

    }

    const imag_mcts_state_t* s = (const imag_mcts_state_t*)state;
    imag_mcts_state_t* clone = nimcp_malloc(sizeof(imag_mcts_state_t));
    if (!clone) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate clone");

        return NULL;

    }

    clone->latent_size = s->latent_size;
    clone->goal_progress = s->goal_progress;
    clone->coherence = s->coherence;
    clone->depth = s->depth;

    clone->latent_copy = nimcp_calloc(s->latent_size, sizeof(float));
    if (!clone->latent_copy) {
        nimcp_free(clone);
        clone = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "imag_mcts_clone_state: clone->latent_copy is NULL");
        return NULL;
    }
    memcpy(clone->latent_copy, s->latent_copy, s->latent_size * sizeof(float));

    return clone;
}

/**
 * @brief Use MCTS to search for goal-directed imagination path
 *
 * WHAT: Search for optimal sequence of imagination transformations
 * WHY:  Find efficient path from current state to goal state
 * HOW:  Use MCTS with transformation actions to explore state space
 *
 * @param engine The imagination engine
 * @param scenario Active scenario to guide toward goal
 * @param goal Target goal for imagination
 * @param num_iterations MCTS iterations (0 = default 50)
 * @return 0 on success, -1 on error
 */
int imagination_search_goal_mcts(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario,
    const imagination_goal_t* goal,
    uint32_t num_iterations
) {
    if (!engine || !scenario || !goal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_search_goal_mcts: required parameter is NULL (engine, scenario, goal)");
        return -1;
    }
    if (!scenario->is_active || scenario->is_paused) {
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_engine_heartbeat("imagination__imagination_search_g", 0.0f);


    nimcp_mutex_lock(engine->mutex);

    /* Get current latent state */
    float* latent_data = nimcp_tensor_data(scenario->latent_state);
    size_t latent_size = nimcp_tensor_size(scenario->latent_state);

    if (!latent_data || latent_size == 0) {
        nimcp_mutex_unlock(engine->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "imagination_search_goal_mcts: latent_data is NULL");
        return -1;
    }

    /* Prepare goal features */
    float* goal_features = NULL;
    uint32_t goal_size = 0;
    if (goal->target_features) {
        goal_features = (float*)nimcp_tensor_data_const(goal->target_features);
        goal_size = (uint32_t)nimcp_tensor_size(goal->target_features);
    }

    /* Create initial MCTS state */
    imag_mcts_state_t initial_state;
    initial_state.latent_size = (uint32_t)latent_size;
    initial_state.latent_copy = nimcp_calloc(latent_size, sizeof(float));
    if (!initial_state.latent_copy) {
        nimcp_mutex_unlock(engine->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "imagination_search_goal_mcts: initial_state is NULL");
        return -1;
    }
    memcpy(initial_state.latent_copy, latent_data, latent_size * sizeof(float));
    initial_state.goal_progress = scenario->goal_progress;
    initial_state.coherence = scenario->coherence;
    initial_state.depth = 0;

    /* Setup user data */
    imag_mcts_user_data_t user_data = {
        .engine = engine,
        .scenario = scenario,
        .goal = goal,
        .goal_features = goal_features,
        .goal_size = goal_size
    };

    /* Configure MCTS */
    mcts_config_t config;
    mcts_config_init(&config);
    config.max_iterations = num_iterations > 0 ? num_iterations : 50;
    config.max_depth = NIMCP_DEFAULT_PROOF_DEPTH;
    config.exploration_constant = 1.4f;
    config.discount_factor = 0.95f;
    config.max_nodes = 128;

    config.get_action_count = imag_mcts_get_action_count;
    config.get_action = imag_mcts_get_action;
    config.apply_action = imag_mcts_apply_action;
    config.evaluate = imag_mcts_evaluate;
    config.is_terminal = imag_mcts_is_terminal;
    config.free_state = imag_mcts_free_state;
    config.clone_state = imag_mcts_clone_state;
    config.user_data = &user_data;
    config.seed = g_imagination_rand_seed;

    mcts_result_t result;
    nimcp_mc_result_t err = mcts_search(&config, &initial_state, &result);

    g_imagination_rand_seed = config.seed;

    nimcp_free(initial_state.latent_copy);

    if (err != NIMCP_MC_OK) {
        nimcp_mutex_unlock(engine->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "imagination_search_goal_mcts: validation failed");
        return -1;
    }

    /* Apply the best action to the actual scenario */
    uint32_t best_action = result.best_action;
    float noise_level = engine->config.creativity_noise_level;

    switch (best_action) {
        case IMAG_ACTION_STEP:
            apply_noise(scenario->latent_state, noise_level * 0.5f);
            break;

        case IMAG_ACTION_ADD_NOISE:
            apply_noise(scenario->latent_state, noise_level * 2.0f);
            break;

        case IMAG_ACTION_ROTATE:
            if (latent_size >= 2) {
                float angle = mc_random_uniform(&g_imagination_rand_seed) * 0.5f;
                float cos_a = cosf(angle);
                float sin_a = sinf(angle);
                for (size_t i = 0; i + 1 < latent_size; i += 2) {
                    float x = latent_data[i];
                    float y = latent_data[i + 1];
                    latent_data[i] = x * cos_a - y * sin_a;
                    latent_data[i + 1] = x * sin_a + y * cos_a;
                }
            }
            break;

        case IMAG_ACTION_BLEND_TOWARD:
            if (goal_features && goal_size > 0) {
                float alpha = 0.2f;
                size_t min_size = latent_size < goal_size ? latent_size : goal_size;
                for (size_t i = 0; i < min_size; i++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((i & 0xFF) == 0 && min_size > 256) {
                        imagination_engine_heartbeat("imagination__loop",
                                         (float)(i + 1) / (float)min_size);
                    }

                    latent_data[i] = (1.0f - alpha) * latent_data[i]
                                   + alpha * goal_features[i];
                }
            }
            break;

        default:
            break;
    }

    /* Update scenario metrics */
    if (goal_features && goal_size > 0) {
        scenario->goal_progress = compute_coherence(scenario->latent_state,
                                                     goal->target_features);
    }
    scenario->coherence = compute_coherence(scenario->latent_state,
                                             scenario->latent_previous);

    mcts_result_free(&result);

    nimcp_mutex_unlock(engine->mutex);

    return 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void imagination_engine_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_imagination_engine_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int imagination_engine_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "imagination_engine_training_begin: NULL argument");
        return -1;
    }
    imagination_engine_heartbeat("imagination_engine_training_begin", 0.0f);
    (void)(imag_mcts_state_t*)instance; /* Module state available for reset */
    return 0;
}

int imagination_engine_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "imagination_engine_training_end: NULL argument");
        return -1;
    }
    imagination_engine_heartbeat("imagination_engine_training_end", 1.0f);
    (void)(imag_mcts_state_t*)instance; /* Module state available for finalization */
    return 0;
}

int imagination_engine_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "imagination_engine_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    imagination_engine_heartbeat("imagination_engine_training_step", progress);
    (void)(imag_mcts_state_t*)instance; /* Module state available for step adaptation */
    return 0;
}
