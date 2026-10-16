/**
 * @file nimcp_rcog_answer.c
 * @brief Answer Refiner Implementation for Recursive Cognition
 *
 * WHAT: Implements RLM's "answer generation via diffusion" pattern
 * WHY:  Answer iteratively refined across reasoning steps until ready
 * HOW:  Latent state updated with evidence, confidence tracks readiness
 *
 * @author NIMCP Development Team
 * @date 2026-01-03
 * @version 1.0.0
 */

#include "cognitive/recursive/nimcp_rcog_answer.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_platform_time.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(rcog_answer, MESH_ADAPTER_CATEGORY_COGNITIVE)

void rcog_answer_set_instance_health_agent(void* ctx, nimcp_health_agent_t* agent) {
    (void)ctx;
    g_rcog_answer_instance_health_agent = agent;
}

/* ============================================================================
 * Phase 8: Instance-level Training Functions
 * ============================================================================ */

int rcog_answer_training_begin(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_answer_training_begin: NULL argument");
        return -1;
    }
    rcog_answer_heartbeat_instance(
        g_rcog_answer_instance_health_agent,
        "rcog_answer_training_begin", 0.0f);
    (void)ctx;
    return 0;
}

int rcog_answer_training_step(void* ctx, float progress) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_answer_training_step: NULL argument");
        return -1;
    }
    float clamped = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    rcog_answer_heartbeat_instance(
        g_rcog_answer_instance_health_agent,
        "rcog_answer_training_step", clamped);
    (void)ctx;
    return 0;
}

int rcog_answer_training_end(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_answer_training_end: NULL argument");
        return -1;
    }
    rcog_answer_heartbeat_instance(
        g_rcog_answer_instance_health_agent,
        "rcog_answer_training_end", 1.0f);
    (void)ctx;
    return 0;
}

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Answer refiner internal structure
 */
struct rcog_answer_refiner {
    /* Configuration */
    rcog_answer_config_t config;

    /* Momentum velocity storage (for momentum-based updates) */
    float* velocity;
    size_t velocity_dim;

    /* Statistics */
    uint64_t total_refinements;
    uint64_t total_answers;
    uint64_t answers_converged;
    uint64_t answers_max_steps;
    uint64_t answers_stalled;
    double total_steps;
    double total_confidence;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

/**
 * @brief Extended answer state with internal history
 */
typedef struct {
    rcog_answer_state_t base;
    rcog_answer_history_t* history;
    float* velocity;                /**< Per-state velocity for momentum */
} rcog_answer_state_internal_t;

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Get current time in milliseconds
 */
static uint64_t get_current_time_ms(void)
{
    return nimcp_platform_time_monotonic_ms();
}

/**
 * @brief Compute L2 norm of a vector
 */
static float vector_norm(const float* v, size_t dim)
{
    float sum = 0.0f;
    for (size_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            rcog_answer_heartbeat("rcog_answer_loop",
                             (float)(i + 1) / (float)dim);
        }

        sum += v[i] * v[i];
    }
    return sqrtf(sum);
}

/**
 * @brief Compute dot product of two vectors
 */
static float vector_dot(const float* a, const float* b, size_t dim)
{
    float sum = 0.0f;
    for (size_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            rcog_answer_heartbeat("rcog_answer_loop",
                             (float)(i + 1) / (float)dim);
        }

        sum += a[i] * b[i];
    }
    return sum;
}

/**
 * @brief Normalize a vector in place
 */
static void vector_normalize(float* v, size_t dim)
{
    float norm = vector_norm(v, dim);
    if (norm > 1e-8f) {
        for (size_t i = 0; i < dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && dim > 256) {
                rcog_answer_heartbeat("rcog_answer_loop",
                                 (float)(i + 1) / (float)dim);
            }

            v[i] /= norm;
        }
    }
}

/**
 * @brief Compute cosine similarity between two vectors
 */
static float cosine_similarity(const float* a, const float* b, size_t dim)
{
    float dot = vector_dot(a, b, dim);
    float norm_a = vector_norm(a, dim);
    float norm_b = vector_norm(b, dim);

    if (norm_a < 1e-8f || norm_b < 1e-8f) {
        return 0.0f;
    }

    return dot / (norm_a * norm_b);
}

/**
 * @brief Allocate and initialize latent vector
 */
static float* create_latent(size_t dim)
{
    float* latent = nimcp_calloc(dim, sizeof(float));
    return latent;
}

/**
 * @brief Add history entry
 */
static rcog_error_t add_history_entry(
    rcog_answer_history_t* history,
    uint32_t step,
    float confidence,
    float delta,
    size_t evidence_count)
{
    if (!history) return RCOG_OK;  /* History disabled */

    if (history->count >= history->capacity) {
        /* Grow history array */
        size_t new_capacity = history->capacity * 2;
        if (new_capacity == 0) new_capacity = 16;

        rcog_answer_history_entry_t* new_entries = nimcp_realloc(
            history->entries,
            new_capacity * sizeof(rcog_answer_history_entry_t)
        );
        if (!new_entries) return RCOG_ERROR_OUT_OF_MEMORY;

        history->entries = new_entries;
        history->capacity = new_capacity;
    }

    rcog_answer_history_entry_t* entry = &history->entries[history->count];
    entry->step = step;
    entry->confidence = confidence;
    entry->delta = delta;
    entry->timestamp_ms = get_current_time_ms();
    entry->evidence_count = evidence_count;

    history->count++;
    return RCOG_OK;
}

/**
 * @brief Create history structure
 */
static rcog_answer_history_t* create_history(size_t initial_capacity)
{
    rcog_answer_history_t* history = nimcp_calloc(1, sizeof(rcog_answer_history_t));
    if (!history) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate history");

        return NULL;

    }

    if (initial_capacity > 0) {
        history->entries = nimcp_calloc(initial_capacity, sizeof(rcog_answer_history_entry_t));
        if (!history->entries) {
            nimcp_free(history);
            history = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_history: history->entries is NULL");
            return NULL;
        }
        history->capacity = initial_capacity;
    }

    history->count = 0;
    return history;
}

/**
 * @brief Compute evidence direction from subtask results
 *
 * This is a simplified evidence integration. In a full implementation,
 * this would use JEPA or similar to project results into latent space.
 */
static rcog_error_t compute_evidence_direction(
    const rcog_subtask_result_t* evidence,
    size_t num_evidence,
    size_t latent_dim,
    float* direction)
{
    if (!evidence || num_evidence == 0 || !direction) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Initialize direction to zero */
    memset(direction, 0, latent_dim * sizeof(float));

    /* Simple aggregation: sum weighted by confidence */
    float total_weight = 0.0f;

    for (size_t i = 0; i < num_evidence; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_evidence > 256) {
            rcog_answer_heartbeat("rcog_answer_loop",
                             (float)(i + 1) / (float)num_evidence);
        }

        const rcog_subtask_result_t* e = &evidence[i];

        /* Accept evidence that is either completed or marked successful */
        if (e->status != RCOG_SUBTASK_COMPLETED && !e->success) continue;

        float weight = e->confidence;

        /* Prefer direct latent representation for answer refinement */
        if (e->latent && e->latent_dim > 0) {
            total_weight += weight;
            size_t dim_to_use = e->latent_dim < latent_dim ? e->latent_dim : latent_dim;
            for (size_t j = 0; j < dim_to_use; j++) {
                direction[j] += weight * e->latent[j];
            }
            continue;
        }

        /* Fall back to hashing result_data into latent dimensions */
        if (!e->result_data || e->result_size == 0) continue;

        total_weight += weight;

        /* Hash result data into latent dimensions */
        const uint8_t* data = (const uint8_t*)e->result_data;
        for (size_t j = 0; j < e->result_size && j < latent_dim; j++) {
            /* Simple hash-based projection */
            size_t idx = (j * 7919 + data[j]) % latent_dim;
            direction[idx] += weight * ((float)data[j] / 255.0f - 0.5f);
        }
    }

    /* Normalize by total weight */
    if (total_weight > 0.0f) {
        for (size_t i = 0; i < latent_dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && latent_dim > 256) {
                rcog_answer_heartbeat("rcog_answer_loop",
                                 (float)(i + 1) / (float)latent_dim);
            }

            direction[i] /= total_weight;
        }
    }

    /* Normalize direction vector */
    vector_normalize(direction, latent_dim);

    return RCOG_OK;
}

/**
 * @brief Compute confidence from latent state coherence
 */
static float compute_confidence(const float* latent, size_t dim)
{
    if (!latent || dim == 0) return 0.0f;

    /* Confidence based on latent vector magnitude and coherence */
    float magnitude = vector_norm(latent, dim);

    /* Normalize to [0, 1] using sigmoid-like function */
    float confidence = 2.0f / (1.0f + expf(-magnitude)) - 1.0f;

    return fmaxf(0.0f, fminf(1.0f, confidence));
}

//=============================================================================
// Public API Implementation
//=============================================================================

rcog_answer_config_t rcog_answer_default_config(void)
{
    /* Phase 8: Heartbeat at operation start */
    rcog_answer_heartbeat("rcog_answer_default_config", 0.0f);


    rcog_answer_config_t config = {0};

    config.latent_dim = RCOG_DEFAULT_LATENT_DIM;
    config.learning_rate = RCOG_DEFAULT_LEARNING_RATE;
    config.momentum = RCOG_DEFAULT_MOMENTUM;
    config.ready_threshold = RCOG_DEFAULT_READY_THRESHOLD;
    config.min_steps = 1;
    config.max_steps = RCOG_DEFAULT_MAX_REFINEMENT_STEPS;
    config.enable_early_stopping = true;
    config.convergence_epsilon = RCOG_DEFAULT_CONVERGENCE_EPSILON;
    config.enable_history = true;
    config.max_history_size = 64;

    return config;
}

rcog_answer_refiner_t* rcog_answer_refiner_create(
    const rcog_answer_config_t* config)
{
    /* Phase 8: Heartbeat at operation start */
    rcog_answer_heartbeat("rcog_answer_refiner_create", 0.0f);


    rcog_answer_refiner_t* refiner = nimcp_calloc(1, sizeof(rcog_answer_refiner_t));
    if (!refiner) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate refiner");

        return NULL;

    }

    if (config) {
        refiner->config = *config;
    } else {
        refiner->config = rcog_answer_default_config();
    }

    /* Validate config */
    if (refiner->config.latent_dim == 0) {
        refiner->config.latent_dim = RCOG_DEFAULT_LATENT_DIM;
    }
    if (refiner->config.latent_dim > RCOG_MAX_LATENT_DIM) {
        refiner->config.latent_dim = RCOG_MAX_LATENT_DIM;
    }

    /* Allocate velocity buffer for momentum */
    refiner->velocity = create_latent(refiner->config.latent_dim);
    if (!refiner->velocity) {
        nimcp_free(refiner);
        refiner = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "rcog_answer_refiner_create: refiner->velocity is NULL");
        return NULL;
    }
    refiner->velocity_dim = refiner->config.latent_dim;

    /* Create mutex */
    mutex_attr_t attr = {.type = MUTEX_TYPE_NORMAL};
    refiner->mutex = nimcp_mutex_create(&attr);
    if (!refiner->mutex) {
        nimcp_free(refiner->velocity);
        nimcp_free(refiner);
        refiner = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "rcog_answer_refiner_create: refiner->mutex is NULL");
        return NULL;
    }

    /* Initialize statistics */
    refiner->total_refinements = 0;
    refiner->total_answers = 0;
    refiner->answers_converged = 0;
    refiner->answers_max_steps = 0;
    refiner->answers_stalled = 0;
    refiner->total_steps = 0.0;
    refiner->total_confidence = 0.0;

    return refiner;
}

rcog_answer_refiner_t* rcog_answer_refiner_create_default(void)
{
    /* Phase 8: Heartbeat at operation start */
    rcog_answer_heartbeat("rcog_answer_refiner_create_defau", 0.0f);


    return rcog_answer_refiner_create(NULL);
}

void rcog_answer_refiner_destroy(rcog_answer_refiner_t* refiner)
{
    if (!refiner) return;

    /* Phase 8: Heartbeat at operation start */
    rcog_answer_heartbeat("rcog_answer_refiner_destroy", 0.0f);


    if (refiner->velocity) {
        nimcp_free(refiner->velocity);
    }

    if (refiner->mutex) {
        nimcp_mutex_destroy(refiner->mutex);
    }

    nimcp_free(refiner);
    refiner = NULL;
}

rcog_error_t rcog_answer_init(
    rcog_answer_refiner_t* refiner,
    const rcog_goal_t* goal,
    rcog_answer_state_t* state)
{
    if (!refiner || !state) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_answer_heartbeat("rcog_answer_init", 0.0f);


    nimcp_mutex_lock(refiner->mutex);

    memset(state, 0, sizeof(rcog_answer_state_t));

    /* Allocate latent vector */
    state->latent = create_latent(refiner->config.latent_dim);
    if (!state->latent) {
        nimcp_mutex_unlock(refiner->mutex);
        return RCOG_ERROR_OUT_OF_MEMORY;
    }
    state->latent_dim = refiner->config.latent_dim;

    /* Initialize state */
    state->content = NULL;
    state->content_size = 0;
    state->confidence = 0.0f;
    state->ready = false;
    state->status = RCOG_ANSWER_INITIALIZING;
    state->refinement_step = 0;
    state->delta = 1.0f;  /* Start with max delta */
    state->started_ms = get_current_time_ms();
    state->last_updated_ms = state->started_ms;

    /* Update statistics */
    refiner->total_answers++;

    nimcp_mutex_unlock(refiner->mutex);
    return RCOG_OK;
}

rcog_answer_state_t* rcog_answer_state_create(
    rcog_answer_refiner_t* refiner,
    const rcog_goal_t* goal)
{
    if (!refiner) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "refiner is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    rcog_answer_heartbeat("rcog_answer_state_create", 0.0f);


    rcog_answer_state_t* state = nimcp_calloc(1, sizeof(rcog_answer_state_t));
    if (!state) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate state");

        return NULL;

    }

    rcog_error_t err = rcog_answer_init(refiner, goal, state);
    if (err != RCOG_OK) {
        nimcp_free(state);
        state = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_answer_state_create: validation failed");
        return NULL;
    }

    state->heap_allocated = true;
    return state;
}

void rcog_answer_state_destroy(rcog_answer_state_t* state)
{
    if (!state) return;

    /* Phase 8: Heartbeat at operation start */
    rcog_answer_heartbeat("rcog_answer_state_destroy", 0.0f);


    if (state->latent) {
        nimcp_free(state->latent);
    }
    if (state->content) {
        nimcp_free(state->content);
    }

    if (state->heap_allocated) {
        nimcp_free(state);
        state = NULL;
    }
}

rcog_error_t rcog_answer_reset(
    rcog_answer_refiner_t* refiner,
    rcog_answer_state_t* state)
{
    if (!refiner || !state) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_answer_heartbeat("rcog_answer_reset", 0.0f);


    nimcp_mutex_lock(refiner->mutex);

    /* Reset latent to zero */
    if (state->latent) {
        memset(state->latent, 0, state->latent_dim * sizeof(float));
    }

    /* Free content */
    if (state->content) {
        nimcp_free(state->content);
        state->content = NULL;
        state->content_size = 0;
    }

    /* Reset state */
    state->confidence = 0.0f;
    state->ready = false;
    state->status = RCOG_ANSWER_INITIALIZING;
    state->refinement_step = 0;
    state->delta = 1.0f;
    state->started_ms = get_current_time_ms();
    state->last_updated_ms = state->started_ms;

    nimcp_mutex_unlock(refiner->mutex);
    return RCOG_OK;
}

rcog_error_t rcog_answer_step(
    rcog_answer_refiner_t* refiner,
    rcog_answer_state_t* state,
    const rcog_subtask_result_t* evidence,
    size_t num_evidence)
{
    if (!refiner || !state) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_answer_heartbeat("rcog_answer_step", 0.0f);


    nimcp_mutex_lock(refiner->mutex);

    /* Check if already at max steps */
    if (state->refinement_step >= refiner->config.max_steps) {
        state->status = RCOG_ANSWER_READY;
        refiner->answers_max_steps++;
        nimcp_mutex_unlock(refiner->mutex);
        return RCOG_OK;
    }

    /* Save previous latent for delta computation */
    float* prev_latent = create_latent(state->latent_dim);
    if (!prev_latent) {
        nimcp_mutex_unlock(refiner->mutex);
        return RCOG_ERROR_OUT_OF_MEMORY;
    }
    memcpy(prev_latent, state->latent, state->latent_dim * sizeof(float));

    /* Compute evidence direction */
    float* direction = create_latent(state->latent_dim);
    if (!direction) {
        nimcp_free(prev_latent);
        prev_latent = NULL;
        nimcp_mutex_unlock(refiner->mutex);
        return RCOG_ERROR_OUT_OF_MEMORY;
    }

    if (evidence && num_evidence > 0) {
        compute_evidence_direction(evidence, num_evidence, state->latent_dim, direction);
    }

    /* Apply momentum update */
    float lr = refiner->config.learning_rate;
    float momentum = refiner->config.momentum;

    for (size_t i = 0; i < state->latent_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && state->latent_dim > 256) {
            rcog_answer_heartbeat("rcog_answer_loop",
                             (float)(i + 1) / (float)state->latent_dim);
        }

        /* velocity = momentum * velocity + lr * direction */
        refiner->velocity[i] = momentum * refiner->velocity[i] + lr * direction[i];

        /* latent += velocity */
        state->latent[i] += refiner->velocity[i];
    }

    nimcp_free(direction);
    direction = NULL;

    /* Compute delta (change from previous) */
    float delta_sum = 0.0f;
    for (size_t i = 0; i < state->latent_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && state->latent_dim > 256) {
            rcog_answer_heartbeat("rcog_answer_loop",
                             (float)(i + 1) / (float)state->latent_dim);
        }

        float d = state->latent[i] - prev_latent[i];
        delta_sum += d * d;
    }
    state->delta = sqrtf(delta_sum / (float)state->latent_dim);

    nimcp_free(prev_latent);
    prev_latent = NULL;

    /* Update confidence */
    state->confidence = compute_confidence(state->latent, state->latent_dim);

    /* Update step and timing */
    state->refinement_step++;
    state->last_updated_ms = get_current_time_ms();
    state->status = RCOG_ANSWER_REFINING;

    /* Check for ready condition */
    if (state->confidence >= refiner->config.ready_threshold &&
        state->refinement_step >= refiner->config.min_steps) {
        state->ready = true;
        state->status = RCOG_ANSWER_READY;
        refiner->answers_converged++;
    } else if (refiner->config.enable_early_stopping &&
               state->delta < refiner->config.convergence_epsilon) {
        state->status = RCOG_ANSWER_CONVERGING;
        if (state->refinement_step >= refiner->config.min_steps) {
            state->ready = true;
            state->status = RCOG_ANSWER_READY;
            refiner->answers_converged++;
        }
    }

    /* Update statistics */
    refiner->total_refinements++;

    nimcp_mutex_unlock(refiner->mutex);
    return RCOG_OK;
}

rcog_error_t rcog_answer_update(
    rcog_answer_refiner_t* refiner,
    rcog_answer_state_t* state,
    const rcog_subtask_result_t* evidence)
{
    /* Phase 8: Heartbeat at operation start */
    rcog_answer_heartbeat("rcog_answer_update", 0.0f);


    return rcog_answer_step(refiner, state, evidence, 1);
}

rcog_error_t rcog_answer_refine_until_ready(
    rcog_answer_refiner_t* refiner,
    rcog_answer_state_t* state,
    rcog_error_t (*evidence_fn)(void* ctx, rcog_subtask_result_t** evidence, size_t* count),
    void* evidence_ctx)
{
    if (!refiner || !state || !evidence_fn) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_answer_heartbeat("rcog_answer_refine_until_ready", 0.0f);


    while (!state->ready && state->refinement_step < refiner->config.max_steps) {
        rcog_subtask_result_t* evidence = NULL;
        size_t count = 0;

        rcog_error_t err = evidence_fn(evidence_ctx, &evidence, &count);
        if (err != RCOG_OK) {
            return err;
        }

        err = rcog_answer_step(refiner, state, evidence, count);
        if (err != RCOG_OK) {
            return err;
        }

        /* Check for stall */
        if (state->status == RCOG_ANSWER_STALLED) {
            break;
        }
    }

    return RCOG_OK;
}

bool rcog_answer_is_ready(
    const rcog_answer_refiner_t* refiner,
    const rcog_answer_state_t* state)
{
    if (!refiner || !state) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    rcog_answer_heartbeat("rcog_answer_is_ready", 0.0f);


    return state->ready || (state->confidence >= refiner->config.ready_threshold);
}

bool rcog_answer_has_converged(
    const rcog_answer_refiner_t* refiner,
    const rcog_answer_state_t* state)
{
    if (!refiner || !state) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    rcog_answer_heartbeat("rcog_answer_has_converged", 0.0f);


    return state->delta < refiner->config.convergence_epsilon;
}

bool rcog_answer_is_stalled(
    const rcog_answer_refiner_t* refiner,
    const rcog_answer_state_t* state,
    uint32_t window)
{
    if (!refiner || !state) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    rcog_answer_heartbeat("rcog_answer_is_stalled", 0.0f);


    if (window == 0) window = 5;

    /* Check if confidence hasn't improved in last N steps */
    /* This is a simplified check - would need history for full implementation */
    return state->status == RCOG_ANSWER_STALLED;
}

float rcog_answer_get_confidence(const rcog_answer_state_t* state)
{
    /* Phase 8: Heartbeat at operation start */
    rcog_answer_heartbeat("rcog_answer_get_confidence", 0.0f);


    return state ? state->confidence : 0.0f;
}

uint32_t rcog_answer_get_step(const rcog_answer_state_t* state)
{
    /* Phase 8: Heartbeat at operation start */
    rcog_answer_heartbeat("rcog_answer_get_step", 0.0f);


    return state ? state->refinement_step : 0;
}

float rcog_answer_get_delta(const rcog_answer_state_t* state)
{
    /* Phase 8: Heartbeat at operation start */
    rcog_answer_heartbeat("rcog_answer_get_delta", 0.0f);


    return state ? state->delta : 0.0f;
}

rcog_error_t rcog_answer_extract(
    rcog_answer_refiner_t* refiner,
    const rcog_answer_state_t* state,
    float** output,
    size_t* output_size)
{
    if (!refiner || !state || !output || !output_size) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_answer_heartbeat("rcog_answer_extract", 0.0f);


    nimcp_mutex_lock(refiner->mutex);

    /* Copy latent as output */
    float* result = nimcp_calloc(state->latent_dim, sizeof(float));
    if (!result) {
        nimcp_mutex_unlock(refiner->mutex);
        return RCOG_ERROR_OUT_OF_MEMORY;
    }

    memcpy(result, state->latent, state->latent_dim * sizeof(float));

    *output = result;
    *output_size = state->latent_dim;

    /* Update final statistics */
    refiner->total_steps += state->refinement_step;
    refiner->total_confidence += state->confidence;

    nimcp_mutex_unlock(refiner->mutex);
    return RCOG_OK;
}

rcog_error_t rcog_answer_get_content(
    const rcog_answer_state_t* state,
    void** content,
    size_t* size)
{
    if (!state || !content || !size) return RCOG_ERROR_NULL_POINTER;

    *content = state->content;
    *size = state->content_size;

    /* Phase 8: Heartbeat at operation start */
    rcog_answer_heartbeat("rcog_answer_get_content", 0.0f);


    return RCOG_OK;
}

rcog_error_t rcog_answer_set_content(
    rcog_answer_state_t* state,
    const void* content,
    size_t size)
{
    if (!state) return RCOG_ERROR_NULL_POINTER;

    /* Free existing content */
    /* Phase 8: Heartbeat at operation start */
    rcog_answer_heartbeat("rcog_answer_set_content", 0.0f);


    if (state->content) {
        nimcp_free(state->content);
        state->content = NULL;
        state->content_size = 0;
    }

    if (content && size > 0) {
        state->content = nimcp_malloc(size);
        if (!state->content) {
            return RCOG_ERROR_OUT_OF_MEMORY;
        }
        memcpy(state->content, content, size);
        state->content_size = size;
    }

    return RCOG_OK;
}

rcog_error_t rcog_answer_get_latent(
    const rcog_answer_state_t* state,
    float** latent,
    size_t* dim)
{
    if (!state || !latent || !dim) return RCOG_ERROR_NULL_POINTER;

    *latent = state->latent;
    *dim = state->latent_dim;

    /* Phase 8: Heartbeat at operation start */
    rcog_answer_heartbeat("rcog_answer_get_latent", 0.0f);


    return RCOG_OK;
}

rcog_error_t rcog_answer_set_latent(
    rcog_answer_state_t* state,
    const float* latent,
    size_t dim)
{
    if (!state || !latent) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_answer_heartbeat("rcog_answer_set_latent", 0.0f);


    if (dim != state->latent_dim) {
        /* Reallocate if dimension differs */
        float* new_latent = nimcp_realloc(state->latent, dim * sizeof(float));
        if (!new_latent) {
            return RCOG_ERROR_OUT_OF_MEMORY;
        }
        state->latent = new_latent;
        state->latent_dim = dim;
    }

    memcpy(state->latent, latent, dim * sizeof(float));

    /* Update confidence after setting latent */
    state->confidence = compute_confidence(state->latent, state->latent_dim);

    return RCOG_OK;
}

rcog_error_t rcog_answer_blend(
    rcog_answer_refiner_t* refiner,
    const rcog_answer_state_t* a,
    const rcog_answer_state_t* b,
    float alpha,
    rcog_answer_state_t* result)
{
    if (!refiner || !a || !b || !result) return RCOG_ERROR_NULL_POINTER;
    if (a->latent_dim != b->latent_dim) return RCOG_ERROR_INVALID_CONFIG;

    /* Phase 8: Heartbeat at operation start */
    rcog_answer_heartbeat("rcog_answer_blend", 0.0f);


    nimcp_mutex_lock(refiner->mutex);

    /* Initialize result if needed */
    if (!result->latent) {
        result->latent = create_latent(a->latent_dim);
        if (!result->latent) {
            nimcp_mutex_unlock(refiner->mutex);
            return RCOG_ERROR_OUT_OF_MEMORY;
        }
        result->latent_dim = a->latent_dim;
    }

    /* Blend latent vectors: result = (1 - alpha) * a + alpha * b */
    for (size_t i = 0; i < a->latent_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && a->latent_dim > 256) {
            rcog_answer_heartbeat("rcog_answer_loop",
                             (float)(i + 1) / (float)a->latent_dim);
        }

        result->latent[i] = (1.0f - alpha) * a->latent[i] + alpha * b->latent[i];
    }

    /* Blend confidence */
    result->confidence = (1.0f - alpha) * a->confidence + alpha * b->confidence;

    /* Update other fields */
    result->ready = a->ready && b->ready;
    result->refinement_step = (a->refinement_step + b->refinement_step) / 2;
    result->delta = (a->delta + b->delta) / 2.0f;
    result->status = RCOG_ANSWER_REFINING;
    result->last_updated_ms = get_current_time_ms();

    nimcp_mutex_unlock(refiner->mutex);
    return RCOG_OK;
}

rcog_error_t rcog_answer_get_history(
    const rcog_answer_refiner_t* refiner,
    const rcog_answer_state_t* state,
    rcog_answer_history_t** history)
{
    if (!refiner || !state || !history) return RCOG_ERROR_NULL_POINTER;

    /* Create a simple history from current state */
    /* In full implementation, this would return the tracked history */
    /* Phase 8: Heartbeat at operation start */
    rcog_answer_heartbeat("rcog_answer_get_history", 0.0f);


    rcog_answer_history_t* h = create_history(1);
    if (!h) return RCOG_ERROR_OUT_OF_MEMORY;

    add_history_entry(h, state->refinement_step, state->confidence,
                      state->delta, 0);

    *history = h;
    return RCOG_OK;
}

void rcog_answer_history_free(rcog_answer_history_t* history)
{
    if (!history) return;

    /* Phase 8: Heartbeat at operation start */
    rcog_answer_heartbeat("rcog_answer_history_free", 0.0f);


    if (history->entries) {
        nimcp_free(history->entries);
    }
    nimcp_free(history);
    history = NULL;
}

rcog_error_t rcog_answer_refiner_get_stats(
    const rcog_answer_refiner_t* refiner,
    rcog_answer_stats_t* stats)
{
    if (!refiner || !stats) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_answer_heartbeat("rcog_answer_refiner_get_stats", 0.0f);


    nimcp_mutex_lock(((rcog_answer_refiner_t*)refiner)->mutex);

    stats->total_refinements = refiner->total_refinements;
    stats->total_answers = refiner->total_answers;
    stats->answers_converged = refiner->answers_converged;
    stats->answers_max_steps = refiner->answers_max_steps;
    stats->answers_stalled = refiner->answers_stalled;

    stats->avg_steps_to_ready = refiner->total_answers > 0 ?
        (float)(refiner->total_steps / refiner->total_answers) : 0.0f;
    stats->avg_final_confidence = refiner->total_answers > 0 ?
        (float)(refiner->total_confidence / refiner->total_answers) : 0.0f;

    nimcp_mutex_unlock(((rcog_answer_refiner_t*)refiner)->mutex);
    return RCOG_OK;
}

void rcog_answer_refiner_reset_stats(rcog_answer_refiner_t* refiner)
{
    if (!refiner) return;

    /* Phase 8: Heartbeat at operation start */
    rcog_answer_heartbeat("rcog_answer_refiner_reset_stats", 0.0f);


    nimcp_mutex_lock(refiner->mutex);

    refiner->total_refinements = 0;
    refiner->total_answers = 0;
    refiner->answers_converged = 0;
    refiner->answers_max_steps = 0;
    refiner->answers_stalled = 0;
    refiner->total_steps = 0.0;
    refiner->total_confidence = 0.0;

    nimcp_mutex_unlock(refiner->mutex);
}

rcog_error_t rcog_answer_set_threshold(
    rcog_answer_refiner_t* refiner,
    float threshold)
{
    if (!refiner) return RCOG_ERROR_NULL_POINTER;
    if (threshold < 0.0f || threshold > 1.0f) return RCOG_ERROR_INVALID_CONFIG;

    /* Phase 8: Heartbeat at operation start */
    rcog_answer_heartbeat("rcog_answer_set_threshold", 0.0f);


    nimcp_mutex_lock(refiner->mutex);
    refiner->config.ready_threshold = threshold;
    nimcp_mutex_unlock(refiner->mutex);

    return RCOG_OK;
}

rcog_error_t rcog_answer_set_learning_rate(
    rcog_answer_refiner_t* refiner,
    float learning_rate)
{
    if (!refiner) return RCOG_ERROR_NULL_POINTER;
    if (learning_rate <= 0.0f || learning_rate > 1.0f) return RCOG_ERROR_INVALID_CONFIG;

    /* Phase 8: Heartbeat at operation start */
    rcog_answer_heartbeat("rcog_answer_set_learning_rate", 0.0f);


    nimcp_mutex_lock(refiner->mutex);
    refiner->config.learning_rate = learning_rate;
    nimcp_mutex_unlock(refiner->mutex);

    return RCOG_OK;
}

rcog_error_t rcog_answer_refiner_get_config(
    const rcog_answer_refiner_t* refiner,
    rcog_answer_config_t* config)
{
    if (!refiner || !config) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_answer_heartbeat("rcog_answer_refiner_get_config", 0.0f);


    nimcp_mutex_lock(((rcog_answer_refiner_t*)refiner)->mutex);
    *config = refiner->config;
    nimcp_mutex_unlock(((rcog_answer_refiner_t*)refiner)->mutex);

    return RCOG_OK;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int rcog_answer_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    rcog_answer_heartbeat("rcog_answer_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Recursive_Cognition_Answer_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                rcog_answer_heartbeat("rcog_answer_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            /* Log self-knowledge observations */
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Recursive_Cognition_Answer_Module");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Recursive_Cognition_Answer_Module");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}
