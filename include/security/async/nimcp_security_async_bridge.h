/**
 * @file nimcp_security_async_bridge.h
 * @brief Security Module - Bio-Async Router Integration Bridge
 * @version 1.0.0
 * @date 2025-01-09
 *
 * WHAT: Bidirectional integration between security module and bio-async router
 * WHY:  Security = Immune system's first response; requires real-time coordination
 *       across all system modules for threat detection and response
 * HOW:  Broadcasts security events, receives distributed threat intel, coordinates
 *       policy enforcement across the bio-async network
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE SYSTEM'S FIRST RESPONSE:
 * --------------------------------
 * The security module mirrors the innate immune system's immediate response to threats:
 *
 * 1. Pattern Recognition (BBB, Anomaly Detector, Pattern DB):
 *    - Like Pattern Recognition Receptors (PRRs) on macrophages and dendritic cells
 *    - Detect Pathogen-Associated Molecular Patterns (PAMPs)
 *    - Immediate, non-specific detection of threats
 *    - Reference: Janeway & Medzhitov (2002) "Innate immune recognition"
 *
 * 2. Blood-Brain Barrier (BBB):
 *    - Physical and functional barrier protecting neural tissue
 *    - Controls what enters the "brain" (system core)
 *    - Endothelial tight junctions = input validation
 *    - Reference: Abbott et al. (2010) "Structure and function of the BBB"
 *
 * 3. Rate Limiting (Throttling):
 *    - Like the inflammatory response that limits pathogen spread
 *    - Prevents cascade failures from overwhelming attacks
 *    - Proportional response based on threat severity
 *
 * 4. Security Events as Cytokine Signals:
 *    - Threat alerts = Pro-inflammatory cytokines (IL-1, IL-6, TNF-alpha)
 *    - Policy changes = Regulatory signals (IL-10, TGF-beta)
 *    - Rate limit hits = Chemokine recruitment signals
 *    - Use NOREPINEPHRINE channel for priority delivery
 *
 * SECURITY <-> ASYNC BIDIRECTIONAL FLOW:
 * --------------------------------------
 *
 * SECURITY -> ASYNC:
 * - Threat alerts broadcast to all modules (like cytokine storm warning)
 * - Security events published for logging/monitoring
 * - Policy changes announced system-wide
 * - BBB alerts when critical threats detected
 *
 * ASYNC -> SECURITY:
 * - Threat reports from other modules (distributed detection)
 * - Pattern updates from network peers
 * - Distributed threat intelligence aggregation
 * - Cross-module anomaly correlation
 *
 * ARCHITECTURE:
 * ```
 * +=========================================================================+
 * |                   SECURITY-ASYNC BRIDGE                                  |
 * +=========================================================================+
 * |                                                                          |
 * |   +--------------------------------------------------------------------+ |
 * |   |                SECURITY -> ASYNC PATHWAYS                          | |
 * |   |                                                                    | |
 * |   |   +---------------+    +---------------+    +------------------+   | |
 * |   |   |     BBB       |    |   Anomaly     |    |    Policy        |   | |
 * |   |   |   Threats     |--->|   Detector    |--->|    Engine        |   | |
 * |   |   +---------------+    +---------------+    +------------------+   | |
 * |   |          |                    |                     |              | |
 * |   |          v                    v                     v              | |
 * |   |   +---------------------------------------------------------+     | |
 * |   |   |              BIO-ASYNC ROUTER                           |     | |
 * |   |   |   - BIO_MSG_SECURITY_THREAT_DETECTED                    |     | |
 * |   |   |   - BIO_MSG_SECURITY_ANOMALY_DETECTED                   |     | |
 * |   |   |   - BIO_MSG_SECURITY_POLICY_CHANGE                      |     | |
 * |   |   |   - BIO_MSG_SECURITY_RATE_LIMIT_HIT                     |     | |
 * |   |   |   - BIO_MSG_SECURITY_BBB_ALERT                          |     | |
 * |   |   |   - BIO_MSG_SECURITY_PATTERN_UPDATE                     |     | |
 * |   |   +---------------------------------------------------------+     | |
 * |   |                                                                    | |
 * |   +--------------------------------------------------------------------+ |
 * |                                                                          |
 * |   +--------------------------------------------------------------------+ |
 * |   |                ASYNC -> SECURITY PATHWAYS                          | |
 * |   |                                                                    | |
 * |   |   +---------------------------------------------------------+     | |
 * |   |   |              BIO-ASYNC ROUTER                           |     | |
 * |   |   |   - Threat reports from peer modules                    |     | |
 * |   |   |   - Pattern updates from network                        |     | |
 * |   |   |   - Distributed threat intelligence                     |     | |
 * |   |   +---------------------------------------------------------+     | |
 * |   |          |                    |                     |              | |
 * |   |          v                    v                     v              | |
 * |   |   +---------------+    +---------------+    +------------------+   | |
 * |   |   |   Pattern     |    |    BBB        |    |   Rate           |   | |
 * |   |   |   Database    |<---|   System      |<---|   Limiter        |   | |
 * |   |   +---------------+    +---------------+    +------------------+   | |
 * |   |                                                                    | |
 * |   +--------------------------------------------------------------------+ |
 * |                                                                          |
 * +=========================================================================+
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 * - bridge_base_t as first member
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SECURITY_ASYNC_BRIDGE_H
#define NIMCP_SECURITY_ASYNC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Base bridge infrastructure */
#include "utils/bridge/nimcp_bridge_base.h"

/* Security module integrations */
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_anomaly_detector.h"
#include "security/nimcp_pattern_db.h"
#include "security/nimcp_policy_engine.h"
#include "security/nimcp_rate_limiter.h"

/* Bio-async integrations */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

/* Common utilities */
#include "utils/validation/nimcp_common.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants and Magic Numbers
 * ============================================================================ */

/** @brief Magic number for bridge validation */
#define NIMCP_SECURITY_ASYNC_BRIDGE_MAGIC 0x53454341  /* 'SECA' */

/** @brief Bridge version */
#define NIMCP_SECURITY_ASYNC_BRIDGE_VERSION 0x0100

/* ============================================================================
 * Security Event Message Types
 *
 * WHAT: Message types for security events in bio-async system
 * WHY:  Enable typed message handling for security coordination
 * HOW:  Extend BIO_MSG_SECURITY_* range (0x0750 - 0x076F)
 * ============================================================================ */

/**
 * @brief Extended security message types for async bridge
 *
 * These extend the base security message types defined in nimcp_bio_messages.h
 * Range: 0x0760 - 0x077F (security async bridge specific)
 */
typedef enum {
    /* Threat detection messages (0x0760 - 0x0767) */
    BIO_MSG_SECURITY_THREAT_DETECTED_EXT = 0x0760,  /**< Detailed threat detection */
    BIO_MSG_SECURITY_THREAT_CLEARED,                /**< Threat cleared/resolved */
    BIO_MSG_SECURITY_THREAT_ESCALATED,              /**< Threat severity escalated */
    BIO_MSG_SECURITY_THREAT_QUARANTINED,            /**< Threat isolated/quarantined */

    /* Policy messages (0x0768 - 0x076B) */
    BIO_MSG_SECURITY_POLICY_CHANGE_EXT = 0x0768,    /**< Detailed policy change */
    BIO_MSG_SECURITY_POLICY_VIOLATION,              /**< Policy violation detected */
    BIO_MSG_SECURITY_POLICY_RELOAD,                 /**< Policy database reloaded */

    /* Pattern database messages (0x076C - 0x076F) */
    BIO_MSG_SECURITY_PATTERN_UPDATE_EXT = 0x076C,   /**< Pattern update with details */
    BIO_MSG_SECURITY_PATTERN_MATCH,                 /**< Pattern match notification */
    BIO_MSG_SECURITY_PATTERN_LEARNED,               /**< New pattern learned */

    /* Rate limiting messages (0x0770 - 0x0773) */
    BIO_MSG_SECURITY_RATE_LIMIT_HIT_EXT = 0x0770,   /**< Detailed rate limit event */
    BIO_MSG_SECURITY_RATE_LIMIT_PENALTY,            /**< Penalty applied to client */
    BIO_MSG_SECURITY_RATE_LIMIT_BLOCKED,            /**< Client blocked */
    BIO_MSG_SECURITY_RATE_LIMIT_UNBLOCKED,          /**< Client unblocked */

    /* BBB alert messages (0x0774 - 0x0777) */
    BIO_MSG_SECURITY_BBB_ALERT_EXT = 0x0774,        /**< Detailed BBB alert */
    BIO_MSG_SECURITY_BBB_VALIDATION_FAILED,         /**< Input validation failed */
    BIO_MSG_SECURITY_BBB_SIGNATURE_INVALID,         /**< Code signature invalid */
    BIO_MSG_SECURITY_BBB_MEMORY_VIOLATION,          /**< Memory boundary violation */

    /* Anomaly detection messages (0x0778 - 0x077B) */
    BIO_MSG_SECURITY_ANOMALY_DETECTED_EXT = 0x0778, /**< Detailed anomaly */
    BIO_MSG_SECURITY_ANOMALY_CONTENT,               /**< Content anomaly detected */
    BIO_MSG_SECURITY_ANOMALY_BEHAVIOR,              /**< Behavioral anomaly detected */
    BIO_MSG_SECURITY_ANOMALY_TIMING,                /**< Timing anomaly detected */

    /* Distributed threat intel messages (0x077C - 0x077F) */
    BIO_MSG_SECURITY_THREAT_INTEL_SHARE = 0x077C,   /**< Share threat intelligence */
    BIO_MSG_SECURITY_THREAT_INTEL_REQUEST,          /**< Request threat intel */
    BIO_MSG_SECURITY_THREAT_INTEL_RESPONSE          /**< Threat intel response */
    /* Note: BIO_MSG_SECURITY_CONSENSUS_REQUEST is defined in nimcp_bio_messages.h */
} security_async_message_type_t;

/* ============================================================================
 * Event Bus Integration Types
 * ============================================================================ */

/**
 * @brief Security event severity levels
 */
typedef enum {
    SECURITY_EVENT_SEVERITY_INFO = 0,       /**< Informational event */
    SECURITY_EVENT_SEVERITY_LOW = 1,        /**< Low severity - log only */
    SECURITY_EVENT_SEVERITY_MEDIUM = 2,     /**< Medium - alert and log */
    SECURITY_EVENT_SEVERITY_HIGH = 3,       /**< High - immediate response */
    SECURITY_EVENT_SEVERITY_CRITICAL = 4    /**< Critical - system lockdown */
} security_event_severity_t;

/**
 * @brief Security event categories for event bus
 */
typedef enum {
    SECURITY_EVENT_CATEGORY_THREAT = 0,     /**< Threat detection events */
    SECURITY_EVENT_CATEGORY_POLICY,         /**< Policy-related events */
    SECURITY_EVENT_CATEGORY_PATTERN,        /**< Pattern database events */
    SECURITY_EVENT_CATEGORY_RATE_LIMIT,     /**< Rate limiting events */
    SECURITY_EVENT_CATEGORY_BBB,            /**< BBB validation events */
    SECURITY_EVENT_CATEGORY_ANOMALY,        /**< Anomaly detection events */
    SECURITY_EVENT_CATEGORY_INTEL,          /**< Distributed threat intel */
    SECURITY_EVENT_CATEGORY_SYSTEM          /**< System-level security events */
} security_event_category_t;

/**
 * @brief Security event for event bus publication
 */
typedef struct {
    security_event_category_t category;     /**< Event category */
    security_event_severity_t severity;     /**< Event severity */
    uint64_t timestamp_us;                  /**< Event timestamp (microseconds) */
    uint32_t source_module;                 /**< Source module ID */
    uint32_t event_id;                      /**< Unique event identifier */

    /* Event-specific data */
    union {
        struct {
            bbb_threat_type_t threat_type;
            bbb_action_t action_taken;
            uint8_t threat_hash[32];
        } threat;

        struct {
            nimcp_pattern_category_t category;
            nimcp_pattern_id_t pattern_id;
            float threat_score;
        } pattern;

        struct {
            char client_id[128];
            nimcp_penalty_action_t penalty;
            uint32_t violation_count;
        } rate_limit;

        struct {
            float anomaly_score;
            uint32_t triggered_features;
            float confidence;
        } anomaly;

        struct {
            nimcp_policy_action_t action;
            nimcp_policy_severity_t policy_severity;
            char rule_name[64];
        } policy;
    } data;

    char description[256];                  /**< Human-readable description */
} security_async_event_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief Security-async bridge configuration
 *
 * WHAT: Configuration for security-async integration
 * WHY:  Enable fine-grained control over feature activation
 * HOW:  Boolean flags for each integration feature
 */
typedef struct {
    /* Feature enables */
    bool enable_threat_broadcast;           /**< Broadcast threat alerts */
    bool enable_policy_announcements;       /**< Announce policy changes */
    bool enable_pattern_sync;               /**< Sync patterns via async */
    bool enable_rate_limit_events;          /**< Publish rate limit events */
    bool enable_bbb_alerts;                 /**< Publish BBB alerts */
    bool enable_anomaly_events;             /**< Publish anomaly events */
    bool enable_distributed_intel;          /**< Enable distributed threat intel */
    bool enable_event_bus;                  /**< Enable event bus integration */

    /* Severity thresholds */
    security_event_severity_t broadcast_threshold;  /**< Min severity for broadcast */
    bbb_severity_t bbb_alert_threshold;             /**< Min BBB severity for alerts */
    float anomaly_alert_threshold;                  /**< Min anomaly score for alerts */

    /* Capacity settings */
    size_t max_pending_events;              /**< Max events in queue */
    size_t max_threat_intel_cache;          /**< Max cached threat intel entries */
    size_t max_pattern_updates;             /**< Max pattern updates to buffer */

    /* Timing settings */
    uint32_t event_batch_interval_ms;       /**< Batch events for this duration */
    uint32_t intel_refresh_interval_ms;     /**< Refresh threat intel interval */
    uint32_t pattern_sync_interval_ms;      /**< Pattern sync interval */

    /* Priority settings */
    nimcp_bio_channel_type_t threat_channel;      /**< Channel for threats */
    nimcp_bio_channel_type_t policy_channel;      /**< Channel for policy events */
    uint32_t threat_priority;               /**< Priority for threat messages */
} security_async_config_t;

/* ============================================================================
 * Effect Structures (Bidirectional)
 * ============================================================================ */

/**
 * @brief Security effects on async system
 *
 * WHAT: How security events modulate async behavior
 * WHY:  Threat detection affects system-wide messaging priorities
 * HOW:  Elevated threats increase message priority, quarantines exclude nodes
 */
typedef struct {
    /* Threat state */
    bool active_threat;                     /**< Currently active threat */
    bbb_severity_t current_threat_level;    /**< Current threat severity */
    uint32_t quarantined_nodes;             /**< Number of quarantined nodes */

    /* Routing modulation */
    float priority_boost;                   /**< Message priority boost [0-1] */
    bool bypass_normal_routing;             /**< Bypass normal routing for alerts */
    uint32_t excluded_node_count;           /**< Nodes excluded from routing */

    /* Rate modulation */
    float rate_reduction_factor;            /**< Global rate reduction [0-1] */
    bool emergency_throttle;                /**< Emergency throttle active */

    /* Policy effects */
    bool policy_updated;                    /**< Policy was recently updated */
    uint32_t active_rules;                  /**< Number of active policy rules */
} security_async_effects_t;

/**
 * @brief Async effects on security
 *
 * WHAT: How async events inform security decisions
 * WHY:  Distributed intelligence improves threat detection
 * HOW:  Threat reports from peers, pattern updates, anomaly correlation
 */
typedef struct {
    /* Distributed intelligence */
    uint32_t peer_threat_reports;           /**< Threats reported by peers */
    uint32_t peer_pattern_updates;          /**< Pattern updates from peers */
    float network_threat_level;             /**< Aggregate network threat level */

    /* Anomaly correlation */
    uint32_t correlated_anomalies;          /**< Cross-module anomaly correlations */
    float correlation_confidence;           /**< Correlation confidence [0-1] */

    /* Consensus state */
    bool consensus_active;                  /**< Security consensus in progress */
    float consensus_agreement;              /**< Consensus agreement level [0-1] */

    /* Intel freshness */
    uint64_t last_intel_update_ms;          /**< Last threat intel update */
    uint32_t stale_intel_count;             /**< Number of stale intel entries */
} async_security_effects_t;

/* ============================================================================
 * State and Statistics Structures
 * ============================================================================ */

/**
 * @brief Security-async bridge state
 */
typedef struct {
    /* Connection state */
    bool bbb_connected;                     /**< BBB system connected */
    bool anomaly_connected;                 /**< Anomaly detector connected */
    bool pattern_db_connected;              /**< Pattern DB connected */
    bool policy_engine_connected;           /**< Policy engine connected */
    bool rate_limiter_connected;            /**< Rate limiter connected */
    bool event_bus_connected;               /**< Event bus connected */

    /* Operational state */
    bool is_active;                         /**< Bridge is active */
    bool emergency_mode;                    /**< Emergency security mode */
    uint64_t last_threat_time_ms;           /**< Last threat detection time */
    uint64_t last_policy_change_ms;         /**< Last policy change time */

    /* Queue state */
    uint32_t pending_events;                /**< Events pending broadcast */
    uint32_t pending_intel;                 /**< Intel updates pending */
    uint32_t pending_patterns;              /**< Pattern updates pending */
} security_async_state_t;

/**
 * @brief Security-async bridge statistics
 */
typedef struct {
    /* Event statistics */
    uint64_t events_published;              /**< Total events published */
    uint64_t events_received;               /**< Total events received */
    uint64_t events_dropped;                /**< Events dropped (queue full) */

    /* By category */
    uint64_t threat_events;                 /**< Threat events published */
    uint64_t policy_events;                 /**< Policy events published */
    uint64_t pattern_events;                /**< Pattern events published */
    uint64_t rate_limit_events;             /**< Rate limit events published */
    uint64_t bbb_events;                    /**< BBB events published */
    uint64_t anomaly_events;                /**< Anomaly events published */

    /* Distributed intel statistics */
    uint64_t intel_shared;                  /**< Intel shared with peers */
    uint64_t intel_received;                /**< Intel received from peers */
    uint64_t patterns_synced;               /**< Patterns synced */

    /* Performance metrics */
    float avg_broadcast_latency_us;         /**< Average broadcast latency */
    float max_broadcast_latency_us;         /**< Maximum broadcast latency */
    float avg_event_processing_us;          /**< Average event processing time */

    /* Error statistics */
    uint64_t broadcast_failures;            /**< Failed broadcasts */
    uint64_t handler_errors;                /**< Handler errors */
    uint64_t queue_overflows;               /**< Queue overflow events */
} security_async_stats_t;

/* ============================================================================
 * Threat Intelligence Structures
 * ============================================================================ */

/**
 * @brief Distributed threat intelligence entry
 */
typedef struct {
    uint8_t threat_hash[32];                /**< Threat signature hash */
    bbb_threat_type_t threat_type;          /**< Type of threat */
    bbb_severity_t severity;                /**< Threat severity */
    uint32_t source_node;                   /**< Reporting node */
    uint64_t first_seen_ms;                 /**< First observation time */
    uint64_t last_seen_ms;                  /**< Most recent observation */
    uint32_t observation_count;             /**< Number of observations */
    float confidence;                       /**< Confidence score [0-1] */
    bool confirmed;                         /**< Confirmed by multiple sources */
} threat_intel_entry_t;

/**
 * @brief Threat intelligence cache
 */
typedef struct {
    threat_intel_entry_t* entries;          /**< Intel entries */
    size_t count;                           /**< Current entry count */
    size_t capacity;                        /**< Maximum capacity */
    uint64_t last_refresh_ms;               /**< Last cache refresh */
} threat_intel_cache_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief Security-async bridge main structure
 *
 * WHAT: Main bridge connecting security module to bio-async router
 * WHY:  Centralized coordination for security-async integration
 * HOW:  Contains config, effects, state, stats, and system handles
 */
typedef struct {
    bridge_base_t base;                     /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    security_async_config_t config;         /**< Bridge configuration */

    /* Security system connections */
    bbb_system_t bbb_system;                /**< Blood-brain barrier system */
    nimcp_anomaly_detector_t anomaly_detector; /**< Anomaly detector */
    nimcp_pattern_db_t pattern_db;          /**< Pattern database */
    nimcp_policy_engine_t policy_engine;    /**< Policy engine */
    nimcp_rate_limiter_t rate_limiter;      /**< Rate limiter */

    /* Bio-router connection (stored in base.system_b for consistency) */
    /* Access via base.system_b or dedicated pointer */
    bio_router_t router;                    /**< Bio-async router */

    /* Event bus handle (if enabled) */
    void* event_bus;                        /**< Event bus handle (opaque) */

    /* Bidirectional effects */
    security_async_effects_t security_effects;  /**< Security -> Async effects */
    async_security_effects_t async_effects;     /**< Async -> Security effects */

    /* State and statistics */
    security_async_state_t state;           /**< Current bridge state */
    security_async_stats_t stats;           /**< Bridge statistics */

    /* Threat intelligence */
    threat_intel_cache_t intel_cache;       /**< Cached threat intelligence */

    /* Event queue */
    security_async_event_t* event_queue;    /**< Pending event queue */
    size_t event_queue_head;                /**< Queue head index */
    size_t event_queue_tail;                /**< Queue tail index */
    size_t event_queue_capacity;            /**< Queue capacity */

} security_async_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default security-async bridge configuration
 *
 * WHAT: Provide sensible defaults for security-async integration
 * WHY:  Easy initialization with security-focused defaults
 * HOW:  Return config with all features enabled, conservative thresholds
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 */
int security_async_default_config(security_async_config_t* config);

/**
 * @brief Create security-async bridge
 *
 * WHAT: Initialize bidirectional security-async integration
 * WHY:  Enable real-time security event coordination across system
 * HOW:  Allocate bridge, initialize state, prepare event queue
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
security_async_bridge_t* security_async_bridge_create(
    const security_async_config_t* config
);

/**
 * @brief Destroy security-async bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect systems, free queues, destroy base
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void security_async_bridge_destroy(security_async_bridge_t* bridge);

/* ============================================================================
 * Connection API - Security Systems
 * ============================================================================ */

/**
 * @brief Connect BBB system to bridge
 *
 * WHAT: Connect Blood-Brain Barrier system for threat alerts
 * WHY:  BBB detects perimeter threats requiring immediate broadcast
 * HOW:  Store handle, register for BBB alerts
 *
 * @param bridge Security-async bridge
 * @param bbb_system BBB system to connect
 * @return 0 on success, error code on failure
 */
int security_async_connect_bbb(
    security_async_bridge_t* bridge,
    bbb_system_t bbb_system
);

/**
 * @brief Connect anomaly detector to bridge
 *
 * WHAT: Connect anomaly detector for ML-based threat detection
 * WHY:  Anomaly events require broadcast for distributed correlation
 * HOW:  Store handle, register for anomaly alerts
 *
 * @param bridge Security-async bridge
 * @param detector Anomaly detector to connect
 * @return 0 on success, error code on failure
 */
int security_async_connect_anomaly_detector(
    security_async_bridge_t* bridge,
    nimcp_anomaly_detector_t detector
);

/**
 * @brief Connect pattern database to bridge
 *
 * WHAT: Connect pattern DB for threat pattern coordination
 * WHY:  Pattern updates should sync across distributed system
 * HOW:  Store handle, register for pattern events
 *
 * @param bridge Security-async bridge
 * @param pattern_db Pattern database to connect
 * @return 0 on success, error code on failure
 */
int security_async_connect_pattern_db(
    security_async_bridge_t* bridge,
    nimcp_pattern_db_t pattern_db
);

/**
 * @brief Connect policy engine to bridge
 *
 * WHAT: Connect policy engine for policy change announcements
 * WHY:  Policy changes must propagate to all modules
 * HOW:  Store handle, register for policy events
 *
 * @param bridge Security-async bridge
 * @param policy_engine Policy engine to connect
 * @return 0 on success, error code on failure
 */
int security_async_connect_policy_engine(
    security_async_bridge_t* bridge,
    nimcp_policy_engine_t policy_engine
);

/**
 * @brief Connect rate limiter to bridge
 *
 * WHAT: Connect rate limiter for throttling event coordination
 * WHY:  Rate limit events inform system-wide load management
 * HOW:  Store handle, register for rate limit events
 *
 * @param bridge Security-async bridge
 * @param rate_limiter Rate limiter to connect
 * @return 0 on success, error code on failure
 */
int security_async_connect_rate_limiter(
    security_async_bridge_t* bridge,
    nimcp_rate_limiter_t rate_limiter
);

/* ============================================================================
 * Connection API - Bio-Async Systems
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async router for message handling
 * WHY:  Enable security event broadcast and threat intel reception
 * HOW:  Register module, set up message handlers
 *
 * @param bridge Security-async bridge
 * @return 0 on success, error code on failure
 */
int security_async_connect_bio_async(security_async_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister from bio-async messaging
 * WHY:  Clean shutdown of async integration
 * HOW:  Unregister module context
 *
 * @param bridge Security-async bridge
 * @return 0 on success, error code on failure
 */
int security_async_disconnect_bio_async(security_async_bridge_t* bridge);

/**
 * @brief Check if connected to bio-async router
 *
 * @param bridge Security-async bridge
 * @return true if connected
 */
bool security_async_is_bio_async_connected(const security_async_bridge_t* bridge);

/**
 * @brief Connect to event bus
 *
 * WHAT: Connect to event bus for event publication
 * WHY:  Event bus provides additional event distribution mechanism
 * HOW:  Register as event publisher
 *
 * @param bridge Security-async bridge
 * @param event_bus Event bus handle
 * @return 0 on success, error code on failure
 */
int security_async_connect_event_bus(
    security_async_bridge_t* bridge,
    void* event_bus
);

/* ============================================================================
 * Security -> Async API (Outbound)
 * ============================================================================ */

/**
 * @brief Broadcast threat alert to all modules
 *
 * WHAT: Send threat detection to all registered modules
 * WHY:  System-wide threat awareness enables coordinated response
 * HOW:  Create message, broadcast via NOREPINEPHRINE channel
 *
 * @param bridge Security-async bridge
 * @param threat_type Type of threat detected
 * @param severity Threat severity
 * @param description Human-readable description
 * @param threat_hash Optional threat signature hash (32 bytes)
 * @return 0 on success, error code on failure
 */
int security_async_broadcast_threat(
    security_async_bridge_t* bridge,
    bbb_threat_type_t threat_type,
    bbb_severity_t severity,
    const char* description,
    const uint8_t* threat_hash
);

/**
 * @brief Publish security event
 *
 * WHAT: Publish security event for logging/monitoring
 * WHY:  Security events need audit trail and monitoring
 * HOW:  Queue event for publication on configured channel
 *
 * @param bridge Security-async bridge
 * @param event Security event to publish
 * @return 0 on success, error code on failure
 */
int security_async_publish_event(
    security_async_bridge_t* bridge,
    const security_async_event_t* event
);

/**
 * @brief Announce policy change
 *
 * WHAT: Broadcast policy change to all modules
 * WHY:  Policy changes must propagate system-wide
 * HOW:  Create policy change message, broadcast on SEROTONIN channel
 *
 * @param bridge Security-async bridge
 * @param action Policy action
 * @param rule_name Name of changed rule
 * @param description Change description
 * @return 0 on success, error code on failure
 */
int security_async_announce_policy_change(
    security_async_bridge_t* bridge,
    nimcp_policy_action_t action,
    const char* rule_name,
    const char* description
);

/**
 * @brief Broadcast BBB alert
 *
 * WHAT: Send BBB perimeter alert to all modules
 * WHY:  BBB violations are critical security events
 * HOW:  Create BBB alert message, broadcast on NOREPINEPHRINE channel
 *
 * @param bridge Security-async bridge
 * @param report BBB threat report
 * @return 0 on success, error code on failure
 */
int security_async_broadcast_bbb_alert(
    security_async_bridge_t* bridge,
    const bbb_threat_report_t* report
);

/**
 * @brief Broadcast rate limit event
 *
 * WHAT: Notify system of rate limiting event
 * WHY:  Rate limits affect system-wide load balancing
 * HOW:  Create rate limit message, send on ACETYLCHOLINE channel
 *
 * @param bridge Security-async bridge
 * @param client_id Affected client identifier
 * @param penalty Penalty applied
 * @param violation_count Total violations
 * @return 0 on success, error code on failure
 */
int security_async_broadcast_rate_limit(
    security_async_bridge_t* bridge,
    const char* client_id,
    nimcp_penalty_action_t penalty,
    uint32_t violation_count
);

/**
 * @brief Broadcast pattern update
 *
 * WHAT: Share pattern database update with peers
 * WHY:  Distributed pattern sync improves detection
 * HOW:  Create pattern update message, broadcast
 *
 * @param bridge Security-async bridge
 * @param pattern_id Updated pattern ID
 * @param category Pattern category
 * @param is_new True if new pattern, false if update
 * @return 0 on success, error code on failure
 */
int security_async_broadcast_pattern_update(
    security_async_bridge_t* bridge,
    nimcp_pattern_id_t pattern_id,
    nimcp_pattern_category_t category,
    bool is_new
);

/* ============================================================================
 * Async -> Security API (Inbound)
 * ============================================================================ */

/**
 * @brief Handle received threat report from peer
 *
 * WHAT: Process threat report from another module
 * WHY:  Distributed detection improves coverage
 * HOW:  Validate report, update threat intel cache, correlate
 *
 * @param bridge Security-async bridge
 * @param source_module Reporting module ID
 * @param threat_type Reported threat type
 * @param threat_hash Threat signature hash
 * @param confidence Reporter's confidence [0-1]
 * @return 0 on success, error code on failure
 */
int security_async_receive_threat_report(
    security_async_bridge_t* bridge,
    uint32_t source_module,
    bbb_threat_type_t threat_type,
    const uint8_t* threat_hash,
    float confidence
);

/**
 * @brief Handle received pattern update from peer
 *
 * WHAT: Process pattern update from another module
 * WHY:  Keep pattern database synchronized across system
 * HOW:  Validate update, apply to local pattern DB
 *
 * @param bridge Security-async bridge
 * @param source_module Source module ID
 * @param entry Pattern entry to add/update
 * @return 0 on success, error code on failure
 */
int security_async_receive_pattern_update(
    security_async_bridge_t* bridge,
    uint32_t source_module,
    const nimcp_pattern_entry_t* entry
);

/**
 * @brief Request distributed threat intelligence
 *
 * WHAT: Query peers for threat intelligence
 * WHY:  Aggregate distributed knowledge for better detection
 * HOW:  Broadcast intel request, collect responses
 *
 * @param bridge Security-async bridge
 * @param threat_hash Optional specific threat to query (NULL for general)
 * @return 0 on success, error code on failure
 */
int security_async_request_threat_intel(
    security_async_bridge_t* bridge,
    const uint8_t* threat_hash
);

/**
 * @brief Share threat intelligence with peers
 *
 * WHAT: Proactively share threat intel with network
 * WHY:  Improve collective security through shared knowledge
 * HOW:  Broadcast cached threat intel entries
 *
 * @param bridge Security-async bridge
 * @param max_entries Maximum entries to share (0 = all)
 * @return 0 on success, error code on failure
 */
int security_async_share_threat_intel(
    security_async_bridge_t* bridge,
    uint32_t max_entries
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update security effects on async (outbound)
 *
 * WHAT: Compute how security state modulates async behavior
 * WHY:  Active threats should affect message priorities
 * HOW:  Aggregate security state, compute routing modulation
 *
 * @param bridge Security-async bridge
 * @return 0 on success, error code on failure
 */
int security_async_update_security_effects(security_async_bridge_t* bridge);

/**
 * @brief Update async effects on security (inbound)
 *
 * WHAT: Process distributed intel and correlate anomalies
 * WHY:  Network-wide perspective improves threat detection
 * HOW:  Aggregate peer reports, compute correlation metrics
 *
 * @param bridge Security-async bridge
 * @return 0 on success, error code on failure
 */
int security_async_update_async_effects(security_async_bridge_t* bridge);

/**
 * @brief Full update cycle (both directions)
 *
 * WHAT: Execute complete bidirectional update
 * WHY:  Single call for regular update loops
 * HOW:  Update both directions, process pending events
 *
 * @param bridge Security-async bridge
 * @param delta_ms Time since last update
 * @return 0 on success, error code on failure
 */
int security_async_bridge_update(
    security_async_bridge_t* bridge,
    uint64_t delta_ms
);

/**
 * @brief Process pending events
 *
 * WHAT: Send queued events and process inbox
 * WHY:  Regular event processing for async communication
 * HOW:  Flush event queue, process incoming messages
 *
 * @param bridge Security-async bridge
 * @param max_events Maximum events to process (0 = all)
 * @return Number of events processed
 */
uint32_t security_async_process_events(
    security_async_bridge_t* bridge,
    uint32_t max_events
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get security effects on async
 *
 * @param bridge Security-async bridge
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int security_async_get_security_effects(
    const security_async_bridge_t* bridge,
    security_async_effects_t* effects
);

/**
 * @brief Get async effects on security
 *
 * @param bridge Security-async bridge
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int security_async_get_async_effects(
    const security_async_bridge_t* bridge,
    async_security_effects_t* effects
);

/**
 * @brief Get bridge state
 *
 * @param bridge Security-async bridge
 * @param state Output state structure
 * @return 0 on success, error code on failure
 */
int security_async_get_state(
    const security_async_bridge_t* bridge,
    security_async_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Security-async bridge
 * @param stats Output statistics structure
 * @return 0 on success, error code on failure
 */
int security_async_get_stats(
    const security_async_bridge_t* bridge,
    security_async_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Security-async bridge
 * @return 0 on success, error code on failure
 */
int security_async_reset_stats(security_async_bridge_t* bridge);

/**
 * @brief Check if bridge is fully connected
 *
 * @param bridge Security-async bridge
 * @return true if all required systems connected
 */
bool security_async_is_connected(const security_async_bridge_t* bridge);

/* ============================================================================
 * Threat Intelligence API
 * ============================================================================ */

/**
 * @brief Add entry to threat intel cache
 *
 * WHAT: Cache threat intelligence entry
 * WHY:  Local cache for fast lookup and sharing
 * HOW:  Add to cache, mark for sharing
 *
 * @param bridge Security-async bridge
 * @param entry Intel entry to cache
 * @return 0 on success, error code on failure
 */
int security_async_cache_threat_intel(
    security_async_bridge_t* bridge,
    const threat_intel_entry_t* entry
);

/**
 * @brief Lookup threat in intel cache
 *
 * WHAT: Check if threat is known in intel cache
 * WHY:  Fast local lookup before network query
 * HOW:  Hash lookup in cache
 *
 * @param bridge Security-async bridge
 * @param threat_hash Threat signature hash
 * @param entry Output entry (can be NULL)
 * @return true if found, false otherwise
 */
bool security_async_lookup_threat_intel(
    const security_async_bridge_t* bridge,
    const uint8_t* threat_hash,
    threat_intel_entry_t* entry
);

/**
 * @brief Clear threat intel cache
 *
 * @param bridge Security-async bridge
 * @return 0 on success, error code on failure
 */
int security_async_clear_threat_intel(security_async_bridge_t* bridge);

/**
 * @brief Get threat intel cache statistics
 *
 * @param bridge Security-async bridge
 * @param count Output: number of entries
 * @param confirmed Output: number of confirmed threats
 * @return 0 on success, error code on failure
 */
int security_async_get_intel_stats(
    const security_async_bridge_t* bridge,
    uint32_t* count,
    uint32_t* confirmed
);

/* ============================================================================
 * Emergency Mode API
 * ============================================================================ */

/**
 * @brief Enter emergency security mode
 *
 * WHAT: Activate emergency security posture
 * WHY:  Critical threat requires maximum protection
 * HOW:  Increase all thresholds, enable maximum broadcast
 *
 * @param bridge Security-async bridge
 * @return 0 on success, error code on failure
 */
int security_async_enter_emergency_mode(security_async_bridge_t* bridge);

/**
 * @brief Exit emergency security mode
 *
 * WHAT: Return to normal security posture
 * WHY:  Threat passed, normal operation can resume
 * HOW:  Restore normal thresholds and behavior
 *
 * @param bridge Security-async bridge
 * @return 0 on success, error code on failure
 */
int security_async_exit_emergency_mode(security_async_bridge_t* bridge);

/**
 * @brief Check if in emergency mode
 *
 * @param bridge Security-async bridge
 * @return true if emergency mode active
 */
bool security_async_is_emergency_mode(const security_async_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_ASYNC_BRIDGE_H */
