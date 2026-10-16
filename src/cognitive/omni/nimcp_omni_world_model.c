/**
 * @file nimcp_omni_world_model.c
 * @brief Implementation of Omnidirectional Generative World Model
 * @version 1.0.0
 * @date 2026-01-04
 *
 * IMPLEMENTATION NOTES:
 * =====================
 * This implementation draws from key research insights:
 *
 * 1. DREAMERV3 (Hafner et al., Nature 2025):
 *    - RSSM architecture with deterministic + stochastic components
 *    - Symlog transformation for reward normalization
 *    - Learning in latent space for efficiency
 *
 * 2. JEPA (LeCun, 2022-2025):
 *    - Prediction in embedding space, not pixel space
 *    - Self-supervised learning from structure
 *
 * 3. ACTIVE INFERENCE (Friston):
 *    - World model for policy evaluation
 *    - Free energy minimization through imagination
 */

#include "cognitive/omni/nimcp_omni_world_model.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "cognitive/world_model/nimcp_world_model_kg_events.h"  /* W8 */
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <float.h>
#include <time.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"
#include "constants/nimcp_math_constants.h"

BRIDGE_BOILERPLATE(omni_world_model, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/** Maximum checkpoints stored in memory */
#define OMNI_WM_MAX_CHECKPOINTS 32

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
 * Symlog Transformation (DreamerV3)
 * ============================================================================ */


/* ============================================================================
 * Internal Helpers - Forward Declarations
 * ============================================================================ */

/* Checkpoint management (defined later in file) */
static void checkpoint_store_destroy_internal(omni_wm_checkpoint_store_t* store);

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */


/* ============================================================================
 * Bio-Async Integration - Message Structures
 * ============================================================================ */

/**
 * @brief Prediction request message (BIO_MSG_OMNI_WM_PREDICT - 0x6201)
 */
typedef struct {
    bio_message_header_t header;
    float state[OMNI_WM_MAX_STATE_DIM];   /**< Current state */
    uint32_t state_dim;                    /**< State dimensionality */
    float action[OMNI_WM_MAX_ACTION_DIM]; /**< Action to predict */
    uint32_t action_dim;                   /**< Action dimensionality */
    omni_wm_direction_t direction;         /**< Dynamics direction */
} bio_msg_omni_wm_predict_t;

/**
 * @brief Prediction response message
 */
typedef struct {
    bio_message_header_t header;
    float predicted_state[OMNI_WM_MAX_STATE_DIM]; /**< Predicted state */
    uint32_t state_dim;                            /**< State dimensionality */
    float prediction_error;                        /**< Prediction error estimate */
    float confidence;                              /**< Prediction confidence */
} bio_msg_omni_wm_predict_response_t;

/**
 * @brief Counterfactual query message (BIO_MSG_OMNI_WM_COUNTERFACTUAL - 0x6202)
 */
typedef struct {
    bio_message_header_t header;
    omni_wm_counterfactual_type_t cf_type;         /**< Type of counterfactual */
    float initial_state[OMNI_WM_MAX_STATE_DIM];    /**< Starting state */
    uint32_t state_dim;                             /**< State dimensionality */
    float hypothetical_action[OMNI_WM_MAX_ACTION_DIM]; /**< Hypothetical action */
    uint32_t action_dim;                            /**< Action dimensionality */
    uint32_t horizon;                               /**< Simulation horizon */
} bio_msg_omni_wm_counterfactual_t;

/**
 * @brief Counterfactual response message
 */
typedef struct {
    bio_message_header_t header;
    float expected_reward;       /**< Expected cumulative reward */
    float divergence;            /**< Divergence from actual trajectory */
    float confidence;            /**< Prediction confidence */
    uint32_t trajectory_length;  /**< Number of simulated steps */
} bio_msg_omni_wm_counterfactual_response_t;

/**
 * @brief Model update message (BIO_MSG_OMNI_WM_UPDATE - 0x6203)
 */
typedef struct {
    bio_message_header_t header;
    float state[OMNI_WM_MAX_STATE_DIM];       /**< Current state */
    uint32_t state_dim;                        /**< State dimensionality */
    float action[OMNI_WM_MAX_ACTION_DIM];     /**< Action taken */
    uint32_t action_dim;                       /**< Action dimensionality */
    float next_state[OMNI_WM_MAX_STATE_DIM];  /**< Resulting state */
    float reward;                              /**< Reward received */
} bio_msg_omni_wm_update_t;

/**
 * @brief Model update acknowledgment message
 */
typedef struct {
    bio_message_header_t header;
    nimcp_error_t status;       /**< Update status */
    float prediction_error;     /**< Error before update */
} bio_msg_omni_wm_update_ack_t;

/**
 * @brief Rollout request message (BIO_MSG_OMNI_WM_ROLLOUT - 0x6204)
 */
typedef struct {
    bio_message_header_t header;
    float initial_state[OMNI_WM_MAX_STATE_DIM]; /**< Starting state */
    uint32_t state_dim;                          /**< State dimensionality */
    float policy_actions[OMNI_WM_MAX_HORIZON * OMNI_WM_MAX_ACTION_DIM]; /**< Action sequence */
    uint32_t action_dim;                         /**< Action dimensionality */
    uint32_t horizon;                            /**< Rollout horizon */
    float preferred_obs[OMNI_WM_MAX_OBS_DIM];   /**< Preferred observations (goal) */
    uint32_t obs_dim;                            /**< Observation dimensionality */
} bio_msg_omni_wm_rollout_t;

/**
 * @brief Rollout response message
 */
typedef struct {
    bio_message_header_t header;
    float total_reward;          /**< Cumulative reward */
    float expected_free_energy;  /**< Expected free energy */
    uint32_t steps_completed;    /**< Actual steps completed */
    float final_state[OMNI_WM_MAX_STATE_DIM]; /**< Final state */
    uint32_t state_dim;                        /**< State dimensionality */
} bio_msg_omni_wm_rollout_response_t;

/* ============================================================================
 * Bio-Async Integration - Message Handlers
 * ============================================================================ */

/**
 * @brief Handle prediction request message
 *
 * WHAT: Process forward/backward/lateral dynamics prediction request
 * WHY:  Allow other modules to query world model predictions via bio-async
 * HOW:  Extract state/action, call appropriate dynamics function, send response
 */


// Forward declarations for static functions (SRP split)
static float randn(unsigned int* seed);
static omni_wm_dynamics_t* dynamics_create(uint32_t h_dim, uint32_t z_dim, uint32_t obs_dim, uint32_t action_dim);
static void dynamics_destroy(omni_wm_dynamics_t* dyn);
static omni_wm_replay_buffer_t* replay_buffer_create(uint32_t capacity);
static void replay_buffer_destroy(omni_wm_replay_buffer_t* buf);
static nimcp_error_t handle_omni_wm_predict( const void* msg, size_t msg_size, nimcp_bio_promise_t response_promise, void* user_data);
static nimcp_error_t handle_omni_wm_counterfactual( const void* msg, size_t msg_size, nimcp_bio_promise_t response_promise, void* user_data);
static nimcp_error_t handle_omni_wm_update( const void* msg, size_t msg_size, nimcp_bio_promise_t response_promise, void* user_data);
static nimcp_error_t handle_omni_wm_rollout( const void* msg, size_t msg_size, nimcp_bio_promise_t response_promise, void* user_data);
static uint32_t crc32_compute(const uint8_t* data, size_t length);
static size_t write_u8(uint8_t* buf, size_t pos, uint8_t val);
static size_t write_u32(uint8_t* buf, size_t pos, uint32_t val);
static size_t write_u64(uint8_t* buf, size_t pos, uint64_t val);
static size_t write_float_be(uint8_t* buf, size_t pos, float val);
static size_t write_double_be(uint8_t* buf, size_t pos, double val);
static uint8_t read_u8(const uint8_t* buf, size_t* pos);
static uint32_t read_u32(const uint8_t* buf, size_t* pos);
static uint64_t read_u64(const uint8_t* buf, size_t* pos);
static float read_float_be(const uint8_t* buf, size_t* pos);
static double read_double_be(const uint8_t* buf, size_t* pos);
static size_t serialize_config(uint8_t* buf, size_t pos, const omni_wm_config_t* cfg);
static size_t deserialize_config(const uint8_t* buf, size_t pos, omni_wm_config_t* cfg);
static size_t serialize_state(uint8_t* buf, size_t pos, const omni_wm_state_t* state);
static omni_wm_state_t* deserialize_state_from_buf(const uint8_t* buf, size_t* pos);
static size_t serialize_rssm_state(uint8_t* buf, size_t pos, const omni_wm_rssm_state_t* state);
static omni_wm_rssm_state_t* deserialize_rssm_state_from_buf(const uint8_t* buf, size_t* pos);
static size_t serialize_dynamics_weights(uint8_t* buf, size_t pos, const omni_wm_dynamics_t* dyn);
static omni_wm_dynamics_t* deserialize_dynamics_from_buf(const uint8_t* buf, size_t* pos);
static size_t serialize_wm_stats(uint8_t* buf, size_t pos, const omni_wm_stats_t* stats);
static size_t deserialize_wm_stats(const uint8_t* buf, size_t pos, omni_wm_stats_t* stats);
static omni_wm_checkpoint_store_t* checkpoint_store_create(void);

//=============================================================================
// SRP Split: Function implementations organized by responsibility
//=============================================================================
#include "nimcp_omni_world_model_part_accessors.c"  // 12 functions: accessors
#include "nimcp_omni_world_model_part_core.c"  // 38 functions: core
#include "nimcp_omni_world_model_part_processing.c"  // 4 functions: processing
#include "nimcp_omni_world_model_part_helpers.c"  // 20 functions: helpers
#include "nimcp_omni_world_model_part_lifecycle.c"  // 25 functions: lifecycle
#include "nimcp_omni_world_model_part_stats.c"  // 5 functions: stats
#include "nimcp_omni_world_model_part_io.c"  // 7 functions: io
