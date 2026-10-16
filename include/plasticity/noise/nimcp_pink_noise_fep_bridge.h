/**
 * @file nimcp_pink_noise_fep_bridge.h
 * @brief Free Energy Principle - Pink Noise Integration Bridge
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Bidirectional integration between Free Energy Principle and pink noise modulation
 * WHY:  Stochastic FEP with 1/f noise provides optimal exploration-exploitation balance.
 *       Essential for robust inference under uncertainty and naturalistic neural variability.
 * HOW:  FEP precision modulates pink noise amplitude; pink noise perturbs FEP beliefs and actions;
 *       noise statistics inform FEP uncertainty estimates about environmental volatility.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * STOCHASTIC FREE ENERGY (Friston et al., 2016):
 * -----------------------------------------------
 * 1. Pink Noise as Optimal Perturbation:
 *    - 1/f spectrum matches cortical fluctuations
 *    - Balances exploration (high freq) and exploitation (low freq)
 *    - Natural timescale diversity for hierarchical inference
 *    - Reference: Friston et al. (2016) "Active inference and learning"
 *
 * 2. Precision-Weighted Stochasticity:
 *    - Low precision → high noise (explore uncertain states)
 *    - High precision → low noise (exploit confident beliefs)
 *    - Adaptive exploration via precision modulation
 *    - Reference: Parr & Friston (2019) "Precision and false perceptual inference"
 *
 * 3. Noise-Driven Temperature Control:
 *    - Pink noise amplitude ~ action selection temperature
 *    - Implements stochastic optimal control
 *    - Prevents premature convergence to local minima
 *
 * FEP → PINK NOISE PATHWAYS:
 * ---------------------------
 * - Low precision → increase noise amplitude (explore)
 * - High precision → decrease noise amplitude (exploit)
 * - Free energy → noise spectral slope modulation
 * - Expected free energy → action noise scaling
 *
 * PINK NOISE → FEP PATHWAYS:
 * ---------------------------
 * - Noise amplitude → environmental uncertainty estimate
 * - Noise variability → volatility tracking
 * - 1/f statistics → complexity priors
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PINK_NOISE_FEP_BRIDGE_H
#define NIMCP_PINK_NOISE_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "plasticity/noise/nimcp_pink_noise.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define PINK_NOISE_FEP_AMPLITUDE_MIN    0.001f  /**< Min noise amplitude */
#define PINK_NOISE_FEP_AMPLITUDE_MAX    0.5f    /**< Max noise amplitude */
#define PINK_NOISE_FEP_ALPHA_MIN        0.5f    /**< Min spectral slope */
#define PINK_NOISE_FEP_ALPHA_MAX        2.0f    /**< Max spectral slope */

/* ============================================================================
 * Structures
 * ============================================================================ */

typedef struct pink_noise_fep_bridge pink_noise_fep_bridge_t;

typedef struct {
    float precision_amplitude_gain;
    float free_energy_alpha_gain;
    float base_amplitude;
    float base_alpha;
    bool enable_precision_amplitude;
    bool enable_fe_spectral_modulation;
    bool enable_noise_uncertainty_feedback;
} pink_noise_fep_config_t;

typedef struct {
    float precision_value;
    float amplitude_scaling;
    float free_energy_value;
    float alpha_scaling;
    float effective_amplitude;
    float effective_alpha;
} pink_noise_fep_effects_t;

typedef struct {
    float noise_amplitude;
    float noise_alpha;
    float uncertainty_estimate;
    float volatility_estimate;
} pink_noise_fep_feedback_t;

typedef struct {
    uint64_t total_updates;
    float avg_amplitude;
    float avg_alpha;
    float avg_precision_scaling;
} pink_noise_fep_stats_t;

struct pink_noise_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    pink_noise_fep_config_t config;
    fep_system_t* fep_system;
    pink_noise_generator_t noise_generator;
    pink_noise_fep_effects_t fep_effects;
    pink_noise_fep_feedback_t noise_effects;
    pink_noise_fep_stats_t stats;
};

/* ============================================================================
 * API
 * ============================================================================ */

int pink_noise_fep_bridge_default_config(pink_noise_fep_config_t* config);
pink_noise_fep_bridge_t* pink_noise_fep_bridge_create(const pink_noise_fep_config_t* config);
void pink_noise_fep_bridge_destroy(pink_noise_fep_bridge_t* bridge);

int pink_noise_fep_bridge_connect_fep(pink_noise_fep_bridge_t* bridge, fep_system_t* fep);
int pink_noise_fep_bridge_connect_noise(pink_noise_fep_bridge_t* bridge, pink_noise_generator_t generator);
int pink_noise_fep_bridge_disconnect(pink_noise_fep_bridge_t* bridge);

float pink_noise_fep_compute_amplitude_from_precision(pink_noise_fep_bridge_t* bridge, float precision);
float pink_noise_fep_compute_alpha_from_free_energy(pink_noise_fep_bridge_t* bridge, float free_energy);
float pink_noise_fep_compute_uncertainty_from_noise(const pink_noise_fep_bridge_t* bridge);

int pink_noise_fep_bridge_update(pink_noise_fep_bridge_t* bridge, uint64_t delta_ms);
int pink_noise_fep_bridge_get_stats(const pink_noise_fep_bridge_t* bridge, pink_noise_fep_stats_t* stats);

int pink_noise_fep_bridge_connect_bio_async(pink_noise_fep_bridge_t* bridge);
int pink_noise_fep_bridge_disconnect_bio_async(pink_noise_fep_bridge_t* bridge);
bool pink_noise_fep_bridge_is_bio_async_connected(const pink_noise_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PINK_NOISE_FEP_BRIDGE_H */
