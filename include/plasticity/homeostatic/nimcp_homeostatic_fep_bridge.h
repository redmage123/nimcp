/**
 * @file nimcp_homeostatic_fep_bridge.h
 * @brief Free Energy Principle - Homeostatic Plasticity Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between FEP and homeostatic plasticity
 * WHY:  FEP precision normalization aligns with homeostatic stability; homeostatic
 *       scaling maintains optimal precision for inference.
 * HOW:  FEP precision targets drive homeostatic set points; synaptic scaling
 *       maintains precision calibration for accurate free energy minimization.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * FEP → HOMEOSTATIC PATHWAYS:
 * ---------------------------
 * 1. Precision as Homeostatic Target:
 *    - FEP precision = optimal signal-to-noise ratio
 *    - Homeostasis maintains this precision via synaptic scaling
 *    - Target firing rate ∝ precision estimate
 *    - Reference: Friston (2008) "Hierarchical models in the brain"
 *
 * 2. Free Energy Drives Homeostatic Adjustments:
 *    - High free energy → increase scaling (adjust set point)
 *    - Low free energy → stable homeostasis
 *    - Homeostatic time constant ∝ 1/free_energy
 *
 * 3. Precision Normalization = Synaptic Scaling:
 *    - Both normalize signals to optimal range
 *    - Precision weighting = gain control = homeostatic scaling
 *    - Implements efficient coding
 *
 * HOMEOSTATIC → FEP PATHWAYS:
 * ---------------------------
 * 1. Synaptic Scaling Maintains Precision:
 *    - Multiplicative scaling preserves precision calibration
 *    - Prevents precision drift during learning
 *    - Stable precision → accurate free energy estimates
 *
 * 2. Firing Rate Homeostasis = Precision Optimization:
 *    - Target rate = information-theoretically optimal
 *    - Deviation from target = precision error
 *    - Homeostasis minimizes precision loss
 *
 * INTEGRATION MECHANISMS:
 * -----------------------
 * - Precision → target rate: target = f(precision)
 * - Free energy → scaling speed: τ_scale ∝ 1/F
 * - Scaling factor → precision: precision *= scaling_factor
 * - Stability → convergence: homeostasis ⇔ FEP convergence
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HOMEOSTATIC_FEP_BRIDGE_H
#define NIMCP_HOMEOSTATIC_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "plasticity/homeostatic/nimcp_homeostatic.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HOMEOSTATIC_FEP_PRECISION_MIN     0.1f
#define HOMEOSTATIC_FEP_PRECISION_MAX     2.0f
#define HOMEOSTATIC_FEP_TARGET_RATE_SCALING 5.0f

typedef struct homeostatic_fep_bridge homeostatic_fep_bridge_t;

typedef struct {
    float precision_target_gain;
    float free_energy_scaling_factor;
    float stability_threshold;
    bool enable_precision_normalization;
    bool enable_fe_modulation;
    bool enable_stability_tracking;
} homeostatic_fep_config_t;

typedef struct {
    float precision_value;
    float precision_target_rate;
    float free_energy_value;
    float scaling_time_modulation;
    float total_normalization_factor;
} homeostatic_fep_effects_t;

typedef struct {
    float current_precision;
    float current_free_energy;
    float target_rate_modulation;
    bool stable;
    uint64_t last_update_time;
} homeostatic_fep_state_t;

typedef struct {
    uint64_t total_updates;
    uint64_t precision_adjustments;
    float avg_precision;
    float avg_free_energy;
    float avg_scaling_factor;
} homeostatic_fep_stats_t;

struct homeostatic_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    homeostatic_fep_config_t config;
    fep_system_t* fep_system;
    homeostatic_controller_t homeostatic_system;
    homeostatic_fep_effects_t effects;
    homeostatic_fep_state_t state;
    homeostatic_fep_stats_t stats;
};

int homeostatic_fep_bridge_default_config(homeostatic_fep_config_t* config);
homeostatic_fep_bridge_t* homeostatic_fep_bridge_create(const homeostatic_fep_config_t* config);
void homeostatic_fep_bridge_destroy(homeostatic_fep_bridge_t* bridge);

int homeostatic_fep_bridge_connect_fep(homeostatic_fep_bridge_t* bridge, fep_system_t* fep);
int homeostatic_fep_bridge_connect_homeostatic(homeostatic_fep_bridge_t* bridge, homeostatic_controller_t homeostatic);
int homeostatic_fep_bridge_disconnect(homeostatic_fep_bridge_t* bridge);

float homeostatic_fep_apply_precision_normalization(homeostatic_fep_bridge_t* bridge, float precision);
float homeostatic_fep_apply_fe_modulation(homeostatic_fep_bridge_t* bridge, float free_energy);
float homeostatic_fep_get_effective_target_rate(const homeostatic_fep_bridge_t* bridge, float base_rate);

int homeostatic_fep_report_scaling(homeostatic_fep_bridge_t* bridge, float scaling_factor);
int homeostatic_fep_bridge_update(homeostatic_fep_bridge_t* bridge, uint64_t delta_ms);

int homeostatic_fep_bridge_get_state(const homeostatic_fep_bridge_t* bridge, homeostatic_fep_state_t* state);
int homeostatic_fep_bridge_get_stats(const homeostatic_fep_bridge_t* bridge, homeostatic_fep_stats_t* stats);

int homeostatic_fep_bridge_connect_bio_async(homeostatic_fep_bridge_t* bridge);
int homeostatic_fep_bridge_disconnect_bio_async(homeostatic_fep_bridge_t* bridge);
bool homeostatic_fep_bridge_is_bio_async_connected(const homeostatic_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HOMEOSTATIC_FEP_BRIDGE_H */
