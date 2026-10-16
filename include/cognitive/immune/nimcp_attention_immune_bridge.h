/**
 * @file nimcp_attention_immune_bridge.h
 * @brief Attention-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional integration between brain immune system and attention mechanisms
 * WHY:  Biological evidence shows inflammation impairs attention and executive function;
 *       threat-focused attention can modulate local immune activity. Essential for
 *       realistic cognitive-immune modeling.
 * HOW:  Cytokines narrow attention and impair sustained focus; inflammation reduces
 *       attention capacity; threat-focused attention enhances local immune response.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → ATTENTION PATHWAYS:
 * ---------------------------
 * 1. Pro-inflammatory Cytokines (IL-1β, IL-6, TNF-α):
 *    - Cross blood-brain barrier
 *    - Impair prefrontal cortex function
 *    - Reduce sustained attention capacity
 *    - Narrow attentional focus (threat-oriented)
 *    - Impair cognitive flexibility
 *    - Reference: Krabbe et al. (2005) "Brain-derived neurotrophic factor inhibits
 *      tumor necrosis factor-α-induced apoptosis in cerebellar granule neurons"
 *    - Reference: Reichenberg et al. (2001) "Cytokine-associated emotional and
 *      cognitive disturbances in humans"
 *
 * 2. Chronic Inflammation:
 *    - Sustained elevation → attention deficits
 *    - Reduced working memory capacity
 *    - Impaired executive function
 *    - Difficulty disengaging from threats
 *    - Reference: Marsland et al. (2006) "Brain morphology links systemic
 *      inflammation to cognitive function in midlife adults"
 *
 * 3. Inflammation-Induced Attention Narrowing:
 *    - High arousal → narrowed attentional beam
 *    - Focus locked on threat-relevant stimuli
 *    - Impaired ability to shift attention
 *    - Reduced attentional breadth
 *    - Reference: Easterbrook (1959) "The effect of emotion on cue utilization
 *      and the organization of behavior"
 *
 * 4. Cytokine Storm Effects:
 *    - Severe inflammation → delirium
 *    - Profound attention disruption
 *    - Disorientation and confusion
 *    - Reference: Pandharipande et al. (2013) "Inflammation and delirium"
 *
 * ATTENTION → IMMUNE PATHWAYS:
 * ---------------------------
 * 1. Threat-Focused Attention:
 *    - Attention to threats → enhanced local immune vigilance
 *    - Directed attention activates sympathetic nervous system
 *    - Catecholamine release modulates immune cell migration
 *    - Enhanced pathogen recognition at attended sites
 *    - Reference: Elenkov et al. (2000) "The sympathetic nerve - an integrative
 *      interface between two supersystems"
 *
 * 2. Sustained Attention and Immune Function:
 *    - Attention training → improved immune markers
 *    - Mindfulness attention → reduced inflammation
 *    - Attentional control → better immune regulation
 *    - Reference: Davidson et al. (2003) "Alterations in brain and immune
 *      function produced by mindfulness meditation"
 *
 * 3. Attention-Mediated Stress Response:
 *    - Hypervigilance → chronic immune activation
 *    - Threat monitoring → elevated inflammatory markers
 *    - Attention bias to threat → sustained cortisol elevation
 *    - Reference: Brosschot et al. (2006) "The perseverative cognition hypothesis"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    ATTENTION-IMMUNE BRIDGE                                 ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  IMMUNE → ATTENTION PATHWAYS                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β → -0.3 │  ───────┐                                       │  ║
 * ║   │   │ IL-6  → -0.2 │         │                                       │  ║
 * ║   │   │ TNF-α → -0.4 │         ├──→ Attention Impairment               │  ║
 * ║   │   │              │         │    (Narrowing, Deficits)              │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │     ATTENTION SYSTEM            │                             │  ║
 * ║   │   │  - Width narrowing              │                             │  ║
 * ║   │   │  - Sustained capacity reduction │                             │  ║
 * ║   │   │  - Executive impairment         │                             │  ║
 * ║   │   │  - Threat bias                  │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────────┐                                     │  ║
 * ║   │   │   INFLAMMATION LEVEL     │                                     │  ║
 * ║   │   │ ──────────────────────── │                                     │  ║
 * ║   │   │ LOCAL    → -10% capacity │                                     │  ║
 * ║   │   │ REGIONAL → -30% capacity │                                     │  ║
 * ║   │   │ SYSTEMIC → -50% capacity │                                     │  ║
 * ║   │   │ STORM    → -80% capacity │                                     │  ║
 * ║   │   └──────────────────────────┘                                     │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  ATTENTION → IMMUNE PATHWAYS                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ THREAT FOCUS │ ──→ Enhanced Local Immune Response              │  ║
 * ║   │   │ VIGILANCE    │ ──→ Increased Immune Surveillance               │  ║
 * ║   │   │ HYPERVIGILANCE│──→ Chronic Inflammation                        │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ CALM FOCUS   │ ──→ Reduced Inflammation                        │  ║
 * ║   │   │ MINDFUL ATT. │ ──→ IL-10 Release                               │  ║
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

#ifndef NIMCP_ATTENTION_IMMUNE_BRIDGE_H
#define NIMCP_ATTENTION_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/attention/nimcp_emotion_attention.h"
#include "plasticity/attention/nimcp_attention.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine attention impact factors */
#define CYTOKINE_IL1_ATTENTION_IMPACT      -0.3f   /**< IL-1β → attention deficit */
#define CYTOKINE_IL6_ATTENTION_IMPACT      -0.2f   /**< IL-6 → attention deficit */
#define CYTOKINE_TNF_ATTENTION_IMPACT      -0.4f   /**< TNF-α → strong deficit */
#define CYTOKINE_IFN_GAMMA_ATTENTION_IMPACT -0.15f /**< IFN-γ → mild deficit */
#define CYTOKINE_IL10_ATTENTION_IMPACT      0.2f   /**< IL-10 → recovery boost */

/* Inflammation attention capacity reduction */
#define INFLAMMATION_NONE_CAPACITY_FACTOR     1.0f   /**< No reduction */
#define INFLAMMATION_LOCAL_CAPACITY_FACTOR    0.9f   /**< -10% capacity */
#define INFLAMMATION_REGIONAL_CAPACITY_FACTOR 0.7f   /**< -30% capacity */
#define INFLAMMATION_SYSTEMIC_CAPACITY_FACTOR 0.5f   /**< -50% capacity */
#define INFLAMMATION_STORM_CAPACITY_FACTOR    0.2f   /**< -80% capacity (delirium) */

/* Inflammation attention narrowing factors */
#define INFLAMMATION_NARROWING_BASE       0.1f    /**< Base narrowing effect */
#define INFLAMMATION_NARROWING_PER_LEVEL  0.15f   /**< Per inflammation level */

/* Threat attention immune boost */
#define THREAT_ATTENTION_IMMUNE_BOOST     0.3f    /**< Threat focus → +30% immune */
#define VIGILANCE_IMMUNE_THRESHOLD        0.7f    /**< High attention → immune activation */
#define HYPERVIGILANCE_INFLAMMATION_RATE  0.05f   /**< Chronic hypervigilance → inflammation */

/* Sustained attention thresholds */
#define SUSTAINED_ATTENTION_DURATION_SEC  30.0f   /**< Duration for "sustained" */
#define MINDFUL_ATTENTION_IL10_BOOST      0.25f   /**< Calm focus → IL-10 */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine attention effects
 *
 * Represents how cytokine levels impair attention
 */
typedef struct {
    /* Pro-inflammatory effects */
    float il1_attention_deficit;        /**< IL-1β induced deficit */
    float il6_attention_deficit;        /**< IL-6 induced deficit */
    float tnf_attention_deficit;        /**< TNF-α induced deficit */
    float ifn_gamma_attention_deficit;  /**< IFN-γ induced deficit */

    /* Anti-inflammatory effects */
    float il10_attention_recovery;      /**< IL-10 recovery boost */

    /* Aggregate effects */
    float total_capacity_reduction;     /**< Combined capacity loss [0-1] */
    float narrowing_factor;             /**< Attention narrowing [0-1] */
    float sustained_impairment;         /**< Sustained attention deficit [0-1] */
    float executive_impairment;         /**< Executive function deficit [0-1] */
} cytokine_attention_effects_t;

/**
 * @brief Inflammation attention state
 *
 * How chronic inflammation affects attention
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;    /**< How long inflamed */
    bool is_chronic;                    /**< >= threshold */

    /* Attention impacts */
    float capacity_factor;              /**< Overall capacity [0-1] */
    float width_narrowing;              /**< Attentional narrowing [0-1] */
    float sustained_deficit;            /**< Sustained attention loss [0-1] */
    float flexibility_impairment;       /**< Difficulty shifting [0-1] */
    float working_memory_deficit;       /**< WM capacity loss [0-1] */

    /* Threat bias */
    float threat_bias_strength;         /**< Bias toward threats [0-1] */
    float disengagement_difficulty;     /**< Can't shift from threats [0-1] */
} inflammation_attention_state_t;

/**
 * @brief Attention-driven immune modulation
 *
 * How attention state affects immune function
 */
typedef struct {
    /* Attention state */
    float attention_strength;           /**< Current attention intensity [0-1] */
    float threat_focus_level;           /**< Focus on threats [0-1] */
    float vigilance_level;              /**< Vigilance/alertness [0-1] */
    float sustained_duration_sec;       /**< How long sustained */

    /* Immune effects */
    float local_immune_boost;           /**< Enhanced local response [0-1] */
    float immune_surveillance_boost;    /**< Increased vigilance [0-1] */
    bool hypervigilance_inflammation;   /**< Chronic stress inflammation */

    /* Mindful attention effects */
    float mindful_attention_level;      /**< Calm, focused attention [0-1] */
    float il10_release_from_mindfulness; /**< IL-10 from calm focus */
    float inflammation_reduction;       /**< Reduced inflammation [0-1] */
} attention_immune_modulation_t;

/**
 * @brief Complete attention-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    emotion_attention_system_t* emotion_attention;
    multihead_attention_t multihead_attention;

    /* Current state */
    cytokine_attention_effects_t cytokine_effects;
    inflammation_attention_state_t inflammation_state;
    attention_immune_modulation_t attention_modulation;

    /* Integration flags */
    bool enable_cytokine_attention_impairment;
    bool enable_inflammation_narrowing;
    bool enable_threat_attention_immune_boost;
    bool enable_mindful_attention_benefits;
    bool enable_hypervigilance_inflammation;

    /* Timing */
    uint64_t last_update_time;
    float hypervigilance_accumulator;   /**< Accumulated hypervigilance time */

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_impairments;
    uint32_t threat_boosts;
    uint32_t mindful_boosts;
    uint32_t hypervigilance_inflammation_events;

    } attention_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_attention_impairment;
    bool enable_inflammation_narrowing;
    bool enable_threat_attention_immune_boost;
    bool enable_mindful_attention_benefits;
    bool enable_hypervigilance_inflammation;

    /* Sensitivity tuning */
    float cytokine_sensitivity;         /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;     /**< Inflammation effect multiplier [0.5-2.0] */
    float attention_immune_sensitivity; /**< Attention→immune multiplier [0.5-2.0] */

    /* Thresholds */
    float vigilance_threshold;          /**< Vigilance for immune boost [0.5-0.9] */
    float hypervigilance_threshold;     /**< Hypervigilance for inflammation [0.7-0.95] */
    float mindful_threshold;            /**< Mindful attention threshold [0.4-0.7] */
} attention_immune_config_t;

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
int attention_immune_default_config(attention_immune_config_t* config);

/**
 * @brief Create attention-immune bridge
 *
 * WHAT: Initialize bidirectional attention-immune integration
 * WHY:  Enable realistic immune-attention coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param emotion_attention Emotion-attention system (optional, can be NULL)
 * @param multihead_attention Multihead attention (optional, can be NULL)
 * @return New bridge or NULL on failure
 */
attention_immune_bridge_t* attention_immune_bridge_create(
    const attention_immune_config_t* config,
    brain_immune_system_t* immune_system,
    emotion_attention_system_t* emotion_attention,
    multihead_attention_t multihead_attention
);

/**
 * @brief Destroy attention-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void attention_immune_bridge_destroy(attention_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Attention API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to attention
 *
 * WHAT: Impair attention based on cytokine levels
 * WHY:  Pro-inflammatory cytokines reduce attention capacity
 * HOW:  Query immune system cytokines, reduce attention width/capacity
 *
 * @param bridge Attention-immune bridge
 * @return 0 on success
 */
int attention_immune_apply_cytokine_effects(attention_immune_bridge_t* bridge);

/**
 * @brief Apply inflammation effects to attention
 *
 * WHAT: Narrow attention and impair sustained focus from inflammation
 * WHY:  Inflammation reduces cognitive capacity and narrows focus
 * HOW:  Check inflammation level/duration, adjust attention parameters
 *
 * @param bridge Attention-immune bridge
 * @return 0 on success
 */
int attention_immune_apply_inflammation_effects(attention_immune_bridge_t* bridge);

/**
 * @brief Compute attention capacity from immune state
 *
 * WHAT: Calculate overall attention capacity given immune status
 * WHY:  Inflammation reduces cognitive resources
 * HOW:  Map inflammation level to capacity factor [0-1]
 *
 * @param bridge Attention-immune bridge
 * @return Capacity factor [0-1] (1.0 = normal, 0.0 = complete impairment)
 */
float attention_immune_compute_capacity(const attention_immune_bridge_t* bridge);

/**
 * @brief Compute attention narrowing from inflammation
 *
 * WHAT: Calculate how much inflammation narrows attentional beam
 * WHY:  High arousal/inflammation narrows focus to threat-relevant stimuli
 * HOW:  Map inflammation level to narrowing factor
 *
 * @param bridge Attention-immune bridge
 * @return Narrowing factor [0-1] (0 = normal width, 1 = maximally narrowed)
 */
float attention_immune_compute_narrowing(const attention_immune_bridge_t* bridge);

/* ============================================================================
 * Attention → Immune API
 * ============================================================================ */

/**
 * @brief Boost local immune response from threat-focused attention
 *
 * WHAT: Enhance immune vigilance when attention focused on threats
 * WHY:  Directed attention activates sympathetic system, modulates immunity
 * HOW:  Query attention state, boost immune surveillance if threat-focused
 *
 * @param bridge Attention-immune bridge
 * @return 0 on success
 */
int attention_immune_boost_from_threat_focus(attention_immune_bridge_t* bridge);

/**
 * @brief Trigger inflammation from chronic hypervigilance
 *
 * WHAT: Activate inflammatory response from sustained high vigilance
 * WHY:  Chronic attention to threats → sustained stress → inflammation
 * HOW:  Track vigilance duration, trigger cytokine release if chronic
 *
 * @param bridge Attention-immune bridge
 * @return 0 on success
 */
int attention_immune_trigger_hypervigilance_inflammation(attention_immune_bridge_t* bridge);

/**
 * @brief Release IL-10 from mindful attention
 *
 * WHAT: Trigger anti-inflammatory response from calm, focused attention
 * WHY:  Mindfulness reduces inflammation and promotes recovery
 * HOW:  Detect calm, sustained attention; release IL-10 cytokine
 *
 * @param bridge Attention-immune bridge
 * @return 0 on success
 */
int attention_immune_release_il10_from_mindfulness(attention_immune_bridge_t* bridge);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update attention-immune bridge (both directions)
 *
 * WHAT: Process all attention-immune interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine effects, trigger immune from attention, adjust parameters
 *
 * @param bridge Attention-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int attention_immune_bridge_update(
    attention_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine attention effects
 *
 * @param bridge Attention-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int attention_immune_get_cytokine_effects(
    const attention_immune_bridge_t* bridge,
    cytokine_attention_effects_t* effects
);

/**
 * @brief Get current inflammation attention state
 *
 * @param bridge Attention-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int attention_immune_get_inflammation_state(
    const attention_immune_bridge_t* bridge,
    inflammation_attention_state_t* state
);

/**
 * @brief Check if experiencing attention deficit from inflammation
 *
 * WHAT: Determine if inflammation causing significant attention impairment
 * WHY:  Detect clinically significant cognitive effects
 * HOW:  Check capacity reduction threshold
 *
 * @param bridge Attention-immune bridge
 * @return true if significant deficit (>30% capacity loss)
 */
bool attention_immune_has_attention_deficit(const attention_immune_bridge_t* bridge);

/**
 * @brief Get current attention capacity factor
 *
 * @param bridge Attention-immune bridge
 * @return Capacity factor [0-1]
 */
float attention_immune_get_capacity_factor(const attention_immune_bridge_t* bridge);

/**
 * @brief Get current attention width narrowing
 *
 * @param bridge Attention-immune bridge
 * @return Narrowing factor [0-1]
 */
float attention_immune_get_narrowing_factor(const attention_immune_bridge_t* bridge);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_ATTENTION
 *
 * @param bridge Attention-immune bridge
 * @return 0 on success, -1 on error
 */
int attention_immune_connect_bio_async(attention_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge Attention-immune bridge
 * @return 0 on success
 */
int attention_immune_disconnect_bio_async(attention_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Attention-immune bridge
 * @return true if connected
 */
bool attention_immune_is_bio_async_connected(const attention_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ATTENTION_IMMUNE_BRIDGE_H */
