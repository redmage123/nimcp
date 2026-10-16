/**
 * @file nimcp_p2p_immune_bridge.h
 * @brief P2P Node-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between brain immune system and P2P networking layer
 * WHY:  Peer health failures map to immune responses; immune state modulates peer
 *       selection, reconnection strategies, and network topology adaptation.
 * HOW:  Peer failures → antigens, unhealthy peers → inflammation escalation,
 *       cytokines → connection policy changes, antibodies → peer blacklisting.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → P2P PATHWAYS:
 * ---------------------------
 * 1. Inflammation and Peer Selection:
 *    - Local inflammation → prefer healthy peers
 *    - Regional inflammation → reduce max peer count (conserve resources)
 *    - Systemic inflammation → disconnect low-trust peers
 *    - Cytokine storm → emergency peer isolation (partition recovery)
 *    - Reference: Tissue inflammation → localize immune response
 *
 * 2. Cytokine-Mediated Connection Policy:
 *    - IL-1β → increase heartbeat frequency (faster failure detection)
 *    - IL-6 → escalate reconnection priority (active repair)
 *    - TNF-α → reduce connection timeout (fail fast)
 *    - IFN-γ → quarantine unhealthy peers (isolation)
 *    - IL-10 → restore normal connection policy (recovery)
 *
 * 3. Antibody-Based Peer Filtering:
 *    - IgM antibodies → temporary peer block (probation)
 *    - IgG antibodies → permanent peer blacklist
 *    - IgE antibodies → emergency network reconfiguration
 *
 * P2P → IMMUNE PATHWAYS:
 * ---------------------------
 * 1. Peer Failures as Immune Threats:
 *    - Heartbeat timeout → antigen presentation
 *    - Connection failure → epitope formation
 *    - Byzantine behavior → high-severity immune response
 *    - Repeated failures → memory B cell formation
 *
 * 2. Network Health as Immune Modulation:
 *    - All peers healthy → immune surveillance mode
 *    - Some peers unhealthy → local inflammation
 *    - Many peers unhealthy → systemic response
 *    - Network partition → cytokine storm
 *
 * 3. Peer Recovery as Immune Resolution:
 *    - Successful reconnection → IL-10 release
 *    - Sustained health → memory formation (trust restoration)
 *    - Full network health → inflammation resolution
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                      P2P-IMMUNE BRIDGE                                     ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │               IMMUNE → P2P PATHWAYS                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ INFLAMMATION │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ LOCAL    → prefer healthy peers                                │  ║
 * ║   │   │ REGIONAL → reduce max peers                                    │  ║
 * ║   │   │ SYSTEMIC → disconnect low-trust                                │  ║
 * ║   │   │ STORM    → emergency isolation                                 │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  ANTIBODIES  │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IgM → temp peer block                                          │  ║
 * ║   │   │ IgG → permanent blacklist                                      │  ║
 * ║   │   │ IgE → network reconfiguration                                  │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │               P2P → IMMUNE PATHWAYS                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ PEER FAIL    │ → Antigen Presentation                          │  ║
 * ║   │   │ UNHEALTHY    │ → Inflammation Escalation                       │  ║
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

#ifndef NIMCP_P2P_IMMUNE_BRIDGE_H
#define NIMCP_P2P_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "networking/p2p/nimcp_p2pnode.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Peer failure severities */
#define PEER_TIMEOUT_SEVERITY           5   /**< Heartbeat timeout → severity 5 */
#define PEER_CONNECTION_FAIL_SEVERITY   6   /**< Connection fail → severity 6 */
#define PEER_BYZANTINE_SEVERITY         9   /**< Byzantine → severity 9 */

/* Inflammation peer thresholds */
#define UNHEALTHY_PEERS_LOCAL_THRESHOLD      1  /**< >=1 unhealthy → local */
#define UNHEALTHY_PEERS_REGIONAL_THRESHOLD   3  /**< >=3 unhealthy → regional */
#define UNHEALTHY_PEERS_SYSTEMIC_THRESHOLD   5  /**< >=5 unhealthy → systemic */

/* Inflammation connection limits */
#define INFLAMMATION_NONE_MAX_PEERS        32   /**< Normal: 32 peers */
#define INFLAMMATION_LOCAL_MAX_PEERS       24   /**< Local: 24 peers */
#define INFLAMMATION_REGIONAL_MAX_PEERS    16   /**< Regional: 16 peers */
#define INFLAMMATION_SYSTEMIC_MAX_PEERS    8    /**< Systemic: 8 peers */
#define INFLAMMATION_STORM_MAX_PEERS       2    /**< Storm: 2 peers (min) */

/* Cytokine connection effects */
#define CYTOKINE_IL1_HEARTBEAT_MULTIPLIER  1.5f /**< IL-1β → +50% heartbeat */
#define CYTOKINE_IL6_RECONNECT_PRIORITY    2.0f /**< IL-6 → 2x priority */
#define CYTOKINE_TNF_TIMEOUT_REDUCTION     0.5f /**< TNF-α → -50% timeout */
#define CYTOKINE_IFN_QUARANTINE_ENABLED    true /**< IFN-γ → quarantine */
#define CYTOKINE_IL10_RESTORE_NORMAL       true /**< IL-10 → restore */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Peer health metrics
 */
typedef struct {
    uint32_t total_peers;
    uint32_t healthy_peers;
    uint32_t unhealthy_peers;
    uint32_t disconnected_peers;
    uint32_t timeout_count;
    uint32_t connection_failures;
    float overall_health_ratio;      /**< Healthy/total [0-1] */
} peer_health_metrics_t;

/**
 * @brief Cytokine P2P effects
 */
typedef struct {
    /* Cytokine levels */
    float il1_level;
    float il6_level;
    float tnf_level;
    float ifn_gamma_level;
    float il10_level;

    /* Computed effects */
    float heartbeat_rate_multiplier;
    float reconnect_priority_multiplier;
    float timeout_reduction_factor;
    bool quarantine_enabled;
    bool restore_normal_policy;
} cytokine_p2p_effects_t;

/**
 * @brief Inflammation P2P state
 */
typedef struct {
    brain_inflammation_level_t current_level;
    uint32_t max_peers;              /**< Max peer count limit */
    bool prefer_healthy_peers;       /**< Prioritize healthy */
    bool disconnect_low_trust;       /**< Disconnect untrusted */
    bool emergency_isolation;        /**< Isolate node */
} inflammation_p2p_state_t;

/**
 * @brief Antibody peer filter
 */
typedef struct {
    uint32_t antibody_id;
    brain_antibody_class_t ab_class;
    uint32_t blocked_peer_id;        /**< Peer to block */
    char blocked_peer_ip[16];        /**< IP to block */
    uint16_t blocked_peer_port;      /**< Port to block */
    bool permanent;                  /**< Permanent block (IgG) */
    uint64_t expiry_time;            /**< Expiry for temp */
    uint32_t blocked_attempts;       /**< Connection attempts blocked */
} antibody_peer_filter_t;

/**
 * @brief P2P-driven immune modulation
 */
typedef struct {
    /* Health tracking */
    peer_health_metrics_t health;

    /* Immune triggers */
    bool unhealthy_triggered_inflammation;
    uint32_t peer_failure_antigen_id;
    bool recovery_triggered_il10;

    /* Filter tracking */
    antibody_peer_filter_t* filters;
    size_t filter_count;
    size_t filter_capacity;
} p2p_immune_modulation_t;

/**
 * @brief Complete P2P-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    p2p_node_t p2p_node;

    /* Current state */
    cytokine_p2p_effects_t cytokine_effects;
    inflammation_p2p_state_t inflammation_state;
    p2p_immune_modulation_t p2p_modulation;

    /* Configuration */
    bool enable_peer_failure_immune_response;
    bool enable_unhealthy_inflammation;
    bool enable_cytokine_p2p_modulation;
    bool enable_antibody_peer_filters;

    /* Timing */
    uint64_t last_update_time;
    uint64_t last_health_check_time;

    /* Statistics */
    uint64_t total_updates;
    uint32_t peer_failure_antigens;
    uint32_t unhealthy_inflammation_events;
    uint32_t recovery_il10_releases;
    uint32_t peers_filtered;

    } p2p_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    bool enable_peer_failure_immune_response;
    bool enable_unhealthy_inflammation;
    bool enable_cytokine_p2p_modulation;
    bool enable_antibody_peer_filters;

    float health_sensitivity;           /**< Health threshold multiplier [0.5-2.0] */
    float immune_p2p_sensitivity;       /**< Immune→P2P strength [0.5-2.0] */

    uint32_t unhealthy_threshold;       /**< Unhealthy peers for inflammation */
    uint32_t max_peer_filters;          /**< Max antibody filters */
} p2p_immune_config_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int p2p_immune_default_config(p2p_immune_config_t* config);

p2p_immune_bridge_t* p2p_immune_bridge_create(
    const p2p_immune_config_t* config,
    brain_immune_system_t* immune_system,
    p2p_node_t p2p_node
);

void p2p_immune_bridge_destroy(p2p_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → P2P API
 * ============================================================================ */

int p2p_immune_apply_cytokine_effects(p2p_immune_bridge_t* bridge);
int p2p_immune_apply_inflammation_effects(p2p_immune_bridge_t* bridge);
int p2p_immune_create_antibody_peer_filter(p2p_immune_bridge_t* bridge, uint32_t antibody_id);
bool p2p_immune_peer_filtered(const p2p_immune_bridge_t* bridge, uint32_t peer_id);

/* ============================================================================
 * P2P → Immune API
 * ============================================================================ */

int p2p_immune_update_health_metrics(p2p_immune_bridge_t* bridge);
int p2p_immune_trigger_unhealthy_inflammation(p2p_immune_bridge_t* bridge);
int p2p_immune_present_peer_failure(p2p_immune_bridge_t* bridge, uint32_t peer_id, uint8_t failure_type);
int p2p_immune_release_il10_from_recovery(p2p_immune_bridge_t* bridge);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

int p2p_immune_bridge_update(p2p_immune_bridge_t* bridge, uint64_t delta_ms);

/* ============================================================================
 * Query API
 * ============================================================================ */

int p2p_immune_get_cytokine_effects(const p2p_immune_bridge_t* bridge, cytokine_p2p_effects_t* effects);
int p2p_immune_get_inflammation_state(const p2p_immune_bridge_t* bridge, inflammation_p2p_state_t* state);
bool p2p_immune_has_unhealthy_peers(const p2p_immune_bridge_t* bridge);
uint32_t p2p_immune_get_max_peers(const p2p_immune_bridge_t* bridge);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

int p2p_immune_connect_bio_async(p2p_immune_bridge_t* bridge);
int p2p_immune_disconnect_bio_async(p2p_immune_bridge_t* bridge);
bool p2p_immune_is_bio_async_connected(const p2p_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_P2P_IMMUNE_BRIDGE_H */
