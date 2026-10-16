//=============================================================================
// nimcp_pink_noise_immune_bridge.h - Pink Noise Immune System Integration
//=============================================================================
/**
 * @file nimcp_pink_noise_immune_bridge.h
 * @brief Bidirectional integration between brain immune system and pink noise
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Connect pink noise modulation with immune system state
 * WHY:  Inflammation affects neural noise characteristics:
 *       - Fever reduces cortical noise precision
 *       - Cytokines modulate neural variability
 *       - Infection triggers compensatory noise changes
 *
 * HOW:  Immune state modulates noise amplitude and spectrum;
 *       Noise statistics inform immune system about neural health.
 *
 * BIOLOGICAL BASIS:
 * =================
 * - IL-1β reduces cortical gamma oscillations (Cunningham et al., 2012)
 * - TNF-α increases neural noise (Stellwagen & Bhattacharyya, 2014)
 * - Fever suppresses synaptic plasticity (energy conservation)
 * - Infection → reduced 1/f → more random (less optimal) dynamics
 *
 * IMMUNE → PINK NOISE PATHWAYS:
 * =============================
 * - IL-1β elevation → increase noise amplitude (disrupted precision)
 * - TNF-α elevation → whiten spectrum (shift toward α=0)
 * - IL-10 (anti-inflammatory) → restore normal noise profile
 * - Inflammation level → amplitude scaling
 *
 * PINK NOISE → IMMUNE PATHWAYS:
 * =============================
 * - Abnormal α (departure from 1.0) → stress signal to immune
 * - Excessive noise variance → potential seizure warning
 * - Loss of 1/f spectrum → metabolic dysfunction signal
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PINK_NOISE_IMMUNE_BRIDGE_H
#define NIMCP_PINK_NOISE_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>
#include "plasticity/noise/nimcp_pink_noise.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations (avoid circular dependencies)
//=============================================================================

// Forward declare brain_immune_system_t to avoid include
typedef struct brain_immune_system brain_immune_system_t;

//=============================================================================
// Constants
//=============================================================================

#define PINK_IMMUNE_MAX_CYTOKINES   6   /**< Maximum tracked cytokines */

//=============================================================================
// Cytokine Types (local mirror to avoid dependency)
//=============================================================================

/**
 * @brief Cytokine types affecting pink noise
 */
typedef enum {
    PINK_CYTOKINE_IL1 = 0,      /**< Pro-inflammatory, increases noise */
    PINK_CYTOKINE_IL6,          /**< Pro-inflammatory, variable effect */
    PINK_CYTOKINE_IL10,         /**< Anti-inflammatory, restores normal */
    PINK_CYTOKINE_TNF,          /**< Pro-inflammatory, whitens spectrum */
    PINK_CYTOKINE_IFN_GAMMA,    /**< Modulates neural activity */
    PINK_CYTOKINE_COUNT
} pink_immune_cytokine_t;

/**
 * @brief Inflammation levels
 */
typedef enum {
    PINK_INFLAMMATION_NONE = 0,
    PINK_INFLAMMATION_LOCAL,
    PINK_INFLAMMATION_REGIONAL,
    PINK_INFLAMMATION_SYSTEMIC,
    PINK_INFLAMMATION_STORM
} pink_inflammation_level_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Cytokine effect parameters
 */
typedef struct {
    float amplitude_factor;     /**< Multiplier for noise amplitude */
    float alpha_shift;          /**< Additive shift to spectral exponent */
    float threshold;            /**< Cytokine level to trigger effect */
} pink_cytokine_effect_t;

/**
 * @brief Immune bridge configuration
 */
typedef struct {
    // Cytokine effects
    pink_cytokine_effect_t cytokine_effects[PINK_CYTOKINE_COUNT];

    // Inflammation effects on noise
    float inflammation_amplitude_scale[5];  /**< Per-level amplitude scaling */
    float inflammation_alpha_shift[5];      /**< Per-level alpha shift */

    // Feedback to immune
    float abnormal_alpha_threshold;         /**< α deviation triggering immune */
    float variance_warning_threshold;       /**< Variance level for warning */
    float feedback_gain;                    /**< Strength of feedback signal */

    // Base parameters
    float base_amplitude;                   /**< Normal noise amplitude */
    float base_alpha;                       /**< Normal spectral exponent */

    bool enable_immune_modulation;          /**< Enable immune→noise */
    bool enable_noise_feedback;             /**< Enable noise→immune */
} pink_immune_config_t;

//=============================================================================
// State Structure
//=============================================================================

/**
 * @brief Current cytokine levels
 */
typedef struct {
    float levels[PINK_CYTOKINE_COUNT];
    pink_inflammation_level_t inflammation;
} pink_immune_state_t;

/**
 * @brief Computed effects on noise
 */
typedef struct {
    float amplitude_modifier;   /**< Combined amplitude effect (multiplicative) */
    float alpha_modifier;       /**< Combined alpha shift (additive) */
    float effective_amplitude;  /**< Final amplitude */
    float effective_alpha;      /**< Final alpha */
} pink_immune_effects_t;

/**
 * @brief Feedback signals to immune system
 */
typedef struct {
    float alpha_deviation;      /**< How far from target α */
    float variance_level;       /**< Current noise variance */
    float criticality_stress;   /**< Stress from criticality deviation */
    bool seizure_warning;       /**< Excessive variance detected */
} pink_immune_feedback_t;

/**
 * @brief Immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    pink_immune_config_t config;
    brain_immune_system_t* immune_system;
    pink_noise_generator_t noise_generator;

    pink_immune_state_t immune_state;
    pink_immune_effects_t effects;
    pink_immune_feedback_t feedback;

    // Statistics
    uint64_t update_count;
    float avg_amplitude_modifier;
    float avg_alpha_modifier;

    // Bio-async integration
    void* bio_ctx;} pink_immune_bridge_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;
    float avg_amplitude_modifier;
    float avg_alpha_modifier;
    float current_inflammation;
    uint32_t seizure_warnings;
} pink_immune_stats_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create pink noise immune bridge
 *
 * WHAT: Initialize bridge for immune-noise integration
 * WHY:  Enable bidirectional immune-noise modulation
 *
 * @param config Bridge configuration
 * @return Bridge handle or NULL on failure
 */
pink_immune_bridge_t* pink_immune_bridge_create(
    const pink_immune_config_t* config
);

/**
 * @brief Destroy immune bridge
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void pink_immune_bridge_destroy(pink_immune_bridge_t* bridge);

/**
 * @brief Get default configuration
 *
 * @return Default immune bridge configuration
 */
pink_immune_config_t pink_immune_bridge_default_config(void);

//=============================================================================
// Connection Functions
//=============================================================================

/**
 * @brief Connect to brain immune system
 *
 * @param bridge Immune bridge
 * @param immune Brain immune system
 * @return 0 on success, negative on error
 */
int pink_immune_bridge_connect_immune(
    pink_immune_bridge_t* bridge,
    brain_immune_system_t* immune
);

/**
 * @brief Connect to pink noise generator
 *
 * @param bridge Immune bridge
 * @param generator Pink noise generator
 * @return 0 on success, negative on error
 */
int pink_immune_bridge_connect_generator(
    pink_immune_bridge_t* bridge,
    pink_noise_generator_t generator
);

/**
 * @brief Disconnect from all systems
 *
 * @param bridge Immune bridge
 * @return 0 on success, negative on error
 */
int pink_immune_bridge_disconnect(pink_immune_bridge_t* bridge);

//=============================================================================
// Update Functions
//=============================================================================

/**
 * @brief Update immune state from connected immune system
 *
 * WHAT: Pull current cytokine levels and inflammation
 * WHY:  Keep bridge state synchronized
 *
 * @param bridge Immune bridge
 * @return 0 on success, negative on error
 */
int pink_immune_bridge_update_immune_state(pink_immune_bridge_t* bridge);

/**
 * @brief Set cytokine level manually
 *
 * @param bridge Immune bridge
 * @param cytokine Cytokine type
 * @param level Level (0-1 normalized)
 * @return 0 on success, negative on error
 */
int pink_immune_bridge_set_cytokine(
    pink_immune_bridge_t* bridge,
    pink_immune_cytokine_t cytokine,
    float level
);

/**
 * @brief Set inflammation level manually
 *
 * @param bridge Immune bridge
 * @param level Inflammation level
 * @return 0 on success, negative on error
 */
int pink_immune_bridge_set_inflammation(
    pink_immune_bridge_t* bridge,
    pink_inflammation_level_t level
);

/**
 * @brief Compute effects on noise parameters
 *
 * WHAT: Calculate amplitude and alpha modifiers from immune state
 * WHY:  Determine how immune affects noise
 *
 * @param bridge Immune bridge
 * @return 0 on success, negative on error
 */
int pink_immune_bridge_compute_effects(pink_immune_bridge_t* bridge);

/**
 * @brief Update feedback from noise statistics
 *
 * WHAT: Compute signals to send back to immune system
 * WHY:  Inform immune about neural state
 *
 * @param bridge Immune bridge
 * @param measured_alpha Current measured spectral exponent
 * @param variance Current noise variance
 * @return 0 on success, negative on error
 */
int pink_immune_bridge_compute_feedback(
    pink_immune_bridge_t* bridge,
    float measured_alpha,
    float variance
);

/**
 * @brief Full update cycle
 *
 * WHAT: Update immune state, compute effects, compute feedback
 * WHY:  Single call for complete update
 *
 * @param bridge Immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success, negative on error
 */
int pink_immune_bridge_update(
    pink_immune_bridge_t* bridge,
    uint64_t delta_ms
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get current amplitude modifier
 *
 * @param bridge Immune bridge
 * @return Amplitude modifier (multiply with base amplitude)
 */
float pink_immune_bridge_get_amplitude_modifier(
    const pink_immune_bridge_t* bridge
);

/**
 * @brief Get current alpha modifier
 *
 * @param bridge Immune bridge
 * @return Alpha modifier (add to base alpha)
 */
float pink_immune_bridge_get_alpha_modifier(
    const pink_immune_bridge_t* bridge
);

/**
 * @brief Get effective amplitude
 *
 * @param bridge Immune bridge
 * @return Effective noise amplitude after immune modulation
 */
float pink_immune_bridge_get_effective_amplitude(
    const pink_immune_bridge_t* bridge
);

/**
 * @brief Get effective alpha
 *
 * @param bridge Immune bridge
 * @return Effective spectral exponent after immune modulation
 */
float pink_immune_bridge_get_effective_alpha(
    const pink_immune_bridge_t* bridge
);

/**
 * @brief Get current feedback state
 *
 * @param bridge Immune bridge
 * @param feedback Output feedback state
 * @return 0 on success, negative on error
 */
int pink_immune_bridge_get_feedback(
    const pink_immune_bridge_t* bridge,
    pink_immune_feedback_t* feedback
);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Immune bridge
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int pink_immune_bridge_get_stats(
    const pink_immune_bridge_t* bridge,
    pink_immune_stats_t* stats
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Immune bridge
 * @return 0 on success, negative on error
 */
int pink_immune_bridge_reset(pink_immune_bridge_t* bridge);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async messaging system
 * WHY:  Enable inter-module communication
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_PINK_NOISE
 *
 * @param bridge Immune bridge
 * @return 0 on success, negative on error
 */
int pink_immune_bridge_connect_bio_async(pink_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Immune bridge
 * @return 0 on success, negative on error
 */
int pink_immune_bridge_disconnect_bio_async(pink_immune_bridge_t* bridge);

/**
 * @brief Check if connected to bio-async
 *
 * @param bridge Immune bridge
 * @return true if connected, false otherwise
 */
bool pink_immune_bridge_is_bio_async_connected(
    const pink_immune_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PINK_NOISE_IMMUNE_BRIDGE_H
