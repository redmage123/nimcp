/**
 * @file nimcp_mirror_immune_integration.h
 * @brief Mirror Neuron - Immune System Integration
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional integration between mirror neurons and brain immune system
 * WHY:  Model biological interactions: illness reduces empathy, social rejection triggers inflammation
 * HOW:  Inflammation modulates mirror neuron resonance; social isolation activates immune response
 *
 * BIOLOGICAL BASIS:
 * ==============================================================================
 * 1. Sickness Behavior & Social Withdrawal (Immune → Mirror)
 *    - Inflammation (IL-1β, IL-6, TNF-α) reduces social motivation
 *    - Mirror neuron activity decreases during illness
 *    - Empathic resonance suppressed to conserve energy during infection
 *    - Reference: Dantzer et al. (2008) "From inflammation to sickness and depression"
 *
 * 2. Social Pain & Immune Activation (Mirror → Immune)
 *    - Social rejection activates inflammatory pathways
 *    - Social isolation increases IL-6, CRP (C-reactive protein)
 *    - Mirror neuron dysfunction correlates with immune dysregulation
 *    - Reference: Eisenberger et al. (2010) "Inflammation and social experience"
 *
 * 3. Cytokine Effects on Social Cognition
 *    - Pro-inflammatory cytokines reduce empathy and social processing
 *    - Anti-inflammatory cytokines (IL-10) restore social function
 *    - Reference: Harrison et al. (2009) "Inflammation causes mood changes"
 *
 * INTEGRATION MECHANISMS:
 * ==============================================================================
 * Immune → Mirror Neuron:
 *   - Cytokine level → Resonance suppression (BG inhibition boost)
 *   - Inflammation severity → Observation mode deactivation
 *   - IL-1/IL-6/TNF-α → Reduce motor resonance threshold
 *   - IL-10 (anti-inflammatory) → Restore normal resonance
 *
 * Mirror Neuron → Immune:
 *   - Social isolation detection → IL-6 release (pro-inflammatory)
 *   - Lack of observation activity → Stress cytokines
 *   - Failed imitation → Inflammatory response
 *   - Successful social learning → IL-10 release (anti-inflammatory)
 *
 * ARCHITECTURE:
 * ==============================================================================
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                   MIRROR NEURON SYSTEM                           │
 * │  ┌──────────────┐  ┌──────────────┐  ┌─────────────────────┐   │
 * │  │ Observation  │  │   Resonance  │  │    Hierarchy        │   │
 * │  │   System     │  │    System    │  │    (Goals/Motor)    │   │
 * │  └──────┬───────┘  └──────┬───────┘  └──────────┬──────────┘   │
 * │         │                 │                     │              │
 * │         └─────────────────┼─────────────────────┘              │
 * │                           ▼                                    │
 * │               ┌───────────────────────┐                        │
 * │               │ IMMUNE INTEGRATION    │                        │
 * │               │  - Cytokine sensing   │                        │
 * │               │  - Social monitoring  │                        │
 * │               └───────────┬───────────┘                        │
 * └───────────────────────────┼────────────────────────────────────┘
 *                             │
 *                 ┌───────────┴────────────┐
 *                 │ BRAIN IMMUNE SYSTEM    │
 *                 │  - Cytokine release    │
 *                 │  - Inflammation sites  │
 *                 │  - T/B cell activation │
 *                 └────────────────────────┘
 *
 * KEY FEATURES:
 * ==============================================================================
 * 1. Cytokine-gated Resonance
 *    - Pro-inflammatory → Suppress empathic resonance
 *    - Anti-inflammatory → Restore resonance
 *
 * 2. Social Isolation Detection
 *    - Monitor observation activity
 *    - Trigger IL-6 on prolonged isolation
 *
 * 3. Empathy Threshold Modulation
 *    - Inflammation raises execution threshold
 *    - Reduces automatic imitation
 *
 * 4. Resolution Feedback
 *    - Successful social interaction → IL-10 (anti-inflammatory)
 *    - Reduces inflammation sites
 *
 * NIMCP CODING STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT/WHY/HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free
 *
 * @see nimcp_mirror_neurons.h
 * @see nimcp_brain_immune.h
 */

#ifndef NIMCP_MIRROR_IMMUNE_INTEGRATION_H
#define NIMCP_MIRROR_IMMUNE_INTEGRATION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Include brain immune types */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct mirror_neurons_system* mirror_neurons_t;
/* brain_immune_system_t is defined in nimcp_brain_immune.h - don't redefine */
typedef struct mirror_immune_integration_system mirror_immune_integration_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Social isolation threshold (seconds without observation) */
#define MIRROR_IMMUNE_ISOLATION_THRESHOLD_S     300.0f  /* 5 minutes */

/** @brief Cytokine sensitivity for resonance suppression */
#define MIRROR_IMMUNE_CYTOKINE_SENSITIVITY      0.5f

/** @brief Max inflammation-induced resonance suppression */
#define MIRROR_IMMUNE_MAX_SUPPRESSION           0.8f

/** @brief IL-10 release on successful social interaction */
#define MIRROR_IMMUNE_IL10_RELEASE_AMOUNT       0.3f

/** @brief IL-6 release on social isolation */
#define MIRROR_IMMUNE_IL6_ISOLATION_AMOUNT      0.5f

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Social state tracked by integration
 *
 * WHAT: Current social engagement state
 * WHY:  Determine when to trigger immune responses
 */
typedef enum {
    SOCIAL_STATE_ENGAGED = 0,      /**< Active social observation/imitation */
    SOCIAL_STATE_PASSIVE,          /**< Observing but not imitating */
    SOCIAL_STATE_ISOLATED,         /**< No social activity detected */
    SOCIAL_STATE_REJECTED          /**< Failed social interactions */
} mirror_social_state_t;

/**
 * @brief Immune modulation effect on mirror neurons
 *
 * WHAT: Type of immune effect on social processing
 * WHY:  Different immune states have different effects
 */
typedef enum {
    IMMUNE_EFFECT_NONE = 0,        /**< No immune modulation */
    IMMUNE_EFFECT_SICKNESS,        /**< Sickness behavior withdrawal */
    IMMUNE_EFFECT_STRESS,          /**< Stress-induced social avoidance */
    IMMUNE_EFFECT_RECOVERY,        /**< Recovering social function */
    IMMUNE_EFFECT_HEALTHY          /**< Normal immune state */
} mirror_immune_effect_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Configuration for mirror-immune integration
 *
 * WHAT: Integration parameters
 * WHY:  Allow customization of bidirectional effects
 */
typedef struct {
    /* Immune → Mirror modulation */
    float cytokine_sensitivity;          /**< How sensitive to cytokines (default: 0.5) */
    float max_resonance_suppression;     /**< Max suppression from inflammation (default: 0.8) */
    bool enable_sickness_behavior;       /**< Enable sickness withdrawal (default: true) */
    float il1_suppression_gain;          /**< IL-1β effect gain (default: 0.4) */
    float il6_suppression_gain;          /**< IL-6 effect gain (default: 0.3) */
    float tnf_suppression_gain;          /**< TNF-α effect gain (default: 0.5) */
    float il10_restoration_gain;         /**< IL-10 restoration gain (default: 0.6) */

    /* Mirror → Immune feedback */
    bool enable_isolation_detection;     /**< Monitor social isolation (default: true) */
    float isolation_threshold_s;         /**< Isolation time threshold (default: 300s) */
    float isolation_il6_release;         /**< IL-6 released on isolation (default: 0.5) */
    float rejection_inflammation_gain;   /**< Failed imitation → inflammation (default: 0.3) */
    bool enable_social_recovery;         /**< Social success → IL-10 (default: true) */
    float social_success_il10_release;   /**< IL-10 on success (default: 0.3) */

    /* Thresholds */
    float empathy_threshold_baseline;    /**< Baseline empathy threshold (default: 0.5) */
    float inflammation_threshold;        /**< Min inflammation for effect (default: 0.2) */

    /* Update timing */
    uint64_t update_interval_ms;         /**< Integration update rate (default: 1000ms) */

    /* Bio-async */
    bool bio_async_enabled;              /**< Enable bio-async messaging (default: true) */

} mirror_immune_config_t;

/**
 * @brief Mirror-immune integration state
 *
 * WHAT: Current integration state
 * WHY:  Track bidirectional influences over time
 */
typedef struct {
    /* Social state tracking */
    mirror_social_state_t social_state;
    uint64_t last_observation_time;      /**< Last mirror observation (microseconds) */
    uint64_t last_imitation_time;        /**< Last successful imitation */
    uint64_t isolation_start_time;       /**< When isolation began (0 if not isolated) */
    uint32_t failed_imitation_count;     /**< Recent failed imitations */

    /* Immune state tracking */
    mirror_immune_effect_t immune_effect;
    float current_cytokine_level;        /**< Aggregate cytokine level (0-1) */
    float current_inflammation;          /**< Current inflammation severity (0-1) */
    float resonance_suppression;         /**< Current resonance suppression (0-1) */
    float empathy_threshold_mod;         /**< Empathy threshold modifier */

    /* Cytokine levels (tracked from immune system) */
    float il1_level;                     /**< IL-1β pro-inflammatory */
    float il6_level;                     /**< IL-6 pro-inflammatory */
    float tnf_level;                     /**< TNF-α pro-inflammatory */
    float il10_level;                    /**< IL-10 anti-inflammatory */

    /* Statistics */
    uint32_t isolation_events;           /**< Times isolation detected */
    uint32_t immune_triggered_il6;       /**< IL-6 releases from isolation */
    uint32_t immune_triggered_il10;      /**< IL-10 releases from social success */
    uint32_t sickness_behavior_events;   /**< Times sickness behavior triggered */

} mirror_immune_state_t;

/**
 * @brief Mirror-immune integration system
 *
 * WHAT: Complete integration subsystem
 * WHY:  Manage bidirectional communication
 */
struct mirror_immune_integration_system {
    mirror_immune_config_t config;
    mirror_immune_state_t state;

    /* References to integrated systems */
    mirror_neurons_t mirror_system;
    brain_immune_system_t* immune_system;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Active state */
    bool enabled;
    uint64_t last_update_time;

    /* Bio-async registration */
    bool bio_async_registered;
    uint32_t handler_id;
};

/**
 * @brief Integration statistics
 *
 * WHAT: Runtime statistics for integration
 * WHY:  Monitor bidirectional effects
 */
typedef struct {
    /* Immune → Mirror effects */
    uint32_t resonance_suppression_events;
    float avg_cytokine_level;
    float avg_resonance_suppression;
    uint32_t sickness_behavior_activations;

    /* Mirror → Immune effects */
    uint32_t isolation_detections;
    uint32_t il6_releases;
    uint32_t il10_releases;
    float avg_social_activity;

    /* State distribution */
    uint32_t time_engaged;            /**< Time in engaged state (ms) */
    uint32_t time_isolated;           /**< Time in isolated state (ms) */
    uint32_t time_rejected;           /**< Time in rejected state (ms) */

} mirror_immune_stats_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biological defaults
 * HOW:  Return struct with literature-based parameters
 *
 * @param config Output configuration
 * @return 0 on success
 */
int mirror_immune_get_default_config(mirror_immune_config_t* config);

/**
 * @brief Create mirror-immune integration
 *
 * WHAT: Initialize bidirectional integration system
 * WHY:  Enable immune-social coupling
 * HOW:  Allocate integration state, store system references
 *
 * @param config Configuration (NULL for defaults)
 * @param mirror_system Mirror neuron system
 * @param immune_system Brain immune system
 * @return Integration handle or NULL on failure
 */
mirror_immune_integration_t* mirror_immune_create(
    const mirror_immune_config_t* config,
    mirror_neurons_t mirror_system,
    brain_immune_system_t* immune_system
);

/**
 * @brief Destroy integration
 *
 * WHAT: Clean up integration resources
 * WHY:  Proper resource deallocation
 * HOW:  Free state, release mutex
 *
 * @param integration Integration to destroy (NULL-safe)
 */
void mirror_immune_destroy(mirror_immune_integration_t* integration);

/**
 * @brief Enable integration
 *
 * WHAT: Activate bidirectional coupling
 * WHY:  Begin immune-social modulation
 * HOW:  Set enabled flag, reset timers
 *
 * @param integration Integration system
 * @return 0 on success
 */
int mirror_immune_enable(mirror_immune_integration_t* integration);

/**
 * @brief Disable integration
 *
 * WHAT: Deactivate bidirectional coupling
 * WHY:  Allow independent operation
 * HOW:  Clear enabled flag, reset modulation
 *
 * @param integration Integration system
 * @return 0 on success
 */
int mirror_immune_disable(mirror_immune_integration_t* integration);

/* ============================================================================
 * Immune → Mirror Neuron Modulation API
 * ============================================================================ */

/**
 * @brief Apply immune modulation to mirror neurons
 *
 * WHAT: Update mirror neuron parameters based on immune state
 * WHY:  Inflammation reduces empathic resonance
 * HOW:  Sample cytokine levels, modulate resonance and thresholds
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (mutex protected)
 *
 * @param integration Integration system
 * @return 0 on success
 */
int mirror_immune_apply_immune_modulation(mirror_immune_integration_t* integration);

/**
 * @brief Compute resonance suppression from cytokines
 *
 * WHAT: Calculate resonance suppression factor
 * WHY:  Pro-inflammatory cytokines reduce social motivation
 * HOW:  Weighted sum: IL-1β (0.4) + IL-6 (0.3) + TNF-α (0.5) - IL-10 (0.6)
 *
 * @param integration Integration system
 * @return Suppression factor (0-1)
 */
float mirror_immune_compute_resonance_suppression(
    const mirror_immune_integration_t* integration
);

/**
 * @brief Update empathy threshold based on inflammation
 *
 * WHAT: Increase execution threshold during inflammation
 * WHY:  Sickness behavior reduces automatic imitation
 * HOW:  Scale threshold by inflammation level
 *
 * @param integration Integration system
 * @return New empathy threshold
 */
float mirror_immune_compute_empathy_threshold(
    const mirror_immune_integration_t* integration
);

/**
 * @brief Apply sickness behavior to mirror system
 *
 * WHAT: Suppress observation mode and motor resonance
 * WHY:  Conserve energy during illness
 * HOW:  Boost BG inhibition, raise execution thresholds
 *
 * @param integration Integration system
 * @param severity Sickness severity (0-1)
 * @return 0 on success
 */
int mirror_immune_apply_sickness_behavior(
    mirror_immune_integration_t* integration,
    float severity
);

/**
 * @brief Restore social function after recovery
 *
 * WHAT: Gradually restore normal resonance
 * WHY:  IL-10 resolution restores social processing
 * HOW:  Reduce suppression, lower thresholds
 *
 * @param integration Integration system
 * @param il10_level IL-10 anti-inflammatory level (0-1)
 * @return 0 on success
 */
int mirror_immune_restore_social_function(
    mirror_immune_integration_t* integration,
    float il10_level
);

/* ============================================================================
 * Mirror Neuron → Immune System Feedback API
 * ============================================================================ */

/**
 * @brief Detect social isolation
 *
 * WHAT: Check if observation activity below threshold
 * WHY:  Prolonged isolation triggers inflammation
 * HOW:  Compare current time to last observation
 *
 * @param integration Integration system
 * @param current_time Current time (microseconds)
 * @return true if isolated
 */
bool mirror_immune_detect_isolation(
    mirror_immune_integration_t* integration,
    uint64_t current_time
);

/**
 * @brief Trigger immune response to social isolation
 *
 * WHAT: Release IL-6 on prolonged isolation
 * WHY:  Social isolation activates inflammatory pathways
 * HOW:  Release cytokine via brain immune system
 *
 * BIOLOGICAL BASIS: Eisenberger et al. (2010) - social rejection → inflammation
 *
 * @param integration Integration system
 * @return 0 on success
 */
int mirror_immune_trigger_isolation_response(
    mirror_immune_integration_t* integration
);

/**
 * @brief Trigger immune response to failed imitation
 *
 * WHAT: Release stress cytokines on imitation failure
 * WHY:  Social rejection induces inflammatory response
 * HOW:  Count failures, release IL-6 if threshold exceeded
 *
 * @param integration Integration system
 * @return 0 on success
 */
int mirror_immune_trigger_rejection_response(
    mirror_immune_integration_t* integration
);

/**
 * @brief Release IL-10 on successful social interaction
 *
 * WHAT: Anti-inflammatory cytokine on positive social event
 * WHY:  Social success reduces inflammation
 * HOW:  Detect successful imitation, release IL-10
 *
 * @param integration Integration system
 * @return 0 on success
 */
int mirror_immune_release_social_success_il10(
    mirror_immune_integration_t* integration
);

/**
 * @brief Update social state from mirror neuron activity
 *
 * WHAT: Classify current social engagement state
 * WHY:  Determine appropriate immune feedback
 * HOW:  Analyze observation/execution activity patterns
 *
 * @param integration Integration system
 * @param current_time Current time (microseconds)
 * @return 0 on success
 */
int mirror_immune_update_social_state(
    mirror_immune_integration_t* integration,
    uint64_t current_time
);

/* ============================================================================
 * Update and Query API
 * ============================================================================ */

/**
 * @brief Update integration state
 *
 * WHAT: Process bidirectional interactions
 * WHY:  Advance integration state machine
 * HOW:  Sample immune, update mirror, check social state
 *
 * Call this periodically (e.g., every 100ms) to maintain integration.
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 *
 * @param integration Integration system
 * @param current_time Current simulation time (microseconds)
 * @return 0 on success
 */
int mirror_immune_update(
    mirror_immune_integration_t* integration,
    uint64_t current_time
);

/**
 * @brief Get integration statistics
 *
 * WHAT: Retrieve runtime statistics
 * WHY:  Monitor integration health
 *
 * @param integration Integration system
 * @param stats Output statistics
 * @return 0 on success
 */
int mirror_immune_get_stats(
    mirror_immune_integration_t* integration,
    mirror_immune_stats_t* stats
);

/**
 * @brief Get current social state
 *
 * @param integration Integration system
 * @return Current social state
 */
mirror_social_state_t mirror_immune_get_social_state(
    const mirror_immune_integration_t* integration
);

/**
 * @brief Get current immune effect
 *
 * @param integration Integration system
 * @return Current immune effect on mirror neurons
 */
mirror_immune_effect_t mirror_immune_get_immune_effect(
    const mirror_immune_integration_t* integration
);

/**
 * @brief Get current resonance suppression
 *
 * @param integration Integration system
 * @return Suppression factor (0-1)
 */
float mirror_immune_get_resonance_suppression(
    const mirror_immune_integration_t* integration
);

/* ============================================================================
 * Event Notification API (for mirror neurons to call)
 * ============================================================================ */

/**
 * @brief Notify observation event
 *
 * WHAT: Mirror neurons notify observation occurrence
 * WHY:  Track social activity to detect isolation
 * HOW:  Update last observation timestamp
 *
 * @param integration Integration system
 * @param timestamp_us Observation time (microseconds)
 */
void mirror_immune_notify_observation(
    mirror_immune_integration_t* integration,
    uint64_t timestamp_us
);

/**
 * @brief Notify successful imitation
 *
 * WHAT: Mirror neurons notify successful imitation
 * WHY:  Trigger IL-10 release (anti-inflammatory)
 * HOW:  Update timestamp, trigger immune feedback
 *
 * @param integration Integration system
 * @param timestamp_us Imitation time (microseconds)
 */
void mirror_immune_notify_imitation_success(
    mirror_immune_integration_t* integration,
    uint64_t timestamp_us
);

/**
 * @brief Notify failed imitation
 *
 * WHAT: Mirror neurons notify imitation failure
 * WHY:  Count failures, trigger stress response
 * HOW:  Increment failure count, check threshold
 *
 * @param integration Integration system
 */
void mirror_immune_notify_imitation_failure(
    mirror_immune_integration_t* integration
);

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

const char* mirror_social_state_to_string(mirror_social_state_t state);
const char* mirror_immune_effect_to_string(mirror_immune_effect_t effect);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MIRROR_IMMUNE_INTEGRATION_H */
