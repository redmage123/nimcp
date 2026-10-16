/**
 * @file nimcp_security_immune_unified_bridge.h
 * @brief Unified Security-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-31
 *
 * WHAT: Comprehensive bidirectional integration between all security components
 *       and the brain immune system through a single unified bridge.
 * WHY:  Biological immune systems coordinate with physical barriers and detection
 *       systems to provide layered defense; this mirrors that by unifying BBB,
 *       anomaly detection, pattern database, rate limiting, and policy engine
 *       with the adaptive immune response system.
 * HOW:  Each security component presents threats as antigens and receives immune
 *       modulation signals; immune responses map to security actions; cytokines
 *       tune security sensitivity; tolerance prevents false positives.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * NEUROIMMUNE INTEGRATION MODEL:
 * The brain's immune system (microglia, astrocytes, perivascular macrophages)
 * works in concert with the blood-brain barrier and glymphatic system to provide
 * comprehensive neural protection. Key biological principles modeled:
 *
 * 1. Blood-Brain Barrier as First Line Defense:
 *    - BBB endothelial cells communicate with immune cells via cytokines
 *    - Inflammation increases BBB permeability (modeled as threshold adjustment)
 *    - Immune cells can signal BBB to strengthen or relax barriers
 *    - Reference: Abbott et al., "Astrocyte-endothelial interactions at the BBB"
 *
 * 2. Pattern Recognition and Adaptive Immunity:
 *    - Microglia recognize patterns via Toll-like receptors (TLRs)
 *    - B cell memory provides rapid secondary response to known threats
 *    - Antibody affinity maturation improves pattern specificity over time
 *    - Reference: Ransohoff & Perry, "Microglial Physiology"
 *
 * 3. Cytokine-Mediated Coordination:
 *    - Pro-inflammatory cytokines (IL-1, IL-6, TNF-a) heighten alertness
 *    - Anti-inflammatory cytokines (IL-10, TGF-b) promote resolution
 *    - Cytokine storms cause pathological hyper-response
 *    - Reference: Dantzer et al., "Cytokine-Induced Sickness Behavior"
 *
 * 4. Tolerance Mechanisms:
 *    - Central tolerance: T/B cells that react to self are eliminated
 *    - Peripheral tolerance: Regulatory T cells suppress over-reaction
 *    - Prevents autoimmune damage (maps to false positive prevention)
 *    - Reference: Wing & Sakaguchi, "Regulatory T cells"
 *
 * SECURITY-IMMUNE MAPPING:
 * ==================================================================================
 *
 * SECURITY COMPONENT          IMMUNE EQUIVALENT           INTEGRATION
 * ─────────────────────────────────────────────────────────────────────────────────
 * BBB (Blood-Brain Barrier)   Physical barrier + TLRs    Threats → Antigens
 *                                                         Inflammation → Thresholds
 *
 * Anomaly Detector            Pattern recognition         Anomalies → B cell activation
 *                             (innate immunity)           Cytokines → Sensitivity
 *
 * Pattern Database            Immune memory (B cells)     Patterns ↔ Memory cells
 *                                                         Affinity → Weight refinement
 *
 * Rate Limiter                Metabolic regulation        Violations → Inflammation
 *                                                         Fatigue → Rate reduction
 *
 * Policy Engine               Regulatory T cells          Violations → T cell response
 *                                                         Policy → Tolerance rules
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════════╗
 * ║              UNIFIED SECURITY-IMMUNE BRIDGE                                    ║
 * ╠═══════════════════════════════════════════════════════════════════════════════╣
 * ║                                                                                ║
 * ║  ┌──────────────────────────────────────────────────────────────────────────┐ ║
 * ║  │                    SECURITY → IMMUNE PATHWAYS                             │ ║
 * ║  │                                                                           │ ║
 * ║  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐            │ ║
 * ║  │  │   BBB   │ │ ANOMALY │ │ PATTERN │ │  RATE   │ │ POLICY  │            │ ║
 * ║  │  │ THREATS │ │ DETECT  │ │   DB    │ │ LIMITER │ │ ENGINE  │            │ ║
 * ║  │  └────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘            │ ║
 * ║  │       │           │           │           │           │                  │ ║
 * ║  │       ▼           ▼           ▼           ▼           ▼                  │ ║
 * ║  │  ┌─────────────────────────────────────────────────────────────────────┐ │ ║
 * ║  │  │              UNIFIED ANTIGEN PRESENTATION                           │ │ ║
 * ║  │  │  - BBB threats → severity-mapped antigens                           │ │ ║
 * ║  │  │  - Anomalies → feature-extracted epitopes                           │ │ ║
 * ║  │  │  - Pattern matches → recognition signatures                         │ │ ║
 * ║  │  │  - Rate violations → abuse indicators                               │ │ ║
 * ║  │  │  - Policy violations → rule-based antigens                          │ │ ║
 * ║  │  └─────────────────────────────────────────────────────────────────────┘ │ ║
 * ║  │                              │                                           │ ║
 * ║  │                              ▼                                           │ ║
 * ║  │  ┌─────────────────────────────────────────────────────────────────────┐ │ ║
 * ║  │  │              IMMUNE RESPONSE GENERATION                             │ │ ║
 * ║  │  │  - B cell activation → antibody production                          │ │ ║
 * ║  │  │  - T cell activation → killer/helper response                       │ │ ║
 * ║  │  │  - Cytokine release → coordination signals                          │ │ ║
 * ║  │  │  - Memory formation → learned threat patterns                       │ │ ║
 * ║  │  └─────────────────────────────────────────────────────────────────────┘ │ ║
 * ║  └──────────────────────────────────────────────────────────────────────────┘ ║
 * ║                                                                                ║
 * ║  ┌──────────────────────────────────────────────────────────────────────────┐ ║
 * ║  │                    IMMUNE → SECURITY PATHWAYS                             │ ║
 * ║  │                                                                           │ ║
 * ║  │  ┌─────────────────────────────────────────────────────────────────────┐ │ ║
 * ║  │  │              CYTOKINE EFFECTS ON SECURITY                           │ │ ║
 * ║  │  │                                                                     │ │ ║
 * ║  │  │  IL-1β:  Boost threat detection sensitivity (+10-15%)               │ │ ║
 * ║  │  │  IL-6:   Escalate security response level (+15-20%)                 │ │ ║
 * ║  │  │  TNF-α:  Trigger emergency security mode (+25-30%)                  │ │ ║
 * ║  │  │  IL-10:  Reduce false positive rate (-15-20%)                       │ │ ║
 * ║  │  │  IFN-γ:  Activate quarantine protocols (+12-15%)                    │ │ ║
 * ║  │  └─────────────────────────────────────────────────────────────────────┘ │ ║
 * ║  │                              │                                           │ ║
 * ║  │                              ▼                                           │ ║
 * ║  │  ┌─────────────────────────────────────────────────────────────────────┐ │ ║
 * ║  │  │              INFLAMMATION-BASED MODULATION                          │ │ ║
 * ║  │  │                                                                     │ │ ║
 * ║  │  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐       │ │ ║
 * ║  │  │  │  BBB    │ │ ANOMALY │ │ PATTERN │ │  RATE   │ │ POLICY  │       │ │ ║
 * ║  │  │  │ ─────── │ │ ─────── │ │ ─────── │ │ ─────── │ │ ─────── │       │ │ ║
 * ║  │  │  │Threshold│ │Threshold│ │ Weight  │ │  RPS    │ │Strictness│      │ │ ║
 * ║  │  │  │Adjust   │ │Modulate │ │  Boost  │ │ Reduce  │ │ Escalate│       │ │ ║
 * ║  │  │  └─────────┘ └─────────┘ └─────────┘ └─────────┘ └─────────┘       │ │ ║
 * ║  │  └─────────────────────────────────────────────────────────────────────┘ │ ║
 * ║  │                                                                           │ ║
 * ║  │  ┌─────────────────────────────────────────────────────────────────────┐ │ ║
 * ║  │  │              ANTIBODY-TO-ACTION MAPPING                             │ │ ║
 * ║  │  │                                                                     │ │ ║
 * ║  │  │  IgM Antibody → Quick block/alert (first response)                  │ │ ║
 * ║  │  │  IgG Antibody → Targeted quarantine (mature response)               │ │ ║
 * ║  │  │  IgE Antibody → Emergency lockdown (severe threat)                  │ │ ║
 * ║  │  │  Killer T     → BFT quarantine + rate limit block                   │ │ ║
 * ║  │  │  Helper T     → Policy escalation + alert broadcast                 │ │ ║
 * ║  │  └─────────────────────────────────────────────────────────────────────┘ │ ║
 * ║  │                                                                           │ ║
 * ║  │  ┌─────────────────────────────────────────────────────────────────────┐ │ ║
 * ║  │  │              TOLERANCE SYSTEM (False Positive Prevention)           │ │ ║
 * ║  │  │                                                                     │ │ ║
 * ║  │  │  - Whitelist patterns (self-tolerance)                              │ │ ║
 * ║  │  │  - Regulatory T cell suppression                                    │ │ ║
 * ║  │  │  - IL-10 mediated relaxation                                        │ │ ║
 * ║  │  │  - Learned benign patterns                                          │ │ ║
 * ║  │  └─────────────────────────────────────────────────────────────────────┘ │ ║
 * ║  └──────────────────────────────────────────────────────────────────────────┘ ║
 * ║                                                                                ║
 * ╚═══════════════════════════════════════════════════════════════════════════════╝
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

#ifndef NIMCP_SECURITY_IMMUNE_UNIFIED_BRIDGE_H
#define NIMCP_SECURITY_IMMUNE_UNIFIED_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Base bridge infrastructure */
#include "utils/bridge/nimcp_bridge_base.h"

/* Security module integrations */
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_anomaly_detector.h"
#include "security/nimcp_pattern_db.h"
#include "security/nimcp_rate_limiter.h"
#include "security/nimcp_policy_engine.h"

/* Immune system integration */
#include "cognitive/immune/nimcp_brain_immune.h"

/* Platform utilities */
#include "utils/platform/nimcp_platform_mutex.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants - Cytokine Effects on Security
 * ============================================================================ */

/**
 * @name Cytokine-to-Security Threshold Modulation
 * Maps biological cytokine effects to security parameter adjustments
 * @{
 */

/* IL-1 beta: Pro-inflammatory, boosts threat detection */
#define SEC_IMMUNE_IL1_BBB_THRESHOLD_IMPACT       -0.10f  /**< Lower BBB thresholds */
#define SEC_IMMUNE_IL1_ANOMALY_THRESHOLD_IMPACT   -0.10f  /**< More sensitive anomaly detection */
#define SEC_IMMUNE_IL1_PATTERN_WEIGHT_IMPACT       0.10f  /**< Boost pattern weights */
#define SEC_IMMUNE_IL1_RATE_LIMIT_IMPACT          -0.15f  /**< Reduce rate limits */
#define SEC_IMMUNE_IL1_POLICY_STRICTNESS_IMPACT    0.10f  /**< Stricter policy enforcement */

/* IL-6: Acute phase response, escalates security */
#define SEC_IMMUNE_IL6_BBB_THRESHOLD_IMPACT       -0.15f  /**< Aggressive BBB */
#define SEC_IMMUNE_IL6_ANOMALY_THRESHOLD_IMPACT   -0.15f  /**< Very sensitive detection */
#define SEC_IMMUNE_IL6_PATTERN_WEIGHT_IMPACT       0.15f  /**< Higher pattern priority */
#define SEC_IMMUNE_IL6_RATE_LIMIT_IMPACT          -0.20f  /**< Significant rate reduction */
#define SEC_IMMUNE_IL6_POLICY_STRICTNESS_IMPACT    0.15f  /**< Escalated policy */

/* TNF-alpha: Severe inflammation, emergency mode */
#define SEC_IMMUNE_TNF_BBB_THRESHOLD_IMPACT       -0.25f  /**< Maximum BBB sensitivity */
#define SEC_IMMUNE_TNF_ANOMALY_THRESHOLD_IMPACT   -0.25f  /**< Paranoid anomaly mode */
#define SEC_IMMUNE_TNF_PATTERN_WEIGHT_IMPACT       0.25f  /**< Emergency pattern priority */
#define SEC_IMMUNE_TNF_RATE_LIMIT_IMPACT          -0.30f  /**< Emergency rate throttling */
#define SEC_IMMUNE_TNF_POLICY_STRICTNESS_IMPACT    0.25f  /**< Maximum policy strictness */

/* IL-10: Anti-inflammatory, reduces false positives */
#define SEC_IMMUNE_IL10_BBB_THRESHOLD_IMPACT       0.15f  /**< Relax BBB thresholds */
#define SEC_IMMUNE_IL10_ANOMALY_THRESHOLD_IMPACT   0.20f  /**< Reduce false positives */
#define SEC_IMMUNE_IL10_PATTERN_WEIGHT_IMPACT     -0.15f  /**< Lower pattern aggressiveness */
#define SEC_IMMUNE_IL10_RATE_LIMIT_IMPACT          0.25f  /**< Recovery rate restoration */
#define SEC_IMMUNE_IL10_POLICY_STRICTNESS_IMPACT  -0.15f  /**< Relaxed policy */

/* IFN-gamma: Antiviral-style response, quarantine protocols */
#define SEC_IMMUNE_IFN_BBB_THRESHOLD_IMPACT       -0.12f  /**< Tighter BBB control */
#define SEC_IMMUNE_IFN_ANOMALY_THRESHOLD_IMPACT   -0.12f  /**< Enhanced monitoring */
#define SEC_IMMUNE_IFN_PATTERN_WEIGHT_IMPACT       0.12f  /**< Isolation patterns */
#define SEC_IMMUNE_IFN_RATE_LIMIT_IMPACT          -0.10f  /**< Moderate restriction */
#define SEC_IMMUNE_IFN_POLICY_STRICTNESS_IMPACT    0.12f  /**< Quarantine policies */

/** @} */

/* ============================================================================
 * Constants - Inflammation Level Modulation
 * ============================================================================ */

/**
 * @name Inflammation-to-Security Factor Mapping
 * Maps inflammation severity to security parameter multipliers
 * @{
 */

/* BBB threat threshold factors (lower = more sensitive) */
#define SEC_IMMUNE_INFL_NONE_BBB_FACTOR           1.00f
#define SEC_IMMUNE_INFL_LOCAL_BBB_FACTOR          0.90f
#define SEC_IMMUNE_INFL_REGIONAL_BBB_FACTOR       0.75f
#define SEC_IMMUNE_INFL_SYSTEMIC_BBB_FACTOR       0.60f
#define SEC_IMMUNE_INFL_STORM_BBB_FACTOR          0.40f

/* Anomaly detection threshold factors */
#define SEC_IMMUNE_INFL_NONE_ANOMALY_FACTOR       1.00f
#define SEC_IMMUNE_INFL_LOCAL_ANOMALY_FACTOR      0.90f
#define SEC_IMMUNE_INFL_REGIONAL_ANOMALY_FACTOR   0.70f
#define SEC_IMMUNE_INFL_SYSTEMIC_ANOMALY_FACTOR   0.50f
#define SEC_IMMUNE_INFL_STORM_ANOMALY_FACTOR      0.30f

/* Pattern weight multipliers (higher = more aggressive) */
#define SEC_IMMUNE_INFL_NONE_PATTERN_FACTOR       1.00f
#define SEC_IMMUNE_INFL_LOCAL_PATTERN_FACTOR      1.10f
#define SEC_IMMUNE_INFL_REGIONAL_PATTERN_FACTOR   1.30f
#define SEC_IMMUNE_INFL_SYSTEMIC_PATTERN_FACTOR   1.50f
#define SEC_IMMUNE_INFL_STORM_PATTERN_FACTOR      2.00f

/* Rate limit factors (lower = more restrictive) */
#define SEC_IMMUNE_INFL_NONE_RATE_FACTOR          1.00f
#define SEC_IMMUNE_INFL_LOCAL_RATE_FACTOR         0.90f
#define SEC_IMMUNE_INFL_REGIONAL_RATE_FACTOR      0.70f
#define SEC_IMMUNE_INFL_SYSTEMIC_RATE_FACTOR      0.50f
#define SEC_IMMUNE_INFL_STORM_RATE_FACTOR         0.20f

/* Policy strictness factors (higher = stricter) */
#define SEC_IMMUNE_INFL_NONE_POLICY_FACTOR        1.00f
#define SEC_IMMUNE_INFL_LOCAL_POLICY_FACTOR       1.10f
#define SEC_IMMUNE_INFL_REGIONAL_POLICY_FACTOR    1.25f
#define SEC_IMMUNE_INFL_SYSTEMIC_POLICY_FACTOR    1.50f
#define SEC_IMMUNE_INFL_STORM_POLICY_FACTOR       2.00f

/** @} */

/* ============================================================================
 * Constants - Antigen Presentation Thresholds
 * ============================================================================ */

/**
 * @name Security-to-Antigen Mapping Thresholds
 * @{
 */

/* BBB threat to antigen mapping */
#define SEC_IMMUNE_BBB_MIN_SEVERITY_FOR_ANTIGEN      BBB_SEVERITY_MEDIUM
#define SEC_IMMUNE_BBB_SEVERITY_MULTIPLIER           2.5f  /**< BBB severity → antigen severity */
#define SEC_IMMUNE_BBB_QUARANTINE_INFLAMMATION       INFLAMMATION_REGIONAL

/* Anomaly detection to antigen mapping */
#define SEC_IMMUNE_ANOMALY_MIN_SCORE_FOR_ANTIGEN     0.5f  /**< Min score to present */
#define SEC_IMMUNE_ANOMALY_SCORE_MULTIPLIER          10.0f /**< Score [0-1] → severity [0-10] */
#define SEC_IMMUNE_ANOMALY_HIGH_SCORE_THRESHOLD      0.8f  /**< High-confidence threshold */

/* Pattern match to antigen mapping */
#define SEC_IMMUNE_PATTERN_MIN_SCORE_FOR_ANTIGEN     0.5f  /**< Min match score */
#define SEC_IMMUNE_PATTERN_SCORE_MULTIPLIER          10.0f /**< Score → severity */
#define SEC_IMMUNE_PATTERN_CRITICAL_THRESHOLD        0.9f  /**< Critical match threshold */

/* Rate limiter violation to antigen mapping */
#define SEC_IMMUNE_RATE_MIN_VIOLATIONS_FOR_ANTIGEN   2     /**< Min violations to present */
#define SEC_IMMUNE_RATE_VIOLATIONS_FOR_INFLAMMATION  5     /**< Violations for inflammation */
#define SEC_IMMUNE_RATE_SEVERITY_BASE                3     /**< Base severity */
#define SEC_IMMUNE_RATE_SEVERITY_PER_VIOLATION       1     /**< +1 per violation */

/* Policy violation to antigen mapping */
#define SEC_IMMUNE_POLICY_MIN_SEVERITY_FOR_ANTIGEN   NIMCP_POLICY_SEVERITY_MEDIUM
#define SEC_IMMUNE_POLICY_SEVERITY_MULTIPLIER        2.0f  /**< Policy severity → antigen */
#define SEC_IMMUNE_POLICY_CRITICAL_THRESHOLD         NIMCP_POLICY_SEVERITY_CRITICAL

/** @} */

/* ============================================================================
 * Constants - Tolerance System
 * ============================================================================ */

/**
 * @name Tolerance/False Positive Prevention
 * @{
 */

#define SEC_IMMUNE_TOLERANCE_MAX_WHITELIST           256   /**< Max whitelisted patterns */
#define SEC_IMMUNE_TOLERANCE_LEARNING_PERIOD_MS      86400000  /**< 24h learning period */
#define SEC_IMMUNE_TOLERANCE_CONFIRMATION_COUNT      5     /**< Confirms for whitelist */
#define SEC_IMMUNE_TOLERANCE_IL10_SUPPRESSION        0.5f  /**< IL-10 suppression factor */
#define SEC_IMMUNE_TOLERANCE_REGULATORY_THRESHOLD    0.3f  /**< T-reg activation threshold */

/** @} */

/* ============================================================================
 * Constants - Memory Formation
 * ============================================================================ */

/**
 * @name Memory Cell Formation from Threats
 * @{
 */

#define SEC_IMMUNE_MEMORY_MIN_NEUTRALIZATIONS        2     /**< Neutralizations for memory */
#define SEC_IMMUNE_MEMORY_PATTERN_PRIORITY           5     /**< Priority for memory patterns */
#define SEC_IMMUNE_MEMORY_ANTIBODY_WEIGHT            0.9f  /**< Weight for learned antibodies */
#define SEC_IMMUNE_MEMORY_DECAY_HALF_LIFE_MS         604800000  /**< 7 day half-life */

/** @} */

/* ============================================================================
 * Structures - Cytokine Effects
 * ============================================================================ */

/**
 * @brief Cytokine effects on all security components
 *
 * Tracks how each cytokine type modulates security parameters
 */
typedef struct {
    /* Per-cytokine effects on BBB */
    float il1_bbb_modulation;
    float il6_bbb_modulation;
    float tnf_bbb_modulation;
    float il10_bbb_modulation;
    float ifn_bbb_modulation;

    /* Per-cytokine effects on anomaly detection */
    float il1_anomaly_modulation;
    float il6_anomaly_modulation;
    float tnf_anomaly_modulation;
    float il10_anomaly_modulation;
    float ifn_anomaly_modulation;

    /* Per-cytokine effects on pattern matching */
    float il1_pattern_modulation;
    float il6_pattern_modulation;
    float tnf_pattern_modulation;
    float il10_pattern_modulation;
    float ifn_pattern_modulation;

    /* Per-cytokine effects on rate limiting */
    float il1_rate_modulation;
    float il6_rate_modulation;
    float tnf_rate_modulation;
    float il10_rate_modulation;
    float ifn_rate_modulation;

    /* Per-cytokine effects on policy */
    float il1_policy_modulation;
    float il6_policy_modulation;
    float tnf_policy_modulation;
    float il10_policy_modulation;
    float ifn_policy_modulation;

    /* Aggregate effects */
    float total_bbb_modulation;           /**< Combined BBB threshold change */
    float total_anomaly_modulation;       /**< Combined anomaly threshold change */
    float total_pattern_modulation;       /**< Combined pattern weight change */
    float total_rate_modulation;          /**< Combined rate limit change */
    float total_policy_modulation;        /**< Combined policy strictness change */

    /* Mode flags */
    bool emergency_mode_active;           /**< TNF-a triggered emergency */
    bool recovery_mode_active;            /**< IL-10 recovery active */
} sec_immune_cytokine_effects_t;

/**
 * @brief Inflammation effects on security
 *
 * How systemic inflammation level affects all security components
 */
typedef struct {
    /* Current inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;
    bool is_chronic;                      /**< > 24h duration */

    /* Per-component factors */
    float bbb_threshold_factor;           /**< BBB multiplier */
    float anomaly_threshold_factor;       /**< Anomaly multiplier */
    float pattern_weight_factor;          /**< Pattern multiplier */
    float rate_limit_factor;              /**< Rate multiplier */
    float policy_strictness_factor;       /**< Policy multiplier */

    /* Mode flags */
    bool hypervigilant_mode;              /**< Systemic+ inflammation */
    bool emergency_lockdown;              /**< Storm-level response */
    bool resource_conservation;           /**< Prioritizing critical functions */
} sec_immune_inflammation_state_t;

/* ============================================================================
 * Structures - Per-Component Modulation State
 * ============================================================================ */

/**
 * @brief BBB-Immune bidirectional state
 */
typedef struct {
    /* BBB → Immune */
    uint32_t threats_presented;           /**< BBB threats as antigens */
    uint32_t quarantines_initiated;       /**< Quarantine actions taken */
    float max_threat_severity;            /**< Highest recent severity */
    uint64_t last_threat_time;            /**< Last BBB threat timestamp */

    /* Immune → BBB */
    float effective_threshold_factor;     /**< Current threshold adjustment */
    bool paranoid_mode;                   /**< Maximum sensitivity active */
    uint32_t threshold_adjustments;       /**< Times threshold was adjusted */
} sec_immune_bbb_state_t;

/**
 * @brief Anomaly-Immune bidirectional state
 */
typedef struct {
    /* Anomaly → Immune */
    uint32_t anomalies_presented;         /**< Anomalies as antigens */
    uint32_t b_cells_activated;           /**< B cells from anomalies */
    float avg_anomaly_score;              /**< Average detection score */
    uint64_t last_anomaly_time;           /**< Last anomaly timestamp */

    /* Immune → Anomaly */
    float effective_threshold;            /**< Current threshold after modulation */
    float sensitivity_boost;              /**< Detection sensitivity increase */
    bool hypervigilant_detection;         /**< Enhanced detection mode */

    /* Training feedback */
    uint32_t true_positives;              /**< Confirmed threats */
    uint32_t false_positives;             /**< Confirmed benign */
    float training_quality;               /**< Feedback quality [0-1] */
} sec_immune_anomaly_state_t;

/**
 * @brief Pattern-Immune bidirectional state
 */
typedef struct {
    /* Pattern → Immune */
    uint32_t matches_presented;           /**< Matches as antigens */
    uint32_t memory_formations;           /**< Memory cells from patterns */
    float max_match_score;                /**< Highest match score */
    uint64_t last_match_time;             /**< Last match timestamp */

    /* Immune → Pattern */
    float effective_weight_factor;        /**< Current weight multiplier */
    uint32_t patterns_from_memory;        /**< Patterns learned from immune */
    uint32_t patterns_refined;            /**< Patterns improved by affinity */
    uint32_t patterns_pruned;             /**< Unused patterns removed */

    /* Memory cell sync */
    uint32_t synced_memory_cells;         /**< Memory cells synced to patterns */
    float avg_affinity_score;             /**< Average antibody affinity */
} sec_immune_pattern_state_t;

/**
 * @brief Rate Limiter-Immune bidirectional state
 */
typedef struct {
    /* Rate → Immune */
    uint32_t violations_presented;        /**< Violations as antigens */
    uint32_t inflammation_triggers;       /**< Times inflammation triggered */
    uint32_t blocked_clients;             /**< Currently blocked */
    uint64_t last_violation_time;         /**< Last violation timestamp */

    /* Immune → Rate */
    float effective_rps_factor;           /**< Current RPS multiplier */
    float effective_burst_factor;         /**< Current burst multiplier */
    float penalty_severity_boost;         /**< Harsher penalties */
    bool emergency_throttling;            /**< Emergency mode active */

    /* Quarantine mapping */
    uint32_t quarantine_actions;          /**< Blocks mapped to quarantine */
    uint32_t memory_formations;           /**< Memory for repeat offenders */
} sec_immune_rate_state_t;

/**
 * @brief Policy-Immune bidirectional state
 */
typedef struct {
    /* Policy → Immune */
    uint32_t violations_presented;        /**< Violations as antigens */
    uint32_t t_cells_activated;           /**< T cells from policy violations */
    float avg_violation_severity;         /**< Average violation severity */
    uint64_t last_violation_time;         /**< Last violation timestamp */

    /* Immune → Policy */
    float effective_strictness_factor;    /**< Current strictness multiplier */
    bool emergency_enforcement;           /**< Emergency mode active */
    uint32_t escalation_count;            /**< Policy escalations */

    /* Regulatory T cell effects */
    float regulatory_suppression;         /**< T-reg suppression active */
    uint32_t suppressed_alerts;           /**< Alerts suppressed by T-regs */
} sec_immune_policy_state_t;

/* ============================================================================
 * Structures - Tolerance System
 * ============================================================================ */

/**
 * @brief Tolerance pattern entry (whitelist)
 */
typedef struct {
    uint8_t pattern[BRAIN_IMMUNE_EPITOPE_SIZE];  /**< Pattern signature */
    size_t pattern_len;                   /**< Pattern length */
    uint32_t confirmation_count;          /**< Times confirmed benign */
    uint64_t first_seen_time;             /**< When first detected */
    uint64_t last_confirmed_time;         /**< Last confirmation */
    bool is_permanent;                    /**< Permanent whitelist */
    const char* description;              /**< Human-readable description */
} sec_immune_tolerance_entry_t;

/**
 * @brief Tolerance system state
 */
typedef struct {
    /* Whitelist */
    sec_immune_tolerance_entry_t* whitelist;
    size_t whitelist_count;
    size_t whitelist_capacity;

    /* Learning state */
    bool learning_mode_active;            /**< In learning period */
    uint64_t learning_start_time;         /**< When learning started */

    /* Regulatory T cell state */
    float regulatory_activity;            /**< Current T-reg activity [0-1] */
    uint32_t active_regulatory_cells;     /**< Active T-reg count */

    /* Statistics */
    uint32_t false_positives_prevented;   /**< Blocked false positives */
    uint32_t patterns_whitelisted;        /**< Patterns added to whitelist */
    uint32_t patterns_confirmed;          /**< Patterns confirmed benign */
} sec_immune_tolerance_state_t;

/* ============================================================================
 * Structures - Antibody-to-Action Mapping
 * ============================================================================ */

/**
 * @brief Mapping from antibody to security action
 */
typedef struct {
    uint32_t antibody_id;                 /**< Source antibody */
    brain_antibody_class_t antibody_class; /**< Antibody class */
    uint32_t target_antigen_id;           /**< Target antigen */

    /* Security actions triggered */
    bbb_action_t bbb_action;              /**< BBB action taken */
    bool rate_limit_block;                /**< Rate limiter block */
    nimcp_policy_action_t policy_action;  /**< Policy action */

    /* Execution state */
    bool executed;                        /**< Action has been executed */
    float effectiveness;                  /**< Action effectiveness [0-1] */
    uint64_t execution_time;              /**< When executed */
} sec_immune_antibody_action_t;

/* ============================================================================
 * Structures - Memory Cell Formation
 * ============================================================================ */

/**
 * @brief Security threat memory cell
 */
typedef struct {
    uint32_t memory_id;                   /**< Unique memory ID */
    uint32_t source_antigen_id;           /**< Original antigen */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];  /**< Threat signature */
    size_t epitope_len;                   /**< Signature length */

    /* Source information */
    brain_antigen_source_t original_source; /**< Which security component */
    bbb_threat_type_t bbb_threat_type;    /**< If BBB source */
    float confidence;                     /**< Detection confidence */

    /* Response information */
    brain_antibody_class_t best_response; /**< Most effective antibody class */
    float best_effectiveness;             /**< Best response effectiveness */
    uint32_t neutralization_count;        /**< Times successfully neutralized */

    /* Pattern DB sync */
    bool synced_to_pattern_db;            /**< Synced to pattern database */
    nimcp_pattern_id_t pattern_id;        /**< Pattern DB ID if synced */

    /* Timing */
    uint64_t formation_time;              /**< When memory formed */
    uint64_t last_recognition_time;       /**< Last time recognized threat */
} sec_immune_memory_cell_t;

/* ============================================================================
 * Structures - Configuration
 * ============================================================================ */

/**
 * @brief Unified security-immune bridge configuration
 */
typedef struct {
    /* ===== Feature Enables ===== */

    /* Security → Immune enables */
    bool enable_bbb_antigen_presentation;
    bool enable_anomaly_antigen_presentation;
    bool enable_pattern_antigen_presentation;
    bool enable_rate_violation_antigen_presentation;
    bool enable_policy_violation_antigen_presentation;

    /* Immune → Security enables */
    bool enable_cytokine_bbb_modulation;
    bool enable_cytokine_anomaly_modulation;
    bool enable_cytokine_pattern_modulation;
    bool enable_cytokine_rate_modulation;
    bool enable_cytokine_policy_modulation;

    bool enable_inflammation_bbb_adjustment;
    bool enable_inflammation_anomaly_adjustment;
    bool enable_inflammation_pattern_adjustment;
    bool enable_inflammation_rate_adjustment;
    bool enable_inflammation_policy_adjustment;

    /* Bidirectional features */
    bool enable_antibody_action_execution;
    bool enable_memory_cell_formation;
    bool enable_pattern_memory_sync;
    bool enable_tolerance_system;
    bool enable_regulatory_t_cells;

    /* Auto-response enables */
    bool auto_trigger_inflammation;       /**< Auto-escalate on high severity */
    bool auto_form_memory;                /**< Auto memory on neutralization */
    bool auto_sync_patterns;              /**< Auto sync immune to patterns */
    bool auto_train_from_feedback;        /**< Auto train from immune feedback */

    /* ===== Threshold Tuning ===== */

    /* BBB thresholds */
    float bbb_base_threshold;             /**< Base BBB threshold */
    float bbb_min_threshold_factor;       /**< Min threshold (protection) */
    float bbb_max_threshold_factor;       /**< Max threshold */

    /* Anomaly thresholds */
    float anomaly_base_threshold;         /**< Base anomaly threshold */
    float anomaly_min_threshold;          /**< Min threshold (high sensitivity) */
    float anomaly_max_threshold;          /**< Max threshold (low sensitivity) */

    /* Pattern weights */
    float pattern_base_weight;            /**< Base pattern weight */
    float pattern_min_weight_factor;      /**< Min weight factor */
    float pattern_max_weight_factor;      /**< Max weight factor */

    /* Rate limits */
    float rate_base_rps;                  /**< Base RPS */
    float rate_base_burst;                /**< Base burst size */
    float rate_min_factor;                /**< Min rate factor */

    /* Policy strictness */
    float policy_base_strictness;         /**< Base policy strictness */
    float policy_min_strictness;          /**< Min strictness */
    float policy_max_strictness;          /**< Max strictness */

    /* ===== Tolerance Configuration ===== */

    size_t max_tolerance_entries;         /**< Max whitelist entries */
    uint32_t tolerance_confirmation_count; /**< Confirmations to whitelist */
    uint64_t tolerance_learning_period_ms; /**< Learning period duration */
    float regulatory_suppression_threshold; /**< T-reg activation threshold */

    /* ===== Memory Configuration ===== */

    uint32_t min_neutralizations_for_memory; /**< Min neutralizations for memory */
    uint64_t memory_decay_half_life_ms;   /**< Memory decay half-life */
    bool sync_memory_to_pattern_db;       /**< Auto sync to pattern DB */

    /* ===== Bio-Async Configuration ===== */

    bool enable_bio_async;                /**< Enable bio-async integration */
    bool broadcast_security_events;       /**< Broadcast security events */
    bool broadcast_immune_modulation;     /**< Broadcast immune modulation */
} sec_immune_unified_config_t;

/* ============================================================================
 * Structures - Statistics
 * ============================================================================ */

/**
 * @brief Comprehensive unified bridge statistics
 */
typedef struct {
    /* ===== Security → Immune Stats ===== */

    /* Antigen presentation counts */
    uint64_t bbb_antigens_presented;
    uint64_t anomaly_antigens_presented;
    uint64_t pattern_antigens_presented;
    uint64_t rate_antigens_presented;
    uint64_t policy_antigens_presented;
    uint64_t total_antigens_presented;

    /* B cell activations */
    uint64_t b_cells_activated;
    uint64_t t_cells_activated;
    uint64_t antibodies_produced;
    uint64_t memory_cells_formed;

    /* Inflammation triggers */
    uint64_t inflammation_triggers;
    uint64_t inflammation_escalations;
    uint64_t inflammation_resolutions;

    /* ===== Immune → Security Stats ===== */

    /* Modulation counts */
    uint64_t bbb_modulations;
    uint64_t anomaly_modulations;
    uint64_t pattern_modulations;
    uint64_t rate_modulations;
    uint64_t policy_modulations;

    /* Antibody action executions */
    uint64_t antibody_actions_executed;
    uint64_t quarantine_actions;
    uint64_t block_actions;

    /* ===== Tolerance Stats ===== */

    uint64_t false_positives_prevented;
    uint64_t patterns_whitelisted;
    uint64_t regulatory_suppressions;

    /* ===== Effectiveness Stats ===== */

    float avg_detection_time_ms;          /**< Average time to detect */
    float avg_response_time_ms;           /**< Average time to respond */
    float avg_neutralization_time_ms;     /**< Average time to neutralize */
    float overall_effectiveness;          /**< Overall system effectiveness */

    /* ===== Current State ===== */

    brain_inflammation_level_t current_inflammation;
    bool emergency_mode_active;
    bool tolerance_learning_active;
    float current_threat_level;           /**< Overall threat level [0-1] */
} sec_immune_unified_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief Unified Security-Immune Bridge
 *
 * Comprehensive bidirectional integration between all security components
 * and the brain immune system.
 */
typedef struct {
    /* MUST BE FIRST: Base bridge infrastructure */
    bridge_base_t base;

    /* ===== System Handles ===== */

    brain_immune_system_t* immune_system;
    bbb_system_t bbb_system;
    nimcp_anomaly_detector_t anomaly_detector;
    nimcp_pattern_db_t pattern_db;
    nimcp_rate_limiter_t rate_limiter;
    nimcp_policy_engine_t policy_engine;

    /* ===== Configuration ===== */

    sec_immune_unified_config_t config;

    /* ===== Cytokine and Inflammation State ===== */

    sec_immune_cytokine_effects_t cytokine_effects;
    sec_immune_inflammation_state_t inflammation_state;

    /* ===== Per-Component State ===== */

    sec_immune_bbb_state_t bbb_state;
    sec_immune_anomaly_state_t anomaly_state;
    sec_immune_pattern_state_t pattern_state;
    sec_immune_rate_state_t rate_state;
    sec_immune_policy_state_t policy_state;

    /* ===== Tolerance System ===== */

    sec_immune_tolerance_state_t tolerance;

    /* ===== Antibody-Action Mapping ===== */

    sec_immune_antibody_action_t* antibody_actions;
    size_t antibody_action_count;
    size_t antibody_action_capacity;

    /* ===== Memory Cells ===== */

    sec_immune_memory_cell_t* memory_cells;
    size_t memory_cell_count;
    size_t memory_cell_capacity;
    uint32_t next_memory_id;

    /* ===== Timing ===== */

    uint64_t last_update_time;
    uint64_t last_cytokine_update;
    uint64_t last_inflammation_update;
    uint64_t last_tolerance_update;
    uint64_t last_memory_decay;

    /* ===== Statistics ===== */

    sec_immune_unified_stats_t stats;

    /* ===== Thread Safety ===== */

    nimcp_platform_mutex_t* mutex;
} sec_immune_unified_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration for unified bridge
 * WHY:  Easy initialization with balanced security-immune integration
 * HOW:  Return struct with moderate sensitivity, all features enabled
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int sec_immune_unified_default_config(sec_immune_unified_config_t* config);

/**
 * @brief Create unified security-immune bridge
 *
 * WHAT: Initialize comprehensive security-immune integration
 * WHY:  Enable bidirectional coordination between all security and immune components
 * HOW:  Allocate state, connect all modules, register callbacks
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system handle
 * @return New bridge or NULL on failure
 */
sec_immune_unified_bridge_t* sec_immune_unified_create(
    const sec_immune_unified_config_t* config,
    brain_immune_system_t* immune_system
);

/**
 * @brief Destroy unified security-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect all modules, free memory
 *
 * @param bridge Bridge to destroy
 */
void sec_immune_unified_destroy(sec_immune_unified_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * WHAT: Reset statistics and modulation state
 * WHY:  Allow fresh start without recreating bridge
 * HOW:  Zero counters, reset modulation factors
 *
 * @param bridge Bridge handle
 * @return 0 on success
 */
int sec_immune_unified_reset(sec_immune_unified_bridge_t* bridge);

/* ============================================================================
 * Security Component Connection API
 * ============================================================================ */

/**
 * @brief Connect BBB system
 *
 * WHAT: Link Blood-Brain Barrier to unified bridge
 * WHY:  Enable BBB threat → antigen presentation and immune → threshold modulation
 * HOW:  Store handle, register threat callback
 *
 * @param bridge Bridge handle
 * @param bbb_system BBB system handle
 * @return 0 on success
 */
int sec_immune_unified_connect_bbb(
    sec_immune_unified_bridge_t* bridge,
    bbb_system_t bbb_system
);

/**
 * @brief Connect anomaly detector
 *
 * WHAT: Link anomaly detector to unified bridge
 * WHY:  Enable anomaly → B cell activation and immune → sensitivity modulation
 * HOW:  Store handle, register detection callback
 *
 * @param bridge Bridge handle
 * @param detector Anomaly detector handle
 * @return 0 on success
 */
int sec_immune_unified_connect_anomaly(
    sec_immune_unified_bridge_t* bridge,
    nimcp_anomaly_detector_t detector
);

/**
 * @brief Connect pattern database
 *
 * WHAT: Link pattern database to unified bridge
 * WHY:  Enable pattern ↔ immune memory bidirectional sync
 * HOW:  Store handle, register match callback
 *
 * @param bridge Bridge handle
 * @param pattern_db Pattern database handle
 * @return 0 on success
 */
int sec_immune_unified_connect_pattern_db(
    sec_immune_unified_bridge_t* bridge,
    nimcp_pattern_db_t pattern_db
);

/**
 * @brief Connect rate limiter
 *
 * WHAT: Link rate limiter to unified bridge
 * WHY:  Enable violation → inflammation and immune → rate adjustment
 * HOW:  Store handle, register violation callback
 *
 * @param bridge Bridge handle
 * @param rate_limiter Rate limiter handle
 * @return 0 on success
 */
int sec_immune_unified_connect_rate_limiter(
    sec_immune_unified_bridge_t* bridge,
    nimcp_rate_limiter_t rate_limiter
);

/**
 * @brief Connect policy engine
 *
 * WHAT: Link policy engine to unified bridge
 * WHY:  Enable policy violation → T cell response and immune → enforcement modulation
 * HOW:  Store handle, register violation callback
 *
 * @param bridge Bridge handle
 * @param policy_engine Policy engine handle
 * @return 0 on success
 */
int sec_immune_unified_connect_policy_engine(
    sec_immune_unified_bridge_t* bridge,
    nimcp_policy_engine_t policy_engine
);

/**
 * @brief Connect all security components at once
 *
 * WHAT: Convenience function to connect all security systems
 * WHY:  Simplify initialization
 * HOW:  Call individual connect functions
 *
 * @param bridge Bridge handle
 * @param bbb_system BBB handle (can be NULL)
 * @param detector Anomaly detector (can be NULL)
 * @param pattern_db Pattern database (can be NULL)
 * @param rate_limiter Rate limiter (can be NULL)
 * @param policy_engine Policy engine (can be NULL)
 * @return 0 on success
 */
int sec_immune_unified_connect_all(
    sec_immune_unified_bridge_t* bridge,
    bbb_system_t bbb_system,
    nimcp_anomaly_detector_t detector,
    nimcp_pattern_db_t pattern_db,
    nimcp_rate_limiter_t rate_limiter,
    nimcp_policy_engine_t policy_engine
);

/* ============================================================================
 * Update and Modulation API
 * ============================================================================ */

/**
 * @brief Update bridge state
 *
 * WHAT: Process immune state changes and apply security modulations
 * WHY:  Keep security parameters in sync with immune state
 * HOW:  Read cytokines/inflammation, compute factors, apply modulations
 *
 * @param bridge Bridge handle
 * @return 0 on success
 */
int sec_immune_unified_update(sec_immune_unified_bridge_t* bridge);

/**
 * @brief Apply cytokine effects to all security components
 *
 * WHAT: Compute and apply cytokine-based modulation
 * WHY:  Implement cytokine-induced security sensitivity changes
 * HOW:  Read each cytokine level, compute aggregate effect, apply
 *
 * @param bridge Bridge handle
 * @return 0 on success
 */
int sec_immune_unified_apply_cytokine_effects(sec_immune_unified_bridge_t* bridge);

/**
 * @brief Apply inflammation-based modulation
 *
 * WHAT: Adjust all security parameters based on inflammation level
 * WHY:  Implement inflammation-induced hypervigilance
 * HOW:  Map inflammation level to factors, apply to each component
 *
 * @param bridge Bridge handle
 * @return 0 on success
 */
int sec_immune_unified_apply_inflammation(sec_immune_unified_bridge_t* bridge);

/* ============================================================================
 * Security → Immune: Antigen Presentation API
 * ============================================================================ */

/**
 * @brief Present BBB threat as antigen
 *
 * WHAT: Convert BBB threat report to immune antigen
 * WHY:  Enable immune response to perimeter threats
 * HOW:  Map severity, extract epitope, present to immune system
 *
 * @param bridge Bridge handle
 * @param threat_type BBB threat type
 * @param severity BBB severity
 * @param threat_data Threat signature data
 * @param data_len Data length
 * @param antigen_id Output: assigned antigen ID
 * @return 0 on success
 */
int sec_immune_unified_present_bbb_threat(
    sec_immune_unified_bridge_t* bridge,
    bbb_threat_type_t threat_type,
    bbb_severity_t severity,
    const uint8_t* threat_data,
    size_t data_len,
    uint32_t* antigen_id
);

/**
 * @brief Present anomaly detection result as antigen
 *
 * WHAT: Convert anomaly detection to immune antigen
 * WHY:  Enable immune response to behavioral anomalies
 * HOW:  Map score to severity, extract features as epitope
 *
 * @param bridge Bridge handle
 * @param result Anomaly detection result
 * @param antigen_id Output: assigned antigen ID
 * @return 0 on success
 */
int sec_immune_unified_present_anomaly(
    sec_immune_unified_bridge_t* bridge,
    const nimcp_anomaly_result_t* result,
    uint32_t* antigen_id
);

/**
 * @brief Present pattern match as antigen
 *
 * WHAT: Convert pattern match to immune antigen
 * WHY:  Enable immune response to pattern-detected threats
 * HOW:  Map match score to severity, use pattern as epitope
 *
 * @param bridge Bridge handle
 * @param match_result Pattern match result
 * @param antigen_id Output: assigned antigen ID
 * @return 0 on success
 */
int sec_immune_unified_present_pattern_match(
    sec_immune_unified_bridge_t* bridge,
    const nimcp_pattern_match_result_t* match_result,
    uint32_t* antigen_id
);

/**
 * @brief Present rate limit violation as antigen
 *
 * WHAT: Convert rate violation to immune antigen
 * WHY:  Enable immune response to abuse
 * HOW:  Map violation count to severity, client ID to epitope
 *
 * @param bridge Bridge handle
 * @param client_id Violating client ID
 * @param violation_count Number of violations
 * @param antigen_id Output: assigned antigen ID
 * @return 0 on success
 */
int sec_immune_unified_present_rate_violation(
    sec_immune_unified_bridge_t* bridge,
    const char* client_id,
    uint32_t violation_count,
    uint32_t* antigen_id
);

/**
 * @brief Present policy violation as antigen
 *
 * WHAT: Convert policy violation to immune antigen
 * WHY:  Enable immune response to policy breaches
 * HOW:  Map severity, rule name to epitope
 *
 * @param bridge Bridge handle
 * @param result Policy evaluation result
 * @param antigen_id Output: assigned antigen ID
 * @return 0 on success
 */
int sec_immune_unified_present_policy_violation(
    sec_immune_unified_bridge_t* bridge,
    const nimcp_policy_result_t* result,
    uint32_t* antigen_id
);

/* ============================================================================
 * Immune → Security: Antibody Action API
 * ============================================================================ */

/**
 * @brief Execute antibody as security action
 *
 * WHAT: Map antibody to security actions and execute
 * WHY:  Translate immune response to security countermeasures
 * HOW:  Map antibody class to BBB/rate/policy actions, execute
 *
 * @param bridge Bridge handle
 * @param antibody_id Antibody to execute
 * @return 0 on success
 */
int sec_immune_unified_execute_antibody_action(
    sec_immune_unified_bridge_t* bridge,
    uint32_t antibody_id
);

/**
 * @brief Execute killer T cell as quarantine
 *
 * WHAT: Map killer T cell action to security quarantine
 * WHY:  Coordinate T cell response with security isolation
 * HOW:  Trigger BBB quarantine + rate limiter block
 *
 * @param bridge Bridge handle
 * @param t_cell_id Killer T cell ID
 * @param target Target node/client
 * @return 0 on success
 */
int sec_immune_unified_execute_killer_action(
    sec_immune_unified_bridge_t* bridge,
    uint32_t t_cell_id,
    uint32_t target
);

/**
 * @brief Execute helper T cell as coordination
 *
 * WHAT: Map helper T cell to security coordination actions
 * WHY:  Amplify security response via helper coordination
 * HOW:  Escalate policy, broadcast alerts
 *
 * @param bridge Bridge handle
 * @param t_cell_id Helper T cell ID
 * @return 0 on success
 */
int sec_immune_unified_execute_helper_action(
    sec_immune_unified_bridge_t* bridge,
    uint32_t t_cell_id
);

/* ============================================================================
 * Memory Cell API
 * ============================================================================ */

/**
 * @brief Form memory cell from neutralized threat
 *
 * WHAT: Create memory cell from successfully neutralized antigen
 * WHY:  Enable faster secondary response to known threats
 * HOW:  Store epitope, response type, effectiveness
 *
 * @param bridge Bridge handle
 * @param antigen_id Neutralized antigen
 * @param antibody_id Successful antibody
 * @param memory_id Output: new memory cell ID
 * @return 0 on success
 */
int sec_immune_unified_form_memory(
    sec_immune_unified_bridge_t* bridge,
    uint32_t antigen_id,
    uint32_t antibody_id,
    uint32_t* memory_id
);

/**
 * @brief Sync memory cell to pattern database
 *
 * WHAT: Create pattern DB entry from memory cell
 * WHY:  Share learned threats with pattern matching
 * HOW:  Convert epitope to pattern, add to DB
 *
 * @param bridge Bridge handle
 * @param memory_id Memory cell to sync
 * @return 0 on success
 */
int sec_immune_unified_sync_memory_to_pattern(
    sec_immune_unified_bridge_t* bridge,
    uint32_t memory_id
);

/**
 * @brief Check for memory cell match
 *
 * WHAT: Check if pattern matches existing memory cell
 * WHY:  Enable fast secondary response
 * HOW:  Compare against memory cell epitopes
 *
 * @param bridge Bridge handle
 * @param epitope Pattern to match
 * @param epitope_len Pattern length
 * @param memory_id Output: matching memory cell (if found)
 * @return 0 if found, -1 if no match
 */
int sec_immune_unified_check_memory(
    sec_immune_unified_bridge_t* bridge,
    const uint8_t* epitope,
    size_t epitope_len,
    uint32_t* memory_id
);

/**
 * @brief Trigger secondary response from memory
 *
 * WHAT: Fast immune response using memory cell
 * WHY:  Previously seen threat gets rapid response
 * HOW:  Activate memory B cell, produce high-affinity antibody
 *
 * @param bridge Bridge handle
 * @param memory_id Memory cell to activate
 * @param antigen_id Current antigen
 * @return 0 on success
 */
int sec_immune_unified_secondary_response(
    sec_immune_unified_bridge_t* bridge,
    uint32_t memory_id,
    uint32_t antigen_id
);

/* ============================================================================
 * Tolerance System API
 * ============================================================================ */

/**
 * @brief Add pattern to tolerance whitelist
 *
 * WHAT: Whitelist pattern to prevent false positives
 * WHY:  Central tolerance - mark known benign patterns
 * HOW:  Add to whitelist with confirmation tracking
 *
 * @param bridge Bridge handle
 * @param pattern Pattern to whitelist
 * @param pattern_len Pattern length
 * @param description Human-readable description
 * @param is_permanent Permanent whitelist flag
 * @return 0 on success
 */
int sec_immune_unified_add_tolerance(
    sec_immune_unified_bridge_t* bridge,
    const uint8_t* pattern,
    size_t pattern_len,
    const char* description,
    bool is_permanent
);

/**
 * @brief Remove pattern from tolerance whitelist
 *
 * WHAT: Remove pattern from whitelist
 * WHY:  Pattern no longer considered benign
 * HOW:  Find and remove from whitelist
 *
 * @param bridge Bridge handle
 * @param pattern Pattern to remove
 * @param pattern_len Pattern length
 * @return 0 on success
 */
int sec_immune_unified_remove_tolerance(
    sec_immune_unified_bridge_t* bridge,
    const uint8_t* pattern,
    size_t pattern_len
);

/**
 * @brief Check if pattern is tolerated (whitelisted)
 *
 * WHAT: Check pattern against tolerance whitelist
 * WHY:  Prevent false positive on benign patterns
 * HOW:  Search whitelist for match
 *
 * @param bridge Bridge handle
 * @param pattern Pattern to check
 * @param pattern_len Pattern length
 * @return true if tolerated (whitelisted)
 */
bool sec_immune_unified_is_tolerated(
    sec_immune_unified_bridge_t* bridge,
    const uint8_t* pattern,
    size_t pattern_len
);

/**
 * @brief Confirm pattern as benign (increment confirmation count)
 *
 * WHAT: Confirm pattern detection was false positive
 * WHY:  Learn to tolerate through repeated confirmation
 * HOW:  Increment confirmation, auto-whitelist if threshold met
 *
 * @param bridge Bridge handle
 * @param pattern Pattern confirmed benign
 * @param pattern_len Pattern length
 * @return 0 on success
 */
int sec_immune_unified_confirm_benign(
    sec_immune_unified_bridge_t* bridge,
    const uint8_t* pattern,
    size_t pattern_len
);

/**
 * @brief Enable/disable tolerance learning mode
 *
 * WHAT: Control tolerance learning period
 * WHY:  Allow system to learn normal patterns
 * HOW:  Set learning mode flag, start timer
 *
 * @param bridge Bridge handle
 * @param enable Enable or disable learning
 * @return 0 on success
 */
int sec_immune_unified_set_learning_mode(
    sec_immune_unified_bridge_t* bridge,
    bool enable
);

/**
 * @brief Activate regulatory T cell suppression
 *
 * WHAT: Engage regulatory T cells to suppress over-reaction
 * WHY:  Prevent autoimmune-style false positives
 * HOW:  Increase regulatory activity, suppress alerts
 *
 * @param bridge Bridge handle
 * @param suppression_level Suppression level [0-1]
 * @return 0 on success
 */
int sec_immune_unified_activate_regulatory(
    sec_immune_unified_bridge_t* bridge,
    float suppression_level
);

/* ============================================================================
 * Training Feedback API
 * ============================================================================ */

/**
 * @brief Provide feedback that detection was true positive
 *
 * WHAT: Confirm threat detection was accurate
 * WHY:  Improve detection accuracy through feedback
 * HOW:  Update training stats, adjust sensitivity
 *
 * @param bridge Bridge handle
 * @param antigen_id Antigen that was true positive
 * @return 0 on success
 */
int sec_immune_unified_feedback_true_positive(
    sec_immune_unified_bridge_t* bridge,
    uint32_t antigen_id
);

/**
 * @brief Provide feedback that detection was false positive
 *
 * WHAT: Indicate threat detection was incorrect
 * WHY:  Reduce false positive rate through feedback
 * HOW:  Update training stats, relax sensitivity, consider tolerance
 *
 * @param bridge Bridge handle
 * @param antigen_id Antigen that was false positive
 * @return 0 on success
 */
int sec_immune_unified_feedback_false_positive(
    sec_immune_unified_bridge_t* bridge,
    uint32_t antigen_id
);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async messaging system
 * WHY:  Enable cross-module security-immune communication
 * HOW:  Register module, set up message handlers
 *
 * @param bridge Bridge handle
 * @return 0 on success
 */
int sec_immune_unified_connect_bio_async(sec_immune_unified_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister from bio-async
 * WHY:  Clean shutdown
 * HOW:  Unregister module, clear handlers
 *
 * @param bridge Bridge handle
 * @return 0 on success
 */
int sec_immune_unified_disconnect_bio_async(sec_immune_unified_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge handle
 * @return true if connected
 */
bool sec_immune_unified_is_bio_async_connected(
    const sec_immune_unified_bridge_t* bridge
);

/**
 * @brief Broadcast security event via bio-async
 *
 * WHAT: Send security event to all listeners
 * WHY:  Enable system-wide security awareness
 * HOW:  Create bio message, broadcast
 *
 * @param bridge Bridge handle
 * @param event_type Event type identifier
 * @param severity Event severity
 * @param data Event data
 * @param data_len Data length
 * @return 0 on success
 */
int sec_immune_unified_broadcast_security_event(
    sec_immune_unified_bridge_t* bridge,
    uint32_t event_type,
    uint32_t severity,
    const void* data,
    size_t data_len
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current effective BBB threshold factor
 *
 * @param bridge Bridge handle
 * @return Effective BBB threshold factor
 */
float sec_immune_unified_get_bbb_threshold_factor(
    const sec_immune_unified_bridge_t* bridge
);

/**
 * @brief Get current effective anomaly threshold
 *
 * @param bridge Bridge handle
 * @return Effective anomaly threshold
 */
float sec_immune_unified_get_anomaly_threshold(
    const sec_immune_unified_bridge_t* bridge
);

/**
 * @brief Get current effective pattern weight factor
 *
 * @param bridge Bridge handle
 * @return Effective pattern weight factor
 */
float sec_immune_unified_get_pattern_weight_factor(
    const sec_immune_unified_bridge_t* bridge
);

/**
 * @brief Get current effective rate limit factor
 *
 * @param bridge Bridge handle
 * @return Effective rate limit factor
 */
float sec_immune_unified_get_rate_limit_factor(
    const sec_immune_unified_bridge_t* bridge
);

/**
 * @brief Get current effective policy strictness factor
 *
 * @param bridge Bridge handle
 * @return Effective policy strictness factor
 */
float sec_immune_unified_get_policy_strictness_factor(
    const sec_immune_unified_bridge_t* bridge
);

/**
 * @brief Check if emergency mode is active
 *
 * @param bridge Bridge handle
 * @return true if emergency mode active (TNF-a or storm inflammation)
 */
bool sec_immune_unified_is_emergency_mode(
    const sec_immune_unified_bridge_t* bridge
);

/**
 * @brief Check if tolerance learning is active
 *
 * @param bridge Bridge handle
 * @return true if in tolerance learning period
 */
bool sec_immune_unified_is_learning_mode(
    const sec_immune_unified_bridge_t* bridge
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success
 */
int sec_immune_unified_get_stats(
    const sec_immune_unified_bridge_t* bridge,
    sec_immune_unified_stats_t* stats
);

/**
 * @brief Get current threat level
 *
 * WHAT: Compute overall threat level from all sources
 * WHY:  Provide single metric for system threat state
 * HOW:  Aggregate inflammation, active antigens, modulation factors
 *
 * @param bridge Bridge handle
 * @return Threat level [0-1]
 */
float sec_immune_unified_get_threat_level(
    const sec_immune_unified_bridge_t* bridge
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get inflammation level name
 *
 * @param level Inflammation level
 * @return Human-readable name
 */
const char* sec_immune_unified_inflammation_name(brain_inflammation_level_t level);

/**
 * @brief Get BBB threat type name
 *
 * @param type BBB threat type
 * @return Human-readable name
 */
const char* sec_immune_unified_bbb_threat_name(bbb_threat_type_t type);

/**
 * @brief Get antibody class name
 *
 * @param ab_class Antibody class
 * @return Human-readable name
 */
const char* sec_immune_unified_antibody_class_name(brain_antibody_class_t ab_class);

/**
 * @brief Print bridge statistics to stdout
 *
 * @param bridge Bridge handle
 */
void sec_immune_unified_print_stats(const sec_immune_unified_bridge_t* bridge);

/**
 * @brief Print cytokine effects to stdout
 *
 * @param bridge Bridge handle
 */
void sec_immune_unified_print_cytokine_effects(
    const sec_immune_unified_bridge_t* bridge
);

/**
 * @brief Print inflammation state to stdout
 *
 * @param bridge Bridge handle
 */
void sec_immune_unified_print_inflammation_state(
    const sec_immune_unified_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_IMMUNE_UNIFIED_BRIDGE_H */
