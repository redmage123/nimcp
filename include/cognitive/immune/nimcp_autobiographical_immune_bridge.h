/**
 * @file nimcp_autobiographical_immune_bridge.h
 * @brief Autobiographical Memory-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional integration between brain immune system and autobiographical memory
 * WHY:  Biological evidence shows immune-memory coupling (IL-1β impairs encoding,
 *       inflammation affects hippocampus, sickness creates distinct memories).
 * HOW:  Cytokines modulate episodic encoding rate and emotional salience,
 *       trauma memories trigger inflammatory responses, sickness episodes become landmarks.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → MEMORY PATHWAYS:
 * -------------------------
 * 1. Pro-inflammatory Cytokines (IL-1β, IL-6, TNF-α):
 *    - IL-1β impairs hippocampal long-term potentiation (LTP)
 *    - Reduces episodic memory encoding efficiency
 *    - Impairs contextual memory formation
 *    - Enhances emotional salience of negative memories
 *    - Reference: Yirmiya & Goshen (2011) "Immune modulation of learning, memory"
 *
 * 2. Chronic Inflammation:
 *    - Accelerates memory decline and cognitive aging
 *    - Reduces hippocampal neurogenesis
 *    - Impairs memory consolidation during sleep
 *    - Increases false memory formation
 *    - Reference: Marsland et al. (2006) "Inflammation and memory"
 *
 * 3. Sickness Episodes:
 *    - Create distinct autobiographical landmarks
 *    - Enhanced memory for emotional events during sickness
 *    - Temporal context marker ("when I was sick")
 *    - Preserved as episodic "crisis" memories
 *    - Reference: Harrison et al. (2015) "Inflammation causes mood changes"
 *
 * 4. Anti-inflammatory Cytokines (IL-10):
 *    - Restore normal memory encoding
 *    - Promote memory consolidation
 *    - Reduce stress-induced memory impairment
 *    - Reference: Khairova et al. (2009) "Anti-inflammatory cytokines"
 *
 * MEMORY → IMMUNE PATHWAYS:
 * -------------------------
 * 1. Trauma Memory Recall:
 *    - Autobiographical trauma memories trigger HPA axis
 *    - Cortisol release → initial immune suppression
 *    - Followed by inflammatory rebound
 *    - Reference: Segerstrom & Miller (2004) "Stress and immune system"
 *
 * 2. Negative Autobiographical Memories:
 *    - High-importance negative memories → chronic stress
 *    - Rumination on failures → sustained inflammation
 *    - Identity-threatening memories → immune dysregulation
 *    - Reference: Kiecolt-Glaser et al. (2002) "Psychoneuroimmunology"
 *
 * 3. Positive Memory Retrieval:
 *    - Recalling achievements → reduced cortisol
 *    - Positive autobiographical memories → enhanced immunity
 *    - Social bond memories → immune benefits
 *    - Reference: Pressman & Cohen (2005) "Positive affect and health"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║           AUTOBIOGRAPHICAL MEMORY-IMMUNE BRIDGE                            ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │               IMMUNE → MEMORY PATHWAYS                              │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β → -50% │  ───────┐                                       │  ║
 * ║   │   │ encoding     │         │                                       │  ║
 * ║   │   │ IL-6  → -30% │         ├──→ Episodic Encoding Impairment       │  ║
 * ║   │   │ TNF-α → -40% │         │                                       │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │  AUTOBIOGRAPHICAL MEMORY        │                             │  ║
 * ║   │   │  - Encoding rate reduced        │                             │  ║
 * ║   │   │  - Emotional salience enhanced  │                             │  ║
 * ║   │   │  - Sickness landmarks created   │                             │  ║
 * ║   │   │  - Consolidation impaired       │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                            ▲                                       │  ║
 * ║   │   ┌──────────────┐         │                                       │  ║
 * ║   │   │ INFLAMMATION │         │                                       │  ║
 * ║   │   │   SYSTEMIC   │  ───────┘                                       │  ║
 * ║   │   │ → Sickness   │     Creates Crisis Landmark                     │  ║
 * ║   │   │   Landmark   │                                                 │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │               MEMORY → IMMUNE PATHWAYS                              │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ TRAUMA       │ ──→ Cortisol → Inflammatory Rebound             │  ║
 * ║   │   │ FAILURE      │ ──→ Chronic Stress → Immune Dysregulation       │  ║
 * ║   │   │ CRISIS       │ ──→ HPA Activation                              │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ ACHIEVEMENT  │ ──→ Reduced Cortisol                            │  ║
 * ║   │   │ LEARNING     │ ──→ Enhanced Immunity                           │  ║
 * ║   │   │ SOCIAL BONDS │ ──→ IL-10 Release                               │  ║
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

#ifndef NIMCP_AUTOBIOGRAPHICAL_IMMUNE_BRIDGE_H
#define NIMCP_AUTOBIOGRAPHICAL_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/nimcp_autobiographical_memory.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine episodic encoding impact factors */
#define CYTOKINE_IL1_ENCODING_IMPACT      -0.5f   /**< IL-1β → 50% encoding impairment */
#define CYTOKINE_IL6_ENCODING_IMPACT      -0.3f   /**< IL-6 → 30% encoding impairment */
#define CYTOKINE_TNF_ENCODING_IMPACT      -0.4f   /**< TNF-α → 40% encoding impairment */
#define CYTOKINE_IFN_GAMMA_ENCODING_IMPACT -0.2f  /**< IFN-γ → 20% encoding impairment */
#define CYTOKINE_IL10_ENCODING_BOOST       0.2f   /**< IL-10 → 20% encoding boost (recovery) */

/* Emotional salience modulation */
#define INFLAMMATION_NEGATIVE_SALIENCE_BOOST  0.3f  /**< Inflammation enhances negative memory salience */
#define INFLAMMATION_POSITIVE_SALIENCE_REDUCE -0.2f /**< Inflammation reduces positive memory salience */

/* Memory-triggered immune thresholds */
#define TRAUMA_MEMORY_IMMUNE_THRESHOLD    0.7f   /**< Trauma memory importance to trigger immune */
#define NEGATIVE_MEMORY_STRESS_THRESHOLD  0.6f   /**< Negative memory intensity threshold */

/* Chronic inflammation duration (seconds) */
#define CHRONIC_INFLAMMATION_MEMORY_THRESHOLD  (86400.0f * 7)  /**< 7 days = chronic */

/* Sickness landmark importance */
#define SICKNESS_LANDMARK_IMPORTANCE      0.8f   /**< Sickness episodes are highly salient */
#define SICKNESS_LANDMARK_IDENTITY_DEFINING true /**< Sickness can be identity-defining */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine effects on episodic encoding
 *
 * Represents how cytokine levels impair/enhance memory formation
 */
typedef struct {
    /* Pro-inflammatory encoding impairment */
    float il1_encoding_impairment;     /**< IL-1β reduction in encoding [0-1] */
    float il6_encoding_impairment;     /**< IL-6 reduction in encoding [0-1] */
    float tnf_encoding_impairment;     /**< TNF-α reduction in encoding [0-1] */
    float ifn_gamma_impairment;        /**< IFN-γ reduction in encoding [0-1] */

    /* Anti-inflammatory encoding restoration */
    float il10_encoding_boost;         /**< IL-10 recovery boost [0-1] */

    /* Aggregate effects */
    float total_encoding_modulation;   /**< Combined encoding factor [0-2] (1.0=normal) */
    float negative_salience_boost;     /**< Enhanced negative memory salience */
    float positive_salience_reduction; /**< Reduced positive memory salience */
    float consolidation_impairment;    /**< Impaired sleep consolidation [0-1] */
} cytokine_memory_effects_t;

/**
 * @brief Inflammation memory state
 *
 * How inflammation affects autobiographical memory processes
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;   /**< How long inflamed */
    bool is_chronic;                   /**< >= 7 days */

    /* Memory impacts */
    float encoding_efficiency;         /**< Current encoding rate [0-1] */
    float consolidation_quality;       /**< Sleep consolidation quality [0-1] */
    float retrieval_accuracy;          /**< Memory retrieval accuracy [0-1] */
    float false_memory_risk;           /**< Risk of false memories [0-1] */

    /* Cognitive aging acceleration */
    float memory_decline_rate;         /**< Chronic inflammation → decline */
    float hippocampal_impairment;      /**< Hippocampal function impairment */

    /* Sickness landmark */
    bool sickness_episode_active;      /**< Currently experiencing sickness behavior */
    uint64_t sickness_episode_id;      /**< Linked memory ID for current episode */
} inflammation_memory_state_t;

/**
 * @brief Memory-triggered immune response
 *
 * How autobiographical memories activate immune system
 */
typedef struct {
    /* Memory characteristics */
    autobio_memory_type_t memory_type; /**< Type of recalled memory */
    memory_valence_t valence;          /**< Positive/negative */
    float importance;                  /**< Memory importance [0-1] */
    float emotional_intensity;         /**< Emotional intensity [0-1] */

    /* Immune triggers */
    bool trauma_triggered;             /**< Trauma memory recalled */
    bool chronic_stress_active;        /**< Ruminating on negative memories */
    float cortisol_release;            /**< Stress hormone release [0-1] */
    float inflammatory_response;       /**< Triggered inflammation [0-1] */

    /* Memory rumination */
    uint32_t rumination_count;         /**< Times memory recalled recently */
    float rumination_duration_sec;     /**< How long ruminating */
} memory_immune_trigger_t;

/**
 * @brief Positive memory immune enhancement
 *
 * How positive autobiographical memories boost immunity
 */
typedef struct {
    /* Positive memory state */
    uint32_t achievement_count;        /**< Recent achievement memories */
    uint32_t social_bond_count;        /**< Recent social bond memories */
    uint32_t learning_count;           /**< Recent learning memories */
    float positive_valence_avg;        /**< Average positive valence */

    /* Immune benefits */
    float immune_enhancement;          /**< Improved immune function [0-1] */
    float cortisol_reduction;          /**< Reduced stress hormone [0-1] */
    float il10_release_boost;          /**< Anti-inflammatory boost */
    float resilience_factor;           /**< Psychological resilience [0-1] */
} positive_memory_immune_boost_t;

/**
 * @brief Sickness landmark memory
 *
 * Special autobiographical memory marking sickness episode
 */
typedef struct {
    uint64_t memory_id;                /**< Autobiographical memory ID */
    uint64_t start_time_ms;            /**< When sickness started */
    uint64_t end_time_ms;              /**< When sickness ended (0=ongoing) */
    brain_inflammation_level_t severity; /**< Inflammation severity */
    float emotional_intensity;         /**< How distressing */
    char description[256];             /**< "I experienced severe inflammation" */
    bool identity_defining;            /**< Did this change self-concept? */
} sickness_landmark_t;

/**
 * @brief Complete autobiographical memory-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    autobiographical_memory_t* autobio_memory;

    /* Current state */
    cytokine_memory_effects_t cytokine_effects;
    inflammation_memory_state_t inflammation_state;
    memory_immune_trigger_t memory_trigger;
    positive_memory_immune_boost_t positive_boost;

    /* Sickness landmarks */
    sickness_landmark_t* sickness_landmarks;
    uint32_t sickness_landmark_count;
    uint32_t sickness_landmark_capacity;
    uint32_t active_sickness_landmark_id; /**< Current sickness episode (0=none) */

    /* Integration flags */
    bool enable_cytokine_encoding_modulation;
    bool enable_inflammation_consolidation_impairment;
    bool enable_sickness_landmark_creation;
    bool enable_trauma_memory_immune_trigger;
    bool enable_positive_memory_immune_boost;
    bool enable_rumination_tracking;

    /* Statistics */
    uint64_t total_updates;
    uint32_t encoding_modulations;
    uint32_t memory_triggered_responses;
    uint32_t positive_boosts;
    uint32_t sickness_landmarks_created;
    uint32_t trauma_recalls;

    } autobio_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_encoding_modulation;
    bool enable_inflammation_consolidation_impairment;
    bool enable_sickness_landmark_creation;
    bool enable_trauma_memory_immune_trigger;
    bool enable_positive_memory_immune_boost;
    bool enable_rumination_tracking;

    /* Sensitivity tuning */
    float cytokine_sensitivity;        /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;    /**< Inflammation effect multiplier [0.5-2.0] */
    float memory_trigger_sensitivity;  /**< Memory trigger multiplier [0.5-2.0] */

    /* Thresholds */
    float trauma_trigger_threshold;    /**< Trauma importance to trigger immune [0.5-0.9] */
    float negative_stress_threshold;   /**< Negative memory intensity threshold [0.4-0.8] */

    /* Sickness landmark settings */
    uint32_t max_sickness_landmarks;   /**< Max stored sickness memories */
} autobio_immune_config_t;

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
int autobio_immune_default_config(autobio_immune_config_t* config);

/**
 * @brief Create autobiographical memory-immune bridge
 *
 * WHAT: Initialize bidirectional autobiographical memory-immune integration
 * WHY:  Enable realistic immune-memory coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param autobio_memory Autobiographical memory system
 * @return New bridge or NULL on failure
 */
autobio_immune_bridge_t* autobio_immune_bridge_create(
    const autobio_immune_config_t* config,
    brain_immune_system_t* immune_system,
    autobiographical_memory_t* autobio_memory
);

/**
 * @brief Destroy autobiographical memory-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void autobio_immune_bridge_destroy(autobio_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Memory API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to episodic encoding
 *
 * WHAT: Modulate memory encoding rate based on cytokine levels
 * WHY:  IL-1β impairs hippocampal LTP and encoding
 * HOW:  Query immune cytokines, reduce encoding efficiency
 *
 * @param bridge Autobiographical memory-immune bridge
 * @return 0 on success
 */
int autobio_immune_apply_cytokine_encoding_effects(autobio_immune_bridge_t* bridge);

/**
 * @brief Apply inflammation to memory consolidation
 *
 * WHAT: Impair sleep-based memory consolidation from inflammation
 * WHY:  Chronic inflammation reduces hippocampal neurogenesis
 * HOW:  Check inflammation level/duration, reduce consolidation quality
 *
 * @param bridge Autobiographical memory-immune bridge
 * @return 0 on success
 */
int autobio_immune_apply_inflammation_consolidation_effects(autobio_immune_bridge_t* bridge);

/**
 * @brief Enhance emotional salience for memories during inflammation
 *
 * WHAT: Boost negative memory salience, reduce positive during sickness
 * WHY:  Inflammation enhances emotional memory formation
 * HOW:  Modulate importance scores for new memories based on valence
 *
 * @param bridge Autobiographical memory-immune bridge
 * @param memory Memory being encoded
 * @return Modified importance score
 */
float autobio_immune_modulate_memory_salience(
    const autobio_immune_bridge_t* bridge,
    const autobiographical_memory_entry_t* memory
);

/**
 * @brief Create sickness landmark memory
 *
 * WHAT: Store autobiographical memory of sickness episode
 * WHY:  Sickness creates distinct temporal landmarks in self-narrative
 * HOW:  Create AUTOBIO_CRISIS memory when inflammation reaches SYSTEMIC
 *
 * @param bridge Autobiographical memory-immune bridge
 * @param severity Inflammation severity
 * @param landmark_id Output: created memory ID
 * @return 0 on success
 */
int autobio_immune_create_sickness_landmark(
    autobio_immune_bridge_t* bridge,
    brain_inflammation_level_t severity,
    uint64_t* landmark_id
);

/**
 * @brief Close sickness landmark memory
 *
 * WHAT: Mark sickness episode as ended, finalize memory
 * WHY:  Complete episodic structure with resolution
 * HOW:  Update end time, compute outcome, store lessons learned
 *
 * @param bridge Autobiographical memory-immune bridge
 * @param landmark_id Sickness landmark to close
 * @return 0 on success
 */
int autobio_immune_close_sickness_landmark(
    autobio_immune_bridge_t* bridge,
    uint64_t landmark_id
);

/**
 * @brief Get current encoding efficiency
 *
 * WHAT: Calculate current episodic encoding rate
 * WHY:  Inflammation reduces encoding efficiency
 * HOW:  Compute from cytokine levels (1.0=normal, <1.0=impaired)
 *
 * @param bridge Autobiographical memory-immune bridge
 * @return Encoding efficiency [0-1.5]
 */
float autobio_immune_get_encoding_efficiency(const autobio_immune_bridge_t* bridge);

/* ============================================================================
 * Memory → Immune API
 * ============================================================================ */

/**
 * @brief Trigger immune response from trauma memory recall
 *
 * WHAT: Activate immune system from recalling traumatic autobiographical memory
 * WHY:  Trauma recall activates HPA axis → cortisol → inflammatory rebound
 * HOW:  Check memory type/valence/importance, trigger cytokine release
 *
 * @param bridge Autobiographical memory-immune bridge
 * @param memory Recalled trauma memory
 * @return 0 on success
 */
int autobio_immune_trigger_from_trauma_recall(
    autobio_immune_bridge_t* bridge,
    const autobiographical_memory_entry_t* memory
);

/**
 * @brief Trigger chronic stress from negative memory rumination
 *
 * WHAT: Sustain inflammatory response from repeated negative memory recall
 * WHY:  Ruminating on failures/crises → chronic stress → immune dysregulation
 * HOW:  Track recall frequency, escalate inflammation if ruminating
 *
 * @param bridge Autobiographical memory-immune bridge
 * @param memory_id Ruminated memory ID
 * @return 0 on success
 */
int autobio_immune_ruminate_on_negative_memory(
    autobio_immune_bridge_t* bridge,
    uint64_t memory_id
);

/**
 * @brief Boost immune from positive memory retrieval
 *
 * WHAT: Enhance immunity from recalling achievements/learning/social bonds
 * WHY:  Positive autobiographical memories reduce cortisol, boost immune function
 * HOW:  Query recent positive memories, release IL-10, reduce inflammation
 *
 * @param bridge Autobiographical memory-immune bridge
 * @param memory Positive memory recalled
 * @return 0 on success
 */
int autobio_immune_boost_from_positive_memory(
    autobio_immune_bridge_t* bridge,
    const autobiographical_memory_entry_t* memory
);

/**
 * @brief Check if memory is identity-threatening
 *
 * WHAT: Determine if memory threatens core self-concept
 * WHY:  Identity-threatening memories trigger stronger immune responses
 * HOW:  Check if memory is core, has negative valence, high importance
 *
 * @param memory Memory to check
 * @return true if identity-threatening
 */
bool autobio_immune_is_identity_threatening(const autobiographical_memory_entry_t* memory);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update autobiographical memory-immune bridge (both directions)
 *
 * WHAT: Process all immune-memory interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine effects, create sickness landmarks, process memory triggers
 *
 * @param bridge Autobiographical memory-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int autobio_immune_bridge_update(
    autobio_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine memory effects
 *
 * @param bridge Autobiographical memory-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int autobio_immune_get_cytokine_effects(
    const autobio_immune_bridge_t* bridge,
    cytokine_memory_effects_t* effects
);

/**
 * @brief Get current inflammation memory state
 *
 * @param bridge Autobiographical memory-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int autobio_immune_get_inflammation_state(
    const autobio_immune_bridge_t* bridge,
    inflammation_memory_state_t* state
);

/**
 * @brief Check if experiencing sickness behavior affecting memory
 *
 * WHAT: Determine if cytokines causing sickness-related memory impairment
 * WHY:  Sickness behavior is distinct memory-affecting state
 * HOW:  Check cytokine levels and inflammation severity
 *
 * @param bridge Autobiographical memory-immune bridge
 * @return true if sickness behavior active
 */
bool autobio_immune_is_sickness_affecting_memory(const autobio_immune_bridge_t* bridge);

/**
 * @brief Get all sickness landmark memories
 *
 * @param bridge Autobiographical memory-immune bridge
 * @param landmarks Output: array of sickness landmarks
 * @param max_landmarks Size of landmarks array
 * @param num_found Output: number of landmarks found
 * @return 0 on success
 */
int autobio_immune_get_sickness_landmarks(
    const autobio_immune_bridge_t* bridge,
    sickness_landmark_t* landmarks,
    uint32_t max_landmarks,
    uint32_t* num_found
);

/**
 * @brief Get consolidation impairment level
 *
 * @param bridge Autobiographical memory-immune bridge
 * @return Consolidation impairment [0-1]
 */
float autobio_immune_get_consolidation_impairment(const autobio_immune_bridge_t* bridge);

/**
 * @brief Get memory decline rate from chronic inflammation
 *
 * @param bridge Autobiographical memory-immune bridge
 * @return Memory decline rate [0-1]
 */
float autobio_immune_get_memory_decline_rate(const autobio_immune_bridge_t* bridge);


/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_AUTOBIOGRAPHICAL
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int autobiographical_immune_connect_bio_async(autobio_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int autobiographical_immune_disconnect_bio_async(autobio_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool autobiographical_immune_is_bio_async_connected(const autobio_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AUTOBIOGRAPHICAL_IMMUNE_BRIDGE_H */
