/**
 * @file nimcp_vae_encoder.h
 * @brief VAE Encoder Network - Maps input to latent distribution
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Encoder network q(z|x) that maps input data to parameters of the
 *       approximate posterior distribution in latent space.
 *
 * WHY:  The encoder performs "recognition" or "inference" - given an
 *       observation, it infers the most likely latent state. This maps
 *       directly to the recognition density in FEP.
 *
 * HOW:  Multi-layer neural network with:
 *       - Configurable hidden layers with activations
 *       - Two output heads: mean (mu) and log-variance (log_var)
 *       - Optional dropout and batch normalization
 *
 * ARCHITECTURE:
 * ```
 *   Input x [input_dim]
 *       |
 *       v
 *   +-------------------+
 *   | Hidden Layer 1    |  (units, activation, dropout, batch_norm)
 *   +-------------------+
 *       |
 *       v
 *   +-------------------+
 *   | Hidden Layer 2    |
 *   +-------------------+
 *       |
 *       v
 *      ...
 *       |
 *       +--------+--------+
 *       |                 |
 *       v                 v
 *   +--------+       +-----------+
 *   | mu     |       | log_var   |
 *   | head   |       | head      |
 *   +--------+       +-----------+
 *       |                 |
 *       v                 v
 *   mu [latent_dim]   log_var [latent_dim]
 * ```
 *
 * BIO_MODULE: 0x1F01
 */

#ifndef NIMCP_VAE_ENCODER_H
#define NIMCP_VAE_ENCODER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/vae/nimcp_vae.h"
#include "utils/tensor/nimcp_tensor.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Default leaky ReLU alpha */
#define VAE_ENCODER_LEAKY_ALPHA         0.01f

/** Default ELU alpha */
#define VAE_ENCODER_ELU_ALPHA           1.0f

/** Batch norm epsilon */
#define VAE_ENCODER_BN_EPSILON          1e-5f

/** Batch norm momentum */
#define VAE_ENCODER_BN_MOMENTUM         0.1f

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Single encoder layer
 */
typedef struct vae_encoder_layer {
    nimcp_tensor_t* weights;         /**< Weight matrix [in_dim, out_dim] */
    nimcp_tensor_t* bias;            /**< Bias vector [out_dim] */
    nimcp_tensor_t* weight_grad;     /**< Weight gradients */
    nimcp_tensor_t* bias_grad;       /**< Bias gradients */

    /* Batch normalization parameters */
    nimcp_tensor_t* bn_gamma;        /**< Scale parameter */
    nimcp_tensor_t* bn_beta;         /**< Shift parameter */
    nimcp_tensor_t* bn_running_mean; /**< Running mean */
    nimcp_tensor_t* bn_running_var;  /**< Running variance */

    /* Layer config */
    uint32_t in_dim;                 /**< Input dimension */
    uint32_t out_dim;                /**< Output dimension */
    vae_activation_t activation;     /**< Activation function */
    float dropout_rate;              /**< Dropout rate */
    bool batch_norm;                 /**< Batch normalization enabled */
    bool use_bias;                   /**< Use bias terms */

    /* Forward pass cache (for backprop) */
    nimcp_tensor_t* pre_activation;  /**< Pre-activation values */
    nimcp_tensor_t* post_activation; /**< Post-activation values */
    nimcp_tensor_t* dropout_mask;    /**< Dropout mask */
    nimcp_tensor_t* bn_cache_mean;   /**< Batch mean (training) */
    nimcp_tensor_t* bn_cache_var;    /**< Batch variance (training) */
} vae_encoder_layer_t;

/**
 * @brief VAE Encoder network
 */
typedef struct vae_encoder {
    /* Configuration */
    vae_encoder_config_t config;

    /* Layers */
    vae_encoder_layer_t* layers;     /**< Hidden layers */
    uint32_t num_layers;             /**< Number of hidden layers */

    /* Output heads */
    nimcp_tensor_t* mu_weights;      /**< Mean output weights */
    nimcp_tensor_t* mu_bias;         /**< Mean output bias */
    nimcp_tensor_t* logvar_weights;  /**< Log-var output weights */
    nimcp_tensor_t* logvar_bias;     /**< Log-var output bias */

    /* Gradient storage for output heads */
    nimcp_tensor_t* mu_weights_grad;
    nimcp_tensor_t* mu_bias_grad;
    nimcp_tensor_t* logvar_weights_grad;
    nimcp_tensor_t* logvar_bias_grad;

    /* State */
    bool is_training;                /**< Training mode flag */
    bool is_initialized;             /**< Initialization flag */

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Statistics */
    uint64_t forward_calls;          /**< Forward pass count */
    uint64_t backward_calls;         /**< Backward pass count */
    float avg_forward_time_us;       /**< Average forward time */
} vae_encoder_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create encoder with configuration
 *
 * @param config Encoder configuration
 * @return Encoder instance or NULL on error
 */
vae_encoder_t* vae_encoder_create(const vae_encoder_config_t* config);

/**
 * @brief Destroy encoder and free resources
 *
 * @param encoder Encoder to destroy (NULL safe)
 */
void vae_encoder_destroy(vae_encoder_t* encoder);

/**
 * @brief Reset encoder state (keep weights)
 *
 * @param encoder Encoder instance
 * @return 0 on success, -1 on error
 */
int vae_encoder_reset(vae_encoder_t* encoder);

/**
 * @brief Initialize encoder weights
 *
 * Uses Xavier/Glorot initialization for weights.
 *
 * @param encoder Encoder instance
 * @param seed Random seed (0 for time-based)
 * @return 0 on success, -1 on error
 */
int vae_encoder_init_weights(vae_encoder_t* encoder, uint64_t seed);

/* ============================================================================
 * Forward Pass API
 * ============================================================================ */

/**
 * @brief Forward pass through encoder
 *
 * Computes mu and log_var from input.
 *
 * @param encoder Encoder instance
 * @param input Input tensor [batch_size, input_dim]
 * @param mu Output mean [batch_size, latent_dim]
 * @param log_var Output log variance [batch_size, latent_dim]
 * @return 0 on success, -1 on error
 */
int vae_encoder_forward(vae_encoder_t* encoder,
                        const nimcp_tensor_t* input,
                        nimcp_tensor_t* mu,
                        nimcp_tensor_t* log_var);

/**
 * @brief Forward through single layer
 *
 * @param layer Layer instance
 * @param input Input tensor
 * @param output Output tensor
 * @param training Training mode flag
 * @return 0 on success, -1 on error
 */
int vae_encoder_layer_forward(vae_encoder_layer_t* layer,
                              const nimcp_tensor_t* input,
                              nimcp_tensor_t* output,
                              bool training);

/* ============================================================================
 * Backward Pass API
 * ============================================================================ */

/**
 * @brief Backward pass through encoder
 *
 * Computes gradients with respect to all parameters.
 *
 * @param encoder Encoder instance
 * @param d_mu Gradient w.r.t. mu [batch_size, latent_dim]
 * @param d_log_var Gradient w.r.t. log_var [batch_size, latent_dim]
 * @param d_input Output gradient w.r.t. input (can be NULL)
 * @return 0 on success, -1 on error
 */
int vae_encoder_backward(vae_encoder_t* encoder,
                         const nimcp_tensor_t* d_mu,
                         const nimcp_tensor_t* d_log_var,
                         nimcp_tensor_t* d_input);

/**
 * @brief Backward through single layer
 *
 * @param layer Layer instance
 * @param d_output Gradient from next layer
 * @param d_input Output gradient to previous layer
 * @return 0 on success, -1 on error
 */
int vae_encoder_layer_backward(vae_encoder_layer_t* layer,
                               const nimcp_tensor_t* d_output,
                               nimcp_tensor_t* d_input);

/* ============================================================================
 * Training API
 * ============================================================================ */

/**
 * @brief Set training mode
 *
 * @param encoder Encoder instance
 * @param training True for training, false for inference
 * @return 0 on success, -1 on error
 */
int vae_encoder_set_training(vae_encoder_t* encoder, bool training);

/**
 * @brief Apply gradients to update weights
 *
 * @param encoder Encoder instance
 * @param learning_rate Learning rate
 * @return 0 on success, -1 on error
 */
int vae_encoder_apply_gradients(vae_encoder_t* encoder, float learning_rate);

/**
 * @brief Zero all gradients
 *
 * @param encoder Encoder instance
 * @return 0 on success, -1 on error
 */
int vae_encoder_zero_grad(vae_encoder_t* encoder);

/**
 * @brief Clip gradients by norm
 *
 * @param encoder Encoder instance
 * @param max_norm Maximum gradient norm
 * @return 0 on success, -1 on error
 */
int vae_encoder_clip_gradients(vae_encoder_t* encoder, float max_norm);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get total parameter count
 *
 * @param encoder Encoder instance
 * @return Total number of trainable parameters
 */
uint64_t vae_encoder_param_count(const vae_encoder_t* encoder);

/**
 * @brief Get gradient norm
 *
 * @param encoder Encoder instance
 * @return L2 norm of all gradients
 */
float vae_encoder_grad_norm(const vae_encoder_t* encoder);

/**
 * @brief Check for NaN in weights or gradients
 *
 * @param encoder Encoder instance
 * @return true if NaN detected
 */
bool vae_encoder_has_nan(const vae_encoder_t* encoder);

/**
 * @brief Get layer output dimension
 *
 * @param encoder Encoder instance
 * @param layer_idx Layer index
 * @return Output dimension or 0 on error
 */
uint32_t vae_encoder_get_layer_dim(const vae_encoder_t* encoder, uint32_t layer_idx);

/* ============================================================================
 * Activation Functions
 * ============================================================================ */

/**
 * @brief Apply activation function
 *
 * @param activation Activation type
 * @param input Input tensor
 * @param output Output tensor
 * @return 0 on success, -1 on error
 */
int vae_activation_forward(vae_activation_t activation,
                           const nimcp_tensor_t* input,
                           nimcp_tensor_t* output);

/**
 * @brief Compute activation gradient
 *
 * @param activation Activation type
 * @param input Pre-activation input
 * @param grad_output Gradient from next layer
 * @param grad_input Output gradient
 * @return 0 on success, -1 on error
 */
int vae_activation_backward(vae_activation_t activation,
                            const nimcp_tensor_t* input,
                            const nimcp_tensor_t* grad_output,
                            nimcp_tensor_t* grad_input);

/**
 * @brief Get activation name as string
 *
 * @param activation Activation type
 * @return Activation name
 */
const char* vae_activation_to_string(vae_activation_t activation);

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

/**
 * @brief Set health agent for encoder
 *
 * @param encoder Encoder instance
 * @param agent Health agent
 */
void vae_encoder_set_health_agent(vae_encoder_t* encoder,
                                  nimcp_health_agent_t* agent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VAE_ENCODER_H */
