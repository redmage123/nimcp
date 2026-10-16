/**
 * @file nimcp_heterosynaptic_pink_noise_bridge.h
 * @brief Heterosynaptic Plasticity-Pink Noise Integration Bridge
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Bidirectional integration between pink noise and heterosynaptic plasticity
 * WHY:  Biological synaptic competition exhibits stochastic fluctuations with 1/f spectrum
 * HOW:  Pink noise modulates competition strength, depression factors, and spatial extent
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * PINK NOISE IN SYNAPTIC COMPETITION:
 * -----------------------------------
 * 1. Stochastic Synaptic Dynamics (Faisal et al., 2008):
 *    - Synaptic strength fluctuates with 1/f spectrum
 *    - Competition outcomes aren't deterministic but probabilistic
 *    - Noise enables weak synapses to occasionally compete successfully
 *    - Prevents premature convergence to local optima
 *    - Reference: Faisal et al. (2008) "Noise in the nervous system"
 *
 * 2. Retrograde Signaling Variability (Regehr et al., 2009):
 *    - Endocannabinoid release is stochastic
 *    - Nitric oxide diffusion has spatial variability
 *    - Competition radius fluctuates naturally (~10-20 μm ± noise)
 *    - Depression strength varies trial-to-trial
 *    - Reference: Regehr et al. (2009) "Short-term synaptic plasticity"
 *
 * 3. Multi-Timescale Competition (Abbott & Regehr, 2004):
 *    - Fast fluctuations (ms-scale): Individual competition events
 *    - Slow fluctuations (min-scale): Sustained competition bias
 *    - 1/f spectrum captures both timescales naturally
 *    - Enables exploration while maintaining stability
 *    - Reference: Abbott & Regehr (2004) "Synaptic computation"
 *
 * 4. Spatial Heterogeneity (Magee & Johnston, 2005):
 *    - Dendritic branches have varying excitability
 *    - Local protein synthesis varies spatially
 *    - Competition strength differs by dendritic location
 *    - Pink noise models this spatial variability
 *    - Reference: Magee & Johnston (2005) "Plasticity in dendrites"
 *
 * PINK NOISE → HETEROSYNAPTIC PATHWAYS:
 * -------------------------------------
 * 1. Competition Strength Modulation:
 *    - Depression factor: η_effective = η_base × (1 + α_comp × noise)
 *    - Allows occasional strong/weak competition events
 *    - Prevents rigid winner-take-all dynamics
 *    - Typical: α_comp = 0.15 (±15% fluctuation)
 *
 * 2. Spatial Radius Fluctuation:
 *    - Competition radius: r_effective = r_base × (1 + α_radius × noise)
 *    - Models stochastic diffusion of retrograde signals
 *    - Some synapses occasionally included/excluded from competition
 *    - Typical: α_radius = 0.10 (±10% fluctuation)
 *
 * 3. Winner-Take-All Threshold Variation:
 *    - WTA threshold: θ_effective = θ_base + α_wta × noise
 *    - Competition outcome becomes probabilistic
 *    - Weak synapses occasionally win when noise favorable
 *    - Prevents premature stabilization
 *
 * 4. Depression Delay Jitter:
 *    - Heterosynaptic LTD delay: d_effective = d_base × (1 + α_delay × noise)
 *    - Models biochemical pathway variability
 *    - Typical delay: 500ms ± 100ms
 *
 * HETEROSYNAPTIC → PINK NOISE PATHWAYS:
 * -------------------------------------
 * 1. Competition Rate → Noise Amplitude:
 *    - High competition frequency → increase noise amplitude
 *    - Excessive competition triggers more stochasticity
 *    - Helps escape pathological winner-take-all states
 *
 * 2. Synaptic Saturation → Noise Boost:
 *    - When weights approaching saturation → increase noise
 *    - Enables continued exploration despite saturation
 *    - Prevents complete convergence
 *
 * 3. Balanced Competition → Noise Reduction:
 *    - Healthy competition dynamics → maintain low noise
 *    - Stable heterosynaptic patterns don't need perturbation
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              HETEROSYNAPTIC-PINK NOISE BRIDGE                              ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   PINK NOISE → HETEROSYNAPTIC:                                            ║
 * ║   ┌──────────────────┐                                                    ║
 * ║   │ PINK NOISE       │                                                    ║
 * ║   │ 1/f spectrum     │  ──→  Competition Strength Modulation              ║
 * ║   │ α=1.0, amp=0.05  │  ──→  Depression Factor Scaling                    ║
 * ║   │ Multi-timescale  │  ──→  Radius Fluctuation                           ║
 * ║   │ Stochastic       │  ──→  WTA Threshold Variation                      ║
 * ║   └──────────────────┘                                                    ║
 * ║                                                                            ║
 * ║   MODULATION FORMULAS:                                                    ║
 * ║   η_eff = η_base × (1 + 0.15 × noise)     [Competition strength]         ║
 * ║   r_eff = r_base × (1 + 0.10 × noise)     [Spatial radius]               ║
 * ║   θ_eff = θ_base + 0.05 × noise           [WTA threshold]                ║
 * ║   d_eff = d_base × (1 + 0.20 × noise)     [Depression delay]             ║
 * ║                                                                            ║
 * ║   HETEROSYNAPTIC → PINK NOISE:                                            ║
 * ║   ┌──────────────────┐                                                    ║
 * ║   │ HIGH COMPETITION │ ──→ Increase Noise Amplitude (adaptive)            ║
 * ║   │ SATURATION       │ ──→ Boost Noise (exploration)                      ║
 * ║   │ BALANCED         │ ──→ Reduce Noise (stability)                       ║
 * ║   └──────────────────┘                                                    ║
 * ║                                                                            ║
 * ║   EFFECTIVE RANGES:                                                       ║
 * ║   Competition: η ∈ [0.34, 0.46] for η_base=0.4, α=0.15                   ║
 * ║   Radius:      r ∈ [13.5, 16.5]μm for r_base=15μm, α=0.10                ║
 * ║   Threshold:   θ ∈ [0.65, 0.75] for θ_base=0.7, α=0.05                   ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * INTEGRATION EXAMPLE:
 * ```c
 * // Create heterosynaptic system and pink noise generator
 * hetero_system_t* hetero = hetero_create(&hetero_cfg, 1000);
 * pink_noise_generator_t pink = pink_noise_create(&pink_cfg);
 *
 * // Create bridge
 * hetero_pink_noise_config_t bridge_cfg;
 * hetero_pink_noise_default_config(&bridge_cfg);
 * hetero_pink_noise_bridge_t* bridge = hetero_pink_noise_bridge_create(
 *     &bridge_cfg, hetero, pink);
 *
 * // Enable modulation
 * hetero_pink_noise_enable(bridge);
 *
 * // Update loop
 * for (int t = 0; t < 10000; t++) {
 *     hetero_pink_noise_bridge_update(bridge, 1);  // 1ms timestep
 *
 *     // Get modulated parameters
 *     float comp_strength = hetero_pink_noise_get_effective_competition(bridge);
 *     float radius = hetero_pink_noise_get_effective_radius(bridge);
 *
 *     // Apply heterosynaptic plasticity with pink noise modulation
 *     hetero_apply_depression(hetero, synapse_id, ltp_amount, t);
 * }
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HETEROSYNAPTIC_PINK_NOISE_BRIDGE_H
#define NIMCP_HETEROSYNAPTIC_PINK_NOISE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "plasticity/heterosynaptic/nimcp_heterosynaptic.h"
#include "plasticity/noise/nimcp_pink_noise.h"

/* Thread safety */
#include "utils/platform/nimcp_platform_mutex.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Default modulation strengths */
#define HETERO_PINK_DEFAULT_COMPETITION_ALPHA   0.15f   /**< ±15% competition modulation */
#define HETERO_PINK_DEFAULT_RADIUS_ALPHA        0.10f   /**< ±10% radius modulation */
#define HETERO_PINK_DEFAULT_WTA_ALPHA           0.05f   /**< ±5% WTA threshold modulation */
#define HETERO_PINK_DEFAULT_DELAY_ALPHA         0.20f   /**< ±20% delay modulation */

/* Feedback thresholds */
#define HETERO_PINK_HIGH_COMPETITION_THRESHOLD  5.0f    /**< Competition rate > 5x baseline */
#define HETERO_PINK_SATURATION_THRESHOLD        0.90f   /**< Weight > 90% of max */
#define HETERO_PINK_BALANCED_COMPETITION_MIN    0.50f   /**< Minimum balanced ratio */
#define HETERO_PINK_BALANCED_COMPETITION_MAX    2.00f   /**< Maximum balanced ratio */

/* Adaptive noise parameters */
#define HETERO_PINK_ADAPTIVE_GAIN               0.10f   /**< Adaptive amplitude adjustment rate */
#define HETERO_PINK_MIN_AMPLITUDE               0.01f   /**< Minimum noise amplitude */
#define HETERO_PINK_MAX_AMPLITUDE               0.30f   /**< Maximum noise amplitude */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Pink noise modulation parameters
 *
 * WHAT: Controls how pink noise modulates heterosynaptic parameters
 * WHY:  Configurable stochastic influence on competition dynamics
 */
typedef struct {
    /* Modulation strengths (0-1 range) */
    float competition_alpha;        /**< Competition strength modulation [0-1] */
    float radius_alpha;             /**< Spatial radius modulation [0-1] */
    float wta_threshold_alpha;      /**< WTA threshold modulation [0-1] */
    float delay_alpha;              /**< Depression delay modulation [0-1] */

    /* Feature enables */
    bool enable_competition_noise;  /**< Modulate competition strength */
    bool enable_radius_noise;       /**< Modulate competition radius */
    bool enable_wta_noise;          /**< Modulate WTA threshold */
    bool enable_delay_noise;        /**< Modulate depression delay */
    bool enable_adaptive_amplitude; /**< Adapt noise based on competition state */
} hetero_pink_noise_modulation_t;

/**
 * @brief Current noise-modulated heterosynaptic parameters
 *
 * WHAT: Effective parameters after pink noise modulation
 * WHY:  Track actual competition dynamics with stochastic influence
 */
typedef struct {
    /* Base parameters */
    float base_competition;         /**< Base depression factor */
    float base_radius;              /**< Base competition radius (μm) */
    float base_wta_threshold;       /**< Base WTA threshold */
    float base_delay;               /**< Base depression delay (ms) */

    /* Current noise values */
    float current_noise;            /**< Latest pink noise sample */

    /* Modulated parameters */
    float effective_competition;    /**< Competition × (1 + α × noise) */
    float effective_radius;         /**< Radius × (1 + α × noise) */
    float effective_wta_threshold;  /**< Threshold + α × noise */
    float effective_delay;          /**< Delay × (1 + α × noise) */
} hetero_pink_noise_state_t;

/**
 * @brief Heterosynaptic competition statistics
 *
 * WHAT: Metrics describing competition dynamics
 * WHY:  Inform adaptive noise amplitude adjustments
 */
typedef struct {
    /* Competition metrics */
    float competition_rate;         /**< Competitions per second */
    float avg_winner_strength;      /**< Average winner weight */
    float avg_competitors;          /**< Average synapses per competition */
    float saturation_fraction;      /**< Fraction of synapses near w_max */

    /* Indicators */
    bool high_competition_detected; /**< Competition rate excessive */
    bool saturation_detected;       /**< Many synapses saturated */
    bool balanced_competition;      /**< Healthy competition dynamics */
} hetero_pink_noise_feedback_t;

/**
 * @brief Heterosynaptic-pink noise bridge configuration
 */
typedef struct {
    /* Modulation parameters */
    hetero_pink_noise_modulation_t modulation;

    /* Base parameters (for restoration) */
    float base_competition;
    float base_radius;
    float base_wta_threshold;
    float base_delay;

    /* Feedback configuration */
    bool enable_feedback;           /**< Enable hetero→noise feedback */
    float feedback_gain;            /**< Adaptive adjustment strength [0-1] */
    float high_competition_threshold;
    float saturation_threshold;

    /* Pink noise configuration */
    pink_noise_config_t pink_config;
} hetero_pink_noise_config_t;

/**
 * @brief Complete heterosynaptic-pink noise bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    hetero_system_t* hetero_system;
    pink_noise_generator_t pink_generator;

    /* Configuration */
    hetero_pink_noise_config_t config;

    /* Current state */
    hetero_pink_noise_state_t noise_state;
    hetero_pink_noise_feedback_t feedback;

    /* Statistics */
    uint64_t total_updates;
    uint32_t noise_samples_generated;
    float avg_effective_competition;
    float avg_effective_radius;

    /* Adaptive state */
    float adaptive_amplitude;       /**< Current adaptive noise amplitude */

    /* Integration flags */
    bool enabled;                   /**< Bridge active */
    bool pink_generator_owned;      /**< We created the generator */

    nimcp_platform_mutex_t mutex;
} hetero_pink_noise_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * WHAT: Get default heterosynaptic-pink noise bridge configuration
 * WHY:  Provide biologically plausible starting parameters
 * HOW:  Return struct with empirically validated defaults
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int hetero_pink_noise_default_config(hetero_pink_noise_config_t* config);

/**
 * WHAT: Create heterosynaptic-pink noise bridge
 * WHY:  Initialize bidirectional integration
 * HOW:  Allocate bridge, connect systems, initialize noise generator
 *
 * @param config Bridge configuration (NULL for defaults)
 * @param hetero_system Heterosynaptic plasticity system
 * @param pink_generator Pink noise generator (NULL to create internal)
 * @return New bridge or NULL on failure
 */
hetero_pink_noise_bridge_t* hetero_pink_noise_bridge_create(
    const hetero_pink_noise_config_t* config,
    hetero_system_t* hetero_system,
    pink_noise_generator_t pink_generator
);

/**
 * WHAT: Destroy heterosynaptic-pink noise bridge
 * WHY:  Clean up resources
 * HOW:  Free structures, destroy owned pink generator if applicable
 *
 * @param bridge Bridge to destroy
 */
void hetero_pink_noise_bridge_destroy(hetero_pink_noise_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * WHAT: Connect to heterosynaptic system
 * WHY:  Attach to existing heterosynaptic plasticity
 * HOW:  Store handle, extract base parameters
 *
 * @param bridge Pink noise bridge
 * @param hetero_system Heterosynaptic system
 * @return 0 on success, -1 on error
 */
int hetero_pink_noise_connect_hetero(
    hetero_pink_noise_bridge_t* bridge,
    hetero_system_t* hetero_system
);

/**
 * WHAT: Connect to pink noise generator
 * WHY:  Attach to existing noise source
 * HOW:  Store handle, validate configuration
 *
 * @param bridge Pink noise bridge
 * @param pink_generator Pink noise generator
 * @return 0 on success, -1 on error
 */
int hetero_pink_noise_connect_generator(
    hetero_pink_noise_bridge_t* bridge,
    pink_noise_generator_t pink_generator
);

/**
 * WHAT: Disconnect from all systems
 * WHY:  Clean shutdown without destroying bridge
 * HOW:  Clear handles, restore base parameters
 *
 * @param bridge Pink noise bridge
 * @return 0 on success
 */
int hetero_pink_noise_disconnect(hetero_pink_noise_bridge_t* bridge);

/* ============================================================================
 * Control API
 * ============================================================================ */

/**
 * WHAT: Enable pink noise modulation
 * WHY:  Start applying stochastic fluctuations to heterosynaptic parameters
 * HOW:  Set enabled flag, start generating noise
 *
 * @param bridge Pink noise bridge
 * @return 0 on success, -1 on error
 */
int hetero_pink_noise_enable(hetero_pink_noise_bridge_t* bridge);

/**
 * WHAT: Disable pink noise modulation
 * WHY:  Stop stochastic modulation, restore deterministic behavior
 * HOW:  Clear enabled flag, restore base parameters
 *
 * @param bridge Pink noise bridge
 * @return 0 on success
 */
int hetero_pink_noise_disable(hetero_pink_noise_bridge_t* bridge);

/**
 * WHAT: Check if bridge is enabled
 *
 * @param bridge Pink noise bridge
 * @return true if enabled, false otherwise
 */
bool hetero_pink_noise_is_enabled(const hetero_pink_noise_bridge_t* bridge);

/* ============================================================================
 * Pink Noise → Heterosynaptic API
 * ============================================================================ */

/**
 * WHAT: Apply pink noise modulation to heterosynaptic parameters
 * WHY:  Update effective competition parameters with stochastic fluctuations
 * HOW:  Sample pink noise, compute modulated values
 *
 * @param bridge Pink noise bridge
 * @return 0 on success, -1 on error
 */
int hetero_pink_noise_apply_modulation(hetero_pink_noise_bridge_t* bridge);

/**
 * WHAT: Get effective competition strength with noise modulation
 * WHY:  Current depression factor for heterosynaptic updates
 * HOW:  η_effective = η_base × (1 + α_comp × noise)
 *
 * @param bridge Pink noise bridge
 * @return Effective competition strength [0-1]
 */
float hetero_pink_noise_get_effective_competition(
    const hetero_pink_noise_bridge_t* bridge
);

/**
 * WHAT: Get effective competition radius with noise modulation
 * WHY:  Current spatial extent of competition
 * HOW:  r_effective = r_base × (1 + α_radius × noise)
 *
 * @param bridge Pink noise bridge
 * @return Effective radius in micrometers
 */
float hetero_pink_noise_get_effective_radius(
    const hetero_pink_noise_bridge_t* bridge
);

/**
 * WHAT: Get effective WTA threshold with noise modulation
 * WHY:  Current threshold for winner-take-all competition
 * HOW:  θ_effective = θ_base + α_wta × noise
 *
 * @param bridge Pink noise bridge
 * @return Effective WTA threshold [0-1]
 */
float hetero_pink_noise_get_effective_wta_threshold(
    const hetero_pink_noise_bridge_t* bridge
);

/**
 * WHAT: Get effective depression delay with noise modulation
 * WHY:  Current time delay for heterosynaptic LTD
 * HOW:  d_effective = d_base × (1 + α_delay × noise)
 *
 * @param bridge Pink noise bridge
 * @return Effective delay in milliseconds
 */
float hetero_pink_noise_get_effective_delay(
    const hetero_pink_noise_bridge_t* bridge
);

/* ============================================================================
 * Heterosynaptic → Pink Noise API (Feedback)
 * ============================================================================ */

/**
 * WHAT: Update feedback from heterosynaptic competition state
 * WHY:  Adapt noise amplitude based on competition dynamics
 * HOW:  Compute competition metrics, adjust noise if needed
 *
 * @param bridge Pink noise bridge
 * @return 0 on success, -1 on error
 */
int hetero_pink_noise_update_feedback(hetero_pink_noise_bridge_t* bridge);

/**
 * WHAT: Get competition feedback state
 * WHY:  Monitor heterosynaptic health and noise adaptation
 * HOW:  Return current feedback metrics
 *
 * @param bridge Pink noise bridge
 * @param feedback Output feedback structure
 * @return 0 on success, -1 on error
 */
int hetero_pink_noise_get_feedback(
    const hetero_pink_noise_bridge_t* bridge,
    hetero_pink_noise_feedback_t* feedback
);

/**
 * WHAT: Adapt noise amplitude based on competition state
 * WHY:  Increase noise when competition pathological, decrease when healthy
 * HOW:  Adjust amplitude within [min, max] bounds
 *
 * @param bridge Pink noise bridge
 * @param adaptation_factor Strength of adaptation [-1, 1]
 * @return New amplitude after adaptation
 */
float hetero_pink_noise_adapt_amplitude(
    hetero_pink_noise_bridge_t* bridge,
    float adaptation_factor
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * WHAT: Update heterosynaptic-pink noise bridge (both directions)
 * WHY:  Advance coupled state machine
 * HOW:  Sample noise, apply modulation, update feedback, adapt if enabled
 *
 * @param bridge Pink noise bridge
 * @param delta_ms Time since last update (milliseconds)
 * @return 0 on success, -1 on error
 */
int hetero_pink_noise_bridge_update(
    hetero_pink_noise_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * WHAT: Get current noise state
 *
 * @param bridge Pink noise bridge
 * @param state Output noise state
 * @return 0 on success, -1 on error
 */
int hetero_pink_noise_get_state(
    const hetero_pink_noise_bridge_t* bridge,
    hetero_pink_noise_state_t* state
);

/**
 * WHAT: Get current adaptive amplitude
 *
 * @param bridge Pink noise bridge
 * @return Current noise amplitude
 */
float hetero_pink_noise_get_adaptive_amplitude(
    const hetero_pink_noise_bridge_t* bridge
);

/**
 * WHAT: Get bridge statistics
 *
 * @param bridge Pink noise bridge
 * @param total_updates Output: total update calls
 * @param noise_samples Output: noise samples generated
 * @param avg_competition Output: average effective competition
 * @param avg_radius Output: average effective radius
 * @return 0 on success
 */
int hetero_pink_noise_get_statistics(
    const hetero_pink_noise_bridge_t* bridge,
    uint64_t* total_updates,
    uint32_t* noise_samples,
    float* avg_competition,
    float* avg_radius
);

/**
 * WHAT: Reset bridge state
 * WHY:  Clear statistics, restart noise sequence
 * HOW:  Reset counters, reseed pink generator
 *
 * @param bridge Pink noise bridge
 * @return 0 on success
 */
int hetero_pink_noise_reset(hetero_pink_noise_bridge_t* bridge);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * WHAT: Connect to bio-async router
 * WHY:  Enable inter-module messaging for noise modulation events
 * HOW:  Register with bio_router using BIO_MODULE_HETEROSYNAPTIC_PINK_NOISE
 *
 * @param bridge Pink noise bridge
 * @return 0 on success, -1 on error
 */
int hetero_pink_noise_connect_bio_async(hetero_pink_noise_bridge_t* bridge);

/**
 * WHAT: Disconnect from bio-async router
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge Pink noise bridge
 * @return 0 on success
 */
int hetero_pink_noise_disconnect_bio_async(hetero_pink_noise_bridge_t* bridge);

/**
 * WHAT: Check if bio-async is connected
 *
 * @param bridge Pink noise bridge
 * @return true if connected
 */
bool hetero_pink_noise_is_bio_async_connected(
    const hetero_pink_noise_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HETEROSYNAPTIC_PINK_NOISE_BRIDGE_H */
