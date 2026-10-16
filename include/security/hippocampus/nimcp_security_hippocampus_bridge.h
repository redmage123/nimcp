/**
 * @file nimcp_security_hippocampus_bridge.h
 * @brief Security - Hippocampus Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Bidirectional bridge integrating security controls with hippocampal memory
 * WHY:  Protect hippocampal memory consolidation from sleep-phase attacks,
 *       memory injection, and consolidation corruption
 * HOW:  Sleep phase protection, consolidation verification, injection detection,
 *       replay validation, and spatial/temporal coherence checking
 *
 * BIOLOGICAL BASIS:
 * - Hippocampus is critical for memory consolidation during sleep
 * - Sharp-wave ripples (100-200 Hz) drive memory replay during NREM sleep
 * - Sleep spindles (12-16 Hz) gate information flow to cortex
 * - Theta rhythms (4-8 Hz) coordinate encoding and consolidation
 * - Spatial and temporal coherence are essential for memory integrity
 *
 * THREAT MODEL:
 * ```
 * +-------------------------------------------------------------------------+
 * |              HIPPOCAMPUS SECURITY THREAT LANDSCAPE                      |
 * +-------------------------------------------------------------------------+
 * |                                                                         |
 * |   SLEEP-PHASE ATTACKS           MEMORY INJECTION                        |
 * |   +------------------+          +------------------+                    |
 * |   | Replay Hijacking |          | False Memory     |                    |
 * |   | Spindle Blocking |          | Pattern Poisoning|                    |
 * |   | Ripple Injection |          | Temporal Splice  |                    |
 * |   +------------------+          +------------------+                    |
 * |                                                                         |
 * |   CONSOLIDATION CORRUPTION      COHERENCE ATTACKS                       |
 * |   +------------------+          +------------------+                    |
 * |   | Transfer Tampering|          | Spatial Scramble|                    |
 * |   | Strength Degradation|        | Temporal Disorder|                   |
 * |   | Selective Erasure |          | Context Mismatch |                   |
 * |   +------------------+          +------------------+                    |
 * +-------------------------------------------------------------------------+
 * ```
 *
 * SECURITY MODEL:
 * ```
 * +-------------------------------------------------------------------------+
 * |              SECURITY-HIPPOCAMPUS BRIDGE ARCHITECTURE                   |
 * +-------------------------------------------------------------------------+
 * |                                                                         |
 * |   SECURITY LAYER                    HIPPOCAMPUS SYSTEMS                 |
 * |   +-----------------+              +-----------------+                  |
 * |   | Sleep Protection|<----------->| Sharp-Wave Ripples|                 |
 * |   | Consolidation   |<----------->| Memory Consolidation|               |
 * |   | Injection Detect|<----------->| Encoding System  |                  |
 * |   | Replay Validate |<----------->| Replay System    |                  |
 * |   | Coherence Check |<----------->| Place/Time Cells |                  |
 * |   +-----------------+              +-----------------+                  |
 * |          |                                  |                           |
 * |          v                                  v                           |
 * |   +---------------------------------------------+                       |
 * |   |           BIDIRECTIONAL EFFECTS             |                       |
 * |   | Security->Hippo: Replay gates, filters     |                       |
 * |   | Hippo->Security: Anomalies, patterns       |                       |
 * |   +---------------------------------------------+                       |
 * +-------------------------------------------------------------------------+
 * ```
 *
 * SLEEP PHASES AND CONSOLIDATION:
 * - AWAKE: Minimal consolidation, active encoding
 * - LIGHT NREM: Hippocampal-cortical dialogue begins
 * - DEEP NREM: Peak consolidation via slow oscillations
 * - REM: Integration and abstraction
 *
 * @see nimcp_memory_sleep_bridge.h
 * @see nimcp_hippocampus_substrate_bridge.h
 * @see nimcp_security_memory_bridge.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SECURITY_HIPPOCAMPUS_BRIDGE_H
#define NIMCP_SECURITY_HIPPOCAMPUS_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Maximum replay sequences tracked */
#define SEC_HIPPO_MAX_REPLAY_SEQUENCES     512

/** @brief Maximum injection detection patterns */
#define SEC_HIPPO_MAX_INJECTION_PATTERNS   256

/** @brief Maximum place cells tracked for coherence */
#define SEC_HIPPO_MAX_PLACE_CELLS          1024

/** @brief Maximum time cells tracked for coherence */
#define SEC_HIPPO_MAX_TIME_CELLS           512

/** @brief Maximum consolidation events tracked */
#define SEC_HIPPO_MAX_CONSOLIDATION_EVENTS 2048

/** @brief Audit log entry limit */
#define SEC_HIPPO_MAX_AUDIT_ENTRIES        4096

/** @brief Bio-async module ID for security-hippocampus bridge */
#define BIO_MODULE_SECURITY_HIPPOCAMPUS    0x0E20

/** @brief Sleep protection check interval (ms) */
#define SEC_HIPPO_SLEEP_CHECK_INTERVAL_MS  100

/** @brief Replay validation window (ms) */
#define SEC_HIPPO_REPLAY_WINDOW_MS         500

/** @brief Coherence check tolerance (normalized) */
#define SEC_HIPPO_COHERENCE_TOLERANCE      0.15f

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Sleep phases for protection
 *
 * WHAT: Sleep states during which hippocampal consolidation occurs
 * WHY:  Different phases have different security requirements
 */
typedef enum {
    SEC_HIPPO_SLEEP_AWAKE = 0,       /**< Awake - encoding mode */
    SEC_HIPPO_SLEEP_DROWSY,          /**< Transitioning to sleep */
    SEC_HIPPO_SLEEP_LIGHT_NREM,      /**< Light NREM (N1/N2) */
    SEC_HIPPO_SLEEP_DEEP_NREM,       /**< Deep NREM (N3/SWS) */
    SEC_HIPPO_SLEEP_REM              /**< REM sleep */
} sec_hippo_sleep_phase_t;

/**
 * @brief Memory injection attack types
 *
 * WHAT: Types of memory injection attacks detected
 * WHY:  Enable specific countermeasures per attack type
 */
typedef enum {
    SEC_HIPPO_INJECT_NONE = 0,       /**< No injection detected */
    SEC_HIPPO_INJECT_FALSE_MEMORY,   /**< Attempt to insert false memories */
    SEC_HIPPO_INJECT_PATTERN_POISON, /**< Poisoning existing patterns */
    SEC_HIPPO_INJECT_TEMPORAL_SPLICE,/**< Splicing temporal sequences */
    SEC_HIPPO_INJECT_SPATIAL_FAKE,   /**< Fake spatial information */
    SEC_HIPPO_INJECT_CONTEXT_CORRUPT,/**< Context corruption attempt */
    SEC_HIPPO_INJECT_RIPPLE_FORGE    /**< Forged sharp-wave ripples */
} sec_hippo_injection_type_t;

/**
 * @brief Consolidation integrity status
 *
 * WHAT: Status of memory consolidation integrity
 * WHY:  Track consolidation health and corruption
 */
typedef enum {
    SEC_HIPPO_CONSOL_OK = 0,         /**< Consolidation integrity verified */
    SEC_HIPPO_CONSOL_DEGRADED,       /**< Minor degradation detected */
    SEC_HIPPO_CONSOL_CORRUPTED,      /**< Significant corruption */
    SEC_HIPPO_CONSOL_TAMPERED,       /**< Tampering detected */
    SEC_HIPPO_CONSOL_INCOMPLETE      /**< Consolidation incomplete */
} sec_hippo_consolidation_status_t;

/**
 * @brief Replay sequence validation status
 *
 * WHAT: Status of replay sequence validation
 * WHY:  Ensure replay sequences are legitimate
 */
typedef enum {
    SEC_HIPPO_REPLAY_VALID = 0,      /**< Replay sequence valid */
    SEC_HIPPO_REPLAY_OUT_OF_ORDER,   /**< Sequence order violation */
    SEC_HIPPO_REPLAY_TIMING_ANOMALY, /**< Timing inconsistency */
    SEC_HIPPO_REPLAY_CONTENT_MISMATCH,/**< Content doesn't match encoding */
    SEC_HIPPO_REPLAY_FORGED,         /**< Detected forged replay */
    SEC_HIPPO_REPLAY_HIJACKED        /**< Replay sequence hijacked */
} sec_hippo_replay_status_t;

/**
 * @brief Spatial/temporal coherence status
 *
 * WHAT: Coherence status of memory representations
 * WHY:  Detect attacks on spatial and temporal memory structures
 */
typedef enum {
    SEC_HIPPO_COHERENCE_OK = 0,      /**< Full coherence */
    SEC_HIPPO_COHERENCE_SPATIAL_DRIFT,/**< Spatial representation drifting */
    SEC_HIPPO_COHERENCE_TEMPORAL_GAP,/**< Temporal gaps detected */
    SEC_HIPPO_COHERENCE_CONTEXT_MISMATCH,/**< Context coherence broken */
    SEC_HIPPO_COHERENCE_SCRAMBLED    /**< Coherence completely broken */
} sec_hippo_coherence_status_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    SEC_HIPPO_STATE_IDLE = 0,        /**< Bridge idle */
    SEC_HIPPO_STATE_MONITORING,      /**< Active monitoring */
    SEC_HIPPO_STATE_PROTECTING,      /**< Active protection engaged */
    SEC_HIPPO_STATE_VERIFYING,       /**< Verification in progress */
    SEC_HIPPO_STATE_DETECTING,       /**< Detection in progress */
    SEC_HIPPO_STATE_VALIDATING,      /**< Validation in progress */
    SEC_HIPPO_STATE_RESPONDING,      /**< Responding to threat */
    SEC_HIPPO_STATE_ERROR            /**< Error state */
} sec_hippo_state_t;

/**
 * @brief Audit event types
 */
typedef enum {
    SEC_HIPPO_AUDIT_SLEEP_PROTECT = 0,/**< Sleep protection event */
    SEC_HIPPO_AUDIT_CONSOL_VERIFY,   /**< Consolidation verification */
    SEC_HIPPO_AUDIT_INJECT_DETECT,   /**< Injection detection */
    SEC_HIPPO_AUDIT_REPLAY_VALIDATE, /**< Replay validation */
    SEC_HIPPO_AUDIT_COHERENCE_CHECK, /**< Coherence check */
    SEC_HIPPO_AUDIT_THREAT_RESPONSE, /**< Threat response */
    SEC_HIPPO_AUDIT_ANOMALY          /**< Anomaly detected */
} sec_hippo_audit_type_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief Security-Hippocampus bridge configuration
 *
 * WHAT: Configuration parameters for the bridge
 * WHY:  Control security features and behavior
 * HOW:  Enable/disable features, set thresholds and policies
 */
typedef struct {
    /* Feature enable flags */
    bool enable_sleep_protection;     /**< Protect sleep-phase consolidation */
    bool enable_consolidation_verify; /**< Verify consolidation integrity */
    bool enable_injection_detection;  /**< Detect memory injection attempts */
    bool enable_replay_validation;    /**< Validate replay sequences */
    bool enable_coherence_checking;   /**< Check spatial/temporal coherence */
    bool enable_audit;                /**< Enable audit logging */

    /* Sleep protection settings */
    bool protect_nrem_deep;           /**< Protect deep NREM especially */
    bool protect_rem;                 /**< Protect REM sleep */
    float ripple_filter_threshold;    /**< Threshold for ripple filtering [0-1] */
    float spindle_gate_threshold;     /**< Threshold for spindle gating [0-1] */

    /* Consolidation settings */
    float consolidation_min_strength; /**< Minimum consolidation strength [0-1] */
    uint32_t consolidation_check_interval_ms; /**< Check interval */
    float degradation_threshold;      /**< Threshold for degradation alert [0-1] */

    /* Injection detection settings */
    float injection_sensitivity;      /**< Detection sensitivity [0.5-2.0] */
    uint32_t pattern_match_window;    /**< Pattern matching window size */
    float false_positive_tolerance;   /**< Tolerated false positive rate [0-1] */

    /* Replay validation settings */
    float replay_timing_tolerance_ms; /**< Timing tolerance in ms */
    float replay_content_match_threshold; /**< Content match threshold [0-1] */
    uint32_t replay_sequence_max_gap; /**< Max sequence gap allowed */

    /* Coherence settings */
    float spatial_coherence_threshold;/**< Spatial coherence threshold [0-1] */
    float temporal_coherence_threshold;/**< Temporal coherence threshold [0-1] */
    float context_coherence_threshold;/**< Context coherence threshold [0-1] */

    /* Sensitivity parameters */
    float security_sensitivity;       /**< Overall security sensitivity [0.5-2.0] */
    float hippocampus_sensitivity;    /**< Hippocampus protection sensitivity [0.5-2.0] */

    /* Bio-async integration */
    bool enable_bio_async;            /**< Enable bio-async callbacks */
} sec_hippo_config_t;

/* ============================================================================
 * Replay Sequence Structure
 * ============================================================================ */

/**
 * @brief Replay sequence information
 *
 * WHAT: Information about a memory replay sequence
 * WHY:  Track and validate replay events
 */
typedef struct {
    uint64_t sequence_id;             /**< Unique sequence identifier */
    uint64_t start_time;              /**< Sequence start timestamp */
    uint64_t end_time;                /**< Sequence end timestamp */
    uint32_t event_count;             /**< Number of events in sequence */
    float replay_strength;            /**< Replay strength [0-1] */
    float content_match_score;        /**< Content match score [0-1] */
    sec_hippo_replay_status_t status; /**< Validation status */
    uint32_t encoding_id;             /**< Original encoding ID */
    bool during_sleep;                /**< Occurred during sleep */
    sec_hippo_sleep_phase_t sleep_phase; /**< Sleep phase if during sleep */
} sec_hippo_replay_sequence_t;

/* ============================================================================
 * Consolidation Event Structure
 * ============================================================================ */

/**
 * @brief Consolidation event information
 *
 * WHAT: Information about a consolidation event
 * WHY:  Track and verify consolidation integrity
 */
typedef struct {
    uint64_t event_id;                /**< Unique event identifier */
    uint64_t timestamp;               /**< Event timestamp */
    uint64_t memory_id;               /**< Memory being consolidated */
    float strength_before;            /**< Strength before consolidation */
    float strength_after;             /**< Strength after consolidation */
    float transfer_rate;              /**< Transfer rate to cortex */
    sec_hippo_consolidation_status_t status; /**< Consolidation status */
    sec_hippo_sleep_phase_t sleep_phase; /**< Sleep phase during consolidation */
    bool verified;                    /**< Whether verified */
} sec_hippo_consolidation_event_t;

/* ============================================================================
 * Effects Structures
 * ============================================================================ */

/**
 * @brief Security effects on hippocampus
 *
 * WHAT: How security controls affect hippocampal operations
 * WHY:  Track security impositions on hippocampus behavior
 */
typedef struct {
    /* Sleep protection effects */
    bool sleep_protection_active;     /**< Sleep protection engaged */
    float ripple_filter_level;        /**< Current ripple filtering level [0-1] */
    float spindle_gate_level;         /**< Current spindle gating level [0-1] */
    uint32_t blocked_ripples;         /**< Ripples blocked this session */

    /* Consolidation effects */
    bool consolidation_paused;        /**< Consolidation paused for verification */
    float consolidation_throttle;     /**< Throttle factor [0-1] */
    uint32_t verified_consolidations; /**< Consolidations verified */
    uint32_t rejected_consolidations; /**< Consolidations rejected */

    /* Injection detection effects */
    bool injection_guard_active;      /**< Injection guard engaged */
    uint32_t injections_blocked;      /**< Injection attempts blocked */
    float current_threat_level;       /**< Current threat level [0-1] */

    /* Replay effects */
    bool replay_validation_active;    /**< Replay validation engaged */
    uint32_t replays_validated;       /**< Replays validated this session */
    uint32_t replays_rejected;        /**< Replays rejected this session */

    /* Performance impact */
    float security_latency_ms;        /**< Security processing latency */
    float throughput_reduction;       /**< Throughput reduction factor [0-1] */
} security_to_hippo_effects_t;

/**
 * @brief Hippocampus effects on security
 *
 * WHAT: How hippocampal behavior affects security
 * WHY:  Hippocampus patterns inform security decisions
 */
typedef struct {
    /* Current state */
    sec_hippo_sleep_phase_t current_sleep_phase; /**< Current sleep phase */
    float current_consolidation_rate; /**< Current consolidation rate */
    float current_replay_frequency;   /**< Current replay frequency Hz */
    float theta_power;                /**< Theta oscillation power [0-1] */
    float gamma_power;                /**< Gamma oscillation power [0-1] */

    /* Anomaly indicators */
    bool anomaly_detected;            /**< Anomaly detected */
    float anomaly_score;              /**< Anomaly severity [0-1] */
    char anomaly_description[128];    /**< Anomaly description */

    /* Injection indicators */
    sec_hippo_injection_type_t injection_type; /**< Detected injection type */
    float injection_confidence;       /**< Injection detection confidence [0-1] */

    /* Coherence status */
    sec_hippo_coherence_status_t coherence_status; /**< Current coherence status */
    float spatial_coherence;          /**< Spatial coherence score [0-1] */
    float temporal_coherence;         /**< Temporal coherence score [0-1] */

    /* Pattern indicators */
    float pattern_regularity;         /**< Pattern regularity [0-1] */
    uint32_t active_place_cells;      /**< Active place cells */
    uint32_t active_time_cells;       /**< Active time cells */
} hippo_to_security_effects_t;

/* ============================================================================
 * State and Statistics Structures
 * ============================================================================ */

/**
 * @brief Bridge operational state
 */
typedef struct {
    sec_hippo_state_t state;          /**< Current operational state */
    uint64_t last_sleep_check;        /**< Last sleep protection check */
    uint64_t last_consol_verify;      /**< Last consolidation verification */
    uint64_t last_inject_scan;        /**< Last injection scan */
    uint64_t last_replay_validate;    /**< Last replay validation */
    uint64_t last_coherence_check;    /**< Last coherence check */
    uint32_t active_protections;      /**< Number of active protections */
    bool hippocampus_connected;       /**< Hippocampus system connected */
    bool sleep_system_connected;      /**< Sleep system connected */
} sec_hippo_state_info_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Sleep protection stats */
    uint64_t sleep_protection_activations; /**< Protection activations */
    uint64_t sleep_phases_protected;  /**< Sleep phases protected */
    uint64_t ripples_analyzed;        /**< Sharp-wave ripples analyzed */
    uint64_t ripples_blocked;         /**< Ripples blocked as suspicious */
    float mean_ripple_check_latency_us; /**< Mean ripple check latency */

    /* Consolidation verification stats */
    uint64_t consolidation_checks;    /**< Consolidation checks performed */
    uint64_t consolidations_verified; /**< Consolidations verified OK */
    uint64_t consolidations_degraded; /**< Degraded consolidations */
    uint64_t consolidations_corrupted;/**< Corrupted consolidations */
    float mean_consol_verify_latency_us; /**< Mean verification latency */

    /* Injection detection stats */
    uint64_t injection_scans;         /**< Injection detection scans */
    uint64_t injections_detected;     /**< Injections detected */
    uint64_t injections_blocked;      /**< Injections blocked */
    uint64_t false_positives;         /**< Known false positives */
    float mean_inject_scan_latency_us;/**< Mean scan latency */

    /* Replay validation stats */
    uint64_t replays_validated;       /**< Replay sequences validated */
    uint64_t replays_valid;           /**< Valid replays */
    uint64_t replays_invalid;         /**< Invalid replays */
    uint64_t replays_hijacked;        /**< Hijacked replays detected */
    float mean_replay_validate_latency_us; /**< Mean validation latency */

    /* Coherence check stats */
    uint64_t coherence_checks;        /**< Coherence checks performed */
    uint64_t coherence_ok;            /**< Coherence OK */
    uint64_t coherence_failures;      /**< Coherence failures */
    float mean_spatial_coherence;     /**< Mean spatial coherence score */
    float mean_temporal_coherence;    /**< Mean temporal coherence score */

    /* Audit stats */
    uint64_t audit_entries;           /**< Total audit entries */
    uint64_t audit_alerts;            /**< Audit alerts generated */

    /* Per-sleep-phase stats */
    uint64_t nrem_deep_events;        /**< Events during deep NREM */
    uint64_t rem_events;              /**< Events during REM */
    uint64_t awake_events;            /**< Events while awake */
} sec_hippo_stats_t;

/* ============================================================================
 * Audit Entry Structure
 * ============================================================================ */

/**
 * @brief Audit log entry
 */
typedef struct {
    uint64_t timestamp;               /**< Event timestamp */
    sec_hippo_audit_type_t type;      /**< Event type */
    sec_hippo_sleep_phase_t sleep_phase; /**< Sleep phase during event */
    bool success;                     /**< Whether operation succeeded */
    float severity;                   /**< Event severity [0-1] */
    char details[256];                /**< Additional details */
} sec_hippo_audit_entry_t;

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

/* Hippocampus system - forward declaration */
typedef struct hippocampus_system* hippocampus_system_t;

/* Sleep system - forward declaration */
typedef struct sleep_system_struct* sleep_system_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Security-Hippocampus bridge
 *
 * WHAT: Main bridge structure connecting security to hippocampus
 * WHY:  Centralized security control for hippocampal operations
 * HOW:  Contains connections, effects, state, and configuration
 */
typedef struct sec_hippo_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    sec_hippo_config_t config;        /**< Bridge configuration */

    /* System connections */
    hippocampus_system_t hippocampus; /**< Connected hippocampus system */
    sleep_system_t sleep_system;      /**< Connected sleep system */

    /* Connection flags */
    bool hippocampus_connected;       /**< Hippocampus connected */
    bool sleep_connected;             /**< Sleep system connected */

    /* Bidirectional effects */
    security_to_hippo_effects_t security_effects; /**< Security->Hippo effects */
    hippo_to_security_effects_t hippo_effects;    /**< Hippo->Security effects */

    /* State and statistics */
    sec_hippo_state_info_t state;     /**< Current operational state */
    sec_hippo_stats_t stats;          /**< Operational statistics */

    /* Replay sequence tracking */
    sec_hippo_replay_sequence_t* replay_sequences; /**< Tracked sequences */
    uint32_t num_replay_sequences;    /**< Number of sequences */

    /* Consolidation event tracking */
    sec_hippo_consolidation_event_t* consolidation_events; /**< Tracked events */
    uint32_t num_consolidation_events;/**< Number of events */

    /* Audit log */
    sec_hippo_audit_entry_t* audit_log; /**< Audit log buffer */
    uint32_t audit_log_head;          /**< Circular buffer head */
    uint32_t audit_log_count;         /**< Number of entries in log */
} sec_hippo_bridge_t;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Starting point for most deployments
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 *
 * Defaults:
 * - enable_sleep_protection: true
 * - enable_consolidation_verify: true
 * - enable_injection_detection: true
 * - enable_replay_validation: true
 * - enable_coherence_checking: true
 * - security_sensitivity: 1.0
 */
int security_hippocampus_default_config(sec_hippo_config_t* config);

/**
 * @brief Create security-hippocampus bridge
 *
 * WHAT: Allocates and initializes bridge
 * WHY:  Entry point for security-hippocampus integration
 * HOW:  Allocates structures, initializes state, applies config
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 *
 * Memory: ~128KB for default configuration
 * Thread safety: Returned handle is thread-safe
 */
sec_hippo_bridge_t* security_hippocampus_bridge_create(
    const sec_hippo_config_t* config
);

/**
 * @brief Destroy security-hippocampus bridge
 *
 * WHAT: Releases all resources
 * WHY:  Clean shutdown
 * HOW:  Disconnects systems, frees memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void security_hippocampus_bridge_destroy(sec_hippo_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * WHAT: Clears state while preserving connections
 * WHY:  Fresh start without reconnection
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_hippocampus_bridge_reset(sec_hippo_bridge_t* bridge);

/* ============================================================================
 * Connection Functions
 * ============================================================================ */

/**
 * @brief Connect hippocampus system
 *
 * @param bridge Bridge handle
 * @param hippocampus Hippocampus system
 * @return 0 on success, -1 on error
 */
int security_hippocampus_connect_hippo(
    sec_hippo_bridge_t* bridge,
    hippocampus_system_t hippocampus
);

/**
 * @brief Connect sleep system
 *
 * @param bridge Bridge handle
 * @param sleep_system Sleep system
 * @return 0 on success, -1 on error
 */
int security_hippocampus_connect_sleep(
    sec_hippo_bridge_t* bridge,
    sleep_system_t sleep_system
);

/**
 * @brief Disconnect all systems
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_hippocampus_disconnect_all(sec_hippo_bridge_t* bridge);

/**
 * @brief Check if fully connected
 *
 * @param bridge Bridge handle
 * @return true if all systems connected, false otherwise
 */
bool security_hippocampus_is_fully_connected(const sec_hippo_bridge_t* bridge);

/* ============================================================================
 * Sleep Protection Functions
 * ============================================================================ */

/**
 * @brief Protect sleep-phase consolidation
 *
 * WHAT: Activate protection for current sleep phase
 * WHY:  Prevent attacks during vulnerable consolidation
 * HOW:  Monitor ripples, gate spindles, filter suspicious activity
 *
 * @param bridge Bridge handle
 * @param phase Current sleep phase
 * @return 0 on success, -1 on error
 *
 * Side effects:
 * - Activates ripple filtering
 * - Engages spindle gating
 * - Updates security effects
 * - Creates audit entry
 */
int security_hippocampus_protect_sleep(
    sec_hippo_bridge_t* bridge,
    sec_hippo_sleep_phase_t phase
);

/**
 * @brief Set sleep phase (for monitoring)
 *
 * @param bridge Bridge handle
 * @param phase New sleep phase
 * @return 0 on success, -1 on error
 */
int security_hippocampus_set_sleep_phase(
    sec_hippo_bridge_t* bridge,
    sec_hippo_sleep_phase_t phase
);

/**
 * @brief Get current sleep phase
 *
 * @param bridge Bridge handle
 * @return Current sleep phase, SEC_HIPPO_SLEEP_AWAKE on error
 */
sec_hippo_sleep_phase_t security_hippocampus_get_sleep_phase(
    const sec_hippo_bridge_t* bridge
);

/**
 * @brief Check if in protected sleep phase
 *
 * @param bridge Bridge handle
 * @return true if in protected phase, false otherwise
 */
bool security_hippocampus_is_sleep_protected(const sec_hippo_bridge_t* bridge);

/* ============================================================================
 * Consolidation Verification Functions
 * ============================================================================ */

/**
 * @brief Verify consolidation integrity
 *
 * WHAT: Check integrity of memory consolidation
 * WHY:  Detect tampering, corruption, or degradation
 * HOW:  Compare strength, verify patterns, check transfer
 *
 * @param bridge Bridge handle
 * @param memory_id Memory ID to verify
 * @param status_out Output consolidation status
 * @param confidence_out Output confidence level [0-1]
 * @return 0 on success, -1 on error
 *
 * Side effects:
 * - Updates consolidation statistics
 * - Creates audit entry
 */
int security_hippocampus_verify_consolidation(
    sec_hippo_bridge_t* bridge,
    uint64_t memory_id,
    sec_hippo_consolidation_status_t* status_out,
    float* confidence_out
);

/**
 * @brief Verify all pending consolidations
 *
 * @param bridge Bridge handle
 * @param verified_count_out Output count of verified
 * @param failed_count_out Output count of failed
 * @return 0 on success, -1 on error
 */
int security_hippocampus_verify_all_consolidations(
    sec_hippo_bridge_t* bridge,
    uint32_t* verified_count_out,
    uint32_t* failed_count_out
);

/**
 * @brief Pause consolidation for verification
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_hippocampus_pause_consolidation(sec_hippo_bridge_t* bridge);

/**
 * @brief Resume consolidation after verification
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_hippocampus_resume_consolidation(sec_hippo_bridge_t* bridge);

/* ============================================================================
 * Injection Detection Functions
 * ============================================================================ */

/**
 * @brief Detect memory injection
 *
 * WHAT: Scan for memory injection attempts
 * WHY:  Prevent false memories, pattern poisoning
 * HOW:  Pattern analysis, provenance checking, signature validation
 *
 * @param bridge Bridge handle
 * @param injection_out Output injection type detected
 * @param confidence_out Output detection confidence [0-1]
 * @param details_out Output buffer for details (can be NULL)
 * @param details_size Size of details buffer
 * @return true if injection detected, false otherwise
 *
 * Side effects:
 * - Updates injection statistics
 * - May block suspicious activity
 * - Creates audit entry
 */
bool security_hippocampus_detect_injection(
    sec_hippo_bridge_t* bridge,
    sec_hippo_injection_type_t* injection_out,
    float* confidence_out,
    char* details_out,
    size_t details_size
);

/**
 * @brief Block detected injection
 *
 * @param bridge Bridge handle
 * @param injection_type Type of injection to block
 * @return 0 on success, -1 on error
 */
int security_hippocampus_block_injection(
    sec_hippo_bridge_t* bridge,
    sec_hippo_injection_type_t injection_type
);

/**
 * @brief Register legitimate encoding pattern
 *
 * WHAT: Whitelist a known legitimate pattern
 * WHY:  Reduce false positives in injection detection
 *
 * @param bridge Bridge handle
 * @param pattern_id Pattern identifier
 * @param pattern_hash Pattern hash for verification
 * @return 0 on success, -1 on error
 */
int security_hippocampus_register_pattern(
    sec_hippo_bridge_t* bridge,
    uint64_t pattern_id,
    uint64_t pattern_hash
);

/* ============================================================================
 * Replay Validation Functions
 * ============================================================================ */

/**
 * @brief Validate replay sequence
 *
 * WHAT: Validate a memory replay sequence
 * WHY:  Ensure replays match original encoding, detect hijacking
 * HOW:  Check timing, content, sequence order
 *
 * @param bridge Bridge handle
 * @param sequence_id Sequence to validate
 * @param status_out Output validation status
 * @param match_score_out Output content match score [0-1]
 * @return 0 on success, -1 on error
 *
 * Side effects:
 * - Updates replay statistics
 * - May reject invalid replays
 * - Creates audit entry
 */
int security_hippocampus_validate_replay(
    sec_hippo_bridge_t* bridge,
    uint64_t sequence_id,
    sec_hippo_replay_status_t* status_out,
    float* match_score_out
);

/**
 * @brief Register encoding for replay validation
 *
 * WHAT: Register an encoding event for later replay validation
 * WHY:  Enable comparison between encoding and replay
 *
 * @param bridge Bridge handle
 * @param encoding_id Encoding identifier
 * @param content_hash Hash of encoded content
 * @param timestamp Encoding timestamp
 * @return 0 on success, -1 on error
 */
int security_hippocampus_register_encoding(
    sec_hippo_bridge_t* bridge,
    uint64_t encoding_id,
    uint64_t content_hash,
    uint64_t timestamp
);

/**
 * @brief Reject invalid replay
 *
 * @param bridge Bridge handle
 * @param sequence_id Sequence to reject
 * @return 0 on success, -1 on error
 */
int security_hippocampus_reject_replay(
    sec_hippo_bridge_t* bridge,
    uint64_t sequence_id
);

/**
 * @brief Get replay sequence info
 *
 * @param bridge Bridge handle
 * @param sequence_id Sequence ID
 * @param sequence_out Output sequence info
 * @return 0 on success, -1 on error
 */
int security_hippocampus_get_replay_info(
    sec_hippo_bridge_t* bridge,
    uint64_t sequence_id,
    sec_hippo_replay_sequence_t* sequence_out
);

/* ============================================================================
 * Coherence Checking Functions
 * ============================================================================ */

/**
 * @brief Check spatial/temporal coherence
 *
 * WHAT: Verify spatial and temporal coherence of memories
 * WHY:  Detect attacks on place cells, time cells, context
 * HOW:  Compare firing patterns, check grid alignment
 *
 * @param bridge Bridge handle
 * @param status_out Output coherence status
 * @param spatial_score_out Output spatial coherence score [0-1]
 * @param temporal_score_out Output temporal coherence score [0-1]
 * @return 0 on success, -1 on error
 *
 * Side effects:
 * - Updates coherence statistics
 * - Creates audit entry if issues detected
 */
int security_hippocampus_check_coherence(
    sec_hippo_bridge_t* bridge,
    sec_hippo_coherence_status_t* status_out,
    float* spatial_score_out,
    float* temporal_score_out
);

/**
 * @brief Check spatial coherence only
 *
 * @param bridge Bridge handle
 * @param score_out Output spatial coherence score [0-1]
 * @return 0 on success, -1 on error
 */
int security_hippocampus_check_spatial_coherence(
    sec_hippo_bridge_t* bridge,
    float* score_out
);

/**
 * @brief Check temporal coherence only
 *
 * @param bridge Bridge handle
 * @param score_out Output temporal coherence score [0-1]
 * @return 0 on success, -1 on error
 */
int security_hippocampus_check_temporal_coherence(
    sec_hippo_bridge_t* bridge,
    float* score_out
);

/**
 * @brief Report place cell activity
 *
 * WHAT: Report place cell firing for coherence tracking
 *
 * @param bridge Bridge handle
 * @param cell_id Place cell ID
 * @param position_x X position
 * @param position_y Y position
 * @param firing_rate Firing rate
 * @return 0 on success, -1 on error
 */
int security_hippocampus_report_place_cell(
    sec_hippo_bridge_t* bridge,
    uint32_t cell_id,
    float position_x,
    float position_y,
    float firing_rate
);

/**
 * @brief Report time cell activity
 *
 * WHAT: Report time cell firing for coherence tracking
 *
 * @param bridge Bridge handle
 * @param cell_id Time cell ID
 * @param timestamp Timestamp of firing
 * @param firing_rate Firing rate
 * @return 0 on success, -1 on error
 */
int security_hippocampus_report_time_cell(
    sec_hippo_bridge_t* bridge,
    uint32_t cell_id,
    uint64_t timestamp,
    float firing_rate
);

/* ============================================================================
 * Bidirectional Update Functions
 * ============================================================================ */

/**
 * @brief Update bridge state (main update loop)
 *
 * WHAT: Process pending operations and update effects
 * WHY:  Maintain bridge synchronization
 * HOW:  Run all checks, update effects, process events
 *
 * @param bridge Bridge handle
 * @param delta_ms Time since last update in milliseconds
 * @return 0 on success, -1 on error
 *
 * Call frequency: Recommended 10-100ms intervals
 */
int security_hippocampus_bridge_update(
    sec_hippo_bridge_t* bridge,
    uint64_t delta_ms
);

/**
 * @brief Apply security effects to hippocampus
 *
 * WHAT: Propagate security state to hippocampus
 * WHY:  Enforce security controls on hippocampal behavior
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_hippocampus_apply_security_effects(sec_hippo_bridge_t* bridge);

/**
 * @brief Gather hippocampus effects for security
 *
 * WHAT: Collect hippocampal behavior data for security
 * WHY:  Inform security decisions based on hippocampal patterns
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_hippocampus_gather_hippo_effects(sec_hippo_bridge_t* bridge);

/* ============================================================================
 * Query Functions
 * ============================================================================ */

/**
 * @brief Get security effects on hippocampus
 *
 * @param bridge Bridge handle
 * @param effects_out Output effects structure
 * @return 0 on success, -1 on error
 */
int security_hippocampus_get_security_effects(
    const sec_hippo_bridge_t* bridge,
    security_to_hippo_effects_t* effects_out
);

/**
 * @brief Get hippocampus effects on security
 *
 * @param bridge Bridge handle
 * @param effects_out Output effects structure
 * @return 0 on success, -1 on error
 */
int security_hippocampus_get_hippo_effects(
    const sec_hippo_bridge_t* bridge,
    hippo_to_security_effects_t* effects_out
);

/**
 * @brief Get bridge state information
 *
 * @param bridge Bridge handle
 * @param state_out Output state structure
 * @return 0 on success, -1 on error
 */
int security_hippocampus_get_state(
    const sec_hippo_bridge_t* bridge,
    sec_hippo_state_info_t* state_out
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats_out Output statistics structure
 * @return 0 on success, -1 on error
 */
int security_hippocampus_get_stats(
    const sec_hippo_bridge_t* bridge,
    sec_hippo_stats_t* stats_out
);

/**
 * @brief Reset statistics counters
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_hippocampus_reset_stats(sec_hippo_bridge_t* bridge);

/* ============================================================================
 * Audit Functions
 * ============================================================================ */

/**
 * @brief Get recent audit entries
 *
 * @param bridge Bridge handle
 * @param entries_out Output buffer for entries
 * @param max_entries Maximum entries to return
 * @param count_out Actual entries returned
 * @return 0 on success, -1 on error
 */
int security_hippocampus_get_audit_log(
    const sec_hippo_bridge_t* bridge,
    sec_hippo_audit_entry_t* entries_out,
    size_t max_entries,
    size_t* count_out
);

/**
 * @brief Clear audit log
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_hippocampus_clear_audit_log(sec_hippo_bridge_t* bridge);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_hippocampus_connect_bio_async(sec_hippo_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_hippocampus_disconnect_bio_async(sec_hippo_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 *
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool security_hippocampus_is_bio_async_connected(const sec_hippo_bridge_t* bridge);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get sleep phase name
 *
 * @param phase Sleep phase
 * @return Human-readable name
 */
const char* security_hippocampus_sleep_phase_name(sec_hippo_sleep_phase_t phase);

/**
 * @brief Get injection type name
 *
 * @param type Injection type
 * @return Human-readable name
 */
const char* security_hippocampus_injection_name(sec_hippo_injection_type_t type);

/**
 * @brief Get consolidation status name
 *
 * @param status Consolidation status
 * @return Human-readable name
 */
const char* security_hippocampus_consolidation_name(sec_hippo_consolidation_status_t status);

/**
 * @brief Get replay status name
 *
 * @param status Replay status
 * @return Human-readable name
 */
const char* security_hippocampus_replay_name(sec_hippo_replay_status_t status);

/**
 * @brief Get coherence status name
 *
 * @param status Coherence status
 * @return Human-readable name
 */
const char* security_hippocampus_coherence_name(sec_hippo_coherence_status_t status);

/**
 * @brief Get bridge state name
 *
 * @param state Bridge state
 * @return Human-readable name
 */
const char* security_hippocampus_state_name(sec_hippo_state_t state);

/**
 * @brief Get audit type name
 *
 * @param type Audit type
 * @return Human-readable name
 */
const char* security_hippocampus_audit_type_name(sec_hippo_audit_type_t type);

/**
 * @brief Print bridge state summary (debug)
 *
 * @param bridge Bridge handle
 */
void security_hippocampus_print_summary(const sec_hippo_bridge_t* bridge);

/**
 * @brief Print statistics (debug)
 *
 * @param stats Statistics to print
 */
void security_hippocampus_print_stats(const sec_hippo_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_HIPPOCAMPUS_BRIDGE_H */
