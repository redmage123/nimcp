/**
 * @file nimcp_population_coding_sleep_bridge.h
 * @brief Sleep-Population Coding Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Bidirectional integration between sleep/wake system and population coding
 * WHY:  Sleep states affect encoding precision, population synchrony, and coding fidelity
 * HOW:  Sleep state modulates encoding parameters and population dynamics
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SLEEP → POPULATION CODING PATHWAYS:
 * ------------------------------------
 * 1. Population Coding Precision (Tononi & Cirelli, 2014):
 *    - AWAKE: High precision, fine-grained population codes
 *    - DROWSY: Reduced precision (70%) - coarser encoding
 *    - NREM: Minimal precision (40%) - sparse representation
 *    - Deep NREM: Very low precision (20%) - global states only
 *    - REM: Moderate precision (60%) - creative recombination
 *    - Reference: "Sleep and synaptic homeostasis hypothesis"
 *
 * 2. Population Synchrony and Sleep:
 *    - AWAKE: Asynchronous coding for information capacity
 *    - DROWSY: Increased synchrony (transition)
 *    - NREM: High synchrony (slow waves, spindles)
 *    - Deep NREM: Maximum synchrony (0.5-4 Hz oscillations)
 *    - REM: Desynchronized (theta-dominated)
 *
 * 3. Encoding Precision by Sleep State:
 *    - AWAKE: 100% precision
 *    - DROWSY: 70% precision
 *    - LIGHT_NREM: 40% precision
 *    - DEEP_NREM: 20% precision
 *    - REM: 60% precision
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                SLEEP-POPULATION CODING INTEGRATION BRIDGE                  ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   SLEEP STATE      Encoding    Synchrony    Sparsity    Effect            ║
 * ║                    Precision   Threshold    Target                        ║
 * ║   ─────────────────────────────────────────────────────────────────────   ║
 * ║   AWAKE            1.0         0.5          0.1         Fine-grained      ║
 * ║   DROWSY           0.7         0.6          0.15        Coarser codes     ║
 * ║   LIGHT_NREM       0.4         0.75         0.25        Sparse codes      ║
 * ║   DEEP_NREM        0.2         0.9          0.4         Global states     ║
 * ║   REM              0.6         0.4          0.2         Creative mode     ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_POPULATION_CODING_SLEEP_BRIDGE_H
#define NIMCP_POPULATION_CODING_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "middleware/encoding/nimcp_population_coding.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants - Sleep State Population Coding Modulation
 * ============================================================================ */

/* Encoding precision factor by sleep state */
#define POPULATION_CODING_SLEEP_PRECISION_AWAKE         1.0f   /**< Full precision */
#define POPULATION_CODING_SLEEP_PRECISION_DROWSY        0.7f   /**< Reduced precision */
#define POPULATION_CODING_SLEEP_PRECISION_LIGHT_NREM    0.4f   /**< Sparse encoding */
#define POPULATION_CODING_SLEEP_PRECISION_DEEP_NREM     0.2f   /**< Minimal precision */
#define POPULATION_CODING_SLEEP_PRECISION_REM           0.6f   /**< Creative mode */

/* Synchrony threshold by sleep state */
#define POPULATION_CODING_SLEEP_SYNCHRONY_AWAKE         0.5f   /**< Asynchronous */
#define POPULATION_CODING_SLEEP_SYNCHRONY_DROWSY        0.6f   /**< Increased sync */
#define POPULATION_CODING_SLEEP_SYNCHRONY_LIGHT_NREM    0.75f  /**< High sync */
#define POPULATION_CODING_SLEEP_SYNCHRONY_DEEP_NREM     0.9f   /**< Maximum sync */
#define POPULATION_CODING_SLEEP_SYNCHRONY_REM           0.4f   /**< Desynchronized */

/* Sparsity target by sleep state */
#define POPULATION_CODING_SLEEP_SPARSITY_AWAKE          0.1f   /**< Dense codes */
#define POPULATION_CODING_SLEEP_SPARSITY_DROWSY         0.15f  /**< Slightly sparse */
#define POPULATION_CODING_SLEEP_SPARSITY_LIGHT_NREM     0.25f  /**< Sparse codes */
#define POPULATION_CODING_SLEEP_SPARSITY_DEEP_NREM      0.4f   /**< Very sparse */
#define POPULATION_CODING_SLEEP_SPARSITY_REM            0.2f   /**< Moderate sparse */

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Sleep-Population Coding bridge configuration
 */
typedef struct {
    bool enable_precision_modulation;    /**< Enable encoding precision changes */
    bool enable_synchrony_modulation;    /**< Enable synchrony threshold changes */
    bool enable_sparsity_modulation;     /**< Enable sparsity target changes */
    float modulation_strength;           /**< Overall modulation strength (0-1) */
} population_coding_sleep_config_t;

/**
 * @brief Computed sleep effects on population coding
 */
typedef struct {
    float encoding_precision_factor;     /**< Multiply encoding precision by this */
    float synchrony_threshold;           /**< Sleep-dependent synchrony threshold */
    float sparsity_target;               /**< Sleep-dependent sparsity level */
    sleep_state_t current_state;         /**< Current sleep state */
    float sleep_pressure;                /**< Current sleep pressure */
    bool encoding_enabled;               /**< False during deep sleep offline */
} population_coding_sleep_effects_t;

/**
 * @brief Sleep-Population Coding integration bridge
 */
typedef struct population_coding_sleep_bridge_struct* population_coding_sleep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * WHAT: Get default sleep-population coding bridge configuration
 * WHY:  Provide sensible defaults based on synaptic homeostasis
 */
int population_coding_sleep_default_config(population_coding_sleep_config_t* config);

/**
 * WHAT: Create sleep-population coding bridge
 * WHY:  Initialize integration between sleep and population coding systems
 */
population_coding_sleep_bridge_t population_coding_sleep_bridge_create(
    const population_coding_sleep_config_t* config,
    sleep_system_t sleep_system
);

/**
 * WHAT: Destroy sleep-population coding bridge
 * WHY:  Clean up resources and unregister callback
 */
void population_coding_sleep_bridge_destroy(population_coding_sleep_bridge_t bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * WHAT: Update population coding effects from sleep system state
 * WHY:  Compute how current sleep state affects encoding parameters
 */
int population_coding_sleep_update(population_coding_sleep_bridge_t bridge);

/**
 * WHAT: Get modulated population coding parameters for current sleep state
 * WHY:  Apply sleep modulation to encoding operations
 */
int population_coding_sleep_get_effects(
    const population_coding_sleep_bridge_t bridge,
    population_coding_sleep_effects_t* effects
);

/**
 * WHAT: Get sleep-modulated encoding precision
 * WHY:  Determine effective encoding fidelity for current sleep state
 */
float population_coding_sleep_get_precision(
    const population_coding_sleep_bridge_t bridge
);

/**
 * WHAT: Get sleep-modulated synchrony threshold
 * WHY:  Determine synchrony detection threshold for current sleep state
 */
float population_coding_sleep_get_synchrony_threshold(
    const population_coding_sleep_bridge_t bridge
);

/**
 * WHAT: Get sleep-modulated sparsity target
 * WHY:  Determine target sparsity for current sleep state
 */
float population_coding_sleep_get_sparsity_target(
    const population_coding_sleep_bridge_t bridge
);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

float population_coding_sleep_get_precision_factor(sleep_state_t state);
float population_coding_sleep_get_synchrony_factor(sleep_state_t state);
float population_coding_sleep_get_sparsity_factor(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_POPULATION_CODING_SLEEP_BRIDGE_H */
