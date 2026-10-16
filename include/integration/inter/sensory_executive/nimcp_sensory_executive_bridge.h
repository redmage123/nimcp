/**
 * @file nimcp_sensory_executive_bridge.h
 * @brief Sensory-Executive Inter-Layer Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Bridges sensory processing to executive control
 * WHY:  Executive functions direct attention and respond to sensory input
 * HOW:  Routes salient stimuli to decision systems, attention modulates processing
 *
 * KEY PATHWAYS:
 * =============
 * Bottom-Up (Sensory → Executive):
 * - Salient stimuli → attention capture
 * - Sensory features → decision input
 * - Sensory conflict → cognitive control trigger
 *
 * Top-Down (Executive → Sensory):
 * - Attention → feature enhancement
 * - Goals → sensory filter settings
 * - Expectations → predictive enhancement
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SENSORY_EXECUTIVE_BRIDGE_H
#define NIMCP_SENSORY_EXECUTIVE_BRIDGE_H

#include "integration/core/nimcp_layer_types.h"
#include "integration/core/nimcp_layer_registry.h"
#include "integration/intra/sensory/nimcp_sensory_intra_coordinator.h"
#include "integration/intra/executive/nimcp_executive_intra_coordinator.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bridge message types */
#define SENS_EXEC_MSG_ATTENTION_CAPTURE (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0070)
#define SENS_EXEC_MSG_DECISION_INPUT    (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0071)
#define SENS_EXEC_MSG_CONFLICT_DETECT   (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0072)
#define SENS_EXEC_MSG_FEATURE_ENHANCE   (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0073)
#define SENS_EXEC_MSG_SENSORY_FILTER    (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0074)
#define SENS_EXEC_MSG_PREDICTIVE_ENH    (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0075)

typedef struct nimcp_sensory_executive_bridge_struct* nimcp_sensory_executive_bridge_t;

typedef struct {
    float attention_capture_threshold;
    float decision_input_coupling;
    float conflict_detection_gain;
    float feature_enhancement_strength;
    float filter_coupling;
    float predictive_enhancement_gain;
    uint32_t update_interval_ms;
    bool enable_attention_control;
    bool enable_logging;
    bool enable_metrics;
} nimcp_sensory_executive_config_t;

typedef struct {
    float attention_capture_level;
    float decision_input_strength;
    float conflict_level;
    float feature_enhancement;
    float filter_strength;
    float predictive_enhancement;
    float bridge_coherence;
    uint64_t bottom_up_messages;
    uint64_t top_down_messages;
} nimcp_sensory_executive_state_t;

typedef struct {
    uint64_t attention_captures;
    uint64_t decision_inputs;
    uint64_t conflict_detections;
    uint64_t feature_enhancements;
    uint64_t filter_updates;
    uint64_t predictive_enhancements;
    float avg_attention_capture;
    float avg_feature_enhancement;
} nimcp_sensory_executive_stats_t;

NIMCP_EXPORT nimcp_sensory_executive_config_t nimcp_sensory_executive_default_config(void);
NIMCP_EXPORT nimcp_sensory_executive_bridge_t nimcp_sensory_executive_create(const nimcp_sensory_executive_config_t* config);
NIMCP_EXPORT void nimcp_sensory_executive_destroy(nimcp_sensory_executive_bridge_t bridge);
NIMCP_EXPORT nimcp_layer_error_t nimcp_sensory_executive_init(
    nimcp_sensory_executive_bridge_t bridge,
    nimcp_layer_registry_t registry,
    nimcp_sensory_intra_t sensory,
    nimcp_executive_intra_t executive
);
NIMCP_EXPORT nimcp_layer_error_t nimcp_sensory_executive_shutdown(nimcp_sensory_executive_bridge_t bridge);

NIMCP_EXPORT nimcp_layer_error_t nimcp_sensory_executive_update(nimcp_sensory_executive_bridge_t bridge, float dt);
NIMCP_EXPORT nimcp_layer_error_t nimcp_sensory_executive_transfer_bottom_up(nimcp_sensory_executive_bridge_t bridge, const nimcp_layer_msg_t* msg);
NIMCP_EXPORT nimcp_layer_error_t nimcp_sensory_executive_transfer_top_down(nimcp_sensory_executive_bridge_t bridge, const nimcp_layer_msg_t* msg);

NIMCP_EXPORT nimcp_layer_error_t nimcp_sensory_executive_get_state(nimcp_sensory_executive_bridge_t bridge, nimcp_sensory_executive_state_t* state_out);
NIMCP_EXPORT nimcp_layer_error_t nimcp_sensory_executive_get_stats(nimcp_sensory_executive_bridge_t bridge, nimcp_sensory_executive_stats_t* stats_out);
NIMCP_EXPORT float nimcp_sensory_executive_get_coherence(nimcp_sensory_executive_bridge_t bridge);
NIMCP_EXPORT nimcp_layer_error_t nimcp_sensory_executive_reset_stats(nimcp_sensory_executive_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SENSORY_EXECUTIVE_BRIDGE_H */
