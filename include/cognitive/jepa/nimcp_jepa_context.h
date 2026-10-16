/**
 * @file nimcp_jepa_context.h
 * @brief JEPA Context Encoder - Task-Conditioned Representation Learning
 * @version 1.0.0
 * @date 2025-12-26
 *
 * WHAT: Context-conditioned encoding for JEPA embeddings
 * WHY:  Same input should produce different representations based on task
 * HOW:  Integrate with Working Memory, Attention, Executive Function
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * CONTEXT-DEPENDENT REPRESENTATIONS:
 * ----------------------------------
 * The brain represents the same stimulus differently based on:
 *   - Current task (what to attend to)
 *   - Prior expectations (what to predict)
 *   - Goals (what matters for action)
 *
 * EXAMPLE:
 * --------
 * Same image of a dog can be encoded as:
 *   - "pet" in a companion context
 *   - "mammal" in a biology context
 *   - "obstacle" in a navigation context
 *
 * CONDITIONING MECHANISMS:
 * ------------------------
 * 1. ATTENTION GATING: Task-relevant features are amplified
 * 2. CONTEXT CONCATENATION: Context vector appended to input
 * 3. FILM CONDITIONING: Feature-wise Linear Modulation
 * 4. CROSS-ATTENTION: Context attends to content
 *
 * BIOLOGICAL BASIS:
 * -----------------
 * Prefrontal Cortex (PFC) provides top-down modulation:
 *   - dlPFC: Task rules and working memory maintenance
 *   - vmPFC: Value and goal representations
 *   - ACC: Conflict monitoring and task switching
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    CONTEXT-CONDITIONED ENCODING                           ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                           ║
 * ║   Executive        Working Memory        Attention                        ║
 * ║   ┌──────────┐     ┌────────────┐       ┌────────────┐                    ║
 * ║   │ Task/Goal│     │ Recent WM  │       │ Attention  │                    ║
 * ║   │ State    │     │ Contents   │       │ Weights    │                    ║
 * ║   └────┬─────┘     └─────┬──────┘       └─────┬──────┘                    ║
 * ║        │                 │                    │                           ║
 * ║        └─────────────────┼────────────────────┘                           ║
 * ║                          │                                                ║
 * ║                          ▼                                                ║
 * ║                   ┌────────────────┐                                      ║
 * ║                   │ Context Vector │                                      ║
 * ║                   │ c = [goal, wm, │                                      ║
 * ║                   │      attn_ctx] │                                      ║
 * ║                   └───────┬────────┘                                      ║
 * ║                           │                                               ║
 * ║   JEPA Embedding          │                                               ║
 * ║   ┌──────────────┐        │                                               ║
 * ║   │ z_input      │────────┼───────────────────────────┐                   ║
 * ║   └──────────────┘        │                           │                   ║
 * ║                           ▼                           ▼                   ║
 * ║                   ┌───────────────┐           ┌──────────────┐            ║
 * ║                   │ FiLM Layer    │    OR     │ Cross-Attn   │            ║
 * ║                   │ γ(c)·z + β(c) │           │ Attn(z, c)   │            ║
 * ║                   └───────┬───────┘           └──────┬───────┘            ║
 * ║                           │                          │                    ║
 * ║                           └──────────┬───────────────┘                    ║
 * ║                                      │                                    ║
 * ║                                      ▼                                    ║
 * ║                           ┌────────────────────┐                          ║
 * ║                           │ z_conditioned      │                          ║
 * ║                           │ (task-specific)    │                          ║
 * ║                           └────────────────────┘                          ║
 * ║                                                                           ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_JEPA_CONTEXT_H
#define NIMCP_JEPA_CONTEXT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/jepa/nimcp_jepa_latent.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Bio-async module ID for JEPA Context */
#define BIO_MODULE_JEPA_CONTEXT         0x0E30

/** @brief Default context vector dimension */
#define JEPA_CONTEXT_DEFAULT_DIM        128

/** @brief Maximum number of context sources */
#define JEPA_CONTEXT_MAX_SOURCES        8

/** @brief Default number of attention heads for cross-attention */
#define JEPA_CONTEXT_DEFAULT_NUM_HEADS  4

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Context conditioning mechanisms
 */
typedef enum {
    JEPA_COND_CONCATENATE = 0,      /**< Append context to embedding */
    JEPA_COND_FILM,                 /**< Feature-wise Linear Modulation */
    JEPA_COND_CROSS_ATTENTION,      /**< Cross-attention conditioning */
    JEPA_COND_ADDITIVE,             /**< Simple additive conditioning */
    JEPA_COND_MULTIPLICATIVE,       /**< Element-wise multiplication */
    JEPA_COND_GATED                 /**< Gated conditioning */
} jepa_conditioning_t;

/**
 * @brief Context source types
 */
typedef enum {
    JEPA_CTX_TASK = 0,              /**< Current task/goal from executive */
    JEPA_CTX_WORKING_MEMORY,        /**< Recent working memory contents */
    JEPA_CTX_ATTENTION,             /**< Attention state/weights */
    JEPA_CTX_TEMPORAL,              /**< Temporal/sequence context */
    JEPA_CTX_SPATIAL,               /**< Spatial context (for vision) */
    JEPA_CTX_SEMANTIC,              /**< Semantic/conceptual context */
    JEPA_CTX_CUSTOM                 /**< User-defined context */
} jepa_context_source_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Context source configuration
 */
typedef struct {
    jepa_context_source_t type;     /**< Source type */
    uint32_t dim;                   /**< Dimension of this context */
    float weight;                   /**< Importance weight */
    bool enabled;                   /**< Whether to use this source */
    const char* name;               /**< Optional name for debugging */
} jepa_context_source_config_t;

/**
 * @brief Context encoder configuration
 */
typedef struct {
    /* Context composition */
    uint32_t context_dim;           /**< Final context vector dimension */
    uint32_t num_sources;           /**< Number of context sources */
    jepa_context_source_config_t sources[JEPA_CONTEXT_MAX_SOURCES];

    /* Conditioning mechanism */
    jepa_conditioning_t conditioning;

    /* For FILM conditioning */
    uint32_t film_hidden_dim;       /**< FiLM MLP hidden dimension */

    /* For cross-attention */
    uint32_t num_attention_heads;   /**< Number of attention heads */
    uint32_t attention_dim;         /**< Attention key/value dimension */

    /* Output */
    uint32_t input_dim;             /**< Input JEPA embedding dimension */
    uint32_t output_dim;            /**< Output conditioned dimension */

    /* Training */
    float dropout_rate;
    bool use_layer_norm;
} jepa_context_config_t;

/**
 * @brief Task/goal state (from executive function)
 */
typedef struct {
    float* goal_embedding;          /**< Current goal representation */
    uint32_t goal_dim;
    float* task_embedding;          /**< Current task representation */
    uint32_t task_dim;
    float confidence;               /**< Task confidence (0-1) */
    uint64_t task_id;               /**< Unique task identifier */
} jepa_task_state_t;

/**
 * @brief Working memory context
 */
typedef struct {
    float* recent_items;            /**< Flattened recent WM items */
    uint32_t item_dim;              /**< Dimension per item */
    uint32_t num_items;             /**< Number of recent items */
    float* aggregated;              /**< Aggregated WM representation */
    uint32_t aggregated_dim;
} jepa_wm_context_t;

/**
 * @brief Attention context
 */
typedef struct {
    float* attention_weights;       /**< Current attention weights */
    uint32_t num_locations;         /**< Number of attended locations */
    float* attended_features;       /**< Attended feature summary */
    uint32_t feature_dim;
    float entropy;                  /**< Attention entropy (diffuse vs focused) */
} jepa_attention_context_t;

/**
 * @brief Combined context state
 */
typedef struct {
    jepa_task_state_t task;         /**< Task/goal context */
    jepa_wm_context_t working_memory; /**< Working memory context */
    jepa_attention_context_t attention; /**< Attention context */
    float* temporal_context;        /**< Temporal/sequence context */
    uint32_t temporal_dim;
    float* custom_context;          /**< User-defined context */
    uint32_t custom_dim;
    uint64_t timestamp_ms;          /**< Context timestamp */
} jepa_context_state_t;

/**
 * @brief FiLM conditioning layer
 */
typedef struct {
    float* gamma_weights;           /**< Scale network: context → gamma */
    float* gamma_bias;
    float* beta_weights;            /**< Shift network: context → beta */
    float* beta_bias;
    uint32_t context_dim;
    uint32_t feature_dim;
    uint32_t hidden_dim;
    float* hidden_weights;          /**< Optional hidden layer */
    float* hidden_bias;
} jepa_film_layer_t;

/**
 * @brief Cross-attention layer for conditioning
 */
typedef struct {
    float* q_weights;               /**< Query projection (from input) */
    float* k_weights;               /**< Key projection (from context) */
    float* v_weights;               /**< Value projection (from context) */
    float* o_weights;               /**< Output projection */
    uint32_t num_heads;
    uint32_t head_dim;
    uint32_t input_dim;
    uint32_t context_dim;
    uint32_t output_dim;
} jepa_cross_attention_t;

/**
 * @brief Context encoder statistics
 */
typedef struct {
    uint64_t encodings_performed;   /**< Total context-conditioned encodings */
    uint64_t context_updates;       /**< Context state updates */
    float avg_context_norm;         /**< Average context vector norm */
    float avg_modulation_strength;  /**< Average FiLM modulation magnitude */
    float avg_attention_entropy;    /**< Average attention entropy */
} jepa_context_stats_t;

/**
 * @brief Context encoder state
 */
typedef struct jepa_context_encoder {
    bridge_base_t base;             /**< MUST be first - bridge pattern */

    /* Configuration */
    jepa_context_config_t config;

    /* Current context state */
    jepa_context_state_t* current_context;

    /* Context composition */
    float* composed_context;        /**< Composed context vector */
    float* context_weights;         /**< Source combination weights */

    /* Conditioning layers */
    union {
        jepa_film_layer_t* film;
        jepa_cross_attention_t* cross_attn;
        float* additive_proj;       /**< Projection for additive */
        float* gate_weights;        /**< Gate for gated conditioning */
    } conditioning;

    /* Working buffers */
    float* input_buffer;
    float* output_buffer;
    float* temp_buffer;

    /* Statistics */
    jepa_context_stats_t stats;
} jepa_context_encoder_t;

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default context encoder configuration
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
int jepa_context_default_config(jepa_context_config_t* config);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create context encoder
 *
 * WHAT: Initialize context-conditioned encoding system
 * WHY:  Enable task-specific JEPA representations
 * HOW:  Create conditioning layers and context buffers
 *
 * @param config Configuration (NULL for defaults)
 * @return New encoder or NULL on failure
 */
jepa_context_encoder_t* jepa_context_encoder_create(
    const jepa_context_config_t* config
);

/**
 * @brief Destroy context encoder
 *
 * @param encoder Encoder to destroy (NULL safe)
 */
void jepa_context_encoder_destroy(jepa_context_encoder_t* encoder);

/**
 * @brief Reset context encoder state
 *
 * @param encoder Context encoder
 * @return NIMCP_SUCCESS on success
 */
int jepa_context_encoder_reset(jepa_context_encoder_t* encoder);

/* ============================================================================
 * Context State API
 * ============================================================================ */

/**
 * @brief Create context state
 *
 * @return New context state or NULL
 */
jepa_context_state_t* jepa_context_state_create(void);

/**
 * @brief Destroy context state
 *
 * @param state State to destroy (NULL safe)
 */
void jepa_context_state_destroy(jepa_context_state_t* state);

/**
 * @brief Set task context
 *
 * @param encoder Context encoder
 * @param goal_embedding Goal representation
 * @param goal_dim Goal dimension
 * @param task_embedding Task representation
 * @param task_dim Task dimension
 * @return NIMCP_SUCCESS on success
 */
int jepa_context_set_task(
    jepa_context_encoder_t* encoder,
    const float* goal_embedding,
    uint32_t goal_dim,
    const float* task_embedding,
    uint32_t task_dim
);

/**
 * @brief Set working memory context
 *
 * @param encoder Context encoder
 * @param wm_items Recent working memory items (flattened)
 * @param item_dim Dimension per item
 * @param num_items Number of items
 * @return NIMCP_SUCCESS on success
 */
int jepa_context_set_working_memory(
    jepa_context_encoder_t* encoder,
    const float* wm_items,
    uint32_t item_dim,
    uint32_t num_items
);

/**
 * @brief Set attention context
 *
 * @param encoder Context encoder
 * @param attention_weights Current attention weights
 * @param num_locations Number of attended locations
 * @param attended_features Attended feature summary
 * @param feature_dim Feature dimension
 * @return NIMCP_SUCCESS on success
 */
int jepa_context_set_attention(
    jepa_context_encoder_t* encoder,
    const float* attention_weights,
    uint32_t num_locations,
    const float* attended_features,
    uint32_t feature_dim
);

/**
 * @brief Set custom context
 *
 * @param encoder Context encoder
 * @param custom Custom context vector
 * @param dim Custom context dimension
 * @return NIMCP_SUCCESS on success
 */
int jepa_context_set_custom(
    jepa_context_encoder_t* encoder,
    const float* custom,
    uint32_t dim
);

/**
 * @brief Clear all context state
 *
 * @param encoder Context encoder
 * @return NIMCP_SUCCESS on success
 */
int jepa_context_clear(jepa_context_encoder_t* encoder);

/* ============================================================================
 * Encoding API
 * ============================================================================ */

/**
 * @brief Encode JEPA embedding with context conditioning
 *
 * WHAT: Apply context to transform embedding
 * WHY:  Same input → different representations for different tasks
 * HOW:  Use configured conditioning mechanism (FiLM, cross-attn, etc.)
 *
 * @param encoder Context encoder
 * @param input Input JEPA embedding
 * @param output Output conditioned embedding
 * @return NIMCP_SUCCESS on success
 */
int jepa_context_encode(
    jepa_context_encoder_t* encoder,
    const jepa_latent_t* input,
    jepa_latent_t* output
);

/**
 * @brief Batch encoding with context
 *
 * @param encoder Context encoder
 * @param inputs Array of input embeddings
 * @param outputs Array of output embeddings
 * @param batch_size Number of embeddings
 * @return NIMCP_SUCCESS on success
 */
int jepa_context_encode_batch(
    jepa_context_encoder_t* encoder,
    jepa_latent_t** inputs,
    jepa_latent_t** outputs,
    uint32_t batch_size
);

/**
 * @brief Get composed context vector
 *
 * @param encoder Context encoder
 * @param context Output context vector
 * @param dim Context dimension
 * @return NIMCP_SUCCESS on success
 */
int jepa_context_get_composed(
    const jepa_context_encoder_t* encoder,
    float* context,
    uint32_t dim
);

/* ============================================================================
 * Conditioning Mechanism API
 * ============================================================================ */

/**
 * @brief Apply FiLM conditioning
 *
 * WHAT: Feature-wise Linear Modulation
 * HOW:  output = γ(context) * input + β(context)
 *
 * @param encoder Context encoder (must have FiLM layer)
 * @param input Input features
 * @param context Context vector
 * @param output Output modulated features
 * @param dim Feature dimension
 * @return NIMCP_SUCCESS on success
 */
int jepa_context_apply_film(
    jepa_context_encoder_t* encoder,
    const float* input,
    const float* context,
    float* output,
    uint32_t dim
);

/**
 * @brief Apply cross-attention conditioning
 *
 * WHAT: Cross-attention between input and context
 * HOW:  output = Attention(Q=input, K=context, V=context)
 *
 * @param encoder Context encoder (must have cross-attention)
 * @param input Input features
 * @param context Context features
 * @param output Output attended features
 * @return NIMCP_SUCCESS on success
 */
int jepa_context_apply_cross_attention(
    jepa_context_encoder_t* encoder,
    const float* input,
    const float* context,
    float* output
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get context encoder statistics
 *
 * @param encoder Context encoder
 * @param stats Output statistics
 * @return NIMCP_SUCCESS on success
 */
int jepa_context_get_stats(
    const jepa_context_encoder_t* encoder,
    jepa_context_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param encoder Context encoder
 * @return NIMCP_SUCCESS on success
 */
int jepa_context_reset_stats(jepa_context_encoder_t* encoder);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * @param encoder Context encoder
 * @return NIMCP_SUCCESS on success
 */
int jepa_context_connect_bio_async(jepa_context_encoder_t* encoder);

/**
 * @brief Disconnect from bio-async router
 *
 * @param encoder Context encoder
 * @return NIMCP_SUCCESS on success
 */
int jepa_context_disconnect_bio_async(jepa_context_encoder_t* encoder);

/**
 * @brief Check bio-async connection
 *
 * @param encoder Context encoder
 * @return true if connected
 */
bool jepa_context_is_bio_async_connected(const jepa_context_encoder_t* encoder);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_JEPA_CONTEXT_H */
