/**
 * @file nimcp_core_directives_immune_bridge.h
 * @brief Core Directives-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-16
 *
 * WHAT: Bidirectional integration between core directives (ethics, safety) and brain immune system
 * WHY:  Biological evidence shows immune activation increases threat vigilance and defensive behaviors;
 *       ethical violations are neural "threats" requiring immune-like defensive responses. Essential
 *       for coordinated threat detection and harm prevention.
 * HOW:  Inflammation modulates directive strictness (tighter thresholds during immune threats);
 *       blocked harmful actions trigger immune responses as detected threats.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → DIRECTIVES PATHWAYS:
 * ---------------------------
 * 1. Pro-inflammatory Cytokines (IL-1β, IL-6, TNF-α):
 *    - Increase defensive behaviors and threat sensitivity
 *    - Lower thresholds for harm detection
 *    - Enhance risk-averse decision making
 *    - Prioritize safety over exploration
 *    - Reference: Schedlowski et al. (2014) "Neuro-immune crosstalk and the control
 *      of behavioral immune responses"
 *
 * 2. Inflammation-Induced Vigilance:
 *    - High inflammation → stricter rule enforcement
 *    - Enhanced pattern matching for threats
 *    - Faster escalation to blocking actions
 *    - Reference: Harrison et al. (2016) "Inflammation causes mood changes through
 *      alterations in subgenual cingulate activity and mesolimbic connectivity"
 *
 * 3. Anti-inflammatory Cytokines (IL-10):
 *    - Restore normal directive thresholds
 *    - Enable balanced decision making
 *    - Reduce over-vigilance
 *    - Reference: Maes et al. (1999) "Anti-inflammatory cytokines in depression"
 *
 * DIRECTIVES → IMMUNE PATHWAYS:
 * ---------------------------
 * 1. Ethical Violation Detection:
 *    - Blocked harmful actions → immune threat signals
 *    - Directive escalation → inflammation escalation
 *    - Combinatorial harm patterns → adaptive immune memory
 *    - Reference: Neural basis of moral decision-making and threat detection share
 *      overlapping circuits (vmPFC, amygdala)
 *
 * 2. Action Blocking as Immune Response:
 *    - Directive blocks → equivalent to immune neutralization
 *    - Repeat violations → memory cell formation
 *    - Cross-reactive threat recognition
 *    - Reference: Biological analogy to pathogen recognition and antibody production
 *
 * 3. Threat Severity Mapping:
 *    - Mild violations → local immune response
 *    - Severe violations → systemic immune activation
 *    - Repeated patterns → chronic inflammation state
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                CORE DIRECTIVES-IMMUNE BRIDGE                               ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              IMMUNE → DIRECTIVES PATHWAYS                           │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β → +0.3 │  ───────┐                                       │  ║
 * ║   │   │ IL-6  → +0.2 │         │                                       │  ║
 * ║   │   │ TNF-α → +0.4 │         ├──→ Stricter Directives                │  ║
 * ║   │   │              │         │    (Lower harm thresholds)            │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │     DIRECTIVE SYSTEM            │                             │  ║
 * ║   │   │  - Strictness modulation        │                             │  ║
 * ║   │   │  - Threshold adjustment         │                             │  ║
 * ║   │   │  - Escalation bias              │                             │  ║
 * ║   │   │  - Vigilance increase           │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                            ▲                                       │  ║
 * ║   │   ┌──────────────┐         │                                       │  ║
 * ║   │   │   IL-10      │         │                                       │  ║
 * ║   │   │ Anti-inflam  │  ───────┘                                       │  ║
 * ║   │   │   -0.3       │     Restore Balance                             │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────────┐                                     │  ║
 * ║   │   │   INFLAMMATION LEVEL     │                                     │  ║
 * ║   │   │ ──────────────────────── │                                     │  ║
 * ║   │   │ LOCAL    → +10% strict   │                                     │  ║
 * ║   │   │ REGIONAL → +30% strict   │                                     │  ║
 * ║   │   │ SYSTEMIC → +50% strict   │                                     │  ║
 * ║   │   │ STORM    → +80% strict   │                                     │  ║
 * ║   │   └──────────────────────────┘                                     │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              DIRECTIVES → IMMUNE PATHWAYS                           │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ BLOCKED ACT  │ ──→ Threat Detection                            │  ║
 * ║   │   │ ESCALATION   │ ──→ Inflammation Trigger                        │  ║
 * ║   │   │ COMBINATORIAL│ ──→ Memory Cell Formation                       │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ SAFE ACTIONS │ ──→ Immune Quiescence                           │  ║
 * ║   │   │ RESOLUTION   │ ──→ IL-10 Release                               │  ║
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

#ifndef NIMCP_CORE_DIRECTIVES_IMMUNE_BRIDGE_H
#define NIMCP_CORE_DIRECTIVES_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/platform/nimcp_platform_mutex.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations for core directives types */
typedef struct core_directives_system core_directives_system_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine directive impact factors */
#define CYTOKINE_IL1_DIRECTIVE_SENSITIVITY      0.3f   /**< IL-1β → +30% strictness */
#define CYTOKINE_IL6_ESCALATION_BOOST          0.2f   /**< IL-6 → +20% escalation */
#define CYTOKINE_TNF_THRESHOLD_REDUCTION       0.4f   /**< TNF-α → -40% harm threshold */
#define CYTOKINE_IFN_GAMMA_VIGILANCE_BOOST     0.25f  /**< IFN-γ → +25% vigilance */
#define CYTOKINE_IL10_TOLERANCE_INCREASE       0.3f   /**< IL-10 → +30% tolerance */

/* Inflammation directive strictness modifiers */
#define INFLAMMATION_NONE_STRICTNESS_FACTOR     1.0f   /**< No change */
#define INFLAMMATION_LOCAL_STRICTNESS_FACTOR    1.1f   /**< +10% strict */
#define INFLAMMATION_REGIONAL_STRICTNESS_FACTOR 1.3f   /**< +30% strict */
#define INFLAMMATION_SYSTEMIC_STRICTNESS_FACTOR 1.5f   /**< +50% strict */
#define INFLAMMATION_STORM_STRICTNESS_FACTOR    1.8f   /**< +80% strict */

/* Directive threat severity mapping */
#define DIRECTIVE_MILD_VIOLATION_SEVERITY      3.0f   /**< Minor harm → severity 3 */
#define DIRECTIVE_MODERATE_VIOLATION_SEVERITY  6.0f   /**< Moderate harm → severity 6 */
#define DIRECTIVE_SEVERE_VIOLATION_SEVERITY    9.0f   /**< Severe harm → severity 9 */
#define DIRECTIVE_CRITICAL_VIOLATION_SEVERITY  10.0f  /**< Critical harm → severity 10 */

/* Threshold adjustment limits */
#define DIRECTIVE_MIN_THRESHOLD_MODIFIER       0.2f   /**< Minimum threshold (80% reduction) */
#define DIRECTIVE_MAX_THRESHOLD_MODIFIER       2.0f   /**< Maximum threshold (2x increase) */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine directive effects
 *
 * Represents how cytokine levels modulate directive enforcement
 */
typedef struct {
    /* Pro-inflammatory effects */
    float il1_strictness_boost;         /**< IL-1β induced strictness increase */
    float il6_escalation_boost;         /**< IL-6 induced escalation bias */
    float tnf_threshold_reduction;      /**< TNF-α induced threshold lowering */
    float ifn_gamma_vigilance_boost;    /**< IFN-γ induced vigilance increase */

    /* Anti-inflammatory effects */
    float il10_tolerance_increase;      /**< IL-10 tolerance restoration */

    /* Aggregate effects */
    float total_strictness_modifier;    /**< Combined strictness [0.2-2.0] */
    float total_escalation_modifier;    /**< Combined escalation bias [0-2.0] */
    float total_threshold_modifier;     /**< Combined threshold adjustment [0.2-2.0] */
} cytokine_directive_effects_t;

/**
 * @brief Inflammation directive state
 *
 * How inflammation affects directive enforcement
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;    /**< How long inflamed */
    bool is_chronic;                    /**< >= threshold */

    /* Directive impacts */
    float strictness_factor;            /**< Overall strictness [1.0-1.8] */
    float harm_threshold_reduction;     /**< Lower harm detection threshold */
    float escalation_likelihood;        /**< Probability of escalation [0-1] */
    float vigilance_level;              /**< Pattern matching sensitivity [0-1] */

    /* Threat response */
    bool immune_alert_active;           /**< Whether in alert state */
    float defensive_bias;               /**< Bias toward blocking [0-1] */
} inflammation_directive_state_t;

/**
 * @brief Directive-driven immune modulation
 *
 * How directive actions affect immune system
 */
typedef struct {
    /* Directive activity */
    uint32_t blocked_actions_count;     /**< Recent blocked actions */
    uint32_t escalations_count;         /**< Recent escalations */
    float avg_harm_score;               /**< Average harm score [0-1] */

    /* Immune effects */
    bool threat_detected;               /**< Directive threat → immune activation */
    float immune_activation_level;      /**< How much to activate immune [0-1] */
    brain_cytokine_type_t cytokine_to_release; /**< Which cytokine to trigger */
    float severity_for_immune;          /**< Threat severity for immune [0-10] */

    /* Pattern memory */
    bool form_memory_cell;              /**< Create immune memory for pattern */
    uint8_t threat_pattern[BRAIN_IMMUNE_EPITOPE_SIZE]; /**< Threat signature */
    size_t threat_pattern_len;          /**< Pattern length */
} directive_immune_modulation_t;

/**
 * @brief Complete directive-immune bridge state
 *
 * Note: Uses tagged struct for forward declaration compatibility
 */
typedef struct directive_immune_bridge {

    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    core_directives_system_t* core_directives;

    /* Current state */
    cytokine_directive_effects_t cytokine_effects;
    inflammation_directive_state_t inflammation_state;
    directive_immune_modulation_t directive_modulation;

    /* Configuration */
    float il1_directive_sensitivity;    /**< IL-1 effect multiplier */
    float il6_escalation_boost;         /**< IL-6 effect multiplier */
    float tnf_block_threshold_reduction; /**< TNF threshold multiplier */
    float il10_tolerance_increase;      /**< IL-10 effect multiplier */
    bool enable_immune_modulation;      /**< Enable immune→directives */
    bool enable_directive_immune_trigger; /**< Enable directives→immune */

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_modulations;
    uint32_t directive_triggered_responses;
    uint32_t blocked_actions_reported;
    uint32_t memory_cells_formed;

    /* Thresholds (copied from config) */
    float threat_severity_threshold;    /**< Min severity to trigger immune [3.0-10.0] */
    float chronic_inflammation_duration_sec; /**< Duration for chronic state */

    nimcp_platform_mutex_t* mutex;
} directive_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_immune_modulation;      /**< Enable immune→directives */
    bool enable_directive_immune_trigger; /**< Enable directives→immune */

    /* Sensitivity tuning */
    float il1_directive_sensitivity;    /**< IL-1 effect multiplier [0.5-2.0] */
    float il6_escalation_boost;         /**< IL-6 effect multiplier [0.5-2.0] */
    float tnf_block_threshold_reduction; /**< TNF threshold multiplier [0.5-2.0] */
    float il10_tolerance_increase;      /**< IL-10 effect multiplier [0.5-2.0] */

    /* Thresholds */
    float threat_severity_threshold;    /**< Min severity to trigger immune [3.0-10.0] */
    float chronic_inflammation_duration_sec; /**< Duration for chronic state */
} directive_immune_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;
    uint32_t cytokine_modulations;
    uint32_t directive_triggered_responses;
    uint32_t blocked_actions_reported;
    uint32_t memory_cells_formed;
    float current_strictness_modifier;
    float current_threshold_modifier;
    bool immune_alert_active;
} directive_immune_stats_t;

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
int directive_immune_bridge_default_config(directive_immune_config_t* config);

/**
 * @brief Create directive-immune bridge
 *
 * WHAT: Initialize bidirectional directive-immune integration
 * WHY:  Enable realistic immune-directive coupling for threat response
 * HOW:  Allocate structure, link subsystems, initialize mutex
 *
 * @param config Configuration (NULL for defaults)
 * @param core_directives Core directives system
 * @param immune_system Brain immune system
 * @return New bridge or NULL on failure
 */
directive_immune_bridge_t* directive_immune_bridge_create(
    const directive_immune_config_t* config,
    core_directives_system_t* core_directives,
    brain_immune_system_t* immune_system
);

/**
 * @brief Destroy directive-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void directive_immune_bridge_destroy(directive_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Directives API
 * ============================================================================ */

/**
 * @brief Update cytokine effects on directives
 *
 * WHAT: Compute directive modulation from current cytokine levels
 * WHY:  Inflammation increases defensive behaviors and strictness
 * HOW:  Query immune cytokines, compute strictness/threshold modifiers
 *
 * @param bridge Directive-immune bridge
 * @return 0 on success
 */
int directive_immune_bridge_update(directive_immune_bridge_t* bridge);

/**
 * @brief Apply immune modulation to directives
 *
 * WHAT: Apply computed cytokine effects to directive enforcement
 * WHY:  Modify directive behavior based on immune state
 * HOW:  Update directive thresholds, strictness, escalation parameters
 *
 * @param bridge Directive-immune bridge
 * @return 0 on success
 */
int directive_immune_bridge_apply_modulation(directive_immune_bridge_t* bridge);

/**
 * @brief Get current cytokine directive effects
 *
 * @param bridge Directive-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int directive_immune_bridge_get_effects(
    const directive_immune_bridge_t* bridge,
    cytokine_directive_effects_t* effects
);

/* ============================================================================
 * Directives → Immune API
 * ============================================================================ */

/**
 * @brief Report threat detected by directives to immune system
 *
 * WHAT: Trigger immune response when directive detects violation
 * WHY:  Ethical violations are neural threats requiring immune activation
 * HOW:  Map violation severity to antigen presentation, trigger cytokines
 *
 * @param bridge Directive-immune bridge
 * @param threat_level Violation severity [0-10]
 * @return 0 on success
 */
int directive_immune_bridge_on_threat_detected(
    directive_immune_bridge_t* bridge,
    float threat_level
);

/**
 * @brief Report blocked action to immune system
 *
 * WHAT: Inform immune system when directive blocks harmful action
 * WHY:  Blocked actions are neutralized threats → memory formation
 * HOW:  Create threat signature, present to immune, form memory cell
 *
 * @param bridge Directive-immune bridge
 * @param action Action identifier/type
 * @param reason Blocking reason string
 * @return 0 on success
 */
int directive_immune_bridge_report_blocked_action(
    directive_immune_bridge_t* bridge,
    const char* action,
    const char* reason
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * @param bridge Directive-immune bridge
 * @param stats Output statistics structure
 * @return 0 on success
 */
int directive_immune_bridge_get_stats(
    const directive_immune_bridge_t* bridge,
    directive_immune_stats_t* stats
);

/**
 * @brief Check if immune alert is active
 *
 * @param bridge Directive-immune bridge
 * @return true if immune alert modulating directives
 */
bool directive_immune_bridge_is_alert_active(const directive_immune_bridge_t* bridge);

/**
 * @brief Get current strictness modifier
 *
 * @param bridge Directive-immune bridge
 * @return Strictness factor [1.0-1.8]
 */
float directive_immune_bridge_get_strictness(const directive_immune_bridge_t* bridge);

/**
 * @brief Get current threshold modifier
 *
 * @param bridge Directive-immune bridge
 * @return Threshold factor [0.2-2.0]
 */
float directive_immune_bridge_get_threshold_modifier(const directive_immune_bridge_t* bridge);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_CORE_DIRECTIVES
 *
 * @param bridge Directive-immune bridge
 * @return 0 on success, -1 on error
 */
int directive_immune_bridge_connect_bio_async(directive_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge Directive-immune bridge
 * @return 0 on success
 */
int directive_immune_bridge_disconnect_bio_async(directive_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Directive-immune bridge
 * @return true if connected
 */
bool directive_immune_bridge_is_bio_async_connected(const directive_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CORE_DIRECTIVES_IMMUNE_BRIDGE_H */
