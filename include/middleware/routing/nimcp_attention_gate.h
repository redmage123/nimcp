//=============================================================================
// nimcp_attention_gate.h - Attention Mechanism for Routing
//=============================================================================

#ifndef NIMCP_ATTENTION_GATE_H
#define NIMCP_ATTENTION_GATE_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/ternary/nimcp_ternary.h"  // Ternary attention mode

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nimcp_attention_gate.h
 * @brief Attention-based signal modulation
 *
 * WHAT: Top-down and bottom-up attention control for routing
 * WHY:  Attention selectively amplifies task-relevant information
 * HOW:  Winner-take-all competition, attention spotlight, salience integration
 *
 * BIOLOGICAL BASIS:
 * - Frontal eye fields (FEF) / Dorsolateral prefrontal cortex (DLPFC): top-down attention
 * - Temporo-parietal junction (TPJ): bottom-up salience detection
 * - Winner-take-all via mutual inhibition (Desimone & Duncan, 1995)
 * - Attention spotlight with limited capacity (Treisman & Gelade, 1980)
 * - Gain modulation of neural responses (Reynolds & Heeger, 2009)
 *
 * ALGORITHMS:
 * - Top-down attention: explicit weight setting [0, 1]
 * - Bottom-up salience: automatic weight based on signal novelty/intensity
 * - Winner-take-all: competitive inhibition, winner gets full attention
 * - Attention spotlight: focus on subset, suppres others
 * - Shift detection: track attention transitions for analysis
 */

// ============================================================================
// CONSTANTS
// ============================================================================

#define ATTENTION_MAX_TARGETS 256        // Maximum attended targets
#define ATTENTION_SPOTLIGHT_SIZE 8       // Focus on top N targets
#define ATTENTION_SHIFT_THRESHOLD 0.3f   // Minimum change to count as shift

// ============================================================================
// TYPES
// ============================================================================

/**
 * @brief Attention mode
 */
typedef enum {
    ATTENTION_MODE_TOPDOWN,      // Explicit attention control
    ATTENTION_MODE_BOTTOMUP,     // Salience-driven attention
    ATTENTION_MODE_MIXED,        // Combination of both
    ATTENTION_MODE_TERNARY       // NIMCP 2.10: Discrete ternary attention
} attention_mode_t;

/**
 * @brief Ternary attention state values for gate targets (NIMCP 2.10)
 *
 * WHAT: Discrete attention states for efficient gating
 * WHY:  Many attention decisions are naturally ternary (suppress/neutral/focus)
 * HOW:  Map continuous attention to {-1, 0, +1}
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex often gates information in binary/ternary fashion
 * - Pulvinar nucleus provides discrete attention signals
 * - Attentional blink suggests discrete attention windows
 *
 * STATES:
 * - SUPPRESS (-1): Actively inhibit this target (attention away)
 * - NEUTRAL (0): No attention modulation (baseline)
 * - FOCUS (+1): Actively enhance this target (attention toward)
 *
 * NOTE: Uses trit_t values from ternary types for consistency:
 * - TRIT_NEGATIVE (-1) = SUPPRESS
 * - TRIT_UNKNOWN (0) = NEUTRAL
 * - TRIT_POSITIVE (+1) = FOCUS
 */
#define TERNARY_ATTENTION_SUPPRESS TRIT_NEGATIVE
#define TERNARY_ATTENTION_NEUTRAL TRIT_UNKNOWN
#define TERNARY_ATTENTION_FOCUS TRIT_POSITIVE

/**
 * @brief Attention target
 */
typedef struct {
    uint32_t target_id;          // Target identifier
    float topdown_weight;        // Top-down attention [0, 1]
    float bottomup_salience;     // Bottom-up salience [0, 1]
    float combined_weight;       // Final attention weight [0, 1]
    bool in_spotlight;           // Within attention focus
    uint64_t last_update_ms;     // Last weight update time

    // NIMCP 2.10: Ternary attention support
    trit_t ternary_state;        // Discrete attention {SUPPRESS, NEUTRAL, FOCUS}
    bool use_ternary;            // Use ternary instead of continuous
} attention_target_t;

/**
 * @brief Attention shift event
 */
typedef struct {
    uint32_t from_target;        // Previous focus
    uint32_t to_target;          // New focus
    double shift_time_ms;        // When shift occurred
    float shift_magnitude;       // Weight change magnitude
} attention_shift_t;

/**
 * @brief Attention gate configuration
 */
typedef struct {
    attention_mode_t mode;               // Attention mode
    uint32_t max_targets;                // Maximum targets
    uint32_t spotlight_size;             // Focus capacity
    bool enable_winner_take_all;         // WTA competition
    bool enable_shift_detection;         // Track attention shifts
    float topdown_weight;                // Top-down influence [0, 1]
    float bottomup_weight;               // Bottom-up influence [0, 1]
    float inhibition_strength;           // Lateral inhibition strength

    // NIMCP 2.10: Ternary attention configuration
    bool enable_ternary_mode;            // Enable ternary attention mode
    float ternary_focus_threshold;       // Threshold for FOCUS state (default 0.7)
    float ternary_suppress_threshold;    // Threshold for SUPPRESS state (default 0.3)
    float ternary_focus_gain;            // Gain multiplier for focused targets (default 2.0)
    float ternary_suppress_gain;         // Gain multiplier for suppressed targets (default 0.2)
} attention_gate_config_t;

/**
 * @brief Opaque attention gate handle
 */
typedef struct attention_gate attention_gate_t;

// ============================================================================
// API
// ============================================================================

/**
 * @brief Create attention gate with configuration
 *
 * WHAT: Initialize attention control system
 * WHY:  Set up attention weights and competition mechanism
 * HOW:  Allocate target storage, initialize weights
 *
 * @param config Gate configuration (NULL for defaults)
 * @return Gate handle or NULL on failure
 */
attention_gate_t* attention_gate_create(const attention_gate_config_t* config);

/**
 * @brief Destroy attention gate and free resources
 */
void attention_gate_destroy(attention_gate_t* gate);

/**
 * @brief Set top-down attention weight
 *
 * WHAT: Explicitly control attention to target
 * WHY:  Implement task-driven attention allocation
 * HOW:  Set weight, update combined weight, apply WTA if enabled
 *
 * @param gate Gate handle
 * @param source_id Source identifier (for routing context)
 * @param target_id Target identifier
 * @param weight Attention weight [0.0, 1.0]
 * @return true on success, false on error
 */
bool attention_gate_set_weight(attention_gate_t* gate,
                                uint32_t source_id,
                                uint32_t target_id,
                                float weight);

/**
 * @brief Get combined attention weight
 *
 * @param gate Gate handle
 * @param source_id Source identifier
 * @param target_id Target identifier
 * @param weight Output: attention weight
 * @return true on success, false if target not found
 */
bool attention_gate_get_weight(const attention_gate_t* gate,
                                uint32_t source_id,
                                uint32_t target_id,
                                float* weight);

/**
 * @brief Update bottom-up salience
 *
 * WHAT: Set salience-driven attention component
 * WHY:  Capture automatically from signal properties
 * HOW:  Update salience, recompute combined weight
 *
 * @param gate Gate handle
 * @param target_id Target identifier
 * @param salience Salience value [0.0, 1.0]
 * @return true on success, false on error
 */
bool attention_gate_update_salience(attention_gate_t* gate,
                                     uint32_t target_id,
                                     float salience);

/**
 * @brief Apply winner-take-all competition
 *
 * WHAT: Select single highest-weighted target
 * WHY:  Model limited attention capacity
 * HOW:  Find max weight, set to 1.0, suppress others
 *
 * @param gate Gate handle
 * @param winner_id Output: winning target ID (can be NULL)
 * @return true on success, false on error
 */
bool attention_gate_apply_wta(attention_gate_t* gate, uint32_t* winner_id);

/**
 * @brief Update attention spotlight
 *
 * WHAT: Select top-N targets for focus
 * WHY:  Model limited but flexible attention capacity
 * HOW:  Sort by weight, mark top-N as in spotlight
 *
 * @param gate Gate handle
 * @param spotlight_ids Output: IDs in spotlight (optional)
 * @param num_in_spotlight Output: number in spotlight (optional)
 * @return true on success, false on error
 */
bool attention_gate_update_spotlight(attention_gate_t* gate,
                                      uint32_t* spotlight_ids,
                                      uint32_t* num_in_spotlight);

/**
 * @brief Get recent attention shifts
 *
 * @param gate Gate handle
 * @param shifts Output array for shifts
 * @param max_shifts Maximum shifts to return
 * @param num_shifts Output: number of shifts returned
 * @return true on success, false on error
 */
bool attention_gate_get_shifts(const attention_gate_t* gate,
                                attention_shift_t* shifts,
                                uint32_t max_shifts,
                                uint32_t* num_shifts);

/**
 * @brief Reset all attention weights
 *
 * WHAT: Clear all attention state
 * WHY:  Start fresh after task change
 * HOW:  Zero all weights, clear spotlight
 */
void attention_gate_reset(attention_gate_t* gate);

/**
 * @brief Get gate statistics
 *
 * @param gate Gate handle
 * @param num_targets Output: number of active targets
 * @param num_in_spotlight Output: targets in spotlight
 * @param total_shifts Output: lifetime attention shifts
 * @return true on success, false on error
 */
bool attention_gate_get_stats(const attention_gate_t* gate,
                               uint32_t* num_targets,
                               uint32_t* num_in_spotlight,
                               uint64_t* total_shifts);

/**
 * @brief Get default configuration
 */
attention_gate_config_t attention_gate_default_config(void);

//=============================================================================
// Ternary Attention API (NIMCP 2.10)
//=============================================================================

/**
 * @brief Set ternary attention state for target
 *
 * WHAT: Directly set discrete attention state
 * WHY:  Efficient attention control with discrete states
 * HOW:  Set ternary_state, enable use_ternary flag
 *
 * @param gate Gate handle
 * @param source_id Source identifier
 * @param target_id Target identifier
 * @param state Ternary attention state {SUPPRESS, NEUTRAL, FOCUS}
 * @return true on success, false on error
 */
bool attention_gate_set_ternary_state(attention_gate_t* gate,
                                       uint32_t source_id,
                                       uint32_t target_id,
                                       trit_t state);

/**
 * @brief Get ternary attention state for target
 *
 * WHAT: Retrieve discrete attention state
 * WHY:  Query current ternary attention
 * HOW:  Return ternary_state from target
 *
 * @param gate Gate handle
 * @param source_id Source identifier
 * @param target_id Target identifier
 * @param state Output: ternary attention state
 * @return true on success, false if target not found
 */
bool attention_gate_get_ternary_state(const attention_gate_t* gate,
                                       uint32_t source_id,
                                       uint32_t target_id,
                                       trit_t* state);

/**
 * @brief Convert all targets to ternary attention mode
 *
 * WHAT: Batch convert continuous attention to ternary
 * WHY:  Switch entire gate to discrete mode
 * HOW:  Quantize combined_weight using thresholds
 *
 * @param gate Gate handle
 * @return Number of targets converted
 */
uint32_t attention_gate_convert_to_ternary(attention_gate_t* gate);

/**
 * @brief Convert all targets back to continuous attention mode
 *
 * WHAT: Batch convert ternary attention to continuous
 * WHY:  Switch back to fine-grained control
 * HOW:  Map ternary states to weight values
 *
 * @param gate Gate handle
 * @return Number of targets converted
 */
uint32_t attention_gate_convert_to_continuous(attention_gate_t* gate);

/**
 * @brief Apply ternary attention gain modulation
 *
 * WHAT: Modulate input signal based on ternary attention
 * WHY:  Efficient attention-based gating
 * HOW:  Multiply by gain based on ternary state
 *
 * @param gate Gate handle
 * @param source_id Source identifier
 * @param target_id Target identifier
 * @param input Input signal value
 * @return Modulated output signal
 *
 * ALGORITHM:
 * - SUPPRESS: output = input * suppress_gain (default 0.2)
 * - NEUTRAL: output = input * 1.0 (no change)
 * - FOCUS: output = input * focus_gain (default 2.0)
 */
float attention_gate_apply_ternary_modulation(const attention_gate_t* gate,
                                               uint32_t source_id,
                                               uint32_t target_id,
                                               float input);

/**
 * @brief Get ternary attention distribution
 *
 * WHAT: Count targets in each ternary state
 * WHY:  Analyze attention allocation
 * HOW:  Count SUPPRESS, NEUTRAL, FOCUS targets
 *
 * @param gate Gate handle
 * @param n_suppress Output: count of suppressed targets
 * @param n_neutral Output: count of neutral targets
 * @param n_focus Output: count of focused targets
 */
void attention_gate_get_ternary_distribution(const attention_gate_t* gate,
                                              uint32_t* n_suppress,
                                              uint32_t* n_neutral,
                                              uint32_t* n_focus);

/**
 * @brief Update ternary states from continuous weights
 *
 * WHAT: Synchronize ternary states with continuous weights
 * WHY:  Keep ternary states consistent after weight updates
 * HOW:  Requantize using config thresholds
 *
 * @param gate Gate handle
 */
void attention_gate_update_ternary_states(attention_gate_t* gate);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_ATTENTION_GATE_H
