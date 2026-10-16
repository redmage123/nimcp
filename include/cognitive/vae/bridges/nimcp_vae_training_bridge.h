/**
 * @file nimcp_vae_training_bridge.h
 * @brief Bridge between VAE and Training System for Integrated Learning
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Integrates VAE training with SNN/neural network training systems
 *
 * WHY:  Unified training enables:
 *       - Joint VAE + SNN loss optimization
 *       - VAE decoder as prediction target for SNN
 *       - ELBO as auxiliary loss for spike-based learning
 *       - E-prop integration with VAE gradients
 *       - Surrogate gradient methods with VAE sampling
 *
 * HOW:  Bridge coordinates training across modalities:
 *       - Forward: Input → VAE encoder → latent → SNN → output
 *       - Backward: Loss → surrogate grads → VAE grads
 *       - Joint: Combined ELBO + spike loss optimization
 *
 * TRAINING MODES:
 * ==============
 * - E-prop with VAE latent eligibility
 * - Surrogate gradients through VAE sampling
 * - BPTT with VAE reconstruction targets
 * - Hybrid STDP + VAE prediction error
 *
 * BIO_MODULE: 0x1F1D (VAE-Training Bridge)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_VAE_TRAINING_BRIDGE_H
#define NIMCP_VAE_TRAINING_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/vae/nimcp_vae.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define VAE_TRAINING_BRIDGE_VERSION    "1.0.0"
#define BIO_MODULE_VAE_TRAINING_BRIDGE 0x1F1D

/** Maximum training batch size */
#define VAE_TRAINING_MAX_BATCH         256

/** Maximum sequence length for BPTT */
#define VAE_TRAINING_MAX_SEQ_LEN       1000

/** Error code range (32540-32549) */
#define NIMCP_ERROR_VAE_TRAIN_BASE          32540
#define NIMCP_ERROR_VAE_TRAIN_NULL          32541
#define NIMCP_ERROR_VAE_TRAIN_NOT_CONNECTED 32542
#define NIMCP_ERROR_VAE_TRAIN_FORWARD_FAIL  32543
#define NIMCP_ERROR_VAE_TRAIN_BACKWARD_FAIL 32544
#define NIMCP_ERROR_VAE_TRAIN_NO_MEMORY     32545
#define NIMCP_ERROR_VAE_TRAIN_NAN_GRADIENT  32546

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Training algorithm
 */
typedef enum {
    VAE_TRAIN_JOINT = 0,          /**< Joint VAE + SNN training */
    VAE_TRAIN_ALTERNATING,         /**< Alternating VAE/SNN updates */
    VAE_TRAIN_VAE_FIRST,           /**< Train VAE, freeze for SNN */
    VAE_TRAIN_SNN_FIRST,           /**< Train SNN, then fine-tune VAE */
    VAE_TRAIN_EPROP_VAE,           /**< E-prop with VAE eligibility */
    VAE_TRAIN_SURROGATE_VAE        /**< Surrogate grads through VAE */
} vae_training_algorithm_t;

/**
 * @brief Loss combination method
 */
typedef enum {
    VAE_LOSS_SUM = 0,             /**< Sum all losses */
    VAE_LOSS_WEIGHTED,             /**< Weighted combination */
    VAE_LOSS_DYNAMIC,              /**< Dynamically balanced */
    VAE_LOSS_UNCERTAINTY           /**< Uncertainty-weighted (Kendall) */
} vae_loss_combination_t;

/**
 * @brief Gradient flow mode
 */
typedef enum {
    VAE_GRAD_FLOW_FULL = 0,       /**< Full gradient flow */
    VAE_GRAD_FLOW_DETACH_LATENT,  /**< Detach at latent sampling */
    VAE_GRAD_FLOW_DETACH_DECODER, /**< Detach decoder gradients */
    VAE_GRAD_FLOW_STOP_GRAD       /**< Stop gradient to VAE */
} vae_gradient_flow_t;

/**
 * @brief Surrogate gradient method for spikes
 */
typedef enum {
    VAE_SURROGATE_SUPERSPIKE = 0, /**< SuperSpike (1/(beta|x|+1)^2) */
    VAE_SURROGATE_FAST_SIGMOID,   /**< Fast sigmoid (x/(1+|x|)^2) */
    VAE_SURROGATE_SIGMOID,        /**< Sigmoid derivative */
    VAE_SURROGATE_ARCTAN,         /**< Arctan derivative */
    VAE_SURROGATE_TRIANGULAR,     /**< Triangular */
    VAE_SURROGATE_STE             /**< Straight-through estimator */
} vae_surrogate_method_t;

/**
 * @brief Bridge state
 */
typedef enum {
    VAE_TRAIN_STATE_DISCONNECTED = 0,
    VAE_TRAIN_STATE_READY,
    VAE_TRAIN_STATE_FORWARD,
    VAE_TRAIN_STATE_BACKWARD,
    VAE_TRAIN_STATE_UPDATING,
    VAE_TRAIN_STATE_ERROR
} vae_training_bridge_state_t;

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

/**
 * @brief VAE-specific loss configuration
 */
typedef struct {
    float beta;                   /**< Beta for beta-VAE (KL weight) */
    float recon_weight;           /**< Reconstruction loss weight */
    float kl_annealing_rate;      /**< KL annealing during warmup */
    float kl_free_bits;           /**< Free bits for KL (prevents collapse) */
    bool use_mse;                 /**< MSE vs BCE for reconstruction */
} vae_training_vae_loss_config_t;

/**
 * @brief SNN-specific loss configuration
 */
typedef struct {
    float spike_count_weight;     /**< Weight for spike count loss */
    float timing_weight;          /**< Weight for spike timing loss */
    float rate_weight;            /**< Weight for rate-coded loss */
    float membrane_weight;        /**< Weight for membrane potential loss */
    float regularization;         /**< Activity regularization */
} vae_training_snn_loss_config_t;

/**
 * @brief Surrogate gradient configuration
 */
typedef struct {
    vae_surrogate_method_t method;
    float beta;                   /**< Steepness parameter */
    float threshold;              /**< Spike threshold */
    bool learn_beta;              /**< Learn beta parameter */
} vae_training_surrogate_config_t;

/**
 * @brief E-prop configuration
 */
typedef struct {
    float eligibility_decay;      /**< Eligibility trace decay */
    bool use_vae_eligibility;     /**< VAE modulates eligibility */
    float latent_trace_decay;     /**< Decay for latent-based traces */
    bool symmetric_eprop;         /**< Symmetric E-prop (RTRL approx) */
} vae_training_eprop_config_t;

/**
 * @brief Optimizer configuration
 */
typedef struct {
    float learning_rate;
    float weight_decay;
    float momentum;               /**< For SGD with momentum */
    float beta1;                  /**< Adam beta1 */
    float beta2;                  /**< Adam beta2 */
    float epsilon;                /**< Adam epsilon */
    bool use_gradient_clipping;
    float gradient_clip_norm;
} vae_training_optimizer_config_t;

/**
 * @brief Main bridge configuration
 */
typedef struct {
    vae_training_algorithm_t algorithm;
    vae_loss_combination_t loss_combination;
    vae_gradient_flow_t gradient_flow;

    /* Loss configs */
    vae_training_vae_loss_config_t vae_loss;
    vae_training_snn_loss_config_t snn_loss;

    /* Method configs */
    vae_training_surrogate_config_t surrogate;
    vae_training_eprop_config_t eprop;
    vae_training_optimizer_config_t optimizer;

    /* Training parameters */
    uint32_t batch_size;
    uint32_t sequence_length;     /**< For temporal training */
    uint32_t warmup_steps;        /**< KL annealing warmup */

    /* Dynamic loss balancing */
    bool use_uncertainty_weighting;
    float vae_loss_weight;        /**< Weight for VAE loss */
    float snn_loss_weight;        /**< Weight for SNN loss */

    bool enable_logging;
} vae_training_bridge_config_t;

/* ============================================================================
 * Result Structures
 * ============================================================================ */

/**
 * @brief Forward pass result
 */
typedef struct {
    float* latent_mu;
    float* latent_log_var;
    float* latent_sample;
    uint32_t latent_dim;
    float* snn_output;
    uint32_t snn_output_dim;
    float* reconstruction;
    uint32_t recon_dim;
    float forward_time_us;
} vae_training_forward_result_t;

/**
 * @brief Loss computation result
 */
typedef struct {
    float total_loss;
    float vae_loss;
    float recon_loss;
    float kl_loss;
    float snn_loss;
    float spike_loss;
    float timing_loss;
    float regularization_loss;
    float elbo;
} vae_training_loss_result_t;

/**
 * @brief Backward pass result
 */
typedef struct {
    float* vae_encoder_grads;
    float* vae_decoder_grads;
    float* snn_grads;
    float grad_norm;
    float grad_max;
    bool has_nan;
    float backward_time_us;
} vae_training_backward_result_t;

/**
 * @brief Training step result
 */
typedef struct {
    vae_training_loss_result_t loss;
    float grad_norm;
    float effective_lr;
    uint64_t step_number;
    float step_time_us;
} vae_training_step_result_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

typedef struct {
    uint64_t total_steps;
    uint64_t total_forward;
    uint64_t total_backward;
    float avg_total_loss;
    float avg_vae_loss;
    float avg_snn_loss;
    float avg_kl_divergence;
    float avg_recon_error;
    float avg_grad_norm;
    float min_loss_observed;
    float current_learning_rate;
    uint64_t nan_gradient_count;
    uint64_t creation_time_us;
    uint64_t last_step_us;
} vae_training_bridge_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

typedef struct vae_training_bridge {
    vae_training_bridge_config_t config;
    vae_system_t* vae;
    void* snn_trainer;            /**< SNN training context */
    void* optimizer;              /**< Optimizer state */
    vae_training_bridge_state_t state;
    bool is_initialized;

    /* Current training state */
    uint64_t current_step;
    float current_kl_weight;      /**< For KL annealing */
    float current_lr;

    /* Gradient buffers */
    float* vae_grad_buffer;
    float* snn_grad_buffer;
    float* combined_grad_buffer;

    /* Eligibility traces (for E-prop) */
    float* eligibility_traces;
    uint32_t num_traces;

    /* Loss history (for dynamic weighting) */
    float* vae_loss_history;
    float* snn_loss_history;
    uint32_t history_head;
    uint32_t history_size;

    /* Uncertainty weights (for uncertainty weighting) */
    float log_var_vae;
    float log_var_snn;

    /* Statistics */
    vae_training_bridge_stats_t stats;
    uint64_t creation_time_us;
} vae_training_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int vae_training_bridge_default_config(vae_training_bridge_config_t* config);
vae_training_bridge_t* vae_training_bridge_create(const vae_training_bridge_config_t* config);
void vae_training_bridge_destroy(vae_training_bridge_t* bridge);
int vae_training_bridge_connect_vae(vae_training_bridge_t* bridge, vae_system_t* vae);
int vae_training_bridge_connect_trainer(vae_training_bridge_t* bridge, void* snn_trainer);
int vae_training_bridge_disconnect(vae_training_bridge_t* bridge);
bool vae_training_bridge_is_connected(const vae_training_bridge_t* bridge);

/* ============================================================================
 * Training API
 * ============================================================================ */

int vae_training_step(vae_training_bridge_t* bridge,
                       const float* input, uint32_t input_dim,
                       const float* target, uint32_t target_dim,
                       vae_training_step_result_t* result);

int vae_training_forward(vae_training_bridge_t* bridge,
                          const float* input, uint32_t input_dim,
                          vae_training_forward_result_t* result);

int vae_training_compute_loss(vae_training_bridge_t* bridge,
                               const vae_training_forward_result_t* forward,
                               const float* target, uint32_t target_dim,
                               vae_training_loss_result_t* result);

int vae_training_backward(vae_training_bridge_t* bridge,
                           const vae_training_loss_result_t* loss,
                           vae_training_backward_result_t* result);

int vae_training_update_weights(vae_training_bridge_t* bridge,
                                 const vae_training_backward_result_t* backward);

/* ============================================================================
 * Batch Training API
 * ============================================================================ */

int vae_training_batch_step(vae_training_bridge_t* bridge,
                             const float* inputs, uint32_t input_dim,
                             const float* targets, uint32_t target_dim,
                             uint32_t batch_size,
                             vae_training_step_result_t* result);

int vae_training_sequence_step(vae_training_bridge_t* bridge,
                                const float* sequence, uint32_t input_dim,
                                uint32_t seq_len,
                                const float* targets, uint32_t target_dim,
                                vae_training_step_result_t* result);

/* ============================================================================
 * E-prop API
 * ============================================================================ */

int vae_training_eprop_update(vae_training_bridge_t* bridge,
                               const float* spike_data,
                               float reward_signal);

int vae_training_update_eligibility(vae_training_bridge_t* bridge,
                                     const float* latent_delta);

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int vae_training_set_learning_rate(vae_training_bridge_t* bridge, float lr);
int vae_training_set_kl_weight(vae_training_bridge_t* bridge, float beta);
int vae_training_set_loss_weights(vae_training_bridge_t* bridge,
                                   float vae_weight, float snn_weight);
int vae_training_anneal_kl(vae_training_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

vae_training_bridge_state_t vae_training_bridge_get_state(const vae_training_bridge_t* bridge);
int vae_training_bridge_get_stats(const vae_training_bridge_t* bridge,
                                   vae_training_bridge_stats_t* stats);
float vae_training_get_current_lr(const vae_training_bridge_t* bridge);
uint64_t vae_training_get_step_count(const vae_training_bridge_t* bridge);
const char* vae_training_algorithm_to_string(vae_training_algorithm_t alg);

/* ============================================================================
 * Result Management
 * ============================================================================ */

void vae_training_forward_result_free(vae_training_forward_result_t* result);
void vae_training_backward_result_free(vae_training_backward_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VAE_TRAINING_BRIDGE_H */
