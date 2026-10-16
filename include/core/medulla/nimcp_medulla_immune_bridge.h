/**
 * @file nimcp_medulla_immune_bridge.h
 * @brief Medulla-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Bidirectional integration between brain immune system and medulla oblongata
 * WHY:  The medulla controls autonomic functions that directly affect immune response;
 *       inflammation affects arousal, protection, and circadian rhythms. This creates
 *       a critical feedback loop for survival and homeostasis.
 * HOW:  Inflammation modulates arousal and triggers protection; medulla protection
 *       levels escalate immune responses; circadian phase affects immune efficiency.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → MEDULLA PATHWAYS:
 * --------------------------
 * 1. Inflammation → Arousal Modulation:
 *    - Pro-inflammatory cytokines cross blood-brain barrier
 *    - IL-1β, IL-6, TNF-α → sickness behavior, fatigue, reduced arousal
 *    - Anti-inflammatory IL-10 → recovery, normalized arousal
 *    - Reference: Dantzer et al. (2008) "From inflammation to sickness and depression"
 *
 * 2. Inflammation → Protection Escalation:
 *    - Systemic inflammation → heightened protection level
 *    - Cytokine storm → emergency protective response
 *    - Local inflammation → localized protection enhancement
 *    - Reference: Tracey (2002) "The inflammatory reflex"
 *
 * 3. Inflammation → Circadian Disruption:
 *    - Chronic inflammation disrupts circadian rhythms
 *    - IL-6 affects suprachiasmatic nucleus signaling
 *    - Sickness behavior overrides normal circadian patterns
 *    - Reference: Cavadini et al. (2007) "TNF-α suppresses the expression
 *      of clock genes by interfering with E-box-mediated transcription"
 *
 * MEDULLA → IMMUNE PATHWAYS:
 * --------------------------
 * 1. Arousal → Immune Modulation:
 *    - High arousal → catecholamine release → immune cell mobilization
 *    - Low arousal → reduced immune surveillance
 *    - Optimal arousal → balanced immune function
 *    - Reference: Elenkov et al. (2000) "The sympathetic nerve - an integrative
 *      interface between two supersystems"
 *
 * 2. Protection Level → Immune Activation:
 *    - Elevated protection → enhanced immune vigilance
 *    - Critical protection → immune emergency response
 *    - Emergency shutdown → systemic immune activation
 *    - Reference: Tracey (2007) "Physiology and immunology of the cholinergic
 *      antiinflammatory pathway"
 *
 * 3. Circadian Phase → Immune Efficiency:
 *    - Day phases → enhanced immune surveillance
 *    - Night phases → immune repair and consolidation
 *    - Sleep deprivation → impaired immunity
 *    - Reference: Scheiermann et al. (2013) "Circadian control of the immune system"
 *
 * 4. Brainstem Coupling → Systemic Immune Coordination:
 *    - Vagal nerve activity → cholinergic anti-inflammatory pathway
 *    - Brainstem nuclei coordinate autonomic-immune responses
 *    - Reference: Pavlov & Tracey (2005) "The cholinergic anti-inflammatory pathway"
 *
 * ARCHITECTURE:
 * ```
 * ╔══════════════════════════════════════════════════════════════════════════════╗
 * ║                     MEDULLA-IMMUNE BRIDGE                                     ║
 * ╠══════════════════════════════════════════════════════════════════════════════╣
 * ║                                                                               ║
 * ║   ┌─────────────────────────────────────────────────────────────────────────┐ ║
 * ║   │                   IMMUNE → MEDULLA PATHWAYS                              │ ║
 * ║   │                                                                          │ ║
 * ║   │   ┌──────────────┐                                                      │ ║
 * ║   │   │  CYTOKINES   │                                                      │ ║
 * ║   │   │ ──────────── │                                                      │ ║
 * ║   │   │ IL-1β → -0.3 │  ───┐                                                │ ║
 * ║   │   │ IL-6  → -0.2 │     ├──→ Arousal Reduction (Sickness Behavior)       │ ║
 * ║   │   │ TNF-α → -0.4 │     │                                                │ ║
 * ║   │   │ IL-10 → +0.2 │  ───┘                                                │ ║
 * ║   │   └──────────────┘                                                      │ ║
 * ║   │                                                                          │ ║
 * ║   │   ┌────────────────────────────┐                                        │ ║
 * ║   │   │   INFLAMMATION LEVEL       │                                        │ ║
 * ║   │   │ ────────────────────────── │                                        │ ║
 * ║   │   │ NONE     → Normal arousal  │                                        │ ║
 * ║   │   │ LOCAL    → -10% arousal    │                                        │ ║
 * ║   │   │ REGIONAL → -25% arousal    │                                        │ ║
 * ║   │   │ SYSTEMIC → -40% arousal    │                                        │ ║
 * ║   │   │ STORM    → Emergency mode  │                                        │ ║
 * ║   │   └────────────────────────────┘                                        │ ║
 * ║   └─────────────────────────────────────────────────────────────────────────┘ ║
 * ║                                                                               ║
 * ║   ┌─────────────────────────────────────────────────────────────────────────┐ ║
 * ║   │                   MEDULLA → IMMUNE PATHWAYS                              │ ║
 * ║   │                                                                          │ ║
 * ║   │   ┌──────────────────────┐                                              │ ║
 * ║   │   │   AROUSAL LEVEL      │                                              │ ║
 * ║   │   │ ──────────────────── │                                              │ ║
 * ║   │   │ Low (0.0-0.3)  → Immune depression                                  │ ║
 * ║   │   │ Normal (0.3-0.7) → Optimal immunity                                 │ ║
 * ║   │   │ High (0.7-1.0) → Enhanced surveillance                              │ ║
 * ║   │   └──────────────────────┘                                              │ ║
 * ║   │                                                                          │ ║
 * ║   │   ┌──────────────────────┐                                              │ ║
 * ║   │   │   PROTECTION LEVEL   │                                              │ ║
 * ║   │   │ ──────────────────── │                                              │ ║
 * ║   │   │ NORMAL   → Baseline immunity                                        │ ║
 * ║   │   │ ELEVATED → +20% immune activity                                     │ ║
 * ║   │   │ HIGH     → +40% immune activity                                     │ ║
 * ║   │   │ CRITICAL → Emergency immune response                                │ ║
 * ║   │   │ SHUTDOWN → Immune storm prevention                                  │ ║
 * ║   │   └──────────────────────┘                                              │ ║
 * ║   │                                                                          │ ║
 * ║   │   ┌──────────────────────┐                                              │ ║
 * ║   │   │   CIRCADIAN PHASE    │                                              │ ║
 * ║   │   │ ──────────────────── │                                              │ ║
 * ║   │   │ MORNING/DAY  → Active immune surveillance                           │ ║
 * ║   │   │ EVENING/NIGHT → Immune repair/consolidation                         │ ║
 * ║   │   └──────────────────────┘                                              │ ║
 * ║   └─────────────────────────────────────────────────────────────────────────┘ ║
 * ╚══════════════════════════════════════════════════════════════════════════════╝
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

#ifndef NIMCP_MEDULLA_IMMUNE_BRIDGE_H
#define NIMCP_MEDULLA_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "core/medulla/nimcp_medulla.h"
#include "async/nimcp_bio_router.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Bridge update interval in milliseconds */
#define MEDULLA_IMMUNE_UPDATE_INTERVAL_MS 100

/** Cytokine effect constants - arousal modulation */
#define CYTOKINE_IL1_AROUSAL_IMPACT    -0.30f  /**< IL-1β reduces arousal (sickness) */
#define CYTOKINE_IL6_AROUSAL_IMPACT    -0.20f  /**< IL-6 reduces arousal */
#define CYTOKINE_TNF_AROUSAL_IMPACT    -0.40f  /**< TNF-α strongly reduces arousal */
#define CYTOKINE_IL10_AROUSAL_IMPACT   +0.20f  /**< IL-10 promotes arousal recovery */
#define CYTOKINE_IFN_AROUSAL_IMPACT    -0.25f  /**< IFN-γ reduces arousal */

/** Inflammation level -> arousal reduction factors */
#define INFLAMMATION_NONE_AROUSAL_FACTOR     1.00f
#define INFLAMMATION_LOCAL_AROUSAL_FACTOR    0.90f
#define INFLAMMATION_REGIONAL_AROUSAL_FACTOR 0.75f
#define INFLAMMATION_SYSTEMIC_AROUSAL_FACTOR 0.60f
#define INFLAMMATION_STORM_AROUSAL_FACTOR    0.30f

/** Protection level -> immune activity modulation */
#define PROTECTION_NORMAL_IMMUNE_FACTOR      1.00f
#define PROTECTION_ELEVATED_IMMUNE_FACTOR    1.20f
#define PROTECTION_HIGH_IMMUNE_FACTOR        1.40f
#define PROTECTION_CRITICAL_IMMUNE_FACTOR    2.00f
#define PROTECTION_SHUTDOWN_IMMUNE_FACTOR    0.50f  /**< Reduce to prevent storm */

/** Arousal level thresholds for immune modulation */
#define AROUSAL_LOW_THRESHOLD     0.30f  /**< Below: immune depression */
#define AROUSAL_HIGH_THRESHOLD    0.70f  /**< Above: enhanced surveillance */

/** Circadian phase immune efficiency factors */
#define CIRCADIAN_DAY_IMMUNE_FACTOR    1.20f  /**< Day: active surveillance */
#define CIRCADIAN_NIGHT_IMMUNE_FACTOR  0.80f  /**< Night: repair mode */

//=============================================================================
// Types
//=============================================================================

/**
 * @brief Configuration for medulla-immune bridge
 */
typedef struct medulla_immune_config {
    /** Enable immune → medulla pathway (inflammation affects arousal) */
    bool enable_immune_to_medulla;

    /** Enable medulla → immune pathway (protection affects immunity) */
    bool enable_medulla_to_immune;

    /** Enable circadian-immune modulation */
    bool enable_circadian_modulation;

    /** Update interval in milliseconds */
    uint32_t update_interval_ms;

    /** Cytokine sensitivity multiplier (default 1.0) */
    float cytokine_sensitivity;

    /** Protection-immune coupling strength (default 1.0) */
    float protection_coupling;

    /** Arousal-immune coupling strength (default 1.0) */
    float arousal_coupling;

    /** Circadian-immune coupling strength (default 1.0) */
    float circadian_coupling;

    /** Enable bio-async messaging */
    bool enable_bio_async;

    /** Trigger emergency on cytokine storm */
    bool emergency_on_storm;
} medulla_immune_config_t;

/**
 * @brief Computed cytokine effects on medulla
 */
typedef struct medulla_cytokine_effects {
    /** Net arousal modulation from cytokines [-1.0, +1.0] */
    float arousal_modulation;

    /** Protection level adjustment from inflammation */
    int protection_adjustment;

    /** Circadian phase disruption factor [0.0, 1.0] */
    float circadian_disruption;

    /** Current inflammation level detected */
    brain_inflammation_level_t inflammation_level;

    /** Whether emergency shutdown should be triggered */
    bool trigger_emergency;

    /** Computed arousal factor from inflammation */
    float inflammation_arousal_factor;
} medulla_cytokine_effects_t;

/**
 * @brief Computed medulla effects on immune system
 */
typedef struct medulla_immune_effects {
    /** Immune activity multiplier from arousal [0.5, 1.5] */
    float arousal_immune_factor;

    /** Immune activity multiplier from protection [0.5, 2.0] */
    float protection_immune_factor;

    /** Immune activity multiplier from circadian [0.8, 1.2] */
    float circadian_immune_factor;

    /** Combined immune modulation factor */
    float combined_immune_factor;

    /** Current arousal level */
    float arousal_level;

    /** Current protection level */
    protection_level_t protection_level;

    /** Current circadian phase */
    circadian_phase_t circadian_phase;

    /** Whether immune surveillance should be enhanced */
    bool enhance_surveillance;
} medulla_immune_effects_t;

/**
 * @brief Bridge statistics
 */
typedef struct medulla_immune_stats {
    /** Total updates performed */
    uint64_t total_updates;

    /** Immune → medulla modulations applied */
    uint64_t immune_to_medulla_count;

    /** Medulla → immune modulations applied */
    uint64_t medulla_to_immune_count;

    /** Emergency shutdowns triggered */
    uint64_t emergencies_triggered;

    /** Cytokine storms detected */
    uint64_t storms_detected;

    /** Circadian disruptions detected */
    uint64_t circadian_disruptions;

    /** Average arousal modulation */
    float avg_arousal_modulation;

    /** Average immune factor */
    float avg_immune_factor;
} medulla_immune_stats_t;

/**
 * @brief Medulla-Immune bridge handle (opaque)
 */
typedef struct medulla_immune_bridge* medulla_immune_bridge_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Returns sensible defaults for bridge configuration
 * WHY:  Provides balanced immune-medulla integration
 * HOW:  Sets biologically-plausible coupling strengths
 *
 * @param config Output configuration structure
 */
void medulla_immune_default_config(medulla_immune_config_t* config);

/**
 * @brief Create a new medulla-immune bridge
 *
 * WHAT: Creates bidirectional integration between medulla and immune system
 * WHY:  Enables realistic autonomic-immune feedback loops
 * HOW:  Allocates bridge, connects systems, initializes state
 *
 * @param config Bridge configuration
 * @param medulla Medulla oblongata instance
 * @param immune Brain immune system instance
 * @return Bridge handle on success, NULL on failure
 */
medulla_immune_bridge_t medulla_immune_create(
    const medulla_immune_config_t* config,
    medulla_t medulla,
    brain_immune_system_t* immune
);

/**
 * @brief Destroy a medulla-immune bridge
 *
 * WHAT: Cleans up bridge resources
 * WHY:  Prevents memory leaks
 * HOW:  Disconnects systems, frees memory
 *
 * @param bridge Bridge to destroy
 */
void medulla_immune_destroy(medulla_immune_bridge_t bridge);

//=============================================================================
// Update Functions
//=============================================================================

/**
 * @brief Update bridge (both directions)
 *
 * WHAT: Performs bidirectional immune-medulla modulation
 * WHY:  Maintains continuous autonomic-immune coordination
 * HOW:  Reads cytokines → modulates medulla; reads medulla → modulates immune
 *
 * @param bridge Bridge to update
 * @return 0 on success, negative error code on failure
 */
int medulla_immune_update(medulla_immune_bridge_t bridge);

/**
 * @brief Update immune → medulla pathway only
 *
 * WHAT: Computes and applies immune effects on medulla
 * WHY:  Inflammation affects arousal and protection
 * HOW:  Reads cytokine levels, computes arousal/protection adjustments
 *
 * @param bridge Bridge to update
 * @param effects Output effects structure (optional)
 * @return 0 on success, negative error code on failure
 */
int medulla_immune_update_immune_to_medulla(
    medulla_immune_bridge_t bridge,
    medulla_cytokine_effects_t* effects
);

/**
 * @brief Update medulla → immune pathway only
 *
 * WHAT: Computes and applies medulla effects on immune system
 * WHY:  Arousal, protection, circadian affect immune function
 * HOW:  Reads medulla state, computes immune modulation factors
 *
 * @param bridge Bridge to update
 * @param effects Output effects structure (optional)
 * @return 0 on success, negative error code on failure
 */
int medulla_immune_update_medulla_to_immune(
    medulla_immune_bridge_t bridge,
    medulla_immune_effects_t* effects
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get current cytokine effects on medulla
 *
 * @param bridge Bridge to query
 * @param effects Output effects structure
 * @return 0 on success, negative error code on failure
 */
int medulla_immune_get_cytokine_effects(
    medulla_immune_bridge_t bridge,
    medulla_cytokine_effects_t* effects
);

/**
 * @brief Get current medulla effects on immune system
 *
 * @param bridge Bridge to query
 * @param effects Output effects structure
 * @return 0 on success, negative error code on failure
 */
int medulla_immune_get_immune_effects(
    medulla_immune_bridge_t bridge,
    medulla_immune_effects_t* effects
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge to query
 * @param stats Output statistics structure
 * @return 0 on success, negative error code on failure
 */
int medulla_immune_get_stats(
    medulla_immune_bridge_t bridge,
    medulla_immune_stats_t* stats
);

//=============================================================================
// Connection Functions
//=============================================================================

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Enables bio-async messaging for immune-medulla coordination
 * WHY:  Allows asynchronous event-driven updates
 * HOW:  Registers with bio-router for cytokine and medulla messages
 *
 * @param bridge Bridge to connect
 * @return 0 on success, negative error code on failure
 */
int medulla_immune_connect_bio_async(medulla_immune_bridge_t bridge);

/**
 * @brief Disconnect bridge from bio-async router
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success, negative error code on failure
 */
int medulla_immune_disconnect_bio_async(medulla_immune_bridge_t bridge);

/**
 * @brief Check if bridge is connected to bio-async
 *
 * @param bridge Bridge to check
 * @return true if connected, false otherwise
 */
bool medulla_immune_is_bio_async_connected(medulla_immune_bridge_t bridge);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Compute arousal factor from inflammation level
 *
 * @param level Inflammation level
 * @return Arousal multiplier [0.3, 1.0]
 */
float medulla_immune_compute_inflammation_arousal(brain_inflammation_level_t level);

/**
 * @brief Compute immune factor from protection level
 *
 * @param level Protection level
 * @return Immune multiplier [0.5, 2.0]
 */
float medulla_immune_compute_protection_immune(protection_level_t level);

/**
 * @brief Compute immune factor from circadian phase
 *
 * @param phase Circadian phase
 * @return Immune multiplier [0.8, 1.2]
 */
float medulla_immune_compute_circadian_immune(circadian_phase_t phase);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MEDULLA_IMMUNE_BRIDGE_H */
