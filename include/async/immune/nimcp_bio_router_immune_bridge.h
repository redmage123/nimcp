/**
 * @file nimcp_bio_router_immune_bridge.h
 * @brief Bio-Async Router - Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional integration between bio-async router and brain immune system
 * WHY:  Biological evidence shows immune signals use neural pathways, inflammation affects
 *       signal routing, and immune messaging has priority during threats
 * HOW:  Cytokines get priority routing, routing anomalies trigger immune response,
 *       message latency affected by inflammation, quarantined nodes excluded from routes
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE SIGNALS USE NEURAL PATHWAYS:
 * -----------------------------------
 * 1. Cytokines as Biological Messages:
 *    - Pro-inflammatory cytokines (IL-1β, IL-6, TNF-α) travel along nerve pathways
 *    - Activate vagus nerve for brain-body communication
 *    - Use same neurotransmitter systems as neural signals
 *    - Reference: Tracey (2009) "Reflex control of immunity"
 *
 * 2. Neural-Immune Communication Speed:
 *    - Fast neural route: milliseconds via vagus nerve
 *    - Slow humoral route: minutes to hours via bloodstream
 *    - Cholinergic anti-inflammatory pathway: ACh suppresses cytokine release
 *    - Reference: Pavlov & Tracey (2012) "Neural circuitry and immunity"
 *
 * 3. Priority Signaling in Threats:
 *    - Immune signals override normal neural traffic during infection
 *    - Norepinephrine release prioritizes immune coordination
 *    - Sickness behavior redirects attention/energy to immune response
 *    - Reference: Dantzer & Kelley (2007) "Cytokines and sickness behavior"
 *
 * INFLAMMATION AFFECTS ROUTING EFFICIENCY:
 * ----------------------------------------
 * 1. Inflammation as Congestion:
 *    - Increased cytokine traffic saturates communication channels
 *    - Neuroinflammation slows neural signal propagation
 *    - Edema (swelling) increases signal latency
 *    - Reference: DiSabato et al. (2016) "Neuroinflammation: the devil is in the details"
 *
 * 2. Routing Table Updates:
 *    - Quarantined nodes (infected/Byzantine) removed from routing tables
 *    - Inflamed regions avoided by non-immune traffic
 *    - Immune signals bypass normal routing constraints
 *    - Reference: Neural routing analogous to IP routing with link costs
 *
 * ROUTER → IMMUNE PATHWAYS:
 * -------------------------
 * 1. Routing Anomalies as Threats:
 *    - Message routing failures → potential network attack
 *    - Latency spikes → congestion or malicious interference
 *    - Packet loss → node compromise or link failure
 *    - Trigger immune investigation and response
 *
 * 2. Byzantine Behavior Detection:
 *    - Inconsistent message forwarding
 *    - Message tampering or corruption
 *    - Route hijacking or redirection
 *    - Present as antigens to immune system
 *
 * IMMUNE → ROUTER PATHWAYS:
 * -------------------------
 * 1. Cytokine Priority Routing:
 *    - Immune messages use NOREPINEPHRINE channel (high priority)
 *    - Bypass normal queueing during inflammation
 *    - Guaranteed delivery during threat response
 *
 * 2. Quarantine Enforcement:
 *    - Killer T cell quarantines → router excludes node from all routes
 *    - Inflammation sites → increased link cost, prefer alternative routes
 *    - Recovery → gradual route restoration based on trust
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                  BIO-ROUTER IMMUNE BRIDGE                                  ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  IMMUNE → ROUTER PATHWAYS                           │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β, IL-6  │  ───────┐                                       │  ║
 * ║   │   │ TNF-α        │         │                                       │  ║
 * ║   │   │ IFN-γ        │         ├──→ Priority Queue                     │  ║
 * ║   │   │              │         │    (NOREPINEPHRINE channel)          │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │     BIO-ASYNC ROUTER            │                             │  ║
 * ║   │   │  - Cytokine messages prioritized│                             │  ║
 * ║   │   │  - Quarantined nodes excluded   │                             │  ║
 * ║   │   │  - Inflammation increases cost  │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                            ▲                                       │  ║
 * ║   │   ┌──────────────┐         │                                       │  ║
 * ║   │   │ QUARANTINE   │         │                                       │  ║
 * ║   │   │ Exclude from │  ───────┘                                       │  ║
 * ║   │   │ routing      │     Node isolation                              │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  ROUTER → IMMUNE PATHWAYS                           │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  ANOMALIES   │ ──→ Routing failures                           │  ║
 * ║   │   │  Latency     │ ──→ Latency spikes                             │  ║
 * ║   │   │  Packet loss │ ──→ Message drops                              │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ BYZANTINE    │ ──→ Antigen presentation                       │  ║
 * ║   │   │ Message      │ ──→ Immune investigation                       │  ║
 * ║   │   │ corruption   │                                                 │  ║
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

#ifndef NIMCP_BIO_ROUTER_IMMUNE_BRIDGE_H
#define NIMCP_BIO_ROUTER_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "async/nimcp_bio_router.h"
#include "cognitive/immune/nimcp_brain_immune.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Routing priority levels for immune messages */
#define ROUTER_IMMUNE_PRIORITY_NORMAL        0    /**< Normal routing priority */
#define ROUTER_IMMUNE_PRIORITY_ELEVATED      5    /**< Elevated (local inflammation) */
#define ROUTER_IMMUNE_PRIORITY_HIGH          8    /**< High (regional inflammation) */
#define ROUTER_IMMUNE_PRIORITY_CRITICAL      10   /**< Critical (systemic inflammation) */

/* Latency impact from inflammation (multipliers) */
#define INFLAMMATION_LATENCY_NONE            1.0f  /**< No inflammation */
#define INFLAMMATION_LATENCY_LOCAL           1.2f  /**< Local inflammation +20% latency */
#define INFLAMMATION_LATENCY_REGIONAL        1.5f  /**< Regional inflammation +50% latency */
#define INFLAMMATION_LATENCY_SYSTEMIC        2.0f  /**< Systemic inflammation +100% latency */
#define INFLAMMATION_LATENCY_STORM           3.0f  /**< Cytokine storm +200% latency */

/* Routing anomaly detection thresholds */
#define ROUTER_ANOMALY_LATENCY_THRESHOLD     100.0f  /**< Latency spike threshold (ms) */
#define ROUTER_ANOMALY_DROP_RATE_THRESHOLD   0.10f   /**< Packet drop rate threshold (10%) */
#define ROUTER_ANOMALY_ERROR_RATE_THRESHOLD  0.05f   /**< Error rate threshold (5%) */

/* Quarantine routing behavior */
#define ROUTER_QUARANTINE_FULL_ISOLATION     true   /**< Fully exclude quarantined nodes */
#define ROUTER_QUARANTINE_LINK_COST_PENALTY  1000   /**< High cost to discourage routing */

/* Cytokine message expiry (milliseconds) */
#define CYTOKINE_MESSAGE_TTL_MS              5000   /**< Cytokine messages expire after 5s */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Routing statistics tracked for immune anomaly detection
 */
typedef struct {
    uint64_t messages_sent;           /**< Total messages sent */
    uint64_t messages_delivered;      /**< Successfully delivered */
    uint64_t messages_dropped;        /**< Dropped messages */
    uint64_t routing_errors;          /**< Routing failures */
    float avg_latency_ms;             /**< Average latency */
    float max_latency_ms;             /**< Peak latency */
    float current_drop_rate;          /**< Current drop rate [0-1] */
    float current_error_rate;         /**< Current error rate [0-1] */
    uint64_t last_update_time;        /**< Last stats update */
} router_immune_stats_t;

/**
 * @brief Routing anomaly event
 */
typedef struct {
    uint32_t node_id;                 /**< Node where anomaly detected */
    uint64_t timestamp;               /**< When detected */

    /* Anomaly characteristics */
    bool latency_spike;               /**< Latency exceeded threshold */
    bool high_drop_rate;              /**< Drop rate exceeded threshold */
    bool high_error_rate;             /**< Error rate exceeded threshold */
    bool byzantine_behavior;          /**< Byzantine routing detected */

    /* Measurements */
    float observed_latency_ms;        /**< Observed latency */
    float observed_drop_rate;         /**< Observed drop rate */
    float observed_error_rate;        /**< Observed error rate */

    /* Severity assessment */
    uint32_t severity;                /**< 1-10 severity */
    float confidence;                 /**< Detection confidence [0-1] */
} router_anomaly_event_t;

/**
 * @brief Cytokine routing priority state
 */
typedef struct {
    brain_cytokine_type_t type;       /**< Cytokine type */
    uint32_t priority_level;          /**< Routing priority (0-10) */
    float concentration;              /**< Signal strength [0-1] */
    uint64_t release_time;            /**< When released */
    bool expired;                     /**< TTL expired */
} cytokine_routing_state_t;

/**
 * @brief Inflammation routing impact state
 */
typedef struct {
    brain_inflammation_level_t level; /**< Inflammation level */
    uint32_t affected_region;         /**< Affected region ID */
    float latency_multiplier;         /**< Latency impact multiplier */
    float routing_cost_penalty;       /**< Routing cost increase */
    uint64_t start_time;              /**< When inflammation started */
    bool active;                      /**< Currently inflamed */
} inflammation_routing_impact_t;

/**
 * @brief Quarantined node routing state
 */
typedef struct {
    uint32_t node_id;                 /**< Quarantined node ID */
    uint64_t quarantine_start;        /**< When quarantined */
    uint64_t quarantine_duration_ms;  /**< Quarantine duration */
    bool fully_isolated;              /**< Complete isolation vs high cost */
    float trust_score;                /**< Node trust score [0-1] */
    uint32_t triggering_antigen_id;   /**< Antigen that triggered quarantine */
} quarantined_node_state_t;

/**
 * @brief Complete bio-router immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    bio_router_t router;              /**< Bio-async router */
    bio_module_context_t module_ctx;  /**< Module context for this bridge */
    brain_immune_system_t* immune_system; /**< Brain immune system */

    /* Routing state */
    router_immune_stats_t stats;      /**< Routing statistics */

    /* Cytokine routing */
    cytokine_routing_state_t* cytokine_states;
    size_t cytokine_count;
    size_t cytokine_capacity;

    /* Inflammation impacts */
    inflammation_routing_impact_t* inflammation_impacts;
    size_t inflammation_count;
    size_t inflammation_capacity;

    /* Quarantined nodes */
    quarantined_node_state_t* quarantined_nodes;
    size_t quarantine_count;
    size_t quarantine_capacity;

    /* Anomaly tracking */
    router_anomaly_event_t* recent_anomalies;
    size_t anomaly_count;
    size_t anomaly_capacity;

    /* Integration flags */
    bool enable_cytokine_priority_routing;
    bool enable_inflammation_latency_impact;
    bool enable_quarantine_routing_exclusion;
    bool enable_anomaly_immune_trigger;
    bool enable_byzantine_detection;

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_messages_routed;
    uint32_t anomalies_detected;
    uint32_t immune_triggers;
    uint32_t nodes_quarantined;

    } router_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_priority_routing;
    bool enable_inflammation_latency_impact;
    bool enable_quarantine_routing_exclusion;
    bool enable_anomaly_immune_trigger;
    bool enable_byzantine_detection;

    /* Capacity */
    size_t max_cytokine_states;       /**< Max tracked cytokine messages */
    size_t max_inflammation_sites;    /**< Max inflammation impacts */
    size_t max_quarantined_nodes;     /**< Max quarantined nodes */
    size_t max_anomaly_history;       /**< Max anomaly events to track */

    /* Thresholds */
    float latency_spike_threshold_ms; /**< Latency anomaly threshold */
    float drop_rate_threshold;        /**< Drop rate anomaly threshold */
    float error_rate_threshold;       /**< Error rate anomaly threshold */

    /* Routing behavior */
    bool fully_isolate_quarantined;   /**< Full isolation vs high cost */
    uint32_t cytokine_ttl_ms;         /**< Cytokine message TTL */
} router_immune_config_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biological defaults
 * HOW:  Return struct with evidence-based parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int router_immune_default_config(router_immune_config_t* config);

/**
 * @brief Create router-immune bridge
 *
 * WHAT: Initialize bidirectional router-immune integration
 * WHY:  Enable realistic immune-routing coupling
 * HOW:  Allocate structure, link router and immune system, register handlers
 *
 * @param config Configuration (NULL for defaults)
 * @param router Bio-async router
 * @param immune_system Brain immune system
 * @return New bridge or NULL on failure
 */
router_immune_bridge_t* router_immune_bridge_create(
    const router_immune_config_t* config,
    bio_router_t router,
    brain_immune_system_t* immune_system
);

/**
 * @brief Destroy router-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void router_immune_bridge_destroy(router_immune_bridge_t* bridge);

/**
 * @brief Start router-immune integration
 *
 * WHAT: Activate bidirectional integration
 * WHY:  Begin routing modulation and anomaly detection
 * HOW:  Register message handlers, start monitoring
 *
 * @param bridge Router-immune bridge
 * @return 0 on success, -1 on error
 */
int router_immune_bridge_start(router_immune_bridge_t* bridge);

/**
 * @brief Stop router-immune integration
 *
 * WHAT: Deactivate integration
 * WHY:  Graceful shutdown
 * HOW:  Unregister handlers, stop monitoring
 *
 * @param bridge Router-immune bridge
 * @return 0 on success, -1 on error
 */
int router_immune_bridge_stop(router_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Router API
 * ============================================================================ */

/**
 * @brief Apply cytokine priority routing
 *
 * WHAT: Prioritize cytokine messages in routing queue
 * WHY:  Immune signals need priority during threats (biological reality)
 * HOW:  Map cytokine type to priority level, use NOREPINEPHRINE channel
 *
 * @param bridge Router-immune bridge
 * @param cytokine_type Cytokine type
 * @param concentration Signal strength [0-1]
 * @param source_cell Source cell ID
 * @return 0 on success, -1 on error
 */
int router_immune_prioritize_cytokine(
    router_immune_bridge_t* bridge,
    brain_cytokine_type_t cytokine_type,
    float concentration,
    uint32_t source_cell
);

/**
 * @brief Apply inflammation latency impact
 *
 * WHAT: Increase routing latency based on inflammation level
 * WHY:  Inflammation causes congestion/edema → slower routing
 * HOW:  Map inflammation level to latency multiplier, apply to routing
 *
 * @param bridge Router-immune bridge
 * @param region_id Affected region
 * @param inflammation_level Inflammation severity
 * @return 0 on success, -1 on error
 */
int router_immune_apply_inflammation_latency(
    router_immune_bridge_t* bridge,
    uint32_t region_id,
    brain_inflammation_level_t inflammation_level
);

/**
 * @brief Exclude quarantined node from routing
 *
 * WHAT: Remove quarantined node from all routing tables
 * WHY:  Infected/Byzantine nodes should not forward messages
 * HOW:  Mark node as excluded or apply extreme routing cost penalty
 *
 * @param bridge Router-immune bridge
 * @param node_id Node to quarantine
 * @param duration_ms Quarantine duration
 * @param trust_score Node trust score [0-1]
 * @param antigen_id Triggering antigen
 * @return 0 on success, -1 on error
 */
int router_immune_quarantine_node(
    router_immune_bridge_t* bridge,
    uint32_t node_id,
    uint64_t duration_ms,
    float trust_score,
    uint32_t antigen_id
);

/**
 * @brief Restore quarantined node to routing
 *
 * WHAT: Re-enable routing through recovered node
 * WHY:  Trust restored, node cleared of threat
 * HOW:  Remove from quarantine list, restore normal routing
 *
 * @param bridge Router-immune bridge
 * @param node_id Node to restore
 * @return 0 on success, -1 on error
 */
int router_immune_restore_node(
    router_immune_bridge_t* bridge,
    uint32_t node_id
);

/**
 * @brief Broadcast immune alert via router
 *
 * WHAT: Send immune alert to all modules via bio-async
 * WHY:  System-wide coordination of immune response
 * HOW:  Use BIO_MSG_SWARM_IMMUNE_ALERT on NOREPINEPHRINE channel
 *
 * @param bridge Router-immune bridge
 * @param antigen_id Alert about this antigen
 * @param severity Alert severity
 * @return 0 on success, -1 on error
 */
int router_immune_broadcast_alert(
    router_immune_bridge_t* bridge,
    uint32_t antigen_id,
    brain_inflammation_level_t severity
);

/* ============================================================================
 * Router → Immune API
 * ============================================================================ */

/**
 * @brief Detect routing anomalies and trigger immune response
 *
 * WHAT: Monitor routing statistics for anomalies, present as antigens
 * WHY:  Routing failures may indicate network attack or node compromise
 * HOW:  Check latency, drop rate, error rate against thresholds
 *
 * @param bridge Router-immune bridge
 * @param node_id Node to check
 * @return 0 on success, -1 on error
 */
int router_immune_detect_anomalies(
    router_immune_bridge_t* bridge,
    uint32_t node_id
);

/**
 * @brief Trigger immune response from routing anomaly
 *
 * WHAT: Present routing anomaly as antigen to immune system
 * WHY:  Persistent routing failures are security threats
 * HOW:  Create antigen from anomaly characteristics, present to immune
 *
 * @param bridge Router-immune bridge
 * @param anomaly Anomaly event
 * @return 0 on success, -1 on error
 */
int router_immune_trigger_from_anomaly(
    router_immune_bridge_t* bridge,
    const router_anomaly_event_t* anomaly
);

/**
 * @brief Detect Byzantine routing behavior
 *
 * WHAT: Identify inconsistent/malicious message forwarding
 * WHY:  Byzantine nodes corrupt or drop messages selectively
 * HOW:  Compare message checksums, validate routing paths
 *
 * @param bridge Router-immune bridge
 * @param node_id Suspected Byzantine node
 * @param msg Message that was corrupted/misrouted
 * @param msg_size Message size
 * @return 0 on success, -1 on error
 */
int router_immune_detect_byzantine(
    router_immune_bridge_t* bridge,
    uint32_t node_id,
    const void* msg,
    size_t msg_size
);

/**
 * @brief Present Byzantine behavior as antigen
 *
 * WHAT: Convert Byzantine routing to immune antigen
 * WHY:  Byzantine behavior is a threat requiring immune response
 * HOW:  Extract behavior signature, present to immune system
 *
 * @param bridge Router-immune bridge
 * @param node_id Byzantine node
 * @param behavior_signature Signature of malicious behavior
 * @param sig_len Signature length
 * @return 0 on success, -1 on error
 */
int router_immune_present_byzantine(
    router_immune_bridge_t* bridge,
    uint32_t node_id,
    const uint8_t* behavior_signature,
    size_t sig_len
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update router-immune bridge (both directions)
 *
 * WHAT: Process all router-immune interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine routing, detect anomalies, enforce quarantines
 *
 * @param bridge Router-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success, -1 on error
 */
int router_immune_bridge_update(
    router_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/**
 * @brief Update routing statistics
 *
 * WHAT: Refresh routing stats for anomaly detection
 * WHY:  Need current metrics to detect anomalies
 * HOW:  Query router stats, update internal state
 *
 * @param bridge Router-immune bridge
 * @return 0 on success, -1 on error
 */
int router_immune_update_stats(router_immune_bridge_t* bridge);

/**
 * @brief Expire old cytokine routing states
 *
 * WHAT: Remove expired cytokine priority states
 * WHY:  Cytokine signals have finite lifetime (TTL)
 * HOW:  Check timestamps, remove expired entries
 *
 * @param bridge Router-immune bridge
 * @param current_time Current timestamp
 * @return 0 on success, -1 on error
 */
int router_immune_expire_cytokines(
    router_immune_bridge_t* bridge,
    uint64_t current_time
);

/**
 * @brief Release expired quarantines
 *
 * WHAT: Restore nodes whose quarantine has expired
 * WHY:  Time-limited quarantines need automatic release
 * HOW:  Check quarantine durations, restore expired nodes
 *
 * @param bridge Router-immune bridge
 * @param current_time Current timestamp
 * @return 0 on success, -1 on error
 */
int router_immune_release_expired_quarantines(
    router_immune_bridge_t* bridge,
    uint64_t current_time
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get routing statistics
 *
 * @param bridge Router-immune bridge
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int router_immune_get_stats(
    const router_immune_bridge_t* bridge,
    router_immune_stats_t* stats
);

/**
 * @brief Check if node is quarantined
 *
 * @param bridge Router-immune bridge
 * @param node_id Node to check
 * @return true if quarantined, false otherwise
 */
bool router_immune_is_node_quarantined(
    const router_immune_bridge_t* bridge,
    uint32_t node_id
);

/**
 * @brief Get latency multiplier for region
 *
 * WHAT: Calculate effective latency multiplier from inflammation
 * WHY:  Needed for accurate routing cost calculations
 * HOW:  Query inflammation state for region, return multiplier
 *
 * @param bridge Router-immune bridge
 * @param region_id Region ID
 * @return Latency multiplier (1.0 = no impact, >1.0 = increased latency)
 */
float router_immune_get_latency_multiplier(
    const router_immune_bridge_t* bridge,
    uint32_t region_id
);

/**
 * @brief Get cytokine routing priority
 *
 * @param bridge Router-immune bridge
 * @param cytokine_type Cytokine type
 * @return Priority level (0-10)
 */
uint32_t router_immune_get_cytokine_priority(
    const router_immune_bridge_t* bridge,
    brain_cytokine_type_t cytokine_type
);

/**
 * @brief Get recent anomaly count
 *
 * @param bridge Router-immune bridge
 * @param time_window_ms Time window to count (0 = all)
 * @return Number of anomalies detected in window
 */
uint32_t router_immune_get_anomaly_count(
    const router_immune_bridge_t* bridge,
    uint64_t time_window_ms
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BIO_ROUTER_IMMUNE_BRIDGE_H */
