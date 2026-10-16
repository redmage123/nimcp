/**
 * @file nimcp_vae.h
 * @brief Variational Autoencoder (VAE) Module for NIMCP
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Variational Autoencoder providing learned latent representations
 *       with uncertainty quantification for cognitive processing.
 *
 * WHY:  VAE enables:
 *       - Efficient compression of sensory/cognitive data
 *       - Generative sampling for imagination and dreaming
 *       - Uncertainty estimation via latent variance
 *       - Anomaly detection via reconstruction error
 *       - Direct integration with Free Energy Principle
 *
 * HOW:  Encoder maps inputs to latent distribution parameters (mu, sigma).
 *       Reparameterization trick enables gradient flow through sampling.
 *       Decoder reconstructs from latent samples.
 *       ELBO loss = Reconstruction + beta * KL divergence.
 *
 * THEORETICAL FOUNDATION:
 * ==================================================================================
 *
 * VARIATIONAL AUTOENCODER (Kingma & Welling, 2014):
 * -------------------------------------------------
 * VAE learns a probabilistic encoder q(z|x) and decoder p(x|z):
 *
 *   ELBO = E_q[log p(x|z)] - KL[q(z|x) || p(z)]
 *        = Reconstruction - Complexity
 *
 * The ELBO is mathematically equivalent to the negative free energy:
 *
 *   F = -ELBO = -E_q[log p(x|z)] + KL[q(z|x) || p(z)]
 *             = Inaccuracy + Complexity
 *
 * REPARAMETERIZATION TRICK:
 * -------------------------
 * To enable backpropagation through sampling:
 *
 *   z = mu + sigma * epsilon,  where epsilon ~ N(0, I)
 *
 * This allows gradients to flow through the stochastic layer.
 *
 * BETA-VAE (Higgins et al., 2017):
 * --------------------------------
 * Introduces beta weighting on KL term for disentanglement:
 *
 *   L = E_q[log p(x|z)] - beta * KL[q(z|x) || p(z)]
 *
 * beta > 1 encourages more disentangled representations.
 *
 * CONNECTION TO FREE ENERGY PRINCIPLE:
 * ------------------------------------
 *   VAE Component        | FEP Component
 *   ---------------------|--------------------
 *   q(z|x)               | Recognition density q(s|o)
 *   p(x|z)               | Generative model p(o|s)
 *   z ~ N(mu, sigma^2)   | Posterior belief about s
 *   Reconstruction error | Prediction error (inaccuracy)
 *   KL(q||p)             | Model complexity
 *   -ELBO                | Variational free energy
 *   1/sigma^2            | Precision weighting
 *
 * REFERENCES:
 * - Kingma & Welling (2014) "Auto-Encoding Variational Bayes"
 * - Higgins et al. (2017) "beta-VAE: Learning Basic Visual Concepts"
 * - Rezende & Mohamed (2015) "Variational Inference with Normalizing Flows"
 * - Friston (2010) "The free-energy principle: a unified brain theory?"
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |                      VARIATIONAL AUTOENCODER MODULE                        |
 * +===========================================================================+
 * |                                                                            |
 * |   +--------------------------------------------------------------------+  |
 * |   |                         ENCODER q(z|x)                             |  |
 * |   |                                                                    |  |
 * |   |   Input x -----> [Layer 1] --> [Layer 2] --> ... --> [Layer N]    |  |
 * |   |                                                           |        |  |
 * |   |                                              +------------+--------+  |
 * |   |                                              |            |        |  |
 * |   |                                           mu (mean)   log_var     |  |
 * |   +--------------------------------------------------------------------+  |
 * |                                                  |            |           |
 * |   +--------------------------------------------------------------------+  |
 * |   |                    LATENT SPACE z ~ N(mu, sigma^2)                 |  |
 * |   |                                                                    |  |
 * |   |   z = mu + sigma * epsilon,   epsilon ~ N(0, I)                   |  |
 * |   |                                                                    |  |
 * |   |   Precision: pi = 1/sigma^2  (mapped to FEP precision)            |  |
 * |   +--------------------------------------------------------------------+  |
 * |                                      |                                    |
 * |   +--------------------------------------------------------------------+  |
 * |   |                         DECODER p(x|z)                             |  |
 * |   |                                                                    |  |
 * |   |   Latent z --> [Layer 1] --> [Layer 2] --> ... --> Reconstruction |  |
 * |   +--------------------------------------------------------------------+  |
 * |                                                                            |
 * |   +--------------------------------------------------------------------+  |
 * |   |                         LOSS COMPUTATION                           |  |
 * |   |                                                                    |  |
 * |   |   ELBO = Reconstruction_Loss + beta * KL_Divergence               |  |
 * |   |   Free_Energy = -ELBO = Inaccuracy + Complexity                   |  |
 * |   +--------------------------------------------------------------------+  |
 * |                                                                            |
 * +===========================================================================+
 * ```
 *
 * BIO_MODULE: 0x1F00 (VAE Core)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_VAE_H
#define NIMCP_VAE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/tensor/nimcp_tensor.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum latent dimension */
#define VAE_MAX_LATENT_DIM              1024

/** Maximum input/output dimension */
#define VAE_MAX_IO_DIM                  65536

/** Maximum encoder/decoder layers */
#define VAE_MAX_LAYERS                  16

/** Default beta for beta-VAE (KL weight) */
#define VAE_DEFAULT_BETA                1.0f

/** Default learning rate */
#define VAE_DEFAULT_LEARNING_RATE       0.001f

/** Minimum variance to prevent posterior collapse */
#define VAE_MIN_VARIANCE                1e-6f

/** Maximum variance to prevent explosion */
#define VAE_MAX_VARIANCE                100.0f

/** Default dropout rate */
#define VAE_DEFAULT_DROPOUT             0.0f

/** Gradient clipping threshold */
#define VAE_DEFAULT_GRADIENT_CLIP       5.0f

/** Default batch size */
#define VAE_DEFAULT_BATCH_SIZE          32

/** Heartbeat interval (operations between heartbeats in loops) */
#define VAE_HEARTBEAT_INTERVAL          256

/** Bio-async module IDs */
#define BIO_MODULE_VAE_CORE             0x1F00
#define BIO_MODULE_VAE_ENCODER          0x1F01
#define BIO_MODULE_VAE_DECODER          0x1F02
#define BIO_MODULE_VAE_LATENT           0x1F03
#define BIO_MODULE_VAE_LOSS             0x1F04
#define BIO_MODULE_VAE_FEP_BRIDGE       0x1F10
#define BIO_MODULE_VAE_IMMUNE_BRIDGE    0x1F11
#define BIO_MODULE_VAE_BBB_BRIDGE       0x1F12

/* ============================================================================
 * Error Codes (32400-32499 range)
 * ============================================================================ */

#define NIMCP_ERROR_VAE_BASE            32400
#define NIMCP_ERROR_VAE_NULL_POINTER    32401
#define NIMCP_ERROR_VAE_INVALID_DIM     32402
#define NIMCP_ERROR_VAE_ENCODER_FAILED  32403
#define NIMCP_ERROR_VAE_DECODER_FAILED  32404
#define NIMCP_ERROR_VAE_LATENT_COLLAPSE 32405
#define NIMCP_ERROR_VAE_VARIANCE_EXPLODE 32406
#define NIMCP_ERROR_VAE_KL_DIVERGE      32407
#define NIMCP_ERROR_VAE_RECON_FAILED    32408
#define NIMCP_ERROR_VAE_GRADIENT_NAN    32409
#define NIMCP_ERROR_VAE_TRAINING_FAILED 32410
#define NIMCP_ERROR_VAE_NO_MEMORY       32411
#define NIMCP_ERROR_VAE_INVALID_CONFIG  32412
#define NIMCP_ERROR_VAE_NOT_INITIALIZED 32413
#define NIMCP_ERROR_VAE_BRIDGE_FAILED   32414
#define NIMCP_ERROR_VAE_BIO_ASYNC_FAILED 32415
#define NIMCP_ERROR_VAE_INVALID_STATE   32416
#define NIMCP_ERROR_VAE_SAMPLE_FAILED   32417
#define NIMCP_ERROR_VAE_LOSS_NAN        32418

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief VAE operating state
 */
typedef enum {
    VAE_STATE_UNINITIALIZED = 0,    /**< Not yet initialized */
    VAE_STATE_IDLE,                  /**< Initialized, not processing */
    VAE_STATE_ENCODING,              /**< Currently encoding */
    VAE_STATE_SAMPLING,              /**< Currently sampling latent */
    VAE_STATE_DECODING,              /**< Currently decoding */
    VAE_STATE_TRAINING,              /**< In training mode */
    VAE_STATE_GENERATING,            /**< Generating samples */
    VAE_STATE_ERROR,                 /**< Error state */
    VAE_STATE_RECOVERING             /**< Recovering from error */
} vae_state_t;

/**
 * @brief VAE activation functions
 */
typedef enum {
    VAE_ACTIVATION_RELU = 0,         /**< ReLU: max(0, x) */
    VAE_ACTIVATION_LEAKY_RELU,       /**< Leaky ReLU: max(0.01x, x) */
    VAE_ACTIVATION_ELU,              /**< ELU: x if x>0 else alpha*(exp(x)-1) */
    VAE_ACTIVATION_TANH,             /**< Tanh: (exp(x)-exp(-x))/(exp(x)+exp(-x)) */
    VAE_ACTIVATION_SIGMOID,          /**< Sigmoid: 1/(1+exp(-x)) */
    VAE_ACTIVATION_SOFTPLUS,         /**< Softplus: log(1+exp(x)) - good for variance */
    VAE_ACTIVATION_GELU,             /**< GELU: x*Phi(x) */
    VAE_ACTIVATION_SWISH,            /**< Swish: x*sigmoid(x) */
    VAE_ACTIVATION_LINEAR            /**< Linear (identity) */
} vae_activation_t;

/**
 * @brief VAE loss types for reconstruction
 */
typedef enum {
    VAE_LOSS_MSE = 0,                /**< Mean squared error reconstruction */
    VAE_LOSS_BCE,                    /**< Binary cross-entropy (for binary data) */
    VAE_LOSS_GAUSSIAN,               /**< Gaussian negative log-likelihood */
    VAE_LOSS_LAPLACE,                /**< Laplace negative log-likelihood */
    VAE_LOSS_BERNOULLI               /**< Bernoulli log-likelihood */
} vae_loss_type_t;

/**
 * @brief Latent prior distribution types
 */
typedef enum {
    VAE_PRIOR_STANDARD_NORMAL = 0,   /**< N(0, I) standard prior */
    VAE_PRIOR_LEARNED,               /**< Learned prior (VampPrior style) */
    VAE_PRIOR_MIXTURE,               /**< Mixture of Gaussians */
    VAE_PRIOR_FLOW                   /**< Normalizing flow prior */
} vae_prior_type_t;

/**
 * @brief VAE variant types
 */
typedef enum {
    VAE_VARIANT_STANDARD = 0,        /**< Standard VAE */
    VAE_VARIANT_BETA,                /**< beta-VAE (disentanglement) */
    VAE_VARIANT_CONDITIONAL,         /**< Conditional VAE */
    VAE_VARIANT_HIERARCHICAL,        /**< Hierarchical VAE */
    VAE_VARIANT_VQ                   /**< Vector-quantized VAE */
} vae_variant_t;

/**
 * @brief Anomaly types for immune system reporting
 */
typedef enum {
    VAE_ANOMALY_RECONSTRUCTION = 0,   /**< High reconstruction error */
    VAE_ANOMALY_HIGH_RECON_ERROR = 0, /**< Alias for RECONSTRUCTION (legacy) */
    VAE_ANOMALY_KL_DIVERGENCE,        /**< Abnormal KL divergence */
    VAE_ANOMALY_VARIANCE_EXPLOSION,   /**< Latent variance explosion */
    VAE_ANOMALY_POSTERIOR_COLLAPSE,   /**< Posterior collapse detected */
    VAE_ANOMALY_OOD_SAMPLE,           /**< Out-of-distribution sample */
    VAE_ANOMALY_GRADIENT_NAN,         /**< NaN in gradients */
    VAE_ANOMALY_FREE_ENERGY_SPIKE,    /**< Free energy spike */
    VAE_ANOMALY_LATENT_DRIFT,         /**< Latent space drift */
    VAE_ANOMALY_TRAINING_UNSTABLE,    /**< Training loss oscillating */
    VAE_ANOMALY_LOSS_NAN,             /**< NaN in loss computation */
    VAE_ANOMALY_COUNT                 /**< Number of anomaly types */
} vae_anomaly_type_t;

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

/**
 * @brief Single layer configuration
 */
typedef struct {
    uint32_t units;                  /**< Number of units in this layer */
    vae_activation_t activation;     /**< Activation function */
    float dropout_rate;              /**< Dropout rate (0 = disabled) */
    bool batch_norm;                 /**< Enable batch normalization */
    bool use_bias;                   /**< Use bias terms */
} vae_layer_config_t;

/**
 * @brief Encoder network configuration
 */
typedef struct {
    uint32_t input_dim;              /**< Input dimension */
    uint32_t latent_dim;             /**< Latent space dimension */
    uint32_t num_layers;             /**< Number of hidden layers */
    vae_layer_config_t layers[VAE_MAX_LAYERS]; /**< Layer configurations */
    vae_activation_t mu_activation;  /**< Activation for mean output (usually LINEAR) */
    vae_activation_t var_activation; /**< Activation for variance (usually SOFTPLUS) */
} vae_encoder_config_t;

/**
 * @brief Decoder network configuration
 */
typedef struct {
    uint32_t latent_dim;             /**< Latent space dimension */
    uint32_t output_dim;             /**< Output dimension */
    uint32_t num_layers;             /**< Number of hidden layers */
    vae_layer_config_t layers[VAE_MAX_LAYERS]; /**< Layer configurations */
    vae_activation_t final_activation; /**< Final layer activation */
    bool output_variance;            /**< Output variance (heteroscedastic) */
} vae_decoder_config_t;

/**
 * @brief Training configuration
 */
typedef struct {
    float learning_rate;             /**< Learning rate */
    float beta;                      /**< KL divergence weight (beta-VAE) */
    float beta_min;                  /**< Minimum beta during warmup */
    float beta_max;                  /**< Maximum beta value */
    uint32_t beta_warmup_steps;      /**< Steps to warm up beta from min to max */
    vae_loss_type_t loss_type;       /**< Reconstruction loss type */
    float gradient_clip;             /**< Gradient clipping threshold */
    bool free_bits;                  /**< Enable free bits for KL */
    float free_bits_lambda;          /**< Free bits threshold per dimension */
    uint32_t batch_size;             /**< Training batch size */
    float weight_decay;              /**< L2 regularization */
    float momentum;                  /**< Optimizer momentum */
} vae_training_config_t;

/**
 * @brief Main VAE configuration
 */
typedef struct {
    vae_encoder_config_t encoder;    /**< Encoder configuration */
    vae_decoder_config_t decoder;    /**< Decoder configuration */
    vae_training_config_t training;  /**< Training configuration */
    vae_variant_t variant;           /**< VAE variant type */
    vae_prior_type_t prior_type;     /**< Latent prior type */

    /* Integration flags */
    bool enable_immune_integration;  /**< Enable immune system integration */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    bool enable_kg_wiring;           /**< Enable KG self-awareness */
    bool enable_logging;             /**< Enable detailed logging */

    /* Thresholds */
    float anomaly_threshold;         /**< Reconstruction error anomaly threshold */
    float collapse_threshold;        /**< Posterior collapse detection threshold */

    /* Health monitoring */
    uint32_t heartbeat_interval_ms;  /**< Heartbeat interval in milliseconds */
    uint32_t max_consecutive_errors; /**< Max errors before entering error state */
} vae_config_t;

/* ============================================================================
 * State and Statistics Structures
 * ============================================================================ */

/**
 * @brief Latent space state (result of encoding)
 */
typedef struct {
    float* mu;                       /**< Mean vector [latent_dim] */
    float* log_var;                  /**< Log variance vector [latent_dim] */
    float* z;                        /**< Sampled latent vector [latent_dim] */
    float* precision;                /**< Precision = 1/var [latent_dim] */
    uint32_t latent_dim;             /**< Latent dimension */
    bool is_valid;                   /**< Whether state is valid */
} vae_latent_state_t;

/**
 * @brief Loss components from ELBO computation
 */
typedef struct {
    float total_loss;                /**< Total ELBO loss (to minimize) */
    float reconstruction_loss;       /**< Reconstruction term E[-log p(x|z)] */
    float kl_divergence;             /**< Raw KL divergence KL[q(z|x)||p(z)] */
    float weighted_kl;               /**< beta * KL divergence */
    float free_energy;               /**< Variational free energy = -ELBO */
    float inaccuracy;                /**< FEP inaccuracy term (= recon loss) */
    float complexity;                /**< FEP complexity term (= KL) */
    float elbo;                      /**< Evidence lower bound = -free_energy */
    uint32_t active_units;           /**< Number of active latent units (KL > threshold) */
} vae_loss_t;

/**
 * @brief VAE operational statistics
 */
typedef struct {
    /* Operation counts */
    uint64_t total_encode_calls;     /**< Total encoding operations */
    uint64_t total_decode_calls;     /**< Total decoding operations */
    uint64_t total_forward_calls;    /**< Total forward passes */
    uint64_t total_train_steps;      /**< Total training steps */
    uint64_t total_generate_calls;   /**< Total generation operations */

    /* Loss statistics (exponential moving average) */
    float ema_total_loss;            /**< EMA of total loss */
    float ema_reconstruction_loss;   /**< EMA of reconstruction loss */
    float ema_kl_divergence;         /**< EMA of KL divergence */
    float ema_free_energy;           /**< EMA of free energy */

    /* Latent space statistics */
    float avg_latent_variance;       /**< Average latent variance */
    float min_latent_variance;       /**< Minimum latent variance (collapse check) */
    float max_latent_variance;       /**< Maximum latent variance (explosion check) */
    uint32_t collapsed_dimensions;   /**< Number of collapsed dimensions */
    uint32_t active_dimensions;      /**< Number of active latent dimensions */

    /* Anomaly statistics */
    uint64_t anomalies_detected;     /**< Total anomalies detected */
    uint64_t immune_reports;         /**< Reports sent to immune system */
    uint64_t posterior_collapses;    /**< Posterior collapse events */
    uint64_t variance_explosions;    /**< Variance explosion events */
    uint64_t gradient_nans;          /**< NaN gradient events */

    /* Bio-async statistics */
    uint64_t bio_async_sent;         /**< Bio-async messages sent */
    uint64_t bio_async_received;     /**< Bio-async messages received */

    /* Training state */
    float current_beta;              /**< Current beta value */
    float current_lr;                /**< Current learning rate */
    uint64_t global_step;            /**< Global training step */

    /* Timing */
    uint64_t last_update_us;         /**< Last update timestamp (microseconds) */
    float avg_encode_latency_us;     /**< Average encode latency */
    float avg_decode_latency_us;     /**< Average decode latency */
} vae_stats_t;

/**
 * @brief Health metrics for resilience monitoring
 */
typedef struct {
    float encoder_health;            /**< Encoder health score (0-1) */
    float decoder_health;            /**< Decoder health score (0-1) */
    float latent_health;             /**< Latent space health (0-1) */
    float training_health;           /**< Training stability (0-1) */
    float overall_health;            /**< Overall system health (0-1) */
    bool is_healthy;                 /**< Quick health check flag */
    uint32_t consecutive_errors;     /**< Consecutive error count */
    uint64_t last_healthy_time_us;   /**< Last healthy timestamp */
    uint64_t last_error_time_us;     /**< Last error timestamp */
    uint32_t error_code;             /**< Last error code */
} vae_health_t;

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

/** Opaque VAE system structure */
typedef struct vae_system vae_system_t;

/** Opaque encoder structure */
typedef struct vae_encoder vae_encoder_t;

/** Opaque decoder structure */
typedef struct vae_decoder vae_decoder_t;

/** Health agent forward declaration */
struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Initialize default configuration
 *
 * Sets all configuration fields to sensible defaults.
 *
 * @param config Configuration structure to initialize
 * @return 0 on success, -1 on error (throws to immune system)
 */
int vae_default_config(vae_config_t* config);

/**
 * @brief Create VAE system
 *
 * Allocates and initializes a new VAE system with the given configuration.
 *
 * @param config Configuration (NULL for defaults)
 * @return VAE system pointer or NULL on error (throws to immune system)
 */
vae_system_t* vae_create(const vae_config_t* config);

/**
 * @brief Destroy VAE system
 *
 * Frees all resources associated with the VAE system.
 *
 * @param vae VAE system to destroy (NULL safe)
 */
void vae_destroy(vae_system_t* vae);

/**
 * @brief Reset VAE to initial state
 *
 * Clears statistics and resets internal state without reallocating.
 *
 * @param vae VAE system
 * @return 0 on success, -1 on error
 */
int vae_reset(vae_system_t* vae);

/* ============================================================================
 * Core Processing API
 * ============================================================================ */

/**
 * @brief Encode input to latent distribution parameters
 *
 * Maps input through encoder network to produce mean and log-variance
 * of the approximate posterior q(z|x).
 *
 * @param vae VAE system
 * @param input Input tensor [batch_size, input_dim]
 * @param mu Output mean tensor [batch_size, latent_dim]
 * @param log_var Output log variance tensor [batch_size, latent_dim]
 * @return 0 on success, -1 on error (throws to immune system)
 */
int vae_encode(vae_system_t* vae,
               const nimcp_tensor_t* input,
               nimcp_tensor_t* mu,
               nimcp_tensor_t* log_var);

/**
 * @brief Sample from latent distribution using reparameterization trick
 *
 * Samples z = mu + sigma * epsilon, where epsilon ~ N(0, I).
 * This allows gradients to flow through the sampling operation.
 *
 * @param vae VAE system
 * @param mu Mean tensor [batch_size, latent_dim]
 * @param log_var Log variance tensor [batch_size, latent_dim]
 * @param z Output samples [batch_size, latent_dim]
 * @return 0 on success, -1 on error
 */
int vae_sample(vae_system_t* vae,
               const nimcp_tensor_t* mu,
               const nimcp_tensor_t* log_var,
               nimcp_tensor_t* z);

/**
 * @brief Decode latent samples to reconstruction
 *
 * Maps latent samples through decoder network to produce reconstructions.
 *
 * @param vae VAE system
 * @param z Latent tensor [batch_size, latent_dim]
 * @param reconstruction Output reconstruction [batch_size, output_dim]
 * @return 0 on success, -1 on error (throws to immune system)
 */
int vae_decode(vae_system_t* vae,
               const nimcp_tensor_t* z,
               nimcp_tensor_t* reconstruction);

/**
 * @brief Full forward pass (encode + sample + decode)
 *
 * Performs complete VAE forward pass from input to reconstruction.
 * Optionally returns the latent state for inspection.
 *
 * @param vae VAE system
 * @param input Input tensor
 * @param reconstruction Output reconstruction (can be NULL if only latent needed)
 * @param latent Optional output latent state (can be NULL)
 * @return 0 on success, -1 on error
 */
int vae_forward(vae_system_t* vae,
                const nimcp_tensor_t* input,
                nimcp_tensor_t* reconstruction,
                vae_latent_state_t* latent);

/**
 * @brief Compute ELBO loss components
 *
 * Computes reconstruction loss, KL divergence, and total ELBO.
 * Also computes FEP-compatible free energy decomposition.
 *
 * @param vae VAE system
 * @param input Original input
 * @param reconstruction Reconstructed output
 * @param mu Latent mean
 * @param log_var Latent log variance
 * @param loss Output loss components
 * @return 0 on success, -1 on error
 */
int vae_compute_loss(vae_system_t* vae,
                     const nimcp_tensor_t* input,
                     const nimcp_tensor_t* reconstruction,
                     const nimcp_tensor_t* mu,
                     const nimcp_tensor_t* log_var,
                     vae_loss_t* loss);

/* ============================================================================
 * Training API
 * ============================================================================ */

/**
 * @brief Single training step
 *
 * Performs one complete training iteration:
 * 1. Forward pass
 * 2. Loss computation
 * 3. Backward pass (gradient computation)
 * 4. Parameter update
 *
 * @param vae VAE system
 * @param input Training input batch
 * @param loss Output loss values
 * @return 0 on success, -1 on error
 */
int vae_train_step(vae_system_t* vae,
                   const nimcp_tensor_t* input,
                   vae_loss_t* loss);

/**
 * @brief Set training mode
 *
 * Enables or disables training mode. In training mode, dropout is active
 * and batch normalization uses batch statistics.
 *
 * @param vae VAE system
 * @param training True for training, false for inference
 * @return 0 on success, -1 on error
 */
int vae_set_training(vae_system_t* vae, bool training);

/**
 * @brief Check if in training mode
 *
 * @param vae VAE system
 * @return true if in training mode
 */
bool vae_is_training(const vae_system_t* vae);

/**
 * @brief Update beta value (for beta-VAE annealing)
 *
 * @param vae VAE system
 * @param beta New beta value
 * @return 0 on success, -1 on error
 */
int vae_set_beta(vae_system_t* vae, float beta);

/**
 * @brief Get current beta value
 *
 * @param vae VAE system
 * @return Current beta value
 */
float vae_get_beta(const vae_system_t* vae);

/**
 * @brief Update learning rate
 *
 * @param vae VAE system
 * @param lr New learning rate
 * @return 0 on success, -1 on error
 */
int vae_set_learning_rate(vae_system_t* vae, float lr);

/* ============================================================================
 * Generation API
 * ============================================================================ */

/**
 * @brief Generate samples from prior
 *
 * Samples from the prior p(z) and decodes to generate new samples.
 *
 * @param vae VAE system
 * @param num_samples Number of samples to generate
 * @param samples Output samples [num_samples, output_dim]
 * @return 0 on success, -1 on error
 */
int vae_generate(vae_system_t* vae,
                 uint32_t num_samples,
                 nimcp_tensor_t* samples);

/**
 * @brief Sample from prior distribution
 *
 * Generates latent samples from prior without decoding.
 *
 * @param vae VAE system
 * @param num_samples Number of samples
 * @param latent_samples Output latent samples [num_samples, latent_dim]
 * @return 0 on success, -1 on error
 */
int vae_sample_prior(vae_system_t* vae,
                     uint32_t num_samples,
                     nimcp_tensor_t* latent_samples);

/**
 * @brief Interpolate between two latent points
 *
 * Generates interpolated samples along a path in latent space.
 *
 * @param vae VAE system
 * @param z1 First latent point [latent_dim]
 * @param z2 Second latent point [latent_dim]
 * @param num_steps Number of interpolation steps
 * @param interpolations Output [num_steps, output_dim]
 * @return 0 on success, -1 on error
 */
int vae_interpolate(vae_system_t* vae,
                    const nimcp_tensor_t* z1,
                    const nimcp_tensor_t* z2,
                    uint32_t num_steps,
                    nimcp_tensor_t* interpolations);

/**
 * @brief Reconstruct input (encode then decode)
 *
 * Convenience function that encodes input and immediately decodes.
 *
 * @param vae VAE system
 * @param input Input tensor
 * @param reconstruction Output reconstruction
 * @return 0 on success, -1 on error
 */
int vae_reconstruct(vae_system_t* vae,
                    const nimcp_tensor_t* input,
                    nimcp_tensor_t* reconstruction);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current operating state
 *
 * @param vae VAE system
 * @return Current state
 */
vae_state_t vae_get_state(const vae_system_t* vae);

/**
 * @brief Get state name as string
 *
 * @param state State value
 * @return State name string
 */
const char* vae_state_to_string(vae_state_t state);

/**
 * @brief Get operational statistics
 *
 * @param vae VAE system
 * @param stats Output statistics structure
 * @return 0 on success, -1 on error
 */
int vae_get_stats(const vae_system_t* vae, vae_stats_t* stats);

/**
 * @brief Get health metrics
 *
 * @param vae VAE system
 * @param health Output health metrics
 * @return 0 on success, -1 on error
 */
int vae_get_health(const vae_system_t* vae, vae_health_t* health);

/**
 * @brief Get current latent state
 *
 * Returns the most recent latent state from encoding.
 *
 * @param vae VAE system
 * @param latent Output latent state
 * @return 0 on success, -1 on error
 */
int vae_get_latent_state(const vae_system_t* vae, vae_latent_state_t* latent);

/**
 * @brief Get configuration
 *
 * @param vae VAE system
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int vae_get_config(const vae_system_t* vae, vae_config_t* config);

/**
 * @brief Get input dimension
 *
 * @param vae VAE system
 * @return Input dimension
 */
uint32_t vae_get_input_dim(const vae_system_t* vae);

/**
 * @brief Get latent dimension
 *
 * @param vae VAE system
 * @return Latent dimension
 */
uint32_t vae_get_latent_dim(const vae_system_t* vae);

/**
 * @brief Get output dimension
 *
 * @param vae VAE system
 * @return Output dimension
 */
uint32_t vae_get_output_dim(const vae_system_t* vae);

/* ============================================================================
 * Anomaly Detection API
 * ============================================================================ */

/**
 * @brief Compute anomaly score for input
 *
 * Uses reconstruction error as anomaly score. High reconstruction error
 * indicates the input is unlike training data.
 *
 * @param vae VAE system
 * @param input Input to check
 * @param anomaly_score Output anomaly score
 * @return 0 on success, -1 on error
 */
int vae_compute_anomaly_score(vae_system_t* vae,
                              const nimcp_tensor_t* input,
                              float* anomaly_score);

/**
 * @brief Check if input is anomalous
 *
 * @param vae VAE system
 * @param input Input to check
 * @param is_anomaly Output flag
 * @return 0 on success, -1 on error
 */
int vae_is_anomaly(vae_system_t* vae,
                   const nimcp_tensor_t* input,
                   bool* is_anomaly);

/* ============================================================================
 * FEP Integration API
 * ============================================================================ */

/**
 * @brief Get variational free energy (negative ELBO)
 *
 * Returns the free energy from the most recent forward pass.
 *
 * @param vae VAE system
 * @return Free energy value, or NAN on error
 */
float vae_get_free_energy(const vae_system_t* vae);

/**
 * @brief Get precision vector from latent variance
 *
 * Precision = 1 / variance, used for FEP precision weighting.
 *
 * @param vae VAE system
 * @param precision Output precision array [latent_dim]
 * @param dim Expected dimension (for validation)
 * @return 0 on success, -1 on error
 */
int vae_get_precision(const vae_system_t* vae, float* precision, uint32_t dim);

/**
 * @brief Get average precision across latent dimensions
 *
 * @param vae VAE system
 * @return Average precision, or NAN on error
 */
float vae_get_avg_precision(const vae_system_t* vae);

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

/**
 * @brief Set health agent for heartbeat reporting
 *
 * @param agent Health agent instance
 */
void vae_set_health_agent(nimcp_health_agent_t* agent);

/**
 * @brief Get current health agent
 *
 * @return Health agent or NULL
 */
nimcp_health_agent_t* vae_get_health_agent(void);

/* ============================================================================
 * Latent State Management
 * ============================================================================ */

/**
 * @brief Allocate latent state structure
 *
 * @param latent_dim Latent dimension
 * @return Allocated state or NULL on error
 */
vae_latent_state_t* vae_latent_state_create(uint32_t latent_dim);

/**
 * @brief Free latent state structure
 *
 * @param state State to free (NULL safe)
 */
void vae_latent_state_destroy(vae_latent_state_t* state);

/**
 * @brief Copy latent state
 *
 * @param dst Destination state
 * @param src Source state
 * @return 0 on success, -1 on error
 */
int vae_latent_state_copy(vae_latent_state_t* dst, const vae_latent_state_t* src);

/**
 * @brief Reset latent state to zero
 *
 * @param state State to reset
 * @return 0 on success, -1 on error
 */
int vae_latent_state_reset(vae_latent_state_t* state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VAE_H */
