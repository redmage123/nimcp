/**
 * @file nimcp_memory_integration_bridge.h
 * @brief Memory-Integration Inter-Layer Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Bridges memory systems to global integration layer
 * WHY:  Memories must bind with consciousness and global state
 * HOW:  Routes memory content to claustrum for global binding
 *
 * KEY PATHWAYS:
 * =============
 * Bottom-Up (Memory → Integration):
 * - Memory retrieval → global workspace entry
 * - Episodic recall → conscious awareness
 * - Context state → arousal modulation
 *
 * Top-Down (Integration → Memory):
 * - Global binding → memory consolidation trigger
 * - Arousal state → encoding gate
 * - Consciousness focus → retrieval priority
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_MEMORY_INTEGRATION_BRIDGE_H
#define NIMCP_MEMORY_INTEGRATION_BRIDGE_H

#include "integration/core/nimcp_layer_types.h"
#include "integration/core/nimcp_layer_registry.h"
#include "integration/intra/memory/nimcp_memory_intra_coordinator.h"
#include "integration/intra/integration_layer/nimcp_integration_intra_coordinator.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bridge message types */
#define MEM_INTEG_MSG_GW_ENTRY          (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0090)
#define MEM_INTEG_MSG_CONSCIOUS_RECALL  (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0091)
#define MEM_INTEG_MSG_AROUSAL_MOD       (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0092)
#define MEM_INTEG_MSG_CONSOLIDATE_TRIG  (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0093)
#define MEM_INTEG_MSG_ENCODING_GATE     (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0094)
#define MEM_INTEG_MSG_RETRIEVAL_PRIORITY (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0095)

typedef struct nimcp_memory_integration_bridge_struct* nimcp_memory_integration_bridge_t;

typedef struct {
    float gw_entry_threshold;
    float conscious_recall_coupling;
    float arousal_modulation_gain;
    float consolidation_trigger_threshold;
    float encoding_gate_coupling;
    float retrieval_priority_coupling;
    uint32_t update_interval_ms;
    bool enable_conscious_access;
    bool enable_logging;
    bool enable_metrics;
} nimcp_memory_integration_config_t;

typedef struct {
    float gw_entry_level;
    float conscious_recall_strength;
    float arousal_modulation;
    float consolidation_trigger;
    float encoding_gate_level;
    float retrieval_priority;
    float bridge_coherence;
    uint64_t bottom_up_messages;
    uint64_t top_down_messages;
} nimcp_memory_integration_state_t;

typedef struct {
    uint64_t gw_entries;
    uint64_t conscious_recalls;
    uint64_t arousal_modulations;
    uint64_t consolidation_triggers;
    uint64_t encoding_gates;
    uint64_t retrieval_priorities;
    float avg_conscious_access;
    float avg_consolidation_rate;
} nimcp_memory_integration_stats_t;

NIMCP_EXPORT nimcp_memory_integration_config_t nimcp_memory_integration_default_config(void);
NIMCP_EXPORT nimcp_memory_integration_bridge_t nimcp_memory_integration_create(const nimcp_memory_integration_config_t* config);
NIMCP_EXPORT void nimcp_memory_integration_destroy(nimcp_memory_integration_bridge_t bridge);
NIMCP_EXPORT nimcp_layer_error_t nimcp_memory_integration_init(
    nimcp_memory_integration_bridge_t bridge,
    nimcp_layer_registry_t registry,
    nimcp_memory_intra_t memory,
    nimcp_integration_intra_t integration
);
NIMCP_EXPORT nimcp_layer_error_t nimcp_memory_integration_shutdown(nimcp_memory_integration_bridge_t bridge);

NIMCP_EXPORT nimcp_layer_error_t nimcp_memory_integration_update(nimcp_memory_integration_bridge_t bridge, float dt);
NIMCP_EXPORT nimcp_layer_error_t nimcp_memory_integration_transfer_bottom_up(nimcp_memory_integration_bridge_t bridge, const nimcp_layer_msg_t* msg);
NIMCP_EXPORT nimcp_layer_error_t nimcp_memory_integration_transfer_top_down(nimcp_memory_integration_bridge_t bridge, const nimcp_layer_msg_t* msg);

NIMCP_EXPORT nimcp_layer_error_t nimcp_memory_integration_get_state(nimcp_memory_integration_bridge_t bridge, nimcp_memory_integration_state_t* state_out);
NIMCP_EXPORT nimcp_layer_error_t nimcp_memory_integration_get_stats(nimcp_memory_integration_bridge_t bridge, nimcp_memory_integration_stats_t* stats_out);
NIMCP_EXPORT float nimcp_memory_integration_get_coherence(nimcp_memory_integration_bridge_t bridge);
NIMCP_EXPORT nimcp_layer_error_t nimcp_memory_integration_reset_stats(nimcp_memory_integration_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MEMORY_INTEGRATION_BRIDGE_H */
