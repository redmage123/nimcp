/**
 * @file nimcp_vae_decoder.h
 * @brief VAE Decoder Network - Maps latent to reconstruction
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Decoder network p(x|z) that maps latent samples back to
 *       reconstructions in the original data space.
 *
 * WHY:  The decoder performs "generation" - given a latent state, it
 *       predicts the most likely observation. This maps directly to
 *       the generative model in FEP.
 *
 * HOW:  Multi-layer neural network mirroring the encoder:
 *       - Configurable hidden layers with activations
 *       - Output layer matching original data dimension
 *       - Optional variance output for heteroscedastic models
 *
 * ARCHITECTURE:
 * ```
 *   Latent z [latent_dim]
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
 *       v
 *   +-------------------+
 *   | Output Layer      |
 *   +-------------------+
 *       |
 *       v
 *   Reconstruction [output_dim]
 * ```
 *
 * BIO_MODULE: 0x1F02
 */

#ifndef NIMCP_VAE_DECODER_H
#define NIMCP_VAE_DECODER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/vae/nimcp_vae.h"
#include "utils/tensor/nimcp_tensor.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Single decoder layer
 */
typedef struct vae_decoder_layer {
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
} vae_decoder_layer_t;

/**
 * @brief VAE Decoder network
 */
typedef struct vae_decoder {
    /* Configuration */
    vae_decoder_config_t config;

    /* Layers */
    vae_decoder_layer_t* layers;     /**< Hidden layers */
    uint32_t num_layers;             /**< Number of hidden layers */

    /* Output layer */
    nimcp_tensor_t* output_weights;  /**< Output weights [last_hidden, output_dim] */
    nimcp_tensor_t* output_bias;     /**< Output bias [output_dim] */
    nimcp_tensor_t* output_weights_grad;
    nimcp_tensor_t* output_bias_grad;

    /* Optional variance output (for heteroscedastic models) */
    nimcp_tensor_t* var_weights;     /**< Variance weights */
    nimcp_tensor_t* var_bias;        /**< Variance bias */
    nimcp_tensor_t* var_weights_grad;
    nimcp_tensor_t* var_bias_grad;

    /* State */
    bool is_training;                /**< Training mode flag */
    bool is_initialized;             /**< Initialization flag */

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Statistics */
    uint64_t forward_calls;          /**< Forward pass count */
    uint64_t backward_calls;         /**< Backward pass count */
    float avg_forward_time_us;       /**< Average forward time */
} vae_decoder_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create decoder with configuration
 *
 * @param config Decoder configuration
 * @return Decoder instance or NULL on error
 */
vae_decoder_t* vae_decoder_create(const vae_decoder_config_t* config);

/**
 * @brief Destroy decoder and free resources
 *
 * @param decoder Decoder to destroy (NULL safe)
 */
void vae_decoder_destroy(vae_decoder_t* decoder);

/**
 * @brief Reset decoder state (keep weights)
 *
 * @param decoder Decoder instance
 * @return 0 on success, -1 on error
 */
int vae_decoder_reset(vae_decoder_t* decoder);

/**
 * @brief Initialize decoder weights
 *
 * Uses Xavier/Glorot initialization for weights.
 *
 * @param decoder Decoder instance
 * @param seed Random seed (0 for time-based)
 * @return 0 on success, -1 on error
 */
int vae_decoder_init_weights(vae_decoder_t* decoder, uint64_t seed);

/* ============================================================================
 * Forward Pass API
 * ============================================================================ */

/**
 * @brief Forward pass through decoder
 *
 * Computes reconstruction from latent samples.
 *
 * @param decoder Decoder instance
 * @param z Latent tensor [batch_size, latent_dim]
 * @param reconstruction Output reconstruction [batch_size, output_dim]
 * @return 0 on success, -1 on error
 */
int vae_decoder_forward(vae_decoder_t* decoder,
                        const nimcp_tensor_t* z,
                        nimcp_tensor_t* reconstruction);

/**
 * @brief Forward pass with variance output (heteroscedastic)
 *
 * @param decoder Decoder instance
 * @param z Latent tensor [batch_size, latent_dim]
 * @param reconstruction Output reconstruction [batch_size, output_dim]
 * @param log_var Output log variance [batch_size, output_dim] (optional)
 * @return 0 on success, -1 on error
 */
int vae_decoder_forward_hetero(vae_decoder_t* decoder,
                               const nimcp_tensor_t* z,
                               nimcp_tensor_t* reconstruction,
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
int vae_decoder_layer_forward(vae_decoder_layer_t* layer,
                              const nimcp_tensor_t* input,
                              nimcp_tensor_t* output,
                              bool training);

/* ============================================================================
 * Backward Pass API
 * ============================================================================ */

/**
 * @brief Backward pass through decoder
 *
 * Computes gradients with respect to all parameters.
 *
 * @param decoder Decoder instance
 * @param d_reconstruction Gradient w.r.t. reconstruction [batch_size, output_dim]
 * @param d_z Output gradient w.r.t. latent (can be NULL)
 * @return 0 on success, -1 on error
 */
int vae_decoder_backward(vae_decoder_t* decoder,
                         const nimcp_tensor_t* d_reconstruction,
                         nimcp_tensor_t* d_z);

/**
 * @brief Backward through single layer
 *
 * @param layer Layer instance
 * @param d_output Gradient from next layer
 * @param d_input Output gradient to previous layer
 * @return 0 on success, -1 on error
 */
int vae_decoder_layer_backward(vae_decoder_layer_t* layer,
                               const nimcp_tensor_t* d_output,
                               nimcp_tensor_t* d_input);

/* ============================================================================
 * Training API
 * ============================================================================ */

/**
 * @brief Set training mode
 *
 * @param decoder Decoder instance
 * @param training True for training, false for inference
 * @return 0 on success, -1 on error
 */
int vae_decoder_set_training(vae_decoder_t* decoder, bool training);

/**
 * @brief Apply gradients to update weights
 *
 * @param decoder Decoder instance
 * @param learning_rate Learning rate
 * @return 0 on success, -1 on error
 */
int vae_decoder_apply_gradients(vae_decoder_t* decoder, float learning_rate);

/**
 * @brief Zero all gradients
 *
 * @param decoder Decoder instance
 * @return 0 on success, -1 on error
 */
int vae_decoder_zero_grad(vae_decoder_t* decoder);

/**
 * @brief Clip gradients by norm
 *
 * @param decoder Decoder instance
 * @param max_norm Maximum gradient norm
 * @return 0 on success, -1 on error
 */
int vae_decoder_clip_gradients(vae_decoder_t* decoder, float max_norm);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get total parameter count
 *
 * @param decoder Decoder instance
 * @return Total number of trainable parameters
 */
uint64_t vae_decoder_param_count(const vae_decoder_t* decoder);

/**
 * @brief Get gradient norm
 *
 * @param decoder Decoder instance
 * @return L2 norm of all gradients
 */
float vae_decoder_grad_norm(const vae_decoder_t* decoder);

/**
 * @brief Check for NaN in weights or gradients
 *
 * @param decoder Decoder instance
 * @return true if NaN detected
 */
bool vae_decoder_has_nan(const vae_decoder_t* decoder);

/**
 * @brief Get layer output dimension
 *
 * @param decoder Decoder instance
 * @param layer_idx Layer index
 * @return Output dimension or 0 on error
 */
uint32_t vae_decoder_get_layer_dim(const vae_decoder_t* decoder, uint32_t layer_idx);

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

/**
 * @brief Set health agent for decoder
 *
 * @param decoder Decoder instance
 * @param agent Health agent
 */
void vae_decoder_set_health_agent(vae_decoder_t* decoder,
                                  nimcp_health_agent_t* agent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VAE_DECODER_H */
