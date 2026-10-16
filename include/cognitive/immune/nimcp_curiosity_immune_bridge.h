/**
 * @file nimcp_curiosity_immune_bridge.h
 * @brief Curiosity-Immune System Integration
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional coupling between brain immune and curiosity systems
 * WHY:  Biological realism - sickness behavior suppresses exploration, novelty triggers immune vigilance
 * HOW:  Monitor cytokine/inflammation to suppress curiosity drive, monitor novelty to trigger immune alertness
 *
 * BIOLOGICAL MODEL:
 * ```
 * IMMUNE → CURIOSITY (Sickness Behavior):
 * ─────────────────────────────────────────────────────────────────
 * Cytokines (IL-1, IL-6, TNF-α)  → Reduce dopamine signaling
 * High inflammation              → Suppress exploration drive
 * Chronic inflammation           → Sustained curiosity reduction
 * Resolution (IL-10)             → Restore exploration behavior
 *
 * CURIOSITY → IMMUNE (Novelty Vigilance):
 * ─────────────────────────────────────────────────────────────────
 * Novel stimuli                  → Increased immune alertness
 * High learning drive            → Enhanced threat detection
 * Exploration behavior           → Prophylactic immune activation
 * ```
 *
 * NEUROSCIENCE BASIS:
 * - Sickness behavior: IL-1β, IL-6, TNF-α reduce dopaminergic activity in VTA/NAc
 * - Exploration suppression: Cytokines signal "conserve energy, avoid risk"
 * - Novelty stress: Novel environments trigger cortisol → immune vigilance
 * - Dopamine-immune coupling: DA receptors on immune cells modulate response
 *
 * REFERENCES:
 * - Dantzer et al. (2008) "From inflammation to sickness and depression"
 * - Capuron & Miller (2011) "Immune system to brain signaling"
 * - Irwin & Cole (2011) "Reciprocal regulation of the neural and innate immune systems"
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CURIOSITY_IMMUNE_BRIDGE_H
#define NIMCP_CURIOSITY_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/curiosity/nimcp_curiosity.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/**
 * @brief Sickness behavior threshold for exploration suppression
 *
 * BIOLOGY: IL-1β at ~10 pg/mL induces lethargy, reduced exploration
 * MAPPING: 0.3 = ~30% cytokine saturation triggers sickness behavior
 */
#define SICKNESS_EXPLORATION_THRESHOLD 0.3f

/**
 * @brief Chronic inflammation duration for sustained suppression (7 days)
 *
 * BIOLOGY: Chronic inflammation (>1 week) associated with anhedonia
 */
#define CHRONIC_INFLAMMATION_SEC (7 * 24 * 3600)

/**
 * @brief Novelty threshold for immune vigilance trigger
 *
 * BIOLOGY: Novel environments elevate cortisol, prime immune response
 * MAPPING: 0.7 = high novelty score triggers immune alertness
 */
#define NOVELTY_IMMUNE_TRIGGER_THRESHOLD 0.7f

/**
 * @brief Maximum curiosity suppression multiplier during cytokine storm
 *
 * BIOLOGY: Severe inflammation can reduce exploration by 80-90%
 */
#define MAX_CURIOSITY_SUPPRESSION 0.1f

/**
 * @brief Immune vigilance boost from high curiosity drive
 *
 * BIOLOGY: Exploratory behavior increases immune alertness ~20%
 */
#define CURIOSITY_IMMUNE_BOOST 1.2f

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for curiosity-immune integration
 */
typedef struct {
    /* Immune → Curiosity features */
    bool enable_sickness_behavior;          /**< Cytokines suppress exploration */
    bool enable_inflammation_suppression;   /**< Inflammation reduces curiosity */
    bool enable_chronic_anhedonia;          /**< Chronic inflammation → anhedonia */
    bool enable_resolution_recovery;        /**< IL-10 restores exploration */

    /* Curiosity → Immune features */
    bool enable_novelty_vigilance;          /**< Novel stimuli → immune alert */
    bool enable_exploration_immune_boost;   /**< High curiosity → immune priming */
    bool enable_learning_stress_response;   /**< Learning challenges → immune activation */

    /* Sensitivity parameters */
    float cytokine_sensitivity;             /**< Cytokine effect multiplier (0.5-2.0) */
    float inflammation_sensitivity;         /**< Inflammation effect multiplier (0.5-2.0) */
    float novelty_sensitivity;              /**< Novelty detection multiplier (0.5-2.0) */

    /* Thresholds */
    float sickness_threshold;               /**< Cytokine level for sickness (0.2-0.4) */
    float novelty_trigger_threshold;        /**< Novelty for immune trigger (0.6-0.8) */
    float chronic_inflammation_days;        /**< Days for chronic classification (5-14) */
} curiosity_immune_config_t;

/**
 * @brief Curiosity-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System references */
    brain_immune_system_t* immune_system;
    curiosity_engine_t curiosity_engine;

    /* Configuration */
    curiosity_immune_config_t config;

    /* Sickness behavior state */
    float current_sickness_level;           /**< Current sickness behavior (0-1) */
    float curiosity_suppression_factor;     /**< Exploration suppression (0-1) */
    float baseline_curiosity_backup;        /**< Original curiosity level */
    bool in_sickness_state;                 /**< Currently experiencing sickness */
    uint64_t sickness_onset_time;           /**< When sickness began */

    /* Immune vigilance state */
    float current_novelty_level;            /**< Current novelty (0-1) */
    float immune_vigilance_boost;           /**< Immune alertness multiplier (1.0-1.5) */
    bool immune_vigilance_active;           /**< Novelty-triggered vigilance */
    uint64_t last_novelty_trigger;          /**< Last novelty trigger time */

    /* Statistics */
    uint64_t sickness_episodes;             /**< Total sickness episodes */
    uint64_t novelty_triggers;              /**< Novelty-based immune triggers */
    uint64_t chronic_suppression_duration;  /**< Total chronic suppression time */
    float max_suppression_observed;         /**< Peak suppression recorded */
    } curiosity_immune_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide biologically-based default configuration
 * WHY:  Easy initialization with evidence-based parameters
 * HOW:  Return struct with literature-derived thresholds
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int curiosity_immune_default_config(curiosity_immune_config_t* config);

/**
 * @brief Create curiosity-immune bridge
 *
 * WHAT: Initialize bidirectional immune-curiosity coupling
 * WHY:  Enable sickness behavior and novelty-immune interactions
 * HOW:  Allocate bridge, link systems, register callbacks
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param curiosity_engine Curiosity engine
 * @return New bridge or NULL on failure
 */
curiosity_immune_bridge_t* curiosity_immune_bridge_create(
    const curiosity_immune_config_t* config,
    brain_immune_system_t* immune_system,
    curiosity_engine_t curiosity_engine
);

/**
 * @brief Destroy curiosity-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper deallocation, restore original curiosity levels
 * HOW:  Unregister callbacks, free memory
 *
 * @param bridge Bridge to destroy
 */
void curiosity_immune_bridge_destroy(curiosity_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Curiosity (Sickness Behavior)
 * ============================================================================ */

/**
 * @brief Update sickness behavior from immune state
 *
 * WHAT: Compute sickness-induced curiosity suppression
 * WHY:  Model cytokine effects on dopaminergic exploration
 * HOW:  Query cytokine levels, compute suppression factor, adjust curiosity
 *
 * BIOLOGY:
 * - IL-1β, IL-6, TNF-α reduce VTA dopamine release
 * - Cytokine storm can reduce exploration by 90%
 * - Effect is dose-dependent and cumulative
 *
 * @param bridge Curiosity-immune bridge
 * @return 0 on success
 */
int curiosity_immune_update_sickness_behavior(curiosity_immune_bridge_t* bridge);

/**
 * @brief Handle cytokine release event
 *
 * WHAT: Callback for immune system cytokine release
 * WHY:  Immediate response to pro-inflammatory signals
 * HOW:  Assess cytokine type, update sickness level, suppress curiosity
 *
 * @param bridge Curiosity-immune bridge
 * @param cytokine Cytokine that was released
 * @return 0 on success
 */
int curiosity_immune_on_cytokine_release(
    curiosity_immune_bridge_t* bridge,
    const brain_cytokine_t* cytokine
);

/**
 * @brief Handle inflammation event
 *
 * WHAT: Callback for immune inflammation state change
 * WHY:  Inflammation duration affects exploration drive
 * HOW:  Track inflammation onset, apply chronic suppression if needed
 *
 * @param bridge Curiosity-immune bridge
 * @param site Inflammation site
 * @return 0 on success
 */
int curiosity_immune_on_inflammation(
    curiosity_immune_bridge_t* bridge,
    const brain_inflammation_site_t* site
);

/**
 * @brief Compute sickness behavior level
 *
 * WHAT: Calculate current sickness behavior intensity
 * WHY:  Aggregate cytokine effects into single sickness metric
 * HOW:  Weighted sum of IL-1, IL-6, TNF-α, modulated by inflammation
 *
 * @param bridge Curiosity-immune bridge
 * @return Sickness level (0-1)
 */
float curiosity_immune_compute_sickness_level(
    const curiosity_immune_bridge_t* bridge
);

/**
 * @brief Apply curiosity suppression from sickness
 *
 * WHAT: Reduce curiosity baseline due to sickness behavior
 * WHY:  Model "conserve energy, avoid risk" strategy during illness
 * HOW:  Compute suppression factor, apply to curiosity engine
 *
 * @param bridge Curiosity-immune bridge
 * @param sickness_level Sickness level (0-1)
 * @return 0 on success
 */
int curiosity_immune_apply_suppression(
    curiosity_immune_bridge_t* bridge,
    float sickness_level
);

/**
 * @brief Restore curiosity after inflammation resolution
 *
 * WHAT: Recover exploration drive after IL-10 anti-inflammatory response
 * WHY:  Model recovery from sickness behavior
 * HOW:  Gradually restore baseline curiosity over time
 *
 * @param bridge Curiosity-immune bridge
 * @return 0 on success
 */
int curiosity_immune_restore_curiosity(curiosity_immune_bridge_t* bridge);

/* ============================================================================
 * Curiosity → Immune (Novelty Vigilance)
 * ============================================================================ */

/**
 * @brief Update immune vigilance from curiosity state
 *
 * WHAT: Compute novelty-induced immune alertness
 * WHY:  Model stress response to novel stimuli
 * HOW:  Query novelty level from curiosity, boost immune sensitivity
 *
 * BIOLOGY:
 * - Novel environments elevate cortisol
 * - Cortisol primes immune system for threats
 * - Exploratory behavior increases immune alertness
 *
 * @param bridge Curiosity-immune bridge
 * @return 0 on success
 */
int curiosity_immune_update_novelty_vigilance(curiosity_immune_bridge_t* bridge);

/**
 * @brief Trigger immune vigilance from high novelty
 *
 * WHAT: Activate immune alertness when novelty exceeds threshold
 * WHY:  Novel = potentially dangerous, prime immune system
 * HOW:  Boost immune recognition thresholds, increase cytokine sensitivity
 *
 * @param bridge Curiosity-immune bridge
 * @param novelty_level Novelty score (0-1)
 * @return 0 on success
 */
int curiosity_immune_trigger_vigilance(
    curiosity_immune_bridge_t* bridge,
    float novelty_level
);

/**
 * @brief Handle knowledge gap detection (novelty event)
 *
 * WHAT: Respond to curiosity detecting novel concept
 * WHY:  Knowledge gaps indicate unfamiliar territory → immune alert
 * HOW:  Assess gap size, trigger immune vigilance if significant
 *
 * @param bridge Curiosity-immune bridge
 * @param gap Knowledge gap detected by curiosity
 * @return 0 on success
 */
int curiosity_immune_on_knowledge_gap(
    curiosity_immune_bridge_t* bridge,
    const knowledge_gap_t* gap
);

/**
 * @brief Compute novelty-based immune boost
 *
 * WHAT: Calculate immune sensitivity multiplier from curiosity state
 * WHY:  Translate exploration drive into immune alertness
 * HOW:  Map curiosity intensity to immune boost factor (1.0-1.5)
 *
 * @param bridge Curiosity-immune bridge
 * @return Immune boost multiplier (1.0-1.5)
 */
float curiosity_immune_compute_novelty_boost(
    const curiosity_immune_bridge_t* bridge
);

/* ============================================================================
 * Update and Query API
 * ============================================================================ */

/**
 * @brief Update curiosity-immune bridge state
 *
 * WHAT: Periodic update for bidirectional coupling
 * WHY:  Maintain synchronized immune-curiosity state
 * HOW:  Update sickness behavior, update novelty vigilance
 *
 * @param bridge Curiosity-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int curiosity_immune_bridge_update(
    curiosity_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/**
 * @brief Get current sickness behavior level
 *
 * @param bridge Curiosity-immune bridge
 * @return Sickness level (0-1)
 */
float curiosity_immune_get_sickness_level(
    const curiosity_immune_bridge_t* bridge
);

/**
 * @brief Get curiosity suppression factor
 *
 * @param bridge Curiosity-immune bridge
 * @return Suppression multiplier (0-1, where 0=max suppression)
 */
float curiosity_immune_get_suppression_factor(
    const curiosity_immune_bridge_t* bridge
);

/**
 * @brief Get immune vigilance boost
 *
 * @param bridge Curiosity-immune bridge
 * @return Vigilance boost multiplier (1.0-1.5)
 */
float curiosity_immune_get_vigilance_boost(
    const curiosity_immune_bridge_t* bridge
);

/**
 * @brief Check if in chronic inflammation state
 *
 * @param bridge Curiosity-immune bridge
 * @return true if chronic inflammation active
 */
bool curiosity_immune_is_chronic_inflammation(
    const curiosity_immune_bridge_t* bridge
);


/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_CURIOSITY
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int curiosity_immune_connect_bio_async(curiosity_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int curiosity_immune_disconnect_bio_async(curiosity_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool curiosity_immune_is_bio_async_connected(const curiosity_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CURIOSITY_IMMUNE_BRIDGE_H */
