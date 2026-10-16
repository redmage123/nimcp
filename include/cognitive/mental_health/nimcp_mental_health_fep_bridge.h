/**
 * @file nimcp_mental_health_fep_bridge.h
 * @brief Free Energy Principle - Mental Health Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between FEP and mental health monitoring
 * WHY:  Mental health disorders reflect pathological inference (distorted precision
 *       weighting, aberrant learning rates, maladaptive priors). FEP framework
 *       explains psychiatric symptoms as inference failures.
 * HOW:  FEP detects aberrant precision/learning; mental health system monitors
 *       for pathological patterns; interventions restore healthy inference.
 *
 * BIOLOGICAL BASIS:
 * - Adams et al. (2013): Computational psychiatry and FEP
 * - Sterzer et al. (2018): Abnormal precision in psychosis
 * - Depression = overly precise negative priors
 * - Anxiety = reduced precision, increased uncertainty
 * - Psychosis = aberrant precision on irrelevant stimuli
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_MENTAL_HEALTH_FEP_BRIDGE_H
#define NIMCP_MENTAL_HEALTH_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/nimcp_mental_health.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MENTAL_HEALTH_FEP_ABERRANT_PRECISION_THRESHOLD 10.0f
#define MENTAL_HEALTH_FEP_PATHOLOGICAL_LR_THRESHOLD    0.01f

typedef struct mental_health_fep_bridge mental_health_fep_bridge_t;

typedef struct {
    float aberrant_precision_threshold;
    float pathological_lr_threshold;
    float negative_prior_threshold;
    bool enable_aberrant_precision_detection;
    bool enable_pathological_learning_detection;
    bool enable_negative_prior_detection;
    float intervention_precision_correction;
    float intervention_lr_correction;
    bool enable_precision_intervention;
    bool enable_lr_intervention;
    float fe_sensitivity;
    float mental_health_sensitivity;
} mental_health_fep_config_t;

typedef struct {
    float current_precision;
    bool aberrant_precision_detected;
    float current_learning_rate;
    bool pathological_learning_detected;
    float negative_prior_strength;
    bool negative_priors_detected;
} mental_health_fep_effects_t;

typedef struct {
    float precision_correction;
    float lr_correction;
    bool intervention_active;
} fep_mental_health_effects_t;

typedef struct {
    float current_precision;
    float current_learning_rate;
    bool pathology_detected;
    uint64_t last_detection_time;
} mental_health_fep_state_t;

typedef struct {
    uint64_t aberrant_precision_events;
    uint64_t pathological_learning_events;
    uint64_t negative_prior_events;
    uint64_t intervention_events;
    float avg_precision;
    float avg_learning_rate;
    float avg_free_energy;
} mental_health_fep_stats_t;

struct mental_health_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    mental_health_fep_config_t config;
    fep_system_t* fep_system;
    mental_health_monitor_t* mental_health_system;
    mental_health_fep_effects_t fep_effects;
    fep_mental_health_effects_t mental_health_effects;
    mental_health_fep_state_t state;
    mental_health_fep_stats_t stats;
};

int mental_health_fep_bridge_default_config(mental_health_fep_config_t* config);
mental_health_fep_bridge_t* mental_health_fep_bridge_create(const mental_health_fep_config_t* config);
void mental_health_fep_bridge_destroy(mental_health_fep_bridge_t* bridge);

int mental_health_fep_bridge_connect_fep(mental_health_fep_bridge_t* bridge, fep_system_t* fep);
int mental_health_fep_bridge_connect_mental_health(mental_health_fep_bridge_t* bridge, mental_health_monitor_t* mh);
int mental_health_fep_bridge_disconnect(mental_health_fep_bridge_t* bridge);

int mental_health_fep_detect_aberrant_precision(mental_health_fep_bridge_t* bridge);
int mental_health_fep_detect_pathological_learning(mental_health_fep_bridge_t* bridge);
int mental_health_fep_detect_negative_priors(mental_health_fep_bridge_t* bridge);

int mental_health_fep_apply_precision_intervention(mental_health_fep_bridge_t* bridge);
int mental_health_fep_apply_lr_intervention(mental_health_fep_bridge_t* bridge);

int mental_health_fep_bridge_update(mental_health_fep_bridge_t* bridge, uint64_t delta_ms);

int mental_health_fep_bridge_get_state(mental_health_fep_bridge_t* bridge, mental_health_fep_state_t* state);
int mental_health_fep_bridge_get_stats(mental_health_fep_bridge_t* bridge, mental_health_fep_stats_t* stats);

int mental_health_fep_bridge_connect_bio_async(mental_health_fep_bridge_t* bridge);
int mental_health_fep_bridge_disconnect_bio_async(mental_health_fep_bridge_t* bridge);
bool mental_health_fep_bridge_is_bio_async_connected(const mental_health_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MENTAL_HEALTH_FEP_BRIDGE_H */
