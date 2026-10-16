/**
 * @file nimcp_jepa_masking.h
 * @brief JEPA Masking Module - Selective Information Hiding for Self-Supervised Learning
 * @version 1.0.0
 * @date 2025-12-26
 *
 * WHAT: Generates masks for JEPA training to hide portions of input
 * WHY:  Core of JEPA training - predict masked regions from visible context
 * HOW:  Multiple masking strategies: random, block, attention-guided, curriculum
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * MASKED PREDICTION (JEPA):
 * -------------------------
 * During training, portions of the input are masked:
 *
 *   x_visible = x ⊙ (1 - mask)
 *   x_target  = x ⊙ mask
 *
 * The encoder sees only x_visible, and the predictor must reconstruct
 * the latent representation of x_target:
 *
 *   z_ctx = Encoder(x_visible)
 *   z_pred = Predictor(z_ctx)
 *   Loss = ||z_pred - StopGrad(Encoder(x_target))||²
 *
 * MASKING STRATEGIES:
 * -------------------
 * 1. RANDOM: Independent random mask per position (simple baseline)
 * 2. BLOCK:  Contiguous rectangular regions (spatial coherence)
 * 3. ATTENTION: Guided by attention scores (mask salient regions)
 * 4. CURRICULUM: Start easy (small masks), gradually increase
 *
 * BIOLOGICAL BASIS:
 * -----------------
 * - Random masking: Stochastic neural dropout
 * - Block masking: Saccadic blind spots during eye movements
 * - Attention masking: Selective attention gates information flow
 * - Curriculum: Developmental learning progression
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_JEPA_MASKING_H
#define NIMCP_JEPA_MASKING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/bridge/nimcp_bridge_base.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Bio-async module ID for JEPA masking */
#define BIO_MODULE_JEPA_MASKING                 0x0E02

/** @brief Maximum mask dimensions */
#define JEPA_MASK_MAX_WIDTH                     256
#define JEPA_MASK_MAX_HEIGHT                    256
#define JEPA_MASK_MAX_TEMPORAL                  64

/** @brief Default masking ratio (fraction of input masked) */
#define JEPA_MASK_DEFAULT_RATIO                 0.75f

/** @brief Default block aspect ratio range */
#define JEPA_MASK_MIN_ASPECT_RATIO              0.3f
#define JEPA_MASK_MAX_ASPECT_RATIO              3.0f

/** @brief Curriculum learning epochs */
#define JEPA_MASK_CURRICULUM_START_RATIO        0.25f
#define JEPA_MASK_CURRICULUM_END_RATIO          0.85f
#define JEPA_MASK_CURRICULUM_EPOCHS             100

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Masking strategy types
 */
typedef enum {
    JEPA_MASK_RANDOM = 0,           /**< Independent random per position */
    JEPA_MASK_BLOCK,                /**< Contiguous block regions */
    JEPA_MASK_BLOCK_MULTI,          /**< Multiple non-overlapping blocks */
    JEPA_MASK_ATTENTION_GUIDED,     /**< Guided by attention scores */
    JEPA_MASK_CURRICULUM,           /**< Adaptive difficulty over training */
    JEPA_MASK_TUBE,                 /**< Temporal tubes (for video) */
    JEPA_MASK_CAUSAL                /**< Causal masking (for sequences) */
} jepa_mask_strategy_t;

/**
 * @brief Mask shape types (for block masking)
 */
typedef enum {
    JEPA_MASK_SHAPE_RECT = 0,       /**< Rectangular blocks */
    JEPA_MASK_SHAPE_SQUARE,         /**< Square blocks only */
    JEPA_MASK_SHAPE_ELLIPSE,        /**< Elliptical regions */
    JEPA_MASK_SHAPE_IRREGULAR       /**< Irregular connected regions */
} jepa_mask_shape_t;

/**
 * @brief Mask application mode
 */
typedef enum {
    JEPA_MASK_MODE_PATCH = 0,       /**< Mask at patch level */
    JEPA_MASK_MODE_PIXEL,           /**< Mask at pixel level */
    JEPA_MASK_MODE_FEATURE          /**< Mask in feature space */
} jepa_mask_mode_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief 2D/3D mask representation
 */
typedef struct {
    float* data;                    /**< Mask values [0,1], 1=masked */
    uint32_t width;                 /**< Spatial width */
    uint32_t height;                /**< Spatial height */
    uint32_t temporal;              /**< Temporal depth (1 for images) */
    uint32_t total_size;            /**< Total elements */

    /* Masking statistics */
    float mask_ratio;               /**< Actual masked fraction */
    uint32_t num_masked;            /**< Number of masked elements */
    uint32_t num_visible;           /**< Number of visible elements */
} jepa_mask_t;

/**
 * @brief Block mask parameters
 */
typedef struct {
    uint32_t num_blocks;            /**< Number of mask blocks */
    float min_block_ratio;          /**< Minimum block size ratio */
    float max_block_ratio;          /**< Maximum block size ratio */
    float min_aspect_ratio;         /**< Minimum aspect ratio */
    float max_aspect_ratio;         /**< Maximum aspect ratio */
    bool allow_overlap;             /**< Allow overlapping blocks */
    jepa_mask_shape_t shape;        /**< Block shape type */
} jepa_block_params_t;

/**
 * @brief Attention-guided mask parameters
 */
typedef struct {
    const float* attention_scores;  /**< Attention weights [H, W] */
    float attention_threshold;      /**< Threshold for masking (0-1) */
    bool mask_high_attention;       /**< true=mask high, false=mask low */
    float temperature;              /**< Softmax temperature for sampling */
} jepa_attention_params_t;

/**
 * @brief Curriculum learning parameters
 */
typedef struct {
    float start_ratio;              /**< Starting mask ratio */
    float end_ratio;                /**< Ending mask ratio */
    uint32_t warmup_steps;          /**< Steps to ramp up */
    uint32_t current_step;          /**< Current training step */
    float (*schedule_fn)(float t);  /**< Custom schedule function (optional) */
} jepa_curriculum_params_t;

/**
 * @brief Tube masking parameters (for video)
 */
typedef struct {
    uint32_t num_tubes;             /**< Number of tubes */
    float tube_ratio;               /**< Fraction of patches per tube */
    bool consistent_spatial;        /**< Same spatial position across time */
} jepa_tube_params_t;

/**
 * @brief Masking configuration
 */
typedef struct {
    jepa_mask_strategy_t strategy;  /**< Masking strategy */
    jepa_mask_mode_t mode;          /**< Application mode */
    float target_ratio;             /**< Target mask ratio */

    /* Strategy-specific parameters */
    union {
        jepa_block_params_t block;
        jepa_attention_params_t attention;
        jepa_curriculum_params_t curriculum;
        jepa_tube_params_t tube;
    } params;

    /* Random seed for reproducibility */
    uint32_t seed;
    bool use_fixed_seed;            /**< Use fixed seed each call */
} jepa_mask_config_t;

/**
 * @brief Mask generator state
 */
typedef struct jepa_mask_generator {
    bridge_base_t base;             /**< MUST be first - bridge pattern */

    /* Configuration */
    jepa_mask_config_t config;

    /* Internal state */
    uint64_t masks_generated;       /**< Total masks generated */
    uint64_t random_state;          /**< PRNG state */

    /* Pre-allocated buffers */
    float* temp_buffer;             /**< Temporary work buffer */
    uint32_t temp_buffer_size;

    /* Curriculum state */
    float current_ratio;            /**< Current effective ratio */
    uint32_t curriculum_step;       /**< Current curriculum step */
} jepa_mask_generator_t;

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Initialize default mask configuration
 *
 * @param config Output configuration
 * @param strategy Masking strategy to use
 * @return NIMCP_SUCCESS on success
 */
int jepa_mask_default_config(jepa_mask_config_t* config, jepa_mask_strategy_t strategy);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create a mask generator
 *
 * WHAT: Initialize mask generator with given configuration
 * WHY:  Central component for JEPA training data augmentation
 * HOW:  Allocate state, initialize PRNG, setup strategy-specific params
 *
 * @param config Configuration (NULL for defaults with RANDOM strategy)
 * @return New generator or NULL on failure
 */
jepa_mask_generator_t* jepa_mask_generator_create(const jepa_mask_config_t* config);

/**
 * @brief Destroy mask generator
 *
 * @param generator Generator to destroy (NULL safe)
 */
void jepa_mask_generator_destroy(jepa_mask_generator_t* generator);

/**
 * @brief Reset generator state
 *
 * WHAT: Reset PRNG and statistics, optionally re-seed
 * WHY:  Allow reproducible mask sequences
 *
 * @param generator Generator to reset
 * @param new_seed New seed (0 = keep current seed)
 * @return NIMCP_SUCCESS on success
 */
int jepa_mask_generator_reset(jepa_mask_generator_t* generator, uint32_t new_seed);

/* ============================================================================
 * Mask Generation API
 * ============================================================================ */

/**
 * @brief Generate a 2D spatial mask
 *
 * WHAT: Create mask for image/patch grid
 * WHY:  Primary use case for visual JEPA
 * HOW:  Apply configured strategy to generate binary/soft mask
 *
 * @param generator Mask generator
 * @param width Spatial width (patches or pixels)
 * @param height Spatial height
 * @param mask Output mask (must be pre-allocated)
 * @return NIMCP_SUCCESS on success
 */
int jepa_mask_generate_2d(jepa_mask_generator_t* generator,
                           uint32_t width,
                           uint32_t height,
                           jepa_mask_t* mask);

/**
 * @brief Generate a 3D spatiotemporal mask
 *
 * WHAT: Create mask for video data
 * WHY:  V-JEPA style temporal masking
 * HOW:  Apply tube or block masking across time
 *
 * @param generator Mask generator
 * @param width Spatial width
 * @param height Spatial height
 * @param temporal Temporal frames
 * @param mask Output mask
 * @return NIMCP_SUCCESS on success
 */
int jepa_mask_generate_3d(jepa_mask_generator_t* generator,
                           uint32_t width,
                           uint32_t height,
                           uint32_t temporal,
                           jepa_mask_t* mask);

/**
 * @brief Generate a 1D sequence mask
 *
 * WHAT: Create mask for sequential data
 * WHY:  Speech, text, or other 1D sequences
 * HOW:  Random or causal masking
 *
 * @param generator Mask generator
 * @param length Sequence length
 * @param mask Output mask
 * @return NIMCP_SUCCESS on success
 */
int jepa_mask_generate_1d(jepa_mask_generator_t* generator,
                           uint32_t length,
                           jepa_mask_t* mask);

/**
 * @brief Generate mask with attention guidance
 *
 * WHAT: Create mask guided by attention scores
 * WHY:  Mask salient/attended regions for harder prediction
 * HOW:  Sample masked positions proportional to attention
 *
 * @param generator Mask generator
 * @param attention Attention scores [width × height]
 * @param width Spatial width
 * @param height Spatial height
 * @param mask Output mask
 * @return NIMCP_SUCCESS on success
 */
int jepa_mask_generate_attention(jepa_mask_generator_t* generator,
                                  const float* attention,
                                  uint32_t width,
                                  uint32_t height,
                                  jepa_mask_t* mask);

/* ============================================================================
 * Mask Creation/Destruction
 * ============================================================================ */

/**
 * @brief Create mask structure
 *
 * @param width Spatial width
 * @param height Spatial height
 * @param temporal Temporal depth (1 for 2D)
 * @return New mask or NULL
 */
jepa_mask_t* jepa_mask_create(uint32_t width, uint32_t height, uint32_t temporal);

/**
 * @brief Destroy mask structure
 *
 * @param mask Mask to destroy (NULL safe)
 */
void jepa_mask_destroy(jepa_mask_t* mask);

/**
 * @brief Clone a mask
 *
 * @param src Source mask
 * @return New copy or NULL
 */
jepa_mask_t* jepa_mask_clone(const jepa_mask_t* src);

/* ============================================================================
 * Mask Operations
 * ============================================================================ */

/**
 * @brief Apply mask to embedding array
 *
 * WHAT: Zero out masked positions in embedding
 * WHY:  Create visible/target splits for training
 * HOW:  out[i] = input[i] * (1 - mask[i])
 *
 * @param mask Mask to apply
 * @param input Input embeddings [size × dim]
 * @param output Output embeddings [size × dim]
 * @param dim Embedding dimension
 * @return NIMCP_SUCCESS on success
 */
int jepa_mask_apply(const jepa_mask_t* mask,
                     const float* input,
                     float* output,
                     uint32_t dim);

/**
 * @brief Get indices of visible (unmasked) positions
 *
 * @param mask Input mask
 * @param indices Output index array (must have space for num_visible)
 * @param num_indices Output number of indices
 * @return NIMCP_SUCCESS on success
 */
int jepa_mask_get_visible_indices(const jepa_mask_t* mask,
                                   uint32_t* indices,
                                   uint32_t* num_indices);

/**
 * @brief Get indices of masked positions
 *
 * @param mask Input mask
 * @param indices Output index array
 * @param num_indices Output number of indices
 * @return NIMCP_SUCCESS on success
 */
int jepa_mask_get_masked_indices(const jepa_mask_t* mask,
                                  uint32_t* indices,
                                  uint32_t* num_indices);

/**
 * @brief Invert mask (swap visible and masked)
 *
 * @param mask Mask to invert (in place)
 * @return NIMCP_SUCCESS on success
 */
int jepa_mask_invert(jepa_mask_t* mask);

/**
 * @brief Compute mask statistics
 *
 * WHAT: Calculate ratio, counts, spatial distribution
 * WHY:  Monitor masking for debugging/logging
 *
 * @param mask Input mask
 * @return NIMCP_SUCCESS on success (updates mask->mask_ratio, etc.)
 */
int jepa_mask_compute_stats(jepa_mask_t* mask);

/* ============================================================================
 * Curriculum Learning API
 * ============================================================================ */

/**
 * @brief Advance curriculum step
 *
 * WHAT: Update curriculum learning state
 * WHY:  Gradually increase masking difficulty
 *
 * @param generator Mask generator (must be CURRICULUM strategy)
 * @return NIMCP_SUCCESS on success
 */
int jepa_mask_curriculum_step(jepa_mask_generator_t* generator);

/**
 * @brief Set curriculum step directly
 *
 * @param generator Mask generator
 * @param step Step to set
 * @return NIMCP_SUCCESS on success
 */
int jepa_mask_curriculum_set_step(jepa_mask_generator_t* generator, uint32_t step);

/**
 * @brief Get current curriculum ratio
 *
 * @param generator Mask generator
 * @return Current mask ratio, 0 on error
 */
float jepa_mask_curriculum_get_ratio(const jepa_mask_generator_t* generator);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect generator to bio-async router
 *
 * @param generator Mask generator
 * @return NIMCP_SUCCESS on success
 */
int jepa_mask_generator_connect_bio_async(jepa_mask_generator_t* generator);

/**
 * @brief Disconnect from bio-async router
 *
 * @param generator Mask generator
 * @return NIMCP_SUCCESS on success
 */
int jepa_mask_generator_disconnect_bio_async(jepa_mask_generator_t* generator);

/**
 * @brief Check bio-async connection status
 *
 * @param generator Mask generator
 * @return true if connected
 */
bool jepa_mask_generator_is_bio_async_connected(const jepa_mask_generator_t* generator);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

/**
 * @brief Convert strategy to string
 *
 * @param strategy Mask strategy
 * @return Human-readable string
 */
const char* jepa_mask_strategy_to_string(jepa_mask_strategy_t strategy);

/**
 * @brief Convert shape to string
 *
 * @param shape Mask shape
 * @return Human-readable string
 */
const char* jepa_mask_shape_to_string(jepa_mask_shape_t shape);

/**
 * @brief Convert mode to string
 *
 * @param mode Mask mode
 * @return Human-readable string
 */
const char* jepa_mask_mode_to_string(jepa_mask_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_JEPA_MASKING_H */
