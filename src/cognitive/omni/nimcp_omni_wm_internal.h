/**
 * @file nimcp_omni_wm_internal.h
 * @brief Internal structures and helpers for Omni World Model
 * @version 1.0.0
 * @date 2026-02-16
 *
 * WHAT: Shared internal structures and helper functions for world model
 * WHY:  Enable code reuse across split implementation files
 * HOW:  Central definitions for internal types, forward declarations,
 *       and common utility functions
 */

#ifndef NIMCP_OMNI_WM_INTERNAL_H
#define NIMCP_OMNI_WM_INTERNAL_H

#include "cognitive/omni/nimcp_omni_world_model.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <float.h>
#include <time.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Internal Constants
 * ============================================================================ */

/** Maximum checkpoints stored in memory */
#define OMNI_WM_MAX_CHECKPOINTS 32

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Checkpoint storage structure
 */
typedef struct {
    omni_wm_checkpoint_t checkpoints[OMNI_WM_MAX_CHECKPOINTS];
    uint32_t count;
    uint64_t next_id;
} omni_wm_checkpoint_store_t;

/**
 * @brief Experience replay buffer (circular)
 */
typedef struct {
    omni_wm_experience_t** experiences;  /**< Experience array */
    uint32_t capacity;                    /**< Maximum size */
    uint32_t size;                        /**< Current size */
    uint32_t head;                        /**< Write position */
    float* priorities;                    /**< Priority values */
} omni_wm_replay_buffer_t;

/**
 * @brief Forward dynamics model weights (simple linear for now)
 */
typedef struct {
    float* W_h;      /**< Weights for deterministic transition */
    float* W_z;      /**< Weights for stochastic prior */
    float* W_obs;    /**< Weights for observation model */
    float* b_h;      /**< Bias for deterministic */
    float* b_z;      /**< Bias for stochastic */
    float* b_obs;    /**< Bias for observation */
    uint32_t h_dim;
    uint32_t z_dim;
    uint32_t obs_dim;
    uint32_t action_dim;
} omni_wm_dynamics_t;

/**
 * @brief World model internal structure
 */
struct omni_world_model {
    /* Configuration */
    omni_wm_config_t config;

    /* Current state */
    omni_wm_state_t* current_state;
    omni_wm_rssm_state_t* rssm_state;

    /* Dynamics models (one per direction) */
    omni_wm_dynamics_t* forward_dynamics;
    omni_wm_dynamics_t* backward_dynamics;
    omni_wm_dynamics_t* lateral_dynamics;

    /* Latent encoder/decoder weights */
    float* encoder_W;
    float* encoder_b;
    float* decoder_W;
    float* decoder_b;

    /* Experience replay */
    omni_wm_replay_buffer_t* replay_buffer;

    /* Statistics */
    omni_wm_stats_t stats;

    /* Active inference integration */
    struct omni_active_inference* ai_ctx;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Random state for sampling */
    unsigned int rand_seed;

    /* Checkpoint storage */
    omni_wm_checkpoint_store_t* checkpoint_store;

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bool bio_async_connected;

    /* Initialized flag */
    bool initialized;
};

/* ============================================================================
 * Internal Helper Functions - Shared Utilities
 * ============================================================================ */

/**
 * @brief Generate random normal value (Box-Muller)
 */
static inline float randn(unsigned int* seed) {
    float u1 = (float)rand_r(seed) / RAND_MAX;
    float u2 = (float)rand_r(seed) / RAND_MAX;
    if (u1 < 1e-10f) u1 = 1e-10f;
    return sqrtf(-2.0f * logf(u1)) * cosf(2.0f * M_PI * u2);
}

/* ============================================================================
 * Dynamics Module (nimcp_omni_wm_dynamics.c)
 * ============================================================================ */

omni_wm_dynamics_t* dynamics_create(uint32_t h_dim, uint32_t z_dim,
                                      uint32_t obs_dim, uint32_t action_dim);
void dynamics_destroy(omni_wm_dynamics_t* dyn);

nimcp_error_t omni_wm_step_dynamics_forward(omni_world_model_t* wm,
                                              const omni_wm_rssm_state_t* state,
                                              const float* action,
                                              uint32_t action_dim,
                                              omni_wm_rssm_state_t* next_state);

nimcp_error_t omni_wm_step_dynamics_backward(omni_world_model_t* wm,
                                               const omni_wm_rssm_state_t* state,
                                               omni_wm_rssm_state_t* prev_state,
                                               float* action_out);

nimcp_error_t omni_wm_step_dynamics_lateral(omni_world_model_t* wm,
                                              const omni_wm_rssm_state_t* source,
                                              omni_wm_rssm_state_t* target);

/* ============================================================================
 * Replay Buffer Module (nimcp_omni_wm_replay_buffer.c)
 * ============================================================================ */

omni_wm_replay_buffer_t* replay_buffer_create(uint32_t capacity);
void replay_buffer_destroy(omni_wm_replay_buffer_t* buf);

nimcp_error_t replay_buffer_add(omni_wm_replay_buffer_t* buf,
                                  const omni_wm_experience_t* exp);

uint32_t replay_buffer_sample(omni_wm_replay_buffer_t* buf,
                                omni_wm_experience_t** batch,
                                uint32_t batch_size);

uint32_t replay_buffer_size(const omni_wm_replay_buffer_t* buf);
nimcp_error_t replay_buffer_clear(omni_wm_replay_buffer_t* buf);

/* ============================================================================
 * Serialization Module (nimcp_omni_wm_serialization.c)
 * ============================================================================ */

uint32_t crc32_compute(const uint8_t* data, size_t length);

size_t write_u8(uint8_t* buf, size_t pos, uint8_t val);
size_t write_u32(uint8_t* buf, size_t pos, uint32_t val);
size_t write_u64(uint8_t* buf, size_t pos, uint64_t val);
size_t write_float_be(uint8_t* buf, size_t pos, float val);
size_t write_double_be(uint8_t* buf, size_t pos, double val);

uint8_t read_u8(const uint8_t* buf, size_t* pos);
uint32_t read_u32(const uint8_t* buf, size_t* pos);
uint64_t read_u64(const uint8_t* buf, size_t* pos);
float read_float_be(const uint8_t* buf, size_t* pos);
double read_double_be(const uint8_t* buf, size_t* pos);

size_t serialize_config(uint8_t* buf, size_t pos, const omni_wm_config_t* cfg);
size_t deserialize_config(const uint8_t* buf, size_t pos, omni_wm_config_t* cfg);

size_t serialize_state(uint8_t* buf, size_t pos, const omni_wm_state_t* state);
omni_wm_state_t* deserialize_state_from_buf(const uint8_t* buf, size_t* pos);

size_t serialize_rssm_state(uint8_t* buf, size_t pos, const omni_wm_rssm_state_t* state);
omni_wm_rssm_state_t* deserialize_rssm_state_from_buf(const uint8_t* buf, size_t* pos);

size_t serialize_dynamics_weights(uint8_t* buf, size_t pos, const omni_wm_dynamics_t* dyn);
omni_wm_dynamics_t* deserialize_dynamics_from_buf(const uint8_t* buf, size_t* pos);

size_t serialize_wm_stats(uint8_t* buf, size_t pos, const omni_wm_stats_t* stats);
size_t deserialize_wm_stats(const uint8_t* buf, size_t pos, omni_wm_stats_t* stats);

/* ============================================================================
 * Checkpoint Module (nimcp_omni_wm_checkpoint.c)
 * ============================================================================ */

omni_wm_checkpoint_store_t* checkpoint_store_create(void);
void checkpoint_store_destroy(omni_wm_checkpoint_store_t* store);

nimcp_error_t checkpoint_store_add(omni_wm_checkpoint_store_t* store,
                                     const omni_wm_checkpoint_t* checkpoint);

const omni_wm_checkpoint_t* checkpoint_store_get(const omni_wm_checkpoint_store_t* store,
                                                   uint64_t id);

nimcp_error_t checkpoint_store_delete(omni_wm_checkpoint_store_t* store, uint64_t id);
uint32_t checkpoint_store_count(const omni_wm_checkpoint_store_t* store);
nimcp_error_t checkpoint_store_clear(omni_wm_checkpoint_store_t* store);

/* ============================================================================
 * Counterfactual Module (nimcp_omni_wm_counterfactual.c)
 * ============================================================================ */

nimcp_error_t counterfactual_query_validate(const omni_wm_counterfactual_query_t* query);
nimcp_error_t counterfactual_rollout(omni_world_model_t* wm,
                                       const omni_wm_counterfactual_query_t* query,
                                       omni_wm_counterfactual_result_t* result);

/* ============================================================================
 * State Module (nimcp_omni_wm_state.c)
 * ============================================================================ */

/* All state/RSSM/latent create/destroy/clone functions are here */
/* Declared in public API, implemented in state module */

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OMNI_WM_INTERNAL_H */
