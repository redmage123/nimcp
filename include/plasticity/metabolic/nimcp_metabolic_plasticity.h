/**
 * @file nimcp_metabolic_plasticity.h
 * @brief Energy/Metabolic Constraints for Synaptic Plasticity
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Biologically realistic metabolic constraints on synaptic plasticity
 * WHY:  Synaptic plasticity is energy-expensive; ATP depletion blocks LTP/LTD
 * HOW:  Track ATP levels, impose different costs for LTP vs LTD, model recovery dynamics
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * ENERGY COSTS OF PLASTICITY:
 * ---------------------------
 * 1. ATP Requirements for LTP:
 *    - Protein synthesis: 4-5 ATP per peptide bond
 *    - Spine morphology changes: Actin polymerization requires ATP
 *    - AMPA receptor trafficking: Vesicle transport and insertion
 *    - Calcium pumps: Restore ionic gradients after calcium influx
 *    - Reference: Harris et al. (2012) "Synaptic energy use and supply"
 *
 * 2. LTP vs LTD Energy Asymmetry:
 *    - LTP costs ~2-3x more ATP than LTD
 *    - LTP: New protein synthesis, spine growth, receptor insertion
 *    - LTD: Primarily receptor endocytosis and protein degradation
 *    - LTD reuses existing cellular machinery (lower synthesis cost)
 *    - Reference: Rangaraju et al. (2014) "Spatially stable mitochondrial compartments"
 *
 * 3. ATP Depletion Blocks Plasticity:
 *    - ATP < 50% baseline → LTP blocked
 *    - ATP < 30% baseline → LTD also blocked
 *    - Severe depletion → synaptic transmission impaired
 *    - Reference: Rangaraju et al. (2019) "Activity-driven local ATP synthesis"
 *
 * 4. Metabolic Recovery Dynamics:
 *    - Mitochondrial ATP production: τ_recovery ~ 10-30 seconds
 *    - Glycolysis contribution: Fast but limited capacity
 *    - Astrocyte lactate shuttle: Supports prolonged high activity
 *    - Sleep enhances recovery (see sleep bridge)
 *    - Reference: Magistretti & Allaman (2015) "Neuron-glia metabolic coupling"
 *
 * 5. Activity-Dependent Energy Demand:
 *    - High-frequency stimulation depletes ATP rapidly
 *    - Massed training more demanding than spaced training
 *    - Homeostatic scaling adjusts to energy availability
 *    - Reference: Bhatt & Bhalla (2016) "Energy constraints on synaptic learning"
 *
 * INTEGRATION POINTS:
 * -------------------
 * - SLEEP BRIDGE: Deep NREM sleep accelerates ATP recovery (2-3x faster)
 * - IMMUNE BRIDGE: Inflammation increases metabolic demand (1.5-3x cost)
 * - HOMEOSTATIC: Energy depletion triggers homeostatic downscaling
 * - STDP: Energy gates STDP window (no ATP = no plasticity)
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    METABOLIC PLASTICITY CONSTRAINTS                        ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                     ATP POOL (Energy State)                         │  ║
 * ║   │                                                                     │  ║
 * ║   │   100% ██████████████████████████  Full capacity                   │  ║
 * ║   │    70% ████████████████            LTP permitted                   │  ║
 * ║   │    50% ██████████                  LTP blocked                     │  ║
 * ║   │    30% ████                        LTD blocked                     │  ║
 * ║   │    10% █                           Critical depletion              │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                     ENERGY DYNAMICS                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   LTP Event  → ATP -= 3.0 units   (Protein synthesis + growth)    │  ║
 * ║   │   LTD Event  → ATP -= 1.0 units   (Receptor endocytosis)          │  ║
 * ║   │   Recovery   → ATP += recovery_rate * dt                           │  ║
 * ║   │   Sleep      → recovery_rate *= 2.5  (NREM restoration)           │  ║
 * ║   │   Inflammation → costs *= 2.0     (Immune metabolic demand)        │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  PLASTICITY GATING                                  │  ║
 * ║   │                                                                     │  ║
 * ║   │   IF (ATP >= min_ltp_threshold) → Allow LTP                        │  ║
 * ║   │   ELSE                           → Block LTP, scale existing LTP   │  ║
 * ║   │                                                                     │  ║
 * ║   │   IF (ATP >= min_ltd_threshold) → Allow LTD                        │  ║
 * ║   │   ELSE                           → Block LTD                       │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * REFERENCES:
 * -----------
 * - Harris, J.J., Jolivet, R., & Attwell, D. (2012). Synaptic energy use and supply.
 *   Neuron 75(5), 762-777.
 * - Rangaraju, V., Calloway, N., & Ryan, T.A. (2014). Activity-driven local ATP synthesis
 *   in synaptic compartments. Cell 156(4), 825-835.
 * - Rangaraju, V., Lauterbach, M., & Schuman, E.M. (2019). Spatially stable mitochondrial
 *   compartments fuel local translation during plasticity. Cell 176(1-2), 73-84.
 * - Magistretti, P.J., & Allaman, I. (2015). A cellular perspective on brain energy
 *   metabolism and functional imaging. Neuron 86(4), 883-901.
 * - Bhatt, D.H., & Bhalla, U.S. (2016). Energy-efficient information coding constrains
 *   synaptic learning. bioRxiv.
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

#ifndef NIMCP_METABOLIC_PLASTICITY_H
#define NIMCP_METABOLIC_PLASTICITY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants - Energy Costs and Thresholds
 * ============================================================================ */

/* ATP pool parameters (normalized to 100 = full capacity) */
#define METABOLIC_ATP_FULL_CAPACITY        100.0f  /**< 100% ATP level */
#define METABOLIC_ATP_INITIAL              100.0f  /**< Starting ATP level */
#define METABOLIC_ATP_MIN                  0.0f    /**< Minimum ATP (critical depletion) */

/* Energy costs for plasticity events (ATP units) */
#define METABOLIC_COST_LTP_BASE            3.0f    /**< LTP cost (protein synthesis + growth) */
#define METABOLIC_COST_LTD_BASE            1.0f    /**< LTD cost (receptor endocytosis) */
#define METABOLIC_COST_SPINE_GROWTH        2.0f    /**< Additional cost for spine morphology */
#define METABOLIC_COST_PROTEIN_SYNTH       1.5f    /**< Additional cost for protein synthesis */

/* ATP thresholds for plasticity gating */
#define METABOLIC_LTP_THRESHOLD            50.0f   /**< Minimum ATP for LTP (50% capacity) */
#define METABOLIC_LTD_THRESHOLD            30.0f   /**< Minimum ATP for LTD (30% capacity) */
#define METABOLIC_CRITICAL_THRESHOLD       10.0f   /**< Critical depletion (10% capacity) */

/* Recovery dynamics (ATP units per second) */
#define METABOLIC_RECOVERY_RATE_BASE       2.0f    /**< Base mitochondrial recovery rate */
#define METABOLIC_RECOVERY_RATE_GLYCOLYSIS 1.0f    /**< Glycolysis contribution */
#define METABOLIC_RECOVERY_RATE_ASTROCYTE  0.5f    /**< Astrocyte lactate shuttle */

/* Recovery time constants (seconds) */
#define METABOLIC_TAU_RECOVERY_FAST        10.0f   /**< Fast recovery (glycolysis) */
#define METABOLIC_TAU_RECOVERY_SLOW        30.0f   /**< Slow recovery (mitochondrial) */

/* Energy state thresholds */
#define METABOLIC_ENERGY_HEALTHY           0.70f   /**< 70% = healthy state */
#define METABOLIC_ENERGY_DEPLETED          0.50f   /**< 50% = depleted state */
#define METABOLIC_ENERGY_CRITICAL          0.30f   /**< 30% = critical state */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct metabolic_plasticity metabolic_plasticity_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Energy state classification
 *
 * Categorizes current ATP level for decision making
 */
typedef enum {
    ENERGY_STATE_HEALTHY = 0,    /**< ATP >= 70%: Full plasticity allowed */
    ENERGY_STATE_DEPLETED,       /**< ATP 50-70%: LTP blocked */
    ENERGY_STATE_CRITICAL,       /**< ATP 30-50%: LTP and LTD blocked */
    ENERGY_STATE_EMERGENCY       /**< ATP < 30%: Severe impairment */
} energy_state_t;

/**
 * @brief Metabolic event types for energy accounting
 *
 * Different plasticity events have different energy costs.
 * Note: Uses METABOLIC_EVENT_* prefix to avoid conflict with
 * plasticity_event_type_t in nimcp_plasticity_orchestrator.h
 */
typedef enum {
    METABOLIC_EVENT_LTP = 0,       /**< Long-term potentiation */
    METABOLIC_EVENT_LTD,           /**< Long-term depression */
    METABOLIC_EVENT_SPINE_GROWTH,  /**< Spine morphology change */
    METABOLIC_EVENT_PROTEIN_SYNTH, /**< Protein synthesis */
    METABOLIC_EVENT_COUNT
} metabolic_event_type_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief ATP pool state
 *
 * Tracks current energy availability
 */
typedef struct {
    float current_atp;           /**< Current ATP level [0-100] */
    float max_capacity;          /**< Maximum ATP capacity (100) */
    float recovery_rate;         /**< Current recovery rate (ATP/sec) */
    energy_state_t state;        /**< Current energy state classification */
    bool ltp_permitted;          /**< True if ATP >= LTP threshold */
    bool ltd_permitted;          /**< True if ATP >= LTD threshold */
    float depletion_severity;    /**< How depleted (0=full, 1=empty) */
} atp_pool_state_t;

/**
 * @brief Energy costs configuration
 *
 * Configurable costs for different plasticity events
 */
typedef struct {
    float ltp_cost;              /**< ATP cost for LTP event */
    float ltd_cost;              /**< ATP cost for LTD event */
    float spine_growth_cost;     /**< ATP cost for spine growth */
    float protein_synth_cost;    /**< ATP cost for protein synthesis */

    /* Threshold modifiers */
    float ltp_threshold;         /**< Minimum ATP for LTP */
    float ltd_threshold;         /**< Minimum ATP for LTD */

    /* Recovery parameters */
    float base_recovery_rate;    /**< Base ATP recovery rate */
    float glycolysis_rate;       /**< Glycolysis contribution */
    float astrocyte_rate;        /**< Astrocyte lactate shuttle */
} energy_cost_config_t;

/**
 * @brief Energy state change callback
 *
 * WHAT: Notification when energy state changes
 * WHY:  Other modules may need to adapt to energy availability
 * HOW:  Callback invoked on state transitions
 *
 * @param old_state Previous energy state
 * @param new_state New energy state
 * @param atp_level Current ATP percentage [0-100]
 * @param user_data User-provided callback data
 */
typedef void (*energy_state_callback_t)(
    energy_state_t old_state,
    energy_state_t new_state,
    float atp_level,
    void* user_data
);

/**
 * @brief Metabolic plasticity configuration
 */
typedef struct {
    /* Initial state */
    float initial_atp;           /**< Starting ATP level [0-100] */

    /* Energy costs */
    energy_cost_config_t costs;

    /* Feature enables */
    bool enable_ltp_gating;      /**< Gate LTP on ATP availability */
    bool enable_ltd_gating;      /**< Gate LTD on ATP availability */
    bool enable_dynamic_recovery; /**< Use dynamic recovery rates */
    bool enable_activity_scaling; /**< Scale costs with activity level */

    /* Sensitivity tuning */
    float cost_sensitivity;      /**< Energy cost multiplier [0.5-2.0] */
    float recovery_sensitivity;  /**< Recovery rate multiplier [0.5-2.0] */

    /* Callbacks */
    energy_state_callback_t state_callback;
    void* callback_user_data;
} metabolic_config_t;

/**
 * @brief Metabolic plasticity statistics
 */
typedef struct {
    /* Event counters */
    uint64_t total_ltp_events;       /**< Total LTP events requested */
    uint64_t total_ltd_events;       /**< Total LTD events requested */
    uint64_t ltp_blocked_count;      /**< LTP events blocked by energy */
    uint64_t ltd_blocked_count;      /**< LTD events blocked by energy */

    /* Energy statistics */
    float total_atp_consumed;        /**< Total ATP consumed */
    float total_atp_recovered;       /**< Total ATP recovered */
    float min_atp_reached;           /**< Minimum ATP level reached */
    float avg_atp_level;             /**< Average ATP level */

    /* State duration tracking */
    uint64_t time_in_healthy_ms;     /**< Time in healthy state */
    uint64_t time_in_depleted_ms;    /**< Time in depleted state */
    uint64_t time_in_critical_ms;    /**< Time in critical state */

    /* Updates */
    uint64_t total_updates;          /**< Total update calls */
} metabolic_stats_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default metabolic configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biological defaults
 * HOW:  Return struct with evidence-based parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int metabolic_plasticity_default_config(metabolic_config_t* config);

/**
 * @brief Create metabolic plasticity system
 *
 * WHAT: Initialize metabolic constraint tracking
 * WHY:  Enable realistic energy-limited plasticity
 * HOW:  Allocate structure, initialize ATP pool
 *
 * @param config Configuration (NULL for defaults)
 * @return New metabolic system or NULL on failure
 */
metabolic_plasticity_t* metabolic_plasticity_create(const metabolic_config_t* config);

/**
 * @brief Destroy metabolic plasticity system
 *
 * WHAT: Clean up metabolic system resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure and mutex
 *
 * @param metabolic Metabolic system to destroy
 */
void metabolic_plasticity_destroy(metabolic_plasticity_t* metabolic);

/* ============================================================================
 * Energy Accounting API
 * ============================================================================ */

/**
 * @brief Check if LTP is energetically permitted
 *
 * WHAT: Determine if sufficient ATP for LTP
 * WHY:  LTP requires ATP for protein synthesis
 * HOW:  Compare current ATP to LTP threshold
 *
 * @param metabolic Metabolic system
 * @return true if LTP permitted
 */
bool metabolic_plasticity_can_ltp(const metabolic_plasticity_t* metabolic);

/**
 * @brief Check if LTD is energetically permitted
 *
 * WHAT: Determine if sufficient ATP for LTD
 * WHY:  LTD requires ATP for endocytosis
 * HOW:  Compare current ATP to LTD threshold
 *
 * @param metabolic Metabolic system
 * @return true if LTD permitted
 */
bool metabolic_plasticity_can_ltd(const metabolic_plasticity_t* metabolic);

/**
 * @brief Consume ATP for plasticity event
 *
 * WHAT: Deduct ATP cost for plasticity event
 * WHY:  Model energy depletion from learning
 * HOW:  Subtract event cost from ATP pool
 *
 * @param metabolic Metabolic system
 * @param event_type Type of plasticity event
 * @param magnitude Event magnitude (scales cost) [0-1]
 * @return 0 on success, -1 if blocked by energy
 */
int metabolic_plasticity_consume_atp(
    metabolic_plasticity_t* metabolic,
    metabolic_event_type_t event_type,
    float magnitude
);

/**
 * @brief Get current ATP level
 *
 * WHAT: Query current ATP pool level
 * WHY:  Other modules may adjust based on energy
 * HOW:  Return current ATP percentage
 *
 * @param metabolic Metabolic system
 * @return ATP level [0-100]
 */
float metabolic_plasticity_get_atp_level(const metabolic_plasticity_t* metabolic);

/**
 * @brief Get current energy state
 *
 * WHAT: Query current energy state classification
 * WHY:  Determine system operating mode
 * HOW:  Return classified state enum
 *
 * @param metabolic Metabolic system
 * @return Current energy state
 */
energy_state_t metabolic_plasticity_get_energy_state(const metabolic_plasticity_t* metabolic);

/**
 * @brief Get ATP pool state
 *
 * WHAT: Get complete ATP pool state snapshot
 * WHY:  Detailed energy monitoring
 * HOW:  Copy current state to output struct
 *
 * @param metabolic Metabolic system
 * @param state Output state structure
 * @return 0 on success
 */
int metabolic_plasticity_get_atp_state(
    const metabolic_plasticity_t* metabolic,
    atp_pool_state_t* state
);

/* ============================================================================
 * Recovery API
 * ============================================================================ */

/**
 * @brief Update ATP recovery
 *
 * WHAT: Advance ATP recovery dynamics
 * WHY:  Model mitochondrial ATP production
 * HOW:  Add recovery_rate * dt to ATP pool (clamped)
 *
 * @param metabolic Metabolic system
 * @param delta_ms Time since last update (milliseconds)
 * @return 0 on success
 */
int metabolic_plasticity_update(metabolic_plasticity_t* metabolic, uint64_t delta_ms);

/**
 * @brief Set ATP recovery rate
 *
 * WHAT: Override current recovery rate
 * WHY:  Sleep/immune bridges modulate recovery
 * HOW:  Update recovery_rate field
 *
 * @param metabolic Metabolic system
 * @param recovery_rate New recovery rate (ATP/sec)
 * @return 0 on success
 */
int metabolic_plasticity_set_recovery_rate(
    metabolic_plasticity_t* metabolic,
    float recovery_rate
);

/**
 * @brief Get current recovery rate
 *
 * WHAT: Query current ATP recovery rate
 * WHY:  Monitor recovery dynamics
 * HOW:  Return current recovery_rate
 *
 * @param metabolic Metabolic system
 * @return Recovery rate (ATP/sec)
 */
float metabolic_plasticity_get_recovery_rate(const metabolic_plasticity_t* metabolic);

/**
 * @brief Manually restore ATP (for testing/recovery)
 *
 * WHAT: Directly set ATP level
 * WHY:  Testing, sleep restoration, or emergency recovery
 * HOW:  Set ATP to specified level (clamped to [0-100])
 *
 * @param metabolic Metabolic system
 * @param atp_level New ATP level [0-100]
 * @return 0 on success
 */
int metabolic_plasticity_restore_atp(metabolic_plasticity_t* metabolic, float atp_level);

/* ============================================================================
 * Modulation API
 * ============================================================================ */

/**
 * @brief Get energy-modulated learning rate
 *
 * WHAT: Scale learning rate by energy availability
 * WHY:  Graceful degradation as ATP depletes
 * HOW:  Multiply base LR by ATP percentage
 *
 * @param metabolic Metabolic system
 * @param base_lr Base learning rate
 * @return Effective learning rate
 */
float metabolic_plasticity_get_effective_lr(
    const metabolic_plasticity_t* metabolic,
    float base_lr
);

/**
 * @brief Get energy-modulated plasticity magnitude
 *
 * WHAT: Scale plasticity magnitude by ATP level
 * WHY:  Reduced ATP → reduced plasticity capacity
 * HOW:  Return scaling factor [0-1] based on ATP
 *
 * @param metabolic Metabolic system
 * @param event_type Type of plasticity event
 * @return Magnitude scaling factor [0-1]
 */
float metabolic_plasticity_get_magnitude_scale(
    const metabolic_plasticity_t* metabolic,
    metabolic_event_type_t event_type
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get metabolic statistics
 *
 * WHAT: Query accumulated statistics
 * WHY:  Monitor energy usage patterns
 * HOW:  Copy stats struct to output
 *
 * @param metabolic Metabolic system
 * @param stats Output statistics structure
 * @return 0 on success
 */
int metabolic_plasticity_get_stats(
    const metabolic_plasticity_t* metabolic,
    metabolic_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * WHAT: Clear accumulated statistics
 * WHY:  Start fresh monitoring period
 * HOW:  Zero all stat counters
 *
 * @param metabolic Metabolic system
 * @return 0 on success
 */
int metabolic_plasticity_reset_stats(metabolic_plasticity_t* metabolic);

/**
 * @brief Get LTP block rate
 *
 * WHAT: Percentage of LTP events blocked by energy
 * WHY:  Monitor energy bottleneck severity
 * HOW:  ltp_blocked / total_ltp_events
 *
 * @param metabolic Metabolic system
 * @return Block rate [0-1]
 */
float metabolic_plasticity_get_ltp_block_rate(const metabolic_plasticity_t* metabolic);

/**
 * @brief Get average ATP level
 *
 * WHAT: Average ATP level over monitoring period
 * WHY:  Assess overall energy health
 * HOW:  Return running average
 *
 * @param metabolic Metabolic system
 * @return Average ATP [0-100]
 */
float metabolic_plasticity_get_avg_atp(const metabolic_plasticity_t* metabolic);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Classify ATP level to energy state
 *
 * WHAT: Convert ATP percentage to state enum
 * WHY:  Standardized state classification
 * HOW:  Compare ATP to threshold constants
 *
 * @param atp_level ATP level [0-100]
 * @return Classified energy state
 */
energy_state_t metabolic_classify_energy_state(float atp_level);

/**
 * @brief Get energy cost for event type
 *
 * WHAT: Lookup ATP cost for plasticity event
 * WHY:  Centralized cost definition
 * HOW:  Return constant based on event type
 *
 * @param event_type Type of plasticity event
 * @return ATP cost (units)
 */
float metabolic_get_event_cost(metabolic_event_type_t event_type);

/**
 * @brief Get energy state name
 *
 * WHAT: Convert energy state enum to string
 * WHY:  Logging and debugging
 * HOW:  Return static string for state
 *
 * @param state Energy state
 * @return State name string
 */
const char* metabolic_energy_state_name(energy_state_t state);

/**
 * @brief Get plasticity event name
 *
 * WHAT: Convert event type enum to string
 * WHY:  Logging and debugging
 * HOW:  Return static string for event type
 *
 * @param event_type Event type
 * @return Event name string
 */
const char* metabolic_event_type_name(metabolic_event_type_t event_type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_METABOLIC_PLASTICITY_H */
