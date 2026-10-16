/**
 * @file nimcp_mirror_hypothalamus_bridge.h
 * @brief Mirror Neuron - Hypothalamus Bidirectional Bridge
 * @version 1.0.0
 * @date 2025-01-05
 *
 * WHAT: Bidirectional integration between mirror neurons and hypothalamus
 * WHY:  Model biological interactions: stress/circadian affects social cognition,
 *       social states influence homeostatic/stress responses
 * HOW:  Hypothalamus state modulates mirroring; mirror activity triggers hormonal responses
 *
 * BIOLOGICAL BASIS:
 * ==============================================================================
 * 1. Hypothalamus -> Mirror Neuron Effects
 *    - Cortisol (stress) reduces social mirroring sensitivity
 *    - Circadian phase affects observation threshold (higher at night)
 *    - Drive states (hunger/thirst) suppress social learning to prioritize survival
 *    - Reference: Hermans et al. (2014) "Stress-related noradrenergic activity"
 *
 * 2. Mirror Neuron -> Hypothalamus Effects
 *    - Social isolation triggers stress response (HPA axis via IL-6)
 *    - Empathic arousal activates autonomic nervous system
 *    - Successful imitation sends reward signals (dopamine-mediated)
 *    - Reference: Porges (2007) "The polyvagal perspective"
 *
 * 3. Neuroendocrine-Social Coupling
 *    - Oxytocin release on positive social interaction
 *    - Cortisol surge on social rejection/isolation
 *    - Circadian gating of social receptivity
 *
 * ARCHITECTURE:
 * ==============================================================================
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                   MIRROR NEURON SYSTEM                          │
 * │  ┌──────────────┐  ┌──────────────┐  ┌─────────────────────┐   │
 * │  │ Observation  │  │   Resonance  │  │    Social State     │   │
 * │  │   System     │  │    System    │  │    Detection        │   │
 * │  └──────┬───────┘  └──────┬───────┘  └──────────┬──────────┘   │
 * │         │                 │                     │              │
 * │         └─────────────────┼─────────────────────┘              │
 * │                           ▼                                    │
 * │               ┌───────────────────────┐                        │
 * │               │  HYPOTHALAMUS BRIDGE  │                        │
 * │               │  - Stress modulation  │                        │
 * │               │  - Circadian gating   │                        │
 * │               │  - Drive suppression  │                        │
 * │               └───────────┬───────────┘                        │
 * └───────────────────────────┼────────────────────────────────────┘
 *                             │
 *                 ┌───────────┴────────────┐
 *                 │     HYPOTHALAMUS       │
 *                 │  - HPA axis (cortisol) │
 *                 │  - SCN (circadian)     │
 *                 │  - Autonomic control   │
 *                 │  - Drive states        │
 *                 └────────────────────────┘
 *
 * BIO-ASYNC MODULE ID: 0x0278 (MIRROR_HYPOTHALAMUS_BRIDGE)
 *
 * NIMCP CODING STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT/WHY/HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free
 *
 * @see nimcp_mirror_neurons.h
 * @see nimcp_hypothalamus_adapter.h
 */

#ifndef NIMCP_MIRROR_HYPOTHALAMUS_BRIDGE_H
#define NIMCP_MIRROR_HYPOTHALAMUS_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Forward declarations to avoid circular dependencies */
typedef struct mirror_neurons_system* mirror_neurons_t;
typedef struct hypothalamus_adapter hypothalamus_adapter_t;

/* Thread synchronization */
#include "utils/thread/nimcp_thread.h"
#include "utils/bridge/nimcp_bridge_base.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Bio-Async Module ID
 * ============================================================================ */

/* Note: BIO_MODULE_MIRROR_HYPOTHALAMUS_BRIDGE (0x0278) is defined in nimcp_bio_messages.h */

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Cortisol sensitivity for mirroring suppression */
#define MIRROR_HYPO_CORTISOL_SENSITIVITY        0.6f

/** @brief Max stress-induced mirroring suppression */
#define MIRROR_HYPO_MAX_STRESS_SUPPRESSION      0.7f

/** @brief Circadian observation threshold boost at night */
#define MIRROR_HYPO_CIRCADIAN_NIGHT_BOOST       0.3f

/** @brief Hunger drive threshold for social suppression */
#define MIRROR_HYPO_HUNGER_SUPPRESSION_THRESH   0.6f

/** @brief Thirst drive threshold for social suppression */
#define MIRROR_HYPO_THIRST_SUPPRESSION_THRESH   0.5f

/** @brief Drive-induced social learning suppression gain */
#define MIRROR_HYPO_DRIVE_SUPPRESSION_GAIN      0.4f

/** @brief Isolation stress response cortisol release */
#define MIRROR_HYPO_ISOLATION_STRESS_LEVEL      0.5f

/** @brief Empathic arousal autonomic activation gain */
#define MIRROR_HYPO_EMPATHIC_AROUSAL_GAIN       0.3f

/** @brief Successful imitation reward signal strength */
#define MIRROR_HYPO_IMITATION_REWARD_SIGNAL     0.4f

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Hypothalamus modulation effect on mirror neurons
 *
 * WHAT: Type of hypothalamic influence on social processing
 * WHY:  Different physiological states have different effects
 */
typedef enum {
    HYPO_EFFECT_NONE = 0,           /**< No hypothalamic modulation */
    HYPO_EFFECT_STRESS_SUPPRESSION, /**< Cortisol suppressing mirroring */
    HYPO_EFFECT_CIRCADIAN_GATING,   /**< Time-of-day observation gating */
    HYPO_EFFECT_DRIVE_OVERRIDE,     /**< Hunger/thirst suppressing social */
    HYPO_EFFECT_COMBINED            /**< Multiple effects active */
} mirror_hypo_effect_t;

/**
 * @brief Mirror neuron feedback to hypothalamus
 *
 * WHAT: Type of mirror-originated hypothalamic activation
 * WHY:  Social states trigger different hormonal responses
 */
typedef enum {
    MIRROR_FEEDBACK_NONE = 0,       /**< No feedback to hypothalamus */
    MIRROR_FEEDBACK_ISOLATION,      /**< Social isolation stress */
    MIRROR_FEEDBACK_AROUSAL,        /**< Empathic autonomic activation */
    MIRROR_FEEDBACK_REWARD,         /**< Successful imitation reward */
    MIRROR_FEEDBACK_REJECTION       /**< Social rejection stress */
} mirror_hypo_feedback_t;

/**
 * @brief Circadian social receptivity phase
 *
 * WHAT: Time-of-day modulation of social cognition
 * WHY:  Circadian rhythms affect social behavior in biological systems
 */
typedef enum {
    CIRCADIAN_SOCIAL_HIGH = 0,      /**< High social receptivity (morning) */
    CIRCADIAN_SOCIAL_MODERATE,      /**< Moderate receptivity (afternoon) */
    CIRCADIAN_SOCIAL_LOW,           /**< Low receptivity (evening) */
    CIRCADIAN_SOCIAL_MINIMAL        /**< Minimal receptivity (night) */
} circadian_social_phase_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Configuration for mirror-hypothalamus bridge
 *
 * WHAT: Bridge parameters for bidirectional integration
 * WHY:  Allow customization of neuroendocrine-social coupling
 */
typedef struct {
    /* Hypothalamus -> Mirror modulation */
    float cortisol_sensitivity;           /**< Sensitivity to cortisol (default: 0.6) */
    float max_stress_suppression;         /**< Max stress-induced suppression (default: 0.7) */
    bool enable_circadian_gating;         /**< Enable time-of-day modulation (default: true) */
    float circadian_night_threshold_boost; /**< Observation threshold boost at night (default: 0.3) */
    bool enable_drive_suppression;        /**< Enable hunger/thirst effects (default: true) */
    float hunger_suppression_threshold;   /**< Hunger level for suppression (default: 0.6) */
    float thirst_suppression_threshold;   /**< Thirst level for suppression (default: 0.5) */
    float drive_suppression_gain;         /**< Drive -> social suppression gain (default: 0.4) */

    /* Mirror -> Hypothalamus feedback */
    bool enable_isolation_stress;         /**< Social isolation triggers HPA (default: true) */
    float isolation_stress_level;         /**< Cortisol release on isolation (default: 0.5) */
    bool enable_empathic_arousal;         /**< Empathy activates autonomic (default: true) */
    float empathic_arousal_gain;          /**< Arousal -> autonomic gain (default: 0.3) */
    bool enable_imitation_reward;         /**< Successful imitation sends reward (default: true) */
    float imitation_reward_signal;        /**< Reward signal strength (default: 0.4) */

    /* Update timing */
    uint64_t update_interval_ms;          /**< Bridge update rate (default: 100ms) */

} mirror_hypo_config_t;

/**
 * @brief Mirror-hypothalamus bridge state
 *
 * WHAT: Current state of bidirectional integration
 * WHY:  Track modulation effects over time
 */
typedef struct {
    /* Hypothalamus -> Mirror state */
    mirror_hypo_effect_t current_effect;      /**< Active hypothalamic effect */
    float stress_suppression;                  /**< Current stress-induced suppression (0-1) */
    float circadian_threshold_mod;             /**< Circadian observation threshold modifier */
    float drive_suppression;                   /**< Current drive-induced suppression (0-1) */
    circadian_social_phase_t social_phase;    /**< Current circadian social phase */

    /* Sampled hypothalamus values */
    float cortisol_level;                      /**< Current cortisol from HPA axis */
    float hunger_drive;                        /**< Current hunger drive level */
    float thirst_drive;                        /**< Current thirst drive level */
    float sympathetic_tone;                    /**< Sympathetic nervous system activity */

    /* Mirror -> Hypothalamus state */
    mirror_hypo_feedback_t last_feedback;     /**< Last feedback sent to hypothalamus */
    uint64_t last_isolation_trigger;          /**< Last isolation stress trigger time */
    uint64_t last_arousal_trigger;            /**< Last empathic arousal trigger time */
    uint64_t last_reward_trigger;             /**< Last reward signal trigger time */

    /* Activity tracking */
    uint64_t last_observation_time;           /**< Last mirror observation timestamp */
    uint64_t last_imitation_time;             /**< Last successful imitation timestamp */
    uint32_t failed_imitation_count;          /**< Recent failed imitation count */
    float empathic_resonance_level;           /**< Current empathic resonance (0-1) */

} mirror_hypo_state_t;

/**
 * @brief Mirror-hypothalamus bridge statistics
 *
 * WHAT: Runtime statistics for integration monitoring
 * WHY:  Track bidirectional effects and health
 */
typedef struct {
    /* Hypothalamus -> Mirror effects */
    uint32_t stress_suppression_events;       /**< Times stress suppressed mirroring */
    uint32_t circadian_gating_events;         /**< Times circadian phase affected threshold */
    uint32_t drive_override_events;           /**< Times drives suppressed social learning */
    float avg_stress_suppression;             /**< Average stress suppression level */
    float avg_circadian_threshold_mod;        /**< Average circadian threshold modifier */

    /* Mirror -> Hypothalamus effects */
    uint32_t isolation_stress_triggers;       /**< Times isolation triggered stress */
    uint32_t empathic_arousal_triggers;       /**< Times empathy activated autonomic */
    uint32_t imitation_reward_signals;        /**< Times imitation sent reward */
    uint32_t rejection_stress_triggers;       /**< Times rejection triggered stress */

    /* Timing */
    uint64_t total_updates;                   /**< Total bridge updates */
    float avg_update_latency_us;              /**< Average update latency */

} mirror_hypo_stats_t;

/**
 * @brief Mirror-hypothalamus bridge system
 *
 * WHAT: Complete bidirectional bridge subsystem
 * WHY:  Manage neuroendocrine-social coupling
 */
typedef struct mirror_hypo_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    struct nimcp_health_agent* health_agent; /**< Instance-level health agent */
    mirror_hypo_config_t config;
    mirror_hypo_state_t state;
    mirror_hypo_stats_t stats;

    /* References to integrated systems */
    mirror_neurons_t mirror_system;
    hypothalamus_adapter_t* hypothalamus;

    /* Active state */
    bool enabled;
    uint64_t last_update_time;
    uint64_t creation_time;

} mirror_hypo_bridge_t;

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
 * @return 0 on success, -1 on error
 */
int mirror_hypo_get_default_config(mirror_hypo_config_t* config);

/**
 * @brief Create mirror-hypothalamus bridge
 *
 * WHAT: Initialize bidirectional bridge system
 * WHY:  Enable neuroendocrine-social coupling
 * HOW:  Allocate bridge state, store system references
 *
 * @param config Configuration (NULL for defaults)
 * @param mirror_system Mirror neuron system
 * @param hypothalamus Hypothalamus adapter
 * @return Bridge handle or NULL on failure
 */
mirror_hypo_bridge_t* mirror_hypo_create(
    const mirror_hypo_config_t* config,
    mirror_neurons_t mirror_system,
    hypothalamus_adapter_t* hypothalamus
);

/**
 * @brief Destroy bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free state, release mutex
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void mirror_hypo_destroy(mirror_hypo_bridge_t* bridge);

/**
 * @brief Enable bridge
 *
 * WHAT: Activate bidirectional coupling
 * WHY:  Begin neuroendocrine-social modulation
 * HOW:  Set enabled flag, reset timers
 *
 * @param bridge Bridge system
 * @return 0 on success, -1 on error
 */
int mirror_hypo_enable(mirror_hypo_bridge_t* bridge);

/**
 * @brief Disable bridge
 *
 * WHAT: Deactivate bidirectional coupling
 * WHY:  Allow independent operation
 * HOW:  Clear enabled flag, reset modulation
 *
 * @param bridge Bridge system
 * @return 0 on success, -1 on error
 */
int mirror_hypo_disable(mirror_hypo_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * WHAT: Clear all state and statistics
 * WHY:  Prepare for new simulation
 * HOW:  Zero state, reset counters
 *
 * @param bridge Bridge system
 * @return 0 on success, -1 on error
 */
int mirror_hypo_reset(mirror_hypo_bridge_t* bridge);

/* ============================================================================
 * Hypothalamus -> Mirror Neuron Modulation API
 * ============================================================================ */

/**
 * @brief Apply hypothalamus modulation to mirror neurons
 *
 * WHAT: Update mirror neuron parameters based on hypothalamus state
 * WHY:  Stress, circadian, and drives affect social cognition
 * HOW:  Sample hypothalamus, compute modulation, apply effects
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (mutex protected)
 *
 * @param bridge Bridge system
 * @return 0 on success, -1 on error
 */
int mirror_hypo_apply_modulation(mirror_hypo_bridge_t* bridge);

/**
 * @brief Compute stress-induced mirroring suppression
 *
 * WHAT: Calculate suppression from cortisol level
 * WHY:  Stress reduces social mirroring sensitivity
 * HOW:  Scale cortisol by sensitivity, clamp to max
 *
 * @param bridge Bridge system
 * @return Suppression factor (0-1)
 */
float mirror_hypo_compute_stress_suppression(const mirror_hypo_bridge_t* bridge);

/**
 * @brief Compute circadian observation threshold modifier
 *
 * WHAT: Calculate observation threshold adjustment by time of day
 * WHY:  Circadian phase affects social receptivity
 * HOW:  Map circadian phase to threshold modifier
 *
 * @param bridge Bridge system
 * @return Threshold modifier (0 = no change, positive = higher threshold)
 */
float mirror_hypo_compute_circadian_modifier(const mirror_hypo_bridge_t* bridge);

/**
 * @brief Compute drive-induced social suppression
 *
 * WHAT: Calculate suppression from hunger/thirst drives
 * WHY:  Survival drives override social learning
 * HOW:  Compare drive levels to thresholds, compute weighted suppression
 *
 * @param bridge Bridge system
 * @return Suppression factor (0-1)
 */
float mirror_hypo_compute_drive_suppression(const mirror_hypo_bridge_t* bridge);

/**
 * @brief Get current circadian social phase
 *
 * WHAT: Map hypothalamus circadian phase to social receptivity
 * WHY:  Different times of day have different social characteristics
 * HOW:  Query hypothalamus circadian, map to social phase
 *
 * @param bridge Bridge system
 * @return Current circadian social phase
 */
circadian_social_phase_t mirror_hypo_get_social_phase(const mirror_hypo_bridge_t* bridge);

/* ============================================================================
 * Mirror Neuron -> Hypothalamus Feedback API
 * ============================================================================ */

/**
 * @brief Trigger isolation stress response
 *
 * WHAT: Activate HPA axis on social isolation
 * WHY:  Prolonged isolation triggers stress hormones
 * HOW:  Call hypothalamus to apply stress, release cortisol
 *
 * BIOLOGICAL BASIS: Cacioppo (2002) - loneliness activates HPA axis
 *
 * @param bridge Bridge system
 * @return 0 on success, -1 on error
 */
int mirror_hypo_trigger_isolation_stress(mirror_hypo_bridge_t* bridge);

/**
 * @brief Trigger empathic arousal autonomic activation
 *
 * WHAT: Activate sympathetic nervous system on empathic arousal
 * WHY:  Observing distress activates autonomic response
 * HOW:  Modulate hypothalamus autonomic balance toward sympathetic
 *
 * BIOLOGICAL BASIS: Decety & Jackson (2004) - empathy and autonomic coupling
 *
 * @param bridge Bridge system
 * @param arousal_level Level of empathic arousal (0-1)
 * @return 0 on success, -1 on error
 */
int mirror_hypo_trigger_empathic_arousal(mirror_hypo_bridge_t* bridge, float arousal_level);

/**
 * @brief Send imitation reward signal
 *
 * WHAT: Send reward signal on successful imitation
 * WHY:  Successful social learning activates reward pathways
 * HOW:  Signal hypothalamus to modulate reward-related outputs
 *
 * @param bridge Bridge system
 * @return 0 on success, -1 on error
 */
int mirror_hypo_send_reward_signal(mirror_hypo_bridge_t* bridge);

/**
 * @brief Trigger social rejection stress
 *
 * WHAT: Activate stress response on repeated imitation failures
 * WHY:  Social rejection activates HPA axis
 * HOW:  Accumulate failures, trigger stress when threshold exceeded
 *
 * @param bridge Bridge system
 * @return 0 on success, -1 on error
 */
int mirror_hypo_trigger_rejection_stress(mirror_hypo_bridge_t* bridge);

/* ============================================================================
 * Update and Query API
 * ============================================================================ */

/**
 * @brief Update bridge state
 *
 * WHAT: Process bidirectional interactions
 * WHY:  Advance bridge state machine
 * HOW:  Sample systems, apply modulation, check triggers
 *
 * Call this periodically (e.g., every 100ms) to maintain integration.
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 *
 * @param bridge Bridge system
 * @param current_time Current simulation time (microseconds)
 * @return 0 on success, -1 on error
 */
int mirror_hypo_update(mirror_hypo_bridge_t* bridge, uint64_t current_time);

/**
 * @brief Get bridge statistics
 *
 * WHAT: Retrieve runtime statistics
 * WHY:  Monitor bridge health and effects
 *
 * @param bridge Bridge system
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int mirror_hypo_get_stats(mirror_hypo_bridge_t* bridge, mirror_hypo_stats_t* stats);

/**
 * @brief Get current bridge state
 *
 * WHAT: Retrieve current integration state
 * WHY:  Query modulation effects
 *
 * @param bridge Bridge system
 * @param state Output state
 * @return 0 on success, -1 on error
 */
int mirror_hypo_get_state(mirror_hypo_bridge_t* bridge, mirror_hypo_state_t* state);

/**
 * @brief Get current hypothalamus effect type
 *
 * @param bridge Bridge system
 * @return Current effect type
 */
mirror_hypo_effect_t mirror_hypo_get_effect(const mirror_hypo_bridge_t* bridge);

/**
 * @brief Get last feedback sent to hypothalamus
 *
 * @param bridge Bridge system
 * @return Last feedback type
 */
mirror_hypo_feedback_t mirror_hypo_get_last_feedback(const mirror_hypo_bridge_t* bridge);

/**
 * @brief Get total modulation suppression
 *
 * WHAT: Get combined suppression from all hypothalamic effects
 * WHY:  Single value for mirror neuron modulation
 * HOW:  Combine stress, circadian, and drive effects
 *
 * @param bridge Bridge system
 * @return Total suppression factor (0-1)
 */
float mirror_hypo_get_total_suppression(const mirror_hypo_bridge_t* bridge);

/* ============================================================================
 * Event Notification API (for mirror neurons to call)
 * ============================================================================ */

/**
 * @brief Notify observation event
 *
 * WHAT: Mirror neurons notify observation occurrence
 * WHY:  Track social activity for isolation detection
 * HOW:  Update last observation timestamp
 *
 * @param bridge Bridge system
 * @param timestamp_us Observation time (microseconds)
 */
void mirror_hypo_notify_observation(mirror_hypo_bridge_t* bridge, uint64_t timestamp_us);

/**
 * @brief Notify successful imitation
 *
 * WHAT: Mirror neurons notify successful imitation
 * WHY:  Trigger reward signal to hypothalamus
 * HOW:  Update timestamp, send reward signal
 *
 * @param bridge Bridge system
 * @param timestamp_us Imitation time (microseconds)
 */
void mirror_hypo_notify_imitation_success(mirror_hypo_bridge_t* bridge, uint64_t timestamp_us);

/**
 * @brief Notify failed imitation
 *
 * WHAT: Mirror neurons notify imitation failure
 * WHY:  Track failures for rejection stress
 * HOW:  Increment failure count
 *
 * @param bridge Bridge system
 */
void mirror_hypo_notify_imitation_failure(mirror_hypo_bridge_t* bridge);

/**
 * @brief Notify empathic resonance level
 *
 * WHAT: Mirror neurons notify current empathic resonance
 * WHY:  Trigger autonomic arousal if resonance high
 * HOW:  Update resonance level, check arousal threshold
 *
 * @param bridge Bridge system
 * @param resonance_level Current resonance level (0-1)
 */
void mirror_hypo_notify_empathic_resonance(mirror_hypo_bridge_t* bridge, float resonance_level);

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

/**
 * @brief Convert hypothalamus effect to string
 *
 * @param effect Effect type
 * @return Human-readable string
 */
const char* mirror_hypo_effect_to_string(mirror_hypo_effect_t effect);

/**
 * @brief Convert mirror feedback to string
 *
 * @param feedback Feedback type
 * @return Human-readable string
 */
const char* mirror_hypo_feedback_to_string(mirror_hypo_feedback_t feedback);

/**
 * @brief Convert circadian social phase to string
 *
 * @param phase Social phase
 * @return Human-readable string
 */
const char* circadian_social_phase_to_string(circadian_social_phase_t phase);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MIRROR_HYPOTHALAMUS_BRIDGE_H */
