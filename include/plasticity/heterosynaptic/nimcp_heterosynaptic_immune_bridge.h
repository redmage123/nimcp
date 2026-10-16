/**
 * @file nimcp_heterosynaptic_immune_bridge.h
 * @brief Heterosynaptic Plasticity-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Bidirectional integration between brain immune system and heterosynaptic plasticity
 * WHY:  Inflammation affects synaptic competition, abnormal competition triggers immune response
 * HOW:  Cytokines modulate competition strength and radius, excessive competition alerts immune system
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → HETEROSYNAPTIC PATHWAYS:
 * ---------------------------------
 * 1. Pro-inflammatory Cytokines Reduce Competition:
 *    - IL-1β, IL-6, TNF-α reduce heterosynaptic depression strength
 *    - Inflammation preserves weak synapses (neuroprotective)
 *    - Prevents excessive pruning during immune activation
 *    - Reference: Stellwagen & Malenka (2006) "Cytokines regulate synaptic strength"
 *
 * 2. Inflammation Narrows Competition Radius:
 *    - Systemic inflammation reduces spatial extent of competition
 *    - Local protection of synaptic clusters
 *    - Preserves memory traces during illness
 *    - Reference: Pickering & O'Connor (2007) "Immune modulation of plasticity"
 *
 * 3. IL-10 Restores Normal Competition:
 *    - Anti-inflammatory cytokine normalizes heterosynaptic depression
 *    - Allows healthy pruning to resume
 *    - Restores winner-take-all dynamics
 *    - Reference: Rizzo et al. (2018) "IL-10 and synaptic plasticity"
 *
 * 4. Chronic Inflammation → Competition Dysfunction:
 *    - Sustained inflammation disrupts lateral inhibition
 *    - May lead to runaway potentiation without competition
 *    - Or excessive depression (synaptic failure)
 *
 * HETEROSYNAPTIC → IMMUNE PATHWAYS:
 * ---------------------------------
 * 1. Excessive Competition → Immune Alert:
 *    - Runaway heterosynaptic depression indicates dysfunction
 *    - Too much synaptic pruning threatens network integrity
 *    - Triggers protective inflammation
 *    - Severity: Depression events > 5x normal
 *
 * 2. Competition Failure → Immune Response:
 *    - No heterosynaptic depression despite potentiation
 *    - Indicates lateral inhibition breakdown
 *    - Triggers moderate immune response
 *    - Severity: Expected depression but none observed
 *
 * 3. Balanced Competition → Anti-inflammatory Signal:
 *    - Normal heterosynaptic dynamics indicate health
 *    - Promotes IL-10 release
 *    - Maintains homeostatic balance
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              HETEROSYNAPTIC-IMMUNE BRIDGE                                  ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   IMMUNE → HETEROSYNAPTIC:                                                ║
 * ║   ┌──────────────┐                                                         ║
 * ║   │ CYTOKINES    │                                                         ║
 * ║   │ IL-1β → -30% │  ──→  Competition Strength Reduction                   ║
 * ║   │ IL-6  → -25% │  ──→  Depression Factor Scaling                        ║
 * ║   │ TNF-α → -40% │  ──→  Radius Narrowing                                 ║
 * ║   │ IL-10 → +40% │  ──→  Restoration of Normal Competition                ║
 * ║   └──────────────┘                                                         ║
 * ║                                                                            ║
 * ║   INFLAMMATION LEVELS:                                                    ║
 * ║   NONE:     100% competition, 15μm radius                                 ║
 * ║   LOCAL:    90% competition, 14μm radius                                  ║
 * ║   REGIONAL: 70% competition, 12μm radius                                  ║
 * ║   SYSTEMIC: 40% competition, 8μm radius                                   ║
 * ║   STORM:    10% competition, 5μm radius                                   ║
 * ║                                                                            ║
 * ║   HETEROSYNAPTIC → IMMUNE:                                                ║
 * ║   ┌──────────────────┐                                                    ║
 * ║   │ RUNAWAY PRUNING  │ ──→ Immune Alert (Severity 8)                      ║
 * ║   │ NO COMPETITION   │ ──→ Immune Response (Severity 6)                   ║
 * ║   │ BALANCED         │ ──→ IL-10 Signal                                   ║
 * ║   └──────────────────┘                                                    ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HETEROSYNAPTIC_IMMUNE_BRIDGE_H
#define NIMCP_HETEROSYNAPTIC_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "plasticity/heterosynaptic/nimcp_heterosynaptic.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine effects on heterosynaptic competition */
#define CYTOKINE_IL1_COMPETITION_REDUCTION    0.70f   /**< IL-1β reduces competition to 70% */
#define CYTOKINE_IL6_COMPETITION_REDUCTION    0.75f   /**< IL-6 reduces competition to 75% */
#define CYTOKINE_TNF_COMPETITION_REDUCTION    0.60f   /**< TNF-α reduces competition to 60% */
#define CYTOKINE_IL10_COMPETITION_RESTORATION 1.40f   /**< IL-10 restores competition by 40% */

/* Inflammation-based competition modulation */
#define INFLAMMATION_COMPETITION_NONE         1.00f   /**< 100% competition */
#define INFLAMMATION_COMPETITION_LOCAL        0.90f   /**< 90% competition */
#define INFLAMMATION_COMPETITION_REGIONAL     0.70f   /**< 70% competition */
#define INFLAMMATION_COMPETITION_SYSTEMIC     0.40f   /**< 40% competition */
#define INFLAMMATION_COMPETITION_STORM        0.10f   /**< 10% competition */

/* Radius modulation by inflammation */
#define INFLAMMATION_RADIUS_NONE              1.00f   /**< 15μm */
#define INFLAMMATION_RADIUS_LOCAL             0.93f   /**< 14μm */
#define INFLAMMATION_RADIUS_REGIONAL          0.80f   /**< 12μm */
#define INFLAMMATION_RADIUS_SYSTEMIC          0.53f   /**< 8μm */
#define INFLAMMATION_RADIUS_STORM             0.33f   /**< 5μm */

/* Heterosynaptic dysfunction thresholds */
#define HETERO_RUNAWAY_PRUNING_THRESHOLD      5.0f    /**< Depression events > 5x normal */
#define HETERO_COMPETITION_FAILURE_THRESHOLD  0.1f    /**< Competition < 10% expected */
#define HETERO_BALANCED_DEPRESSION_MIN        0.3f    /**< Minimum for balanced */
#define HETERO_BALANCED_DEPRESSION_MAX        2.0f    /**< Maximum for balanced */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine effects on heterosynaptic parameters
 */
typedef struct {
    /* Pro-inflammatory effects */
    float il1_competition_reduction;     /**< IL-1β competition scaling */
    float il6_competition_reduction;     /**< IL-6 competition scaling */
    float tnf_competition_reduction;     /**< TNF-α competition scaling */

    /* Anti-inflammatory effects */
    float il10_competition_restoration;  /**< IL-10 restoration factor */

    /* Aggregate effects */
    float competition_factor;            /**< Combined competition scaling [0-2] */
    float depression_factor;             /**< Combined depression scaling [0-2] */
    float radius_factor;                 /**< Combined radius scaling [0-1] */
} cytokine_hetero_effects_t;

/**
 * @brief Inflammation effects on heterosynaptic plasticity
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;
    bool is_chronic;

    /* Heterosynaptic impacts */
    float competition_suppression;       /**< Competition strength reduction [0-1] */
    float radius_narrowing;              /**< Radius reduction [0-1] */
    float wta_weakening;                 /**< WTA dynamics weakening [0-1] */

    /* Dysfunction indicators */
    float pruning_excess;                /**< Excessive depression severity */
    float competition_deficit;           /**< Insufficient competition severity */
} inflammation_hetero_state_t;

/**
 * @brief Heterosynaptic instability detection
 */
typedef struct {
    /* Competition state */
    float recent_depression_rate;        /**< Recent LTD events per time */
    float expected_depression_rate;      /**< Expected LTD baseline */
    float competition_efficiency;        /**< Actual vs expected competition */

    /* Instability flags */
    bool runaway_pruning_detected;       /**< Excessive heterosynaptic LTD */
    bool competition_failure_detected;   /**< Insufficient competition */
    bool balanced_competition;           /**< Healthy dynamics */

    /* Severity */
    float instability_severity;          /**< Threat level [0-1] */
} hetero_instability_state_t;

/**
 * @brief Heterosynaptic modulation snapshot
 */
typedef struct {
    /* Current modulation factors */
    float competition_modulation;        /**< Competition multiplier [0-2] */
    float depression_modulation;         /**< Depression multiplier [0-2] */
    float radius_modulation;             /**< Radius multiplier [0-1] */

    /* Effective parameters */
    float effective_competition;
    float effective_depression;
    float effective_radius;
} hetero_modulation_state_t;

/**
 * @brief Complete heterosynaptic-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    hetero_system_t* hetero_system;

    /* Current state */
    cytokine_hetero_effects_t cytokine_effects;
    inflammation_hetero_state_t inflammation_state;
    hetero_instability_state_t instability_state;

    /* Base parameters (for restoration) */
    float base_competition;
    float base_depression;
    float base_radius;

    /* Integration flags */
    bool enable_cytokine_modulation;
    bool enable_inflammation_suppression;
    bool enable_instability_detection;
    bool enable_homeostatic_feedback;

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_modulations;
    uint32_t instability_alerts;
    uint32_t competition_restorations;

    } hetero_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_modulation;
    bool enable_inflammation_suppression;
    bool enable_instability_detection;
    bool enable_homeostatic_feedback;

    /* Sensitivity tuning */
    float cytokine_sensitivity;          /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;      /**< Inflammation effect multiplier [0.5-2.0] */
    float instability_sensitivity;       /**< Instability detection multiplier [0.5-2.0] */

    /* Base parameters */
    float base_competition;
    float base_depression;
    float base_radius;

    /* Thresholds */
    float runaway_pruning_threshold;
    float competition_failure_threshold;
} hetero_immune_config_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * WHAT: Get default configuration
 * WHY:  Easy initialization with biological defaults
 */
int hetero_immune_default_config(hetero_immune_config_t* config);

/**
 * WHAT: Create heterosynaptic-immune bridge
 * WHY:  Initialize bidirectional integration
 */
hetero_immune_bridge_t* hetero_immune_bridge_create(
    const hetero_immune_config_t* config,
    brain_immune_system_t* immune_system,
    hetero_system_t* hetero_system
);

/**
 * WHAT: Destroy heterosynaptic-immune bridge
 * WHY:  Clean up resources
 */
void hetero_immune_bridge_destroy(hetero_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Heterosynaptic API
 * ============================================================================ */

/**
 * WHAT: Apply cytokine effects to heterosynaptic parameters
 * WHY:  Cytokines modulate competition strength
 */
int hetero_immune_apply_cytokine_effects(hetero_immune_bridge_t* bridge);

/**
 * WHAT: Apply inflammation effects to heterosynaptic plasticity
 * WHY:  Chronic inflammation suppresses competition
 */
int hetero_immune_apply_inflammation_effects(hetero_immune_bridge_t* bridge);

/**
 * WHAT: Get inflammation-modulated competition strength
 * WHY:  Fever/inflammation reduces competition
 */
float hetero_immune_get_effective_competition(
    const hetero_immune_bridge_t* bridge,
    float base_competition
);

/**
 * WHAT: Get modulation state
 * WHY:  Need current modulation factors for heterosynaptic updates
 */
int hetero_immune_get_modulation_state(
    const hetero_immune_bridge_t* bridge,
    hetero_modulation_state_t* modulation
);

/**
 * WHAT: Restore heterosynaptic parameters after recovery
 * WHY:  IL-10 and resolution restore normal competition
 */
int hetero_immune_restore_competition(
    hetero_immune_bridge_t* bridge,
    float recovery_factor
);

/* ============================================================================
 * Heterosynaptic → Immune API
 * ============================================================================ */

/**
 * WHAT: Detect heterosynaptic instability
 * WHY:  Abnormal competition patterns threaten network
 */
int hetero_immune_detect_instability(hetero_immune_bridge_t* bridge);

/**
 * WHAT: Alert immune system of competition dysfunction
 * WHY:  Runaway pruning or competition failure is threat
 */
int hetero_immune_alert_instability(
    hetero_immune_bridge_t* bridge,
    uint32_t* antigen_id
);

/**
 * WHAT: Signal balanced competition to immune system
 * WHY:  Healthy heterosynaptic dynamics indicate neural health
 */
int hetero_immune_signal_balanced_competition(hetero_immune_bridge_t* bridge);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * WHAT: Update heterosynaptic-immune bridge (both directions)
 * WHY:  Advance coupled state machine
 */
int hetero_immune_bridge_update(
    hetero_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

int hetero_immune_get_cytokine_effects(
    const hetero_immune_bridge_t* bridge,
    cytokine_hetero_effects_t* effects
);

int hetero_immune_get_inflammation_state(
    const hetero_immune_bridge_t* bridge,
    inflammation_hetero_state_t* state
);

int hetero_immune_get_instability_state(
    const hetero_immune_bridge_t* bridge,
    hetero_instability_state_t* state
);

bool hetero_immune_is_competition_impaired(const hetero_immune_bridge_t* bridge);

float hetero_immune_get_competition_reduction(const hetero_immune_bridge_t* bridge);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

int hetero_immune_connect_bio_async(hetero_immune_bridge_t* bridge);
int hetero_immune_disconnect_bio_async(hetero_immune_bridge_t* bridge);
bool hetero_immune_is_bio_async_connected(const hetero_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HETEROSYNAPTIC_IMMUNE_BRIDGE_H */
