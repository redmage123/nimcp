//=============================================================================
// nimcp_routing_immune.h - Brain Immune Integration with Routing and Events
//=============================================================================

#ifndef NIMCP_ROUTING_IMMUNE_H
#define NIMCP_ROUTING_IMMUNE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "middleware/routing/nimcp_thalamic_router.h"
#include "middleware/routing/nimcp_attention_gate.h"
#include "middleware/events/nimcp_event_bus.h"
#include "cognitive/immune/nimcp_brain_immune.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nimcp_routing_immune.h
 * @brief Integration of brain immune system with routing and event modules
 *
 * WHAT: Bidirectional integration between immune system and signal routing
 * WHY:  Inflammation affects routing; routing anomalies trigger immune alerts
 * HOW:  Immune inflammation modulates routing priorities and attention weights;
 *       routing anomalies are presented as antigens to immune system
 *
 * BIOLOGICAL BASIS:
 * - Thalamic gating is modulated by inflammatory states (cytokines affect neural excitability)
 * - Immune signaling uses neural pathways (vagus nerve, sympathetic nervous system)
 * - Inflammation alters attention allocation (pain/threat capture attention)
 * - Neural dysfunction triggers microglial immune response (routing failures → immune activation)
 *
 * INTEGRATION MODEL:
 * ```
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                   ROUTING IMMUNE INTEGRATION                     │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                  │
 * │  IMMUNE → ROUTING:                                              │
 * │  ┌────────────────┐           ┌──────────────────────────┐     │
 * │  │ Inflammation   │  ────────▶│ Routing Priority Boost   │     │
 * │  │   LOCAL        │           │ (Normal + 10%)           │     │
 * │  │   REGIONAL     │           │ (Normal + 30%)           │     │
 * │  │   SYSTEMIC     │           │ (Normal + 60%)           │     │
 * │  │   STORM        │           │ (Emergency routing)      │     │
 * │  └────────────────┘           └──────────────────────────┘     │
 * │                                                                  │
 * │  ┌────────────────┐           ┌──────────────────────────┐     │
 * │  │ Cytokines      │  ────────▶│ Attention Modulation     │     │
 * │  │   IL-1/IL-6    │           │ (Pro-inflammatory boost) │     │
 * │  │   IL-10        │           │ (Anti-inflammatory calm) │     │
 * │  │   TNF-α/IFN-γ  │           │ (High alert state)       │     │
 * │  └────────────────┘           └──────────────────────────┘     │
 * │                                                                  │
 * │  ROUTING → IMMUNE:                                              │
 * │  ┌────────────────┐           ┌──────────────────────────┐     │
 * │  │ Anomalies      │  ────────▶│ Antigen Presentation     │     │
 * │  │  Drop rate↑    │           │ (Routing threat)         │     │
 * │  │  Latency↑      │           │                          │     │
 * │  │  Queue full    │           │                          │     │
 * │  └────────────────┘           └──────────────────────────┘     │
 * │                                                                  │
 * │  ┌────────────────┐           ┌──────────────────────────┐     │
 * │  │ Pattern Events │  ────────▶│ Event Bus Integration    │     │
 * │  │  Attention↓    │           │ (Immune event types)     │     │
 * │  │  Signal fails  │           │                          │     │
 * │  └────────────────┘           └──────────────────────────┘     │
 * │                                                                  │
 * └─────────────────────────────────────────────────────────────────┘
 * ```
 *
 * DESIGN PATTERNS:
 * - Observer: Immune system observes routing metrics
 * - Strategy: Inflammation-based routing modulation strategies
 * - Mediator: Routing immune bridge coordinates both systems
 *
 * NIMCP STANDARDS:
 * - Functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutexes
 * - nimcp_malloc/nimcp_free memory management
 */

//=============================================================================
// Constants
//=============================================================================

#define ROUTING_IMMUNE_MAX_ANOMALIES    64      /**< Max tracked anomalies */
#define ROUTING_IMMUNE_WINDOW_MS        1000    /**< Anomaly detection window */
#define ROUTING_IMMUNE_DROP_THRESHOLD   0.05f   /**< 5% drop rate threshold */
#define ROUTING_IMMUNE_LATENCY_THRESHOLD 100.0f /**< 100ms latency threshold */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Routing anomaly types that trigger immune response
 */
typedef enum {
    ROUTING_ANOMALY_NONE = 0,
    ROUTING_ANOMALY_HIGH_DROP_RATE,     /**< Signals being dropped */
    ROUTING_ANOMALY_HIGH_LATENCY,       /**< Excessive routing delays */
    ROUTING_ANOMALY_QUEUE_OVERFLOW,     /**< Queue capacity exceeded */
    ROUTING_ANOMALY_ATTENTION_COLLAPSE, /**< Attention weights failing */
    ROUTING_ANOMALY_ROUTE_FAILURE,      /**< Routing table corruption */
    ROUTING_ANOMALY_SIGNAL_CORRUPTION   /**< Signal data integrity issues */
} routing_anomaly_type_t;

/**
 * @brief Inflammation-based routing strategies
 */
typedef enum {
    ROUTING_STRATEGY_NORMAL = 0,        /**< Normal routing */
    ROUTING_STRATEGY_ALERT,             /**< Increased vigilance */
    ROUTING_STRATEGY_DEFENSIVE,         /**< Conservative routing */
    ROUTING_STRATEGY_EMERGENCY          /**< Maximum priority to threats */
} routing_immune_strategy_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Routing anomaly for immune presentation
 */
typedef struct {
    uint32_t id;
    routing_anomaly_type_t type;
    uint64_t detection_time_ms;
    uint32_t affected_source_id;
    uint32_t affected_dest_id;
    float severity;                     /**< Anomaly severity [0-1] */

    /* Metrics */
    float drop_rate;
    float avg_latency_ms;
    uint32_t queue_depth;
    float attention_weight;

    /* Immune processing */
    bool presented_to_immune;
    uint32_t antigen_id;                /**< Associated antigen if presented */
} routing_anomaly_t;

/**
 * @brief Routing immune bridge configuration
 */
typedef struct {
    /* Anomaly detection thresholds */
    float drop_rate_threshold;          /**< Drop rate to trigger alert */
    float latency_threshold_ms;         /**< Latency to trigger alert */
    uint32_t queue_overflow_threshold;  /**< Queue depth to trigger alert */
    float attention_collapse_threshold; /**< Min attention before alert */

    /* Immune modulation parameters */
    float local_inflammation_boost;     /**< Priority boost for LOCAL (0.1 = 10%) */
    float regional_inflammation_boost;  /**< Priority boost for REGIONAL */
    float systemic_inflammation_boost;  /**< Priority boost for SYSTEMIC */
    float storm_inflammation_boost;     /**< Priority boost for STORM */

    float pro_cytokine_attention_boost; /**< Attention boost from IL-1/IL-6/TNF */
    float anti_cytokine_attention_calm; /**< Attention reduction from IL-10 */

    /* Event integration */
    bool enable_immune_events;          /**< Publish immune events to event bus */
    bool enable_anomaly_detection;      /**< Monitor routing for anomalies */

    /* Update timing */
    uint64_t update_interval_ms;        /**< How often to check routing metrics */
} routing_immune_config_t;

/**
 * @brief Routing immune statistics
 */
typedef struct {
    uint64_t anomalies_detected;
    uint64_t anomalies_presented;
    uint64_t immune_modulations_applied;
    uint64_t cytokine_effects_applied;

    float current_inflammation_boost;
    float current_cytokine_attention_mod;
    routing_immune_strategy_t current_strategy;
} routing_immune_stats_t;

/**
 * @brief Opaque routing immune bridge handle
 */
typedef struct routing_immune_bridge routing_immune_bridge_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with balanced thresholds
 * HOW:  Return struct with default values
 *
 * @return Default configuration
 */
routing_immune_config_t routing_immune_default_config(void);

/**
 * @brief Create routing immune bridge
 *
 * WHAT: Initialize integration between routing and immune systems
 * WHY:  Enable bidirectional communication and modulation
 * HOW:  Allocate tracking structures, register callbacks
 *
 * @param immune_system Brain immune system handle
 * @param thalamic_router Thalamic router handle
 * @param attention_gate Attention gate handle (optional, can be NULL)
 * @param event_bus Event bus handle (optional, can be NULL)
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
routing_immune_bridge_t* routing_immune_create(
    brain_immune_system_t* immune_system,
    thalamic_router_t* thalamic_router,
    attention_gate_t* attention_gate,
    event_bus_t event_bus,
    const routing_immune_config_t* config
);

/**
 * @brief Destroy routing immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Unregister callbacks, free memory
 *
 * @param bridge Bridge handle
 */
void routing_immune_destroy(routing_immune_bridge_t* bridge);

//=============================================================================
// Immune → Routing: Inflammation Effects
//=============================================================================

/**
 * @brief Apply inflammation effects to routing priorities
 *
 * WHAT: Modulate routing based on inflammation level
 * WHY:  Inflammatory states should increase threat signal priority
 * HOW:  Boost priority for signals based on inflammation severity
 *
 * BIOLOGICAL BASIS:
 * Inflammation increases neural excitability and attention to threats.
 * Pro-inflammatory cytokines enhance vigilance and defensive responses.
 *
 * @param bridge Bridge handle
 * @param inflammation_level Current inflammation level
 * @return true on success, false on error
 */
bool routing_immune_apply_inflammation_effect(
    routing_immune_bridge_t* bridge,
    brain_inflammation_level_t inflammation_level
);

/**
 * @brief Apply cytokine effects to attention gating
 *
 * WHAT: Modulate attention weights based on cytokine state
 * WHY:  Cytokines affect neural attention allocation
 * HOW:  Boost/reduce attention based on pro/anti-inflammatory cytokines
 *
 * BIOLOGICAL BASIS:
 * IL-1, IL-6, TNF-α increase attention to threats (hypervigilance).
 * IL-10 reduces inflammation and calms attention (resolution).
 *
 * @param bridge Bridge handle
 * @param cytokine_type Cytokine type
 * @param concentration Cytokine concentration [0-1]
 * @return true on success, false on error
 */
bool routing_immune_apply_cytokine_effect(
    routing_immune_bridge_t* bridge,
    brain_cytokine_type_t cytokine_type,
    float concentration
);

/**
 * @brief Set routing strategy based on immune state
 *
 * WHAT: Change overall routing behavior based on immune phase
 * WHY:  Immune state should influence routing decisions
 * HOW:  Select strategy: normal, alert, defensive, emergency
 *
 * @param bridge Bridge handle
 * @param immune_phase Current immune phase
 * @return true on success, false on error
 */
bool routing_immune_set_strategy_from_phase(
    routing_immune_bridge_t* bridge,
    brain_immune_phase_t immune_phase
);

//=============================================================================
// Routing → Immune: Anomaly Detection
//=============================================================================

/**
 * @brief Detect routing anomalies from statistics
 *
 * WHAT: Analyze routing metrics for abnormal patterns
 * WHY:  Routing failures may indicate threats
 * HOW:  Check drop rate, latency, queue depth against thresholds
 *
 * @param bridge Bridge handle
 * @param stats Current routing statistics
 * @param anomaly_detected Output: true if anomaly found
 * @param anomaly_type Output: type of anomaly (if detected)
 * @return true on success, false on error
 */
bool routing_immune_detect_anomaly(
    routing_immune_bridge_t* bridge,
    const routing_stats_t* stats,
    bool* anomaly_detected,
    routing_anomaly_type_t* anomaly_type
);

/**
 * @brief Present routing anomaly as antigen to immune system
 *
 * WHAT: Convert routing anomaly to immune antigen
 * WHY:  Routing dysfunction should trigger immune response
 * HOW:  Create antigen from anomaly signature, present to immune
 *
 * BIOLOGICAL BASIS:
 * Damaged neurons release danger signals (DAMPs) that trigger microglia.
 * Routing failures are analogous to neural dysfunction.
 *
 * @param bridge Bridge handle
 * @param anomaly Routing anomaly to present
 * @param antigen_id Output: assigned antigen ID
 * @return true on success, false on error
 */
bool routing_immune_present_anomaly(
    routing_immune_bridge_t* bridge,
    const routing_anomaly_t* anomaly,
    uint32_t* antigen_id
);

/**
 * @brief Record routing anomaly for tracking
 *
 * WHAT: Store anomaly in bridge's tracking list
 * WHY:  Track patterns and prevent duplicate presentations
 * HOW:  Add to anomaly array, replace oldest if full
 *
 * @param bridge Bridge handle
 * @param anomaly Anomaly to record
 * @return true on success, false on error
 */
bool routing_immune_record_anomaly(
    routing_immune_bridge_t* bridge,
    const routing_anomaly_t* anomaly
);

//=============================================================================
// Event Integration
//=============================================================================

/**
 * @brief Publish immune event to event bus
 *
 * WHAT: Send immune system event through event bus
 * WHY:  Allow other modules to respond to immune activity
 * HOW:  Convert immune event to middleware event, publish
 *
 * @param bridge Bridge handle
 * @param event_type Immune event type (from event_types.h)
 * @param data Event-specific data
 * @return true on success, false on error
 */
bool routing_immune_publish_event(
    routing_immune_bridge_t* bridge,
    event_type_t event_type,
    const void* data
);

/**
 * @brief Subscribe to routing events that may trigger immune response
 *
 * WHAT: Register callback for routing-related events
 * WHY:  Respond to routing failures and attention shifts
 * HOW:  Subscribe to event bus with routing event filter
 *
 * @param bridge Bridge handle
 * @return true on success, false on error
 */
bool routing_immune_subscribe_routing_events(
    routing_immune_bridge_t* bridge
);

//=============================================================================
// Update and Query API
//=============================================================================

/**
 * @brief Update routing immune bridge state
 *
 * WHAT: Periodic update for anomaly detection and immune effects
 * WHY:  Continuously monitor routing and apply immune modulation
 * HOW:  Check routing stats, detect anomalies, apply effects
 *
 * @param bridge Bridge handle
 * @param delta_ms Time since last update (milliseconds)
 * @return true on success, false on error
 */
bool routing_immune_update(
    routing_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/**
 * @brief Get routing immune statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return true on success, false on error
 */
bool routing_immune_get_stats(
    const routing_immune_bridge_t* bridge,
    routing_immune_stats_t* stats
);

/**
 * @brief Get current inflammation boost factor
 *
 * WHAT: Query current routing priority boost from inflammation
 * WHY:  Monitoring and diagnostics
 * HOW:  Return cached boost value
 *
 * @param bridge Bridge handle
 * @return Boost factor (1.0 = no boost, 1.5 = 50% boost)
 */
float routing_immune_get_inflammation_boost(
    const routing_immune_bridge_t* bridge
);

/**
 * @brief Get current cytokine attention modifier
 *
 * WHAT: Query current attention modulation from cytokines
 * WHY:  Monitoring and diagnostics
 * HOW:  Return cached attention modifier
 *
 * @param bridge Bridge handle
 * @return Attention modifier (1.0 = normal, >1.0 = boosted, <1.0 = calmed)
 */
float routing_immune_get_cytokine_modifier(
    const routing_immune_bridge_t* bridge
);

/**
 * @brief Get current routing strategy
 *
 * @param bridge Bridge handle
 * @return Current routing strategy
 */
routing_immune_strategy_t routing_immune_get_strategy(
    const routing_immune_bridge_t* bridge
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get routing anomaly type name
 *
 * @param type Anomaly type
 * @return Human-readable name
 */
const char* routing_anomaly_type_name(routing_anomaly_type_t type);

/**
 * @brief Get routing strategy name
 *
 * @param strategy Routing strategy
 * @return Human-readable name
 */
const char* routing_immune_strategy_name(routing_immune_strategy_t strategy);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_ROUTING_IMMUNE_H
