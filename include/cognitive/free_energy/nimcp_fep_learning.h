/**
 * @file nimcp_fep_learning.h
 * @brief Learnable Generative Models for Free Energy Principle
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Online learning system for FEP generative model parameters
 * WHY:  Enables adaptive generative models that improve through experience,
 *       implementing synaptic plasticity at the generative model level
 * HOW:  Gradient descent with L2 regularization on transition and likelihood
 *       matrices, tracking convergence and learning statistics
 *
 * THEORETICAL FOUNDATION:
 * ==================================================================================
 *
 * LEARNING IN THE FREE ENERGY PRINCIPLE:
 * --------------------------------------
 * The FEP distinguishes between:
 * 1. PERCEPTION: Fast inference (belief updates, minimize F)
 * 2. LEARNING: Slow parameter updates (improve generative model)
 *
 * Learning updates model parameters θ to minimize expected free energy:
 *   ∂θ/∂t = -η * ∂⟨F⟩/∂θ
 *
 * Where:
 *   θ = Generative model parameters (A, B, C, D matrices)
 *   η = Learning rate
 *   ⟨F⟩ = Expected free energy over time
 *
 * TRANSITION MATRIX LEARNING:
 * ---------------------------
 * Learn state transitions: A[i,j] = P(s_t = i | s_{t-1} = j)
 *
 * Objective: Minimize prediction error on state transitions
 *   L_A = ||s_t - A * s_{t-1}||² + λ||A||²
 *
 * Gradient: ∂L_A/∂A = -2(s_t - A*s_{t-1}) ⊗ s_{t-1}^T + 2λA
 * Update:   A_{new} = A - η * ∂L_A/∂A
 *
 * LIKELIHOOD MATRIX LEARNING:
 * ---------------------------
 * Learn observation likelihood: B[i,j] = P(o_t = i | s_t = j)
 *
 * Objective: Minimize observation prediction error
 *   L_B = ||o_t - B * s_t||² + λ||B||²
 *
 * Gradient: ∂L_B/∂B = -2(o_t - B*s_t) ⊗ s_t^T + 2λB
 * Update:   B_{new} = B - η * ∂L_B/∂B
 *
 * BIOLOGICAL BASIS:
 * -----------------
 * This implements Hebbian-like learning at the generative model level:
 *
 * SYNAPTIC PLASTICITY ANALOG:
 * - Weight updates: ΔW = η * pre * post (Hebbian rule)
 * - Matrix updates: ΔA = η * error * state^T (prediction error-modulated)
 * - Regularization: Weight decay prevents runaway growth (homeostasis)
 *
 * TIMESCALES:
 * - Fast (10-100ms): Belief updates (perception/inference)
 * - Medium (1-10s): Precision updates (attention allocation)
 * - Slow (minutes-hours): Parameter learning (structural changes)
 *
 * CONSOLIDATION:
 * - Online updates accumulate evidence over trials
 * - Batch learning enables replay-based consolidation
 * - Statistics track learning progress and convergence
 *
 * REGULARIZATION:
 * ---------------
 * L2 regularization (weight decay) serves multiple purposes:
 *
 * 1. BIOLOGICAL: Prevents synaptic weights from growing unbounded
 * 2. STATISTICAL: Reduces overfitting to noisy observations
 * 3. COMPUTATIONAL: Maintains numerical stability
 *
 * Regularization term: λ||θ||² encourages smaller parameter values
 * Balance: Large λ → simple models; Small λ → flexible models
 *
 * CONVERGENCE:
 * ------------
 * Learning is monitored via:
 * - Loss history: Track prediction error over time
 * - Gradient norm: Detect convergence when ||∂L/∂θ|| → 0
 * - Parameter change: Monitor Δθ between updates
 * - Validation error: Check generalization (future extension)
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    FEP LEARNING SYSTEM                                     ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   TRANSITION LEARNER                                │  ║
 * ║   │                                                                     │  ║
 * ║   │   Input: (s_t, s_{t-1})                                            │  ║
 * ║   │      ↓                                                             │  ║
 * ║   │   Compute: error = s_t - A * s_{t-1}                              │  ║
 * ║   │      ↓                                                             │  ║
 * ║   │   Gradient: ∂L/∂A = -error ⊗ s_{t-1}^T + λA                       │  ║
 * ║   │      ↓                                                             │  ║
 * ║   │   Update: A = A - η * ∂L/∂A                                       │  ║
 * ║   │      ↓                                                             │  ║
 * ║   │   Output: Updated transition matrix A                              │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   LIKELIHOOD LEARNER                                │  ║
 * ║   │                                                                     │  ║
 * ║   │   Input: (o_t, s_t)                                                │  ║
 * ║   │      ↓                                                             │  ║
 * ║   │   Compute: error = o_t - B * s_t                                  │  ║
 * ║   │      ↓                                                             │  ║
 * ║   │   Gradient: ∂L/∂B = -error ⊗ s_t^T + λB                           │  ║
 * ║   │      ↓                                                             │  ║
 * ║   │   Update: B = B - η * ∂L/∂B                                       │  ║
 * ║   │      ↓                                                             │  ║
 * ║   │   Output: Updated likelihood matrix B                              │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   BATCH LEARNING                                    │  ║
 * ║   │                                                                     │  ║
 * ║   │   Accumulate: Σ gradients over batch                               │  ║
 * ║   │   Average: gradient = (1/N) * Σ ∂L_i/∂θ                           │  ║
 * ║   │   Update: θ = θ - η * gradient                                    │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * REFERENCES:
 * - Friston et al. (2016) "Active inference and learning"
 * - Friston (2008) "Hierarchical models in the brain"
 * - Rumelhart et al. (1986) "Learning internal representations by error propagation"
 * - Rao & Ballard (1999) "Predictive coding in the visual cortex"
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_FEP_LEARNING_H
#define NIMCP_FEP_LEARNING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/free_energy/nimcp_free_energy.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Default learning parameters */
#define FEP_LEARNING_DEFAULT_LR           0.01f     /**< Default learning rate */
#define FEP_LEARNING_DEFAULT_REG          0.001f    /**< Default L2 regularization */
#define FEP_LEARNING_DEFAULT_BATCH_SIZE   32        /**< Default batch size */
#define FEP_LEARNING_DEFAULT_MOMENTUM     0.9f      /**< Default momentum */

/* Learning rate bounds */
#define FEP_LEARNING_MIN_LR               1e-6f     /**< Minimum learning rate */
#define FEP_LEARNING_MAX_LR               1.0f      /**< Maximum learning rate */

/* Regularization bounds */
#define FEP_LEARNING_MIN_REG              0.0f      /**< Minimum regularization */
#define FEP_LEARNING_MAX_REG              1.0f      /**< Maximum regularization */

/* Convergence thresholds */
#define FEP_LEARNING_CONVERGENCE_LOSS     0.001f    /**< Loss change threshold */
#define FEP_LEARNING_CONVERGENCE_GRAD     0.0001f   /**< Gradient norm threshold */

/* Buffer sizes */
#define FEP_LEARNING_MAX_HISTORY          1000      /**< Maximum loss history */

/* Bio-async configuration */
#define FEP_LEARNING_BIO_INBOX_SIZE       32        /**< Bio-async inbox capacity */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Learning optimizer types
 */
typedef enum {
    FEP_OPTIMIZER_SGD = 0,          /**< Stochastic gradient descent */
    FEP_OPTIMIZER_MOMENTUM,         /**< SGD with momentum */
    FEP_OPTIMIZER_ADAM,             /**< Adam optimizer */
    FEP_OPTIMIZER_RMSPROP           /**< RMSprop optimizer */
} fep_optimizer_type_t;

/**
 * @brief Learning state
 */
typedef enum {
    FEP_LEARNING_IDLE = 0,          /**< Not learning */
    FEP_LEARNING_ACTIVE,            /**< Actively learning */
    FEP_LEARNING_CONVERGED,         /**< Converged */
    FEP_LEARNING_DIVERGED           /**< Diverged (numerical issues) */
} fep_learning_state_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Learning configuration
 */
typedef struct {
    /* Learning rates */
    float learning_rate;            /**< Base learning rate */
    float learning_rate_decay;      /**< LR decay per epoch */
    float min_learning_rate;        /**< Minimum LR */

    /* Regularization */
    float l2_regularization;        /**< L2 weight decay coefficient */

    /* Optimizer settings */
    fep_optimizer_type_t optimizer; /**< Optimizer type */
    float momentum;                 /**< Momentum coefficient (0.0-1.0) */
    float beta1;                    /**< Adam beta1 (first moment) */
    float beta2;                    /**< Adam beta2 (second moment) */
    float epsilon;                  /**< Numerical stability epsilon */

    /* Batch learning */
    uint32_t batch_size;            /**< Batch size for updates */
    bool use_batch_learning;        /**< Enable batch accumulation */

    /* Convergence */
    float convergence_threshold;    /**< Loss change threshold */
    float gradient_threshold;       /**< Gradient norm threshold */
    uint32_t convergence_window;    /**< Window for convergence check */

    /* Statistics */
    bool track_statistics;          /**< Enable detailed statistics */
    uint32_t history_size;          /**< Loss history buffer size */
} fep_learning_config_t;

/**
 * @brief Learning statistics
 */
typedef struct {
    /* Update counters */
    uint64_t total_updates;         /**< Total updates performed */
    uint64_t online_updates;        /**< Online (single) updates */
    uint64_t batch_updates;         /**< Batch updates */

    /* Loss tracking */
    float current_loss;             /**< Current loss value */
    float min_loss;                 /**< Minimum loss achieved */
    float avg_loss;                 /**< Running average loss */
    float* loss_history;            /**< Loss history buffer */
    uint32_t history_count;         /**< Number of history entries */
    uint32_t history_capacity;      /**< History buffer capacity */

    /* Gradient tracking */
    float current_grad_norm;        /**< Current gradient norm */
    float max_grad_norm;            /**< Maximum gradient norm seen */
    float avg_grad_norm;            /**< Average gradient norm */

    /* Convergence tracking */
    fep_learning_state_t state;     /**< Current learning state */
    uint32_t convergence_count;     /**< Consecutive convergence checks */
    uint32_t divergence_count;      /**< Divergence event count */

    /* Timing */
    uint64_t total_time_us;         /**< Total learning time (microseconds) */
    uint64_t last_update_time_us;   /**< Last update time */
} fep_learning_stats_t;

/* Forward declaration for callback */
struct fep_transition_learner;

/**
 * @brief Callback invoked after each gradient descent step
 *
 * @param learner The transition learner that was updated
 * @param loss Current loss after update
 * @param user_data Opaque user pointer
 */
typedef void (*fep_model_update_callback_t)(
    struct fep_transition_learner* learner,
    float loss,
    void* user_data
);

/**
 * @brief Transition matrix learner
 *
 * WHAT: Learns state transition dynamics A
 * WHY:  Improves prediction of state evolution
 * HOW:  Gradient descent on ||s_t - A*s_{t-1}||²
 */
typedef struct fep_transition_learner {
    /* Matrix storage (tensor-based) */
    nimcp_tensor_t* matrix;         /**< Transition matrix A (state_dim × state_dim) */
    nimcp_tensor_t* gradient;       /**< Accumulated gradient */
    nimcp_tensor_t* momentum;       /**< Momentum buffer (for momentum/Adam) */
    nimcp_tensor_t* velocity;       /**< Velocity buffer (for Adam) */

    /* Dimensions */
    uint32_t state_dim;             /**< State dimensionality */

    /* Batch accumulation */
    uint32_t batch_count;           /**< Current batch size */
    nimcp_tensor_t* batch_gradient; /**< Batch gradient accumulator */

    /* Configuration */
    fep_learning_config_t config;   /**< Learning configuration */

    /* Statistics */
    fep_learning_stats_t stats;     /**< Learning statistics */

    /* Bio-async integration */
    bio_module_context_t bio_ctx;   /**< Bio-async module context */
    bool bio_async_enabled;         /**< Whether bio-async is active */

    /* Thread safety */
    nimcp_mutex_t* mutex;           /**< Thread synchronization */

    /* Phase 5: Model update callback */
    fep_model_update_callback_t update_callback;
    void* callback_user_data;
} fep_transition_learner_t;

/**
 * @brief Likelihood matrix learner
 *
 * WHAT: Learns observation likelihood B
 * WHY:  Improves prediction of observations from states
 * HOW:  Gradient descent on ||o_t - B*s_t||²
 */
typedef struct {
    /* Matrix storage (tensor-based) */
    nimcp_tensor_t* matrix;         /**< Likelihood matrix B (obs_dim × state_dim) */
    nimcp_tensor_t* gradient;       /**< Accumulated gradient */
    nimcp_tensor_t* momentum;       /**< Momentum buffer */
    nimcp_tensor_t* velocity;       /**< Velocity buffer (for Adam) */

    /* Dimensions */
    uint32_t observation_dim;       /**< Observation dimensionality */
    uint32_t state_dim;             /**< State dimensionality */

    /* Batch accumulation */
    uint32_t batch_count;           /**< Current batch size */
    nimcp_tensor_t* batch_gradient; /**< Batch gradient accumulator */

    /* Configuration */
    fep_learning_config_t config;   /**< Learning configuration */

    /* Statistics */
    fep_learning_stats_t stats;     /**< Learning statistics */

    /* Bio-async integration */
    bio_module_context_t bio_ctx;   /**< Bio-async module context */
    bool bio_async_enabled;         /**< Whether bio-async is active */

    /* Thread safety */
    nimcp_mutex_t* mutex;           /**< Thread synchronization */
} fep_likelihood_learner_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default learning configuration
 *
 * WHAT: Provide sensible default learning parameters
 * WHY:  Easy initialization with biologically-plausible values
 * HOW:  Set standard learning rates and regularization
 *
 * @param config Output configuration
 * @return 0 on success, negative on error
 */
int fep_learning_default_config(fep_learning_config_t* config);

/**
 * @brief Create transition learner
 *
 * WHAT: Initialize transition matrix learner
 * WHY:  Enable learning of state dynamics
 * HOW:  Allocate matrix and gradient buffers
 *
 * @param config Learning configuration (NULL for defaults)
 * @param state_dim State dimensionality
 * @return New learner or NULL on failure
 */
fep_transition_learner_t* fep_transition_learner_create(
    const fep_learning_config_t* config,
    uint32_t state_dim
);

/**
 * @brief Destroy transition learner
 *
 * @param learner Learner to destroy (NULL safe)
 */
void fep_transition_learner_destroy(fep_transition_learner_t* learner);

/**
 * @brief Create likelihood learner
 *
 * WHAT: Initialize likelihood matrix learner
 * WHY:  Enable learning of observation model
 * HOW:  Allocate matrix and gradient buffers
 *
 * @param config Learning configuration (NULL for defaults)
 * @param observation_dim Observation dimensionality
 * @param state_dim State dimensionality
 * @return New learner or NULL on failure
 */
fep_likelihood_learner_t* fep_likelihood_learner_create(
    const fep_learning_config_t* config,
    uint32_t observation_dim,
    uint32_t state_dim
);

/**
 * @brief Destroy likelihood learner
 *
 * @param learner Learner to destroy (NULL safe)
 */
void fep_likelihood_learner_destroy(fep_likelihood_learner_t* learner);

/* ============================================================================
 * Learning API
 * ============================================================================ */

/**
 * @brief Learn from single transition
 *
 * WHAT: Online update for transition matrix
 * WHY:  Immediate learning from experience
 * HOW:  Compute gradient, apply update with regularization
 *
 * Biological basis: Synaptic plasticity occurs on fast timescales
 * after single experiences (immediate early genes, LTP/LTD)
 *
 * @param learner Transition learner
 * @param sys FEP system to update
 * @param state_t Current state s_t
 * @param state_t1 Next state s_{t+1}
 * @param dim State dimension
 * @return 0 on success, negative on error
 */
int fep_learn_transition(
    fep_transition_learner_t* learner,
    fep_system_t* sys,
    const float* state_t,
    const float* state_t1,
    size_t dim
);

/**
 * @brief Learn from batch of transitions
 *
 * WHAT: Batch update for transition matrix
 * WHY:  Stable learning from multiple experiences
 * HOW:  Accumulate gradients, average, apply update
 *
 * Biological basis: Memory consolidation during sleep replays
 * multiple experiences for stable learning
 *
 * @param learner Transition learner
 * @param sys FEP system to update
 * @param states State sequence [state_t, state_t1, ...] (flattened)
 * @param n_transitions Number of transitions
 * @param dim State dimension
 * @return 0 on success, negative on error
 */
int fep_learn_transition_batch(
    fep_transition_learner_t* learner,
    fep_system_t* sys,
    const float* states,
    size_t n_transitions,
    size_t dim
);

/**
 * @brief Learn from single observation-state pair
 *
 * WHAT: Online update for likelihood matrix
 * WHY:  Immediate learning of observation model
 * HOW:  Compute gradient from prediction error
 *
 * @param learner Likelihood learner
 * @param sys FEP system to update
 * @param observation Current observation o_t
 * @param state Current state s_t
 * @param obs_dim Observation dimension
 * @param state_dim State dimension
 * @return 0 on success, negative on error
 */
int fep_learn_likelihood(
    fep_likelihood_learner_t* learner,
    fep_system_t* sys,
    const float* observation,
    const float* state,
    size_t obs_dim,
    size_t state_dim
);

/**
 * @brief Learn from batch of observation-state pairs
 *
 * WHAT: Batch update for likelihood matrix
 * WHY:  Stable learning from multiple observations
 * HOW:  Accumulate gradients, average, apply update
 *
 * @param learner Likelihood learner
 * @param sys FEP system to update
 * @param observations Observation sequence (flattened)
 * @param states State sequence (flattened)
 * @param n_pairs Number of observation-state pairs
 * @param obs_dim Observation dimension
 * @param state_dim State dimension
 * @return 0 on success, negative on error
 */
int fep_learn_likelihood_batch(
    fep_likelihood_learner_t* learner,
    fep_system_t* sys,
    const float* observations,
    const float* states,
    size_t n_pairs,
    size_t obs_dim,
    size_t state_dim
);

/* ============================================================================
 * Matrix Access API
 * ============================================================================ */

/**
 * @brief Get learned transition matrix
 *
 * WHAT: Retrieve current transition matrix
 * WHY:  Inspect learned dynamics
 * HOW:  Copy matrix values to output buffer
 *
 * @param learner Transition learner
 * @param matrix Output matrix buffer (dim × dim)
 * @param dim State dimension
 * @return 0 on success, negative on error
 */
int fep_get_learned_transition(
    const fep_transition_learner_t* learner,
    float* matrix,
    size_t dim
);

/**
 * @brief Get learned likelihood matrix
 *
 * WHAT: Retrieve current likelihood matrix
 * WHY:  Inspect learned observation model
 * HOW:  Copy matrix values to output buffer
 *
 * @param learner Likelihood learner
 * @param matrix Output matrix buffer (obs_dim × state_dim)
 * @param obs_dim Observation dimension
 * @param state_dim State dimension
 * @return 0 on success, negative on error
 */
int fep_get_learned_likelihood(
    const fep_likelihood_learner_t* learner,
    float* matrix,
    size_t obs_dim,
    size_t state_dim
);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Apply learned transition matrix to FEP system
 *
 * WHAT: Update FEP system with learned dynamics
 * WHY:  Integrate learning results into inference
 * HOW:  Copy learned matrix to FEP hierarchy level
 *
 * @param learner Transition learner
 * @param sys FEP system to update
 * @return 0 on success, negative on error
 */
int fep_apply_learned_transition(
    fep_transition_learner_t* learner,
    fep_system_t* sys
);

/**
 * @brief Apply learned likelihood matrix to FEP system
 *
 * WHAT: Update FEP system with learned observation model
 * WHY:  Integrate learning results into inference
 * HOW:  Copy learned matrix to FEP hierarchy level
 *
 * @param learner Likelihood learner
 * @param sys FEP system to update
 * @return 0 on success, negative on error
 */
int fep_apply_learned_likelihood(
    fep_likelihood_learner_t* learner,
    fep_system_t* sys
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get transition learning statistics
 *
 * @param learner Transition learner
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int fep_transition_learning_get_stats(
    const fep_transition_learner_t* learner,
    fep_learning_stats_t* stats
);

/**
 * @brief Get likelihood learning statistics
 *
 * @param learner Likelihood learner
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int fep_likelihood_learning_get_stats(
    const fep_likelihood_learner_t* learner,
    fep_learning_stats_t* stats
);

/**
 * @brief Reset learning statistics
 *
 * @param learner Transition or likelihood learner
 * @return 0 on success, negative on error
 */
int fep_learning_reset_stats(void* learner);

/* ============================================================================
 * Bio-async Integration API
 * ============================================================================ */

/**
 * @brief Connect transition learner to bio-async router
 *
 * WHAT: Enable inter-module messaging for learning events
 * WHY:  Coordinate with other modules during learning
 * HOW:  Register with bio-async router, allocate inbox
 *
 * @param learner Transition learner
 * @return 0 on success, negative on error
 */
int fep_transition_learner_connect_bio_async(fep_transition_learner_t* learner);

/**
 * @brief Disconnect transition learner from bio-async
 *
 * @param learner Transition learner
 * @return 0 on success, negative on error
 */
int fep_transition_learner_disconnect_bio_async(fep_transition_learner_t* learner);

/**
 * @brief Connect likelihood learner to bio-async router
 *
 * @param learner Likelihood learner
 * @return 0 on success, negative on error
 */
int fep_likelihood_learner_connect_bio_async(fep_likelihood_learner_t* learner);

/**
 * @brief Disconnect likelihood learner from bio-async
 *
 * @param learner Likelihood learner
 * @return 0 on success, negative on error
 */
int fep_likelihood_learner_disconnect_bio_async(fep_likelihood_learner_t* learner);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

/**
 * @brief Convert optimizer type to string
 *
 * @param type Optimizer type
 * @return Human-readable string
 */
const char* fep_optimizer_type_to_string(fep_optimizer_type_t type);

/**
 * @brief Convert learning state to string
 *
 * @param state Learning state
 * @return Human-readable string
 */
const char* fep_learning_state_to_string(fep_learning_state_t state);

/**
 * @brief Set callback for post-update notification
 *
 * @param learner Transition learner
 * @param callback Function to call after each update (NULL to disable)
 * @param user_data Opaque user pointer passed to callback
 * @return 0 on success, negative on error
 */
int fep_transition_learner_set_update_callback(
    fep_transition_learner_t* learner,
    fep_model_update_callback_t callback,
    void* user_data
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FEP_LEARNING_H */
