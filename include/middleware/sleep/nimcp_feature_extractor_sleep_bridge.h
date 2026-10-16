/**
 * @file nimcp_feature_extractor_sleep_bridge.h
 * @brief Sleep-Feature Extractor Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Bidirectional integration between sleep/wake system and feature extractor
 * WHY:  Sleep states affect feature detection thresholds, extraction sensitivity, and priorities
 * HOW:  Sleep state modulates detection parameters and feature selection
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SLEEP → FEATURE EXTRACTION PATHWAYS:
 * -------------------------------------
 * 1. Sensory Processing During Sleep (Tononi & Massimini, 2008):
 *    - AWAKE: Full feature extraction across all modalities
 *    - DROWSY: Reduced sensitivity (80%) - attentional lapses
 *    - NREM: Minimal extraction (30%) - reduced sensory processing
 *    - Deep NREM: Very low extraction (10%) - cortical offline
 *    - REM: Selective extraction (50%) - internally driven
 *    - Reference: "Why does consciousness fade in early sleep?"
 *
 * 2. Feature Detection Thresholds and Arousal:
 *    - AWAKE: Low thresholds (high sensitivity)
 *    - DROWSY: Moderately raised thresholds
 *    - NREM: High thresholds (reduced responsiveness)
 *    - Deep NREM: Very high thresholds (near-complete gating)
 *    - REM: Variable thresholds (emotional bias)
 *
 * 3. Extraction Sensitivity by Sleep State:
 *    - AWAKE: 100% sensitivity
 *    - DROWSY: 80% sensitivity
 *    - LIGHT_NREM: 30% sensitivity
 *    - DEEP_NREM: 10% sensitivity
 *    - REM: 50% sensitivity
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                SLEEP-FEATURE EXTRACTOR INTEGRATION BRIDGE                  ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   SLEEP STATE      Detection   Extraction   Feature     Effect            ║
 * ║                    Threshold   Sensitivity  Priority                      ║
 * ║   ─────────────────────────────────────────────────────────────────────   ║
 * ║   AWAKE            0.1         1.0          All         Full extraction   ║
 * ║   DROWSY           0.15        0.8          High        Reduced sense     ║
 * ║   LIGHT_NREM       0.3         0.3          Critical    Minimal extract   ║
 * ║   DEEP_NREM        0.5         0.1          None        Near-offline      ║
 * ║   REM              0.2         0.5          Emotional   Selective extract ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_FEATURE_EXTRACTOR_SLEEP_BRIDGE_H
#define NIMCP_FEATURE_EXTRACTOR_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "middleware/features/nimcp_feature_extractor.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants - Sleep State Feature Extractor Modulation
 * ============================================================================ */

/* Detection threshold by sleep state */
#define FEATURE_EXTRACTOR_SLEEP_THRESHOLD_AWAKE         0.1f   /**< Low threshold */
#define FEATURE_EXTRACTOR_SLEEP_THRESHOLD_DROWSY        0.15f  /**< Slightly raised */
#define FEATURE_EXTRACTOR_SLEEP_THRESHOLD_LIGHT_NREM    0.3f   /**< High threshold */
#define FEATURE_EXTRACTOR_SLEEP_THRESHOLD_DEEP_NREM     0.5f   /**< Very high threshold */
#define FEATURE_EXTRACTOR_SLEEP_THRESHOLD_REM           0.2f   /**< Moderate threshold */

/* Extraction sensitivity by sleep state */
#define FEATURE_EXTRACTOR_SLEEP_SENSITIVITY_AWAKE       1.0f   /**< Full sensitivity */
#define FEATURE_EXTRACTOR_SLEEP_SENSITIVITY_DROWSY      0.8f   /**< Reduced sensitivity */
#define FEATURE_EXTRACTOR_SLEEP_SENSITIVITY_LIGHT_NREM  0.3f   /**< Minimal sensitivity */
#define FEATURE_EXTRACTOR_SLEEP_SENSITIVITY_DEEP_NREM   0.1f   /**< Very low sensitivity */
#define FEATURE_EXTRACTOR_SLEEP_SENSITIVITY_REM         0.5f   /**< Moderate sensitivity */

/* Window duration modulation by sleep state */
#define FEATURE_EXTRACTOR_SLEEP_WINDOW_AWAKE            1.0f   /**< Normal window */
#define FEATURE_EXTRACTOR_SLEEP_WINDOW_DROWSY           1.2f   /**< Slightly longer */
#define FEATURE_EXTRACTOR_SLEEP_WINDOW_LIGHT_NREM       1.5f   /**< Extended window */
#define FEATURE_EXTRACTOR_SLEEP_WINDOW_DEEP_NREM        2.0f   /**< Long window */
#define FEATURE_EXTRACTOR_SLEEP_WINDOW_REM              1.3f   /**< Moderate window */

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Sleep-Feature Extractor bridge configuration
 */
typedef struct {
    bool enable_threshold_modulation;    /**< Enable detection threshold changes */
    bool enable_sensitivity_modulation;  /**< Enable extraction sensitivity changes */
    bool enable_window_modulation;       /**< Enable analysis window changes */
    float modulation_strength;           /**< Overall modulation strength (0-1) */
} feature_extractor_sleep_config_t;

/**
 * @brief Computed sleep effects on feature extractor
 */
typedef struct {
    float detection_threshold;           /**< Sleep-dependent detection threshold */
    float extraction_sensitivity_factor; /**< Multiply sensitivity by this */
    float window_duration_factor;        /**< Multiply window duration by this */
    sleep_state_t current_state;         /**< Current sleep state */
    float sleep_pressure;                /**< Current sleep pressure */
    bool extraction_enabled;             /**< False during deep sleep offline */
} feature_extractor_sleep_effects_t;

/**
 * @brief Sleep-Feature Extractor integration bridge
 */
typedef struct feature_extractor_sleep_bridge_struct* feature_extractor_sleep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * WHAT: Get default sleep-feature extractor bridge configuration
 * WHY:  Provide sensible defaults based on sensory processing research
 */
int feature_extractor_sleep_default_config(feature_extractor_sleep_config_t* config);

/**
 * WHAT: Create sleep-feature extractor bridge
 * WHY:  Initialize integration between sleep and feature extraction systems
 */
feature_extractor_sleep_bridge_t feature_extractor_sleep_bridge_create(
    const feature_extractor_sleep_config_t* config,
    sleep_system_t sleep_system
);

/**
 * WHAT: Destroy sleep-feature extractor bridge
 * WHY:  Clean up resources and unregister callback
 */
void feature_extractor_sleep_bridge_destroy(feature_extractor_sleep_bridge_t bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * WHAT: Update feature extractor effects from sleep system state
 * WHY:  Compute how current sleep state affects extraction parameters
 */
int feature_extractor_sleep_update(feature_extractor_sleep_bridge_t bridge);

/**
 * WHAT: Get modulated feature extractor parameters for current sleep state
 * WHY:  Apply sleep modulation to extraction operations
 */
int feature_extractor_sleep_get_effects(
    const feature_extractor_sleep_bridge_t bridge,
    feature_extractor_sleep_effects_t* effects
);

/**
 * WHAT: Get sleep-modulated detection threshold
 * WHY:  Determine threshold for feature detection in current sleep state
 */
float feature_extractor_sleep_get_threshold(
    const feature_extractor_sleep_bridge_t bridge
);

/**
 * WHAT: Get sleep-modulated extraction sensitivity
 * WHY:  Determine effective sensitivity for current sleep state
 */
float feature_extractor_sleep_get_sensitivity(
    const feature_extractor_sleep_bridge_t bridge
);

/**
 * WHAT: Get sleep-modulated analysis window duration
 * WHY:  Determine effective window for current sleep state
 */
float feature_extractor_sleep_get_window_duration(
    const feature_extractor_sleep_bridge_t bridge
);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

float feature_extractor_sleep_get_threshold_factor(sleep_state_t state);
float feature_extractor_sleep_get_sensitivity_factor(sleep_state_t state);
float feature_extractor_sleep_get_window_factor(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FEATURE_EXTRACTOR_SLEEP_BRIDGE_H */
