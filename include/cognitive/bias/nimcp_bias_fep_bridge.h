/**
 * @file nimcp_bias_fep_bridge.h
 * @brief Free Energy Principle - Cognitive Bias Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between FEP and Cognitive Bias system
 * WHY:  Cognitive biases = systematic prediction errors in FEP. Biases arise from
 *       overly-strong priors, precision misweighting, or model misspecification.
 *       FEP explains bias origins; bias detection improves FEP accuracy.
 * HOW:  FEP systematic PEs → bias detection; bias → FEP prior correction
 *
 * BIOLOGICAL BASIS:
 * - Confirmation bias = overly-precise priors (ignore counter-evidence)
 * - Availability bias = recency-weighted precision
 * - Anchoring bias = insufficient belief updating (low learning rate)
 * - Reference: Friston (2009) "The free-energy principle: a rough guide to the brain?"
 */

#ifndef NIMCP_BIAS_FEP_BRIDGE_H
#define NIMCP_BIAS_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BIAS_FEP_SYSTEMATIC_PE_THRESHOLD     3.0f
#define BIAS_FEP_CONFIRMATION_THRESHOLD      0.7f
#define BIAS_FEP_PRIOR_CORRECTION_RATE       0.15f

typedef struct bias_fep_bridge bias_fep_bridge_t;

typedef enum {
    BIAS_TYPE_CONFIRMATION,
    BIAS_TYPE_AVAILABILITY,
    BIAS_TYPE_ANCHORING,
    BIAS_TYPE_RECENCY,
    BIAS_TYPE_COUNT
} cognitive_bias_type_t;

typedef struct {
    float systematic_pe_threshold;
    float confirmation_threshold;
    float prior_correction_rate;
    bool enable_bias_detection;
    bool enable_prior_correction;
    float pe_sensitivity;
} bias_fep_config_t;

typedef struct {
    float systematic_pe;
    cognitive_bias_type_t detected_bias;
    bool bias_active;
} bias_fep_effects_t;

typedef struct {
    uint32_t biases_detected;
    float bias_magnitude;
    cognitive_bias_type_t current_bias;
} bias_fep_state_t;

typedef struct {
    uint64_t bias_detections_total;
    uint64_t prior_corrections_total;
    float avg_systematic_pe;
} bias_fep_stats_t;

struct bias_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    bias_fep_config_t config;
    fep_system_t* fep_system;
    bias_fep_effects_t effects;
    bias_fep_state_t state;
    bias_fep_stats_t stats;
};

int bias_fep_bridge_default_config(bias_fep_config_t* config);
bias_fep_bridge_t* bias_fep_bridge_create(const bias_fep_config_t* config);
void bias_fep_bridge_destroy(bias_fep_bridge_t* bridge);

int bias_fep_bridge_connect_fep(bias_fep_bridge_t* bridge, fep_system_t* fep);

int bias_fep_detect_bias(bias_fep_bridge_t* bridge, float prediction_error);
int bias_fep_correct_prior(bias_fep_bridge_t* bridge, cognitive_bias_type_t bias);

int bias_fep_bridge_update(bias_fep_bridge_t* bridge, uint64_t delta_ms);
int bias_fep_bridge_get_state(bias_fep_bridge_t* bridge, bias_fep_state_t* state);
int bias_fep_bridge_get_stats(bias_fep_bridge_t* bridge, bias_fep_stats_t* stats);

int bias_fep_bridge_connect_bio_async(bias_fep_bridge_t* bridge);
int bias_fep_bridge_disconnect_bio_async(bias_fep_bridge_t* bridge);
bool bias_fep_bridge_is_bio_async_connected(bias_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BIAS_FEP_BRIDGE_H */
