/**
 * @file nimcp_distributed_immune_bridge.h
 * @brief Distributed Cognition-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between brain immune system and distributed cognition
 * WHY:  Network congestion and failures map directly to immune inflammation responses;
 *       distributed systems require coordinated immune responses across nodes.
 * HOW:  Network metrics trigger immune responses; immune state modulates network behavior;
 *       inflammation → reduced broadcast rate, cytokines → priority routing changes.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → NETWORK PATHWAYS:
 * ---------------------------
 * 1. Inflammation and Network Performance:
 *    - Local inflammation → reduced local broadcast rate (energy conservation)
 *    - Regional inflammation → selective peer filtering (isolate sick nodes)
 *    - Systemic inflammation → emergency protocols (circuit breaker pattern)
 *    - Cytokine storm → network partition mode (quarantine)
 *    - Reference: Metabolic costs of immune activation parallel network overhead
 *
 * 2. Cytokine-Mediated Network Adaptation:
 *    - IL-1β → increase heartbeat frequency (detect failures faster)
 *    - IL-6 → escalate to coordinator nodes (hierarchy activation)
 *    - TNF-α → trigger peer health checks (active monitoring)
 *    - IFN-γ → enable strict message validation (quarantine mode)
 *    - IL-10 → restore normal routing (recovery phase)
 *
 * 3. Immune Memory and Network Optimization:
 *    - Learned threat patterns → blacklist/whitelist peer updates
 *    - Memory B cells → fast re-isolation of previously problematic nodes
 *    - Secondary response → immediate quarantine without consensus delay
 *
 * NETWORK → IMMUNE PATHWAYS:
 * ---------------------------
 * 1. Network Congestion as Inflammation:
 *    - Packet loss >5% → local inflammation
 *    - Latency >100ms → regional inflammation
 *    - Partition detected → systemic inflammation
 *    - Cascade failures → cytokine storm trigger
 *
 * 2. Peer Failures as Immune Threats:
 *    - Missed heartbeats → antigen presentation
 *    - Protocol violations → Byzantine detection → immune response
 *    - Corrupted messages → antibody production (filters/validators)
 *    - Reconnection success → IL-10 release (resolution)
 *
 * 3. Network Recovery as Immune Resolution:
 *    - Latency normalization → inflammation reduction
 *    - Peer restored → memory formation (trust recovery)
 *    - Full network health → immune surveillance mode
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              DISTRIBUTED COGNITION-IMMUNE BRIDGE                           ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │             IMMUNE → NETWORK PATHWAYS                               │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ INFLAMMATION │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ LOCAL    → -20% broadcast rate                                 │  ║
 * ║   │   │ REGIONAL → -50% + peer filtering                               │  ║
 * ║   │   │ SYSTEMIC → -80% + emergency mode                               │  ║
 * ║   │   │ STORM    → network partition                                   │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β → +heartbeat frequency                                   │  ║
 * ║   │   │ IL-6  → coordinator escalation                                 │  ║
 * ║   │   │ TNF-α → peer health checks                                     │  ║
 * ║   │   │ IFN-γ → strict validation                                      │  ║
 * ║   │   │ IL-10 → restore normal routing                                 │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │             NETWORK → IMMUNE PATHWAYS                               │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CONGESTION  │ → Inflammation Escalation                       │  ║
 * ║   │   │  PACKET LOSS │ → Antigen Presentation                          │  ║
 * ║   │   │  PEER FAIL   │ → Immune Response                               │  ║
 * ║   │   │  RECOVERY    │ → IL-10 Release                                 │  ║
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

#ifndef NIMCP_DISTRIBUTED_IMMUNE_BRIDGE_H
#define NIMCP_DISTRIBUTED_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "networking/distributed/nimcp_distributed_cognition.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Network congestion thresholds */
#define CONGESTION_PACKET_LOSS_LOCAL      0.05f   /**< >5% loss → local inflammation */
#define CONGESTION_PACKET_LOSS_REGIONAL   0.15f   /**< >15% loss → regional */
#define CONGESTION_PACKET_LOSS_SYSTEMIC   0.30f   /**< >30% loss → systemic */

#define CONGESTION_LATENCY_LOCAL_MS       100.0f  /**< >100ms → local inflammation */
#define CONGESTION_LATENCY_REGIONAL_MS    500.0f  /**< >500ms → regional */
#define CONGESTION_LATENCY_SYSTEMIC_MS    2000.0f /**< >2s → systemic */

/* Inflammation broadcast rate modulation */
#define INFLAMMATION_NONE_BROADCAST_FACTOR     1.0f   /**< Normal rate */
#define INFLAMMATION_LOCAL_BROADCAST_FACTOR    0.8f   /**< -20% */
#define INFLAMMATION_REGIONAL_BROADCAST_FACTOR 0.5f   /**< -50% */
#define INFLAMMATION_SYSTEMIC_BROADCAST_FACTOR 0.2f   /**< -80% */
#define INFLAMMATION_STORM_BROADCAST_FACTOR    0.05f  /**< Emergency only */

/* Cytokine network effects */
#define CYTOKINE_IL1_HEARTBEAT_MULTIPLIER  1.5f   /**< IL-1β → +50% heartbeat rate */
#define CYTOKINE_IL6_COORDINATOR_THRESHOLD 0.6f   /**< IL-6 → escalate if >0.6 */
#define CYTOKINE_TNF_HEALTH_CHECK_RATE     2.0f   /**< TNF-α → 2x health checks */
#define CYTOKINE_IFN_VALIDATION_STRICTNESS 0.9f   /**< IFN-γ → 90% validation threshold */
#define CYTOKINE_IL10_RECOVERY_BOOST       0.3f   /**< IL-10 → +30% recovery rate */

/* Peer health mapping */
#define PEER_UNHEALTHY_SEVERITY            5      /**< Unhealthy peer → severity 5 antigen */
#define PEER_BYZANTINE_SEVERITY            8      /**< Byzantine peer → severity 8 */
#define PEER_RECOVERED_MEMORY_DURATION     3600   /**< Remember recovery for 1 hour */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Network congestion metrics
 */
typedef struct {
    float packet_loss_rate;        /**< Packet loss [0-1] */
    float avg_latency_ms;          /**< Average latency (ms) */
    uint32_t timeouts;             /**< Timeout count */
    uint32_t unhealthy_peers;      /**< Number of unhealthy peers */
    bool partition_detected;       /**< Network partition detected */
} network_congestion_metrics_t;

/**
 * @brief Cytokine network effects
 */
typedef struct {
    /* Cytokine levels */
    float il1_level;               /**< IL-1β concentration */
    float il6_level;               /**< IL-6 concentration */
    float tnf_level;               /**< TNF-α concentration */
    float ifn_gamma_level;         /**< IFN-γ concentration */
    float il10_level;              /**< IL-10 concentration */

    /* Computed effects */
    float heartbeat_rate_multiplier; /**< Heartbeat frequency adjustment */
    bool coordinator_escalation;     /**< Escalate to coordinator */
    float health_check_rate_multiplier; /**< Health check frequency */
    float validation_strictness;     /**< Message validation threshold */
    float recovery_rate_boost;       /**< Recovery speed boost */
} cytokine_network_effects_t;

/**
 * @brief Inflammation network state
 */
typedef struct {
    brain_inflammation_level_t current_level;
    float broadcast_rate_factor;   /**< Broadcast rate modulation [0-1] */
    bool peer_filtering_enabled;   /**< Filter problematic peers */
    bool emergency_mode;            /**< Emergency protocols active */
    bool partition_mode;            /**< Network partition isolation */
} inflammation_network_state_t;

/**
 * @brief Network-driven immune modulation
 */
typedef struct {
    /* Network metrics */
    network_congestion_metrics_t congestion;

    /* Immune triggers */
    bool congestion_triggered_inflammation;
    uint32_t peer_failure_antigen_id;      /**< Antigen ID for peer failure */
    bool recovery_triggered_il10;

    /* Peer tracking */
    uint32_t quarantined_peer_count;
    uint32_t recovered_peer_count;
} network_immune_modulation_t;

/**
 * @brief Complete distributed-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    distrib_cognition_t distributed_cognition;

    /* Current state */
    cytokine_network_effects_t cytokine_effects;
    inflammation_network_state_t inflammation_state;
    network_immune_modulation_t network_modulation;

    /* Configuration */
    bool enable_congestion_inflammation;
    bool enable_peer_failure_immune_response;
    bool enable_cytokine_network_modulation;
    bool enable_recovery_memory;

    /* Timing */
    uint64_t last_update_time;
    uint64_t last_congestion_check_time;

    /* Statistics */
    uint64_t total_updates;
    uint32_t congestion_inflammation_events;
    uint32_t peer_failure_antigens;
    uint32_t recovery_il10_releases;
    uint32_t network_modulations;

    } distributed_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_congestion_inflammation;
    bool enable_peer_failure_immune_response;
    bool enable_cytokine_network_modulation;
    bool enable_recovery_memory;

    /* Sensitivity tuning */
    float congestion_sensitivity;      /**< Congestion threshold multiplier [0.5-2.0] */
    float immune_network_sensitivity;  /**< Immune→network effect strength [0.5-2.0] */

    /* Thresholds */
    float packet_loss_threshold;       /**< Packet loss for inflammation [0.01-0.5] */
    float latency_threshold_ms;        /**< Latency for inflammation [50-5000] */
    uint32_t peer_failure_threshold;   /**< Failed peers for systemic [1-10] */
} distributed_immune_config_t;

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
int distributed_immune_default_config(distributed_immune_config_t* config);

/**
 * @brief Create distributed-immune bridge
 *
 * WHAT: Initialize bidirectional distributed-immune integration
 * WHY:  Enable realistic network-immune coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param distributed_cognition Distributed cognition coordinator
 * @return New bridge or NULL on failure
 */
distributed_immune_bridge_t* distributed_immune_bridge_create(
    const distributed_immune_config_t* config,
    brain_immune_system_t* immune_system,
    distrib_cognition_t distributed_cognition
);

/**
 * @brief Destroy distributed-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void distributed_immune_bridge_destroy(distributed_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Network API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to network
 *
 * WHAT: Modulate network behavior based on cytokine levels
 * WHY:  Immune state should influence network performance
 * HOW:  Adjust heartbeat, validation, routing based on cytokines
 *
 * @param bridge Distributed-immune bridge
 * @return 0 on success
 */
int distributed_immune_apply_cytokine_effects(distributed_immune_bridge_t* bridge);

/**
 * @brief Apply inflammation effects to network
 *
 * WHAT: Reduce broadcast rate and enable filtering from inflammation
 * WHY:  Inflammation represents resource constraint
 * HOW:  Scale network activity based on inflammation level
 *
 * @param bridge Distributed-immune bridge
 * @return 0 on success
 */
int distributed_immune_apply_inflammation_effects(distributed_immune_bridge_t* bridge);

/**
 * @brief Compute network broadcast rate from immune state
 *
 * WHAT: Calculate broadcast rate factor given immune status
 * WHY:  Inflammation should reduce network load
 * HOW:  Map inflammation level to rate factor [0-1]
 *
 * @param bridge Distributed-immune bridge
 * @return Broadcast rate factor [0-1]
 */
float distributed_immune_compute_broadcast_rate(const distributed_immune_bridge_t* bridge);

/* ============================================================================
 * Network → Immune API
 * ============================================================================ */

/**
 * @brief Update congestion metrics
 *
 * WHAT: Sample network congestion and update metrics
 * WHY:  Detect network stress for immune triggering
 * HOW:  Query distributed cognition stats
 *
 * @param bridge Distributed-immune bridge
 * @return 0 on success
 */
int distributed_immune_update_congestion_metrics(distributed_immune_bridge_t* bridge);

/**
 * @brief Trigger inflammation from network congestion
 *
 * WHAT: Activate immune inflammation response from network stress
 * WHY:  High congestion maps to metabolic stress
 * HOW:  Map congestion metrics to inflammation level
 *
 * @param bridge Distributed-immune bridge
 * @return 0 on success
 */
int distributed_immune_trigger_congestion_inflammation(distributed_immune_bridge_t* bridge);

/**
 * @brief Present peer failure as antigen
 *
 * WHAT: Convert peer failure to immune threat
 * WHY:  Failed peers are threats to network integrity
 * HOW:  Create antigen from peer failure data
 *
 * @param bridge Distributed-immune bridge
 * @param peer_id Failed peer identifier
 * @param failure_type Type of failure (timeout, protocol, etc.)
 * @return 0 on success
 */
int distributed_immune_present_peer_failure(
    distributed_immune_bridge_t* bridge,
    uint32_t peer_id,
    uint8_t failure_type
);

/**
 * @brief Release IL-10 from network recovery
 *
 * WHAT: Trigger anti-inflammatory response from network health restoration
 * WHY:  Recovery should reduce inflammation
 * HOW:  Release IL-10 cytokine when metrics normalize
 *
 * @param bridge Distributed-immune bridge
 * @return 0 on success
 */
int distributed_immune_release_il10_from_recovery(distributed_immune_bridge_t* bridge);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update distributed-immune bridge (both directions)
 *
 * WHAT: Process all network-immune interactions
 * WHY:  Advance coupled state machine
 * HOW:  Check congestion, apply immune effects, adjust network
 *
 * @param bridge Distributed-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int distributed_immune_bridge_update(
    distributed_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine network effects
 *
 * @param bridge Distributed-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int distributed_immune_get_cytokine_effects(
    const distributed_immune_bridge_t* bridge,
    cytokine_network_effects_t* effects
);

/**
 * @brief Get current inflammation network state
 *
 * @param bridge Distributed-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int distributed_immune_get_inflammation_state(
    const distributed_immune_bridge_t* bridge,
    inflammation_network_state_t* state
);

/**
 * @brief Check if network is congested
 *
 * @param bridge Distributed-immune bridge
 * @return true if significant congestion detected
 */
bool distributed_immune_is_congested(const distributed_immune_bridge_t* bridge);

/**
 * @brief Get current broadcast rate factor
 *
 * @param bridge Distributed-immune bridge
 * @return Broadcast rate factor [0-1]
 */
float distributed_immune_get_broadcast_rate_factor(const distributed_immune_bridge_t* bridge);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_NETWORKING_DISTRIBUTED
 *
 * @param bridge Distributed-immune bridge
 * @return 0 on success, -1 on error
 */
int distributed_immune_connect_bio_async(distributed_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge Distributed-immune bridge
 * @return 0 on success
 */
int distributed_immune_disconnect_bio_async(distributed_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Distributed-immune bridge
 * @return true if connected
 */
bool distributed_immune_is_bio_async_connected(const distributed_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DISTRIBUTED_IMMUNE_BRIDGE_H */
