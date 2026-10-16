/**
 * @file nimcp_rate_limiter_immune_bridge.h
 * @brief Rate Limiter-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between brain immune system and rate limiting
 * WHY:  Biological immune metabolic regulation maps to rate limiting (cytokine-induced
 *       fatigue); high threat load should trigger stricter rate limits (immune overload
 *       prevention); chronic violations indicate persistent threat
 * HOW:  High inflammation → reduced rate limits (immune fatigue);
 *       rate limit violations → present as antigens (abuse detection);
 *       immune storm → emergency rate reduction (protect system resources)
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → RATE LIMITER PATHWAYS:
 * --------------------------------
 * 1. Cytokine-Induced Metabolic Fatigue:
 *    - Pro-inflammatory cytokines → reduced energy availability
 *    - IL-1β, IL-6, TNF-α → suppress cellular metabolism
 *    - Maps to: High inflammation → lower request rate limits
 *    - Reference: Analogous to sickness behavior and energy conservation
 *
 * 2. Immune System Resource Exhaustion:
 *    - Prolonged immune response → resource depletion
 *    - System prioritizes threat response over normal function
 *    - Maps to: Active threats → reduce capacity for new requests
 *    - Reference: Similar to immune system's resource allocation during infection
 *
 * 3. Inflammation-Based Throttling:
 *    - LOCAL inflammation → slight rate reduction (conserve local resources)
 *    - REGIONAL → moderate reduction (regional overload protection)
 *    - SYSTEMIC → aggressive reduction (system protection)
 *    - STORM → emergency throttling (survival mode)
 *
 * 4. Recovery and Rate Restoration:
 *    - IL-10 (anti-inflammatory) → gradual rate limit restoration
 *    - Successful threat resolution → return to normal capacity
 *    - Maps to: Post-infection recovery phase
 *
 * RATE LIMITER → IMMUNE PATHWAYS:
 * --------------------------------
 * 1. Violations as Threat Indicators:
 *    - Rate limit violation → potential DoS attack
 *    - Present violation as antigen to immune system
 *    - Repeated violations → immune memory formation
 *    - Reference: Like immune system recognizing repeated infections
 *
 * 2. Violation Pattern Recognition:
 *    - Client violation patterns → epitope signatures
 *    - Burst patterns, sustained abuse → immune classification
 *    - Penalty level → antigen severity
 *
 * 3. Coordinated Defense:
 *    - High violation rate → trigger inflammation
 *    - Blocked clients → quarantine action (killer T cells)
 *    - Repeated offenders → permanent memory (immune memory cells)
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                  RATE LIMITER-IMMUNE BRIDGE                               ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │               IMMUNE → RATE LIMITER PATHWAYS                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ INFLAMMATION │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ NONE    →1.0x│  ───────┐                                       │  ║
 * ║   │   │ LOCAL   →0.9x│         │                                       │  ║
 * ║   │   │ REGIONAL→0.7x│         ├──→ Rate Limit Modulation             │  ║
 * ║   │   │ SYSTEMIC→0.5x│         │    (Lower = More Restrictive)        │  ║
 * ║   │   │ STORM   →0.2x│         │                                       │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │      RATE LIMITER               │                             │  ║
 * ║   │   │  - RPS reduction                │                             │  ║
 * ║   │   │  - Burst size reduction         │                             │  ║
 * ║   │   │  - Stricter penalties           │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────────┐                                     │  ║
 * ║   │   │   CYTOKINE EFFECTS       │                                     │  ║
 * ║   │   │ ──────────────────────── │                                     │  ║
 * ║   │   │ IL-1β   → -15% RPS       │                                     │  ║
 * ║   │   │ IL-6    → -20% RPS       │                                     │  ║
 * ║   │   │ TNF-α   → -30% RPS       │                                     │  ║
 * ║   │   │ IL-10   → +25% RPS       │                                     │  ║
 * ║   │   └──────────────────────────┘                                     │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │             RATE LIMITER → IMMUNE PATHWAYS                          │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  VIOLATION   │ ──→ Present as Antigen                          │  ║
 * ║   │   │  DETECTED    │ ──→ Activate Immune Response                    │  ║
 * ║   │   │  (>threshold)│ ──→ Trigger Inflammation                        │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CLIENT      │ ──→ Epitope from Client Pattern                 │  ║
 * ║   │   │  PATTERN     │ ──→ Violation Signature                         │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ BLOCKED      │ ──→ Quarantine (Killer T Cells)                 │  ║
 * ║   │   │ CLIENT       │ ──→ Memory Formation                            │  ║
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

#ifndef NIMCP_RATE_LIMITER_IMMUNE_BRIDGE_H
#define NIMCP_RATE_LIMITER_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "security/nimcp_rate_limiter.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine rate limit modulation factors */
#define CYTOKINE_IL1_RPS_IMPACT      -0.15f  /**< IL-1β → reduce RPS */
#define CYTOKINE_IL6_RPS_IMPACT      -0.20f  /**< IL-6 → reduce RPS */
#define CYTOKINE_TNF_RPS_IMPACT      -0.30f  /**< TNF-α → aggressive reduction */
#define CYTOKINE_IFN_GAMMA_RPS_IMPACT -0.10f /**< IFN-γ → moderate reduction */
#define CYTOKINE_IL10_RPS_IMPACT      0.25f  /**< IL-10 → recovery boost */

/* Inflammation rate limit modulation */
#define INFLAMMATION_NONE_RPS_FACTOR     1.0f   /**< No change */
#define INFLAMMATION_LOCAL_RPS_FACTOR    0.9f   /**< -10% RPS */
#define INFLAMMATION_REGIONAL_RPS_FACTOR 0.7f   /**< -30% RPS */
#define INFLAMMATION_SYSTEMIC_RPS_FACTOR 0.5f   /**< -50% RPS */
#define INFLAMMATION_STORM_RPS_FACTOR    0.2f   /**< -80% RPS (emergency) */

/* Inflammation burst size modulation */
#define INFLAMMATION_NONE_BURST_FACTOR     1.0f   /**< No change */
#define INFLAMMATION_LOCAL_BURST_FACTOR    0.8f   /**< -20% burst */
#define INFLAMMATION_REGIONAL_BURST_FACTOR 0.6f   /**< -40% burst */
#define INFLAMMATION_SYSTEMIC_BURST_FACTOR 0.4f   /**< -60% burst */
#define INFLAMMATION_STORM_BURST_FACTOR    0.2f   /**< -80% burst */

/* Violation-to-immune mapping */
#define VIOLATION_SEVERITY_BASE           3      /**< Base severity for violations */
#define VIOLATION_SEVERITY_PER_COUNT      1      /**< +1 severity per additional violation */
#define VIOLATION_ANTIGEN_THRESHOLD       2      /**< Min violations to present antigen */
#define VIOLATION_INFLAMMATION_THRESHOLD  5      /**< Violations to trigger inflammation */

/* Client ID to epitope mapping */
#define CLIENT_ID_EPITOPE_SIZE            32     /**< Epitope size from client ID */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine effects on rate limiting
 *
 * Represents how cytokines modulate rate limits (immune fatigue)
 */
typedef struct {
    /* Pro-inflammatory effects (reduce capacity) */
    float il1_rps_reduction;            /**< IL-1β induced reduction */
    float il6_rps_reduction;            /**< IL-6 induced reduction */
    float tnf_rps_reduction;            /**< TNF-α induced reduction */
    float ifn_gamma_rps_reduction;      /**< IFN-γ induced reduction */

    /* Anti-inflammatory effects (restore capacity) */
    float il10_rps_increase;            /**< IL-10 recovery */

    /* Aggregate effects */
    float total_rps_modulation;         /**< Combined RPS change [-1, 1] */
    float total_burst_modulation;       /**< Combined burst change [-1, 1] */
    float effective_rps_factor;         /**< Actual RPS multiplier */
    float effective_burst_factor;       /**< Actual burst multiplier */
} rate_limiter_cytokine_effects_t;

/**
 * @brief Inflammation effects on rate limiting
 *
 * How systemic inflammation affects rate limiting behavior
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;    /**< How long inflamed */
    bool is_chronic;                    /**< Chronic inflammation flag */

    /* Rate limit modulation */
    float rps_factor;                   /**< RPS multiplier */
    float burst_factor;                 /**< Burst size multiplier */
    float penalty_severity_boost;       /**< Harsher penalties [0-1] */

    /* Mode flags */
    bool emergency_mode;                /**< Storm-level restrictions */
    bool resource_conservation_mode;    /**< Conserving resources */
} rate_limiter_inflammation_state_t;

/**
 * @brief Rate limiter immune modulation
 *
 * How rate limit violations affect immune system
 */
typedef struct {
    /* Violation tracking */
    uint32_t recent_violations;         /**< Recent violation count */
    uint32_t blocked_clients;           /**< Currently blocked clients */
    uint32_t total_violations;          /**< Total violations seen */
    uint64_t last_violation_time;       /**< Last violation timestamp */

    /* Immune effects */
    uint32_t antigens_presented;        /**< Violations presented as antigens */
    uint32_t inflammation_triggers;     /**< Times inflammation triggered */
    uint32_t quarantine_actions;        /**< Client blocks mapped to quarantine */

    /* Pattern tracking */
    uint32_t unique_violators;          /**< Unique clients violating */
    float violation_severity_avg;       /**< Average severity */
} rate_limiter_immune_modulation_t;

/**
 * @brief Rate limiter immune bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_rps_modulation;
    bool enable_inflammation_throttling;
    bool enable_violation_antigen_presentation;
    bool enable_blocked_client_quarantine;
    bool enable_auto_inflammation_trigger;

    /* Rate modulation config */
    float base_rps;                     /**< Base requests per second */
    float base_burst_size;              /**< Base burst size */
    float min_rps_factor;               /**< Min RPS multiplier (protection) */
    float min_burst_factor;             /**< Min burst multiplier */

    /* Violation mapping config */
    uint32_t min_violations_for_antigen; /**< Min violations to present */
    uint32_t violations_for_inflammation; /**< Violations to trigger inflammation */
    float violation_severity_multiplier; /**< Violation count → severity */

    /* Auto-response */
    bool auto_quarantine_on_block;      /**< Auto-quarantine blocked clients */
    bool auto_memory_on_repeated_violations; /**< Form memory for repeat offenders */
} rate_limiter_immune_config_t;

/**
 * @brief Complete rate limiter-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    nimcp_rate_limiter_t rate_limiter;

    /* Configuration */
    rate_limiter_immune_config_t config;

    /* Current state */
    rate_limiter_cytokine_effects_t cytokine_effects;
    rate_limiter_inflammation_state_t inflammation_state;
    rate_limiter_immune_modulation_t immune_modulation;

    /* Timing */
    uint64_t last_update_time;

    /* Statistics */
    uint64_t total_updates;
    uint32_t rps_modulations;
    uint32_t antigens_presented;
    uint32_t quarantine_actions;

    nimcp_platform_mutex_t* mutex;
} rate_limiter_immune_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization
 * HOW:  Return struct with balanced parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int rate_limiter_immune_default_config(rate_limiter_immune_config_t* config);

/**
 * @brief Create rate limiter-immune bridge
 *
 * WHAT: Initialize bridge between rate limiter and immune system
 * WHY:  Enable bidirectional integration
 * HOW:  Allocate state, connect modules, register callbacks
 *
 * @param config Configuration (NULL for defaults)
 * @param rate_limiter Rate limiter handle
 * @param immune_system Immune system handle
 * @return New bridge or NULL on failure
 */
rate_limiter_immune_bridge_t* rate_limiter_immune_create(
    const rate_limiter_immune_config_t* config,
    nimcp_rate_limiter_t rate_limiter,
    brain_immune_system_t* immune_system
);

/**
 * @brief Destroy rate limiter-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect modules, free memory
 *
 * @param bridge Bridge to destroy
 */
void rate_limiter_immune_destroy(rate_limiter_immune_bridge_t* bridge);

/* ============================================================================
 * Update and Modulation API
 * ============================================================================ */

/**
 * @brief Update bridge state
 *
 * WHAT: Process immune state, update rate limits
 * WHY:  Keep rate limits in sync with immune state
 * HOW:  Read cytokines/inflammation, modulate RPS/burst
 *
 * @param bridge Bridge handle
 * @return 0 on success
 */
int rate_limiter_immune_update(rate_limiter_immune_bridge_t* bridge);

/**
 * @brief Apply rate limit modulation
 *
 * WHAT: Adjust rate limits based on immune state
 * WHY:  Implement inflammation-induced throttling
 * HOW:  Calculate modulated RPS/burst, update rate limiter config
 *
 * @param bridge Bridge handle
 * @return 0 on success
 */
int rate_limiter_immune_apply_modulation(rate_limiter_immune_bridge_t* bridge);

/**
 * @brief Present rate limit violation as immune antigen
 *
 * WHAT: Convert violation to immune antigen
 * WHY:  Enable immune response to abuse
 * HOW:  Map client pattern to epitope, present to immune system
 *
 * @param bridge Bridge handle
 * @param client_id Client that violated
 * @param violation_count Number of violations
 * @param antigen_id Output: assigned antigen ID
 * @return 0 on success
 */
int rate_limiter_immune_present_violation(
    rate_limiter_immune_bridge_t* bridge,
    const char* client_id,
    uint32_t violation_count,
    uint32_t* antigen_id
);

/**
 * @brief Map blocked client to immune quarantine
 *
 * WHAT: Treat client block as immune quarantine action
 * WHY:  Coordinate rate limiting with immune killer T cells
 * HOW:  Activate killer T cell, track in immune system
 *
 * @param bridge Bridge handle
 * @param client_id Blocked client
 * @return 0 on success
 */
int rate_limiter_immune_quarantine_client(
    rate_limiter_immune_bridge_t* bridge,
    const char* client_id
);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async messaging
 * WHY:  Enable inter-module communication
 * HOW:  Register module, set up handlers
 *
 * @param bridge Bridge handle
 * @return 0 on success
 */
int rate_limiter_immune_connect_bio_async(rate_limiter_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister from bio-async
 * WHY:  Clean shutdown
 * HOW:  Unregister module
 *
 * @param bridge Bridge handle
 * @return 0 on success
 */
int rate_limiter_immune_disconnect_bio_async(rate_limiter_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge handle
 * @return true if connected
 */
bool rate_limiter_immune_is_bio_async_connected(const rate_limiter_immune_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current effective RPS
 *
 * WHAT: Return current RPS after immune modulation
 * WHY:  Monitor rate limit capacity
 * HOW:  Return computed effective RPS
 *
 * @param bridge Bridge handle
 * @return Effective RPS (requests per second)
 */
float rate_limiter_immune_get_effective_rps(const rate_limiter_immune_bridge_t* bridge);

/**
 * @brief Get current effective burst size
 *
 * @param bridge Bridge handle
 * @return Effective burst size
 */
uint32_t rate_limiter_immune_get_effective_burst(const rate_limiter_immune_bridge_t* bridge);

/**
 * @brief Check if in emergency mode
 *
 * @param bridge Bridge handle
 * @return true if emergency throttling active
 */
bool rate_limiter_immune_is_emergency_mode(const rate_limiter_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RATE_LIMITER_IMMUNE_BRIDGE_H */
