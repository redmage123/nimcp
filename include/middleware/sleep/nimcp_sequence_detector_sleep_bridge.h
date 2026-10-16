/**
 * @file nimcp_sequence_detector_sleep_bridge.h
 * @brief Sleep-Sequence Detector Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Bidirectional integration between sleep/wake system and sequence detector
 * WHY:  Sleep states affect pattern matching sensitivity, replay detection, and temporal tolerance
 * HOW:  Sleep state modulates matching parameters and sequence learning
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SLEEP → SEQUENCE DETECTION PATHWAYS:
 * -------------------------------------
 * 1. Hippocampal Replay During Sleep (Wilson & McNaughton, 1994):
 *    - AWAKE: Online sequence learning, forward replay
 *    - DROWSY: Transition to offline processing
 *    - LIGHT_NREM: Enhanced replay detection (ripples, spindles)
 *    - DEEP_NREM: Maximum replay activity (slow wave-coupled)
 *    - REM: Creative sequence recombination
 *    - Reference: "Reactivation of hippocampal ensemble memories"
 *
 * 2. Sequence Matching Sensitivity and Arousal:
 *    - AWAKE: High sensitivity (online learning)
 *    - DROWSY: Moderate sensitivity
 *    - LIGHT_NREM: Enhanced sensitivity (replay mode)
 *    - DEEP_NREM: Maximum sensitivity (consolidation focus)
 *    - REM: High sensitivity (creative combinations)
 *
 * 3. Temporal Tolerance by Sleep State:
 *    - AWAKE: Standard tolerance (precise matching)
 *    - DROWSY: Slightly relaxed (transition)
 *    - LIGHT_NREM: Relaxed tolerance (compressed replay)
 *    - DEEP_NREM: Maximum tolerance (slow wave integration)
 *    - REM: Moderate tolerance (creative flexibility)
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                SLEEP-SEQUENCE DETECTOR INTEGRATION BRIDGE                  ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   SLEEP STATE      Matching    Temporal     Replay      Effect            ║
 * ║                    Sensitivity Tolerance    Detect                        ║
 * ║   ─────────────────────────────────────────────────────────────────────   ║
 * ║   AWAKE            1.0         1.0          Yes         Online learning   ║
 * ║   DROWSY           0.9         1.2          Yes         Transition        ║
 * ║   LIGHT_NREM       1.2         1.5          Enhanced    Replay mode       ║
 * ║   DEEP_NREM        1.5         2.0          Maximum     Consolidation     ║
 * ║   REM              1.1         1.3          Yes         Creative mode     ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SEQUENCE_DETECTOR_SLEEP_BRIDGE_H
#define NIMCP_SEQUENCE_DETECTOR_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "middleware/patterns/nimcp_sequence_detector.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants - Sleep State Sequence Detector Modulation
 * ============================================================================ */

/* Matching sensitivity factor by sleep state */
#define SEQUENCE_DETECTOR_SLEEP_SENSITIVITY_AWAKE       1.0f   /**< Standard sensitivity */
#define SEQUENCE_DETECTOR_SLEEP_SENSITIVITY_DROWSY      0.9f   /**< Slightly reduced */
#define SEQUENCE_DETECTOR_SLEEP_SENSITIVITY_LIGHT_NREM  1.2f   /**< Enhanced for replay */
#define SEQUENCE_DETECTOR_SLEEP_SENSITIVITY_DEEP_NREM   1.5f   /**< Maximum for consolidation */
#define SEQUENCE_DETECTOR_SLEEP_SENSITIVITY_REM         1.1f   /**< Enhanced for creativity */

/* Temporal tolerance factor by sleep state */
#define SEQUENCE_DETECTOR_SLEEP_TOLERANCE_AWAKE         1.0f   /**< Standard tolerance */
#define SEQUENCE_DETECTOR_SLEEP_TOLERANCE_DROWSY        1.2f   /**< Slightly relaxed */
#define SEQUENCE_DETECTOR_SLEEP_TOLERANCE_LIGHT_NREM    1.5f   /**< Relaxed for replay */
#define SEQUENCE_DETECTOR_SLEEP_TOLERANCE_DEEP_NREM     2.0f   /**< Maximum tolerance */
#define SEQUENCE_DETECTOR_SLEEP_TOLERANCE_REM           1.3f   /**< Moderate tolerance */

/* Minimum strength threshold by sleep state */
#define SEQUENCE_DETECTOR_SLEEP_MIN_STRENGTH_AWAKE      0.5f   /**< Standard threshold */
#define SEQUENCE_DETECTOR_SLEEP_MIN_STRENGTH_DROWSY     0.45f  /**< Slightly lowered */
#define SEQUENCE_DETECTOR_SLEEP_MIN_STRENGTH_LIGHT_NREM 0.4f   /**< Lowered for replay */
#define SEQUENCE_DETECTOR_SLEEP_MIN_STRENGTH_DEEP_NREM  0.3f   /**< Minimum for consolidation */
#define SEQUENCE_DETECTOR_SLEEP_MIN_STRENGTH_REM        0.4f   /**< Moderate threshold */

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Sleep-Sequence Detector bridge configuration
 */
typedef struct {
    bool enable_sensitivity_modulation;  /**< Enable matching sensitivity changes */
    bool enable_tolerance_modulation;    /**< Enable temporal tolerance changes */
    bool enable_threshold_modulation;    /**< Enable minimum strength changes */
    bool enable_replay_enhancement;      /**< Enable enhanced replay detection in NREM */
    float modulation_strength;           /**< Overall modulation strength (0-1) */
} sequence_detector_sleep_config_t;

/**
 * @brief Computed sleep effects on sequence detector
 */
typedef struct {
    float matching_sensitivity_factor;   /**< Multiply sensitivity by this */
    float temporal_tolerance_factor;     /**< Multiply tolerance by this */
    float min_strength_threshold;        /**< Sleep-dependent minimum strength */
    bool replay_detection_enhanced;      /**< Enhanced replay mode in NREM */
    sleep_state_t current_state;         /**< Current sleep state */
    float sleep_pressure;                /**< Current sleep pressure */
    bool detection_enabled;              /**< Always true (even in deep sleep) */
} sequence_detector_sleep_effects_t;

/**
 * @brief Sleep-Sequence Detector integration bridge
 */
typedef struct sequence_detector_sleep_bridge_struct* sequence_detector_sleep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * WHAT: Get default sleep-sequence detector bridge configuration
 * WHY:  Provide sensible defaults based on hippocampal replay research
 */
int sequence_detector_sleep_default_config(sequence_detector_sleep_config_t* config);

/**
 * WHAT: Create sleep-sequence detector bridge
 * WHY:  Initialize integration between sleep and sequence detection systems
 */
sequence_detector_sleep_bridge_t sequence_detector_sleep_bridge_create(
    const sequence_detector_sleep_config_t* config,
    sleep_system_t sleep_system
);

/**
 * WHAT: Destroy sleep-sequence detector bridge
 * WHY:  Clean up resources and unregister callback
 */
void sequence_detector_sleep_bridge_destroy(sequence_detector_sleep_bridge_t bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * WHAT: Update sequence detector effects from sleep system state
 * WHY:  Compute how current sleep state affects detection parameters
 */
int sequence_detector_sleep_update(sequence_detector_sleep_bridge_t bridge);

/**
 * WHAT: Get modulated sequence detector parameters for current sleep state
 * WHY:  Apply sleep modulation to detection operations
 */
int sequence_detector_sleep_get_effects(
    const sequence_detector_sleep_bridge_t bridge,
    sequence_detector_sleep_effects_t* effects
);

/**
 * WHAT: Get sleep-modulated matching sensitivity
 * WHY:  Determine effective sensitivity for current sleep state
 */
float sequence_detector_sleep_get_sensitivity(
    const sequence_detector_sleep_bridge_t bridge
);

/**
 * WHAT: Get sleep-modulated temporal tolerance
 * WHY:  Determine temporal flexibility for current sleep state
 */
float sequence_detector_sleep_get_tolerance(
    const sequence_detector_sleep_bridge_t bridge
);

/**
 * WHAT: Get sleep-modulated minimum strength threshold
 * WHY:  Determine minimum match strength for current sleep state
 */
float sequence_detector_sleep_get_min_strength(
    const sequence_detector_sleep_bridge_t bridge
);

/**
 * WHAT: Check if replay detection is enhanced
 * WHY:  Determine if in NREM replay mode
 */
bool sequence_detector_sleep_is_replay_enhanced(
    const sequence_detector_sleep_bridge_t bridge
);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

float sequence_detector_sleep_get_sensitivity_factor(sleep_state_t state);
float sequence_detector_sleep_get_tolerance_factor(sleep_state_t state);
float sequence_detector_sleep_get_min_strength_factor(sleep_state_t state);
bool sequence_detector_sleep_is_replay_state(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SEQUENCE_DETECTOR_SLEEP_BRIDGE_H */
