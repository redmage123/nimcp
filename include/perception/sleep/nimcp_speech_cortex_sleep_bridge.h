/**
 * @file nimcp_speech_cortex_sleep_bridge.h
 * @brief Sleep-Speech Cortex Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Bidirectional integration between sleep/wake system and speech cortex
 * WHY:  Sleep states fundamentally alter speech clarity and comprehension
 * HOW:  Sleep state modulates phoneme discrimination, speech clarity, comprehension
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SLEEP → SPEECH CORTEX PATHWAYS:
 * ---------------------------------
 * 1. Phoneme Discrimination Degradation:
 *    - AWAKE: Sharp categorical perception (clear phoneme boundaries)
 *    - DROWSY: Reduced phoneme discrimination (confusion errors increase)
 *    - NREM: Minimal speech comprehension (Wernicke's area suppressed)
 *    - REM: Sleep talking possible but comprehension impaired
 *    - Reference: Portas et al. (2000) "Auditory processing during sleep"
 *
 * 2. Speech Clarity Modulation (Production):
 *    - AWAKE: Clear articulation, normal prosody
 *    - DROWSY: Slurred speech, reduced pitch variation
 *    - NREM: No voluntary speech (motor cortex suppressed)
 *    - REM: Sleep talking occurs but atonia limits articulation
 *    - Reference: Siegel (2005) "REM sleep behavior disorder"
 *
 * 3. Comprehension Degradation (Wernicke's Area):
 *    - AWAKE: Full semantic comprehension, lexical access
 *    - DROWSY: Slower word recognition, missed words
 *    - NREM: Minimal semantic processing (K-complex to salient words)
 *    - REM: Internal language generation but poor external comprehension
 *    - Reference: Nir & Tononi (2010) "Dreaming and consciousness"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              SLEEP-SPEECH CORTEX INTEGRATION BRIDGE                        ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   SLEEP STATE      Phoneme    Clarity    Comprehen  Effect                ║
 * ║   ─────────────────────────────────────────────────────────────────────   ║
 * ║   AWAKE            1.0        1.0        1.0        Full speech           ║
 * ║   DROWSY           0.7        0.6        0.5        Slurred, missed words ║
 * ║   LIGHT_NREM       0.3        0.2        0.2        Minimal processing    ║
 * ║   DEEP_NREM        0.1        0.0        0.1        No speech production  ║
 * ║   REM              0.4        0.3        0.3        Sleep talking         ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SPEECH_CORTEX_SLEEP_BRIDGE_H
#define NIMCP_SPEECH_CORTEX_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "perception/nimcp_speech_cortex.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants - Sleep State Speech Modulation
 * ============================================================================ */

/* Phoneme discrimination modulation by sleep state */
#define SPEECH_SLEEP_PHONEME_AWAKE         1.0f   /**< Clear discrimination */
#define SPEECH_SLEEP_PHONEME_DROWSY        0.7f   /**< Reduced clarity */
#define SPEECH_SLEEP_PHONEME_LIGHT_NREM    0.3f   /**< Minimal discrimination */
#define SPEECH_SLEEP_PHONEME_DEEP_NREM     0.1f   /**< No discrimination */
#define SPEECH_SLEEP_PHONEME_REM           0.4f   /**< Internal speech active */

/* Speech clarity modulation by sleep state (production) */
#define SPEECH_SLEEP_CLARITY_AWAKE         1.0f   /**< Clear articulation */
#define SPEECH_SLEEP_CLARITY_DROWSY        0.6f   /**< Slurred speech */
#define SPEECH_SLEEP_CLARITY_LIGHT_NREM    0.2f   /**< No voluntary speech */
#define SPEECH_SLEEP_CLARITY_DEEP_NREM     0.0f   /**< No speech */
#define SPEECH_SLEEP_CLARITY_REM           0.3f   /**< Sleep talking */

/* Comprehension modulation by sleep state (Wernicke's) */
#define SPEECH_SLEEP_COMPREHEN_AWAKE       1.0f   /**< Full comprehension */
#define SPEECH_SLEEP_COMPREHEN_DROWSY      0.5f   /**< Missed words */
#define SPEECH_SLEEP_COMPREHEN_LIGHT_NREM  0.2f   /**< Minimal semantic */
#define SPEECH_SLEEP_COMPREHEN_DEEP_NREM   0.1f   /**< No comprehension */
#define SPEECH_SLEEP_COMPREHEN_REM         0.3f   /**< Internal language */

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Sleep-speech cortex bridge configuration
 */
typedef struct {
    bool enable_phoneme_modulation;      /**< Enable phoneme discrimination changes */
    bool enable_clarity_modulation;      /**< Enable speech clarity changes */
    bool enable_comprehension_modulation;/**< Enable comprehension changes */
    float modulation_strength;           /**< Overall modulation strength (0-1) */
} speech_sleep_config_t;

/**
 * @brief Computed sleep effects on speech processing
 */
typedef struct {
    float phoneme_discrimination_factor; /**< Multiply phoneme accuracy by this */
    float speech_clarity_factor;         /**< Multiply speech clarity by this */
    float comprehension_factor;          /**< Multiply comprehension by this */
    sleep_state_t current_state;         /**< Current sleep state */
    float sleep_pressure;                /**< Current sleep pressure */
    bool speech_processing_enabled;      /**< False during deep sleep */
} speech_sleep_effects_t;

/**
 * @brief Sleep-speech cortex integration bridge
 */
typedef struct speech_sleep_bridge_struct* speech_sleep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * WHAT: Get default sleep-speech cortex bridge configuration
 * WHY:  Provide sensible defaults based on speech neuroscience
 */
int speech_sleep_default_config(speech_sleep_config_t* config);

/**
 * WHAT: Create sleep-speech cortex bridge
 * WHY:  Initialize integration between sleep and speech cortex systems
 * HOW:  Register callback for automatic sleep state updates
 *
 * @param config Bridge configuration (NULL for defaults)
 * @param sleep_system Sleep system instance
 * @return Bridge handle or NULL on failure
 *
 * BIOLOGICAL BASIS:
 * - Phoneme categorical perception degrades with fatigue
 * - Speech production requires motor cortex activation (suppressed in NREM)
 * - Wernicke's area shows reduced activity during sleep
 */
speech_sleep_bridge_t speech_sleep_bridge_create(
    const speech_sleep_config_t* config,
    sleep_system_t sleep_system
);

/**
 * WHAT: Destroy sleep-speech cortex bridge
 * WHY:  Clean up resources and unregister callback
 * HOW:  Unregister sleep state callback, free memory
 */
void speech_sleep_bridge_destroy(speech_sleep_bridge_t bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * WHAT: Update speech effects from sleep system state (manual polling)
 * WHY:  Compute how current sleep state affects speech processing
 * HOW:  Query sleep state and update modulation factors
 *
 * NOTE: This is for manual polling. If callback is registered, updates happen automatically.
 */
int speech_sleep_update(speech_sleep_bridge_t bridge);

/**
 * WHAT: Get modulated speech parameters for current sleep state
 * WHY:  Apply sleep modulation to speech processing
 */
int speech_sleep_get_effects(const speech_sleep_bridge_t bridge, speech_sleep_effects_t* effects);

/**
 * WHAT: Get sleep-modulated phoneme discrimination factor
 * WHY:  Convenience function for phoneme recognition
 */
float speech_sleep_get_phoneme_discrimination(const speech_sleep_bridge_t bridge, float base_discrimination);

/**
 * WHAT: Get sleep-modulated speech clarity factor
 * WHY:  Apply to speech production (Broca's area)
 */
float speech_sleep_get_speech_clarity(const speech_sleep_bridge_t bridge, float base_clarity);

/**
 * WHAT: Get sleep-modulated comprehension factor
 * WHY:  Apply to word recognition (Wernicke's area)
 */
float speech_sleep_get_comprehension(const speech_sleep_bridge_t bridge, float base_comprehension);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

float speech_sleep_get_phoneme_factor(sleep_state_t state);
float speech_sleep_get_clarity_factor(sleep_state_t state);
float speech_sleep_get_comprehension_factor(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SPEECH_CORTEX_SLEEP_BRIDGE_H */
