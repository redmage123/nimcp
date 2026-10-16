/**
 * @file nimcp_shadow_emotions_fep_bridge.h
 * @brief Free Energy Principle - Shadow Emotions Integration Bridge
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Bidirectional integration between Free Energy Principle and shadow emotions system
 * WHY:  Maladaptive emotions emerge from dysregulated prediction error signals;
 *       FEP provides framework for understanding and correcting shadow patterns.
 * HOW:  FEP prediction errors trigger shadow emotion monitoring; interventions
 *       restore precision calibration; self-awareness enables FEP-guided correction.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SHADOW EMOTIONS AS FEP DYSREGULATION:
 * -------------------------------------
 * - Jealousy = over-precision in threat detection
 * - Hubris = under-precision in self-assessment
 * - Reference: Badcock et al. (2017) "The hierarchically mechanistic mind"
 *
 * FEP → SHADOW EMOTIONS PATHWAYS:
 * -------------------------------
 * 1. Prediction Error Calibration:
 *    - Over-weighting PE → jealousy, envy
 *    - Under-weighting PE → hubris, narcissism
 *    - Dysregulated precision → obsession
 *
 * 2. Precision Optimization Interventions:
 *    - Recalibrate precision weights
 *    - Restore prediction error balance
 *    - FEP-guided self-correction
 *
 * SHADOW EMOTIONS → FEP PATHWAYS:
 * --------------------------------
 * 1. Shadow Detection Signals Dysregulation:
 *    - High jealousy → inflate threat precision
 *    - High hubris → deflate error precision
 *    - Shadow patterns → FEP diagnostic
 *
 * 2. Self-Correction Restores FEP:
 *    - CBT interventions → precision recalibration
 *    - Mindfulness → prediction error acceptance
 *    - Insight → belief model updates
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SHADOW_EMOTIONS_FEP_BRIDGE_H
#define NIMCP_SHADOW_EMOTIONS_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/nimcp_shadow_emotions.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SHADOW_FEP_DYSREGULATION_THRESHOLD   5.0f
#define SHADOW_FEP_PRECISION_CALIBRATION     0.6f
#define SHADOW_FEP_INTERVENTION_STRENGTH     0.75f

typedef struct shadow_emotions_fep_bridge shadow_emotions_fep_bridge_t;

typedef struct {
    float dysregulation_threshold;
    float precision_calibration_strength;
    float intervention_effectiveness;
    bool enable_pe_calibration;
    bool enable_precision_interventions;
    bool enable_self_correction;
    float jealousy_pe_sensitivity;
    float hubris_pe_insensitivity;
    bool enable_diagnostic_mode;
    bool enable_restoration_mode;
    float fe_sensitivity;
    float shadow_sensitivity;
} shadow_emotions_fep_config_t;

typedef struct {
    float prediction_error_dysregulation;
    float precision_over_weighting;
    float precision_under_weighting;
    bool intervention_triggered;
    shadow_intervention_type_t recommended_intervention;
    bool recalibration_active;
} shadow_emotions_fep_effects_t;

typedef struct {
    float jealousy_precision_inflation;
    float hubris_precision_deflation;
    float obsession_pe_fixation;
    bool shadow_pattern_detected;
    shadow_emotion_type_t dominant_shadow;
    bool fep_diagnostic_active;
} fep_shadow_emotions_effects_t;

typedef struct {
    float current_dysregulation;
    float current_precision_bias;
    bool intervention_active;
    shadow_emotion_type_t target_emotion;
    uint64_t last_intervention_time;
    uint64_t last_calibration_time;
} shadow_emotions_fep_state_t;

typedef struct {
    uint64_t intervention_events;
    uint64_t calibration_events;
    uint64_t pattern_detections;
    float avg_dysregulation;
    float avg_precision_bias;
    uint64_t successful_corrections;
    uint64_t failed_corrections;
    float avg_free_energy;
} shadow_emotions_fep_stats_t;

struct shadow_emotions_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    shadow_emotions_fep_config_t config;
    fep_system_t* fep_system;
    shadow_emotion_system_t* shadow_system;
    shadow_emotions_fep_effects_t fep_effects;
    fep_shadow_emotions_effects_t shadow_effects;
    shadow_emotions_fep_state_t state;
    shadow_emotions_fep_stats_t stats;
};

int shadow_emotions_fep_bridge_default_config(shadow_emotions_fep_config_t* config);
shadow_emotions_fep_bridge_t* shadow_emotions_fep_bridge_create(const shadow_emotions_fep_config_t* config);
void shadow_emotions_fep_bridge_destroy(shadow_emotions_fep_bridge_t* bridge);

int shadow_emotions_fep_bridge_connect_fep(shadow_emotions_fep_bridge_t* bridge, fep_system_t* fep);
int shadow_emotions_fep_bridge_connect_shadow(shadow_emotions_fep_bridge_t* bridge, shadow_emotion_system_t* shadow);
int shadow_emotions_fep_bridge_disconnect(shadow_emotions_fep_bridge_t* bridge);

int shadow_emotions_fep_detect_dysregulation(shadow_emotions_fep_bridge_t* bridge);
int shadow_emotions_fep_trigger_intervention(shadow_emotions_fep_bridge_t* bridge, shadow_emotion_type_t emotion);
int shadow_emotions_fep_recalibrate_precision(shadow_emotions_fep_bridge_t* bridge);

int shadow_emotions_fep_apply_shadow_diagnostic(shadow_emotions_fep_bridge_t* bridge);
int shadow_emotions_fep_update_beliefs_from_correction(shadow_emotions_fep_bridge_t* bridge);

int shadow_emotions_fep_bridge_update(shadow_emotions_fep_bridge_t* bridge, uint64_t delta_ms);
int shadow_emotions_fep_bridge_get_state(shadow_emotions_fep_bridge_t* bridge, shadow_emotions_fep_state_t* state);
int shadow_emotions_fep_bridge_get_stats(shadow_emotions_fep_bridge_t* bridge, shadow_emotions_fep_stats_t* stats);

int shadow_emotions_fep_bridge_connect_bio_async(shadow_emotions_fep_bridge_t* bridge);
int shadow_emotions_fep_bridge_disconnect_bio_async(shadow_emotions_fep_bridge_t* bridge);
bool shadow_emotions_fep_bridge_is_bio_async_connected(const shadow_emotions_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SHADOW_EMOTIONS_FEP_BRIDGE_H */
