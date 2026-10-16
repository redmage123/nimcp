/**
 * @file nimcp_neuromod_executive_bridge.h
 * @brief Neuromodulatory-Executive Inter-Layer Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Bridges neuromodulatory state to executive functions
 * WHY:  Executive control depends on optimal arousal/motivation
 * HOW:  DA/NE levels modulate PFC function and decision-making
 *
 * KEY PATHWAYS:
 * =============
 * Bottom-Up (Neuromodulatory → Executive):
 * - DA levels → reward prediction, motivation
 * - NE levels → cognitive flexibility, task switching
 * - 5-HT levels → impulse control, patience
 *
 * Top-Down (Executive → Neuromodulatory):
 * - Goal achievement → DA reward signal
 * - Cognitive effort → NE sustained release
 * - Decision conflict → 5-HT modulation request
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NEUROMOD_EXECUTIVE_BRIDGE_H
#define NIMCP_NEUROMOD_EXECUTIVE_BRIDGE_H

#include "integration/core/nimcp_layer_types.h"
#include "integration/core/nimcp_layer_registry.h"
#include "integration/intra/neuromodulatory/nimcp_neuromod_intra_coordinator.h"
#include "integration/intra/executive/nimcp_executive_intra_coordinator.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bridge message types */
#define NEURO_EXEC_MSG_MOTIVATION       (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0050)
#define NEURO_EXEC_MSG_FLEXIBILITY      (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0051)
#define NEURO_EXEC_MSG_IMPULSE_CTRL     (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0052)
#define NEURO_EXEC_MSG_GOAL_REWARD      (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0053)
#define NEURO_EXEC_MSG_EFFORT_SIGNAL    (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0054)
#define NEURO_EXEC_MSG_CONFLICT         (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0055)

typedef struct nimcp_neuromod_executive_bridge_struct* nimcp_neuromod_executive_bridge_t;

typedef struct {
    float da_motivation_coupling;
    float ne_flexibility_coupling;
    float serotonin_impulse_coupling;
    float goal_reward_gain;
    float effort_ne_gain;
    float conflict_serotonin_gain;
    uint32_t update_interval_ms;
    bool enable_optimal_arousal;
    bool enable_logging;
    bool enable_metrics;
} nimcp_neuromod_executive_config_t;

typedef struct {
    float motivation_level;
    float cognitive_flexibility;
    float impulse_control;
    float goal_reward_signal;
    float effort_signal;
    float conflict_level;
    float bridge_coherence;
    uint64_t bottom_up_messages;
    uint64_t top_down_messages;
} nimcp_neuromod_executive_state_t;

typedef struct {
    uint64_t motivation_updates;
    uint64_t flexibility_changes;
    uint64_t impulse_control_events;
    uint64_t goal_rewards;
    uint64_t effort_signals;
    uint64_t conflict_events;
    float avg_motivation;
    float avg_flexibility;
} nimcp_neuromod_executive_stats_t;

NIMCP_EXPORT nimcp_neuromod_executive_config_t nimcp_neuromod_executive_default_config(void);
NIMCP_EXPORT nimcp_neuromod_executive_bridge_t nimcp_neuromod_executive_create(const nimcp_neuromod_executive_config_t* config);
NIMCP_EXPORT void nimcp_neuromod_executive_destroy(nimcp_neuromod_executive_bridge_t bridge);
NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_executive_init(
    nimcp_neuromod_executive_bridge_t bridge,
    nimcp_layer_registry_t registry,
    nimcp_neuromod_intra_t neuromod,
    nimcp_executive_intra_t executive
);
NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_executive_shutdown(nimcp_neuromod_executive_bridge_t bridge);

NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_executive_update(nimcp_neuromod_executive_bridge_t bridge, float dt);
NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_executive_transfer_bottom_up(nimcp_neuromod_executive_bridge_t bridge, const nimcp_layer_msg_t* msg);
NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_executive_transfer_top_down(nimcp_neuromod_executive_bridge_t bridge, const nimcp_layer_msg_t* msg);

NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_executive_get_state(nimcp_neuromod_executive_bridge_t bridge, nimcp_neuromod_executive_state_t* state_out);
NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_executive_get_stats(nimcp_neuromod_executive_bridge_t bridge, nimcp_neuromod_executive_stats_t* stats_out);
NIMCP_EXPORT float nimcp_neuromod_executive_get_coherence(nimcp_neuromod_executive_bridge_t bridge);
NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_executive_reset_stats(nimcp_neuromod_executive_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEUROMOD_EXECUTIVE_BRIDGE_H */
