/**
 * @file nimcp_portia_learning_immune_bridge.h
 * @brief Portia Learning-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between brain immune system and Portia learning mechanisms
 * WHY:  Biological evidence shows inflammation impairs learning and memory formation;
 *       learning experiences can modulate immune function (stress vs. reward learning).
 * HOW:  Cytokines reduce learning rates and memory consolidation; learning failures
 *       trigger stress immune responses while successful learning reduces inflammation.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → LEARNING PATHWAYS:
 * --------------------------
 * 1. Pro-inflammatory Cytokines (IL-1β, IL-6, TNF-α):
 *    - Impair long-term potentiation (LTP)
 *    - Reduce learning rate and plasticity
 *    - Impair memory consolidation
 *    - Enhance fear/aversive learning (adaptive)
 *    - Reference: Yirmiya & Goshen (2011) "Immune modulation of learning, memory,
 *      neural plasticity and neurogenesis"
 *    - Reference: Barrientos et al. (2006) "Time course of hippocampal IL-1β and
 *      memory consolidation impairments after lipopolysaccharide treatment"
 *
 * 2. Fever and Inflammation:
 *    - Reduced habituation (persistent responses)
 *    - Impaired extinction learning
 *    - Slower association formation
 *    - Enhanced sensitization to threats
 *    - Reference: Hennessy et al. (2010) "Proinflammatory activity and the
 *      sensitization of depressive-like behavior during maternal separation"
 *
 * 3. Chronic Inflammation:
 *    - Long-term learning deficits
 *    - Impaired reversal learning
 *    - Difficulty forming new associations
 *    - Preserved or enhanced aversive learning
 *
 * LEARNING → IMMUNE PATHWAYS:
 * --------------------------
 * 1. Learning Failures/Frustration:
 *    - Repeated failure → stress response
 *    - IL-6 release (frustration signaling)
 *    - Low-level inflammation
 *    - Enhanced immune vigilance
 *    - Reference: Learning-induced stress activates HPA axis
 *
 * 2. Successful Learning:
 *    - Reward learning → dopamine → immune modulation
 *    - IL-10 release (anti-inflammatory)
 *    - Reduced inflammation
 *    - Enhanced immune regulation
 *    - Reference: Irwin & Cole (2011) "Reciprocal regulation of the neural and
 *      innate immune systems"
 *
 * 3. Associative Learning (Threat):
 *    - Threat associations → enhanced immune response
 *    - Conditioned immune activation
 *    - Preparatory immune mobilization
 *    - Classical conditioning of immune responses
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                PORTIA LEARNING-IMMUNE BRIDGE                               ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   IMMUNE → LEARNING                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β → -0.4 │  ───────┐                                       │  ║
 * ║   │   │ IL-6  → -0.3 │         │                                       │  ║
 * ║   │   │ TNF-α → -0.5 │         ├──→ Learning Rate Reduction            │  ║
 * ║   │   │              │         │    Consolidation Impairment           │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │     LEARNING SYSTEM             │                             │  ║
 * ║   │   │  - Reduced learning rate        │                             │  ║
 * ║   │   │  - Impaired consolidation       │                             │  ║
 * ║   │   │  - Enhanced aversive learning   │                             │  ║
 * ║   │   │  - Slower habituation           │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────────┐                                     │  ║
 * ║   │   │   INFLAMMATION LEVEL     │                                     │  ║
 * ║   │   │ ──────────────────────── │                                     │  ║
 * ║   │   │ LOCAL    → -20% LR       │                                     │  ║
 * ║   │   │ REGIONAL → -40% LR       │                                     │  ║
 * ║   │   │ SYSTEMIC → -60% LR       │                                     │  ║
 * ║   │   │ STORM    → -85% LR       │                                     │  ║
 * ║   │   └──────────────────────────┘                                     │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   LEARNING → IMMUNE                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │LEARNING FAIL │──→ IL-6 Release (Frustration)                   │  ║
 * ║   │   │REPEATED FAIL │──→ Chronic Low Inflammation                     │  ║
 * ║   │   │LOW SUCCESS   │──→ Immune Stress Response                       │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │SUCCESS/REWARD│──→ IL-10 Release (Anti-inflammatory)            │  ║
 * ║   │   │CONSOLIDATION │──→ Reduced Inflammation                         │  ║
 * ║   │   │MASTERY       │──→ Enhanced Immune Regulation                   │  ║
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

#ifndef NIMCP_PORTIA_LEARNING_IMMUNE_BRIDGE_H
#define NIMCP_PORTIA_LEARNING_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "portia/nimcp_portia_learning.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine learning rate impact factors */
#define CYTOKINE_IL1_LR_IMPAIRMENT           -0.4f   /**< IL-1β → LR reduction */
#define CYTOKINE_IL6_LR_IMPAIRMENT           -0.3f   /**< IL-6 → LR reduction */
#define CYTOKINE_TNF_LR_IMPAIRMENT           -0.5f   /**< TNF-α → strong LR reduction */
#define CYTOKINE_IFN_GAMMA_LR_IMPAIRMENT     -0.25f  /**< IFN-γ → moderate LR reduction */
#define CYTOKINE_IL10_LR_RECOVERY            0.2f    /**< IL-10 → LR recovery */

/* Inflammation learning rate reduction */
#define INFLAMMATION_NONE_LR_FACTOR          1.0f    /**< No reduction */
#define INFLAMMATION_LOCAL_LR_FACTOR         0.8f    /**< -20% LR */
#define INFLAMMATION_REGIONAL_LR_FACTOR      0.6f    /**< -40% LR */
#define INFLAMMATION_SYSTEMIC_LR_FACTOR      0.4f    /**< -60% LR */
#define INFLAMMATION_STORM_LR_FACTOR         0.15f   /**< -85% LR */

/* Inflammation consolidation impairment */
#define INFLAMMATION_CONSOLIDATION_BASE      0.1f    /**< Base impairment */
#define INFLAMMATION_CONSOLIDATION_PER_LEVEL 0.15f   /**< Per inflammation level */

/* Learning-triggered immune thresholds */
#define LEARNING_FAILURE_THRESHOLD           0.3f    /**< Low success → frustration */
#define LEARNING_SUCCESS_THRESHOLD           0.7f    /**< High success → reward */
#define LEARNING_REPEATED_FAILURE_COUNT      5       /**< Failures before inflammation */

/* Learning immune response rates */
#define LEARNING_FAILURE_IL6_RELEASE         0.15f   /**< Failure → IL-6 */
#define LEARNING_SUCCESS_IL10_RELEASE        0.2f    /**< Success → IL-10 */
#define LEARNING_REPEATED_FAIL_INFLAMMATION  0.1f    /**< Chronic failure → inflammation */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine learning effects
 *
 * How cytokine levels impair learning
 */
typedef struct {
    /* Pro-inflammatory effects */
    float il1_lr_reduction;              /**< IL-1β learning rate loss */
    float il6_lr_reduction;              /**< IL-6 learning rate loss */
    float tnf_lr_reduction;              /**< TNF-α learning rate loss */
    float ifn_gamma_lr_reduction;        /**< IFN-γ learning rate loss */

    /* Anti-inflammatory effects */
    float il10_lr_recovery;              /**< IL-10 LR recovery boost */

    /* Aggregate effects */
    float total_lr_factor;               /**< Combined LR multiplier [0-1] */
    float consolidation_impairment;      /**< Memory consolidation loss [0-1] */
    float habituation_slowdown;          /**< Habituation rate reduction [0-1] */
    float aversive_learning_boost;       /**< Fear learning enhancement [0-1] */
} cytokine_learning_effects_t;

/**
 * @brief Inflammation learning state
 *
 * How chronic inflammation affects learning
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;     /**< How long inflamed */
    bool is_chronic;                     /**< >= threshold */

    /* Learning impacts */
    float lr_factor;                     /**< Overall LR multiplier [0-1] */
    float consolidation_impairment;      /**< Consolidation deficit [0-1] */
    float ltp_impairment;                /**< LTP deficit [0-1] */
    float extinction_impairment;         /**< Extinction learning deficit [0-1] */
    float reversal_learning_deficit;     /**< Reversal learning impairment [0-1] */

    /* Learning biases */
    float aversive_bias;                 /**< Threat learning bias [0-1] */
    float reward_learning_deficit;       /**< Reward learning impairment [0-1] */
} inflammation_learning_state_t;

/**
 * @brief Learning-driven immune modulation
 *
 * How learning state affects immune function
 */
typedef struct {
    /* Learning state */
    float learning_success_rate;         /**< Recent success rate [0-1] */
    uint32_t consecutive_failures;       /**< Consecutive failures */
    uint32_t consecutive_successes;      /**< Consecutive successes */
    float frustration_level;             /**< Frustration from failures [0-1] */

    /* Immune effects */
    float failure_stress_level;          /**< Failure stress [0-1] */
    float success_reward_level;          /**< Success reward [0-1] */
    float chronic_failure_inflammation;  /**< Chronic failure → inflammation [0-1] */

    /* Cytokine releases */
    float il6_release_from_failure;      /**< IL-6 from failure */
    float il10_release_from_success;     /**< IL-10 from success */
    bool repeated_failure_inflammation;  /**< Chronic frustration inflammation */
} learning_immune_modulation_t;

/**
 * @brief Learning-immune bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_learning_impairment;
    bool enable_inflammation_lr_reduction;
    bool enable_learning_failure_immune_trigger;
    bool enable_learning_success_immune_benefit;
    bool enable_repeated_failure_inflammation;

    /* Sensitivity tuning */
    float cytokine_sensitivity;          /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;      /**< Inflammation effect multiplier [0.5-2.0] */
    float learning_immune_sensitivity;   /**< Learning→immune multiplier [0.5-2.0] */

    /* Thresholds */
    float failure_threshold;             /**< Failure threshold [0.2-0.4] */
    float success_threshold;             /**< Success threshold [0.6-0.8] */
    uint32_t repeated_failure_count;     /**< Failures before inflammation [3-10] */
} portia_learning_immune_config_t;

/**
 * @brief Complete learning-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    portia_learning_state_t* learning_system;

    /* Configuration */
    portia_learning_immune_config_t config;

    /* Current state */
    cytokine_learning_effects_t cytokine_effects;
    inflammation_learning_state_t inflammation_state;
    learning_immune_modulation_t learning_modulation;

    /* Timing */
    uint64_t last_update_time;
    float failure_accumulator;           /**< Accumulated failure stress */

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_impairments;
    uint32_t failure_immune_triggers;
    uint32_t success_immune_benefits;
    uint32_t repeated_failure_inflammations;

    } portia_learning_immune_bridge_t;

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
int portia_learning_immune_default_config(portia_learning_immune_config_t* config);

/**
 * @brief Create learning-immune bridge
 *
 * WHAT: Initialize bidirectional learning-immune integration
 * WHY:  Enable realistic immune-learning coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param learning_system Portia learning system
 * @return New bridge or NULL on failure
 */
portia_learning_immune_bridge_t* portia_learning_immune_create(
    const portia_learning_immune_config_t* config,
    brain_immune_system_t* immune_system,
    portia_learning_state_t* learning_system
);

/**
 * @brief Destroy learning-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void portia_learning_immune_destroy(portia_learning_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Learning API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to learning
 *
 * WHAT: Reduce learning rate and consolidation based on cytokines
 * WHY:  Pro-inflammatory cytokines impair plasticity and LTP
 * HOW:  Query immune system cytokines, adjust learning parameters
 *
 * @param bridge Learning-immune bridge
 * @return 0 on success
 */
int portia_learning_immune_apply_cytokine_effects(portia_learning_immune_bridge_t* bridge);

/**
 * @brief Apply inflammation effects to learning
 *
 * WHAT: Reduce learning rate and consolidation from inflammation
 * WHY:  Inflammation impairs memory formation and plasticity
 * HOW:  Check inflammation level/duration, adjust learning parameters
 *
 * @param bridge Learning-immune bridge
 * @return 0 on success
 */
int portia_learning_immune_apply_inflammation_effects(portia_learning_immune_bridge_t* bridge);

/**
 * @brief Compute learning rate factor from immune state
 *
 * WHAT: Calculate LR multiplier given immune status
 * WHY:  Inflammation reduces learning capacity
 * HOW:  Map inflammation level to LR factor [0-1]
 *
 * @param bridge Learning-immune bridge
 * @return LR factor [0-1] (1.0 = normal, 0.0 = complete impairment)
 */
float portia_learning_immune_compute_lr_factor(const portia_learning_immune_bridge_t* bridge);

/**
 * @brief Compute consolidation impairment from inflammation
 *
 * WHAT: Calculate how much inflammation impairs memory consolidation
 * WHY:  Inflammation disrupts long-term memory formation
 * HOW:  Map inflammation level to consolidation impairment
 *
 * @param bridge Learning-immune bridge
 * @return Consolidation impairment [0-1] (0 = no impairment, 1 = complete block)
 */
float portia_learning_immune_compute_consolidation_impairment(
    const portia_learning_immune_bridge_t* bridge
);

/* ============================================================================
 * Learning → Immune API
 * ============================================================================ */

/**
 * @brief Trigger immune response from learning failures
 *
 * WHAT: Activate stress immune response from repeated failures
 * WHY:  Learning frustration activates stress/inflammatory cascade
 * HOW:  Track failures, release IL-6 if frustrated
 *
 * @param bridge Learning-immune bridge
 * @return 0 on success
 */
int portia_learning_immune_trigger_failure_response(portia_learning_immune_bridge_t* bridge);

/**
 * @brief Release IL-10 from learning success
 *
 * WHAT: Trigger anti-inflammatory response from successful learning
 * WHY:  Reward learning reduces inflammation (positive feedback)
 * HOW:  Detect high success rate, release IL-10
 *
 * @param bridge Learning-immune bridge
 * @return 0 on success
 */
int portia_learning_immune_release_il10_from_success(portia_learning_immune_bridge_t* bridge);

/**
 * @brief Trigger inflammation from repeated failures
 *
 * WHAT: Activate chronic inflammation from sustained frustration
 * WHY:  Chronic learning failure → chronic stress → inflammation
 * HOW:  Track consecutive failures, trigger inflammation if threshold exceeded
 *
 * @param bridge Learning-immune bridge
 * @return 0 on success
 */
int portia_learning_immune_trigger_repeated_failure_inflammation(
    portia_learning_immune_bridge_t* bridge
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update learning-immune bridge (both directions)
 *
 * WHAT: Process all learning-immune interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine effects, trigger immune from learning, adjust parameters
 *
 * @param bridge Learning-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int portia_learning_immune_update(portia_learning_immune_bridge_t* bridge, uint64_t delta_ms);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine learning effects
 *
 * @param bridge Learning-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int portia_learning_immune_get_cytokine_effects(
    const portia_learning_immune_bridge_t* bridge,
    cytokine_learning_effects_t* effects
);

/**
 * @brief Get current inflammation learning state
 *
 * @param bridge Learning-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int portia_learning_immune_get_inflammation_state(
    const portia_learning_immune_bridge_t* bridge,
    inflammation_learning_state_t* state
);

/**
 * @brief Check if experiencing learning deficit from inflammation
 *
 * WHAT: Determine if inflammation causing significant learning impairment
 * WHY:  Detect clinically significant cognitive effects
 * HOW:  Check LR reduction threshold
 *
 * @param bridge Learning-immune bridge
 * @return true if significant impairment (>40% LR loss)
 */
bool portia_learning_immune_has_learning_deficit(const portia_learning_immune_bridge_t* bridge);

/**
 * @brief Get current learning rate factor
 *
 * @param bridge Learning-immune bridge
 * @return LR factor [0-1]
 */
float portia_learning_immune_get_lr_factor(const portia_learning_immune_bridge_t* bridge);

/**
 * @brief Get current consolidation impairment
 *
 * @param bridge Learning-immune bridge
 * @return Consolidation impairment [0-1]
 */
float portia_learning_immune_get_consolidation_impairment(
    const portia_learning_immune_bridge_t* bridge
);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_PORTIA_LEARNING
 *
 * @param bridge Learning-immune bridge
 * @return 0 on success, -1 on error
 */
int portia_learning_immune_connect_bio_async(portia_learning_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge Learning-immune bridge
 * @return 0 on success
 */
int portia_learning_immune_disconnect_bio_async(portia_learning_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Learning-immune bridge
 * @return true if connected
 */
bool portia_learning_immune_is_bio_async_connected(const portia_learning_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PORTIA_LEARNING_IMMUNE_BRIDGE_H */
