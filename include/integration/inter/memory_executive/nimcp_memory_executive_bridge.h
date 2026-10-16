/**
 * @file nimcp_memory_executive_bridge.h
 * @brief Memory-Executive Inter-Layer Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Bridges memory systems to executive functions
 * WHY:  Decision-making relies on past experience and working memory
 * HOW:  Retrieves relevant memories for decisions, encodes outcomes
 *
 * KEY PATHWAYS:
 * =============
 * Bottom-Up (Memory → Executive):
 * - Relevant memories → decision context
 * - Episodic recall → value estimation
 * - Semantic knowledge → rule application
 *
 * Top-Down (Executive → Memory):
 * - Working memory → rehearsal
 * - Goal state → memory retrieval cues
 * - Decision outcomes → learning signal
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_MEMORY_EXECUTIVE_BRIDGE_H
#define NIMCP_MEMORY_EXECUTIVE_BRIDGE_H

#include "integration/core/nimcp_layer_types.h"
#include "integration/core/nimcp_layer_registry.h"
#include "integration/intra/memory/nimcp_memory_intra_coordinator.h"
#include "integration/intra/executive/nimcp_executive_intra_coordinator.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bridge message types */
#define MEM_EXEC_MSG_DECISION_CONTEXT   (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0080)
#define MEM_EXEC_MSG_VALUE_ESTIMATE     (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0081)
#define MEM_EXEC_MSG_RULE_APPLICATION   (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0082)
#define MEM_EXEC_MSG_WM_REHEARSAL       (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0083)
#define MEM_EXEC_MSG_RETRIEVAL_CUE      (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0084)
#define MEM_EXEC_MSG_LEARNING_SIGNAL    (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0085)

typedef struct nimcp_memory_executive_bridge_struct* nimcp_memory_executive_bridge_t;

typedef struct {
    float context_retrieval_coupling;
    float value_estimation_gain;
    float rule_application_coupling;
    float wm_rehearsal_strength;
    float retrieval_cue_coupling;
    float learning_signal_gain;
    uint32_t update_interval_ms;
    bool enable_prospective_memory;
    bool enable_logging;
    bool enable_metrics;
} nimcp_memory_executive_config_t;

typedef struct {
    float decision_context_strength;
    float value_estimate;
    float rule_activation;
    float wm_rehearsal_level;
    float retrieval_cue_strength;
    float learning_signal;
    float bridge_coherence;
    uint64_t bottom_up_messages;
    uint64_t top_down_messages;
} nimcp_memory_executive_state_t;

typedef struct {
    uint64_t context_retrievals;
    uint64_t value_estimations;
    uint64_t rule_applications;
    uint64_t wm_rehearsals;
    uint64_t retrieval_cues;
    uint64_t learning_signals;
    float avg_context_strength;
    float avg_value_accuracy;
} nimcp_memory_executive_stats_t;

NIMCP_EXPORT nimcp_memory_executive_config_t nimcp_memory_executive_default_config(void);
NIMCP_EXPORT nimcp_memory_executive_bridge_t nimcp_memory_executive_create(const nimcp_memory_executive_config_t* config);
NIMCP_EXPORT void nimcp_memory_executive_destroy(nimcp_memory_executive_bridge_t bridge);
NIMCP_EXPORT nimcp_layer_error_t nimcp_memory_executive_init(
    nimcp_memory_executive_bridge_t bridge,
    nimcp_layer_registry_t registry,
    nimcp_memory_intra_t memory,
    nimcp_executive_intra_t executive
);
NIMCP_EXPORT nimcp_layer_error_t nimcp_memory_executive_shutdown(nimcp_memory_executive_bridge_t bridge);

NIMCP_EXPORT nimcp_layer_error_t nimcp_memory_executive_update(nimcp_memory_executive_bridge_t bridge, float dt);
NIMCP_EXPORT nimcp_layer_error_t nimcp_memory_executive_transfer_bottom_up(nimcp_memory_executive_bridge_t bridge, const nimcp_layer_msg_t* msg);
NIMCP_EXPORT nimcp_layer_error_t nimcp_memory_executive_transfer_top_down(nimcp_memory_executive_bridge_t bridge, const nimcp_layer_msg_t* msg);

NIMCP_EXPORT nimcp_layer_error_t nimcp_memory_executive_get_state(nimcp_memory_executive_bridge_t bridge, nimcp_memory_executive_state_t* state_out);
NIMCP_EXPORT nimcp_layer_error_t nimcp_memory_executive_get_stats(nimcp_memory_executive_bridge_t bridge, nimcp_memory_executive_stats_t* stats_out);
NIMCP_EXPORT float nimcp_memory_executive_get_coherence(nimcp_memory_executive_bridge_t bridge);
NIMCP_EXPORT nimcp_layer_error_t nimcp_memory_executive_reset_stats(nimcp_memory_executive_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MEMORY_EXECUTIVE_BRIDGE_H */
