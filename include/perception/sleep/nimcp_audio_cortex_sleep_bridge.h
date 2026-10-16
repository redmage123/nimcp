/**
 * @file nimcp_audio_cortex_sleep_bridge.h
 * @brief Sleep-Audio Cortex Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Bidirectional integration between sleep/wake system and audio cortex
 * WHY:  Sleep states fundamentally alter auditory processing thresholds and speed
 * HOW:  Sleep state modulates auditory threshold, frequency selectivity, processing speed
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SLEEP → AUDIO CORTEX PATHWAYS:
 * --------------------------------
 * 1. Auditory Threshold Elevation During Sleep:
 *    - AWAKE: Normal hearing threshold (~0 dB SPL for 1kHz)
 *    - DROWSY: Slightly elevated threshold (reduced attention)
 *    - LIGHT_NREM: Elevated threshold (30-40 dB increase)
 *    - DEEP_NREM: Highest threshold (50+ dB increase, protective deafness)
 *    - REM: Variable threshold (dream-dependent, K-complex responses)
 *    - Reference: Velluti (1997) "Interactions between sleep and sensory physiology"
 *
 * 2. Frequency Selectivity Modulation:
 *    - AWAKE: Sharp frequency tuning in A1 neurons
 *    - DROWSY: Slightly broader tuning curves
 *    - NREM: Reduced frequency discrimination (coarser tuning)
 *    - REM: Maintained tuning but reduced external responsiveness
 *    - Reference: Edeline et al. (2001) "Auditory thalamus neurons during sleep"
 *
 * 3. Processing Speed Degradation:
 *    - AWAKE: Full temporal resolution (gap detection ~2-3ms)
 *    - DROWSY: Slower temporal processing
 *    - NREM: Reduced temporal acuity (longer integration windows)
 *    - REM: Internal auditory imagery but slower external processing
 *    - Reference: Issa & Wang (2008) "Sleep states and temporal processing"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              SLEEP-AUDIO CORTEX INTEGRATION BRIDGE                         ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   SLEEP STATE      Threshold  Frequency  Speed     Effect                 ║
 * ║   ─────────────────────────────────────────────────────────────────────   ║
 * ║   AWAKE            1.0        1.0        1.0       Full hearing           ║
 * ║   DROWSY           0.8        0.9        0.7       Reduced attention      ║
 * ║   LIGHT_NREM       0.5        0.6        0.4       Elevated threshold     ║
 * ║   DEEP_NREM        0.2        0.3        0.2       Protective deafness    ║
 * ║   REM              0.4        0.7        0.5       Dream auditory         ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_AUDIO_CORTEX_SLEEP_BRIDGE_H
#define NIMCP_AUDIO_CORTEX_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "perception/nimcp_audio_cortex.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants - Sleep State Audio Modulation
 * ============================================================================ */

/* Auditory threshold modulation by sleep state (higher = better hearing) */
#define AUDIO_SLEEP_THRESHOLD_AWAKE         1.0f   /**< Normal threshold */
#define AUDIO_SLEEP_THRESHOLD_DROWSY        0.8f   /**< Slightly elevated */
#define AUDIO_SLEEP_THRESHOLD_LIGHT_NREM    0.5f   /**< Moderately elevated */
#define AUDIO_SLEEP_THRESHOLD_DEEP_NREM     0.2f   /**< Highly elevated */
#define AUDIO_SLEEP_THRESHOLD_REM           0.4f   /**< Variable threshold */

/* Frequency selectivity modulation by sleep state */
#define AUDIO_SLEEP_FREQUENCY_AWAKE         1.0f   /**< Sharp tuning */
#define AUDIO_SLEEP_FREQUENCY_DROWSY        0.9f   /**< Slightly broader */
#define AUDIO_SLEEP_FREQUENCY_LIGHT_NREM    0.6f   /**< Broader tuning */
#define AUDIO_SLEEP_FREQUENCY_DEEP_NREM     0.3f   /**< Coarse tuning */
#define AUDIO_SLEEP_FREQUENCY_REM           0.7f   /**< Maintained tuning */

/* Processing speed modulation by sleep state */
#define AUDIO_SLEEP_SPEED_AWAKE             1.0f   /**< Full speed */
#define AUDIO_SLEEP_SPEED_DROWSY            0.7f   /**< Slower processing */
#define AUDIO_SLEEP_SPEED_LIGHT_NREM        0.4f   /**< Reduced speed */
#define AUDIO_SLEEP_SPEED_DEEP_NREM         0.2f   /**< Minimal speed */
#define AUDIO_SLEEP_SPEED_REM               0.5f   /**< Moderate speed */

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Sleep-audio cortex bridge configuration
 */
typedef struct {
    bool enable_threshold_modulation;    /**< Enable threshold changes */
    bool enable_frequency_modulation;    /**< Enable frequency selectivity changes */
    bool enable_speed_modulation;        /**< Enable processing speed changes */
    float modulation_strength;           /**< Overall modulation strength (0-1) */
} audio_sleep_config_t;

/**
 * @brief Computed sleep effects on audio processing
 */
typedef struct {
    float threshold_factor;              /**< Multiply threshold by this (lower = worse) */
    float frequency_selectivity_factor;  /**< Multiply frequency selectivity by this */
    float processing_speed_factor;       /**< Multiply processing speed by this */
    sleep_state_t current_state;         /**< Current sleep state */
    float sleep_pressure;                /**< Current sleep pressure */
    bool audio_processing_enabled;       /**< False during deep sleep */
} audio_sleep_effects_t;

/**
 * @brief Sleep-audio cortex integration bridge
 */
typedef struct audio_sleep_bridge_struct* audio_sleep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * WHAT: Get default sleep-audio cortex bridge configuration
 * WHY:  Provide sensible defaults based on auditory neuroscience
 */
int audio_sleep_default_config(audio_sleep_config_t* config);

/**
 * WHAT: Create sleep-audio cortex bridge
 * WHY:  Initialize integration between sleep and audio cortex systems
 * HOW:  Register callback for automatic sleep state updates
 *
 * @param config Bridge configuration (NULL for defaults)
 * @param sleep_system Sleep system instance
 * @return Bridge handle or NULL on failure
 *
 * BIOLOGICAL BASIS:
 * - Auditory threshold increases 30-50 dB during NREM sleep
 * - Thalamic gating reduces auditory cortex responsiveness
 * - Frequency discrimination degrades with sleep pressure
 */
audio_sleep_bridge_t audio_sleep_bridge_create(
    const audio_sleep_config_t* config,
    sleep_system_t sleep_system
);

/**
 * WHAT: Destroy sleep-audio cortex bridge
 * WHY:  Clean up resources and unregister callback
 * HOW:  Unregister sleep state callback, free memory
 */
void audio_sleep_bridge_destroy(audio_sleep_bridge_t bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * WHAT: Update audio effects from sleep system state (manual polling)
 * WHY:  Compute how current sleep state affects audio processing
 * HOW:  Query sleep state and update modulation factors
 *
 * NOTE: This is for manual polling. If callback is registered, updates happen automatically.
 */
int audio_sleep_update(audio_sleep_bridge_t bridge);

/**
 * WHAT: Get modulated audio parameters for current sleep state
 * WHY:  Apply sleep modulation to audio processing
 */
int audio_sleep_get_effects(const audio_sleep_bridge_t bridge, audio_sleep_effects_t* effects);

/**
 * WHAT: Get sleep-modulated auditory threshold factor
 * WHY:  Convenience function for audio cortex processing
 */
float audio_sleep_get_threshold(const audio_sleep_bridge_t bridge, float base_threshold);

/**
 * WHAT: Get sleep-modulated frequency selectivity factor
 * WHY:  Apply to mel filterbank and MFCC processing
 */
float audio_sleep_get_frequency_selectivity(const audio_sleep_bridge_t bridge, float base_selectivity);

/**
 * WHAT: Get sleep-modulated processing speed factor
 * WHY:  Adjust temporal resolution during sleep
 */
float audio_sleep_get_processing_speed(const audio_sleep_bridge_t bridge, float base_speed);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

float audio_sleep_get_threshold_factor(sleep_state_t state);
float audio_sleep_get_frequency_factor(sleep_state_t state);
float audio_sleep_get_speed_factor(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AUDIO_CORTEX_SLEEP_BRIDGE_H */
