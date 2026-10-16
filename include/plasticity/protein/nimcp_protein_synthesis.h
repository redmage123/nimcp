/**
 * @file nimcp_protein_synthesis.h
 * @brief Protein Synthesis Constraints and Synaptic Tagging System
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Protein synthesis as limited resource for late-phase synaptic consolidation
 * WHY:  Only tagged synapses with sufficient plasticity-related proteins (PRPs) achieve
 *       permanent long-term memory, preventing runaway potentiation and ensuring selectivity
 * HOW:  Frey & Morris synaptic tagging and capture model - tags set by strong stimulation,
 *       capture available PRPs, achieve late-phase consolidation only when both are present
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SYNAPTIC TAGGING AND CAPTURE (FREY & MORRIS 1997):
 * ---------------------------------------------------
 * 1. Early vs Late-Phase LTP:
 *    - EARLY-PHASE (E-LTP): 1-3 hours, protein synthesis independent
 *    - LATE-PHASE (L-LTP): >3 hours, protein synthesis REQUIRED
 *    - Without PRPs: E-LTP decays back to baseline
 *    - With PRPs: E-LTP consolidates into L-LTP
 *    - Reference: Frey & Morris (1997) "Synaptic tagging and long-term potentiation"
 *
 * 2. Synaptic Tags:
 *    - Set by strong stimulation (e.g., high-frequency tetanus)
 *    - Local, synapse-specific marks (calcium-dependent kinase activation)
 *    - Persist ~2-3 hours without protein synthesis
 *    - Tag defines "capture competence" - ability to use PRPs
 *    - Reference: Redondo & Morris (2011) "Making memories last"
 *
 * 3. Plasticity-Related Proteins (PRPs):
 *    - Synthesized at soma (or local dendrites)
 *    - Limited pool shared across synapses
 *    - Diffuse to tagged synapses within critical window
 *    - Captured only by tagged synapses
 *    - Include: CaMKII, PKMζ, Arc/Arg3.1, BDNF, cytoskeletal proteins
 *    - Reference: Kelleher et al. (2004) "Translational control of synaptic plasticity"
 *
 * 4. Capture Mechanism:
 *    - Tag + sufficient PRPs → late-phase consolidation
 *    - Tag without PRPs → decay back to baseline
 *    - PRPs without tag → no consolidation (proteins wasted)
 *    - Competition: Multiple tagged synapses compete for limited PRPs
 *    - Reference: Sajikumar & Frey (2004) "Late-associativity, synaptic tagging"
 *
 * 5. Protein Synthesis Dynamics:
 *    - Basal synthesis rate: Low during wake, high during sleep
 *    - Induced synthesis: mRNA translation triggered by strong stimulation
 *    - Time course: 30-60 min to peak synthesis
 *    - Decay: Proteins degrade with half-life ~6-12 hours
 *    - Reference: Sutton & Schuman (2006) "Dendritic protein synthesis"
 *
 * SLEEP ENHANCEMENT OF PROTEIN SYNTHESIS:
 * ----------------------------------------
 * 1. Sleep Upregulates Protein Synthesis:
 *    - AWAKE: Low basal synthesis (metabolic cost)
 *    - LIGHT_NREM: Moderate synthesis (preparation)
 *    - DEEP_NREM: Maximum synthesis (2-3x wake levels)
 *    - REM: Moderate synthesis with enhanced delivery
 *    - Reference: Rasch & Born (2013) "About sleep's role in memory"
 *
 * 2. Sleep-Dependent Consolidation:
 *    - Tags set during wake persist into sleep
 *    - Sleep protein synthesis allows capture
 *    - Sleep deprivation → failed consolidation
 *    - Reference: Diekelmann & Born (2010) "Memory consolidation during sleep"
 *
 * IMMUNE MODULATION OF PROTEIN SYNTHESIS:
 * ----------------------------------------
 * 1. Pro-inflammatory Cytokines Suppress Synthesis:
 *    - IL-1β, IL-6, TNF-α reduce mRNA translation
 *    - Inflammation → reduced ribosomal activity
 *    - 30-50% reduction in protein synthesis rate
 *    - Prevents consolidation during immune challenge
 *    - Reference: Costa-Mattioli et al. (2009) "Translation dysregulation in brain disease"
 *
 * 2. Inflammation Effects on Consolidation:
 *    - NONE: 100% synthesis rate
 *    - LOCAL: 90% synthesis rate
 *    - REGIONAL: 70% synthesis rate
 *    - SYSTEMIC: 40% synthesis rate
 *    - STORM: 10% synthesis rate (critical impairment)
 *
 * 3. Anti-inflammatory Restoration:
 *    - IL-10 restores protein synthesis capacity
 *    - Allows consolidation after inflammation resolves
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    PROTEIN SYNTHESIS & TAGGING SYSTEM                      ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  SYNAPTIC TAG LIFECYCLE                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   Strong LTP ──→ Set Tag ──→ Capture PRPs ──→ L-LTP                │  ║
 * ║   │                      │              │                               │  ║
 * ║   │                      │              └──→ PRPs < threshold ──→ Decay │  ║
 * ║   │                      └──→ Tag expires (2-3h) ──→ Decay              │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  PRP POOL DYNAMICS                                  │  ║
 * ║   │                                                                     │  ║
 * ║   │   [PRP Pool]  ←── Synthesis (soma/dendrites)                       │  ║
 * ║   │        │          Rate: Sleep-modulated, Immune-suppressed          │  ║
 * ║   │        ├──→ Captured by Tagged Synapse A                           │  ║
 * ║   │        ├──→ Captured by Tagged Synapse B                           │  ║
 * ║   │        └──→ Decay (half-life ~6-12h)                               │  ║
 * ║   │                                                                     │  ║
 * ║   │   Competition: Multiple tags share limited PRP pool                │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  MODULATION PATHWAYS                                │  ║
 * ║   │                                                                     │  ║
 * ║   │   SLEEP STATE          PRP Synthesis Rate                          │  ║
 * ║   │   ────────────────────────────────────────────────                 │  ║
 * ║   │   AWAKE                1.0x (baseline)                             │  ║
 * ║   │   DROWSY               1.2x                                        │  ║
 * ║   │   LIGHT_NREM           1.5x                                        │  ║
 * ║   │   DEEP_NREM            2.5x (maximum)                              │  ║
 * ║   │   REM                  1.8x                                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   INFLAMMATION         PRP Synthesis Suppression                   │  ║
 * ║   │   ────────────────────────────────────────────────                 │  ║
 * ║   │   NONE                 1.0x (normal)                               │  ║
 * ║   │   LOCAL                0.9x                                        │  ║
 * ║   │   REGIONAL             0.7x                                        │  ║
 * ║   │   SYSTEMIC             0.4x                                        │  ║
 * ║   │   STORM                0.1x (severe)                               │  ║
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

#ifndef NIMCP_PROTEIN_SYNTHESIS_H
#define NIMCP_PROTEIN_SYNTHESIS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Synaptic tag duration (milliseconds) */
#define PROTEIN_TAG_DURATION_MS          (10800000)  /**< 3 hours tag persistence */
#define PROTEIN_TAG_HALF_LIFE_MS         (7200000)   /**< 2 hour tag decay half-life */

/* PRP synthesis rates (units/ms) */
#define PROTEIN_BASE_SYNTHESIS_RATE      0.01f       /**< Baseline synthesis during wake */
#define PROTEIN_DECAY_RATE               0.00001f    /**< PRP decay (half-life ~6-12h) */
#define PROTEIN_INDUCED_SYNTHESIS_BOOST  2.0f        /**< Boost from strong stimulation */

/* PRP capture thresholds */
#define PROTEIN_CAPTURE_THRESHOLD_MIN    10.0f       /**< Minimum PRPs for consolidation */
#define PROTEIN_CAPTURE_THRESHOLD_OPTIMAL 50.0f      /**< Optimal PRPs for full consolidation */

/* Sleep modulation factors (synthesis rate multipliers) */
#define PROTEIN_SLEEP_SYNTH_AWAKE        1.0f        /**< Baseline synthesis */
#define PROTEIN_SLEEP_SYNTH_DROWSY       1.2f        /**< Slight increase */
#define PROTEIN_SLEEP_SYNTH_LIGHT_NREM   1.5f        /**< Moderate increase */
#define PROTEIN_SLEEP_SYNTH_DEEP_NREM    2.5f        /**< Maximum synthesis */
#define PROTEIN_SLEEP_SYNTH_REM          1.8f        /**< High synthesis + delivery */

/* Inflammation suppression factors (synthesis rate multipliers) */
#define PROTEIN_INFLAM_SYNTH_NONE        1.0f        /**< No suppression */
#define PROTEIN_INFLAM_SYNTH_LOCAL       0.9f        /**< 10% reduction */
#define PROTEIN_INFLAM_SYNTH_REGIONAL    0.7f        /**< 30% reduction */
#define PROTEIN_INFLAM_SYNTH_SYSTEMIC    0.4f        /**< 60% reduction */
#define PROTEIN_INFLAM_SYNTH_STORM       0.1f        /**< 90% reduction (severe) */

/* Consolidation callback severity levels */
#define PROTEIN_CONSOL_SUCCESS           0           /**< Successful consolidation */
#define PROTEIN_CONSOL_PARTIAL           1           /**< Partial consolidation (low PRPs) */
#define PROTEIN_CONSOL_FAILED_NO_TAG     2           /**< No tag present */
#define PROTEIN_CONSOL_FAILED_NO_PRP     3           /**< Insufficient PRPs */

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Synaptic tag state
 *
 * Marks synapse as capture-competent for PRPs
 */
typedef enum {
    TAG_STATE_UNTAGGED,        /**< No tag present */
    TAG_STATE_TAGGED,          /**< Tag active, can capture PRPs */
    TAG_STATE_CONSOLIDATING,   /**< Capturing PRPs */
    TAG_STATE_CONSOLIDATED     /**< L-LTP achieved */
} tag_state_t;

/**
 * @brief Synaptic tag structure
 *
 * Local synaptic mark enabling PRP capture
 */
typedef struct {
    tag_state_t state;           /**< Current tag state */
    uint64_t tag_set_time_ms;    /**< When tag was set */
    float tag_strength;          /**< Tag strength [0-1], decays over time */
    float prps_captured;         /**< PRPs accumulated by this tag */
    bool consolidation_achieved; /**< L-LTP achieved */
    uint32_t synapse_id;         /**< ID of tagged synapse */
} synaptic_tag_t;

/**
 * @brief PRP pool state
 *
 * Shared pool of plasticity-related proteins
 */
typedef struct {
    float current_prp_pool;      /**< Current PRP availability */
    float max_prp_pool;          /**< Maximum pool capacity */
    float synthesis_rate;        /**< Current synthesis rate (units/ms) */
    float decay_rate;            /**< PRP decay rate */
    float sleep_modulation;      /**< Sleep-dependent synthesis boost [1.0-2.5] */
    float immune_suppression;    /**< Inflammation-dependent suppression [0.1-1.0] */
    uint64_t total_prps_synthesized; /**< Cumulative PRPs created */
    uint64_t total_prps_captured;    /**< Cumulative PRPs captured by tags */
    uint64_t total_prps_decayed;     /**< Cumulative PRPs lost to decay */
} prp_pool_state_t;

/**
 * @brief Consolidation event
 *
 * Notification when synapse achieves L-LTP or fails
 */
typedef struct {
    uint32_t synapse_id;         /**< Synapse that consolidated */
    tag_state_t final_state;     /**< Final tag state */
    float prps_used;             /**< PRPs consumed */
    bool success;                /**< True if consolidated */
    uint32_t severity;           /**< Event severity (PROTEIN_CONSOL_*) */
    uint64_t timestamp_ms;       /**< Event time */
} consolidation_event_t;

/**
 * @brief Consolidation event callback
 *
 * Called when consolidation attempt completes
 *
 * @param event Consolidation event details
 * @param user_data User context pointer
 */
typedef void (*consolidation_callback_t)(
    const consolidation_event_t* event,
    void* user_data
);

/**
 * @brief Protein synthesis system configuration
 */
typedef struct {
    /* PRP pool parameters */
    float initial_prp_pool;           /**< Starting PRP pool size */
    float max_prp_pool;               /**< Maximum PRP pool capacity */
    float base_synthesis_rate;        /**< Baseline synthesis rate */
    float decay_rate;                 /**< PRP decay rate */

    /* Tag parameters */
    uint64_t tag_duration_ms;         /**< How long tags persist */
    float tag_decay_rate;             /**< Tag strength decay rate */
    float capture_threshold_min;      /**< Min PRPs for consolidation */
    float capture_threshold_optimal;  /**< Optimal PRPs for full consolidation */

    /* Modulation enables */
    bool enable_sleep_modulation;     /**< Sleep enhances synthesis */
    bool enable_immune_suppression;   /**< Inflammation suppresses synthesis */

    /* Callbacks */
    consolidation_callback_t consolidation_callback;  /**< Event callback */
    void* callback_user_data;         /**< User data for callback */

    /* Limits */
    uint32_t max_tags;                /**< Maximum concurrent tags */
} protein_synthesis_config_t;

/**
 * @brief Protein synthesis system statistics
 */
typedef struct {
    /* Tag statistics */
    uint64_t total_tags_set;          /**< Total tags created */
    uint64_t total_tags_expired;      /**< Tags that expired */
    uint64_t total_consolidations;    /**< Successful L-LTP */
    uint64_t total_consolidation_failures; /**< Failed consolidations */

    /* PRP statistics */
    uint64_t total_prps_synthesized;  /**< Total PRPs created */
    uint64_t total_prps_captured;     /**< Total PRPs used */
    uint64_t total_prps_decayed;      /**< Total PRPs wasted */
    float current_prp_pool;           /**< Current available PRPs */

    /* Modulation statistics */
    float avg_sleep_boost;            /**< Average sleep synthesis boost */
    float avg_immune_suppression;     /**< Average inflammation suppression */

    /* Efficiency metrics */
    float capture_efficiency;         /**< PRPs captured / PRPs synthesized */
    float consolidation_success_rate; /**< Consolidations / tags set */
} protein_synthesis_stats_t;

/**
 * @brief Protein synthesis system
 */
typedef struct protein_synthesis_struct* protein_synthesis_system_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * WHAT: Get default protein synthesis configuration
 * WHY:  Provide biologically-plausible defaults
 * HOW:  Return configuration based on Frey & Morris parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int protein_synthesis_default_config(protein_synthesis_config_t* config);

/**
 * WHAT: Create protein synthesis system
 * WHY:  Initialize PRP pool and tagging mechanism
 * HOW:  Allocate structures, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New system or NULL on failure
 */
protein_synthesis_system_t protein_synthesis_create(
    const protein_synthesis_config_t* config
);

/**
 * WHAT: Destroy protein synthesis system
 * WHY:  Clean up resources
 * HOW:  Free allocations, invoke callbacks
 *
 * @param system System to destroy
 */
void protein_synthesis_destroy(protein_synthesis_system_t system);

/* ============================================================================
 * Synaptic Tag API
 * ============================================================================ */

/**
 * WHAT: Set synaptic tag on synapse
 * WHY:  Mark synapse as capture-competent after strong stimulation
 * HOW:  Create tag with strength proportional to stimulation
 *
 * @param system Protein synthesis system
 * @param synapse_id Synapse to tag
 * @param stimulation_strength Strength of LTP induction [0-1]
 * @return 0 on success, -1 on error
 */
int protein_synthesis_set_tag(
    protein_synthesis_system_t system,
    uint32_t synapse_id,
    float stimulation_strength
);

/**
 * WHAT: Remove synaptic tag
 * WHY:  Explicitly clear tag (e.g., after consolidation or failure)
 * HOW:  Remove tag from active list
 *
 * @param system Protein synthesis system
 * @param synapse_id Synapse to untag
 * @return 0 on success, -1 on error
 */
int protein_synthesis_remove_tag(
    protein_synthesis_system_t system,
    uint32_t synapse_id
);

/**
 * WHAT: Check if synapse has active tag
 * WHY:  Query tag state for consolidation logic
 * HOW:  Lookup tag in active tag list
 *
 * @param system Protein synthesis system
 * @param synapse_id Synapse to check
 * @return true if tagged
 */
bool protein_synthesis_is_tagged(
    const protein_synthesis_system_t system,
    uint32_t synapse_id
);

/**
 * WHAT: Get tag state for synapse
 * WHY:  Retrieve detailed tag information
 * HOW:  Return tag structure or NULL if not tagged
 *
 * @param system Protein synthesis system
 * @param synapse_id Synapse to query
 * @param tag Output tag structure
 * @return 0 on success (tag exists), -1 if not tagged
 */
int protein_synthesis_get_tag(
    const protein_synthesis_system_t system,
    uint32_t synapse_id,
    synaptic_tag_t* tag
);

/* ============================================================================
 * PRP Pool API
 * ============================================================================ */

/**
 * WHAT: Get current PRP pool level
 * WHY:  Check if sufficient PRPs available for consolidation
 * HOW:  Return current pool value
 *
 * @param system Protein synthesis system
 * @return Current PRP pool level
 */
float protein_synthesis_get_prp_pool(
    const protein_synthesis_system_t system
);

/**
 * WHAT: Get effective synthesis rate
 * WHY:  Check current protein synthesis rate with modulation
 * HOW:  Return base_rate * sleep_mod * immune_suppression
 *
 * @param system Protein synthesis system
 * @return Effective synthesis rate (units/ms)
 */
float protein_synthesis_get_synthesis_rate(
    const protein_synthesis_system_t system
);

/**
 * WHAT: Trigger induced protein synthesis
 * WHY:  Strong stimulation triggers mRNA translation
 * HOW:  Boost synthesis rate temporarily
 *
 * @param system Protein synthesis system
 * @param boost_factor Synthesis boost [1.0-3.0]
 * @param duration_ms How long boost lasts
 * @return 0 on success
 */
int protein_synthesis_induce_synthesis(
    protein_synthesis_system_t system,
    float boost_factor,
    uint64_t duration_ms
);

/**
 * WHAT: Manually add PRPs to pool
 * WHY:  Simulate external protein synthesis or injection
 * HOW:  Add to current pool (capped at max)
 *
 * @param system Protein synthesis system
 * @param prp_amount PRPs to add
 * @return 0 on success
 */
int protein_synthesis_add_prps(
    protein_synthesis_system_t system,
    float prp_amount
);

/* ============================================================================
 * Consolidation API
 * ============================================================================ */

/**
 * WHAT: Attempt to consolidate tagged synapse
 * WHY:  Capture available PRPs to achieve L-LTP
 * HOW:  Check tag + PRP availability, transfer PRPs, update state
 *
 * @param system Protein synthesis system
 * @param synapse_id Synapse to consolidate
 * @return 0 on success (consolidated), -1 on failure
 */
int protein_synthesis_consolidate_synapse(
    protein_synthesis_system_t system,
    uint32_t synapse_id
);

/**
 * WHAT: Check if synapse can consolidate
 * WHY:  Determine if tag + PRPs are sufficient
 * HOW:  Verify tag exists and PRP pool >= threshold
 *
 * @param system Protein synthesis system
 * @param synapse_id Synapse to check
 * @return true if can consolidate
 */
bool protein_synthesis_can_consolidate(
    const protein_synthesis_system_t system,
    uint32_t synapse_id
);

/**
 * WHAT: Get consolidation progress for synapse
 * WHY:  Monitor how close synapse is to L-LTP
 * HOW:  Return PRPs captured / threshold
 *
 * @param system Protein synthesis system
 * @param synapse_id Synapse to check
 * @return Consolidation progress [0-1]
 */
float protein_synthesis_get_consolidation_progress(
    const protein_synthesis_system_t system,
    uint32_t synapse_id
);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * WHAT: Update protein synthesis system
 * WHY:  Advance time-dependent processes (synthesis, decay, tag expiration)
 * HOW:  Synthesize PRPs, decay PRPs/tags, expire old tags
 *
 * @param system Protein synthesis system
 * @param delta_ms Time since last update (milliseconds)
 * @return 0 on success
 */
int protein_synthesis_update(
    protein_synthesis_system_t system,
    uint64_t delta_ms
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * WHAT: Get protein synthesis statistics
 * WHY:  Monitor system efficiency and resource usage
 * HOW:  Return accumulated statistics
 *
 * @param system Protein synthesis system
 * @param stats Output statistics structure
 * @return 0 on success
 */
int protein_synthesis_get_stats(
    const protein_synthesis_system_t system,
    protein_synthesis_stats_t* stats
);

/**
 * WHAT: Reset statistics counters
 * WHY:  Start fresh tracking period
 * HOW:  Zero statistics (preserve current state)
 *
 * @param system Protein synthesis system
 * @return 0 on success
 */
int protein_synthesis_reset_stats(protein_synthesis_system_t system);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * WHAT: Get number of active tags
 * WHY:  Monitor system load and competition
 * HOW:  Return count of tagged synapses
 *
 * @param system Protein synthesis system
 * @return Number of active tags
 */
uint32_t protein_synthesis_get_num_tags(
    const protein_synthesis_system_t system
);

/**
 * WHAT: Get all active tag IDs
 * WHY:  Enumerate tagged synapses for analysis
 * HOW:  Fill array with synapse IDs
 *
 * @param system Protein synthesis system
 * @param synapse_ids Output array (must hold at least max_tags)
 * @param max_ids Size of output array
 * @return Number of IDs written
 */
uint32_t protein_synthesis_get_tag_ids(
    const protein_synthesis_system_t system,
    uint32_t* synapse_ids,
    uint32_t max_ids
);

/**
 * WHAT: Get PRP pool state
 * WHY:  Detailed PRP pool information
 * HOW:  Return pool state structure
 *
 * @param system Protein synthesis system
 * @param state Output state structure
 * @return 0 on success
 */
int protein_synthesis_get_prp_state(
    const protein_synthesis_system_t system,
    prp_pool_state_t* state
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PROTEIN_SYNTHESIS_H */
