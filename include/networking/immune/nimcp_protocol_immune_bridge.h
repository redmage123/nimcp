/**
 * @file nimcp_protocol_immune_bridge.h
 * @brief Protocol-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between brain immune system and NIMCP protocol layer
 * WHY:  Protocol violations and message corruption map to immune threats; immune state
 *       should modulate protocol validation strictness and error handling.
 * HOW:  Protocol errors trigger immune responses; cytokines adjust validation thresholds;
 *       inflammation → stricter validation, antibodies → message filters.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → PROTOCOL PATHWAYS:
 * ---------------------------
 * 1. Inflammation and Protocol Strictness:
 *    - Local inflammation → enable checksum validation
 *    - Regional inflammation → strict feature code filtering
 *    - Systemic inflammation → reject unknown message types
 *    - Cytokine storm → accept only authenticated messages
 *    - Reference: Heightened immune state → stricter barrier function (BBB analogy)
 *
 * 2. Cytokine-Mediated Protocol Adaptation:
 *    - IL-1β → increase message timeout (allow slower peers)
 *    - IL-6 → enable sequence number tracking (detect replay)
 *    - TNF-α → drop low-confidence messages
 *    - IFN-γ → quarantine mode (whitelist-only communication)
 *    - IL-10 → relax validation (recovery mode)
 *
 * 3. Antibody-Based Message Filtering:
 *    - IgM antibodies → temporary block of suspicious patterns
 *    - IgG antibodies → permanent filter rules
 *    - IgE antibodies → emergency block (DDoS protection)
 *
 * PROTOCOL → IMMUNE PATHWAYS:
 * ---------------------------
 * 1. Protocol Violations as Immune Threats:
 *    - Invalid magic number → antigen presentation
 *    - Checksum mismatch → antigen with signature
 *    - Version mismatch → mild immune activation
 *    - Malformed message → high-severity antigen
 *
 * 2. Message Patterns as Threat Signatures:
 *    - Repeated errors from source → epitope formation
 *    - DDoS patterns → rapid immune escalation
 *    - Protocol scan attempts → memory formation
 *
 * 3. Recovery and Memory:
 *    - Error rate normalization → IL-10 release
 *    - Successful reconnection → memory B cell formation
 *    - Whitelisted source → regulatory T cell (suppress response)
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                   PROTOCOL-IMMUNE BRIDGE                                   ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              IMMUNE → PROTOCOL PATHWAYS                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ INFLAMMATION │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ LOCAL    → enable checksums                                    │  ║
 * ║   │   │ REGIONAL → strict filtering                                    │  ║
 * ║   │   │ SYSTEMIC → reject unknown                                      │  ║
 * ║   │   │ STORM    → authenticated only                                  │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  ANTIBODIES  │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IgM → temp message block                                       │  ║
 * ║   │   │ IgG → permanent filter                                         │  ║
 * ║   │   │ IgE → emergency block                                          │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              PROTOCOL → IMMUNE PATHWAYS                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ VIOLATIONS   │ → Antigen Presentation                          │  ║
 * ║   │   │ ERRORS       │ → Epitope Formation                             │  ║
 * ║   │   │ RECOVERY     │ → IL-10 Release                                 │  ║
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

#ifndef NIMCP_PROTOCOL_IMMUNE_BRIDGE_H
#define NIMCP_PROTOCOL_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "networking/protocol/nimcp_protocol.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Protocol error severities */
#define PROTOCOL_ERROR_INVALID_MAGIC_SEVERITY    8  /**< Invalid magic → severity 8 */
#define PROTOCOL_ERROR_CHECKSUM_SEVERITY         7  /**< Checksum fail → severity 7 */
#define PROTOCOL_ERROR_VERSION_SEVERITY          4  /**< Version mismatch → severity 4 */
#define PROTOCOL_ERROR_MALFORMED_SEVERITY        9  /**< Malformed → severity 9 */

/* Inflammation validation thresholds */
#define INFLAMMATION_NONE_VALIDATION_STRICTNESS     0.5f  /**< Normal: 50% */
#define INFLAMMATION_LOCAL_VALIDATION_STRICTNESS    0.7f  /**< Local: 70% */
#define INFLAMMATION_REGIONAL_VALIDATION_STRICTNESS 0.85f /**< Regional: 85% */
#define INFLAMMATION_SYSTEMIC_VALIDATION_STRICTNESS 0.95f /**< Systemic: 95% */
#define INFLAMMATION_STORM_VALIDATION_STRICTNESS    1.0f  /**< Storm: 100% */

/* Cytokine protocol effects */
#define CYTOKINE_IL1_TIMEOUT_MULTIPLIER       1.5f   /**< IL-1β → +50% timeout */
#define CYTOKINE_IL6_SEQUENCE_TRACKING        true   /**< IL-6 → track sequences */
#define CYTOKINE_TNF_CONFIDENCE_THRESHOLD     0.7f   /**< TNF-α → min 70% confidence */
#define CYTOKINE_IFN_QUARANTINE_MODE          true   /**< IFN-γ → quarantine */
#define CYTOKINE_IL10_VALIDATION_RELAXATION   0.3f   /**< IL-10 → relax 30% */

/* Error rate thresholds */
#define ERROR_RATE_LOCAL_THRESHOLD      0.01f   /**< >1% errors → local inflammation */
#define ERROR_RATE_REGIONAL_THRESHOLD   0.05f   /**< >5% errors → regional */
#define ERROR_RATE_SYSTEMIC_THRESHOLD   0.15f   /**< >15% errors → systemic */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Protocol error metrics
 */
typedef struct {
    uint32_t total_messages;        /**< Total messages processed */
    uint32_t invalid_magic_errors;  /**< Invalid magic number */
    uint32_t checksum_errors;       /**< Checksum failures */
    uint32_t version_errors;        /**< Version mismatches */
    uint32_t malformed_errors;      /**< Malformed messages */
    uint32_t unknown_type_errors;   /**< Unknown message types */
    float error_rate;               /**< Overall error rate [0-1] */
} protocol_error_metrics_t;

/**
 * @brief Cytokine protocol effects
 */
typedef struct {
    /* Cytokine levels */
    float il1_level;
    float il6_level;
    float tnf_level;
    float ifn_gamma_level;
    float il10_level;

    /* Computed effects */
    float timeout_multiplier;       /**< Message timeout adjustment */
    bool sequence_tracking_enabled; /**< Track sequence numbers */
    float confidence_threshold;     /**< Min confidence to accept */
    bool quarantine_mode;           /**< Whitelist-only mode */
    float validation_relaxation;    /**< Relax validation strictness */
} cytokine_protocol_effects_t;

/**
 * @brief Inflammation protocol state
 */
typedef struct {
    brain_inflammation_level_t current_level;
    float validation_strictness;    /**< Validation threshold [0-1] */
    bool checksum_required;         /**< Checksums mandatory */
    bool strict_filtering;          /**< Strict feature filtering */
    bool reject_unknown_types;      /**< Reject unknown message types */
    bool authenticated_only;        /**< Require authentication */
} inflammation_protocol_state_t;

/**
 * @brief Antibody message filter
 */
typedef struct {
    uint32_t antibody_id;           /**< Associated antibody ID */
    brain_antibody_class_t ab_class;
    uint8_t blocked_pattern[64];    /**< Pattern to block */
    size_t pattern_len;
    uint32_t source_node_filter;    /**< Source to filter (0=all) */
    bool permanent;                 /**< Permanent filter (IgG) */
    uint64_t expiry_time;           /**< Expiry for temp filters */
    uint32_t blocked_count;         /**< Messages blocked */
} antibody_message_filter_t;

/**
 * @brief Protocol-driven immune modulation
 */
typedef struct {
    /* Error tracking */
    protocol_error_metrics_t errors;

    /* Immune triggers */
    bool error_triggered_inflammation;
    uint32_t violation_antigen_id;
    bool recovery_triggered_il10;

    /* Filter tracking */
    antibody_message_filter_t* filters;
    size_t filter_count;
    size_t filter_capacity;
} protocol_immune_modulation_t;

/**
 * @brief Complete protocol-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;

    /* Current state */
    cytokine_protocol_effects_t cytokine_effects;
    inflammation_protocol_state_t inflammation_state;
    protocol_immune_modulation_t protocol_modulation;

    /* Configuration */
    bool enable_error_inflammation;
    bool enable_violation_immune_response;
    bool enable_cytokine_protocol_modulation;
    bool enable_antibody_filters;

    /* Timing */
    uint64_t last_update_time;
    uint64_t last_error_check_time;

    /* Statistics */
    uint64_t total_updates;
    uint32_t error_inflammation_events;
    uint32_t violation_antigens;
    uint32_t recovery_il10_releases;
    uint32_t messages_filtered;

    } protocol_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_error_inflammation;
    bool enable_violation_immune_response;
    bool enable_cytokine_protocol_modulation;
    bool enable_antibody_filters;

    /* Sensitivity tuning */
    float error_sensitivity;            /**< Error threshold multiplier [0.5-2.0] */
    float immune_protocol_sensitivity;  /**< Immune→protocol strength [0.5-2.0] */

    /* Thresholds */
    float error_rate_threshold;         /**< Error rate for inflammation [0.001-0.5] */
    uint32_t max_filters;               /**< Max antibody filters [10-1000] */
} protocol_immune_config_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 */
int protocol_immune_default_config(protocol_immune_config_t* config);

/**
 * @brief Create protocol-immune bridge
 */
protocol_immune_bridge_t* protocol_immune_bridge_create(
    const protocol_immune_config_t* config,
    brain_immune_system_t* immune_system
);

/**
 * @brief Destroy protocol-immune bridge
 */
void protocol_immune_bridge_destroy(protocol_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Protocol API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to protocol
 *
 * WHAT: Modulate protocol behavior based on cytokine levels
 * WHY:  Immune state should influence protocol strictness
 * HOW:  Adjust timeouts, validation, filtering based on cytokines
 */
int protocol_immune_apply_cytokine_effects(protocol_immune_bridge_t* bridge);

/**
 * @brief Apply inflammation effects to protocol
 *
 * WHAT: Increase protocol strictness from inflammation
 * WHY:  Inflammation represents threat → stricter validation
 * HOW:  Enable checksums, filtering, authentication based on level
 */
int protocol_immune_apply_inflammation_effects(protocol_immune_bridge_t* bridge);

/**
 * @brief Create message filter from antibody
 *
 * WHAT: Generate protocol filter from immune antibody
 * WHY:  Antibodies represent learned countermeasures
 * HOW:  Extract pattern from antibody, create filter rule
 */
int protocol_immune_create_antibody_filter(
    protocol_immune_bridge_t* bridge,
    uint32_t antibody_id
);

/**
 * @brief Check if message matches antibody filter
 *
 * WHAT: Test message against antibody filters
 * WHY:  Block messages matching threat patterns
 * HOW:  Compare message to filter patterns
 */
bool protocol_immune_message_filtered(
    const protocol_immune_bridge_t* bridge,
    const uint8_t* message,
    size_t message_len,
    uint32_t source_node
);

/* ============================================================================
 * Protocol → Immune API
 * ============================================================================ */

/**
 * @brief Update protocol error metrics
 *
 * WHAT: Track protocol errors for immune triggering
 * WHY:  Errors indicate potential threats
 * HOW:  Accumulate error counts and rates
 */
int protocol_immune_update_error_metrics(
    protocol_immune_bridge_t* bridge,
    const protocol_error_metrics_t* metrics
);

/**
 * @brief Trigger inflammation from error rate
 *
 * WHAT: Activate immune inflammation from protocol errors
 * WHY:  High error rate indicates attack or malfunction
 * HOW:  Map error rate to inflammation level
 */
int protocol_immune_trigger_error_inflammation(protocol_immune_bridge_t* bridge);

/**
 * @brief Present protocol violation as antigen
 *
 * WHAT: Convert protocol violation to immune threat
 * WHY:  Violations are attacks on system integrity
 * HOW:  Create antigen from violation data
 */
int protocol_immune_present_violation(
    protocol_immune_bridge_t* bridge,
    uint8_t error_type,
    const uint8_t* message_data,
    size_t data_len,
    uint32_t source_node
);

/**
 * @brief Release IL-10 from error recovery
 *
 * WHAT: Trigger anti-inflammatory response from error normalization
 * WHY:  Recovery should reduce protocol strictness
 * HOW:  Release IL-10 when error rate drops
 */
int protocol_immune_release_il10_from_recovery(protocol_immune_bridge_t* bridge);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update protocol-immune bridge
 */
int protocol_immune_bridge_update(
    protocol_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

int protocol_immune_get_cytokine_effects(
    const protocol_immune_bridge_t* bridge,
    cytokine_protocol_effects_t* effects
);

int protocol_immune_get_inflammation_state(
    const protocol_immune_bridge_t* bridge,
    inflammation_protocol_state_t* state
);

bool protocol_immune_has_high_error_rate(const protocol_immune_bridge_t* bridge);

float protocol_immune_get_validation_strictness(const protocol_immune_bridge_t* bridge);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

int protocol_immune_connect_bio_async(protocol_immune_bridge_t* bridge);
int protocol_immune_disconnect_bio_async(protocol_immune_bridge_t* bridge);
bool protocol_immune_is_bio_async_connected(const protocol_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PROTOCOL_IMMUNE_BRIDGE_H */
