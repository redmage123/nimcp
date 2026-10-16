/**
 * @file nimcp_amygdala_stress_bridge.h
 * @brief Amygdala-Stress/Wellbeing Integration Bridge
 * @version 1.0.0
 * @date 2025-12-22
 *
 * WHAT: Bidirectional integration between amygdala (fear/anxiety) and stress/wellbeing systems
 * WHY:  Chronic amygdala activation increases HPA axis stress response; stress sensitizes amygdala
 *       reactivity. Essential for modeling stress-anxiety feedback loops.
 * HOW:  Fear/anxiety drive cortisol release; chronic stress sensitizes amygdala; wellbeing buffers
 *       amygdala hyperreactivity. Allostatic load tracking models cumulative stress burden.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * AMYGDALA → STRESS PATHWAYS:
 * ---------------------------
 * 1. Fear-Induced HPA Axis Activation:
 *    - Central amygdala (CeA) → Paraventricular nucleus (PVN) → CRH release
 *    - CRH triggers ACTH from pituitary → Adrenal cortisol release
 *    - Fear/threat detection → rapid cortisol elevation
 *    - Sustained anxiety → chronic cortisol elevation
 *    - Reference: Herman et al. (2016) "Regulation of the hypothalamic-pituitary-
 *      adrenocortical stress response"
 *
 * 2. Anxiety-Induced Chronic Stress:
 *    - Background anxiety maintains elevated baseline cortisol
 *    - Anxiety sensitizes stress reactivity to minor stressors
 *    - Chronic anxiety → allostatic load accumulation
 *    - Reference: McEwen (2007) "Physiology and neurobiology of stress and adaptation"
 *
 * 3. Threat Vigilance and Stress Markers:
 *    - High amygdala threat detection → sustained HPA activation
 *    - Hypervigilance prevents cortisol recovery
 *    - Sleep disruption from anxiety → impaired HPA recovery
 *    - Reference: Chrousos (2009) "Stress and disorders of the stress system"
 *
 * STRESS → AMYGDALA PATHWAYS:
 * ---------------------------
 * 1. Stress-Induced Amygdala Sensitization:
 *    - Chronic glucocorticoids increase amygdala dendritic complexity
 *    - Stress enhances amygdala-hippocampus-PFC connectivity
 *    - Cortisol increases CRH in amygdala (local amplification)
 *    - Stress primes amygdala for hyperreactivity to threats
 *    - Reference: Roozendaal et al. (2009) "Stress, memory and the amygdala"
 *
 * 2. Allostatic Load and Amygdala Dysfunction:
 *    - Cumulative stress exposure → amygdala structural changes
 *    - High allostatic load → increased fear generalization
 *    - Chronic stress impairs fear extinction (prefrontal deficit)
 *    - Stress shifts balance toward fear acquisition vs. extinction
 *    - Reference: Joëls et al. (2013) "Stress and emotional memory: a matter of timing"
 *
 * 3. Stress-Anxiety Positive Feedback Loop:
 *    - Stress → amygdala sensitization → increased anxiety
 *    - Anxiety → HPA activation → more stress
 *    - Loop maintains chronic stress/anxiety states
 *    - Reference: Shin & Liberzon (2010) "The neurocircuitry of fear, stress, and anxiety"
 *
 * WELLBEING → AMYGDALA PATHWAYS:
 * ------------------------------
 * 1. Wellbeing Buffers Amygdala Reactivity:
 *    - Positive affect reduces amygdala threat responses
 *    - Mindfulness/relaxation reduces amygdala-HPA coupling
 *    - Social support dampens amygdala reactivity
 *    - High wellbeing protects against stress sensitization
 *    - Reference: Taylor et al. (2011) "Neural bases of moderation of cortisol stress
 *      responses by psychosocial resources"
 *
 * 2. Wellbeing Interventions Reduce Inflammation:
 *    - Reduced inflammation → reduced amygdala hyperreactivity
 *    - Wellbeing practices lower cortisol and cytokines
 *    - Exercise/meditation → amygdala volume normalization
 *    - Reference: Creswell et al. (2014) "Mindfulness-Based Stress Reduction training
 *      reduces loneliness and pro-inflammatory gene expression"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                  AMYGDALA-STRESS-WELLBEING BRIDGE                          ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                AMYGDALA → STRESS PATHWAY                            │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  AMYGDALA    │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ Fear    → +0.7 cortisol                                        │  ║
 * ║   │   │ Anxiety → +0.4 cortisol (chronic)                              │  ║
 * ║   │   │ Threat  → +0.5 cortisol                                        │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │           │                                                         │  ║
 * ║   │           ▼                                                         │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │   STRESS SYSTEM                 │                             │  ║
 * ║   │   │  - Cortisol elevation           │                             │  ║
 * ║   │   │  - HPA axis activation          │                             │  ║
 * ║   │   │  - Allostatic load accumulation │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                STRESS → AMYGDALA PATHWAY                            │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────────┐                                     │  ║
 * ║   │   │   CHRONIC STRESS         │                                     │  ║
 * ║   │   │ ──────────────────────── │                                     │  ║
 * ║   │   │ Cortisol [0.3-0.5] → +10% sensitization                        │  ║
 * ║   │   │ Cortisol [0.5-0.7] → +30% sensitization                        │  ║
 * ║   │   │ Cortisol [0.7-1.0] → +60% sensitization                        │  ║
 * ║   │   └──────────────────────────┘                                     │  ║
 * ║   │           │                                                         │  ║
 * ║   │           ▼                                                         │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │   AMYGDALA                      │                             │  ║
 * ║   │   │  - Increased reactivity         │                             │  ║
 * ║   │   │  - Enhanced fear acquisition    │                             │  ║
 * ║   │   │  - Impaired fear extinction     │                             │  ║
 * ║   │   │  - Heightened anxiety           │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              WELLBEING → AMYGDALA PATHWAY                           │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ WELLBEING    │ ──→ Reduced amygdala reactivity                │  ║
 * ║   │   │ > 0.7        │ ──→ -40% sensitization                          │  ║
 * ║   │   │              │ ──→ Enhanced fear extinction                    │  ║
 * ║   │   │              │ ──→ Allostatic load recovery                    │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              POSITIVE FEEDBACK LOOP                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   Amygdala → Stress → Sensitization → Amygdala (amplification)     │  ║
 * ║   │   Wellbeing breaks the loop by reducing both stress and reactivity │  ║
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

#ifndef NIMCP_AMYGDALA_STRESS_BRIDGE_H
#define NIMCP_AMYGDALA_STRESS_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "core/brain/subcortical/nimcp_amygdala.h"
#include "cognitive/wellbeing/nimcp_wellbeing.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Amygdala → Stress conversion factors */
#define AMYGDALA_FEAR_CORTISOL_FACTOR       0.7f   /**< Fear → cortisol multiplier */
#define AMYGDALA_ANXIETY_CORTISOL_FACTOR    0.4f   /**< Anxiety → cortisol (chronic) */
#define AMYGDALA_THREAT_CORTISOL_FACTOR     0.5f   /**< Threat detection → cortisol */

/* Stress → Amygdala sensitization thresholds */
#define STRESS_MILD_THRESHOLD               0.3f   /**< Mild stress boundary */
#define STRESS_MODERATE_THRESHOLD           0.5f   /**< Moderate stress boundary */
#define STRESS_HIGH_THRESHOLD               0.7f   /**< High stress boundary */

#define STRESS_MILD_SENSITIZATION           0.1f   /**< +10% reactivity */
#define STRESS_MODERATE_SENSITIZATION       0.3f   /**< +30% reactivity */
#define STRESS_HIGH_SENSITIZATION           0.6f   /**< +60% reactivity */

/* Allostatic load parameters */
#define ALLOSTATIC_LOAD_ACCUMULATION_RATE   0.01f  /**< Load increase per update */
#define ALLOSTATIC_LOAD_DECAY_RATE          0.005f /**< Recovery rate (slow) */
#define ALLOSTATIC_LOAD_THRESHOLD_CHRONIC   0.6f   /**< Threshold for chronic burden */

/* Wellbeing buffering factors */
#define WELLBEING_HIGH_THRESHOLD            0.7f   /**< High wellbeing boundary */
#define WELLBEING_BUFFER_SENSITIZATION      -0.4f  /**< -40% sensitization */
#define WELLBEING_EXTINCTION_BOOST          0.3f   /**< Enhanced extinction rate */
#define WELLBEING_LOAD_RECOVERY_BOOST       2.0f   /**< 2x recovery rate */

/* Cortisol dynamics */
#define CORTISOL_DECAY_RATE                 0.02f  /**< Cortisol decay per update */
#define CORTISOL_MAX_ACCUMULATION           1.0f   /**< Max cortisol level */

/* Bio-async module ID */
#define BIO_MODULE_AMYGDALA_STRESS          0x0D70 /**< Amygdala-stress bridge */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_fear_cortisol;              /**< Fear drives cortisol */
    bool enable_anxiety_cortisol;           /**< Anxiety drives cortisol */
    bool enable_stress_sensitization;       /**< Stress sensitizes amygdala */
    bool enable_wellbeing_buffering;        /**< Wellbeing protects amygdala */
    bool enable_allostatic_load;            /**< Track cumulative burden */

    /* Sensitivity tuning */
    float cortisol_sensitivity;             /**< Amygdala→stress multiplier [0.5-2.0] */
    float sensitization_sensitivity;        /**< Stress→amygdala multiplier [0.5-2.0] */
    float wellbeing_sensitivity;            /**< Wellbeing effect multiplier [0.5-2.0] */

    /* Thresholds */
    float stress_mild_threshold;            /**< Mild stress boundary [0.2-0.4] */
    float stress_high_threshold;            /**< High stress boundary [0.6-0.8] */
    float wellbeing_threshold;              /**< High wellbeing boundary [0.6-0.8] */
    float allostatic_threshold;             /**< Chronic burden threshold [0.5-0.7] */
} amygdala_stress_config_t;

/**
 * @brief Amygdala-driven stress effects
 */
typedef struct {
    /* Amygdala state */
    float fear_level;                       /**< Current fear [0-1] */
    float anxiety_level;                    /**< Current anxiety [0-1] */
    float threat_level_normalized;          /**< Threat (0-1, from enum) */

    /* Cortisol effects */
    float fear_cortisol_contribution;       /**< Fear → cortisol */
    float anxiety_cortisol_contribution;    /**< Anxiety → cortisol */
    float threat_cortisol_contribution;     /**< Threat → cortisol */
    float total_cortisol_drive;             /**< Combined cortisol drive */
} amygdala_stress_effects_t;

/**
 * @brief Stress-induced amygdala changes
 */
typedef struct {
    /* Stress state */
    float cortisol_level;                   /**< Current cortisol [0-1] */
    float stress_duration_sec;              /**< How long stressed */
    bool is_chronic_stress;                 /**< Exceeds threshold */

    /* Amygdala sensitization */
    float sensitization_factor;             /**< Increased reactivity [0-1] */
    float fear_acquisition_boost;           /**< Enhanced fear learning */
    float fear_extinction_impairment;       /**< Impaired extinction */
    float anxiety_elevation;                /**< Baseline anxiety increase */
} stress_amygdala_effects_t;

/**
 * @brief Wellbeing protective effects
 */
typedef struct {
    /* Wellbeing state */
    float wellbeing_level;                  /**< Current wellbeing [0-1] */
    bool is_high_wellbeing;                 /**< Above threshold */

    /* Protective effects */
    float reactivity_buffer;                /**< Reduced amygdala reactivity */
    float sensitization_reduction;          /**< Counteracts stress sensitization */
    float extinction_enhancement;           /**< Improved fear extinction */
    float load_recovery_boost;              /**< Enhanced allostatic recovery */
} wellbeing_amygdala_effects_t;

/**
 * @brief Allostatic load tracking
 */
typedef struct {
    float current_load;                     /**< Current cumulative burden [0-1] */
    float peak_load;                        /**< Peak load experienced */
    float load_duration_sec;                /**< Time above chronic threshold */
    bool is_chronic_burden;                 /**< Load > chronic threshold */
    uint32_t stress_episodes;               /**< Number of stress episodes */
} allostatic_load_state_t;

/**
 * @brief Complete amygdala-stress bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    amygdala_t* amygdala;                   /**< Amygdala instance */
    void* stress_system;                    /**< Stress system (opaque) */
    void* wellbeing_system;                 /**< Wellbeing system (opaque) */

    /* Configuration */
    amygdala_stress_config_t config;

    /* Current state */
    amygdala_stress_effects_t amygdala_effects;
    stress_amygdala_effects_t stress_effects;
    wellbeing_amygdala_effects_t wellbeing_effects;
    allostatic_load_state_t allostatic_load;

    /* Dynamic state */
    float cortisol_level;                   /**< Current cortisol [0-1] */
    float amygdala_sensitization;           /**< Current sensitization [0-1] */
    float wellbeing_buffer;                 /**< Current wellbeing buffer [0-1] */

    /* Integration flags */
    bool amygdala_connected;
    bool stress_connected;
    bool wellbeing_connected;

    /* Timing */
    uint64_t last_update_time_ms;
    float stress_accumulator_sec;           /**< Tracks stress duration */
    float load_accumulator_sec;             /**< Tracks load duration */

    /* Statistics */
    uint64_t total_updates;
    uint32_t stress_episodes;
    uint32_t wellbeing_interventions;
    uint32_t chronic_burden_episodes;

    } amygdala_stress_bridge_t;

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
 * @return 0 on success, error code on failure
 */
int amygdala_stress_default_config(amygdala_stress_config_t* config);

/**
 * @brief Create amygdala-stress bridge
 *
 * WHAT: Initialize bidirectional amygdala-stress integration
 * WHY:  Enable realistic stress-anxiety feedback loops
 * HOW:  Allocate structure, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
amygdala_stress_bridge_t* amygdala_stress_create(const amygdala_stress_config_t* config);

/**
 * @brief Destroy amygdala-stress bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void amygdala_stress_destroy(amygdala_stress_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect amygdala to bridge
 *
 * WHAT: Establish amygdala connection
 * WHY:  Enable fear/anxiety → stress pathway
 * HOW:  Store amygdala pointer, validate
 *
 * @param bridge Bridge instance
 * @param amygdala Amygdala instance
 * @return 0 on success, error code on failure
 */
int amygdala_stress_connect_amygdala(amygdala_stress_bridge_t* bridge, amygdala_t* amygdala);

/**
 * @brief Connect stress system to bridge
 *
 * WHAT: Establish stress system connection
 * WHY:  Enable stress → amygdala sensitization pathway
 * HOW:  Store stress pointer (opaque), validate
 *
 * @param bridge Bridge instance
 * @param stress Stress system (opaque pointer)
 * @return 0 on success, error code on failure
 */
int amygdala_stress_connect_stress(amygdala_stress_bridge_t* bridge, void* stress);

/**
 * @brief Connect wellbeing system to bridge
 *
 * WHAT: Establish wellbeing system connection
 * WHY:  Enable wellbeing → amygdala buffering pathway
 * HOW:  Store wellbeing pointer (opaque), validate
 *
 * @param bridge Bridge instance
 * @param wellbeing Wellbeing system (opaque pointer)
 * @return 0 on success, error code on failure
 */
int amygdala_stress_connect_wellbeing(amygdala_stress_bridge_t* bridge, void* wellbeing);

/* ============================================================================
 * Amygdala → Stress API
 * ============================================================================ */

/**
 * @brief Convert amygdala fear to cortisol release
 *
 * WHAT: Map fear level to cortisol increase
 * WHY:  Fear activates HPA axis for stress response
 * HOW:  Query fear level, compute cortisol contribution
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int amygdala_stress_apply_fear_cortisol(amygdala_stress_bridge_t* bridge);

/**
 * @brief Convert amygdala anxiety to chronic cortisol
 *
 * WHAT: Map anxiety level to sustained cortisol elevation
 * WHY:  Chronic anxiety maintains elevated HPA tone
 * HOW:  Query anxiety level, accumulate cortisol over time
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int amygdala_stress_apply_anxiety_cortisol(amygdala_stress_bridge_t* bridge);

/**
 * @brief Update allostatic load from stress
 *
 * WHAT: Accumulate cumulative stress burden
 * WHY:  Track long-term stress impact on system
 * HOW:  Integrate cortisol over time, decay slowly
 *
 * @param bridge Bridge instance
 * @param delta_sec Time since last update (seconds)
 * @return 0 on success, error code on failure
 */
int amygdala_stress_update_allostatic_load(amygdala_stress_bridge_t* bridge, float delta_sec);

/* ============================================================================
 * Stress → Amygdala API
 * ============================================================================ */

/**
 * @brief Apply stress-induced amygdala sensitization
 *
 * WHAT: Increase amygdala reactivity based on cortisol level
 * WHY:  Chronic stress primes amygdala for hyperreactivity
 * HOW:  Map cortisol level to sensitization factor
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int amygdala_stress_apply_sensitization(amygdala_stress_bridge_t* bridge);

/**
 * @brief Compute effective amygdala reactivity
 *
 * WHAT: Calculate current amygdala reactivity including stress effects
 * WHY:  Stress amplifies threat responses
 * HOW:  Base reactivity × (1 + sensitization_factor)
 *
 * @param bridge Bridge instance
 * @return Effective reactivity multiplier [1.0 - 1.6]
 */
float amygdala_stress_get_effective_reactivity(const amygdala_stress_bridge_t* bridge);

/* ============================================================================
 * Wellbeing → Amygdala API
 * ============================================================================ */

/**
 * @brief Apply wellbeing buffering to amygdala
 *
 * WHAT: Reduce amygdala hyperreactivity from high wellbeing
 * WHY:  Wellbeing practices reduce threat reactivity
 * HOW:  Query wellbeing level, reduce sensitization if high
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int amygdala_stress_apply_wellbeing_buffer(amygdala_stress_bridge_t* bridge);

/**
 * @brief Enhance fear extinction from wellbeing
 *
 * WHAT: Boost extinction learning rate when wellbeing is high
 * WHY:  Wellbeing interventions improve emotion regulation
 * HOW:  Query wellbeing, scale extinction rate
 *
 * @param bridge Bridge instance
 * @return Extinction rate multiplier [1.0 - 1.3]
 */
float amygdala_stress_get_extinction_boost(const amygdala_stress_bridge_t* bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update bridge state (all pathways)
 *
 * WHAT: Process all amygdala-stress-wellbeing interactions
 * WHY:  Advance coupled state machine
 * HOW:  Update cortisol, sensitization, allostatic load, apply effects
 *
 * @param bridge Bridge instance
 * @param delta_ms Time since last update (milliseconds)
 * @return 0 on success, error code on failure
 */
int amygdala_stress_update(amygdala_stress_bridge_t* bridge, uint64_t delta_ms);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cortisol level
 *
 * @param bridge Bridge instance
 * @return Cortisol level [0-1]
 */
float amygdala_stress_get_cortisol(const amygdala_stress_bridge_t* bridge);

/**
 * @brief Get current allostatic load
 *
 * @param bridge Bridge instance
 * @return Allostatic load [0-1]
 */
float amygdala_stress_get_allostatic_load(const amygdala_stress_bridge_t* bridge);

/**
 * @brief Get current amygdala sensitization factor
 *
 * @param bridge Bridge instance
 * @return Sensitization [0-1] (added to baseline)
 */
float amygdala_stress_get_sensitization(const amygdala_stress_bridge_t* bridge);

/**
 * @brief Check if experiencing chronic stress burden
 *
 * WHAT: Determine if allostatic load exceeds chronic threshold
 * WHY:  Detect clinically significant cumulative stress
 * HOW:  Check load > threshold
 *
 * @param bridge Bridge instance
 * @return true if chronic burden (load > 0.6)
 */
bool amygdala_stress_is_chronic_burden(const amygdala_stress_bridge_t* bridge);

/**
 * @brief Get amygdala-stress effects
 *
 * @param bridge Bridge instance
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int amygdala_stress_get_effects(
    const amygdala_stress_bridge_t* bridge,
    amygdala_stress_effects_t* effects
);

/**
 * @brief Get stress-amygdala effects
 *
 * @param bridge Bridge instance
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int amygdala_stress_get_stress_effects(
    const amygdala_stress_bridge_t* bridge,
    stress_amygdala_effects_t* effects
);

/**
 * @brief Get wellbeing-amygdala effects
 *
 * @param bridge Bridge instance
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int amygdala_stress_get_wellbeing_effects(
    const amygdala_stress_bridge_t* bridge,
    wellbeing_amygdala_effects_t* effects
);

/**
 * @brief Get allostatic load state
 *
 * @param bridge Bridge instance
 * @param state Output state structure
 * @return 0 on success, error code on failure
 */
int amygdala_stress_get_allostatic_state(
    const amygdala_stress_bridge_t* bridge,
    allostatic_load_state_t* state
);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register with inter-module messaging
 * WHY:  Enable async communication
 * HOW:  Register module with bio-async router
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int amygdala_stress_connect_bio_async(amygdala_stress_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister from messaging
 * WHY:  Clean shutdown
 * HOW:  Unregister module from bio-async router
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int amygdala_stress_disconnect_bio_async(amygdala_stress_bridge_t* bridge);

/**
 * @brief Check if bio-async connected
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool amygdala_stress_is_bio_async_connected(const amygdala_stress_bridge_t* bridge);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge instance
 * @param total_updates Output: total updates
 * @param stress_episodes Output: stress episode count
 * @param wellbeing_interventions Output: wellbeing intervention count
 * @param chronic_burden_episodes Output: chronic burden episode count
 * @return 0 on success, error code on failure
 */
int amygdala_stress_get_statistics(
    const amygdala_stress_bridge_t* bridge,
    uint64_t* total_updates,
    uint32_t* stress_episodes,
    uint32_t* wellbeing_interventions,
    uint32_t* chronic_burden_episodes
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AMYGDALA_STRESS_BRIDGE_H */
