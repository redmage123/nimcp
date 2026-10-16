/**
 * @file nimcp_salience_fep_bridge.h
 * @brief Free Energy Principle - Salience Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between Free Energy Principle and salience system
 * WHY:  Salience modulates FEP precision (salient stimuli get high precision weighting);
 *       FEP prediction errors determine salience (surprise → attention)
 * HOW:  Salience scores modulate FEP precision matrices; FEP errors drive salience
 *       computation (novelty, surprise, urgency)
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SALIENCE AS PRECISION MODULATION:
 * --------------------------------
 * 1. Attention as Precision Weighting:
 *    - Salient stimuli → increased precision
 *    - Precision = inverse variance (confidence)
 *    - High precision → larger influence on inference
 *    - Reference: Feldman & Friston (2010) "Attention, uncertainty, and free energy"
 *
 * 2. Prediction Errors Drive Salience:
 *    - Surprise → salience
 *    - Novelty = high prediction error
 *    - Urgency = high-precision error
 *    - Reference: Itti & Baldi (2009) "Bayesian surprise attracts human attention"
 *
 * 3. Salience Network and FEP:
 *    - Anterior insula detects salient events
 *    - ACC monitors prediction errors
 *    - Reference: Menon & Uddin (2010) "Saliency, switching, attention and control"
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SALIENCE_FEP_BRIDGE_H
#define NIMCP_SALIENCE_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/salience/nimcp_salience.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants & Structures
 * ============================================================================ */

#define SALIENCE_PRECISION_BOOST_MAX      2.0f
#define SURPRISE_SALIENCE_THRESHOLD       0.7f

typedef struct salience_fep_bridge salience_fep_bridge_t;

typedef struct {
    float salience_precision_gain;
    float surprise_salience_weight;
    float novelty_precision_boost;
    float urgency_precision_boost;
    bool enable_precision_modulation;
    bool enable_pe_salience;
    bool enable_salience_gating;
} salience_fep_config_t;

typedef struct {
    float precision_boost;
    float salience_from_pe;
    float gating_factor;
} salience_fep_effects_t;

typedef struct {
    float current_salience;
    float current_precision_boost;
    uint32_t high_salience_events;
    float avg_prediction_error;
} salience_fep_state_t;

typedef struct {
    uint64_t total_precision_boosts;
    uint64_t total_salience_gates;
    float avg_salience;
    float avg_precision_boost;
} salience_fep_stats_t;

struct salience_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    salience_fep_config_t config;
    fep_system_t* fep_system;
    salience_evaluator_t salience_evaluator;
    salience_fep_effects_t effects;
    salience_fep_state_t state;
    salience_fep_stats_t stats;
};

/* ============================================================================
 * API
 * ============================================================================ */

int salience_fep_bridge_default_config(salience_fep_config_t* config);
salience_fep_bridge_t* salience_fep_bridge_create(const salience_fep_config_t* config);
void salience_fep_bridge_destroy(salience_fep_bridge_t* bridge);

int salience_fep_bridge_connect_fep(salience_fep_bridge_t* bridge, fep_system_t* fep);
int salience_fep_bridge_connect_salience(salience_fep_bridge_t* bridge, salience_evaluator_t salience);

int salience_fep_modulate_precision_by_salience(salience_fep_bridge_t* bridge);
int salience_fep_compute_salience_from_pe(salience_fep_bridge_t* bridge);
int salience_fep_gate_by_salience(salience_fep_bridge_t* bridge);

int salience_fep_bridge_update(salience_fep_bridge_t* bridge, uint64_t delta_ms);
int salience_fep_bridge_get_state(const salience_fep_bridge_t* bridge, salience_fep_state_t* state);
int salience_fep_bridge_get_stats(const salience_fep_bridge_t* bridge, salience_fep_stats_t* stats);

int salience_fep_bridge_connect_bio_async(salience_fep_bridge_t* bridge);
int salience_fep_bridge_disconnect_bio_async(salience_fep_bridge_t* bridge);
bool salience_fep_bridge_is_bio_async_connected(const salience_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SALIENCE_FEP_BRIDGE_H */
