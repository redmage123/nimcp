/**
 * @file nimcp_hypothalamus_internal_bus.h
 * @brief Internal message bus for bidirectional communication between hypothalamus modules
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Lightweight internal event bus for intra-hypothalamus module communication
 *
 * WHY: Biological hypothalamus has extensive cross-talk between subsystems:
 *      - Circadian rhythm affects hunger timing (ghrelin peaks before meals)
 *      - Stress (cortisol) suppresses appetite short-term
 *      - Fatigue reduces curiosity/exploration drive
 *      - Social connection (oxytocin) reduces threat sensitivity
 *      - Extreme hunger/thirst can trigger stress response
 *
 * HOW: Lightweight pub-sub within hypothalamus adapter, separate from the
 *      external orchestrator which handles bridges to other brain regions.
 *
 * ARCHITECTURE:
 *      ┌─────────────────────────────────────────────────────────────┐
 *      │                HYPOTHALAMUS INTERNAL BUS                     │
 *      ├─────────────────────────────────────────────────────────────┤
 *      │  Circadian ←→ Drives ←→ HPA Axis ←→ Homeostasis ←→ Autonomic │
 *      └─────────────────────────────────────────────────────────────┘
 *
 * THREAD SAFETY: All functions are thread-safe with internal mutex.
 *
 * PERFORMANCE:
 * - Publish: O(s) where s = number of subscribers for event type
 * - Subscribe: O(1) amortized
 * - Memory: Fixed allocation, no dynamic memory during operation
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HYPOTHALAMUS_INTERNAL_BUS_H
#define NIMCP_HYPOTHALAMUS_INTERNAL_BUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Maximum number of internal event subscribers */
#define HYPO_IBUS_MAX_SUBSCRIBERS 32

/** Maximum number of pending events in queue */
#define HYPO_IBUS_MAX_QUEUE 64

/** Maximum module name length */
#define HYPO_IBUS_MAX_NAME 32

/** Maximum event description length */
#define HYPO_IBUS_MAX_DESC 128

/* ============================================================================
 * INTERNAL MODULE TYPES
 * ============================================================================ */

/**
 * @brief Internal hypothalamus module identifiers
 *
 * These are the modules that communicate via the internal bus.
 * Different from external bridge types in the orchestrator.
 */
typedef enum {
    HYPO_IMOD_CIRCADIAN = 0,    /**< Circadian rhythm system */
    HYPO_IMOD_HPA_AXIS,         /**< HPA stress axis */
    HYPO_IMOD_DRIVES,           /**< Drive system (all 9 drives) */
    HYPO_IMOD_HOMEOSTASIS,      /**< Homeostatic regulation */
    HYPO_IMOD_AUTONOMIC,        /**< Autonomic nervous system */
    HYPO_IMOD_APPETITE,         /**< Appetite/hunger subsystem */
    HYPO_IMOD_HYDRATION,        /**< Hydration/thirst subsystem */
    HYPO_IMOD_THERMOREGULATION, /**< Temperature regulation */
    HYPO_IMOD_ALIGNMENT,        /**< Alignment safety system */
    HYPO_IMOD_COUNT
} hypo_internal_module_t;

/* ============================================================================
 * INTERNAL EVENT TYPES
 * ============================================================================ */

/**
 * @brief Internal event types for module cross-talk
 *
 * BIOLOGICAL BASIS: These represent real neuroendocrine signals
 */
typedef enum {
    /* Circadian Events */
    HYPO_IEVT_CIRCADIAN_PHASE_CHANGE = 0,  /**< Circadian phase transition */
    HYPO_IEVT_MELATONIN_ONSET,             /**< Melatonin secretion started */
    HYPO_IEVT_MELATONIN_OFFSET,            /**< Melatonin secretion ended */
    HYPO_IEVT_CORTISOL_AWAKENING,          /**< Morning cortisol surge */

    /* HPA Axis Events */
    HYPO_IEVT_STRESS_ONSET,                /**< Stress response initiated */
    HYPO_IEVT_STRESS_PEAK,                 /**< Stress reached peak level */
    HYPO_IEVT_STRESS_RECOVERY,             /**< Stress recovery phase */
    HYPO_IEVT_CORTISOL_ELEVATED,           /**< Cortisol above threshold */
    HYPO_IEVT_CORTISOL_NORMALIZED,         /**< Cortisol returned to baseline */

    /* Drive Events */
    HYPO_IEVT_DRIVE_THRESHOLD_CROSSED,     /**< Drive crossed urgency threshold */
    HYPO_IEVT_DRIVE_SATISFIED,             /**< Drive reached satisfaction */
    HYPO_IEVT_DRIVE_CONFLICT,              /**< Multiple drives competing */
    HYPO_IEVT_HUNGER_ONSET,                /**< Hunger drive activated */
    HYPO_IEVT_FATIGUE_ONSET,               /**< Fatigue drive activated */
    HYPO_IEVT_SAFETY_THREAT,               /**< Safety drive activated */

    /* Homeostatic Events */
    HYPO_IEVT_SETPOINT_DEVIATION,          /**< Significant deviation from setpoint */
    HYPO_IEVT_SETPOINT_RESTORED,           /**< Setpoint restored */
    HYPO_IEVT_TEMPERATURE_ALERT,           /**< Temperature out of range */

    /* Autonomic Events */
    HYPO_IEVT_SYMPATHETIC_ACTIVATION,      /**< Fight-or-flight activation */
    HYPO_IEVT_PARASYMPATHETIC_ACTIVATION,  /**< Rest-and-digest activation */
    HYPO_IEVT_AUTONOMIC_BALANCE_SHIFT,     /**< Shift in ANS balance */

    /* Alignment Events */
    HYPO_IEVT_ALIGNMENT_WARNING,           /**< Alignment constraint approaching */
    HYPO_IEVT_ALIGNMENT_VIOLATION,         /**< Alignment constraint violated */

    HYPO_IEVT_COUNT
} hypo_internal_event_type_t;

/* ============================================================================
 * EVENT DATA STRUCTURES
 * ============================================================================ */

/**
 * @brief Circadian event data
 */
typedef struct {
    uint32_t old_phase;         /**< Previous phase (0-7) */
    uint32_t new_phase;         /**< New phase (0-7) */
    float melatonin_level;      /**< Current melatonin [0, 1] */
    float cortisol_level;       /**< Current cortisol [0, 1] */
    float alertness;            /**< Current alertness [0, 1] */
    float sleep_pressure;       /**< Sleep pressure [0, 1] */
} hypo_ibus_circadian_data_t;

/**
 * @brief Stress/HPA event data
 */
typedef struct {
    float stress_level;         /**< Current stress [0, 1] */
    float cortisol_level;       /**< Cortisol level [0, 1] */
    float crh_level;            /**< CRH level [0, 1] */
    float acth_level;           /**< ACTH level [0, 1] */
    bool is_acute;              /**< Acute vs chronic stress */
    uint32_t stressor_type;     /**< Type of stressor */
} hypo_ibus_stress_data_t;

/**
 * @brief Drive event data
 */
typedef struct {
    uint32_t drive_type;        /**< Which drive (hypo_drive_type_t) */
    float drive_level;          /**< Current level [0, 1] */
    float urgency;              /**< Urgency weight [0, 1] */
    float deviation;            /**< Deviation from setpoint */
    bool is_satisfied;          /**< Whether drive is satisfied */
    uint32_t competing_drive;   /**< Competing drive if conflict */
} hypo_ibus_drive_data_t;

/**
 * @brief Homeostatic event data
 */
typedef struct {
    uint32_t variable_id;       /**< Homeostatic variable */
    float current_value;        /**< Current value */
    float setpoint;             /**< Target setpoint */
    float error;                /**< Current error */
    float output;               /**< Control output */
} hypo_ibus_homeostatic_data_t;

/**
 * @brief Autonomic event data
 */
typedef struct {
    float sympathetic_tone;     /**< Sympathetic activity [0, 1] */
    float parasympathetic_tone; /**< Parasympathetic activity [0, 1] */
    float balance;              /**< ANS balance [-1, 1] */
    float heart_rate_mod;       /**< Heart rate modulation */
} hypo_ibus_autonomic_data_t;

/**
 * @brief Alignment event data
 */
typedef struct {
    uint32_t constraint_id;     /**< Which constraint */
    float margin;               /**< Safety margin remaining */
    bool is_locked;             /**< Whether drives are locked */
    char description[64];       /**< Human-readable description */
} hypo_ibus_alignment_data_t;

/**
 * @brief Internal event structure
 */
typedef struct {
    hypo_internal_event_type_t type;    /**< Event type */
    hypo_internal_module_t source;       /**< Source module */
    uint64_t timestamp_us;               /**< Event timestamp */
    uint32_t sequence_id;                /**< Sequence number */

    /** Event-specific data */
    union {
        hypo_ibus_circadian_data_t circadian;
        hypo_ibus_stress_data_t stress;
        hypo_ibus_drive_data_t drive;
        hypo_ibus_homeostatic_data_t homeostatic;
        hypo_ibus_autonomic_data_t autonomic;
        hypo_ibus_alignment_data_t alignment;
        uint8_t raw[128];  /**< Raw data for custom events */
    } data;
} hypo_internal_event_t;

/* ============================================================================
 * CALLBACK AND SUBSCRIPTION
 * ============================================================================ */

/**
 * @brief Callback function for internal events
 *
 * @param event The event data
 * @param user_data User-provided context
 * @return 0 on success, negative on error
 */
typedef int (*hypo_ibus_callback_t)(
    const hypo_internal_event_t* event,
    void* user_data
);

/**
 * @brief Modulation effect from an internal event
 *
 * Callbacks can return modulation effects that the bus applies
 * to other modules (e.g., cortisol reducing appetite).
 */
typedef struct {
    hypo_internal_module_t target;  /**< Target module to modulate */
    float modulation_factor;        /**< Modulation factor [0, 2] */
    uint32_t parameter_id;          /**< Which parameter to modulate */
    uint64_t duration_us;           /**< How long modulation lasts */
    bool is_additive;               /**< Additive vs multiplicative */
} hypo_ibus_modulation_t;

/* ============================================================================
 * CONFIGURATION
 * ============================================================================ */

/**
 * @brief Internal bus configuration
 */
typedef struct {
    uint32_t max_subscribers;       /**< Max subscribers (default: 32) */
    uint32_t max_queue_size;        /**< Max pending events (default: 64) */
    bool enable_async;              /**< Enable async delivery (default: false) */
    bool enable_modulation;         /**< Enable cross-modulation (default: true) */
    bool enable_logging;            /**< Log internal events (default: false) */

    /* Biological interaction parameters */
    float circadian_hunger_amplitude;   /**< How much circadian affects hunger [0, 1] */
    float circadian_fatigue_amplitude;  /**< How much circadian affects fatigue [0, 1] */
    float cortisol_appetite_suppression; /**< How much cortisol suppresses appetite [0, 1] */
    float fatigue_curiosity_reduction;  /**< How much fatigue reduces curiosity [0, 1] */
    float social_safety_modulation;     /**< How social reduces safety drive [0, 1] */
    float hunger_stress_threshold;      /**< Hunger level that triggers stress [0, 1] */
} hypo_ibus_config_t;

/**
 * @brief Internal bus statistics
 */
typedef struct {
    uint64_t events_published;      /**< Total events published */
    uint64_t events_delivered;      /**< Total events delivered to subscribers */
    uint64_t events_dropped;        /**< Events dropped (queue full) */
    uint64_t modulations_applied;   /**< Cross-modulations applied */
    uint32_t active_subscribers;    /**< Current subscriber count */
    uint32_t queue_depth;           /**< Current queue depth */
    uint32_t peak_queue_depth;      /**< Peak queue depth */

    /* Per-module statistics */
    uint64_t module_events[HYPO_IMOD_COUNT];  /**< Events from each module */
    uint64_t module_receives[HYPO_IMOD_COUNT]; /**< Events received by each module */
} hypo_ibus_stats_t;

/* ============================================================================
 * OPAQUE TYPE
 * ============================================================================ */

/**
 * @brief Opaque internal bus handle
 */
typedef struct hypo_internal_bus* hypo_ibus_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * Default biological parameters based on research:
 * - circadian_hunger_amplitude: 0.3 (30% hunger variation)
 * - circadian_fatigue_amplitude: 0.5 (50% fatigue variation)
 * - cortisol_appetite_suppression: 0.4 (40% appetite reduction under stress)
 * - fatigue_curiosity_reduction: 0.6 (60% curiosity reduction when tired)
 * - social_safety_modulation: 0.3 (30% safety drive reduction with social)
 * - hunger_stress_threshold: 0.85 (trigger stress at 85% hunger)
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int hypo_ibus_default_config(hypo_ibus_config_t* config);

/**
 * @brief Create internal bus
 *
 * @param config Configuration (NULL for defaults)
 * @return Bus handle, or NULL on error
 */
hypo_ibus_t hypo_ibus_create(const hypo_ibus_config_t* config);

/**
 * @brief Destroy internal bus
 *
 * @param bus Bus to destroy (NULL-safe)
 */
void hypo_ibus_destroy(hypo_ibus_t bus);

/**
 * @brief Reset internal bus state
 *
 * Clears pending events and resets statistics, keeps subscriptions.
 *
 * @param bus Internal bus
 * @return 0 on success, -1 on error
 */
int hypo_ibus_reset(hypo_ibus_t bus);

/* ============================================================================
 * SUBSCRIPTION API
 * ============================================================================ */

/**
 * @brief Subscribe to an internal event type
 *
 * @param bus Internal bus
 * @param subscriber Module subscribing
 * @param event_type Event type to subscribe to
 * @param callback Function to call when event occurs
 * @param user_data User context passed to callback
 * @return Subscription ID (>= 0), or -1 on error
 */
int hypo_ibus_subscribe(
    hypo_ibus_t bus,
    hypo_internal_module_t subscriber,
    hypo_internal_event_type_t event_type,
    hypo_ibus_callback_t callback,
    void* user_data
);

/**
 * @brief Subscribe to all events from a module
 *
 * @param bus Internal bus
 * @param subscriber Module subscribing
 * @param source_module Module to receive events from
 * @param callback Function to call
 * @param user_data User context
 * @return Subscription ID (>= 0), or -1 on error
 */
int hypo_ibus_subscribe_to_module(
    hypo_ibus_t bus,
    hypo_internal_module_t subscriber,
    hypo_internal_module_t source_module,
    hypo_ibus_callback_t callback,
    void* user_data
);

/**
 * @brief Unsubscribe from events
 *
 * @param bus Internal bus
 * @param subscription_id ID returned from subscribe
 * @return 0 on success, -1 on error
 */
int hypo_ibus_unsubscribe(hypo_ibus_t bus, int subscription_id);

/**
 * @brief Unsubscribe all subscriptions for a module
 *
 * @param bus Internal bus
 * @param module Module to unsubscribe
 * @return Number of subscriptions removed, or -1 on error
 */
int hypo_ibus_unsubscribe_module(hypo_ibus_t bus, hypo_internal_module_t module);

/* ============================================================================
 * PUBLISHING API
 * ============================================================================ */

/**
 * @brief Publish an internal event
 *
 * Synchronously delivers event to all subscribers.
 *
 * @param bus Internal bus
 * @param event Event to publish
 * @return Number of subscribers notified, or -1 on error
 */
int hypo_ibus_publish(hypo_ibus_t bus, const hypo_internal_event_t* event);

/**
 * @brief Publish circadian phase change
 *
 * Convenience function for circadian module.
 *
 * @param bus Internal bus
 * @param old_phase Previous phase
 * @param new_phase New phase
 * @param melatonin Current melatonin level
 * @param cortisol Current cortisol level
 * @param alertness Current alertness
 * @return 0 on success, -1 on error
 */
int hypo_ibus_publish_circadian_phase(
    hypo_ibus_t bus,
    uint32_t old_phase,
    uint32_t new_phase,
    float melatonin,
    float cortisol,
    float alertness
);

/**
 * @brief Publish stress event
 *
 * @param bus Internal bus
 * @param event_type STRESS_ONSET, STRESS_PEAK, or STRESS_RECOVERY
 * @param stress_level Current stress [0, 1]
 * @param cortisol Cortisol level [0, 1]
 * @param is_acute Acute vs chronic
 * @return 0 on success, -1 on error
 */
int hypo_ibus_publish_stress(
    hypo_ibus_t bus,
    hypo_internal_event_type_t event_type,
    float stress_level,
    float cortisol,
    bool is_acute
);

/**
 * @brief Publish drive event
 *
 * @param bus Internal bus
 * @param event_type DRIVE_THRESHOLD_CROSSED, DRIVE_SATISFIED, etc.
 * @param drive_type Which drive
 * @param drive_level Current level [0, 1]
 * @param urgency Urgency weight [0, 1]
 * @return 0 on success, -1 on error
 */
int hypo_ibus_publish_drive(
    hypo_ibus_t bus,
    hypo_internal_event_type_t event_type,
    uint32_t drive_type,
    float drive_level,
    float urgency
);

/**
 * @brief Publish autonomic event
 *
 * @param bus Internal bus
 * @param event_type SYMPATHETIC_ACTIVATION, etc.
 * @param sympathetic Sympathetic tone [0, 1]
 * @param parasympathetic Parasympathetic tone [0, 1]
 * @return 0 on success, -1 on error
 */
int hypo_ibus_publish_autonomic(
    hypo_ibus_t bus,
    hypo_internal_event_type_t event_type,
    float sympathetic,
    float parasympathetic
);

/* ============================================================================
 * MODULATION API
 * ============================================================================ */

/**
 * @brief Register a cross-module modulation rule
 *
 * Example: "When cortisol > 0.7, reduce appetite by 40%"
 *
 * @param bus Internal bus
 * @param source_event Event that triggers modulation
 * @param modulation Modulation to apply
 * @return Rule ID (>= 0), or -1 on error
 */
int hypo_ibus_register_modulation(
    hypo_ibus_t bus,
    hypo_internal_event_type_t source_event,
    const hypo_ibus_modulation_t* modulation
);

/**
 * @brief Get current modulation factor for a module
 *
 * @param bus Internal bus
 * @param module Module to query
 * @param parameter_id Parameter to query (0 for overall)
 * @return Modulation factor [0, 2], or 1.0 if no modulation
 */
float hypo_ibus_get_modulation(
    hypo_ibus_t bus,
    hypo_internal_module_t module,
    uint32_t parameter_id
);

/**
 * @brief Update modulation decay
 *
 * Call periodically to decay time-limited modulations.
 *
 * @param bus Internal bus
 * @param delta_us Time since last update in microseconds
 * @return 0 on success, -1 on error
 */
int hypo_ibus_update_modulations(hypo_ibus_t bus, uint64_t delta_us);

/**
 * @brief Clear all active modulations
 *
 * @param bus Internal bus
 * @return 0 on success, -1 on error
 */
int hypo_ibus_clear_modulations(hypo_ibus_t bus);

/* ============================================================================
 * QUERY API
 * ============================================================================ */

/**
 * @brief Get bus statistics
 *
 * @param bus Internal bus
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int hypo_ibus_get_stats(hypo_ibus_t bus, hypo_ibus_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param bus Internal bus
 * @return 0 on success, -1 on error
 */
int hypo_ibus_reset_stats(hypo_ibus_t bus);

/**
 * @brief Check if module has subscribers
 *
 * @param bus Internal bus
 * @param module Module to check
 * @return true if module has subscribers
 */
bool hypo_ibus_has_subscribers(hypo_ibus_t bus, hypo_internal_module_t module);

/**
 * @brief Get subscriber count for event type
 *
 * @param bus Internal bus
 * @param event_type Event type to query
 * @return Number of subscribers
 */
uint32_t hypo_ibus_subscriber_count(
    hypo_ibus_t bus,
    hypo_internal_event_type_t event_type
);

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

/**
 * @brief Get module name
 *
 * @param module Module type
 * @return Human-readable name (never NULL)
 */
const char* hypo_ibus_module_name(hypo_internal_module_t module);

/**
 * @brief Get event type name
 *
 * @param event_type Event type
 * @return Human-readable name (never NULL)
 */
const char* hypo_ibus_event_name(hypo_internal_event_type_t event_type);

/**
 * @brief Print bus summary to stdout
 *
 * @param bus Internal bus (NULL-safe)
 */
void hypo_ibus_print_summary(hypo_ibus_t bus);

/**
 * @brief Print statistics to stdout
 *
 * @param stats Statistics (NULL-safe)
 */
void hypo_ibus_print_stats(const hypo_ibus_stats_t* stats);

/* ============================================================================
 * BUILT-IN BIOLOGICAL MODULATION RULES
 * ============================================================================ */

/**
 * @brief Register default biological modulation rules
 *
 * Registers the following biologically-inspired rules:
 * 1. Circadian → Hunger: Meal-time hunger peaks
 * 2. Circadian → Fatigue: Nighttime fatigue increase
 * 3. Cortisol → Appetite: Stress suppresses appetite
 * 4. Fatigue → Curiosity: Tiredness reduces exploration
 * 5. Social → Safety: Social connection reduces threat sensitivity
 * 6. Hunger → Stress: Extreme hunger triggers stress
 * 7. Temperature → Fatigue: Hyperthermia increases drowsiness
 *
 * @param bus Internal bus
 * @return Number of rules registered, or -1 on error
 */
int hypo_ibus_register_default_modulations(hypo_ibus_t bus);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPOTHALAMUS_INTERNAL_BUS_H */
