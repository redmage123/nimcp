/**
 * @file nimcp_executive_integration_bridge.h
 * @brief Executive-Integration Inter-Layer Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Bridges executive functions to global integration layer
 * WHY:  Executive decisions must integrate with consciousness
 * HOW:  Routes decisions to global workspace, receives arousal state
 *
 * KEY PATHWAYS:
 * =============
 * Bottom-Up (Executive → Integration):
 * - Decisions → conscious awareness
 * - Goals → global broadcast
 * - Cognitive conflict → arousal increase
 *
 * Top-Down (Integration → Executive):
 * - Global state → executive priority
 * - Arousal → cognitive capacity
 * - Binding → decision coherence
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_EXECUTIVE_INTEGRATION_BRIDGE_H
#define NIMCP_EXECUTIVE_INTEGRATION_BRIDGE_H

#include "integration/core/nimcp_layer_types.h"
#include "integration/core/nimcp_layer_registry.h"
#include "integration/intra/executive/nimcp_executive_intra_coordinator.h"
#include "integration/intra/integration_layer/nimcp_integration_intra_coordinator.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bridge message types */
#define EXEC_INTEG_MSG_DECISION_AWARE   (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x00A0)
#define EXEC_INTEG_MSG_GOAL_BROADCAST   (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x00A1)
#define EXEC_INTEG_MSG_CONFLICT_AROUSAL (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x00A2)
#define EXEC_INTEG_MSG_EXEC_PRIORITY    (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x00A3)
#define EXEC_INTEG_MSG_COG_CAPACITY     (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x00A4)
#define EXEC_INTEG_MSG_DECISION_COHER   (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x00A5)

typedef struct nimcp_executive_integration_bridge_struct* nimcp_executive_integration_bridge_t;

typedef struct {
    float decision_awareness_coupling;
    float goal_broadcast_strength;
    float conflict_arousal_gain;
    float exec_priority_coupling;
    float cognitive_capacity_coupling;
    float decision_coherence_coupling;
    uint32_t update_interval_ms;
    bool enable_conscious_control;
    bool enable_logging;
    bool enable_metrics;
} nimcp_executive_integration_config_t;

typedef struct {
    float decision_awareness;
    float goal_broadcast_level;
    float conflict_arousal;
    float executive_priority;
    float cognitive_capacity;
    float decision_coherence;
    float bridge_coherence;
    uint64_t bottom_up_messages;
    uint64_t top_down_messages;
} nimcp_executive_integration_state_t;

typedef struct {
    uint64_t decision_awarenesses;
    uint64_t goal_broadcasts;
    uint64_t conflict_arousals;
    uint64_t exec_priorities;
    uint64_t capacity_updates;
    uint64_t coherence_updates;
    float avg_decision_awareness;
    float avg_cognitive_capacity;
} nimcp_executive_integration_stats_t;

NIMCP_EXPORT nimcp_executive_integration_config_t nimcp_executive_integration_default_config(void);
NIMCP_EXPORT nimcp_executive_integration_bridge_t nimcp_executive_integration_create(const nimcp_executive_integration_config_t* config);
NIMCP_EXPORT void nimcp_executive_integration_destroy(nimcp_executive_integration_bridge_t bridge);
NIMCP_EXPORT nimcp_layer_error_t nimcp_executive_integration_init(
    nimcp_executive_integration_bridge_t bridge,
    nimcp_layer_registry_t registry,
    nimcp_executive_intra_t executive,
    nimcp_integration_intra_t integration
);
NIMCP_EXPORT nimcp_layer_error_t nimcp_executive_integration_shutdown(nimcp_executive_integration_bridge_t bridge);

NIMCP_EXPORT nimcp_layer_error_t nimcp_executive_integration_update(nimcp_executive_integration_bridge_t bridge, float dt);
NIMCP_EXPORT nimcp_layer_error_t nimcp_executive_integration_transfer_bottom_up(nimcp_executive_integration_bridge_t bridge, const nimcp_layer_msg_t* msg);
NIMCP_EXPORT nimcp_layer_error_t nimcp_executive_integration_transfer_top_down(nimcp_executive_integration_bridge_t bridge, const nimcp_layer_msg_t* msg);

NIMCP_EXPORT nimcp_layer_error_t nimcp_executive_integration_get_state(nimcp_executive_integration_bridge_t bridge, nimcp_executive_integration_state_t* state_out);
NIMCP_EXPORT nimcp_layer_error_t nimcp_executive_integration_get_stats(nimcp_executive_integration_bridge_t bridge, nimcp_executive_integration_stats_t* stats_out);
NIMCP_EXPORT float nimcp_executive_integration_get_coherence(nimcp_executive_integration_bridge_t bridge);
NIMCP_EXPORT nimcp_layer_error_t nimcp_executive_integration_reset_stats(nimcp_executive_integration_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EXECUTIVE_INTEGRATION_BRIDGE_H */
