/**
 * @file nimcp_cnn_training.h
 * @brief CNN Training Integration for NIMCP
 *
 * WHAT: Convolutional Neural Network training with SNN conversion pathway
 * WHY:  Enable hybrid CNN+SNN architectures and pretrain-then-convert workflow
 * HOW:  Classical CNN training (backprop) + rate/spike encoding for SNN transfer
 *
 * BIOLOGICAL INSPIRATION:
 * - Convolutional layers model V1 simple/complex cells:
 *   * Gabor filters ≈ orientation-selective simple cells (Hubel & Wiesel 1962)
 *   * Pooling ≈ complex cell position invariance
 *   * Hierarchical features ≈ V1 → V2 → V4 → IT pathway
 *
 * - CNN-to-SNN conversion models developmental learning:
 *   * Prenatal: Genetically-wired CNN structure (innate feature detectors)
 *   * Postnatal: STDP refinement via spiking activity
 *   * Hybrid training: CNN pretraining + SNN fine-tuning with plasticity
 *
 * INTEGRATION WITH NIMCP:
 * - Visual Cortex: CNN layers for hierarchical visual features
 * - Audio Cortex: CNN for spectrotemporal feature extraction
 * - SNN Conversion: Rate/population coding bridges to spiking networks
 * - Training Pipeline: Optimizers, gradient manager, loss functions
 * - Bio-async: Inter-module messaging for distributed training
 * - Immune System: Instability detection and learning rate modulation
 *
 * CONVERSION PATHWAY (CNN → SNN):
 * 1. Train CNN to convergence with backpropagation
 * 2. Normalize activations to target firing rate range
 * 3. Convert weights to SNN synapses with rate coding
 * 4. Fine-tune with STDP/R-STDP for spike-based plasticity
 * 5. Deploy as spiking network with energy efficiency
 *
 * USE CASES:
 * - Visual perception: Pretrain CNN on images → Convert to SNN for robot vision
 * - Audio processing: CNN spectrograms → SNN cochlear model
 * - Transfer learning: ImageNet CNN → SNN visual cortex initialization
 * - Hybrid networks: CNN feature extraction + SNN decision making
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines with guard clauses
 * - WHAT-WHY-HOW documentation on every function
 * - Single Responsibility Principle
 * - Biological grounding in comments
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 * @version 1.0.0
 */

#ifndef NIMCP_CNN_TRAINING_H
#define NIMCP_CNN_TRAINING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* NIMCP core dependencies */
#include "utils/error/nimcp_error_codes.h"
#include "utils/tensor/nimcp_tensor.h"
#include "middleware/training/nimcp_optimizers.h"
#include "middleware/training/nimcp_gradient_manager.h"
#include "middleware/training/nimcp_loss_functions.h"

/* SNN integration */
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_encoding.h"

/* Bio-async communication */
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

//=============================================================================
// Constants
//=============================================================================

#define CNN_MAX_LAYERS 32                 /**< Maximum CNN layers */
#define CNN_MAX_KERNEL_SIZE 11            /**< Maximum kernel dimension */
#define CNN_MAX_CHANNELS 2048             /**< Maximum channels per layer */
#define CNN_MAX_BATCH_SIZE 512            /**< Maximum batch size */
#define CNN_DEFAULT_PADDING_VALUE 0.0f    /**< Default padding value */

/* Bio-async module ID */
#define BIO_MODULE_CNN_TRAINING 0x0700    /**< CNN training module ID */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief CNN layer types
 *
 * BIOLOGICAL ANALOGY:
 * - CONV2D: V1 simple cells (orientation selectivity)
 * - POOLING: V1 complex cells (position invariance)
 * - BATCH_NORM: Lateral inhibition, divisive normalization
 * - DROPOUT: Synaptic pruning during development
 */
typedef enum {
    CNN_LAYER_CONV2D = 0,           /**< 2D convolution */
    CNN_LAYER_CONV1D,               /**< 1D convolution (temporal) */
    CNN_LAYER_DEPTHWISE_CONV2D,     /**< Depthwise separable conv */
    CNN_LAYER_POOLING,              /**< Max/average/global pooling */
    CNN_LAYER_BATCH_NORM,           /**< Batch normalization */
    CNN_LAYER_DROPOUT,              /**< Dropout regularization */
    CNN_LAYER_DENSE,                /**< Fully connected */
    CNN_LAYER_FLATTEN,              /**< Reshape to 1D */
    CNN_LAYER_ACTIVATION,           /**< Standalone activation */
    CNN_LAYER_RESIDUAL,             /**< Residual skip connection */
    CNN_LAYER_TYPE_COUNT
} cnn_layer_type_t;

/**
 * @brief CNN activation functions
 */
typedef enum {
    CNN_ACTIVATION_NONE = 0,        /**< Linear (no activation) */
    CNN_ACTIVATION_RELU,            /**< ReLU (biological: depolarization threshold) */
    CNN_ACTIVATION_LEAKY_RELU,      /**< Leaky ReLU (leak channels) */
    CNN_ACTIVATION_SILU,            /**< SiLU/Swish (smooth sigmoid-like) */
    CNN_ACTIVATION_GELU,            /**< GELU (Gaussian-weighted) */
    CNN_ACTIVATION_SIGMOID,         /**< Sigmoid (firing rate saturation) */
    CNN_ACTIVATION_TANH,            /**< Tanh (balanced activation) */
    CNN_ACTIVATION_SOFTMAX,         /**< Softmax (winner-take-all normalization) */
    CNN_ACTIVATION_COUNT
} cnn_activation_t;

/**
 * @brief Pooling types
 *
 * BIOLOGICAL ANALOGY:
 * - MAX: Complex cells respond to maximum response across position
 * - AVERAGE: Population averaging across receptive field
 * - GLOBAL: Whole-field integration (IT invariant representations)
 */
typedef enum {
    CNN_POOL_MAX = 0,               /**< Max pooling */
    CNN_POOL_AVERAGE,               /**< Average pooling */
    CNN_POOL_GLOBAL_MAX,            /**< Global max pooling */
    CNN_POOL_GLOBAL_AVERAGE,        /**< Global average pooling */
    CNN_POOL_STOCHASTIC,            /**< Stochastic pooling (sampling) */
    CNN_POOL_LP,                    /**< Lp norm pooling */
    CNN_POOL_TYPE_COUNT
} cnn_pooling_type_t;

/**
 * @brief Padding modes
 */
typedef enum {
    CNN_PADDING_VALID = 0,          /**< No padding (shrink output) */
    CNN_PADDING_SAME,               /**< Pad to maintain dimensions */
    CNN_PADDING_FULL,               /**< Full convolution (expand output) */
    CNN_PADDING_REFLECT,            /**< Reflect edge values */
    CNN_PADDING_REPLICATE,          /**< Replicate edge values */
    CNN_PADDING_CIRCULAR,           /**< Circular/wrap padding */
    CNN_PADDING_MODE_COUNT
} cnn_padding_mode_t;

/**
 * @brief CNN-to-SNN conversion methods
 *
 * BIOLOGICAL GROUNDING:
 * - RATE_CODING: Firing rate encodes activation magnitude
 * - POPULATION_CODING: Distributed representation across neurons
 * - TEMPORAL_CODING: Spike timing encodes information
 * - HYBRID: Mixed coding for different layer types
 */
typedef enum {
    CNN_TO_SNN_RATE_CODING = 0,     /**< Normalize to firing rates */
    CNN_TO_SNN_POPULATION_CODING,   /**< Population code with Gaussian RFs */
    CNN_TO_SNN_TEMPORAL_CODING,     /**< Time-to-first-spike coding */
    CNN_TO_SNN_BURST_CODING,        /**< Burst count encoding */
    CNN_TO_SNN_HYBRID,              /**< Layer-specific coding schemes */
    CNN_TO_SNN_METHOD_COUNT
} cnn_to_snn_method_t;

/**
 * @brief Data augmentation types
 */
typedef enum {
    CNN_AUGMENT_FLIP_HORIZONTAL = (1 << 0),  /**< Horizontal flip */
    CNN_AUGMENT_FLIP_VERTICAL = (1 << 1),    /**< Vertical flip */
    CNN_AUGMENT_ROTATE = (1 << 2),           /**< Random rotation */
    CNN_AUGMENT_SCALE = (1 << 3),            /**< Random scaling */
    CNN_AUGMENT_TRANSLATE = (1 << 4),        /**< Random translation */
    CNN_AUGMENT_BRIGHTNESS = (1 << 5),       /**< Brightness jitter */
    CNN_AUGMENT_CONTRAST = (1 << 6),         /**< Contrast jitter */
    CNN_AUGMENT_NOISE = (1 << 7),            /**< Gaussian noise */
    CNN_AUGMENT_CUTOUT = (1 << 8),           /**< Random cutout/dropout */
    CNN_AUGMENT_MIXUP = (1 << 9)             /**< MixUp blending */
} cnn_augmentation_flags_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief Convolution layer configuration
 *
 * BIOLOGICAL BASIS:
 * Receptive fields in V1 are oriented Gabor-like filters:
 * - kernel_size: Receptive field dimensions (typical: 3x3 to 11x11)
 * - stride: Overlap between adjacent receptive fields
 * - in_channels: Input feature maps (e.g., RGB = 3)
 * - out_channels: Number of feature detectors (V1 ≈ 10^8 neurons)
 */
typedef struct {
    uint32_t kernel_h;              /**< Kernel height */
    uint32_t kernel_w;              /**< Kernel width */
    uint32_t stride_h;              /**< Vertical stride */
    uint32_t stride_w;              /**< Horizontal stride */
    uint32_t padding_h;             /**< Vertical padding */
    uint32_t padding_w;             /**< Horizontal padding */
    uint32_t dilation_h;            /**< Vertical dilation (atrous) */
    uint32_t dilation_w;            /**< Horizontal dilation */
    uint32_t in_channels;           /**< Input channels */
    uint32_t out_channels;          /**< Output channels (filters) */
    uint32_t groups;                /**< Grouped convolution */
    cnn_padding_mode_t padding_mode; /**< Padding strategy */
    cnn_activation_t activation;    /**< Post-conv activation */
    bool use_bias;                  /**< Include bias terms */
    float weight_init_std;          /**< Weight initialization stddev */
} cnn_conv_config_t;

/**
 * @brief Pooling layer configuration
 *
 * BIOLOGICAL BASIS:
 * Complex cells pool over spatial position for invariance:
 * - pool_size: Pooling receptive field (typical: 2x2 or 3x3)
 * - stride: Subsampling factor
 * - type: Max pooling ≈ winner-take-all competition
 */
typedef struct {
    cnn_pooling_type_t type;        /**< Pooling type */
    uint32_t pool_h;                /**< Pool height */
    uint32_t pool_w;                /**< Pool width */
    uint32_t stride_h;              /**< Vertical stride */
    uint32_t stride_w;              /**< Horizontal stride */
    uint32_t padding_h;             /**< Vertical padding */
    uint32_t padding_w;             /**< Horizontal padding */
    float p_norm;                   /**< Lp norm order (for LP pooling) */
} cnn_pool_config_t;

/**
 * @brief Batch normalization configuration
 *
 * BIOLOGICAL BASIS:
 * Models divisive normalization in visual cortex:
 * - Lateral inhibition normalizes responses
 * - Maintains stable activity distributions
 * - Prevents saturation (homeostatic plasticity analog)
 */
typedef struct {
    uint32_t num_features;          /**< Number of channels to normalize */
    float epsilon;                  /**< Numerical stability constant */
    float momentum;                 /**< Running stats momentum */
    bool affine;                    /**< Learnable scale/shift */
    bool track_running_stats;       /**< Track running mean/var */
} cnn_batch_norm_config_t;

/**
 * @brief Dropout configuration
 *
 * BIOLOGICAL BASIS:
 * Models synaptic pruning and stochastic neuron dropout:
 * - Developmental synaptic elimination
 * - Stochastic synaptic transmission failures
 * - Prevents over-reliance on specific pathways
 */
typedef struct {
    float dropout_rate;             /**< Dropout probability [0, 1) */
    bool spatial_dropout;           /**< Drop entire feature maps */
    bool variational_dropout;       /**< Same mask across time */
} cnn_dropout_config_t;

/**
 * @brief Dense (fully connected) layer configuration
 */
typedef struct {
    uint32_t in_features;           /**< Input dimension */
    uint32_t out_features;          /**< Output dimension */
    cnn_activation_t activation;    /**< Post-linear activation */
    bool use_bias;                  /**< Include bias terms */
    float weight_init_std;          /**< Weight initialization stddev */
} cnn_dense_config_t;

/**
 * @brief Data augmentation configuration
 */
typedef struct {
    uint32_t flags;                 /**< Bitmask of augmentation_flags_t */
    float rotation_range;           /**< Max rotation angle (degrees) */
    float scale_range_min;          /**< Min scale factor */
    float scale_range_max;          /**< Max scale factor */
    float translation_range;        /**< Max translation (fraction) */
    float brightness_range;         /**< Brightness jitter range */
    float contrast_range;           /**< Contrast jitter range */
    float noise_stddev;             /**< Gaussian noise stddev */
    float cutout_size;              /**< Cutout region size (fraction) */
    float mixup_alpha;              /**< MixUp beta distribution alpha */
    float probability;              /**< Probability of applying augmentation */
} cnn_augmentation_config_t;

/**
 * @brief CNN-to-SNN conversion configuration
 *
 * BIOLOGICAL GROUNDING:
 * Rate coding conversion normalizes CNN activations to biological firing rates:
 * - max_rate: Typical cortical neurons fire 0-200 Hz
 * - time_steps: Temporal window for rate averaging (e.g., 50ms → 10 steps @ 5ms)
 * - threshold_normalization: Adjust SNN thresholds to match CNN decision boundaries
 */
typedef struct {
    cnn_to_snn_method_t method;     /**< Conversion method */
    float max_firing_rate;          /**< Maximum SNN firing rate (Hz) */
    uint32_t time_steps;            /**< Simulation time steps */
    float timestep_ms;              /**< Time step duration (ms) */
    bool normalize_weights;         /**< Normalize weight scales */
    bool normalize_thresholds;      /**< Normalize neuron thresholds */
    float threshold_quantile;       /**< Threshold calibration quantile */

    /* Rate coding parameters */
    snn_rate_encoder_config_t rate_config;

    /* Population coding parameters */
    snn_population_encoder_config_t pop_config;

    /* Temporal coding parameters */
    snn_temporal_encoder_config_t temporal_config;

    /* Post-conversion fine-tuning */
    bool enable_stdp_finetuning;    /**< STDP refinement after conversion */
    uint32_t finetune_epochs;       /**< STDP fine-tuning epochs */
    float stdp_learning_rate;       /**< STDP learning rate */
} cnn_to_snn_config_t;

/**
 * @brief Data loader configuration
 */
typedef struct {
    uint32_t batch_size;            /**< Batch size */
    bool shuffle;                   /**< Shuffle data */
    uint32_t num_workers;           /**< Data loading threads */
    bool pin_memory;                /**< Pin memory for GPU transfer */
    bool drop_last;                 /**< Drop incomplete batches */
    cnn_augmentation_config_t augmentation; /**< Data augmentation */
} cnn_dataloader_config_t;

/**
 * @brief CNN trainer configuration
 */
typedef struct {
    /* Optimizer settings */
    nimcp_optimizer_config_t optimizer_config;

    /* Gradient management */
    nimcp_gradient_manager_config_t gradient_config;

    /* Loss function */
    nimcp_loss_type_t loss_type;
    void* loss_config;              /**< Loss-specific config (e.g., nimcp_cross_entropy_config_t*) */

    /* Training hyperparameters */
    uint32_t max_epochs;            /**< Maximum training epochs */
    float learning_rate;            /**< Initial learning rate */
    float weight_decay;             /**< L2 regularization */
    float gradient_clip_value;      /**< Gradient clipping threshold */

    /* Data pipeline */
    cnn_dataloader_config_t dataloader;

    /* Validation */
    bool use_validation;            /**< Enable validation split */
    float validation_split;         /**< Validation data fraction */
    uint32_t validation_frequency;  /**< Validate every N batches */

    /* Checkpointing */
    bool save_checkpoints;          /**< Save model checkpoints */
    uint32_t checkpoint_frequency;  /**< Save every N epochs */
    const char* checkpoint_dir;     /**< Checkpoint directory path */

    /* Early stopping */
    bool use_early_stopping;        /**< Enable early stopping */
    uint32_t patience;              /**< Early stopping patience */
    float min_delta;                /**< Minimum improvement threshold */

    /* Bio-async integration */
    bool enable_bio_async;          /**< Enable inter-module messaging */

    /* Anti-collapse / gradient health */
    float diversity_loss_weight;        /**< Output diversity loss weight (default: 0.1) */
    uint32_t diversity_buffer_size;     /**< Recent outputs to compare (default: 16) */
    bool use_gradient_normalization;    /**< Normalize gradients to fixed norm instead of clipping */
    float gradient_target_norm;         /**< Target norm for gradient normalization (default: 1.0) */

    /* Debugging */
    bool verbose;                   /**< Print training progress */
    uint32_t log_frequency;         /**< Log every N batches */
} cnn_trainer_config_t;

//=============================================================================
// Opaque Types
//=============================================================================

/**
 * @brief Opaque CNN layer handle
 */
typedef struct cnn_layer_s cnn_layer_t;

/**
 * @brief Opaque CNN trainer context
 */
typedef struct cnn_trainer_s cnn_trainer_t;

/**
 * @brief Opaque CNN dataloader handle
 */
typedef struct cnn_dataloader_s cnn_dataloader_t;

/**
 * @brief Opaque CNN-to-SNN converter handle
 */
typedef struct cnn_to_snn_converter_s cnn_to_snn_converter_t;

//=============================================================================
// Result Structures
//=============================================================================

/**
 * @brief CNN forward pass result
 */
typedef struct {
    nimcp_tensor_t* output;         /**< Network output */
    nimcp_tensor_t** activations;   /**< Per-layer activations (for backprop) */
    uint32_t num_layers;            /**< Number of layers */
    float forward_time_ms;          /**< Forward pass duration */
} cnn_forward_result_t;

/**
 * @brief CNN training epoch result
 */
typedef struct {
    uint32_t epoch;                 /**< Epoch number */
    float train_loss;               /**< Training loss */
    float train_accuracy;           /**< Training accuracy (if classification) */
    float val_loss;                 /**< Validation loss */
    float val_accuracy;             /**< Validation accuracy */
    float learning_rate;            /**< Current learning rate */
    float epoch_time_ms;            /**< Epoch duration */
    bool converged;                 /**< Early stopping triggered */
} cnn_epoch_result_t;

/**
 * @brief CNN-to-SNN conversion result
 */
typedef struct {
    snn_network_t* snn;             /**< Converted SNN network */
    float conversion_accuracy;      /**< Accuracy loss from conversion */
    uint32_t total_neurons;         /**< Total SNN neurons */
    uint32_t total_synapses;        /**< Total SNN synapses */
    float avg_firing_rate;          /**< Average firing rate (Hz) */
    float conversion_time_ms;       /**< Conversion duration */
} cnn_to_snn_result_t;

//=============================================================================
// CNN Trainer Lifecycle
//=============================================================================

/**
 * @brief Create CNN trainer
 *
 * WHAT: Initialize CNN training context with optimizers and gradient management
 * WHY:  Set up infrastructure for backpropagation-based CNN training
 * HOW:  Allocate trainer, configure optimizer and gradient manager
 *
 * @param config Trainer configuration
 * @return Trainer context or NULL on failure
 */
cnn_trainer_t* cnn_trainer_create(const cnn_trainer_config_t* config);

/**
 * @brief Destroy CNN trainer
 *
 * WHAT: Free trainer resources
 * WHY:  Prevent memory leaks
 * HOW:  Release all allocated tensors and state
 *
 * @param trainer Trainer context to destroy
 */
void cnn_trainer_destroy(cnn_trainer_t* trainer);

/**
 * @brief Get default CNN trainer configuration
 *
 * WHAT: Initialize config with sensible defaults
 * WHY:  Simplify trainer setup
 * HOW:  Set standard hyperparameters (Adam optimizer, CE loss, etc.)
 *
 * @param config Configuration to initialize
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t cnn_trainer_default_config(cnn_trainer_config_t* config);

//=============================================================================
// Layer Construction API
//=============================================================================

/**
 * @brief Add convolutional layer to trainer
 *
 * WHAT: Append Conv2D layer to network architecture
 * WHY:  Build hierarchical feature extraction (models V1 → V2 → V4)
 * HOW:  Allocate layer with kernel weights, initialize via He/Glorot
 *
 * BIOLOGICAL ANALOGY: Each conv layer ≈ cortical area with receptive fields
 *
 * @param trainer Trainer context
 * @param config Convolution configuration
 * @return Layer handle or NULL on failure
 */
cnn_layer_t* cnn_trainer_add_conv_layer(cnn_trainer_t* trainer,
                                        const cnn_conv_config_t* config);

/**
 * @brief Add pooling layer to trainer
 *
 * WHAT: Append pooling layer for spatial downsampling
 * WHY:  Achieve position invariance (complex cell behavior)
 * HOW:  Configure max/average pooling operation
 *
 * BIOLOGICAL ANALOGY: Complex cells pool over simple cell positions
 *
 * @param trainer Trainer context
 * @param config Pooling configuration
 * @return Layer handle or NULL on failure
 */
cnn_layer_t* cnn_trainer_add_pool_layer(cnn_trainer_t* trainer,
                                        const cnn_pool_config_t* config);

/**
 * @brief Add batch normalization layer
 *
 * WHAT: Append batch norm for activation normalization
 * WHY:  Stabilize training (models divisive normalization in cortex)
 * HOW:  Normalize per channel, learnable scale/shift
 *
 * BIOLOGICAL ANALOGY: Divisive normalization and lateral inhibition
 *
 * @param trainer Trainer context
 * @param config Batch norm configuration
 * @return Layer handle or NULL on failure
 */
cnn_layer_t* cnn_trainer_add_batch_norm_layer(cnn_trainer_t* trainer,
                                               const cnn_batch_norm_config_t* config);

/**
 * @brief Add dropout layer
 *
 * WHAT: Append dropout for regularization
 * WHY:  Prevent overfitting (models synaptic pruning)
 * HOW:  Stochastically drop activations during training
 *
 * BIOLOGICAL ANALOGY: Developmental synaptic pruning, stochastic transmission
 *
 * @param trainer Trainer context
 * @param config Dropout configuration
 * @return Layer handle or NULL on failure
 */
cnn_layer_t* cnn_trainer_add_dropout_layer(cnn_trainer_t* trainer,
                                            const cnn_dropout_config_t* config);

/**
 * @brief Add dense (fully connected) layer
 *
 * WHAT: Append fully connected layer
 * WHY:  Final classification or regression (IT → PFC pathway)
 * HOW:  Matrix multiplication with learned weights
 *
 * @param trainer Trainer context
 * @param config Dense layer configuration
 * @return Layer handle or NULL on failure
 */
cnn_layer_t* cnn_trainer_add_dense_layer(cnn_trainer_t* trainer,
                                          const cnn_dense_config_t* config);

/**
 * @brief Add flatten layer
 *
 * WHAT: Reshape from spatial (H, W, C) to vector
 * WHY:  Transition from conv to dense layers
 * HOW:  Flatten spatial dimensions
 *
 * @param trainer Trainer context
 * @return Layer handle or NULL on failure
 */
cnn_layer_t* cnn_trainer_add_flatten_layer(cnn_trainer_t* trainer);

/**
 * @brief Add standalone activation layer
 *
 * WHAT: Apply activation function without learned parameters
 * WHY:  Allow activation after non-conv layers (e.g., batch norm)
 * HOW:  Apply element-wise activation (ReLU, etc.)
 *
 * @param trainer Trainer context
 * @param activation Activation function type
 * @return Layer handle or NULL on failure
 */
cnn_layer_t* cnn_trainer_add_activation_layer(cnn_trainer_t* trainer,
                                               cnn_activation_t activation);

//=============================================================================
// Training API
//=============================================================================

/**
 * @brief Forward propagation through CNN
 *
 * WHAT: Compute network output and cache activations
 * WHY:  Generate predictions and prepare for backprop
 * HOW:  Sequential layer forward passes with activation caching
 *
 * @param trainer Trainer context
 * @param input Input tensor (batch_size, channels, height, width)
 * @param result Output structure to populate
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t cnn_trainer_forward(cnn_trainer_t* trainer,
                                   const nimcp_tensor_t* input,
                                   cnn_forward_result_t* result);

/**
 * @brief Backward propagation through CNN
 *
 * WHAT: Compute gradients via backpropagation
 * WHY:  Calculate weight updates for learning
 * HOW:  Reverse-order gradient computation using cached activations
 *
 * @param trainer Trainer context
 * @param target Target labels/values
 * @param forward_result Cached forward pass activations
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t cnn_trainer_backward(cnn_trainer_t* trainer,
                                    const nimcp_tensor_t* target,
                                    const cnn_forward_result_t* forward_result);

/**
 * @brief Optimizer step (weight update)
 *
 * WHAT: Update weights using computed gradients
 * WHY:  Apply learning rule to minimize loss
 * HOW:  Call optimizer (SGD/Adam) with current gradients
 *
 * @param trainer Trainer context
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t cnn_trainer_step(cnn_trainer_t* trainer);

/**
 * @brief Zero all gradients in the network
 *
 * WHAT: Reset all weight and bias gradients to zero
 * WHY:  Prevent gradient accumulation between backward passes
 * HOW:  Fill gradient tensors with zeros
 *
 * BIOLOGICAL ANALOGY: Synaptic plasticity reset before new learning trial
 *
 * @param trainer Trainer context
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t cnn_trainer_zero_grad(cnn_trainer_t* trainer);

/**
 * @brief Train CNN for one epoch
 *
 * WHAT: Complete training pass over entire dataset
 * WHY:  One iteration of learning cycle
 * HOW:  Loop over batches: forward → backward → step
 *
 * @param trainer Trainer context
 * @param dataloader Training data loader
 * @param result Epoch statistics to populate
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t cnn_trainer_train_epoch(cnn_trainer_t* trainer,
                                       cnn_dataloader_t* dataloader,
                                       cnn_epoch_result_t* result);

/**
 * @brief Validate CNN on held-out data
 *
 * WHAT: Evaluate model on validation set (no gradient updates)
 * WHY:  Monitor generalization and detect overfitting
 * HOW:  Forward-only passes, compute loss and metrics
 *
 * @param trainer Trainer context
 * @param dataloader Validation data loader
 * @param val_loss Output: validation loss
 * @param val_accuracy Output: validation accuracy
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t cnn_trainer_validate(cnn_trainer_t* trainer,
                                    cnn_dataloader_t* dataloader,
                                    float* val_loss,
                                    float* val_accuracy);

/**
 * @brief Full training loop with early stopping
 *
 * WHAT: Train for multiple epochs with validation monitoring
 * WHY:  Convenience function for complete training workflow
 * HOW:  Epoch loop with validation, checkpointing, early stopping
 *
 * @param trainer Trainer context
 * @param train_loader Training data loader
 * @param val_loader Validation data loader (optional)
 * @return NIMCP_SUCCESS on success or early stop
 */
nimcp_error_t cnn_trainer_fit(cnn_trainer_t* trainer,
                               cnn_dataloader_t* train_loader,
                               cnn_dataloader_t* val_loader);

//=============================================================================
// Data Pipeline API
//=============================================================================

/**
 * @brief Create data loader
 *
 * WHAT: Initialize data loading pipeline with batching and augmentation
 * WHY:  Manage data flow during training
 * HOW:  Wrap dataset with batch sampling and transforms
 *
 * @param data Input dataset tensor
 * @param labels Target labels tensor
 * @param config Data loader configuration
 * @return Data loader handle or NULL on failure
 */
cnn_dataloader_t* cnn_dataloader_create(const nimcp_tensor_t* data,
                                         const nimcp_tensor_t* labels,
                                         const cnn_dataloader_config_t* config);

/**
 * @brief Destroy data loader
 *
 * @param loader Data loader to destroy
 */
void cnn_dataloader_destroy(cnn_dataloader_t* loader);

/**
 * @brief Get next batch from data loader
 *
 * WHAT: Retrieve next mini-batch with augmentation
 * WHY:  Provide data for training iteration
 * HOW:  Sample batch, apply augmentation, return tensors
 *
 * @param loader Data loader
 * @param batch_data Output: batch input data
 * @param batch_labels Output: batch labels
 * @return NIMCP_SUCCESS on success, NIMCP_ERROR_NOT_FOUND if epoch complete
 */
nimcp_error_t cnn_dataloader_next_batch(cnn_dataloader_t* loader,
                                         nimcp_tensor_t** batch_data,
                                         nimcp_tensor_t** batch_labels);

/**
 * @brief Reset data loader to start of dataset
 *
 * @param loader Data loader to reset
 */
void cnn_dataloader_reset(cnn_dataloader_t* loader);

/**
 * @brief Apply data augmentation to batch
 *
 * WHAT: Apply random transformations to data batch
 * WHY:  Increase training diversity, improve generalization
 * HOW:  Random flips, rotations, crops, color jitter, etc.
 *
 * @param batch Input batch tensor (modified in-place)
 * @param config Augmentation configuration
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t cnn_augment_batch(nimcp_tensor_t* batch,
                                 const cnn_augmentation_config_t* config);

//=============================================================================
// CNN-to-SNN Conversion API
//=============================================================================

/**
 * @brief Create CNN-to-SNN converter
 *
 * WHAT: Initialize converter for CNN → SNN transformation
 * WHY:  Enable deployment as energy-efficient spiking network
 * HOW:  Configure encoding scheme and normalization parameters
 *
 * BIOLOGICAL GROUNDING:
 * Mimics developmental transition from rate-based (prenatal) to
 * spike-based (postnatal) neural coding in biological systems.
 *
 * @param config Conversion configuration
 * @return Converter handle or NULL on failure
 */
cnn_to_snn_converter_t* cnn_to_snn_converter_create(const cnn_to_snn_config_t* config);

/**
 * @brief Destroy CNN-to-SNN converter
 *
 * @param converter Converter to destroy
 */
void cnn_to_snn_converter_destroy(cnn_to_snn_converter_t* converter);

/**
 * @brief Convert trained CNN to SNN
 *
 * WHAT: Transform CNN weights and activations to spiking network
 * WHY:  Deploy CNN as SNN for neuromorphic hardware or bio-plausible operation
 * HOW:  Normalize activations → rate/population encoding → SNN synapses
 *
 * CONVERSION ALGORITHM:
 * 1. Analyze CNN activation statistics (mean, max, percentiles)
 * 2. Normalize weights to target firing rate range (0-max_rate Hz)
 * 3. Set SNN neuron thresholds based on activation quantiles
 * 4. Convert Conv2D → SNN with spatial connectivity
 * 5. Convert Dense → SNN fully connected
 * 6. Optionally fine-tune with STDP on training data
 *
 * BIOLOGICAL GROUNDING:
 * - Rate coding: Firing rate ∝ CNN activation magnitude
 * - Population coding: Multiple neurons encode single CNN unit
 * - Temporal coding: High activation → early spike
 *
 * @param converter Converter context
 * @param trainer Trained CNN trainer
 * @param calibration_data Data for normalization calibration
 * @param result Output: conversion results and SNN network
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t cnn_to_snn_convert(cnn_to_snn_converter_t* converter,
                                  const cnn_trainer_t* trainer,
                                  const nimcp_tensor_t* calibration_data,
                                  cnn_to_snn_result_t* result);

/**
 * @brief Fine-tune converted SNN with STDP
 *
 * WHAT: Refine SNN weights using spike-timing dependent plasticity
 * WHY:  Recover accuracy lost in conversion, adapt to spiking dynamics
 * HOW:  Run SNN on training data, apply STDP learning rule
 *
 * BIOLOGICAL GROUNDING:
 * Models postnatal refinement of innate circuitry via experience-dependent
 * plasticity (Hebbian learning in early sensory development).
 *
 * @param result Conversion result with SNN network
 * @param train_data Training data for fine-tuning
 * @param epochs Number of fine-tuning epochs
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t cnn_to_snn_finetune_stdp(cnn_to_snn_result_t* result,
                                        const nimcp_tensor_t* train_data,
                                        uint32_t epochs);

//=============================================================================
// NIMCP Integration API
//=============================================================================

/**
 * @brief Connect CNN trainer to visual cortex
 *
 * WHAT: Integrate CNN layers as visual cortex V1 feature extractors
 * WHY:  Enable CNN-driven visual perception in NIMCP brains
 * HOW:  Map CNN outputs to visual cortex features
 *
 * @param trainer CNN trainer
 * @param visual_cortex Visual cortex module
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t cnn_connect_visual_cortex(cnn_trainer_t* trainer,
                                         void* visual_cortex);

/**
 * @brief Connect CNN trainer to audio cortex
 *
 * WHAT: Integrate CNN for spectrotemporal feature extraction
 * WHY:  Enable CNN-driven auditory processing
 * HOW:  Map CNN outputs to cochlear/A1 representations
 *
 * @param trainer CNN trainer
 * @param audio_cortex Audio cortex module
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t cnn_connect_audio_cortex(cnn_trainer_t* trainer,
                                        void* audio_cortex);

/**
 * @brief Connect CNN trainer to bio-async router
 *
 * WHAT: Enable inter-module messaging for CNN training events
 * WHY:  Coordinate with other NIMCP modules (immune, plasticity, etc.)
 * HOW:  Register with bio-async router, send training status messages
 *
 * @param trainer CNN trainer
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t cnn_connect_bio_async(cnn_trainer_t* trainer);

/**
 * @brief Disconnect from bio-async router
 *
 * @param trainer CNN trainer
 */
void cnn_disconnect_bio_async(cnn_trainer_t* trainer);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get CNN layer output shape
 *
 * WHAT: Compute output dimensions for given layer
 * WHY:  Validate architecture, allocate tensors
 * HOW:  Apply layer-specific shape transformations
 *
 * @param layer_type Layer type
 * @param input_shape Input tensor shape
 * @param config Layer configuration
 * @param output_shape Output: computed output shape
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t cnn_get_output_shape(cnn_layer_type_t layer_type,
                                    const nimcp_tensor_shape_t* input_shape,
                                    const void* config,
                                    nimcp_tensor_shape_t* output_shape);

/**
 * @brief Count CNN parameters
 *
 * WHAT: Count total trainable parameters in network
 * WHY:  Monitor model complexity
 * HOW:  Sum weight and bias counts across layers
 *
 * @param trainer CNN trainer
 * @return Total parameter count
 */
size_t cnn_count_parameters(const cnn_trainer_t* trainer);

/**
 * @brief Get CNN layer by index
 *
 * @param trainer CNN trainer
 * @param layer_idx Layer index (0-based)
 * @return Layer handle or NULL if out of range
 */
cnn_layer_t* cnn_get_layer(const cnn_trainer_t* trainer, uint32_t layer_idx);

/**
 * @brief Get CNN layer count
 *
 * @param trainer CNN trainer
 * @return Number of layers
 */
uint32_t cnn_get_layer_count(const cnn_trainer_t* trainer);

/**
 * @brief Get layer weight gradients
 *
 * WHAT: Access weight gradient tensor for a layer
 * WHY:  Debugging, gradient visualization, custom training loops
 * HOW:  Clone internal gradient tensor
 *
 * @param trainer CNN trainer
 * @param layer_idx Layer index
 * @param out_grad Output: pointer to receive gradient copy (caller must free with nimcp_free)
 * @param out_size Output: number of gradient elements
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
nimcp_error_t cnn_get_layer_weight_grad(const cnn_trainer_t* trainer,
                                         uint32_t layer_idx,
                                         float** out_grad,
                                         size_t* out_size);

/**
 * @brief Mark trainer as managed by UTM (disables local anti-collapse + optimizer)
 */
void cnn_trainer_set_managed_by_utm(cnn_trainer_t* trainer, bool managed);

/**
 * @brief Get the input gradient from the last backward pass
 * @return Pointer to tensor (owned by trainer, do NOT destroy), or NULL
 */
const nimcp_tensor_t* cnn_trainer_get_input_grad(const cnn_trainer_t* trainer);

/**
 * @brief Get the trainer's optimizer context (for runtime LR manipulation).
 *
 * WHAT: Exposes the opaque nimcp_optimizer_context_t* owned by the trainer.
 * WHY:  Per-step LR scaling by external modulators (e.g. substrate-aware
 *       cortex adapters) needs get_lr / set_lr on the optimizer without
 *       opening the struct itself.
 * HOW:  Returns the internal pointer. The caller MUST NOT destroy the
 *       optimizer. Returns NULL if trainer is NULL or has no optimizer.
 */
nimcp_optimizer_context_t* cnn_trainer_get_optimizer(const cnn_trainer_t* trainer);

/**
 * @brief C5: Backward pass with externally provided gradient (skip internal MSE)
 *
 * Used by UTM adapter: the composite loss gradient is computed by UTM,
 * so CNN backward should use that gradient directly instead of computing
 * its own MSE gradient from targets.
 *
 * @param trainer     CNN trainer
 * @param grad_tensor External gradient tensor (dL/dOutput)
 * @param forward_result Cached forward pass activations
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t cnn_trainer_backward_with_gradient(cnn_trainer_t* trainer,
                                                   const nimcp_tensor_t* grad_tensor,
                                                   const cnn_forward_result_t* forward_result);

/**
 * @brief Save CNN trainer weights and state to file
 * @return 0 on success, -1 on error
 */
int cnn_trainer_save(const cnn_trainer_t* trainer, const char* path);

/**
 * @brief Load CNN trainer weights from file into existing trainer
 * @return 0 on success, -1 on error
 */
int cnn_trainer_load_weights(cnn_trainer_t* trainer, const char* path);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CNN_TRAINING_H */
