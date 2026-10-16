/**
 * @file nimcp_global_workspace_immune.h
 * @brief Global Workspace - Brain Immune System Integration
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Integration layer connecting Global Workspace Theory with Brain Immune System
 * WHY:  Inflammation disrupts conscious integration; anomalous broadcasts indicate threats
 * HOW:  Bidirectional modulation:
 *       - Immune → GW: Inflammation affects broadcast threshold and integration capacity
 *       - GW → Immune: Anomalous broadcast patterns trigger immune response
 *
 * BIOLOGICAL BASIS:
 * ```
 * INFLAMMATION EFFECT ON CONSCIOUSNESS:
 * ─────────────────────────────────────────────────────────────────────
 * Normal (None)         → Standard GW operation, normal threshold
 * Local (Mild)          → Slightly increased threshold (+10%), reduced capacity
 * Regional (Moderate)   → Significantly increased threshold (+30%), fragmented broadcasts
 * Systemic (Severe)     → Greatly increased threshold (+50%), severely limited access
 * Storm (Critical)      → Near-complete disruption (+80%), minimal conscious access
 *
 * CLINICAL CORRELATES:
 * - Delirium: Systemic inflammation → disrupted consciousness (ICU patients)
 * - Brain fog: Regional inflammation → reduced cognitive integration (COVID, autoimmune)
 * - Confusion: Storm-level → complete consciousness disruption (sepsis)
 *
 * BROADCAST ANOMALIES AS IMMUNE TRIGGERS:
 * - Rapid switching (< refractory period): Possible seizure-like activity
 * - Extreme strength oscillations: Instability, potential mania/psychosis
 * - Unexpected module dominance: Module hijacking, cognitive intrusion
 * - Repetitive content: Rumination, obsessive patterns
 * - Null/corrupted content: Data integrity violation
 * ```
 *
 * INTEGRATION ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    GLOBAL WORKSPACE ← → IMMUNE SYSTEM                      ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   IMMUNE → GW MODULATION                            │  ║
 * ║   │                                                                     │  ║
 * ║   │  Inflammation Level    Effect on GW                                │  ║
 * ║   │  ─────────────────    ────────────                                 │  ║
 * ║   │  NONE               → Normal (threshold=0.6, capacity=100%)        │  ║
 * ║   │  LOCAL              → Mild (threshold=0.66, capacity=90%)          │  ║
 * ║   │  REGIONAL           → Moderate (threshold=0.78, capacity=70%)      │  ║
 * ║   │  SYSTEMIC           → Severe (threshold=0.90, capacity=50%)        │  ║
 * ║   │  STORM              → Critical (threshold=1.08, capacity=20%)      │  ║
 * ║   │                                                                     │  ║
 * ║   │  Mechanism: brain_immune_get_max_inflammation_level()              │  ║
 * ║   │             → gw_immune_adjust_for_inflammation()                  │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   GW → IMMUNE SIGNALING                             │  ║
 * ║   │                                                                     │  ║
 * ║   │  Anomaly Type          Immune Response                             │  ║
 * ║   │  ─────────────        ────────────────                             │  ║
 * ║   │  Rapid switching    → Present antigen (ANOMALY source)            │  ║
 * ║   │  Strength spikes    → Inflammation escalation                     │  ║
 * ║   │  Module hijacking   → Killer T activation + quarantine            │  ║
 * ║   │  Repetitive pattern → Helper T + cytokine IL-6 (acute phase)      │  ║
 * ║   │  Corrupted content  → Full immune activation + IL-1 (alert)       │  ║
 * ║   │                                                                     │  ║
 * ║   │  Mechanism: gw_immune_detect_anomalies()                           │  ║
 * ║   │             → brain_immune_present_antigen()                       │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * DESIGN PATTERNS:
 * - Observer: GW observes immune inflammation changes
 * - Strategy: Different anomaly detection strategies
 * - Mediator: Coordinates GW ↔ Immune communication
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe (same as GW and immune: single-threaded per brain)
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_GLOBAL_WORKSPACE_IMMUNE_H
#define NIMCP_GLOBAL_WORKSPACE_IMMUNE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/global_workspace/nimcp_global_workspace.h"
#include "cognitive/immune/nimcp_brain_immune.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/**
 * @brief Maximum number of broadcast anomalies tracked in history
 *
 * WHAT: How many recent anomalies to remember
 * WHY:  Detect patterns of anomalous behavior
 * VALUE: Tier-based (FULL=100, MEDIUM=50, CONSTRAINED=20, MINIMAL=5)
 */
#define GW_IMMUNE_MAX_ANOMALY_HISTORY NIMCP_ANOMALY_HISTORY_SIZE

/**
 * @brief Anomaly detection window size (broadcasts)
 *
 * WHAT: How many recent broadcasts to analyze for patterns
 * WHY:  Need temporal context for anomaly detection
 * VALUE: Tier-based (matches GW history depth)
 */
#define GW_IMMUNE_ANOMALY_WINDOW NIMCP_HISTORY_SIZE_SMALL

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Types of broadcast anomalies detected
 *
 * BIOLOGICAL BASIS:
 * Different anomalies map to different pathological states:
 * - RAPID_SWITCHING: Epileptic-like activity
 * - STRENGTH_SPIKE: Manic episode, hyper-salience
 * - MODULE_HIJACK: Intrusive thoughts, obsessions
 * - REPETITIVE: Rumination, perseveration
 * - CORRUPTED: Data integrity violation, delirium
 */
typedef enum {
    GW_ANOMALY_NONE = 0,           /**< No anomaly detected */
    GW_ANOMALY_RAPID_SWITCHING,    /**< Broadcasts switching too fast */
    GW_ANOMALY_STRENGTH_SPIKE,     /**< Sudden extreme strength change */
    GW_ANOMALY_MODULE_HIJACK,      /**< Unexpected module dominance */
    GW_ANOMALY_REPETITIVE_PATTERN, /**< Same module/content repeating */
    GW_ANOMALY_CORRUPTED_CONTENT,  /**< NaN/Inf/extreme values */
    GW_ANOMALY_COUNT
} gw_anomaly_type_t;

/**
 * @brief Anomaly severity levels
 */
typedef enum {
    GW_ANOMALY_SEVERITY_NONE = 0,  /**< No anomaly */
    GW_ANOMALY_SEVERITY_MILD,      /**< Minor deviation, monitor */
    GW_ANOMALY_SEVERITY_MODERATE,  /**< Significant deviation, alert */
    GW_ANOMALY_SEVERITY_SEVERE,    /**< Major deviation, respond */
    GW_ANOMALY_SEVERITY_CRITICAL   /**< System integrity threat */
} gw_anomaly_severity_t;

/* ============================================================================
 * Core Structures
 * ============================================================================ */

/**
 * @brief Broadcast anomaly record
 *
 * WHAT: Detected anomalous broadcast event
 * WHY:  Track anomalies for pattern detection and immune response
 */
typedef struct {
    gw_anomaly_type_t type;        /**< Anomaly type */
    gw_anomaly_severity_t severity; /**< Severity level */
    uint32_t broadcast_id;         /**< Which broadcast had anomaly */
    cognitive_module_t source_module; /**< Source module */
    uint64_t timestamp_ms;         /**< When detected */
    float anomaly_score;           /**< Quantitative anomaly score (0-1) */
    char description[128];         /**< Human-readable description */
    bool immune_triggered;         /**< Was immune system notified? */
    uint32_t antigen_id;           /**< Immune antigen ID (if triggered) */
} gw_broadcast_anomaly_t;

/**
 * @brief Anomaly detection configuration
 *
 * WHAT: Thresholds and parameters for anomaly detection
 * WHY:  Tune sensitivity of anomaly detection
 */
typedef struct {
    /* Rapid switching detection */
    float rapid_switch_threshold_ms;  /**< Min time between broadcasts (default: refractory/2) */

    /* Strength spike detection */
    float strength_spike_threshold;   /**< Min change for spike (default: 0.3) */

    /* Module hijack detection */
    float module_hijack_threshold;    /**< Max allowed dominance (default: 0.8) */
    uint32_t module_hijack_window;    /**< Window for dominance check (default: 5) */

    /* Repetitive pattern detection */
    uint32_t repetitive_count_threshold; /**< Min repeats to trigger (default: 3) */

    /* Corrupted content detection */
    float content_nan_threshold;      /**< Max NaN/Inf ratio (default: 0.01) */
    float content_extreme_threshold;  /**< Max extreme value (default: 1e6) */

    /* General */
    bool enable_anomaly_detection;    /**< Enable anomaly detection */
    bool auto_trigger_immune;         /**< Automatically trigger immune on anomaly */
} gw_immune_anomaly_config_t;

/**
 * @brief Inflammation modulation state
 *
 * WHAT: How current inflammation affects GW operation
 * WHY:  Cache computed modulation factors
 */
typedef struct {
    brain_inflammation_level_t level; /**< Current inflammation level */
    float threshold_multiplier;       /**< Ignition threshold multiplier */
    float capacity_multiplier;        /**< Effective capacity multiplier */
    uint64_t last_update_ms;          /**< When last updated */
} gw_inflammation_modulation_t;

/**
 * @brief Global workspace immune integration context
 *
 * WHAT: State for GW ↔ Immune integration
 * WHY:  Track connection, anomalies, modulation
 */
typedef struct {
    /* Integration handles */
    global_workspace_t* workspace;    /**< Connected workspace */
    brain_immune_system_t* immune;    /**< Connected immune system */
    bool connected;                   /**< Integration active */

    /* Anomaly detection */
    gw_immune_anomaly_config_t anomaly_config;
    gw_broadcast_anomaly_t anomaly_history[GW_IMMUNE_MAX_ANOMALY_HISTORY];
    uint32_t anomaly_count;           /**< Total anomalies detected */
    uint32_t anomaly_head;            /**< Circular buffer head */

    /* Inflammation modulation */
    gw_inflammation_modulation_t modulation;
    float baseline_threshold;         /**< Original threshold (before modulation) */

    /* Statistics */
    uint64_t total_broadcasts_checked; /**< Total broadcasts analyzed */
    uint64_t anomalies_detected;      /**< Total anomalies detected */
    uint64_t immune_triggers;         /**< Immune responses triggered */

    /* State */
    uint64_t last_broadcast_time_ms;  /**< Track for rapid switching */
    cognitive_module_t last_broadcast_module; /**< Track for hijacking */
    uint32_t module_streak_count;     /**< Consecutive broadcasts from same module */

} gw_immune_context_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Connect global workspace to brain immune system
 *
 * WHAT: Establish bidirectional GW ↔ Immune integration
 * WHY:  Enable inflammation modulation and anomaly-triggered immune response
 * HOW:  Create context, register callbacks, enable monitoring
 *
 * @param workspace Global workspace handle
 * @param immune Brain immune system handle
 * @return Integration context or NULL on failure
 *
 * THREAD-SAFE: No (both GW and immune are single-threaded)
 * MALLOC: Yes (context structure + buffers)
 *
 * USAGE:
 * @code
 *   global_workspace_t* gw = global_workspace_create();
 *   brain_immune_system_t* immune = brain_immune_create(NULL);
 *
 *   gw_immune_context_t* ctx = global_workspace_connect_immune(gw, immune);
 *   if (!ctx) {
 *       fprintf(stderr, "Failed to connect GW to immune system\n");
 *   }
 *
 *   // GW now modulated by inflammation, broadcasts trigger immune on anomalies
 * @endcode
 */
gw_immune_context_t* global_workspace_connect_immune(
    global_workspace_t* workspace,
    brain_immune_system_t* immune
);

/**
 * @brief Disconnect global workspace from immune system
 *
 * WHAT: Remove GW ↔ Immune integration
 * WHY:  Clean shutdown, testing, reconfiguration
 * HOW:  Unregister callbacks, restore baseline thresholds, free context
 *
 * @param context Integration context
 */
void global_workspace_disconnect_immune(gw_immune_context_t* context);

/**
 * @brief Get default anomaly detection configuration
 *
 * WHAT: Sensible default parameters for anomaly detection
 * WHY:  Easy initialization with balanced sensitivity
 * HOW:  Return struct with NIMCP standard values
 *
 * @return Default configuration
 *
 * DEFAULTS:
 * - rapid_switch_threshold_ms = REFRACTORY_PERIOD / 2 (25ms)
 * - strength_spike_threshold = 0.3 (30% change)
 * - module_hijack_threshold = 0.8 (80% dominance)
 * - repetitive_count_threshold = 3 (3 consecutive)
 * - enable_anomaly_detection = true
 * - auto_trigger_immune = true
 */
gw_immune_anomaly_config_t gw_immune_default_anomaly_config(void);

/* ============================================================================
 * Inflammation Modulation API
 * ============================================================================ */

/**
 * @brief Update GW modulation based on current inflammation
 *
 * WHAT: Query immune system inflammation, adjust GW thresholds/capacity
 * WHY:  Inflammation disrupts conscious integration (biological realism)
 * HOW:  Get max inflammation level → compute multipliers → update GW threshold
 *
 * ALGORITHM:
 * 1. Query brain_immune for max inflammation level across all sites
 * 2. Compute threshold_multiplier based on level:
 *    - NONE:     1.0   (no change)
 *    - LOCAL:    1.1   (+10% harder to access)
 *    - REGIONAL: 1.3   (+30% harder)
 *    - SYSTEMIC: 1.5   (+50% harder)
 *    - STORM:    1.8   (+80% harder, near-complete disruption)
 * 3. Compute capacity_multiplier (inverse of difficulty):
 *    - NONE:     1.0   (100% capacity)
 *    - LOCAL:    0.9   (90% capacity)
 *    - REGIONAL: 0.7   (70% capacity)
 *    - SYSTEMIC: 0.5   (50% capacity)
 *    - STORM:    0.2   (20% capacity, minimal access)
 * 4. Update GW ignition threshold: threshold = baseline × multiplier
 *
 * @param context Integration context
 * @return 0 on success, -1 on error
 *
 * FREQUENCY: Call on each broadcast or periodically (10-100ms)
 * BIOLOGICAL: Mimics how systemic inflammation causes delirium/confusion
 *
 * USAGE:
 * @code
 *   // Before each competition cycle
 *   gw_immune_update_inflammation_modulation(ctx);
 *
 *   // GW threshold now adjusted based on inflammation
 *   global_workspace_compete(gw, MODULE_WORKING_MEMORY, content, 256, 0.75);
 * @endcode
 */
int gw_immune_update_inflammation_modulation(gw_immune_context_t* context);

/**
 * @brief Get current inflammation modulation state
 *
 * WHAT: Retrieve current modulation factors
 * WHY:  Monitor how inflammation affects GW
 * HOW:  Copy current modulation struct
 *
 * @param context Integration context
 * @param modulation Output: modulation state
 * @return 0 on success, -1 on error
 */
int gw_immune_get_modulation(
    gw_immune_context_t* context,
    gw_inflammation_modulation_t* modulation
);

/**
 * @brief Manually set inflammation level (for testing)
 *
 * WHAT: Override inflammation detection with manual level
 * WHY:  Testing, simulation, diagnostic scenarios
 * HOW:  Compute modulation from specified level
 *
 * @param context Integration context
 * @param level Inflammation level to simulate
 * @return 0 on success, -1 on error
 *
 * NOTE: This bypasses immune system query. Used for testing only.
 */
int gw_immune_set_manual_inflammation(
    gw_immune_context_t* context,
    brain_inflammation_level_t level
);

/* ============================================================================
 * Anomaly Detection API
 * ============================================================================ */

/**
 * @brief Detect anomalies in current broadcast
 *
 * WHAT: Analyze broadcast for anomalous patterns
 * WHY:  Detect pathological GW states (seizure-like, hijacking, corruption)
 * HOW:  Run detection algorithms on broadcast + history
 *
 * DETECTION ALGORITHMS:
 * 1. Rapid switching: Check if time since last broadcast < threshold
 * 2. Strength spike: Compare current vs previous strength
 * 3. Module hijack: Check if same module dominates recent history
 * 4. Repetitive pattern: Check for repeated module broadcasts
 * 5. Corrupted content: Scan for NaN/Inf/extreme values
 *
 * @param context Integration context
 * @param anomalies Output array (caller allocates, at least max_anomalies size)
 * @param max_anomalies Maximum anomalies to return
 * @param actual_count Output: actual anomalies detected
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(W × D) where W=window size, D=content dim
 * FREQUENCY: Call after each broadcast
 *
 * USAGE:
 * @code
 *   gw_broadcast_anomaly_t anomalies[5];
 *   uint32_t count;
 *
 *   if (gw_immune_detect_anomalies(ctx, anomalies, 5, &count) == 0) {
 *       for (uint32_t i = 0; i < count; i++) {
 *           printf("Anomaly: %s (severity=%d, score=%.2f)\n",
 *                  anomalies[i].description,
 *                  anomalies[i].severity,
 *                  anomalies[i].anomaly_score);
 *       }
 *   }
 * @endcode
 */
int gw_immune_detect_anomalies(
    gw_immune_context_t* context,
    gw_broadcast_anomaly_t* anomalies,
    uint32_t max_anomalies,
    uint32_t* actual_count
);

/**
 * @brief Trigger immune response to broadcast anomaly
 *
 * WHAT: Convert broadcast anomaly to immune antigen presentation
 * WHY:  Pathological GW states should trigger immune defense
 * HOW:  Create epitope from anomaly signature → present to immune system
 *
 * EPITOPE CONSTRUCTION:
 * - Bytes 0-3: Anomaly type (uint32_t)
 * - Bytes 4-7: Broadcast ID (uint32_t)
 * - Bytes 8-11: Source module (uint32_t)
 * - Bytes 12-15: Anomaly score (float as bytes)
 * - Bytes 16-63: Hash of broadcast content (SHA-256 truncated)
 *
 * IMMUNE MAPPING:
 * - Severity MILD → Antigen severity 3
 * - Severity MODERATE → Antigen severity 5, IL-1 cytokine
 * - Severity SEVERE → Antigen severity 7, IL-6 cytokine, activate helper T
 * - Severity CRITICAL → Antigen severity 10, IL-6 + TNF-α, activate killer T
 *
 * @param context Integration context
 * @param anomaly Anomaly to trigger response for
 * @return 0 on success, -1 on error
 *
 * SIDE EFFECTS:
 * - Presents antigen to immune system
 * - May trigger B cell activation, antibody production
 * - May release cytokines (IL-1, IL-6, TNF-α)
 * - May escalate inflammation if severe
 *
 * USAGE:
 * @code
 *   gw_broadcast_anomaly_t anomaly;
 *   // ... detect anomaly ...
 *
 *   if (anomaly.severity >= GW_ANOMALY_SEVERITY_MODERATE) {
 *       gw_immune_trigger_response(ctx, &anomaly);
 *       // Immune system now responding to anomalous broadcast
 *   }
 * @endcode
 */
int gw_immune_trigger_response(
    gw_immune_context_t* context,
    const gw_broadcast_anomaly_t* anomaly
);

/**
 * @brief Check broadcast after competition for anomalies
 *
 * WHAT: Wrapper that checks GW broadcast + optionally triggers immune
 * WHY:  Convenient single-call for monitoring after each broadcast
 * HOW:  Detect anomalies → auto-trigger if configured
 *
 * @param context Integration context
 * @return Number of anomalies detected (0 = normal)
 *
 * TYPICAL USAGE: Call after each global_workspace_compete()
 *
 * @code
 *   // Module competes for workspace
 *   bool won = global_workspace_compete(gw, MODULE_WORKING_MEMORY, content, 256, 0.8);
 *
 *   // Check for anomalies (auto-triggers immune if configured)
 *   uint32_t anomalies = gw_immune_check_broadcast(ctx);
 *   if (anomalies > 0) {
 *       printf("Warning: %u anomalies detected in broadcast\n", anomalies);
 *   }
 * @endcode
 */
uint32_t gw_immune_check_broadcast(gw_immune_context_t* context);

/* ============================================================================
 * Query and Statistics API
 * ============================================================================ */

/**
 * @brief Get anomaly history
 *
 * WHAT: Retrieve recent anomaly records
 * WHY:  Analyze patterns of anomalous behavior
 * HOW:  Copy from circular buffer (most recent first)
 *
 * @param context Integration context
 * @param history Output array (caller allocates)
 * @param max_history Maximum entries to retrieve
 * @param actual_count Output: actual entries returned
 * @return 0 on success, -1 on error
 *
 * ORDER: Most recent first (history[0] = most recent anomaly)
 */
int gw_immune_get_anomaly_history(
    gw_immune_context_t* context,
    gw_broadcast_anomaly_t* history,
    uint32_t max_history,
    uint32_t* actual_count
);

/**
 * @brief Get integration statistics
 *
 * WHAT: Retrieve GW ↔ Immune integration metrics
 * WHY:  Monitor integration health, debug issues
 * HOW:  Return counts and rates
 *
 * @param context Integration context
 * @param total_broadcasts Output: total broadcasts checked
 * @param total_anomalies Output: total anomalies detected
 * @param total_immune_triggers Output: immune responses triggered
 * @return 0 on success, -1 on error
 */
int gw_immune_get_stats(
    gw_immune_context_t* context,
    uint64_t* total_broadcasts,
    uint64_t* total_anomalies,
    uint64_t* total_immune_triggers
);

/**
 * @brief Print integration state for debugging
 *
 * WHAT: Human-readable dump of GW ↔ Immune state
 * WHY:  Debugging, monitoring, development
 * HOW:  Print modulation, anomalies, stats to stderr
 *
 * @param context Integration context
 * @param verbose If true, print detailed anomaly history
 */
void gw_immune_print_state(gw_immune_context_t* context, bool verbose);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Convert anomaly type to string
 *
 * @param type Anomaly type
 * @return String name (e.g., "RAPID_SWITCHING")
 */
const char* gw_anomaly_type_to_string(gw_anomaly_type_t type);

/**
 * @brief Convert anomaly severity to string
 *
 * @param severity Severity level
 * @return String name (e.g., "MODERATE")
 */
const char* gw_anomaly_severity_to_string(gw_anomaly_severity_t severity);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GLOBAL_WORKSPACE_IMMUNE_H */
