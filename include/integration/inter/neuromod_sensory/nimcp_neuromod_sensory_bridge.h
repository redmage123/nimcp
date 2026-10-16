/**
 * @file nimcp_neuromod_sensory_bridge.h
 * @brief Neuromodulatory-Sensory Inter-Layer Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Bridges neuromodulatory state to sensory processing
 * WHY:  Arousal and attention modulate sensory gain
 * HOW:  NE/DA levels adjust sensory thresholds and gain
 *
 * KEY PATHWAYS:
 * =============
 * Bottom-Up (Neuromodulatory → Sensory):
 * - NE levels → sensory gain enhancement
 * - DA levels → salience detection boost
 * - Arousal state → sensory threshold adjustment
 *
 * Top-Down (Sensory → Neuromodulatory):
 * - Novel stimuli → LC activation (NE burst)
 * - Rewarding stimuli → VTA activation (DA burst)
 * - Aversive stimuli → stress response trigger
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NEUROMOD_SENSORY_BRIDGE_H
#define NIMCP_NEUROMOD_SENSORY_BRIDGE_H

#include "integration/core/nimcp_layer_types.h"
#include "integration/core/nimcp_layer_registry.h"
#include "integration/intra/neuromodulatory/nimcp_neuromod_intra_coordinator.h"
#include "integration/intra/sensory/nimcp_sensory_intra_coordinator.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bridge message types */
#define NEURO_SENS_MSG_GAIN_MODULATE    (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0030)
#define NEURO_SENS_MSG_SALIENCE_BOOST   (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0031)
#define NEURO_SENS_MSG_THRESHOLD_ADJ    (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0032)
#define NEURO_SENS_MSG_NOVEL_STIMULUS   (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0033)
#define NEURO_SENS_MSG_REWARD_STIMULUS  (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0034)
#define NEURO_SENS_MSG_AVERSIVE_STIM    (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0035)

typedef struct nimcp_neuromod_sensory_bridge_struct* nimcp_neuromod_sensory_bridge_t;

typedef struct {
    float ne_gain_coupling;
    float da_salience_coupling;
    float arousal_threshold_coupling;
    float novelty_lc_gain;
    float reward_vta_gain;
    uint32_t update_interval_ms;
    bool enable_gain_control;
    bool enable_logging;
    bool enable_metrics;
} nimcp_neuromod_sensory_config_t;

typedef struct {
    float sensory_gain;
    float salience_boost;
    float threshold_level;
    float novelty_signal;
    float reward_signal;
    float aversive_signal;
    float bridge_coherence;
    uint64_t bottom_up_messages;
    uint64_t top_down_messages;
} nimcp_neuromod_sensory_state_t;

typedef struct {
    uint64_t gain_modulations;
    uint64_t salience_boosts;
    uint64_t threshold_adjustments;
    uint64_t novel_stimuli;
    uint64_t reward_stimuli;
    uint64_t aversive_stimuli;
    float avg_sensory_gain;
    float avg_salience;
} nimcp_neuromod_sensory_stats_t;

NIMCP_EXPORT nimcp_neuromod_sensory_config_t nimcp_neuromod_sensory_default_config(void);
NIMCP_EXPORT nimcp_neuromod_sensory_bridge_t nimcp_neuromod_sensory_create(const nimcp_neuromod_sensory_config_t* config);
NIMCP_EXPORT void nimcp_neuromod_sensory_destroy(nimcp_neuromod_sensory_bridge_t bridge);
NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_sensory_init(
    nimcp_neuromod_sensory_bridge_t bridge,
    nimcp_layer_registry_t registry,
    nimcp_neuromod_intra_t neuromod,
    nimcp_sensory_intra_t sensory
);
NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_sensory_shutdown(nimcp_neuromod_sensory_bridge_t bridge);

NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_sensory_update(nimcp_neuromod_sensory_bridge_t bridge, float dt);
NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_sensory_transfer_bottom_up(nimcp_neuromod_sensory_bridge_t bridge, const nimcp_layer_msg_t* msg);
NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_sensory_transfer_top_down(nimcp_neuromod_sensory_bridge_t bridge, const nimcp_layer_msg_t* msg);

NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_sensory_get_state(nimcp_neuromod_sensory_bridge_t bridge, nimcp_neuromod_sensory_state_t* state_out);
NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_sensory_get_stats(nimcp_neuromod_sensory_bridge_t bridge, nimcp_neuromod_sensory_stats_t* stats_out);
NIMCP_EXPORT float nimcp_neuromod_sensory_get_coherence(nimcp_neuromod_sensory_bridge_t bridge);
NIMCP_EXPORT nimcp_layer_error_t nimcp_neuromod_sensory_reset_stats(nimcp_neuromod_sensory_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEUROMOD_SENSORY_BRIDGE_H */
