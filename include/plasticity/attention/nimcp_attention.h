/**
 * @file nimcp_attention.h
 * @brief Biology-based multihead attention mechanism
 *
 * WHAT: Implements cortical column-inspired parallel attention streams with thalamic gating
 * WHY:  Enables selective focus on relevant input features across multiple representation spaces
 * HOW:  Multiple attention heads (cortical columns) process input in parallel,
 *       modulated by thalamic gating for top-down control
 *
 * BIOLOGICAL INSPIRATION:
 * - Cortical Columns: Each attention head acts as a specialized processing column
 * - Thalamic Gating: Gate mechanism controls information flow (like thalamic relay)
 * - Salience Weighting: Biologically-inspired attention based on feature importance
 * - Parallel Streams: Multiple heads process different aspects simultaneously
 *
 * DESIGN PATTERNS:
 * - Strategy Pattern: Pluggable attention computation strategies
 * - Factory Pattern: Create attention heads with different configurations
 * - Composite Pattern: Combine multiple heads into unified system
 * - SRP: Each component has single, well-defined responsibility
 *
 * PERFORMANCE:
 * - SIMD-friendly memory layout (cache-aligned structures)
 * - Thread-local buffers for zero-contention computation
 * - Atomic statistics for lock-free monitoring
 * - Early returns to minimize computation
 */

#ifndef NIMCP_ATTENTION_H
#define NIMCP_ATTENTION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "utils/encoding/nimcp_positional_encoding.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct attention_head_struct* attention_head_t;
typedef struct multihead_attention_struct* multihead_attention_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * WHAT: Configuration for single attention head (cortical column)
 * WHY:  Each head specializes in different feature representations
 */
typedef struct {
    uint32_t input_dim;           // Input feature dimension
    uint32_t output_dim;          // Output dimension per head
    uint32_t key_dim;             // Key/query dimension (typically input_dim / num_heads)
    uint32_t value_dim;           // Value dimension (typically same as key_dim)
    float temperature;            // Softmax temperature (biological gain control)
    float dropout_rate;           // Dropout for regularization (0.0 = none)
} attention_head_config_t;

/**
 * WHAT: Configuration for multihead attention system
 * WHY:  Coordinates multiple parallel attention streams with gating
 */
typedef struct {
    uint32_t num_heads;           // Number of attention heads (cortical columns)
    uint32_t input_dim;           // Input feature dimension
    uint32_t output_dim;          // Final output dimension
    uint32_t sequence_length;     // Maximum sequence length
    bool use_thalamic_gate;       // Enable thalamic gating mechanism
    bool use_salience_weighting;  // Enable salience-based attention
    float gate_bias;              // Thalamic gate bias (default opening)

    /* WHAT: Positional encoding configuration (Strategy Pattern)
     * WHY:  Allow runtime selection of RoPE vs ALiBi vs none
     * HOW:  Encoder created based on pe_type, applied in forward pass
     */
    bool use_positional_encoding; // Enable positional encoding
    nimcp_pos_encoding_type_t pe_type;  // PE type: NIMCP_POS_ROTARY or NIMCP_POS_ALIBI
    float rope_base;              // RoPE frequency base (default: 10000.0)
    float alibi_slope_base;       // ALiBi slope base for geometric sequence

    /* WHAT: Quantum attention acceleration configuration
     * WHY:  Enable O(√N) speedup for attention head selection
     * HOW:  Grover-inspired amplitude amplification for head selection
     */
    bool enable_quantum_attention; // Enable quantum-accelerated attention (default: true)
} multihead_attention_config_t;

/**
 * WHAT: Statistics for monitoring attention system
 * WHY:  Track performance and behavior for debugging and optimization
 */
typedef struct {
    uint64_t forward_calls;       // Number of forward passes
    float avg_attention_entropy;  // Average entropy of attention weights
    float avg_gate_activation;    // Average thalamic gate opening
    uint32_t active_heads;        // Number of currently active heads
    float computation_time_ms;    // Average forward pass time
} attention_stats_t;

//=============================================================================
// Attention Head API (Single Cortical Column)
//=============================================================================

/**
 * WHAT: Create single attention head (cortical column)
 * WHY:  Each head specializes in different representational space
 * @param config Head configuration
 * @return Attention head handle or NULL on failure
 * PERFORMANCE: ~10μs creation time, thread-safe
 */
attention_head_t attention_head_create(const attention_head_config_t* config);

/**
 * WHAT: Destroy attention head and free resources
 * WHY:  Clean up allocated memory
 * @param head Attention head to destroy
 * THREAD_SAFETY: Must not be called concurrently
 */
void attention_head_destroy(attention_head_t head);

/**
 * WHAT: Compute attention for single head
 * WHY:  Projects input to Q/K/V, computes attention weights, applies to values
 * @param head Attention head
 * @param query Query vectors [sequence_length × input_dim]
 * @param key Key vectors [sequence_length × input_dim]
 * @param value Value vectors [sequence_length × input_dim]
 * @param sequence_length Length of sequences
 * @param output Output buffer [sequence_length × output_dim]
 * @param attention_weights Output attention weights [sequence_length × sequence_length] (optional)
 * @param pos_encoder Positional encoder for RoPE/ALiBi (optional, NULL = no PE)
 * @param head_idx Head index for ALiBi bias (only used if pos_encoder is ALiBi type)
 * @return true on success, false on error
 * PERFORMANCE: ~50μs for 128-dim, 32-length sequence (without PE)
 * THREAD_SAFETY: Not thread-safe, use separate heads per thread
 */
bool attention_head_forward(attention_head_t head,
                           const float* query,
                           const float* key,
                           const float* value,
                           uint32_t sequence_length,
                           float* output,
                           float* attention_weights,
                           nimcp_pos_encoder_t* pos_encoder,
                           uint32_t head_idx);

//=============================================================================
// Multihead Attention API (Cortical Column System)
//=============================================================================

/**
 * WHAT: Create multihead attention system
 * WHY:  Coordinate multiple parallel attention streams with gating
 * @param config System configuration
 * @return Multihead attention handle or NULL on failure
 * PERFORMANCE: ~100μs creation time, thread-safe
 */
multihead_attention_t multihead_attention_create(const multihead_attention_config_t* config);

/**
 * WHAT: Destroy multihead attention system
 * WHY:  Clean up all heads and resources
 * @param mha Multihead attention system
 * THREAD_SAFETY: Must not be called concurrently
 */
void multihead_attention_destroy(multihead_attention_t mha);

/**
 * WHAT: Forward pass through multihead attention
 * WHY:  Process input through all heads in parallel, apply gating, aggregate outputs
 * @param mha Multihead attention system
 * @param input Input sequence [sequence_length × input_dim]
 * @param sequence_length Length of input sequence
 * @param salience Salience scores per token (optional) [sequence_length]
 * @param output Output sequence [sequence_length × output_dim]
 * @return true on success, false on error
 * PERFORMANCE: ~200μs for 8 heads, 128-dim, 32-length sequence
 * THREAD_SAFETY: Not thread-safe, use separate systems per thread
 */
bool multihead_attention_forward(multihead_attention_t mha,
                                const float* input,
                                uint32_t sequence_length,
                                const float* salience,
                                float* output);

/**
 * WHAT: Update thalamic gate state
 * WHY:  Control top-down attention modulation (executive control)
 * @param mha Multihead attention system
 * @param gate_signal Gate control signal [0.0-1.0]
 * @return true on success
 * PERFORMANCE: ~1μs, lock-free
 */
bool multihead_attention_set_gate(multihead_attention_t mha, float gate_signal);

/**
 * WHAT: Get attention statistics
 * WHY:  Monitor system behavior and performance
 * @param mha Multihead attention system
 * @param stats Output statistics structure
 * @return true on success
 * THREAD_SAFETY: Thread-safe read operation
 */
bool multihead_attention_get_stats(multihead_attention_t mha, attention_stats_t* stats);

/**
 * WHAT: Reset attention statistics
 * WHY:  Clear accumulated statistics for new measurement period
 * @param mha Multihead attention system
 */
void multihead_attention_reset_stats(multihead_attention_t mha);

//=============================================================================
// Deferred Callback API
//=============================================================================

/**
 * WHAT: Attention event types for deferred callback notifications
 * WHY:  Enable post-forward-pass event notification without mutex contention
 */
typedef enum {
    ATTENTION_EVENT_FOCUS_SHIFT = 0,     /**< Attention focus changed significantly */
    ATTENTION_EVENT_GATE_CHANGE = 1,     /**< Thalamic gate state changed */
    ATTENTION_EVENT_ENTROPY_SPIKE = 2,   /**< Attention entropy exceeded threshold */
    ATTENTION_EVENT_HEAD_SATURATED = 3,  /**< Attention head reached saturation */
    ATTENTION_EVENT_COUNT = 4            /**< Number of event types */
} attention_event_type_t;

/**
 * WHAT: Callback function type for deferred attention events
 * WHY:  Allow external modules to react to attention state changes
 * @param event_type Type of attention event
 * @param head_index Index of the attention head that triggered the event
 * @param value Event-specific value (e.g., entropy, gate signal)
 * @param user_data User-provided context pointer
 */
typedef void (*attention_deferred_callback_t)(
    attention_event_type_t event_type,
    uint32_t head_index,
    float value,
    void* user_data
);

/**
 * WHAT: Maximum number of deferred callbacks in the buffer
 * WHY:  Bounded buffer prevents unbounded growth during high-frequency events
 *
 * NOTE: If more than ATTENTION_MAX_DEFERRED_CALLBACKS events are generated
 *       in a single forward pass, excess events are dropped with a warning.
 *       Use attention_get_deferred_drop_count() to monitor drops.
 */
#define ATTENTION_MAX_DEFERRED_CALLBACKS 48

/**
 * WHAT: Register a callback for deferred attention events
 * WHY:  Enable external modules to subscribe to attention state changes
 * @param mha Multihead attention system
 * @param callback Function to call when event occurs
 * @param user_data User context passed to callback
 * @return 0 on success, -1 on error (NULL params or max subscribers reached)
 *
 * THREAD_SAFETY: Not thread-safe, register before starting forward passes
 */
int multihead_attention_register_callback(multihead_attention_t mha,
                                          attention_deferred_callback_t callback,
                                          void* user_data);

/**
 * WHAT: Get number of deferred callbacks dropped due to buffer overflow
 * WHY:  Monitor if the deferred buffer size is sufficient
 * @param mha Multihead attention system
 * @return Number of dropped callbacks since last reset, or 0 on error
 *
 * THREAD_SAFETY: Thread-safe read operation
 */
uint64_t attention_get_deferred_drop_count(multihead_attention_t mha);

/**
 * WHAT: Reset the deferred callback drop counter
 * WHY:  Allow periodic monitoring of drop rate
 * @param mha Multihead attention system
 */
void attention_reset_deferred_drop_count(multihead_attention_t mha);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * WHAT: Compute attention entropy
 * WHY:  Measure how focused vs diffuse attention is
 * @param attention_weights Attention weight matrix [seq_len × seq_len]
 * @param sequence_length Length of sequence
 * @return Entropy value (lower = more focused, higher = more diffuse)
 * PERFORMANCE: ~5μs for 32-length sequence
 */
float attention_compute_entropy(const float* attention_weights, uint32_t sequence_length);

/**
 * WHAT: Validate attention configuration
 * WHY:  Catch configuration errors early
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 */
bool attention_validate_config(const multihead_attention_config_t* config);

/**
 * WHAT: Get average attention strength across all heads
 * WHY:  Enable attention-gated working memory (attended items more salient)
 * HOW:  Average gate activation across heads, bounded [0.0-1.0]
 *
 * @param mha Multihead attention system
 * @return Average attention strength [0.0-1.0], or 0.0 if NULL
 *
 * COMPLEXITY: O(1)
 * THREAD_SAFETY: Thread-safe read operation
 *
 * BIOLOGICAL BASIS:
 * - Attended stimuli have enhanced cortical representation
 * - Attention gates what enters working memory (PFC)
 * - Inattentional blindness: unattended items don't reach awareness
 *
 * USAGE:
 * ```c
 * float attention_strength = multihead_attention_get_strength(brain->multihead_attention);
 * float salience = base_salience + (attention_strength * 0.3f);  // Boost by up to 30%
 * working_memory_add(brain->working_memory, features, num_features, salience);
 * ```
 */
float multihead_attention_get_strength(multihead_attention_t mha);

//=============================================================================
// Positional Encoding Integration (Strategy Pattern)
//=============================================================================

/**
 * WHAT: Set positional encoding type for attention system
 * WHY:  Allow runtime switching between RoPE and ALiBi strategies
 * HOW:  Destroy old encoder, create new one with specified type
 *
 * @param mha Multihead attention system
 * @param pe_type Encoding type (NIMCP_POS_ROTARY or NIMCP_POS_ALIBI)
 * @return true on success, false on error
 *
 * PERFORMANCE: ~100μs (encoder creation overhead)
 * THREAD_SAFETY: Not thread-safe, call before forward passes
 *
 * BIOLOGICAL BASIS:
 * - Different brain regions use different spatial coding schemes
 * - Place cells: absolute position (analogous to learned PE)
 * - Grid cells: relative position (analogous to RoPE)
 * - Distance coding in parietal cortex (analogous to ALiBi)
 *
 * USAGE:
 * ```c
 * // Use RoPE for long-range dependencies
 * multihead_attention_set_pe_type(mha, NIMCP_POS_ROTARY);
 *
 * // Use ALiBi for efficiency on very long sequences
 * multihead_attention_set_pe_type(mha, NIMCP_POS_ALIBI);
 * ```
 */
bool multihead_attention_set_pe_type(multihead_attention_t mha,
                                     nimcp_pos_encoding_type_t pe_type);

/**
 * WHAT: Apply RoPE to query/key projections (internal helper)
 * WHY:  Inject relative position information into attention computation
 * HOW:  Rotate Q/K by position-dependent angles before scoring
 *
 * @param mha Multihead attention system (must have NIMCP_POS_ROTARY encoder)
 * @param query_proj Query projections [seq_len × key_dim]
 * @param key_proj Key projections [seq_len × key_dim]
 * @param seq_length Sequence length
 * @param key_dim Key dimension
 * @param query_out Rotated query output [seq_len × key_dim]
 * @param key_out Rotated key output [seq_len × key_dim]
 * @return true on success, false on error
 *
 * PERFORMANCE: ~O(seq_len × key_dim)
 * COMPLEXITY: Applied per-head in attention_head_forward()
 *
 * THEORETICAL BASIS:
 * - Su et al. (2021): RoFormer - Enhanced Transformer with Rotary Position Embedding
 * - Encodes relative position via rotation: q^T k = f(q, m) f(k, n)^T
 * - Rotation angle proportional to position difference (m - n)
 * - Maintains dot product structure while encoding position
 */
bool multihead_attention_apply_rope(multihead_attention_t mha,
                                   const float* query_proj,
                                   const float* key_proj,
                                   uint32_t seq_length,
                                   uint32_t key_dim,
                                   float* query_out,
                                   float* key_out);

/**
 * WHAT: Get ALiBi attention bias matrix
 * WHY:  Add position-dependent bias to attention scores
 * HOW:  Compute linear distance penalties: bias[i][j] = -slope * |i - j|
 *
 * @param mha Multihead attention system (must have NIMCP_POS_ALIBI encoder)
 * @param seq_length Sequence length
 * @param bias_out Output bias matrix [num_heads × seq_length × seq_length]
 * @return true on success, false on error
 *
 * PERFORMANCE: ~O(num_heads × seq_length^2)
 * USAGE: Add to attention scores before softmax
 *
 * THEORETICAL BASIS:
 * - Press et al. (2022): Train Short, Test Long - ALiBi
 * - Linear bias encourages nearby tokens: attention[i][j] += bias[i][j]
 * - Different slopes per head: slope[h] = 2^(-8(h+1)/num_heads)
 * - Extrapolates to longer sequences than training length
 *
 * BIOLOGICAL ANALOGY:
 * - Distance-dependent synaptic strength in cortex
 * - Lateral inhibition: nearby neurons compete, distant less so
 * - Temporal discounting in working memory
 */
bool multihead_attention_get_alibi_bias(multihead_attention_t mha,
                                       uint32_t seq_length,
                                       float* bias_out);

//=============================================================================
// Hard Ternary Attention API
//=============================================================================

#include "utils/ternary/nimcp_ternary_types.h"

/**
 * @brief Ternary attention states
 *
 * WHAT: Discrete attention states for hard attention mechanisms
 * WHY:  Enable interpretable, discrete attention decisions
 * HOW:  Map soft attention weights to {FOCUS, NEUTRAL, SUPPRESS}
 *
 * BIOLOGICAL BASIS:
 * - Winner-take-all circuits in cortex
 * - Lateral inhibition creates discrete selection
 * - Attentional blink: finite capacity attention
 */
#ifndef TERNARY_ATTENTION_STATE_DEFINED
#define TERNARY_ATTENTION_STATE_DEFINED
typedef enum {
    ATTENTION_SUPPRESS = -1,         /**< Actively suppress (inhibition) */
    ATTENTION_NEUTRAL = 0,           /**< Neutral, no modulation */
    ATTENTION_FOCUS = 1              /**< Active focus (enhancement) */
} ternary_attention_state_t;
#endif

/**
 * @brief Configuration for hard ternary attention
 */
typedef struct {
    float focus_threshold;           /**< Threshold for FOCUS state (default: 0.7) */
    float suppress_threshold;        /**< Threshold for SUPPRESS state (default: 0.3) */
    bool use_top_k;                  /**< Use top-k selection instead of threshold */
    uint32_t top_k;                  /**< Number of items to focus on */
    float focus_gain;                /**< Gain multiplier for focused items */
    float suppress_gain;             /**< Gain multiplier for suppressed items (< 1) */
    bool temperature_annealing;      /**< Use temperature annealing during training */
    float initial_temperature;       /**< Starting temperature (high = soft) */
    float final_temperature;         /**< Ending temperature (low = hard) */
    uint32_t annealing_steps;        /**< Steps over which to anneal */
} ternary_attention_config_t;

/**
 * @brief Ternary attention statistics
 */
typedef struct {
    uint64_t n_focus;                /**< Count of FOCUS decisions */
    uint64_t n_neutral;              /**< Count of NEUTRAL decisions */
    uint64_t n_suppress;             /**< Count of SUPPRESS decisions */
    float focus_ratio;               /**< Fraction of focused items */
    float suppress_ratio;            /**< Fraction of suppressed items */
    float sparsity;                  /**< Attention sparsity (1 - focus_ratio) */
    float avg_temperature;           /**< Average temperature used */
} ternary_attention_stats_t;

/**
 * @brief Ternary attention context (opaque)
 */
typedef struct ternary_attention_ctx_s ternary_attention_ctx_t;

/**
 * @brief Get default ternary attention configuration
 *
 * DEFAULTS:
 * - focus_threshold: 0.7
 * - suppress_threshold: 0.3
 * - use_top_k: false
 * - focus_gain: 1.5
 * - suppress_gain: 0.1
 *
 * @param config Configuration to initialize
 * @return 0 on success, negative on error
 */
int ternary_attention_default_config(ternary_attention_config_t* config);

/**
 * @brief Create ternary attention context
 *
 * @param config Configuration
 * @return Context or NULL on failure
 */
ternary_attention_ctx_t* ternary_attention_create(
    const ternary_attention_config_t* config
);

/**
 * @brief Destroy ternary attention context
 *
 * @param ctx Context to destroy
 */
void ternary_attention_destroy(ternary_attention_ctx_t* ctx);

/**
 * @brief Convert soft attention weights to hard ternary states
 *
 * WHAT: Discretize continuous attention to {FOCUS, NEUTRAL, SUPPRESS}
 * WHY:  Enable discrete, interpretable attention decisions
 * HOW:
 *   - attention > focus_threshold => FOCUS
 *   - attention < suppress_threshold => SUPPRESS
 *   - otherwise => NEUTRAL
 *
 * COMPARISON (standard hard attention):
 * - Luong et al. (2015): Local vs Global attention
 * - Xu et al. (2015): Hard attention via reinforcement learning
 * - NIMCP: Ternary hard attention with three states
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex biases visual cortex via gain modulation
 * - Three states match excitation/neutral/inhibition in neural circuits
 *
 * @param ctx Ternary attention context
 * @param soft_attention Input soft attention weights [seq_length]
 * @param seq_length Sequence length
 * @param ternary_out Output ternary states [seq_length]
 * @return 0 on success, negative on error
 */
int ternary_attention_discretize(
    ternary_attention_ctx_t* ctx,
    const float* soft_attention,
    uint32_t seq_length,
    ternary_attention_state_t* ternary_out
);

/**
 * @brief Apply ternary attention modulation to values
 *
 * WHAT: Modulate values based on ternary attention states
 * WHY:  Apply discrete attention decisions to data
 * HOW:
 *   - FOCUS: value *= focus_gain
 *   - NEUTRAL: value unchanged
 *   - SUPPRESS: value *= suppress_gain (near zero)
 *
 * @param ctx Ternary attention context
 * @param values Input values [seq_length x dim]
 * @param ternary_attention Ternary attention states [seq_length]
 * @param seq_length Sequence length
 * @param dim Feature dimension
 * @param output Output modulated values [seq_length x dim]
 * @return 0 on success, negative on error
 */
int ternary_attention_apply(
    ternary_attention_ctx_t* ctx,
    const float* values,
    const ternary_attention_state_t* ternary_attention,
    uint32_t seq_length,
    uint32_t dim,
    float* output
);

/**
 * @brief Compute straight-through gradient for ternary attention
 *
 * WHAT: Gradient estimator for discrete attention decisions
 * WHY:  Enable backprop through hard attention
 * HOW:  Use straight-through estimator (pass gradient through unchanged)
 *
 * @param ctx Ternary attention context
 * @param grad_output Upstream gradient [seq_length x dim]
 * @param soft_attention Original soft attention [seq_length]
 * @param seq_length Sequence length
 * @param dim Feature dimension
 * @param grad_attention Output gradient for attention weights [seq_length]
 * @return 0 on success, negative on error
 */
int ternary_attention_backward(
    ternary_attention_ctx_t* ctx,
    const float* grad_output,
    const float* soft_attention,
    uint32_t seq_length,
    uint32_t dim,
    float* grad_attention
);

/**
 * @brief Update temperature for annealing schedule
 *
 * WHAT: Decrease temperature over training to harden attention
 * WHY:  Start soft for exploration, end hard for discrete decisions
 * HOW:  Linear or exponential decay from initial to final temperature
 *
 * @param ctx Ternary attention context
 * @param step Current training step
 */
void ternary_attention_update_temperature(
    ternary_attention_ctx_t* ctx,
    uint32_t step
);

/**
 * @brief Get current temperature
 *
 * @param ctx Ternary attention context
 * @return Current temperature value
 */
float ternary_attention_get_temperature(const ternary_attention_ctx_t* ctx);

/**
 * @brief Get ternary attention statistics
 *
 * @param ctx Ternary attention context
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int ternary_attention_get_stats(
    const ternary_attention_ctx_t* ctx,
    ternary_attention_stats_t* stats
);

/**
 * @brief Sparse top-k ternary attention
 *
 * WHAT: Select top-k items to focus on, suppress others
 * WHY:  Enforce attention sparsity for interpretability
 * HOW:
 *   - Sort by attention weights
 *   - Top k get FOCUS
 *   - Bottom n-k get SUPPRESS (optionally some NEUTRAL)
 *
 * @param soft_attention Input soft attention [seq_length]
 * @param seq_length Sequence length
 * @param k Number of items to focus on
 * @param ternary_out Output ternary states [seq_length]
 * @return 0 on success, negative on error
 */
int ternary_attention_top_k(
    const float* soft_attention,
    uint32_t seq_length,
    uint32_t k,
    ternary_attention_state_t* ternary_out
);

/**
 * @brief Convert ternary attention state to gain multiplier
 *
 * @param state Ternary attention state
 * @param config Attention configuration
 * @return Gain multiplier
 */
static inline float ternary_attention_state_to_gain(
    ternary_attention_state_t state,
    const ternary_attention_config_t* config
) {
    switch (state) {
        case ATTENTION_FOCUS:    return config ? config->focus_gain : 1.5f;
        case ATTENTION_SUPPRESS: return config ? config->suppress_gain : 0.1f;
        case ATTENTION_NEUTRAL:
        default:                 return 1.0f;
    }
}

/**
 * @brief Get string name for ternary attention state
 *
 * @param state Attention state
 * @return String name
 */
static inline const char* ternary_attention_state_name(ternary_attention_state_t state) {
    switch (state) {
        case ATTENTION_SUPPRESS: return "SUPPRESS";
        case ATTENTION_NEUTRAL:  return "NEUTRAL";
        case ATTENTION_FOCUS:    return "FOCUS";
        default:                 return "INVALID";
    }
}

#ifdef __cplusplus
}
#endif

#endif // NIMCP_ATTENTION_H
