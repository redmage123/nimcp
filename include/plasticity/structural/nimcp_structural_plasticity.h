/**
 * @file nimcp_structural_plasticity.h
 * @brief Structural Plasticity - Spine Dynamics and Synapse Formation/Elimination
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Activity-dependent spine formation, stabilization, and pruning
 * WHY:  Structural plasticity provides long-term memory storage and network rewiring
 * HOW:  Spine morphology tracking with state-based lifecycle management
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SPINE MORPHOLOGY AND DYNAMICS:
 * ------------------------------
 * 1. Dendritic Spine Structure:
 *    - Thin spines: Learning phase, high motility, small PSD
 *    - Mushroom spines: Stable memory, large PSD, strong connections
 *    - Stubby spines: Intermediate state, moderate stability
 *    - Filopodia: Transient protrusions, synapse seeking
 *    - Reference: Bhatt et al. (2009) "Dendritic spine dynamics"
 *    - Reference: Holtmaat & Svoboda (2009) "Experience-dependent structural
 *      synaptic plasticity in the mammalian brain"
 *
 * 2. Activity-Dependent Formation:
 *    - High-frequency stimulation triggers spine formation
 *    - Actin polymerization drives spine growth
 *    - Nascent spines appear within minutes-hours
 *    - Formation threshold: ~20-50 Hz sustained activity
 *    - Reference: Matsuzaki et al. (2004) "Structural basis of long-term
 *      potentiation in single dendritic spines"
 *
 * 3. Spine Stabilization:
 *    - Repeated activation consolidates nascent spines
 *    - PSD growth correlates with synaptic strength
 *    - Maturation time: 1-7 days for full stabilization
 *    - CaMKII translocation stabilizes structure
 *    - Reference: Kasai et al. (2010) "Structural dynamics of dendritic spines"
 *
 * 4. Spine Pruning and Elimination:
 *    - Low activity leads to spine retraction
 *    - Microglia-mediated synaptic pruning during development/sleep
 *    - Complement-tagged synapses targeted for elimination
 *    - Pruning threshold: <1 Hz for extended period
 *    - Reference: Hong et al. (2016) "Complement and microglia mediate
 *      early synapse loss in Alzheimer mouse models"
 *
 * 5. Sleep-Dependent Consolidation:
 *    - NREM sleep strengthens tagged spines
 *    - REM sleep prunes weak spines
 *    - Synaptic homeostasis during sleep (downscaling)
 *    - Sleep deprivation impairs spine formation
 *    - Reference: Tononi & Cirelli (2014) "Sleep and synaptic homeostasis"
 *
 * 6. Immune-Mediated Pruning:
 *    - Microglia engulf synaptic material (synaptic pruning)
 *    - Complement C1q/C3 tag weak synapses
 *    - Inflammation can trigger excessive pruning
 *    - Critical for development and memory refinement
 *    - Reference: Schafer et al. (2012) "Microglia sculpt postnatal
 *      neural circuits in an activity and complement-dependent manner"
 *
 * SPINE STATE LIFECYCLE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                      STRUCTURAL PLASTICITY LIFECYCLE                       ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   NASCENT (Thin Spine)                                                    ║
 * ║   ├─ High motility, small PSD, unstable                                   ║
 * ║   ├─ Formation: Activity > formation_threshold                            ║
 * ║   └─ Transition: Stabilization signal OR timeout → STABLE/ELIMINATED      ║
 * ║                                                                            ║
 * ║   STABLE (Mushroom Spine)                                                 ║
 * ║   ├─ Low motility, large PSD, strong synapse                              ║
 * ║   ├─ Consolidation: Repeated activation during maturation window          ║
 * ║   └─ Transition: LTP → POTENTIATED, Low activity → PRUNING                ║
 * ║                                                                            ║
 * ║   POTENTIATED (Enlarged Spine)                                            ║
 * ║   ├─ Enlarged PSD, maximal strength                                       ║
 * ║   ├─ Trigger: Strong LTP or sleep consolidation                           ║
 * ║   └─ Transition: Sustained activity → STABLE, Decay → STABLE              ║
 * ║                                                                            ║
 * ║   PRUNING (Shrinking Spine)                                               ║
 * ║   ├─ Decreased volume, flagged for elimination                            ║
 * ║   ├─ Trigger: Activity < pruning_threshold OR complement tag              ║
 * ║   └─ Transition: Recovery → STABLE, Timeout → ELIMINATED                  ║
 * ║                                                                            ║
 * ║   ELIMINATED (Removed)                                                    ║
 * ║   └─ Synapse removed, resources freed                                     ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    STRUCTURAL PLASTICITY SYSTEM                            ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   INPUTS                  PROCESSING                    OUTPUTS           ║
 * ║   ──────────────────────────────────────────────────────────────────      ║
 * ║                                                                            ║
 * ║   Activity Rate     ──►   Formation Logic        ──►   New Spines         ║
 * ║   LTP/LTD Events    ──►   Stabilization Logic    ──►   Stable Synapses   ║
 * ║   Sleep State       ──►   Consolidation Logic    ──►   Potentiation       ║
 * ║   Immune Signals    ──►   Pruning Logic          ──►   Elimination        ║
 * ║   Complement Tags   ──►   Microglia Pruning      ──►   Synapse Removal    ║
 * ║                                                                            ║
 * ║   INTEGRATION BRIDGES:                                                    ║
 * ║   ┌────────────────────┐  ┌────────────────────┐                         ║
 * ║   │  Sleep Bridge      │  │  Immune Bridge     │                         ║
 * ║   │  ──────────────    │  │  ──────────────    │                         ║
 * ║   │  NREM: Consolidate │  │  Microglia: Prune  │                         ║
 * ║   │  REM: Prune weak   │  │  Complement: Tag   │                         ║
 * ║   │  Awake: Formation  │  │  Inflammation: ↑↓  │                         ║
 * ║   └────────────────────┘  └────────────────────┘                         ║
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

#ifndef NIMCP_STRUCTURAL_PLASTICITY_H
#define NIMCP_STRUCTURAL_PLASTICITY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/nimcp_sleep_wake.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define STRUCTURAL_MAX_SPINES              65536  /**< Max tracked spines */
#define STRUCTURAL_EPITOPE_SIZE            32     /**< Complement tag size */
#define STRUCTURAL_MODULE_NAME             "structural_plasticity"

/* Formation thresholds (Hz) */
#define STRUCTURAL_FORMATION_THRESHOLD_MIN  20.0f  /**< Min activity for formation */
#define STRUCTURAL_FORMATION_THRESHOLD_MAX  50.0f  /**< Typical formation threshold */

/* Pruning thresholds (Hz) */
#define STRUCTURAL_PRUNING_THRESHOLD_MIN    0.5f   /**< Very low activity */
#define STRUCTURAL_PRUNING_THRESHOLD_MAX    2.0f   /**< Low activity threshold */

/* Maturation timescales (seconds) */
#define STRUCTURAL_MATURATION_TIME_MIN      3600.0f   /**< 1 hour */
#define STRUCTURAL_MATURATION_TIME_TYPICAL  86400.0f  /**< 24 hours */
#define STRUCTURAL_MATURATION_TIME_MAX      604800.0f /**< 7 days */

/* Spine volume ranges (arbitrary units) */
#define STRUCTURAL_VOLUME_NASCENT_MIN       0.1f   /**< Thin spine */
#define STRUCTURAL_VOLUME_NASCENT_MAX       0.3f
#define STRUCTURAL_VOLUME_STABLE_MIN        0.5f   /**< Mushroom spine */
#define STRUCTURAL_VOLUME_STABLE_MAX        1.0f
#define STRUCTURAL_VOLUME_POTENTIATED_MIN   1.0f   /**< Enlarged spine */
#define STRUCTURAL_VOLUME_POTENTIATED_MAX   1.5f

/* PSD size ranges (arbitrary units) */
#define STRUCTURAL_PSD_NASCENT_MIN          0.2f
#define STRUCTURAL_PSD_NASCENT_MAX          0.4f
#define STRUCTURAL_PSD_STABLE_MIN           0.6f
#define STRUCTURAL_PSD_STABLE_MAX           1.0f
#define STRUCTURAL_PSD_POTENTIATED_MIN      1.0f
#define STRUCTURAL_PSD_POTENTIATED_MAX      1.8f

/* Actin dynamics (growth/shrinkage rate) */
#define STRUCTURAL_ACTIN_GROWTH_RATE        0.1f   /**< Volume/sec during formation */
#define STRUCTURAL_ACTIN_SHRINK_RATE        0.05f  /**< Volume/sec during pruning */

/* Spine stability parameters */
#define STRUCTURAL_STABILITY_NASCENT        0.2f   /**< Low stability for nascent spines */
#define STRUCTURAL_STABILITY_STABLE         0.8f   /**< High stability for mature spines */
#define STRUCTURAL_STABILITY_POTENTIATED    0.95f  /**< Maximum stability */

/* Spine motility (inverse of stability) */
#define STRUCTURAL_MOTILITY_NASCENT         0.8f   /**< High motility for nascent */
#define STRUCTURAL_MOTILITY_STABLE          0.2f   /**< Low motility for stable */
#define STRUCTURAL_MOTILITY_POTENTIATED     0.05f  /**< Minimal motility */

/* Actin dynamics by state */
#define STRUCTURAL_ACTIN_STABLE             0.02f  /**< Slow dynamics for stable spines */
#define STRUCTURAL_ACTIN_POTENTIATED        0.01f  /**< Minimal dynamics for potentiated */

/* CaMKII concentrations by state */
#define STRUCTURAL_CAMKII_NASCENT           0.1f   /**< Low CaMKII in nascent */
#define STRUCTURAL_CAMKII_STABLE            0.6f   /**< Moderate CaMKII in stable */
#define STRUCTURAL_CAMKII_POTENTIATED       0.9f   /**< High CaMKII in potentiated */

/* Receptor counts */
#define STRUCTURAL_AMPAR_NASCENT            5.0f   /**< Few AMPARs in nascent */
#define STRUCTURAL_AMPAR_STABLE             30.0f  /**< Moderate AMPARs in stable */
#define STRUCTURAL_AMPAR_POTENTIATED        80.0f  /**< Many AMPARs in potentiated */
#define STRUCTURAL_NMDAR_NASCENT            3.0f   /**< Few NMDARs in nascent */
#define STRUCTURAL_NMDAR_STABLE             15.0f  /**< Moderate NMDARs in stable */
#define STRUCTURAL_NMDAR_POTENTIATED        30.0f  /**< Many NMDARs in potentiated */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Synapse structural states
 *
 * BIOLOGICAL BASIS:
 * Spines progress through lifecycle from nascent → stable → potentiated
 * or nascent → pruning → eliminated based on activity
 */
typedef enum {
    SYNAPSE_STATE_NASCENT = 0,     /**< Newly formed, thin spine */
    SYNAPSE_STATE_STABLE,          /**< Consolidated, mushroom spine */
    SYNAPSE_STATE_POTENTIATED,     /**< Strengthened, enlarged spine */
    SYNAPSE_STATE_PRUNING,         /**< Flagged for elimination */
    SYNAPSE_STATE_ELIMINATED,      /**< Removed from network */
    SYNAPSE_STATE_COUNT
} synapse_state_t;

/**
 * @brief Structural change event types
 */
typedef enum {
    STRUCTURAL_EVENT_FORMATION = 0,   /**< New spine formed */
    STRUCTURAL_EVENT_STABILIZATION,   /**< Nascent → stable */
    STRUCTURAL_EVENT_POTENTIATION,    /**< Stable → potentiated */
    STRUCTURAL_EVENT_PRUNING_START,   /**< Marked for pruning */
    STRUCTURAL_EVENT_ELIMINATION,     /**< Synapse removed */
    STRUCTURAL_EVENT_RECOVERY,        /**< Pruning → stable recovery */
    STRUCTURAL_EVENT_COUNT
} structural_event_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Spine morphology state
 *
 * BIOLOGICAL BASIS:
 * Spine structure determines synaptic strength and stability.
 * Volume, PSD size, and actin dynamics reflect maturation state.
 */
typedef struct {
    float spine_volume;         /**< Spine head volume [0.1-1.5] */
    float psd_size;             /**< Postsynaptic density size [0.2-1.8] */
    float actin_dynamics;       /**< Actin polymerization rate */
    float spine_stability;      /**< Structural stability [0-1] */
    float spine_motility;       /**< Head motility (inverse stability) */

    /* Biochemical state */
    float camkii_concentration; /**< CaMKII at spine [0-1] */
    float ampar_count;          /**< AMPA receptor count [0-100] */
    float nmdar_count;          /**< NMDA receptor count [0-50] */
} spine_morphology_t;

/**
 * @brief Synapse structural state
 */
typedef struct {
    /* Identity */
    uint32_t synapse_id;        /**< Unique synapse identifier */
    uint32_t pre_neuron_id;     /**< Presynaptic neuron */
    uint32_t post_neuron_id;    /**< Postsynaptic neuron */

    /* State */
    synapse_state_t state;      /**< Current structural state */
    spine_morphology_t morphology;

    /* Activity tracking */
    float recent_activity_hz;   /**< Recent firing rate */
    float ltp_accumulator;      /**< Accumulated LTP events */
    float ltd_accumulator;      /**< Accumulated LTD events */
    uint64_t last_active_time;  /**< Last spike time */

    /* Maturation */
    uint64_t formation_time;    /**< When spine formed */
    float maturation_progress;  /**< Progress to stable [0-1] */
    bool consolidation_tagged;  /**< Tagged for sleep consolidation */

    /* Pruning */
    bool complement_tagged;     /**< Marked by complement for pruning */
    uint8_t complement_tag[STRUCTURAL_EPITOPE_SIZE]; /**< Immune tag */
    float pruning_urgency;      /**< Urgency of elimination [0-1] */
    uint64_t pruning_start_time;/**< When pruning began */

    /* Statistics */
    uint32_t formation_events;  /**< Times reformed */
    uint32_t potentiation_events;
    uint32_t pruning_events;
} synapse_structural_state_t;

/**
 * @brief Structural plasticity configuration
 */
typedef struct {
    /* Formation parameters */
    float formation_threshold_hz;    /**< Activity rate for formation */
    float formation_rate;            /**< Formation probability per update */

    /* Stabilization parameters */
    float maturation_time_sec;       /**< Time to reach stable state */
    float stabilization_threshold;   /**< Activity for stabilization */
    bool require_sleep_consolidation;/**< Need sleep for stabilization */

    /* Pruning parameters */
    float pruning_threshold_hz;      /**< Low activity triggers pruning */
    float pruning_rate;              /**< Pruning speed */
    float inactivity_timeout_sec;    /**< Time before auto-prune */

    /* Potentiation parameters */
    float ltp_potentiation_threshold;/**< LTP events for potentiation */
    float potentiation_decay_rate;   /**< Decay back to stable */

    /* Immune integration */
    bool enable_immune_pruning;      /**< Allow microglia pruning */
    float complement_sensitivity;    /**< Sensitivity to complement tags */

    /* Sleep integration */
    bool enable_sleep_consolidation; /**< Sleep-dependent stabilization */
    float sleep_consolidation_boost; /**< Sleep consolidation multiplier */

    /* Resource limits */
    uint32_t max_spines;             /**< Max concurrent spines */
    float spine_density_limit;       /**< Max spines per neuron */
} structural_plasticity_config_t;

/**
 * @brief Structural plasticity system
 */
typedef struct structural_plasticity_system structural_plasticity_system_t;

/**
 * @brief Structural change callback
 *
 * @param event Event type
 * @param synapse_id Affected synapse
 * @param old_state Previous state
 * @param new_state New state
 * @param user_data User context
 */
typedef void (*structural_change_callback_t)(
    structural_event_t event,
    uint32_t synapse_id,
    synapse_state_t old_state,
    synapse_state_t new_state,
    void* user_data
);

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
int structural_plasticity_default_config(structural_plasticity_config_t* config);

/**
 * @brief Create structural plasticity system
 *
 * WHAT: Initialize spine dynamics tracking system
 * WHY:  Enable long-term structural changes
 * HOW:  Allocate tracking structures, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New system or NULL on failure
 */
structural_plasticity_system_t* structural_plasticity_create(
    const structural_plasticity_config_t* config
);

/**
 * @brief Destroy structural plasticity system
 *
 * WHAT: Clean up structural plasticity resources
 * WHY:  Proper resource deallocation
 * HOW:  Free all spines and system state
 *
 * @param system System to destroy
 */
void structural_plasticity_destroy(structural_plasticity_system_t* system);

/* ============================================================================
 * Formation and Elimination API
 * ============================================================================ */

/**
 * @brief Form new synapse (spine formation)
 *
 * WHAT: Create new dendritic spine with nascent state
 * WHY:  High activity triggers spine formation
 * HOW:  Initialize spine morphology, add to tracking
 *
 * @param system Structural plasticity system
 * @param pre_neuron_id Presynaptic neuron
 * @param post_neuron_id Postsynaptic neuron
 * @param activity_hz Current activity rate
 * @param synapse_id Output synapse ID
 * @return 0 on success
 */
int structural_plasticity_form_synapse(
    structural_plasticity_system_t* system,
    uint32_t pre_neuron_id,
    uint32_t post_neuron_id,
    float activity_hz,
    uint32_t* synapse_id
);

/**
 * @brief Eliminate synapse (spine pruning)
 *
 * WHAT: Remove synapse from network
 * WHY:  Low activity or immune-mediated pruning
 * HOW:  Transition to eliminated state, free resources
 *
 * @param system Structural plasticity system
 * @param synapse_id Synapse to eliminate
 * @return 0 on success
 */
int structural_plasticity_eliminate_synapse(
    structural_plasticity_system_t* system,
    uint32_t synapse_id
);

/**
 * @brief Check if synapse should be formed
 *
 * WHAT: Determine if activity warrants new spine
 * WHY:  Activity-dependent formation logic
 * HOW:  Compare activity to formation threshold
 *
 * @param system Structural plasticity system
 * @param activity_hz Recent activity rate
 * @return true if should form
 */
bool structural_plasticity_should_form(
    const structural_plasticity_system_t* system,
    float activity_hz
);

/**
 * @brief Check if synapse should be pruned
 *
 * WHAT: Determine if spine should be eliminated
 * WHY:  Low activity or immune tagging triggers pruning
 * HOW:  Check activity threshold and complement tags
 *
 * @param system Structural plasticity system
 * @param synapse Synapse to check
 * @return true if should prune
 */
bool structural_plasticity_should_prune(
    const structural_plasticity_system_t* system,
    const synapse_structural_state_t* synapse
);

/* ============================================================================
 * Stabilization and Potentiation API
 * ============================================================================ */

/**
 * @brief Stabilize nascent synapse
 *
 * WHAT: Transition nascent spine to stable state
 * WHY:  Repeated activation consolidates structure
 * HOW:  Increase volume, PSD size, stability
 *
 * @param system Structural plasticity system
 * @param synapse_id Synapse to stabilize
 * @return 0 on success
 */
int structural_plasticity_stabilize_synapse(
    structural_plasticity_system_t* system,
    uint32_t synapse_id
);

/**
 * @brief Potentiate stable synapse
 *
 * WHAT: Enlarge spine and strengthen synapse
 * WHY:  Strong LTP or sleep consolidation
 * HOW:  Increase volume, PSD, AMPAR count
 *
 * @param system Structural plasticity system
 * @param synapse_id Synapse to potentiate
 * @return 0 on success
 */
int structural_plasticity_potentiate_synapse(
    structural_plasticity_system_t* system,
    uint32_t synapse_id
);

/**
 * @brief Tag synapse for sleep consolidation
 *
 * WHAT: Mark synapse for strengthening during sleep
 * WHY:  Learning-related spines consolidate during NREM
 * HOW:  Set consolidation flag
 *
 * @param system Structural plasticity system
 * @param synapse_id Synapse to tag
 * @return 0 on success
 */
int structural_plasticity_tag_for_consolidation(
    structural_plasticity_system_t* system,
    uint32_t synapse_id
);

/* ============================================================================
 * Activity Tracking API
 * ============================================================================ */

/**
 * @brief Update synapse activity
 *
 * WHAT: Record spike event and update activity rate
 * WHY:  Activity determines formation/pruning decisions
 * HOW:  Update exponential moving average of firing rate
 *
 * @param system Structural plasticity system
 * @param synapse_id Synapse that spiked
 * @param current_time Current time (ms)
 * @return 0 on success
 */
int structural_plasticity_update_activity(
    structural_plasticity_system_t* system,
    uint32_t synapse_id,
    uint64_t current_time
);

/**
 * @brief Record LTP event
 *
 * WHAT: Accumulate LTP toward potentiation
 * WHY:  LTP strengthens and enlarges spines
 * HOW:  Increment LTP counter, check potentiation threshold
 *
 * @param system Structural plasticity system
 * @param synapse_id Synapse experiencing LTP
 * @param ltp_magnitude LTP strength
 * @return 0 on success
 */
int structural_plasticity_record_ltp(
    structural_plasticity_system_t* system,
    uint32_t synapse_id,
    float ltp_magnitude
);

/**
 * @brief Record LTD event
 *
 * WHAT: Accumulate LTD toward pruning
 * WHY:  LTD weakens and shrinks spines
 * HOW:  Increment LTD counter, check pruning threshold
 *
 * @param system Structural plasticity system
 * @param synapse_id Synapse experiencing LTD
 * @param ltd_magnitude LTD strength
 * @return 0 on success
 */
int structural_plasticity_record_ltd(
    structural_plasticity_system_t* system,
    uint32_t synapse_id,
    float ltd_magnitude
);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update structural plasticity system
 *
 * WHAT: Advance spine dynamics (maturation, pruning checks)
 * WHY:  Structural changes occur over time
 * HOW:  Update all spine states, trigger transitions
 *
 * @param system Structural plasticity system
 * @param delta_sec Time since last update
 * @return 0 on success
 */
int structural_plasticity_update(
    structural_plasticity_system_t* system,
    float delta_sec
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get synapse state
 *
 * @param system Structural plasticity system
 * @param synapse_id Synapse to query
 * @param state Output state structure
 * @return 0 on success
 */
int structural_plasticity_get_synapse_state(
    structural_plasticity_system_t* system,
    uint32_t synapse_id,
    synapse_structural_state_t* state
);

/**
 * @brief Get spine morphology
 *
 * @param system Structural plasticity system (non-const due to mutex)
 * @param synapse_id Synapse to query
 * @param morphology Output morphology structure
 * @return 0 on success
 */
int structural_plasticity_get_morphology(
    structural_plasticity_system_t* system,
    uint32_t synapse_id,
    spine_morphology_t* morphology
);

/**
 * @brief Get spine count by state
 *
 * @param system Structural plasticity system (non-const due to mutex)
 * @param state State to count
 * @return Count of spines in state
 */
uint32_t structural_plasticity_get_spine_count(
    structural_plasticity_system_t* system,
    synapse_state_t state
);

/**
 * @brief Get total spine count
 *
 * @param system Structural plasticity system (non-const due to mutex)
 * @return Total active spines
 */
uint32_t structural_plasticity_get_total_spines(
    structural_plasticity_system_t* system
);

/* ============================================================================
 * Callback API
 * ============================================================================ */

/**
 * @brief Register structural change callback
 *
 * WHAT: Register for spine state change notifications
 * WHY:  Enable reactive responses to structural changes
 * HOW:  Store callback, invoke on state transitions
 *
 * @param system Structural plasticity system
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success
 */
int structural_plasticity_register_callback(
    structural_plasticity_system_t* system,
    structural_change_callback_t callback,
    void* user_data
);

/* ============================================================================
 * Immune Integration API (for immune bridge)
 * ============================================================================ */

/**
 * @brief Tag synapse with complement for immune pruning
 *
 * WHAT: Mark synapse for microglia-mediated elimination
 * WHY:  Weak synapses tagged by complement C1q/C3
 * HOW:  Set complement tag, increase pruning urgency
 *
 * @param system Structural plasticity system
 * @param synapse_id Synapse to tag
 * @param tag Complement tag (epitope)
 * @param tag_len Tag length
 * @return 0 on success
 */
int structural_plasticity_tag_complement(
    structural_plasticity_system_t* system,
    uint32_t synapse_id,
    const uint8_t* tag,
    size_t tag_len
);

/**
 * @brief Check if synapse is complement-tagged
 *
 * @param system Structural plasticity system
 * @param synapse_id Synapse to check
 * @return true if tagged
 */
bool structural_plasticity_is_complement_tagged(
    structural_plasticity_system_t* system,
    uint32_t synapse_id
);

/**
 * @brief Get complement-tagged synapses
 *
 * WHAT: List all synapses marked for immune pruning
 * WHY:  Microglia need targets for engulfment
 * HOW:  Return array of tagged synapse IDs
 *
 * @param system Structural plasticity system
 * @param synapse_ids Output array (allocated by caller)
 * @param max_count Array capacity
 * @param count Output actual count
 * @return 0 on success
 */
int structural_plasticity_get_complement_tagged(
    structural_plasticity_system_t* system,
    uint32_t* synapse_ids,
    uint32_t max_count,
    uint32_t* count
);

/* ============================================================================
 * Slot Reclamation API
 * ============================================================================ */

/**
 * @brief Compact spine array by removing eliminated slots
 *
 * WHAT: Remove eliminated spines from the array to reclaim memory
 * WHY:  Eliminated spines consume memory and slow down iteration
 * HOW:  Shift non-eliminated spines to fill gaps
 *
 * NOTE: This is called automatically during form_synapse when no eliminated
 * slots are available for reuse. Call manually for batch compaction.
 *
 * @param system Structural plasticity system
 * @return Number of slots reclaimed, or -1 on error
 */
int structural_plasticity_compact(structural_plasticity_system_t* system);

/**
 * @brief Count eliminated (reclaimable) spine slots
 *
 * WHAT: Count how many slots are occupied by eliminated spines
 * WHY:  Monitor fragmentation before deciding to compact
 * HOW:  Linear scan of spine array
 *
 * @param system Structural plasticity system
 * @return Number of eliminated slots
 */
uint32_t structural_plasticity_count_eliminated(
    const structural_plasticity_system_t* system
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_STRUCTURAL_PLASTICITY_H */
