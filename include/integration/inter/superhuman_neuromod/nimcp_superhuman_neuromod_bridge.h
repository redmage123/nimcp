/**
 * @file nimcp_superhuman_neuromod_bridge.h
 * @brief Superhuman-Neuromodulatory Inter-Layer Bridge (Feedback Loop)
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Feedback bridge from superhuman capabilities to neuromodulation
 * WHY:  Enhanced perception affects arousal and attention systems
 * HOW:  Superhuman percepts trigger neuromodulatory responses
 *
 * KEY PATHWAYS (FEEDBACK LOOP):
 * =============================
 * Superhuman → Neuromodulatory:
 * - Novel enhanced percepts → LC (NE) activation
 * - Threat detection (IR/UV) → stress response
 * - Time dilation → arousal adjustment
 * - Enhanced reward signals → VTA (DA) modulation
 *
 * Neuromodulatory → Superhuman:
 * - NE levels → enhancement sensitivity
 * - DA levels → capability reward learning
 * - Stress → time dilation trigger
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SUPERHUMAN_NEUROMOD_BRIDGE_H
#define NIMCP_SUPERHUMAN_NEUROMOD_BRIDGE_H

#include "integration/core/nimcp_layer_types.h"
#include "integration/core/nimcp_layer_registry.h"
#include "integration/intra/superhuman/nimcp_superhuman_intra_coordinator.h"
#include "integration/intra/neuromodulatory/nimcp_neuromod_intra_coordinator.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bridge message types (feedback) */
#define SUPER_NEURO_MSG_NOVEL_ENHANCE   (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x00C0)
#define SUPER_NEURO_MSG_THREAT_DETECT   (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x00C1)
#define SUPER_NEURO_MSG_TIME_AROUSAL    (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x00C2)
#define SUPER_NEURO_MSG_ENHANCE_REWARD  (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x00C3)
#define SUPER_NEURO_MSG_SENSITIVITY     (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x00C4)
#define SUPER_NEURO_MSG_CAP_LEARNING    (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x00C5)
#define SUPER_NEURO_MSG_STRESS_DILATE   (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x00C6)

typedef struct nimcp_superhuman_neuromod_bridge_struct* nimcp_superhuman_neuromod_bridge_t;

typedef struct {
    float novel_enhance_lc_gain;
    float threat_stress_gain;
    float time_arousal_coupling;
    float enhance_reward_da_gain;
    float ne_sensitivity_coupling;
    float da_cap_learning_coupling;
    float stress_dilation_threshold;
    uint32_t update_interval_ms;
    bool enable_feedback_loop;
    bool enable_logging;
    bool enable_metrics;
} nimcp_superhuman_neuromod_config_t;

typedef struct {
    float novel_enhancement_signal;
    float threat_detection_level;
    float time_arousal_adjustment;
    float enhancement_reward;
    float sensitivity_level;
    float capability_learning_signal;
    float stress_dilation_trigger;
    float bridge_coherence;
    uint64_t feedback_messages;
    uint64_t modulation_messages;
} nimcp_superhuman_neuromod_state_t;

typedef struct {
    uint64_t novel_enhancements;
    uint64_t threat_detections;
    uint64_t time_arousal_changes;
    uint64_t enhance_rewards;
    uint64_t sensitivity_updates;
    uint64_t cap_learning_events;
    uint64_t stress_dilations;
    float avg_enhancement_signal;
    float avg_sensitivity;
} nimcp_superhuman_neuromod_stats_t;

NIMCP_EXPORT nimcp_superhuman_neuromod_config_t nimcp_superhuman_neuromod_default_config(void);
NIMCP_EXPORT nimcp_superhuman_neuromod_bridge_t nimcp_superhuman_neuromod_create(const nimcp_superhuman_neuromod_config_t* config);
NIMCP_EXPORT void nimcp_superhuman_neuromod_destroy(nimcp_superhuman_neuromod_bridge_t bridge);
NIMCP_EXPORT nimcp_layer_error_t nimcp_superhuman_neuromod_init(
    nimcp_superhuman_neuromod_bridge_t bridge,
    nimcp_layer_registry_t registry,
    nimcp_superhuman_intra_t superhuman,
    nimcp_neuromod_intra_t neuromod
);
NIMCP_EXPORT nimcp_layer_error_t nimcp_superhuman_neuromod_shutdown(nimcp_superhuman_neuromod_bridge_t bridge);

NIMCP_EXPORT nimcp_layer_error_t nimcp_superhuman_neuromod_update(nimcp_superhuman_neuromod_bridge_t bridge, float dt);
NIMCP_EXPORT nimcp_layer_error_t nimcp_superhuman_neuromod_transfer_feedback(nimcp_superhuman_neuromod_bridge_t bridge, const nimcp_layer_msg_t* msg);
NIMCP_EXPORT nimcp_layer_error_t nimcp_superhuman_neuromod_transfer_modulation(nimcp_superhuman_neuromod_bridge_t bridge, const nimcp_layer_msg_t* msg);

NIMCP_EXPORT nimcp_layer_error_t nimcp_superhuman_neuromod_get_state(nimcp_superhuman_neuromod_bridge_t bridge, nimcp_superhuman_neuromod_state_t* state_out);
NIMCP_EXPORT nimcp_layer_error_t nimcp_superhuman_neuromod_get_stats(nimcp_superhuman_neuromod_bridge_t bridge, nimcp_superhuman_neuromod_stats_t* stats_out);
NIMCP_EXPORT float nimcp_superhuman_neuromod_get_coherence(nimcp_superhuman_neuromod_bridge_t bridge);
NIMCP_EXPORT nimcp_layer_error_t nimcp_superhuman_neuromod_reset_stats(nimcp_superhuman_neuromod_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SUPERHUMAN_NEUROMOD_BRIDGE_H */
