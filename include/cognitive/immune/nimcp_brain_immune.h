/**
 * @file nimcp_brain_immune.h
 * @brief Brain Immune System - Adaptive Defense Coordination Layer
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Coordination layer implementing biological immune concepts (B cells,
 *       T cells, antibodies, cytokines) by orchestrating existing FT and
 *       security modules.
 * WHY:  Provides unified immune-style API for threat detection and response,
 *       coordinating BBB security, BFT, swarm immune, and recovery systems.
 * HOW:  Maps biological immune concepts to existing module operations:
 *       - B cells → Swarm immune memory cells + BBB threat signatures
 *       - T cells → BFT quarantine actions + recovery triggers
 *       - Antibodies → Coordinated response strategies
 *       - Cytokines → Bio-async signaling between modules
 *       - Inflammation → Hierarchical recovery escalation
 *
 * BIOLOGICAL MODEL:
 * ```
 * BIOLOGICAL CONCEPT              NIMCP IMPLEMENTATION
 * ─────────────────────────────────────────────────────────────────
 * Antigen presentation         → BBB threat detection → immune processing
 * B cells (antibody production)→ Swarm immune memory cells + response gen
 * Helper T cells (CD4+)        → Coordination signals via bio-async
 * Killer T cells (CD8+/CTL)    → BFT quarantine + DFT node isolation
 * Antibodies                   → Swarm immune response strategies
 * Memory cells                 → Swarm immune memory + BFT trust scores
 * Cytokines                    → Bio-async messages (NOREPINEPHRINE channel)
 * Inflammation                 → Hierarchical recovery escalation
 * Resolution                   → Recovery completion + trust restoration
 * ```
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    BRAIN IMMUNE SYSTEM (Coordination Layer)                ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              ANTIGEN PRESENTATION (Threat Intake)                   │  ║
 * ║   │   ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐    │  ║
 * ║   │   │ BBB Threats  │  │ BFT Byzantine│  │ Anomaly Detection    │    │  ║
 * ║   │   │   Reports    │  │   Reports    │  │     Alerts           │    │  ║
 * ║   │   └──────┬───────┘  └──────┬───────┘  └──────────┬───────────┘    │  ║
 * ║   │          │                 │                      │                │  ║
 * ║   │          └─────────────────┼──────────────────────┘                │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │                   [Antigen Processing]                             │  ║
 * ║   │                    (Threat Analysis)                               │  ║
 * ║   └────────────────────────────┬───────────────────────────────────────┘  ║
 * ║                                │                                          ║
 * ║                                ▼                                          ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                    ADAPTIVE RESPONSE                                │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌─────────────┐    ┌─────────────┐    ┌─────────────────────┐   │  ║
 * ║   │   │   B CELLS   │    │   T CELLS   │    │     CYTOKINES       │   │  ║
 * ║   │   │ ──────────  │    │ ──────────  │    │ ─────────────────   │   │  ║
 * ║   │   │ Swarm Immune│    │ Helper:     │    │ BIO_MSG_SECURITY_*  │   │  ║
 * ║   │   │ Memory Cells│    │  Coordinate │    │ BIO_MSG_SWARM_      │   │  ║
 * ║   │   │ + Signatures│    │ Killer:     │    │  IMMUNE_ALERT       │   │  ║
 * ║   │   │             │    │  BFT Quaran │    │                     │   │  ║
 * ║   │   └──────┬──────┘    └──────┬──────┘    └──────────┬──────────┘   │  ║
 * ║   │          │                  │                      │              │  ║
 * ║   │          ▼                  ▼                      ▼              │  ║
 * ║   │   ┌─────────────────────────────────────────────────────────────┐ │  ║
 * ║   │   │                     ANTIBODIES                               │ │  ║
 * ║   │   │    Swarm Immune Response Strategies + BBB Actions            │ │  ║
 * ║   │   │    (ISOLATION, EVASION, COUNTER_ATTACK, RECONFIGURATION)     │ │  ║
 * ║   │   └─────────────────────────────────────────────────────────────┘ │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                │                                          ║
 * ║                                ▼                                          ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                    INFLAMMATION & RECOVERY                          │  ║
 * ║   │   Hierarchical Recovery (Node→Pod→Region→Global) + Resolution       │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * DESIGN PATTERNS:
 * - Facade: Unified interface over BBB, BFT, Swarm Immune, Recovery
 * - Observer: Callbacks for immune events
 * - Strategy: Pluggable response strategies
 * - Mediator: Coordinates cross-module communication
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

#ifndef NIMCP_BRAIN_IMMUNE_H
#define NIMCP_BRAIN_IMMUNE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/fault_tolerance/nimcp_byzantine_fault_tolerance.h"
#include "swarm/nimcp_swarm_immune.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "glial/microglia/nimcp_microglia.h"  /* For cytokine_type_t */

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define BRAIN_IMMUNE_MAX_ANTIGENS        256   /**< Max pending antigens */
#define BRAIN_IMMUNE_MAX_B_CELLS         512   /**< Max B cell abstractions */
#define BRAIN_IMMUNE_MAX_T_CELLS         512   /**< Max T cell abstractions */
#define BRAIN_IMMUNE_MAX_ANTIBODIES      1024  /**< Max active antibodies */
#define BRAIN_IMMUNE_MAX_CYTOKINES       128   /**< Max active cytokine signals */
#define BRAIN_IMMUNE_MAX_INFLAMMATION    64    /**< Max inflammation sites */
#define BRAIN_IMMUNE_EPITOPE_SIZE        64    /**< Threat signature size */
#define BRAIN_IMMUNE_MODULE_NAME         "brain_immune"

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct brain_immune_system brain_immune_system_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief B cell states (maps to swarm immune memory lifecycle)
 *
 * BIOLOGICAL BASIS:
 * B cells progress from naive → activated → memory/plasma states.
 * We map this to swarm immune memory cell lifecycle.
 */
typedef enum {
    B_CELL_NAIVE = 0,          /**< Unactivated, no bound antigen */
    B_CELL_ACTIVATED,          /**< Activated by antigen recognition */
    B_CELL_PLASMA,             /**< Producing antibodies (responses) */
    B_CELL_MEMORY,             /**< Long-term memory (swarm memory cell) */
    B_CELL_APOPTOTIC           /**< Marked for removal */
} brain_b_cell_state_t;

/**
 * @brief T cell types (maps to BFT and coordination actions)
 *
 * BIOLOGICAL BASIS:
 * CD4+ Helper T cells coordinate response.
 * CD8+ Killer T cells directly eliminate threats.
 */
typedef enum {
    T_CELL_NAIVE = 0,          /**< Unactivated */
    T_CELL_HELPER,             /**< Coordinates response (cytokine release) */
    T_CELL_KILLER,             /**< Direct action (BFT quarantine) */
    T_CELL_REGULATORY,         /**< Suppresses over-response */
    T_CELL_MEMORY              /**< Long-term (BFT trust history) */
} brain_t_cell_type_t;

/**
 * @brief Antibody classes (maps to response intensity)
 *
 * BIOLOGICAL BASIS:
 * IgM = first, fast but weak; IgG = mature, high affinity
 */
typedef enum {
    ANTIBODY_IGM = 0,          /**< First response, lower effectiveness */
    ANTIBODY_IGG,              /**< Mature response, high effectiveness */
    ANTIBODY_IGE               /**< Emergency/severe threat response */
} brain_antibody_class_t;

/**
 * @brief Brain-specific cytokine types (extends microglia cytokine_type_t)
 *
 * BIOLOGICAL BASIS:
 * Different cytokines trigger different immune cascades.
 * We use the microglia cytokine definitions and add IFN-gamma for antiviral response.
 *
 * NOTE: IL1, IL6, IL10, TNF-alpha already defined in glial/microglia/nimcp_microglia.h as cytokine_type_t
 */
typedef enum {
    BRAIN_CYTOKINE_IL1 = CYTOKINE_IL1B,    /**< Pro-inflammatory, alerts */
    BRAIN_CYTOKINE_IL6 = CYTOKINE_IL6,     /**< Acute phase, escalation */
    BRAIN_CYTOKINE_IL10 = CYTOKINE_IL10,   /**< Anti-inflammatory, resolution */
    BRAIN_CYTOKINE_TNF = CYTOKINE_TNFA,    /**< Severe inflammation */
    BRAIN_CYTOKINE_IFN_GAMMA = 5,          /**< Antiviral-style (quarantine) - brain-specific */
    BRAIN_CYTOKINE_IL4 = 6,                /**< Th2 driver, B-cell class switching IgM→IgG */
    BRAIN_CYTOKINE_COUNT = 7
} brain_cytokine_type_t;

/* Backward compatibility aliases for brain_cytokine_type_t */
#define CYTOKINE_TNF_ALPHA  ((brain_cytokine_type_t)BRAIN_CYTOKINE_TNF)
#define CYTOKINE_IFN_GAMMA  ((brain_cytokine_type_t)BRAIN_CYTOKINE_IFN_GAMMA)
#define CYTOKINE_IL1_BRAIN  ((brain_cytokine_type_t)BRAIN_CYTOKINE_IL1)

/**
 * @brief Antigen source types
 */
typedef enum {
    ANTIGEN_SOURCE_BBB = 0,    /**< Blood-brain barrier threat */
    ANTIGEN_SOURCE_BFT,        /**< Byzantine fault detection */
    ANTIGEN_SOURCE_ANOMALY,    /**< Behavioral anomaly */
    ANTIGEN_SOURCE_SWARM,      /**< Swarm immune detection */
    ANTIGEN_SOURCE_MANUAL      /**< Manually reported */
} brain_antigen_source_t;

/**
 * @brief Inflammation severity levels (discrete labels for logging/display)
 *
 * NOTE: These discrete levels are retained for backward compatibility in
 * logging, serialization, and display. Internal dispatch and behavior
 * should use the continuous inflammation_level float [0.0-1.0] instead.
 * Use inflammation_level_from_continuous() to derive labels from floats.
 */
typedef enum {
    INFLAMMATION_NONE = 0,     /**< No inflammation (continuous: 0.0-0.05) */
    INFLAMMATION_LOCAL,        /**< Node-level (continuous: 0.05-0.30) */
    INFLAMMATION_REGIONAL,     /**< Pod/region-level (continuous: 0.30-0.60) */
    INFLAMMATION_SYSTEMIC,     /**< System-wide (continuous: 0.60-0.85) */
    INFLAMMATION_STORM         /**< Cytokine storm (continuous: 0.85-1.0) */
} brain_inflammation_level_t;

/* ============================================================================
 * Continuous Inflammation Model
 *
 * Instead of switching on discrete enum values, inflammation effects are
 * computed from a continuous float in [0.0, 1.0]:
 *   0.0 = perfectly healthy (NONE)
 *   1.0 = cytokine storm (STORM)
 *
 * Smooth mathematical functions replace discrete switch statements:
 *   - Linear interpolation for simple scaling
 *   - Sigmoid curves for threshold-like transitions
 *   - Power curves for superlinear damage accumulation
 * ============================================================================ */

/**
 * @brief Inflammation effects computed from continuous level
 *
 * Centralized output structure for all inflammation-modulated quantities.
 * Bridges read the fields they need rather than computing their own switch.
 */
typedef struct {
    float level;                           /**< Input continuous level [0.0-1.0] */
    brain_inflammation_level_t label;      /**< Derived discrete label */

    /* Capacity/performance scaling (1.0 = unimpaired, 0.0 = fully impaired) */
    float capacity_factor;                 /**< General cognitive capacity */
    float transfer_factor;                 /**< Cross-modal transfer efficiency */
    float channel_factor;                  /**< Information channel capacity */

    /* Immune response parameters */
    float response_strength;               /**< Immune response intensity [0.0-1.0] */
    float recovery_rate;                   /**< Healing speed (1.0 = normal) */
    float damage_accumulation;             /**< Tissue damage rate [0.0-1.0] */
    float resource_demand;                 /**< Resource allocation needed [0.0-1.0] */

    /* Cytokine model */
    float pro_inflammatory_drive;          /**< Pro-inflammatory cytokine push [0.0-1.0] */
    float anti_inflammatory_drive;         /**< Anti-inflammatory suppression [0.0-1.0] */
} inflammation_effects_t;

/**
 * @brief Derive discrete label from continuous inflammation level
 *
 * @param level Continuous inflammation [0.0-1.0]
 * @return Corresponding discrete label for logging/display
 */
static inline brain_inflammation_level_t inflammation_level_from_continuous(float level) {
    if (level < 0.05f) return INFLAMMATION_NONE;
    if (level < 0.30f) return INFLAMMATION_LOCAL;
    if (level < 0.60f) return INFLAMMATION_REGIONAL;
    if (level < 0.85f) return INFLAMMATION_SYSTEMIC;
    return INFLAMMATION_STORM;
}

/**
 * @brief Convert discrete inflammation label to continuous midpoint
 *
 * @param label Discrete inflammation level
 * @return Representative continuous value in [0.0-1.0]
 */
static inline float inflammation_level_to_continuous(brain_inflammation_level_t label) {
    switch (label) {
        case INFLAMMATION_NONE:     return 0.0f;
        case INFLAMMATION_LOCAL:    return 0.175f;
        case INFLAMMATION_REGIONAL: return 0.45f;
        case INFLAMMATION_SYSTEMIC: return 0.725f;
        case INFLAMMATION_STORM:    return 0.925f;
        default:                    return 0.0f;
    }
}

/**
 * @brief Compute a capacity factor from continuous inflammation level
 *
 * Maps inflammation [0.0-1.0] to a capacity factor using smooth interpolation.
 * At level=0.0 returns value_at_none; at level=1.0 returns value_at_storm.
 * Uses a power curve (exponent 1.5) for biologically plausible superlinear drop.
 *
 * @param level       Continuous inflammation [0.0-1.0]
 * @param at_none     Factor value when level=0.0 (typically 1.0)
 * @param at_storm    Factor value when level=1.0 (e.g. 0.2 for 80% impairment)
 * @return Smoothly interpolated factor
 */
static inline float inflammation_compute_factor(float level, float at_none, float at_storm) {
    if (level <= 0.0f) return at_none;
    if (level >= 1.0f) return at_storm;
    /* Superlinear power curve: mild inflammation has less impact than severe */
    float t = level * level * level;  /* cubic for sharper onset at high levels */
    t = t * 0.7f + level * level * 0.3f;  /* blend cubic + quadratic */
    return at_none + (at_storm - at_none) * t;
}

/**
 * @brief Compute full inflammation effects from continuous level
 *
 * @param level  Continuous inflammation [0.0-1.0]
 * @param out    Output effects structure (must not be NULL)
 * @return 0 on success, -1 on error
 */
static inline int inflammation_compute_effects(float level, inflammation_effects_t* out) {
    if (!out) return -1;

    /* Clamp to valid range */
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;

    out->level = level;
    out->label = inflammation_level_from_continuous(level);

    /* Capacity factors: 1.0 at health, reduced at high inflammation */
    out->capacity_factor  = inflammation_compute_factor(level, 1.0f, 0.2f);
    out->transfer_factor  = inflammation_compute_factor(level, 1.0f, 0.05f);
    out->channel_factor   = inflammation_compute_factor(level, 1.0f, 0.1f);

    /* Immune response: sigmoid activation centered at 0.3 */
    float sigmoid_arg = 10.0f * (level - 0.3f);
    /* Approximate sigmoid to avoid expf dependency in inline */
    float abs_arg = sigmoid_arg < 0 ? -sigmoid_arg : sigmoid_arg;
    float approx_sigmoid = 0.5f + 0.5f * sigmoid_arg / (1.0f + abs_arg);
    out->response_strength = approx_sigmoid;

    /* Recovery rate: slows as inflammation increases */
    out->recovery_rate = inflammation_compute_factor(level, 1.0f, 0.15f);

    /* Damage: superlinear accumulation */
    out->damage_accumulation = level * level;  /* quadratic */

    /* Resource demand: linear ramp */
    out->resource_demand = level;

    /* Cytokine drives */
    out->pro_inflammatory_drive = level;
    /* Anti-inflammatory peaks at moderate levels (bell curve) */
    out->anti_inflammatory_drive = 4.0f * level * (1.0f - level);  /* parabola max at 0.5 */

    return 0;
}

/**
 * @brief Immune system phase
 */
typedef enum {
    IMMUNE_PHASE_SURVEILLANCE = 0,  /**< Normal monitoring */
    IMMUNE_PHASE_RECOGNITION,       /**< Antigen detected */
    IMMUNE_PHASE_ACTIVATION,        /**< Immune response activating */
    IMMUNE_PHASE_EFFECTOR,          /**< Active response */
    IMMUNE_PHASE_RESOLUTION,        /**< Threat cleared */
    IMMUNE_PHASE_MEMORY             /**< Memory formation */
} brain_immune_phase_t;

/* ============================================================================
 * Core Structures
 * ============================================================================ */

/**
 * @brief Antigen - processed threat for immune response
 *
 * Represents a threat detected by BBB, BFT, or anomaly systems,
 * normalized for immune processing.
 */
typedef struct {
    uint32_t id;                           /**< Unique antigen ID */
    brain_antigen_source_t source;         /**< Detection source */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE]; /**< Threat signature */
    size_t epitope_len;                    /**< Signature length */

    /* Source-specific data */
    bbb_threat_type_t bbb_threat_type;     /**< BBB threat type (if source=BBB) */
    bft_behavior_t bft_behavior;           /**< BFT behavior (if source=BFT) */
    uint32_t source_node_id;               /**< Source node/entity */

    /* Severity and confidence */
    uint32_t severity;                     /**< 1-10 severity */
    float confidence;                      /**< Detection confidence (0-1) */
    float danger_signal;                   /**< Cumulative danger (0-1) */

    /* State */
    uint64_t detection_time;               /**< When first detected */
    bool processed;                        /**< Processed by T cells */
    bool neutralized;                      /**< Threat neutralized */
    uint32_t response_count;               /**< Responses generated */
} brain_antigen_t;

/**
 * @brief B Cell - antibody-producing abstraction
 *
 * Maps to swarm immune memory cells and response generation.
 */
typedef struct {
    uint32_t id;                           /**< Unique B cell ID */
    brain_b_cell_state_t state;            /**< Current state */
    uint8_t receptor[BRAIN_IMMUNE_EPITOPE_SIZE]; /**< Recognition pattern */
    size_t receptor_len;                   /**< Pattern length */
    float affinity;                        /**< Antigen binding strength (0-1) */

    uint32_t bound_antigen_id;             /**< Currently bound antigen */
    uint32_t swarm_memory_cell_id;         /**< Linked swarm memory cell */

    uint32_t antibodies_produced;          /**< Total antibodies produced */
    uint64_t activation_time;              /**< When activated */
    bool received_t_help;                  /**< Received helper T cell signal */
} brain_b_cell_t;

/**
 * @brief T Cell - coordination and killer abstraction
 *
 * Helper T cells coordinate response via cytokines.
 * Killer T cells trigger BFT quarantine and isolation.
 */
typedef struct {
    uint32_t id;                           /**< Unique T cell ID */
    brain_t_cell_type_t type;              /**< T cell type */
    uint8_t receptor[BRAIN_IMMUNE_EPITOPE_SIZE]; /**< Recognition pattern */
    size_t receptor_len;                   /**< Pattern length */

    uint32_t recognized_antigen_id;        /**< Target antigen */
    float activation_level;                /**< Activation (0-1) */

    /* Actions taken */
    uint32_t kills;                        /**< Nodes quarantined (killer) */
    uint32_t help_given;                   /**< B cells helped (helper) */
    uint32_t cytokines_released;           /**< Signals sent */

    uint64_t activation_time;              /**< When activated */
} brain_t_cell_t;

/**
 * @brief Antibody - active threat countermeasure
 *
 * Maps to swarm immune response strategies and BBB actions.
 */
typedef struct {
    uint32_t id;                           /**< Unique antibody ID */
    brain_antibody_class_t ab_class;       /**< Ig class (renamed from 'class' for C++ compat) */
    uint32_t target_antigen_id;            /**< Target antigen */
    uint32_t producer_b_cell_id;           /**< Producing B cell */

    /* Mapped response */
    NimcpSwarmResponseType swarm_response; /**< Swarm response type */
    bbb_action_t bbb_action;               /**< BBB action taken */
    uint32_t swarm_response_id;            /**< Swarm response ID */

    float effectiveness;                   /**< Effectiveness (0-1) */
    uint64_t creation_time;                /**< When created */
    bool active;                           /**< Currently active */
    uint32_t neutralizations;              /**< Successful neutralizations */
} brain_antibody_t;

/**
 * @brief Cytokine - signaling molecule abstraction
 *
 * Maps to bio-async messages for cross-module coordination.
 */
typedef struct {
    uint32_t id;                           /**< Unique cytokine ID */
    brain_cytokine_type_t type;            /**< Cytokine type */
    uint32_t source_cell_id;               /**< Producing cell */
    uint32_t target_region;                /**< Target (0=broadcast) */

    float concentration;                   /**< Signal strength (0-1) */
    bool pro_inflammatory;                 /**< Pro vs anti-inflammatory */

    /* Bio-async mapping */
    bio_message_type_t message_type;       /**< Mapped bio-message */
    uint64_t release_time;                 /**< When released */
    bool delivered;                        /**< Message sent */
} brain_cytokine_t;

/**
 * @brief Inflammation site - localized response escalation
 *
 * Maps to hierarchical recovery levels.
 */
typedef struct {
    uint32_t id;                           /**< Site ID */
    uint32_t region_id;                    /**< Affected region/node */
    brain_inflammation_level_t level;      /**< Discrete label (derived from continuous) */
    float inflammation_level;              /**< Continuous severity [0.0-1.0] */

    uint32_t triggering_antigen_id;        /**< Antigen that triggered */
    uint64_t start_time;                   /**< When started */

    /* Resource mobilization */
    float resource_allocation;             /**< Extra resources (0-1) */
    uint32_t cells_recruited;              /**< Immune cells at site */
    bool isolated;                         /**< Region isolated */

    /* Recovery mapping */
    float resolution_progress;             /**< Resolution (0-1) */
} brain_inflammation_site_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Brain immune system configuration
 */
typedef struct {
    /* Population limits */
    size_t max_antigens;                   /**< Max pending antigens */
    size_t max_b_cells;                    /**< Max B cells */
    size_t max_t_cells;                    /**< Max T cells */
    size_t max_antibodies;                 /**< Max antibodies */

    /* Timing (milliseconds) */
    uint64_t activation_delay_ms;          /**< Delay before response */
    uint64_t memory_formation_delay_ms;    /**< Delay for memory formation */
    uint64_t antibody_half_life_ms;        /**< Antibody decay time */

    /* Thresholds */
    float recognition_threshold;           /**< Antigen recognition (0-1) */
    float activation_threshold;            /**< Cell activation (0-1) */
    float inflammation_threshold;          /**< Inflammation trigger (0-1) */
    float cytokine_storm_threshold;        /**< Storm prevention (0-1) */

    /* Response tuning */
    float memory_response_multiplier;      /**< Faster secondary response */
    float helper_amplification;            /**< Helper T cell boost */

    /* Integration enables */
    bool enable_bbb_integration;           /**< Integrate with BBB */
    bool enable_bft_integration;           /**< Integrate with BFT */
    bool enable_swarm_integration;         /**< Integrate with swarm immune */
    bool enable_bio_async;                 /**< Enable bio-async messaging */
    bool enable_logging;                   /**< Enable security logging */
} brain_immune_config_t;

/**
 * @brief Immune system statistics
 */
typedef struct {
    /* Cell counts */
    uint32_t active_b_cells;
    uint32_t active_t_cells;
    uint32_t active_antibodies;
    uint32_t memory_cells;

    /* Activity */
    uint64_t antigens_processed;
    uint64_t threats_neutralized;
    uint64_t responses_generated;
    uint64_t cytokines_released;

    /* Health */
    uint32_t inflammation_sites;
    float avg_response_time_ms;
    float system_health;                   /**< Overall health (0-1) */

    /* Integration stats */
    uint64_t bbb_threats_processed;
    uint64_t bft_byzantines_handled;
    uint64_t swarm_alerts_processed;

    /* Cytokine levels (0.0-1.0 concentration) - for medulla/autonomic integration */
    float cytokine_il1;                    /**< IL-1β pro-inflammatory level */
    float cytokine_il6;                    /**< IL-6 pro-inflammatory level */
    float cytokine_il10;                   /**< IL-10 anti-inflammatory level */
    float cytokine_tnf;                    /**< TNF-α pro-inflammatory level */
    float cytokine_ifn_gamma;              /**< IFN-γ antiviral level */
    float cytokine_il4;                    /**< IL-4 Th2/B-cell class switching level */

    /* Inflammation state */
    brain_inflammation_level_t inflammation_level; /**< Current discrete label (backward compat) */
    float inflammation_level_continuous;   /**< Current continuous level [0.0-1.0] */
} brain_immune_stats_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Callback for antigen detection
 */
typedef void (*brain_immune_antigen_cb_t)(
    brain_immune_system_t* system,
    const brain_antigen_t* antigen,
    void* user_data
);

/**
 * @brief Callback for threat neutralization
 */
typedef void (*brain_immune_neutralize_cb_t)(
    brain_immune_system_t* system,
    uint32_t antigen_id,
    const brain_antibody_t* antibody,
    void* user_data
);

/**
 * @brief Callback for cytokine signaling
 */
typedef void (*brain_immune_cytokine_cb_t)(
    brain_immune_system_t* system,
    const brain_cytokine_t* cytokine,
    void* user_data
);

/**
 * @brief Callback for inflammation events
 */
typedef void (*brain_immune_inflammation_cb_t)(
    brain_immune_system_t* system,
    const brain_inflammation_site_t* site,
    void* user_data
);

/**
 * @brief Callback for killer T cell actions
 */
typedef void (*brain_immune_kill_cb_t)(
    brain_immune_system_t* system,
    const brain_t_cell_t* killer,
    uint32_t target_node_id,
    void* user_data
);

/* ============================================================================
 * Main System Structure
 * ============================================================================ */

/**
 * @brief Brain immune system state
 */
struct brain_immune_system {
    brain_immune_config_t config;          /**< Configuration */
    brain_immune_phase_t phase;            /**< Current phase */

    /* Antigen pool */
    brain_antigen_t* antigens;
    size_t antigen_count;
    size_t antigen_capacity;
    uint32_t next_antigen_id;

    /* B cells */
    brain_b_cell_t* b_cells;
    size_t b_cell_count;
    size_t b_cell_capacity;
    uint32_t next_b_cell_id;

    /* T cells */
    brain_t_cell_t* t_cells;
    size_t t_cell_count;
    size_t t_cell_capacity;
    uint32_t next_t_cell_id;

    /* Antibodies */
    brain_antibody_t* antibodies;
    size_t antibody_count;
    size_t antibody_capacity;
    uint32_t next_antibody_id;

    /* Cytokines */
    brain_cytokine_t* cytokines;
    size_t cytokine_count;
    size_t cytokine_capacity;
    uint32_t next_cytokine_id;

    /* Inflammation sites */
    brain_inflammation_site_t* inflammation_sites;
    size_t inflammation_count;
    size_t inflammation_capacity;
    uint32_t next_inflammation_id;

    /* Integration handles */
    bbb_system_t bbb_system;               /**< BBB system handle */
    bft_context_t* bft_context;            /**< BFT context */
    NimcpSwarmImmuneSystem* swarm_immune;  /**< Swarm immune system */
    bio_module_context_t bio_context;      /**< Bio-async context */

    /* Callbacks (per-callback user_data to avoid last-writer-wins) */
    brain_immune_antigen_cb_t on_antigen;
    void* on_antigen_user_data;
    brain_immune_neutralize_cb_t on_neutralize;
    void* on_neutralize_user_data;
    brain_immune_cytokine_cb_t on_cytokine;
    void* on_cytokine_user_data;
    brain_immune_inflammation_cb_t on_inflammation;
    void* on_inflammation_user_data;
    brain_immune_kill_cb_t on_kill;
    void* on_kill_user_data;
    void* callback_user_data;              /**< Legacy/shared (used by tick system) */

    /* Statistics */
    brain_immune_stats_t stats;

    /* Thread safety */
    void* mutex;                           /**< Platform mutex */

    /* State */
    bool running;                          /**< System is active */
    uint64_t start_time;                   /**< System start time */

    /* W11: optional back-reference to the parent brain for KG emission.
     * Set via brain_immune_set_brain_ref() during brain init; NULL-safe. */
    void* brain_ref;                       /**< brain_t (opaque) for W11 KG events */

    /* Phase C: Regulatory T cell system for cytokine storm damping.
     * Created in brain_immune_create(), destroyed in brain_immune_destroy().
     * Opaque void* here to avoid circular include with nimcp_regulatory_tcells.h
     * (which includes this header). Always cast to treg_system_t* internally. */
    void* tregs;
    bool treg_storm_engaged;               /**< True while pro-cyto suppression active */
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with good defaults
 * HOW:  Return struct with balanced parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int brain_immune_default_config(brain_immune_config_t* config);

/**
 * @brief Create brain immune system
 *
 * WHAT: Initialize immune coordination layer
 * WHY:  Set up unified threat response infrastructure
 * HOW:  Allocate pools, register with bio-async
 *
 * @param config Configuration (NULL for defaults)
 * @return New immune system or NULL on failure
 */
brain_immune_system_t* brain_immune_create(const brain_immune_config_t* config);

/**
 * @brief Destroy brain immune system
 *
 * WHAT: Clean up immune system resources
 * WHY:  Proper resource deallocation
 * HOW:  Free pools, unregister from bio-async
 *
 * @param system System to destroy
 */
void brain_immune_destroy(brain_immune_system_t* system);

/**
 * @brief Start immune system monitoring
 *
 * WHAT: Activate immune surveillance
 * WHY:  Begin threat detection and response
 * HOW:  Register handlers with integrated modules
 *
 * @param system Immune system
 * @return 0 on success
 */
int brain_immune_start(brain_immune_system_t* system);

/**
 * @brief Stop immune system
 *
 * WHAT: Deactivate immune system
 * WHY:  Graceful shutdown
 * HOW:  Unregister handlers, complete pending responses
 *
 * @param system Immune system
 * @return 0 on success
 */
int brain_immune_stop(brain_immune_system_t* system);

/**
 * @brief W11: set the back-reference to the parent brain for KG emission.
 *
 * Call this from brain init after both the brain and immune subsystem
 * exist. NULL-safe; leaving it unset disables W11 KG emission from the
 * immune system (everything else continues to work).
 *
 * @param system Immune system.
 * @param brain  `brain_t` (passed as void* to avoid a cyclic include).
 */
void brain_immune_set_brain_ref(brain_immune_system_t* system, void* brain);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Connect to BBB security system
 *
 * WHAT: Link immune system to BBB threat detection
 * WHY:  Receive BBB threats as antigens
 * HOW:  Register as BBB alert callback target
 *
 * @param system Immune system
 * @param bbb_system BBB system handle
 * @return 0 on success
 */
int brain_immune_connect_bbb(
    brain_immune_system_t* system,
    bbb_system_t bbb_system
);

/**
 * @brief Connect to Byzantine fault tolerance
 *
 * WHAT: Link immune system to BFT detection
 * WHY:  Receive Byzantine detections as antigens
 * HOW:  Register as BFT byzantine callback
 *
 * @param system Immune system
 * @param bft_context BFT context
 * @return 0 on success
 */
int brain_immune_connect_bft(
    brain_immune_system_t* system,
    bft_context_t* bft_context
);

/**
 * @brief Connect to swarm immune system
 *
 * WHAT: Link to swarm immune for memory cells
 * WHY:  Share threat patterns and responses
 * HOW:  Direct integration with swarm immune
 *
 * @param system Immune system
 * @param swarm_immune Swarm immune system
 * @return 0 on success
 */
int brain_immune_connect_swarm(
    brain_immune_system_t* system,
    NimcpSwarmImmuneSystem* swarm_immune
);

/**
 * @brief Connect to hierarchical recovery
 *
 * WHAT: Link immune system to hierarchical recovery
 * WHY:  Release IL-10 on successful recoveries
 * HOW:  Register completion callback
 *
 * @param system Immune system
 * @param hr_context Hierarchical recovery context
 * @return 0 on success
 */
int brain_immune_connect_hierarchical_recovery(
    brain_immune_system_t* system,
    void* hr_context
);

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Enable cytokine messaging via bio-async
 * WHY:  Distributed immune signaling
 * HOW:  Register module, set up handlers
 *
 * @param system Immune system
 * @return 0 on success
 */
int brain_immune_connect_bio_async(brain_immune_system_t* system);

/**
 * @brief Handle BFT accusation event
 *
 * WHAT: Process BFT accusation as antigen presentation
 * WHY:  Auto-present Byzantine accusations as immune threats
 * HOW:  Create antigen from accusation evidence
 *
 * @param system Immune system
 * @param accuser_id Accusing node
 * @param accused_id Accused node
 * @param behavior Byzantine behavior detected
 * @param evidence Evidence array
 * @param evidence_count Evidence count
 * @return 0 on success
 */
int brain_immune_handle_bft_accusation(
    brain_immune_system_t* system,
    uint32_t accuser_id,
    uint32_t accused_id,
    bft_behavior_t behavior,
    const bft_evidence_t* evidence,
    uint32_t evidence_count
);

/**
 * @brief Handle BFT quarantine action
 *
 * WHAT: Coordinate killer T cell response with BFT quarantine
 * WHY:  Unified threat isolation
 * HOW:  Activate killer T cell, track quarantine in immune system
 *
 * @param system Immune system
 * @param node_id Quarantined node
 * @param duration_ms Quarantine duration
 * @param trust_score Node trust score
 * @return 0 on success
 */
int brain_immune_handle_bft_quarantine(
    brain_immune_system_t* system,
    uint32_t node_id,
    uint64_t duration_ms,
    float trust_score
);

/**
 * @brief Handle BFT trust recovery
 *
 * WHAT: Form immune memory on trust restoration
 * WHY:  Map trust recovery to learned immunity
 * HOW:  Convert B cell to memory, store in swarm immune
 *
 * @param system Immune system
 * @param node_id Recovered node
 * @param old_trust Previous trust score
 * @param new_trust New trust score
 * @return 0 on success
 */
int brain_immune_handle_bft_trust_recovery(
    brain_immune_system_t* system,
    uint32_t node_id,
    float old_trust,
    float new_trust
);

/* ============================================================================
 * Enhanced Swarm Integration API
 * ============================================================================ */

/**
 * @brief Automatically sync swarm threat to brain immune antigen
 *
 * WHAT: Auto-present swarm-detected threats as brain immune antigens
 * WHY:  Ensure all swarm threats are processed by brain immune system
 * HOW:  Called automatically when swarm detects threat
 *
 * @param system Immune system
 * @param threat Swarm threat to sync
 * @return 0 on success
 */
int brain_immune_auto_sync_swarm_threat(
    brain_immune_system_t* system,
    const NimcpSwarmThreat* threat
);

/**
 * @brief Sync brain immune memory cell to swarm immune memory
 *
 * WHAT: Create swarm immune memory cell from brain immune B cell memory
 * WHY:  Share learned threat patterns across swarm
 * HOW:  Convert B cell receptor pattern to swarm threat signature
 *
 * @param system Immune system
 * @param b_cell_id B cell memory to sync
 * @return 0 on success
 */
int brain_immune_sync_memory_to_swarm(
    brain_immune_system_t* system,
    uint32_t b_cell_id
);

/**
 * @brief Trigger swarm response from brain antibody
 *
 * WHAT: Execute swarm immune response when brain antibody is activated
 * WHY:  Translate brain immune action to swarm-level coordinated response
 * HOW:  Map antibody class to swarm response type and execute
 *
 * @param system Immune system
 * @param antibody_id Antibody to trigger response from
 * @return 0 on success
 */
int brain_immune_trigger_swarm_response(
    brain_immune_system_t* system,
    uint32_t antibody_id
);

/**
 * @brief Broadcast collective inflammation state to swarm
 *
 * WHAT: Share inflammation level across swarm nodes via consensus
 * WHY:  Enable swarm-wide coordinated inflammatory response
 * HOW:  Send cytokine message with inflammation severity
 *
 * @param system Immune system
 * @param site_id Inflammation site to broadcast
 * @return 0 on success
 */
int brain_immune_broadcast_inflammation_state(
    brain_immune_system_t* system,
    uint32_t site_id
);

/**
 * @brief Request consensus on threat severity via swarm
 *
 * WHAT: Use swarm consensus to assess threat severity collectively
 * WHY:  Prevent false positives, ensure distributed agreement
 * HOW:  Each node votes on severity, weighted by confidence
 *
 * @param system Immune system
 * @param antigen_id Antigen to assess
 * @param agreed_severity_out Output: consensus severity
 * @return 0 on success
 */
int brain_immune_consensus_threat_severity(
    brain_immune_system_t* system,
    uint32_t antigen_id,
    float* agreed_severity_out
);

/**
 * @brief Propagate secondary response across swarm
 *
 * WHAT: When any node recognizes learned threat, trigger swarm-wide response
 * WHY:  Collective memory - if one node remembers, entire swarm benefits
 * HOW:  Share memory cell activation, broadcast rapid response
 *
 * @param system Immune system
 * @param memory_b_cell_id Memory B cell that recognized threat
 * @param antigen_id Antigen that was recognized
 * @return 0 on success
 */
int brain_immune_propagate_secondary_response(
    brain_immune_system_t* system,
    uint32_t memory_b_cell_id,
    uint32_t antigen_id
);

/* ============================================================================
 * Antigen Presentation API
 * ============================================================================ */

/**
 * @brief Present antigen from BBB threat
 *
 * WHAT: Convert BBB threat to immune antigen
 * WHY:  Unified threat processing
 * HOW:  Extract epitope from threat data
 *
 * @param system Immune system
 * @param threat_type BBB threat type
 * @param severity BBB severity
 * @param threat_data Threat signature data
 * @param data_len Data length
 * @param antigen_id Output: assigned antigen ID
 * @return 0 on success
 */
int brain_immune_present_bbb_threat(
    brain_immune_system_t* system,
    bbb_threat_type_t threat_type,
    bbb_severity_t severity,
    const uint8_t* threat_data,
    size_t data_len,
    uint32_t* antigen_id
);

/**
 * @brief Present antigen from BFT byzantine detection
 *
 * WHAT: Convert BFT detection to immune antigen
 * WHY:  Handle Byzantine nodes as threats
 * HOW:  Create antigen from node behavior
 *
 * @param system Immune system
 * @param node_id Byzantine node ID
 * @param behavior Detected behavior
 * @param evidence Evidence data
 * @param evidence_len Evidence length
 * @param antigen_id Output: assigned antigen ID
 * @return 0 on success
 */
int brain_immune_present_byzantine(
    brain_immune_system_t* system,
    uint32_t node_id,
    bft_behavior_t behavior,
    const bft_evidence_t* evidence,
    size_t evidence_len,
    uint32_t* antigen_id
);

/**
 * @brief Present antigen from swarm threat
 *
 * WHAT: Convert swarm threat to immune antigen
 * WHY:  Process swarm-detected threats
 * HOW:  Map swarm threat to antigen
 *
 * @param system Immune system
 * @param threat Swarm threat
 * @param antigen_id Output: assigned antigen ID
 * @return 0 on success
 */
int brain_immune_present_swarm_threat(
    brain_immune_system_t* system,
    const NimcpSwarmThreat* threat,
    uint32_t* antigen_id
);

/**
 * @brief Present generic antigen
 *
 * WHAT: Present arbitrary threat signature
 * WHY:  Handle custom/manual threat reports
 * HOW:  Create antigen from raw data
 *
 * @param system Immune system
 * @param source Antigen source
 * @param epitope Threat signature
 * @param epitope_len Signature length
 * @param severity Severity (1-10)
 * @param source_node Source node/entity ID
 * @param antigen_id Output: assigned antigen ID
 * @return 0 on success
 */
int brain_immune_present_antigen(
    brain_immune_system_t* system,
    brain_antigen_source_t source,
    const uint8_t* epitope,
    size_t epitope_len,
    uint32_t severity,
    uint32_t source_node,
    uint32_t* antigen_id
);

/* ============================================================================
 * B Cell API
 * ============================================================================ */

/**
 * @brief Activate B cell for antigen
 *
 * WHAT: Activate B cell that recognizes antigen
 * WHY:  Begin antibody production process
 * HOW:  Find/create B cell, set activated state
 *
 * @param system Immune system
 * @param antigen_id Target antigen
 * @param b_cell_id Output: activated B cell ID
 * @return 0 on success, -1 if no match
 */
int brain_immune_activate_b_cell(
    brain_immune_system_t* system,
    uint32_t antigen_id,
    uint32_t* b_cell_id
);

/**
 * @brief Convert B cell to memory
 *
 * WHAT: Transition B cell to memory state
 * WHY:  Persist learned threat pattern
 * HOW:  Create swarm memory cell, update state
 *
 * @param system Immune system
 * @param b_cell_id B cell to convert
 * @return 0 on success
 */
int brain_immune_b_cell_to_memory(
    brain_immune_system_t* system,
    uint32_t b_cell_id
);

/* ============================================================================
 * T Cell API
 * ============================================================================ */

/**
 * @brief Activate helper T cell
 *
 * WHAT: Activate helper T for antigen coordination
 * WHY:  Amplify immune response
 * HOW:  Create helper T, begin cytokine release
 *
 * @param system Immune system
 * @param antigen_id Target antigen
 * @param t_cell_id Output: activated T cell ID
 * @return 0 on success
 */
int brain_immune_activate_helper_t(
    brain_immune_system_t* system,
    uint32_t antigen_id,
    uint32_t* t_cell_id
);

/**
 * @brief Activate killer T cell
 *
 * WHAT: Activate killer T for direct action
 * WHY:  Eliminate threat via quarantine
 * HOW:  Create killer T, prepare BFT quarantine
 *
 * @param system Immune system
 * @param antigen_id Target antigen
 * @param t_cell_id Output: activated T cell ID
 * @return 0 on success
 */
int brain_immune_activate_killer_t(
    brain_immune_system_t* system,
    uint32_t antigen_id,
    uint32_t* t_cell_id
);

/**
 * @brief Execute killer T cell action
 *
 * WHAT: Quarantine/isolate threat node
 * WHY:  Direct threat elimination
 * HOW:  Trigger BFT quarantine
 *
 * @param system Immune system
 * @param t_cell_id Killer T cell
 * @param target_node Target node to quarantine
 * @return 0 on success
 */
int brain_immune_t_cell_kill(
    brain_immune_system_t* system,
    uint32_t t_cell_id,
    uint32_t target_node
);

/**
 * @brief Helper T provides help to B cell
 *
 * WHAT: Signal B cell to enhance response
 * WHY:  Amplify antibody production
 * HOW:  Set B cell flag, boost effectiveness
 *
 * @param system Immune system
 * @param helper_id Helper T cell ID
 * @param b_cell_id Target B cell ID
 * @return 0 on success
 */
int brain_immune_t_help_b(
    brain_immune_system_t* system,
    uint32_t helper_id,
    uint32_t b_cell_id
);

/* ============================================================================
 * Antibody API
 * ============================================================================ */

/**
 * @brief Produce antibody from B cell
 *
 * WHAT: Generate antibody response
 * WHY:  Create countermeasure for antigen
 * HOW:  Map to swarm immune response
 *
 * @param system Immune system
 * @param b_cell_id Producing B cell
 * @param ab_class Antibody class
 * @param antibody_id Output: new antibody ID
 * @return 0 on success
 */
int brain_immune_produce_antibody(
    brain_immune_system_t* system,
    uint32_t b_cell_id,
    brain_antibody_class_t ab_class,
    uint32_t* antibody_id
);

/**
 * @brief Execute antibody response
 *
 * WHAT: Apply antibody countermeasure
 * WHY:  Neutralize threat
 * HOW:  Execute swarm response + BBB action
 *
 * @param system Immune system
 * @param antibody_id Antibody to execute
 * @return 0 on success
 */
int brain_immune_execute_antibody(
    brain_immune_system_t* system,
    uint32_t antibody_id
);

/**
 * @brief Mark antigen as neutralized
 *
 * WHAT: Record successful neutralization
 * WHY:  Complete immune response cycle
 * HOW:  Update antigen, antibody stats
 *
 * @param system Immune system
 * @param antigen_id Neutralized antigen
 * @param antibody_id Successful antibody
 * @return 0 on success
 */
int brain_immune_neutralize(
    brain_immune_system_t* system,
    uint32_t antigen_id,
    uint32_t antibody_id
);

/* ============================================================================
 * Cytokine Signaling API
 * ============================================================================ */

/**
 * @brief Release cytokine signal
 *
 * WHAT: Send immune coordination message
 * WHY:  Cross-module immune signaling
 * HOW:  Send bio-async message
 *
 * @param system Immune system
 * @param type Cytokine type
 * @param source_cell Source cell ID
 * @param concentration Signal strength (0-1)
 * @param target_region Target region (0=broadcast)
 * @param cytokine_id Output: new cytokine ID
 * @return 0 on success
 */
int brain_immune_release_cytokine(
    brain_immune_system_t* system,
    brain_cytokine_type_t type,
    uint32_t source_cell,
    float concentration,
    uint32_t target_region,
    uint32_t* cytokine_id
);

/**
 * @brief Broadcast immune alert
 *
 * WHAT: System-wide immune alert
 * WHY:  Notify all modules of threat
 * HOW:  Bio-async broadcast message
 *
 * @param system Immune system
 * @param antigen_id Alert about this antigen
 * @param severity Alert severity
 * @return 0 on success
 */
int brain_immune_broadcast_alert(
    brain_immune_system_t* system,
    uint32_t antigen_id,
    brain_inflammation_level_t severity
);

/* ============================================================================
 * Inflammation API
 * ============================================================================ */

/**
 * @brief Initiate inflammation at region
 *
 * WHAT: Start localized immune response
 * WHY:  Mobilize resources to threat site
 * HOW:  Trigger hierarchical recovery
 *
 * @param system Immune system
 * @param region_id Affected region
 * @param antigen_id Triggering antigen
 * @param site_id Output: inflammation site ID
 * @return 0 on success
 */
int brain_immune_initiate_inflammation(
    brain_immune_system_t* system,
    uint32_t region_id,
    uint32_t antigen_id,
    uint32_t* site_id
);

/**
 * @brief Escalate inflammation level
 *
 * WHAT: Increase inflammation severity
 * WHY:  More severe threat response
 * HOW:  Escalate recovery level
 *
 * @param system Immune system
 * @param site_id Inflammation site
 * @return 0 on success
 */
int brain_immune_escalate_inflammation(
    brain_immune_system_t* system,
    uint32_t site_id
);

/**
 * @brief Resolve inflammation
 *
 * WHAT: Begin inflammation resolution
 * WHY:  Threat cleared, return to normal
 * HOW:  Release anti-inflammatory cytokines
 *
 * @param system Immune system
 * @param site_id Site to resolve
 * @return 0 on success
 */
int brain_immune_resolve_inflammation(
    brain_immune_system_t* system,
    uint32_t site_id
);

/* ============================================================================
 * Memory Response API
 * ============================================================================ */

/**
 * @brief Check for memory cell match
 *
 * WHAT: Check if antigen matches memory
 * WHY:  Faster secondary response
 * HOW:  Search swarm immune memory cells
 *
 * @param system Immune system
 * @param antigen_id Antigen to match
 * @param b_cell_id Output: matching memory B cell (if found)
 * @return 0 if found, -1 if no match
 */
int brain_immune_check_memory(
    brain_immune_system_t* system,
    uint32_t antigen_id,
    uint32_t* b_cell_id
);

/**
 * @brief Trigger secondary response
 *
 * WHAT: Fast response from memory
 * WHY:  Previously seen antigen
 * HOW:  Rapid B cell activation + antibody production
 *
 * @param system Immune system
 * @param antigen_id Target antigen
 * @param memory_b_cell_id Memory B cell to reactivate
 * @return 0 on success
 */
int brain_immune_secondary_response(
    brain_immune_system_t* system,
    uint32_t antigen_id,
    uint32_t memory_b_cell_id
);

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

/**
 * @brief Set antigen detection callback
 */
int brain_immune_set_antigen_callback(
    brain_immune_system_t* system,
    brain_immune_antigen_cb_t callback,
    void* user_data
);

/**
 * @brief Set neutralization callback
 */
int brain_immune_set_neutralize_callback(
    brain_immune_system_t* system,
    brain_immune_neutralize_cb_t callback,
    void* user_data
);

/**
 * @brief Set cytokine callback
 */
int brain_immune_set_cytokine_callback(
    brain_immune_system_t* system,
    brain_immune_cytokine_cb_t callback,
    void* user_data
);

/**
 * @brief Set inflammation callback
 */
int brain_immune_set_inflammation_callback(
    brain_immune_system_t* system,
    brain_immune_inflammation_cb_t callback,
    void* user_data
);

/**
 * @brief Set kill callback
 */
int brain_immune_set_kill_callback(
    brain_immune_system_t* system,
    brain_immune_kill_cb_t callback,
    void* user_data
);

/* ============================================================================
 * Update and Query API
 * ============================================================================ */

/**
 * @brief Update immune system state
 *
 * WHAT: Process pending immune activity
 * WHY:  Advance immune state machine
 * HOW:  Process antigens, decay antibodies, resolve inflammation
 *
 * @param system Immune system
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int brain_immune_update(
    brain_immune_system_t* system,
    uint64_t delta_ms
);

/**
 * @brief Get immune system statistics
 *
 * @param system Immune system
 * @param stats Output statistics
 * @return 0 on success
 */
int brain_immune_get_stats(
    brain_immune_system_t* system,
    brain_immune_stats_t* stats
);

/**
 * @brief Get immune state snapshot for checkpointing
 *
 * WHAT: Capture immune state for fault tolerance checkpoints
 * WHY:  Include immune health in system checkpoints
 * HOW:  Extract key metrics for checkpoint storage
 *
 * @param system Immune system
 * @param state Output state snapshot (must be bft_immune_state_t*)
 * @return 0 on success
 */
int brain_immune_get_checkpoint_state(
    brain_immune_system_t* system,
    void* state
);

/**
 * @brief Get current immune phase
 *
 * @param system Immune system
 * @return Current phase
 */
brain_immune_phase_t brain_immune_get_phase(
    brain_immune_system_t* system
);

/**
 * @brief Check if antigen is neutralized
 *
 * @param system Immune system
 * @param antigen_id Antigen to check
 * @return true if neutralized
 */
bool brain_immune_is_neutralized(
    brain_immune_system_t* system,
    uint32_t antigen_id
);

/**
 * @brief Get antigen by ID
 *
 * @param system Immune system
 * @param antigen_id Antigen ID
 * @return Antigen or NULL if not found
 */
const brain_antigen_t* brain_immune_get_antigen(
    brain_immune_system_t* system,
    uint32_t antigen_id
);

/**
 * @brief Thread-safe antigen copy by ID (no dangling pointer risk)
 * @return 0 on success, -1 if not found
 */
int brain_immune_get_antigen_copy(
    brain_immune_system_t* system,
    uint32_t antigen_id,
    brain_antigen_t* out
);

/**
 * @brief Compute affinity between patterns
 *
 * WHAT: Calculate pattern similarity
 * WHY:  Determine receptor-epitope binding strength
 * HOW:  Fuzzy pattern matching
 *
 * @param pattern1 First pattern
 * @param len1 First length
 * @param pattern2 Second pattern
 * @param len2 Second length
 * @return Affinity score (0-1)
 */
float brain_immune_compute_affinity(
    const uint8_t* pattern1,
    size_t len1,
    const uint8_t* pattern2,
    size_t len2
);

/* ============================================================================
 * Cytokine and Inflammation Getters
 * ============================================================================ */

/**
 * @brief Get current cytokine level
 *
 * WHAT: Query current concentration of a specific cytokine
 * WHY:  Allow modules to adjust behavior based on inflammatory state
 * HOW:  Look up cytokine level from internal stats
 *
 * @param system Immune system
 * @param type Cytokine type to query
 * @return Cytokine concentration (0.0-1.0), or 0.0 on error
 */
float brain_immune_get_cytokine_level(
    brain_immune_system_t* system,
    brain_cytokine_type_t type
);

/**
 * @brief Get current inflammation level
 *
 * WHAT: Query current system-wide inflammation state
 * WHY:  Allow modules to modulate behavior during inflammation
 * HOW:  Return current inflammation level from stats
 *
 * @param system Immune system
 * @return Current inflammation level
 */
brain_inflammation_level_t brain_immune_get_inflammation_level(
    brain_immune_system_t* system
);

/**
 * @brief Get current continuous inflammation level
 *
 * WHAT: Query current system-wide inflammation as continuous float
 * WHY:  Enables smooth, proportional modulation instead of discrete steps
 * HOW:  Return max continuous inflammation across all active sites
 *
 * @param system Immune system
 * @return Continuous inflammation level [0.0-1.0], 0.0 on error
 */
float brain_immune_get_inflammation_level_continuous(
    brain_immune_system_t* system
);

/**
 * @brief Get full inflammation effects for current system state
 *
 * WHAT: Compute all inflammation-modulated quantities for current state
 * WHY:  Single call gives bridges everything they need for continuous modulation
 * HOW:  Query max inflammation, compute effects via smooth curves
 *
 * @param system Immune system
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 */
int brain_immune_get_inflammation_effects(
    brain_immune_system_t* system,
    inflammation_effects_t* effects
);

/* ============================================================================
 * Imagination Engine Integration
 * ============================================================================ */

/**
 * @brief Send imagination modulation based on inflammation
 *
 * WHAT: Notify imagination engine of immune-driven modulation
 * WHY:  Sickness behavior includes reduced imaginative capacity
 * HOW:  Compute vividness/coherence modifiers, send via bio-async
 *
 * BIOLOGICAL BASIS:
 * Inflammation triggers "sickness behavior" including:
 * - Reduced creativity and imagination vividness
 * - Lower working memory capacity
 * - Impaired cognitive flexibility
 *
 * @param system Brain immune system
 * @return 0 on success, -1 on error
 */
int brain_immune_send_imagination_modulation(brain_immune_system_t* system);

/**
 * @brief Register handler for imagination engine messages
 *
 * WHAT: Enable bidirectional communication with imagination engine
 * WHY:  Imagination may query immune state for modulation
 * HOW:  Register bio-async handler for imagination message types
 *
 * @param system Brain immune system
 * @return 0 on success, -1 on error
 */
int brain_immune_register_imagination_handler(brain_immune_system_t* system);

/* ============================================================================
 * Exception System Integration
 * ============================================================================ */

/* Forward declarations for exception types */
struct nimcp_exception;
typedef struct nimcp_exception nimcp_exception_t;

/**
 * @brief Callback for exception-based antigen presentation
 *
 * Called when exception system presents exception as antigen.
 */
typedef void (*brain_immune_exception_cb_t)(
    brain_immune_system_t* system,
    const nimcp_exception_t* exception,
    uint32_t antigen_id,
    void* user_data
);

/**
 * @brief Callback for recovery action completion
 *
 * Called when recovery action completes (success or failure).
 */
typedef void (*brain_immune_recovery_cb_t)(
    brain_immune_system_t* system,
    uint32_t antigen_id,
    int recovery_action,
    bool success,
    void* user_data
);

/**
 * @brief Present exception as antigen to immune system
 *
 * WHAT: Convert exception to antigen and initiate immune response
 * WHY:  Enable automatic error recovery through immune system
 * HOW:  Create antigen from exception epitope, trigger processing
 *
 * @param system Brain immune system
 * @param exception Exception to present
 * @param antigen_id_out Output: assigned antigen ID
 * @return 0 on success, -1 on error
 */
int brain_immune_present_exception(
    brain_immune_system_t* system,
    const nimcp_exception_t* exception,
    uint32_t* antigen_id_out
);

/**
 * @brief Set exception presentation callback
 *
 * @param system Immune system
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success
 */
int brain_immune_set_exception_callback(
    brain_immune_system_t* system,
    brain_immune_exception_cb_t callback,
    void* user_data
);

/**
 * @brief Set recovery completion callback
 *
 * @param system Immune system
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success
 */
int brain_immune_set_recovery_callback(
    brain_immune_system_t* system,
    brain_immune_recovery_cb_t callback,
    void* user_data
);

/**
 * @brief Notify immune system of recovery result
 *
 * WHAT: Inform immune system whether recovery succeeded
 * WHY:  Enable memory formation for successful patterns
 * HOW:  Update antibody effectiveness, form memory cells
 *
 * @param system Immune system
 * @param antigen_id Antigen that was recovered
 * @param recovery_action Action that was taken
 * @param success Whether recovery succeeded
 * @return 0 on success
 */
int brain_immune_notify_recovery_result(
    brain_immune_system_t* system,
    uint32_t antigen_id,
    int recovery_action,
    bool success
);

/**
 * @brief Get recommended recovery action for antigen
 *
 * WHAT: Query immune system for best recovery action
 * WHY:  Let immune memory guide recovery decisions
 * HOW:  Check for matching memory cells, return learned action
 *
 * @param system Immune system
 * @param antigen_id Antigen to query
 * @param action_out Output: recommended action
 * @return 0 on success, -1 if no recommendation
 */
int brain_immune_get_recovery_recommendation(
    brain_immune_system_t* system,
    uint32_t antigen_id,
    int* action_out
);

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

const char* brain_immune_phase_to_string(brain_immune_phase_t phase);
const char* brain_immune_b_cell_state_to_string(brain_b_cell_state_t state);
const char* brain_immune_t_cell_type_to_string(brain_t_cell_type_t type);
const char* brain_immune_cytokine_to_string(brain_cytokine_type_t type);
const char* brain_immune_inflammation_to_string(brain_inflammation_level_t level);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_IMMUNE_H */
