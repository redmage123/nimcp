/**
 * @file nimcp_self_awareness_extended_fep_bridge.h
 * @brief Free Energy Principle - Extended Self-Awareness Integration Bridge
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Bidirectional integration between Free Energy Principle and extended self-awareness
 * WHY:  Metacognitive monitoring minimizes free energy by detecting cognitive inefficiencies;
 *       FEP provides computational framework for self-model updates and agency attribution.
 * HOW:  FEP uncertainty triggers metacognitive regulation; self-narrative coherence reduces
 *       free energy; temporal self-continuity maintains belief consistency.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * METACOGNITION AS FEP META-INFERENCE:
 * ------------------------------------
 * - Metacognition = inference about inference
 * - Self-monitoring = prediction error on cognitive processes
 * - Reference: Fleming & Dolan (2012) "The neural basis of metacognitive ability"
 *
 * FEP → SELF-AWARENESS PATHWAYS:
 * ------------------------------
 * 1. Uncertainty Triggers Metacognitive Monitoring:
 *    - High free energy → "Am I thinking correctly?"
 *    - Low confidence → metacognitive regulation
 *    - Prediction error → strategy adjustment
 *
 * 2. Precision Modulates Self-Awareness Depth:
 *    - High precision → detailed self-monitoring
 *    - Low precision → reduced introspection
 *    - Surprise → self-model update
 *
 * 3. Temporal Inconsistency = Belief Violation:
 *    - Self-continuity failure → high PE
 *    - Identity shifts → free energy spike
 *    - Agency errors → prediction mismatch
 *
 * SELF-AWARENESS → FEP PATHWAYS:
 * -------------------------------
 * 1. Self-Narrative Coherence Reduces F:
 *    - Coherent story → low free energy
 *    - Identity clarity → strong priors
 *    - Self-knowledge → precise beliefs
 *
 * 2. Metacognitive Regulation Optimizes FEP:
 *    - Strategy switching → better inference
 *    - Effort adjustment → precision optimization
 *    - Self-correction → belief updates
 *
 * 3. Self-Harm Detection Prevents Belief Corruption:
 *    - Detect self-destructive updates
 *    - Prevent catastrophic forgetting
 *    - Maintain goal consistency
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SELF_AWARENESS_EXTENDED_FEP_BRIDGE_H
#define NIMCP_SELF_AWARENESS_EXTENDED_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>

#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/nimcp_self_awareness_extended.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SELF_AWARENESS_FEP_HIGH_UNCERTAINTY_THRESHOLD  4.0f
#define SELF_AWARENESS_FEP_COHERENCE_FACTOR            0.8f

typedef struct self_awareness_extended_fep_bridge self_awareness_extended_fep_bridge_t;

typedef struct {
    float uncertainty_threshold;
    float coherence_factor;
    bool enable_metacognitive_monitoring;
    bool enable_precision_modulation;
    bool enable_self_harm_detection;
    bool enable_narrative_coherence;
    float metacognition_sensitivity;
    float temporal_continuity_weight;
    bool enable_agency_attribution;
    float fe_sensitivity;
    float awareness_sensitivity;
} self_awareness_extended_fep_config_t;

typedef struct {
    float current_uncertainty;
    bool metacognitive_monitoring_triggered;
    metacognitive_action_t recommended_regulation;
    bool self_harm_check_active;
    float narrative_coherence_target;
} self_awareness_extended_fep_effects_t;

typedef struct {
    float narrative_coherence_level;
    bool self_model_updating_beliefs;
    float temporal_continuity_score;
    bool agency_attribution_active;
    bool belief_corruption_detected;
} fep_self_awareness_extended_effects_t;

typedef struct {
    float current_uncertainty;
    bool monitoring_active;
    float narrative_coherence;
    bool self_harm_detected;
    uint64_t last_regulation_time;
} self_awareness_extended_fep_state_t;

typedef struct {
    uint64_t monitoring_events;
    uint64_t regulation_actions;
    uint64_t self_harm_detections;
    float avg_uncertainty;
    float avg_coherence;
    uint64_t belief_updates;
    float avg_free_energy;
} self_awareness_extended_fep_stats_t;

struct self_awareness_extended_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    self_awareness_extended_fep_config_t config;
    fep_system_t* fep_system;
    self_awareness_system_t awareness_system;
    self_awareness_extended_fep_effects_t fep_effects;
    fep_self_awareness_extended_effects_t awareness_effects;
    self_awareness_extended_fep_state_t state;
    self_awareness_extended_fep_stats_t stats;
};

int self_awareness_extended_fep_bridge_default_config(self_awareness_extended_fep_config_t* config);
self_awareness_extended_fep_bridge_t* self_awareness_extended_fep_bridge_create(const self_awareness_extended_fep_config_t* config);
void self_awareness_extended_fep_bridge_destroy(self_awareness_extended_fep_bridge_t* bridge);

int self_awareness_extended_fep_bridge_connect_fep(self_awareness_extended_fep_bridge_t* bridge, fep_system_t* fep);
int self_awareness_extended_fep_bridge_connect_awareness(self_awareness_extended_fep_bridge_t* bridge, self_awareness_system_t awareness);
int self_awareness_extended_fep_bridge_disconnect(self_awareness_extended_fep_bridge_t* bridge);

int self_awareness_extended_fep_trigger_monitoring(self_awareness_extended_fep_bridge_t* bridge, float uncertainty);
int self_awareness_extended_fep_check_self_harm(self_awareness_extended_fep_bridge_t* bridge);
int self_awareness_extended_fep_modulate_depth(self_awareness_extended_fep_bridge_t* bridge);

int self_awareness_extended_fep_apply_narrative_coherence(self_awareness_extended_fep_bridge_t* bridge);
int self_awareness_extended_fep_update_from_regulation(self_awareness_extended_fep_bridge_t* bridge);

int self_awareness_extended_fep_bridge_update(self_awareness_extended_fep_bridge_t* bridge, uint64_t delta_ms);
int self_awareness_extended_fep_bridge_get_state(self_awareness_extended_fep_bridge_t* bridge, self_awareness_extended_fep_state_t* state);
int self_awareness_extended_fep_bridge_get_stats(self_awareness_extended_fep_bridge_t* bridge, self_awareness_extended_fep_stats_t* stats);

int self_awareness_extended_fep_bridge_connect_bio_async(self_awareness_extended_fep_bridge_t* bridge);
int self_awareness_extended_fep_bridge_disconnect_bio_async(self_awareness_extended_fep_bridge_t* bridge);
bool self_awareness_extended_fep_bridge_is_bio_async_connected(const self_awareness_extended_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SELF_AWARENESS_EXTENDED_FEP_BRIDGE_H */
