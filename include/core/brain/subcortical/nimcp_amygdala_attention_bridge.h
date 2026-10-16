/**
 * @file nimcp_amygdala_attention_bridge.h
 * @brief Amygdala-Attention Integration Bridge
 * @version 1.0.0
 * @date 2025-12-22
 *
 * WHAT: Bidirectional integration between amygdala (fear/threat processing)
 *       and attention system (salience-based focus)
 * WHY:  Amygdala modulates attention toward threat-relevant stimuli. High fear/anxiety
 *       increases attentional bias toward threats (hypervigilance). Attention feeds back
 *       to influence amygdala processing of attended stimuli.
 * HOW:  Amygdala → Attention: Fear/threat levels increase attention to threat-related
 *       stimuli (threat_salience_boost). Attention → Amygdala: Attended stimuli receive
 *       enhanced amygdala processing (attention_enhancement). Hypervigilance mode when
 *       anxiety is high.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * AMYGDALA → ATTENTION PATHWAYS:
 * ---------------------------
 * 1. Fear-Induced Attention Bias:
 *    - Amygdala activation → enhanced attention to threat-relevant stimuli
 *    - Fear increases attentional priority for threat-related cues
 *    - Anxiety narrows attention beam toward potential threats
 *    - Reference: Vuilleumier, P. (2005) "How brains beware: neural mechanisms
 *      of emotional attention"
 *    - Reference: Anderson, A. K., & Phelps, E. A. (2001) "Lesions of the human
 *      amygdala impair enhanced perception of emotionally salient events"
 *
 * 2. Hypervigilance State:
 *    - High anxiety → sustained attention to threats
 *    - Reduced attentional disengagement from threats
 *    - Broadened threat detection (false alarms)
 *    - Reference: Bar-Haim et al. (2007) "Threat-related attentional bias in
 *      anxious and nonanxious individuals: A meta-analytic study"
 *
 * 3. Amygdala-Prefrontal Modulation of Attention:
 *    - Amygdala projects to prefrontal attention networks
 *    - BLA → dlPFC: enhances threat monitoring
 *    - CeA → ACC: increases conflict detection
 *    - Reference: Ochsner & Gross (2005) "The cognitive control of emotion"
 *
 * ATTENTION → AMYGDALA PATHWAYS:
 * ---------------------------
 * 1. Attentional Enhancement of Amygdala Processing:
 *    - Attended stimuli receive enhanced amygdala processing
 *    - Top-down attention amplifies amygdala responses
 *    - Attention gates amygdala threat evaluation
 *    - Reference: Pessoa, L. (2005) "To what extent are emotional visual stimuli
 *      processed without attention and awareness?"
 *
 * 2. Prefrontal Attention → Amygdala Regulation:
 *    - Sustained attention to non-threat stimuli → reduced amygdala activation
 *    - Cognitive reappraisal via attention → decreased fear
 *    - Distraction from threat → amygdala downregulation
 *    - Reference: Bishop, S. J. (2007) "Neurocognitive mechanisms of anxiety"
 *
 * 3. Attention Training Effects:
 *    - Attention bias modification → reduced amygdala hyperreactivity
 *    - Mindful attention → amygdala habituation
 *    - Reference: MacLeod & Mathews (2012) "Cognitive bias modification
 *      approaches to anxiety"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    AMYGDALA-ATTENTION BRIDGE                               ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  AMYGDALA → ATTENTION PATHWAYS                      │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌─────────────┐                                                  │  ║
 * ║   │   │  AMYGDALA   │                                                  │  ║
 * ║   │   │ ─────────── │                                                  │  ║
 * ║   │   │ Fear: 0.8   │  ───────┐                                        │  ║
 * ║   │   │ Anxiety: 0.6│         │                                        │  ║
 * ║   │   │ Threat: HIGH│         ├──→ Threat Salience Boost               │  ║
 * ║   │   │             │         │    (↑ Attention to Threats)            │  ║
 * ║   │   └─────────────┘         │                                        │  ║
 * ║   │                           ▼                                        │  ║
 * ║   │   ┌──────────────────────────────────┐                            │  ║
 * ║   │   │     ATTENTION SYSTEM             │                            │  ║
 * ║   │   │  - Threat salience: +40%         │                            │  ║
 * ║   │   │  - Hypervigilance: ACTIVE        │                            │  ║
 * ║   │   │  - Disengagement: IMPAIRED       │                            │  ║
 * ║   │   └──────────────────────────────────┘                            │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────────┐                                     │  ║
 * ║   │   │   THREAT LEVEL           │                                     │  ║
 * ║   │   │ ──────────────────────── │                                     │  ║
 * ║   │   │ NONE     → +0% salience  │                                     │  ║
 * ║   │   │ LOW      → +10% salience │                                     │  ║
 * ║   │   │ MODERATE → +20% salience │                                     │  ║
 * ║   │   │ HIGH     → +40% salience │                                     │  ║
 * ║   │   │ SEVERE   → +80% salience │                                     │  ║
 * ║   │   └──────────────────────────┘                                     │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  ATTENTION → AMYGDALA PATHWAYS                      │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ ATTENTION    │ ──→ Enhanced Amygdala Processing                │  ║
 * ║   │   │ FOCUS        │ ──→ Attended Stimuli Amplified                  │  ║
 * ║   │   │ TO STIMULUS  │ ──→ Gated Threat Evaluation                     │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ DISTRACTION  │ ──→ Reduced Amygdala Activation                 │  ║
 * ║   │   │ FROM THREAT  │ ──→ Fear Downregulation                         │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_AMYGDALA_ATTENTION_BRIDGE_H
#define NIMCP_AMYGDALA_ATTENTION_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "core/brain/subcortical/nimcp_amygdala.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Threat level salience boost factors */
#define AMYG_ATT_THREAT_NONE_BOOST     0.0f   /**< No threat → no boost */
#define AMYG_ATT_THREAT_LOW_BOOST      0.1f   /**< Low threat → +10% salience */
#define AMYG_ATT_THREAT_MODERATE_BOOST 0.2f   /**< Moderate threat → +20% salience */
#define AMYG_ATT_THREAT_HIGH_BOOST     0.4f   /**< High threat → +40% salience */
#define AMYG_ATT_THREAT_SEVERE_BOOST   0.8f   /**< Severe threat → +80% salience */

/* Fear/anxiety modulation */
#define AMYG_ATT_FEAR_SALIENCE_SCALE   0.5f   /**< Fear → salience multiplier */
#define AMYG_ATT_ANXIETY_VIGILANCE_SCALE 0.6f /**< Anxiety → vigilance multiplier */
#define AMYG_ATT_HYPERVIGILANCE_THRESHOLD 0.7f /**< Anxiety threshold for hypervigilance */

/* Attention enhancement factors */
#define AMYG_ATT_ATTENTION_ENHANCEMENT_BASE 0.3f  /**< Base attention → amygdala boost */
#define AMYG_ATT_FOCUS_ENHANCEMENT_SCALE    0.4f  /**< Focus intensity scaling */
#define AMYG_ATT_DISTRACTION_SUPPRESSION    0.5f  /**< Distraction → amygdala reduction */

/* Disengagement impairment */
#define AMYG_ATT_DISENGAGEMENT_IMPAIRMENT_BASE 0.2f  /**< Base disengagement difficulty */
#define AMYG_ATT_DISENGAGEMENT_PER_FEAR        0.3f  /**< Per unit fear */

/* Default configuration */
#define AMYG_ATT_DEFAULT_THREAT_SENSITIVITY    1.0f  /**< Threat modulation sensitivity */
#define AMYG_ATT_DEFAULT_ATTENTION_SENSITIVITY 1.0f  /**< Attention modulation sensitivity */
#define AMYG_ATT_DEFAULT_HYPERVIGILANCE_ENABLED true /**< Enable hypervigilance mode */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Amygdala effects on attention
 *
 * How amygdala state modulates attention
 */
typedef struct {
    /* Amygdala state */
    float fear_level;                    /**< Current fear [0-1] */
    float anxiety_level;                 /**< Current anxiety [0-1] */
    amyg_threat_level_t threat_level;    /**< Threat assessment */

    /* Attention modulation */
    float threat_salience_boost;         /**< Boost to threat salience [0-1] */
    float vigilance_level;               /**< Overall vigilance [0-1] */
    float disengagement_difficulty;      /**< Difficulty shifting from threat [0-1] */
    bool hypervigilance_active;          /**< Hypervigilance state */
} amygdala_attention_effects_t;

/**
 * @brief Attention effects on amygdala
 *
 * How attention state modulates amygdala processing
 */
typedef struct {
    /* Attention state */
    float attention_strength;            /**< Current attention intensity [0-1] */
    float focus_on_threat;               /**< Focus on threat stimuli [0-1] */
    float focus_on_neutral;              /**< Focus on neutral stimuli [0-1] */

    /* Amygdala modulation */
    float attention_enhancement;         /**< Enhancement for attended stimuli [0-1] */
    float distraction_suppression;       /**< Suppression from distraction [0-1] */
    float prefrontal_regulation;         /**< Top-down regulation [0-1] */
} attention_amygdala_effects_t;

/**
 * @brief Amygdala-attention bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_threat_salience_boost;   /**< Enable threat → attention boost */
    bool enable_hypervigilance_mode;     /**< Enable hypervigilance state */
    bool enable_attention_enhancement;   /**< Enable attention → amygdala boost */
    bool enable_distraction_suppression; /**< Enable distraction → amygdala reduction */

    /* Sensitivity tuning */
    float threat_sensitivity;            /**< Threat effect multiplier [0.5-2.0] */
    float attention_sensitivity;         /**< Attention effect multiplier [0.5-2.0] */

    /* Thresholds */
    float hypervigilance_threshold;      /**< Anxiety threshold for hypervigilance [0.5-0.9] */
    float attention_focus_threshold;     /**< Attention threshold for enhancement [0.3-0.7] */
} amygdala_attention_config_t;

/**
 * @brief Main amygdala-attention bridge structure
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    amygdala_t* amygdala;                /**< Connected amygdala */
    void* attention;                     /**< Connected attention system (opaque) */

    /* Current state */
    amygdala_attention_effects_t amygdala_effects;
    attention_amygdala_effects_t attention_effects;

    /* Configuration */
    amygdala_attention_config_t config;

    /* Connection state */
    bool amygdala_connected;
    bool attention_connected;
    bool bridge_active;

    /* Timing */
    uint64_t last_update_time;

    /* Statistics */
    uint64_t total_updates;
    uint32_t hypervigilance_activations;
    uint32_t threat_boosts;
    uint32_t attention_enhancements;

    } amygdala_attention_bridge_t;

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biological defaults
 * HOW:  Return struct with evidence-based parameters
 *
 * @param config Output configuration
 * @return 0 on success, NIMCP_ERROR_NULL_POINTER on error
 */
int amygdala_attention_default_config(amygdala_attention_config_t* config);

/**
 * @brief Validate configuration
 *
 * WHAT: Check configuration parameters are valid
 * WHY:  Prevent runtime errors from bad config
 * HOW:  Check ranges, thresholds
 *
 * @param config Configuration to validate
 * @return 0 if valid, NIMCP_ERROR_INVALID_PARAMETER if invalid
 */
int amygdala_attention_validate_config(const amygdala_attention_config_t* config);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create amygdala-attention bridge
 *
 * WHAT: Initialize bidirectional amygdala-attention integration
 * WHY:  Enable realistic fear-attention coupling
 * HOW:  Allocate structure, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
amygdala_attention_bridge_t* amygdala_attention_create(
    const amygdala_attention_config_t* config
);

/**
 * @brief Destroy amygdala-attention bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy connected systems)
 *
 * @param bridge Bridge to destroy
 */
void amygdala_attention_destroy(amygdala_attention_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * WHAT: Clear state, keep connections
 * WHY:  Restart without reallocation
 * HOW:  Zero state, reset statistics
 *
 * @param bridge Bridge to reset
 * @return 0 on success, NIMCP_ERROR_NULL_POINTER on error
 */
int amygdala_attention_reset(amygdala_attention_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect amygdala to bridge
 *
 * WHAT: Link amygdala system to bridge
 * WHY:  Enable amygdala → attention modulation
 * HOW:  Store pointer, mark as connected
 *
 * @param bridge Bridge instance
 * @param amygdala Amygdala system
 * @return 0 on success, error code on failure
 */
int amygdala_attention_connect_amygdala(
    amygdala_attention_bridge_t* bridge,
    amygdala_t* amygdala
);

/**
 * @brief Connect attention system to bridge
 *
 * WHAT: Link attention system to bridge
 * WHY:  Enable attention → amygdala modulation
 * HOW:  Store pointer, mark as connected
 *
 * @param bridge Bridge instance
 * @param attention Attention system (opaque pointer)
 * @return 0 on success, error code on failure
 */
int amygdala_attention_connect_attention(
    amygdala_attention_bridge_t* bridge,
    void* attention
);

/**
 * @brief Disconnect amygdala from bridge
 *
 * WHAT: Remove amygdala connection
 * WHY:  Clean shutdown or reconfiguration
 * HOW:  Clear pointer, mark as disconnected
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int amygdala_attention_disconnect_amygdala(amygdala_attention_bridge_t* bridge);

/**
 * @brief Disconnect attention from bridge
 *
 * WHAT: Remove attention connection
 * WHY:  Clean shutdown or reconfiguration
 * HOW:  Clear pointer, mark as disconnected
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int amygdala_attention_disconnect_attention(amygdala_attention_bridge_t* bridge);

/**
 * @brief Check if bridge is connected
 *
 * WHAT: Query connection status
 * WHY:  Verify both systems connected before update
 * HOW:  Check both connection flags
 *
 * @param bridge Bridge instance
 * @return true if both systems connected
 */
bool amygdala_attention_is_connected(const amygdala_attention_bridge_t* bridge);

/* ============================================================================
 * Amygdala → Attention API
 * ============================================================================ */

/**
 * @brief Apply amygdala effects to attention
 *
 * WHAT: Modulate attention based on amygdala state
 * WHY:  Fear/threat increases attention to threat-relevant stimuli
 * HOW:  Query amygdala fear/threat, boost attention salience
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int amygdala_attention_apply_amygdala_effects(amygdala_attention_bridge_t* bridge);

/**
 * @brief Compute threat salience boost
 *
 * WHAT: Calculate attention boost from threat level
 * WHY:  Threat increases attentional priority
 * HOW:  Map threat level to salience boost factor
 *
 * @param bridge Bridge instance
 * @return Salience boost [0-1]
 */
float amygdala_attention_compute_threat_boost(
    const amygdala_attention_bridge_t* bridge
);

/**
 * @brief Check if hypervigilance is active
 *
 * WHAT: Determine if in hypervigilance state
 * WHY:  High anxiety triggers sustained threat monitoring
 * HOW:  Check anxiety against threshold
 *
 * @param bridge Bridge instance
 * @return true if hypervigilance active
 */
bool amygdala_attention_is_hypervigilant(
    const amygdala_attention_bridge_t* bridge
);

/**
 * @brief Compute disengagement difficulty
 *
 * WHAT: Calculate difficulty shifting attention from threats
 * WHY:  Fear impairs attentional disengagement
 * HOW:  Map fear level to disengagement difficulty
 *
 * @param bridge Bridge instance
 * @return Disengagement difficulty [0-1]
 */
float amygdala_attention_compute_disengagement_difficulty(
    const amygdala_attention_bridge_t* bridge
);

/* ============================================================================
 * Attention → Amygdala API
 * ============================================================================ */

/**
 * @brief Apply attention effects to amygdala
 *
 * WHAT: Modulate amygdala based on attention state
 * WHY:  Attended stimuli receive enhanced amygdala processing
 * HOW:  Query attention focus, enhance/suppress amygdala
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int amygdala_attention_apply_attention_effects(amygdala_attention_bridge_t* bridge);

/**
 * @brief Compute attention enhancement
 *
 * WHAT: Calculate amygdala boost from attention
 * WHY:  Attended stimuli receive amplified processing
 * HOW:  Map attention strength to enhancement factor
 *
 * @param bridge Bridge instance
 * @return Enhancement factor [0-1]
 */
float amygdala_attention_compute_attention_enhancement(
    const amygdala_attention_bridge_t* bridge
);

/**
 * @brief Compute distraction suppression
 *
 * WHAT: Calculate amygdala reduction from distraction
 * WHY:  Attention to non-threat stimuli reduces fear
 * HOW:  Map neutral focus to suppression factor
 *
 * @param bridge Bridge instance
 * @return Suppression factor [0-1]
 */
float amygdala_attention_compute_distraction_suppression(
    const amygdala_attention_bridge_t* bridge
);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update bridge (both directions)
 *
 * WHAT: Process all amygdala-attention interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply amygdala→attention and attention→amygdala effects
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int amygdala_attention_update(amygdala_attention_bridge_t* bridge);

/**
 * @brief Apply modulation to connected systems
 *
 * WHAT: Push computed effects to amygdala and attention
 * WHY:  Actuate bidirectional coupling
 * HOW:  Write to amygdala and attention state
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int amygdala_attention_apply_modulation(amygdala_attention_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current amygdala effects
 *
 * @param bridge Bridge instance
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int amygdala_attention_get_amygdala_effects(
    const amygdala_attention_bridge_t* bridge,
    amygdala_attention_effects_t* effects
);

/**
 * @brief Get current attention effects
 *
 * @param bridge Bridge instance
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int amygdala_attention_get_attention_effects(
    const amygdala_attention_bridge_t* bridge,
    attention_amygdala_effects_t* effects
);

/**
 * @brief Get threat salience boost
 *
 * @param bridge Bridge instance
 * @return Salience boost [0-1]
 */
float amygdala_attention_get_threat_salience_boost(
    const amygdala_attention_bridge_t* bridge
);

/**
 * @brief Get attention enhancement
 *
 * @param bridge Bridge instance
 * @return Enhancement factor [0-1]
 */
float amygdala_attention_get_attention_enhancement(
    const amygdala_attention_bridge_t* bridge
);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge instance
 * @param total_updates Output: total updates
 * @param hypervigilance_activations Output: hypervigilance count
 * @param threat_boosts Output: threat boost count
 * @param attention_enhancements Output: attention enhancement count
 * @return 0 on success, error code on failure
 */
int amygdala_attention_get_statistics(
    const amygdala_attention_bridge_t* bridge,
    uint64_t* total_updates,
    uint32_t* hypervigilance_activations,
    uint32_t* threat_boosts,
    uint32_t* attention_enhancements
);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for threat signals
 * HOW:  Register with bio_router using BIO_MODULE_AMYGDALA_ATTENTION
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int amygdala_attention_connect_bio_async(amygdala_attention_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int amygdala_attention_disconnect_bio_async(amygdala_attention_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool amygdala_attention_is_bio_async_connected(
    const amygdala_attention_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AMYGDALA_ATTENTION_BRIDGE_H */
