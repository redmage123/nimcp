/**
 * @file nimcp_neuromod_memory_bridge.h
 * @brief Neuromodulatory-Memory Inter-Layer Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Bridges neuromodulatory state to memory systems
 * WHY:  Memory encoding depends on emotional/arousal state
 * HOW:  NE/DA/5-HT levels modulate memory consolidation
 *
 * KEY PATHWAYS:
 * =============
 * Bottom-Up (Neuromodulatory → Memory):
 * - NE levels → encoding enhancement (flashbulb memory)
 * - DA levels → reward-related memory tagging
 * - 5-HT levels → pattern separation/completion balance
 *
 * Top-Down (Memory → Neuromodulatory):
 * - Memory retrieval → emotional state activation
 * - Familiar stimuli → reduced NE response
 * - Memory mismatch → prediction error → DA signal
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NEUROMOD_MEMORY_BRIDGE_H
#define NIMCP_NEUROMOD_MEMORY_BRIDGE_H

#include "integration/core/nimcp_layer_types.h"
#include "integration/core/nimcp_layer_registry.h"
#include "integration/intra/neuromodulatory/nimcp_neuromod_intra_coordinator.h"
#include "integration/intra/memory/nimcp_memory_intra_coordinator.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bridge message types */
#define NEURO_MEM_MSG_ENCODE_ENHANCE    (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0040)
#define NEURO_MEM_MSG_REWARD_TAG        (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0041)
#define NEURO_MEM_MSG_PATTERN_BALANCE   (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0042)
#define NEURO_MEM_MSG_EMOTIONAL_STATE   (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0043)
#define NEURO_MEM_MSG_FAMILIAR_SIGNAL   (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0044)
#define NEURO_MEM_MSG_PREDICTION_ERROR  (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0045)

typedef struct nimcp_neuromod_memory_bridge_struct* nimcp_neuromod_memory_bridge_t;

typedef struct {
    float ne_encoding_coupling;
    float da_reward_tagging_gain;
    float serotonin_pattern_coupling;
    float emotional_retrieval_gain;
    float familiarity_suppression;
    float prediction_error_gain;
    uint32_t update_interval_ms;
    bool enable_emotional_memory;
    bool enable_logging;
    bool enable_metrics;
} nimcp_neuromod_memory_config_t;

typedef struct {
    float encoding_enhancement;
    float reward_tag_strength;
    float pattern_separation_bias;
    float emotional_activation;
    float familiarity_signal;
    float prediction_error;
    float bridge_coherence;
    uint64_t bottom_up_messages;
    uint64_t top_down_messages;
} nimcp_neuromod_memory_state_t;

typedef struct {
    uint64_t encoding_enhancements;
    uint64_t reward_taggings;
    uint64_t pattern_balance_events;
    uint64_t emotional_activations;
    uint64_t familiarity_signals;
    uint64_t prediction_errors;
    float avg_encoding_strength;
    float avg_emotional_activation;
} nimcp_neuromod_memory_stats_t;

NIMCP_EXPORT nimcp_neuromod_memory_config_t nimcp_neuromod_memory_default_config(void);
NIMCP_EXPORT nimcp_neuromod_memory_bridge_t nimcp_neuromod_memory_create(const nimcp_neuromod_memory_config_t* config);
NIMCP_EXPORT void nimcp_neuromod_memory_destroy(nimcp_neuromod_memory_bridge_t bridge);
NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_memory_init(
    nimcp_neuromod_memory_bridge_t bridge,
    nimcp_layer_registry_t registry,
    nimcp_neuromod_intra_t neuromod,
    nimcp_memory_intra_t memory
);
NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_memory_shutdown(nimcp_neuromod_memory_bridge_t bridge);

NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_memory_update(nimcp_neuromod_memory_bridge_t bridge, float dt);
NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_memory_transfer_bottom_up(nimcp_neuromod_memory_bridge_t bridge, const nimcp_layer_msg_t* msg);
NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_memory_transfer_top_down(nimcp_neuromod_memory_bridge_t bridge, const nimcp_layer_msg_t* msg);

NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_memory_get_state(nimcp_neuromod_memory_bridge_t bridge, nimcp_neuromod_memory_state_t* state_out);
NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_memory_get_stats(nimcp_neuromod_memory_bridge_t bridge, nimcp_neuromod_memory_stats_t* stats_out);
NIMCP_EXPORT float nimcp_neuromod_memory_get_coherence(nimcp_neuromod_memory_bridge_t bridge);
NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_memory_reset_stats(nimcp_neuromod_memory_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEUROMOD_MEMORY_BRIDGE_H */
