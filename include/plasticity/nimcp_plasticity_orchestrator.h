/**
 * @file nimcp_plasticity_orchestrator.h
 * @brief Unified Plasticity Orchestrator - Coordinates All Plasticity Mechanisms
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Central coordinator for all NIMCP plasticity mechanisms
 * WHY:  Enables coherent multi-mechanism plasticity with proper sequencing
 * HOW:  Orchestrates STDP, BCM, homeostatic, structural, metabolic, etc.
 *
 * Orchestration Order (biologically-motivated):
 * 1. Metabolic check - ensure sufficient ATP
 * 2. Calcium dynamics - compute [Ca²⁺] from spike activity
 * 3. STDP/Triplet STDP - Hebbian weight changes
 * 4. Heterosynaptic - neighbor depression
 * 5. BCM - threshold-based selectivity
 * 6. Homeostatic - maintain target rates
 * 7. Metaplasticity - update sliding thresholds
 * 8. Protein synthesis - tag capture for consolidation
 * 9. Structural - spine dynamics
 * 10. Astrocyte - glial modulation
 *
 * All mechanisms are modulated by:
 * - Sleep state (via sleep bridges)
 * - Immune state (via immune bridges)
 * - Neuromodulators (DA, 5-HT, ACh, NE)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PLASTICITY_ORCHESTRATOR_H
#define NIMCP_PLASTICITY_ORCHESTRATOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

/* Core plasticity modules */
struct stdp_synapse_struct;
struct triplet_stdp_synapse_struct;
struct bcm_synapse_struct;
struct homeostatic_state_struct;
struct eligibility_trace_struct;
struct stp_synapse_struct;
struct dendritic_compartment_struct;
struct predictive_coding_layer_struct;

/* New enhancement modules */
struct structural_plasticity_struct;
struct heterosynaptic_state_struct;
struct calcium_dynamics_struct;
struct astrocyte_state_struct;
struct protein_synthesis_struct;
struct extended_metaplasticity_struct;
struct metabolic_state_struct;

/* Integration modules */
struct brain_immune_system;
struct sleep_system_struct;
struct neuromodulator_system_struct;

/* Opaque orchestrator handle */
typedef struct plasticity_orchestrator_struct plasticity_orchestrator_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * Plasticity event types for callbacks
 */
typedef enum {
    PLASTICITY_EVENT_LTP,              /* Long-term potentiation occurred */
    PLASTICITY_EVENT_LTD,              /* Long-term depression occurred */
    PLASTICITY_EVENT_SPINE_FORMED,     /* New spine created */
    PLASTICITY_EVENT_SPINE_ELIMINATED, /* Spine pruned */
    PLASTICITY_EVENT_CONSOLIDATION,    /* Memory consolidated */
    PLASTICITY_EVENT_THRESHOLD_SHIFT,  /* Metaplasticity threshold changed */
    PLASTICITY_EVENT_HOMEOSTATIC_SCALE,/* Homeostatic scaling applied */
    PLASTICITY_EVENT_ENERGY_DEPLETED,  /* ATP below threshold */
    PLASTICITY_EVENT_ENERGY_RESTORED,  /* ATP recovered */
    PLASTICITY_EVENT_ASTROCYTE_RELEASE,/* Gliotransmitter released */
    PLASTICITY_EVENT_CALCIUM_SPIKE,    /* [Ca²⁺] crossed threshold */
    PLASTICITY_EVENT_COUNT
} plasticity_event_type_t;

/**
 * Plasticity event data
 */
typedef struct {
    plasticity_event_type_t type;
    uint32_t synapse_id;
    uint32_t neuron_id;
    float old_value;
    float new_value;
    float delta;
    uint64_t timestamp_ms;
    void* context;
} plasticity_event_t;

/**
 * Callback for plasticity events
 */
typedef void (*plasticity_event_callback_t)(
    const plasticity_event_t* event,
    void* user_data
);

/**
 * Callback for module updates
 */
typedef void (*plasticity_update_callback_t)(
    plasticity_orchestrator_t* orchestrator,
    uint64_t delta_ms,
    void* user_data
);

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * Which plasticity mechanisms are enabled
 */
typedef struct {
    bool enable_classic_stdp;
    bool enable_triplet_stdp;
    bool enable_bcm;
    bool enable_homeostatic;
    bool enable_eligibility;
    bool enable_stp;
    bool enable_dendritic;
    bool enable_predictive;
    bool enable_structural;
    bool enable_heterosynaptic;
    bool enable_calcium;
    bool enable_astrocyte;
    bool enable_protein_synthesis;
    bool enable_metaplasticity;
    bool enable_metabolic;
} plasticity_enable_flags_t;

/**
 * Orchestrator configuration
 */
typedef struct {
    /* Enable flags for each mechanism */
    plasticity_enable_flags_t enabled;

    /* Global modulation factors */
    float global_learning_rate;      /* Scales all learning (default: 1.0) */
    float sleep_modulation;          /* From sleep bridge (default: 1.0) */
    float immune_modulation;         /* From immune bridge (default: 1.0) */

    /* Timing parameters */
    uint64_t update_interval_ms;     /* How often to run full update (default: 1) */
    uint64_t consolidation_interval_ms; /* Protein synthesis check (default: 60000) */
    uint64_t homeostatic_interval_ms;   /* Scaling check (default: 1000) */

    /* Integration options */
    bool connect_sleep_bridges;      /* Auto-connect sleep bridges */
    bool connect_immune_bridges;     /* Auto-connect immune bridges */
    bool connect_bio_async;          /* Register with bio-async router */

    /* Logging level */
    uint8_t log_level;               /* 0=off, 1=errors, 2=warnings, 3=info, 4=debug */

} plasticity_orchestrator_config_t;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * Get default orchestrator configuration
 *
 * WHAT: Returns sensible defaults for orchestrator
 * WHY:  Easy initialization with common settings
 * HOW:  All mechanisms enabled, standard intervals
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int plasticity_orchestrator_default_config(plasticity_orchestrator_config_t* config);

/**
 * Create plasticity orchestrator
 *
 * WHAT: Allocates and initializes the orchestrator
 * WHY:  Central point for plasticity management
 * HOW:  Creates internal state, initializes all enabled mechanisms
 *
 * @param config Configuration (NULL for defaults)
 * @return Orchestrator handle, or NULL on error
 */
plasticity_orchestrator_t* plasticity_orchestrator_create(
    const plasticity_orchestrator_config_t* config
);

/**
 * Destroy plasticity orchestrator
 *
 * WHAT: Frees all resources
 * WHY:  Clean shutdown
 * HOW:  Disconnects bridges, frees memory
 *
 * @param orchestrator Orchestrator to destroy
 */
void plasticity_orchestrator_destroy(plasticity_orchestrator_t* orchestrator);

/* ============================================================================
 * Integration Functions
 * ============================================================================ */

/**
 * Connect to brain immune system
 *
 * WHAT: Links orchestrator to immune system for cytokine modulation
 * WHY:  Inflammation affects all plasticity mechanisms
 * HOW:  Registers as observer, receives cytokine updates
 *
 * @param orchestrator Orchestrator
 * @param immune Brain immune system
 * @return 0 on success, -1 on error
 */
int plasticity_orchestrator_connect_immune(
    plasticity_orchestrator_t* orchestrator,
    struct brain_immune_system* immune
);

/**
 * Connect to sleep system
 *
 * WHAT: Links orchestrator to sleep system for state modulation
 * WHY:  Sleep dramatically affects plasticity (consolidation, scaling)
 * HOW:  Registers callback for sleep state changes
 *
 * @param orchestrator Orchestrator
 * @param sleep Sleep system
 * @return 0 on success, -1 on error
 */
int plasticity_orchestrator_connect_sleep(
    plasticity_orchestrator_t* orchestrator,
    struct sleep_system_struct* sleep
);

/**
 * Connect to neuromodulator system
 *
 * WHAT: Links orchestrator to neuromodulators (DA, 5-HT, ACh, NE)
 * WHY:  Neuromodulators gate plasticity
 * HOW:  Reads modulator concentrations during updates
 *
 * @param orchestrator Orchestrator
 * @param neuromod Neuromodulator system
 * @return 0 on success, -1 on error
 */
int plasticity_orchestrator_connect_neuromodulators(
    plasticity_orchestrator_t* orchestrator,
    struct neuromodulator_system_struct* neuromod
);

/**
 * Connect to bio-async router
 *
 * WHAT: Registers with bio-async for inter-module messaging
 * WHY:  Enables event-driven plasticity coordination
 * HOW:  Registers as BIO_MODULE_PLASTICITY_ORCHESTRATOR
 *
 * @param orchestrator Orchestrator
 * @return 0 on success, -1 on error
 */
int plasticity_orchestrator_connect_bio_async(
    plasticity_orchestrator_t* orchestrator
);

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

/**
 * Register callback for plasticity events
 *
 * @param orchestrator Orchestrator
 * @param event_type Which event type to listen for
 * @param callback Callback function
 * @param user_data User context
 * @return Callback ID (>0), or -1 on error
 */
int plasticity_orchestrator_register_event_callback(
    plasticity_orchestrator_t* orchestrator,
    plasticity_event_type_t event_type,
    plasticity_event_callback_t callback,
    void* user_data
);

/**
 * Unregister event callback
 *
 * @param orchestrator Orchestrator
 * @param callback_id ID from registration
 * @return 0 on success, -1 on error
 */
int plasticity_orchestrator_unregister_event_callback(
    plasticity_orchestrator_t* orchestrator,
    int callback_id
);

/**
 * Register pre-update callback
 *
 * Called before each plasticity update cycle
 *
 * @param orchestrator Orchestrator
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success, -1 on error
 */
int plasticity_orchestrator_register_pre_update(
    plasticity_orchestrator_t* orchestrator,
    plasticity_update_callback_t callback,
    void* user_data
);

/**
 * Register post-update callback
 *
 * Called after each plasticity update cycle
 *
 * @param orchestrator Orchestrator
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success, -1 on error
 */
int plasticity_orchestrator_register_post_update(
    plasticity_orchestrator_t* orchestrator,
    plasticity_update_callback_t callback,
    void* user_data
);

/* ============================================================================
 * Main Update Function
 * ============================================================================ */

/**
 * Run plasticity update cycle
 *
 * WHAT: Executes all enabled plasticity mechanisms in proper order
 * WHY:  Maintains biological sequencing and interactions
 * HOW:  See orchestration order in file header
 *
 * @param orchestrator Orchestrator
 * @param delta_ms Time since last update
 * @return 0 on success, -1 on error
 */
int plasticity_orchestrator_update(
    plasticity_orchestrator_t* orchestrator,
    uint64_t delta_ms
);

/* ============================================================================
 * Spike Event Handling
 * ============================================================================ */

/**
 * Process presynaptic spike
 *
 * WHAT: Handles a presynaptic spike across all mechanisms
 * WHY:  Updates traces, triggers STDP, calcium influx, etc.
 * HOW:  Propagates to all enabled mechanisms
 *
 * @param orchestrator Orchestrator
 * @param synapse_id Synapse identifier
 * @param timestamp_ms Spike time
 * @return 0 on success, -1 on error
 */
int plasticity_orchestrator_pre_spike(
    plasticity_orchestrator_t* orchestrator,
    uint32_t synapse_id,
    uint64_t timestamp_ms
);

/**
 * Process postsynaptic spike
 *
 * WHAT: Handles a postsynaptic spike across all mechanisms
 * WHY:  Updates traces, triggers STDP, calcium spikes, etc.
 * HOW:  Propagates to all enabled mechanisms
 *
 * @param orchestrator Orchestrator
 * @param neuron_id Neuron identifier
 * @param timestamp_ms Spike time
 * @return 0 on success, -1 on error
 */
int plasticity_orchestrator_post_spike(
    plasticity_orchestrator_t* orchestrator,
    uint32_t neuron_id,
    uint64_t timestamp_ms
);

/**
 * Process reward signal
 *
 * WHAT: Delivers reward/punishment for reinforcement learning
 * WHY:  Triggers dopamine, eligibility trace updates
 * HOW:  Modulates weight changes based on accumulated traces
 *
 * @param orchestrator Orchestrator
 * @param reward_magnitude Reward value (positive) or punishment (negative)
 * @param timestamp_ms Signal time
 * @return 0 on success, -1 on error
 */
int plasticity_orchestrator_reward(
    plasticity_orchestrator_t* orchestrator,
    float reward_magnitude,
    uint64_t timestamp_ms
);

/* ============================================================================
 * Module Access Functions
 * ============================================================================ */

/**
 * Get synapse weight
 *
 * @param orchestrator Orchestrator
 * @param synapse_id Synapse identifier
 * @return Current weight, or NaN on error
 */
float plasticity_orchestrator_get_weight(
    const plasticity_orchestrator_t* orchestrator,
    uint32_t synapse_id
);

/**
 * Set synapse weight (manual override)
 *
 * @param orchestrator Orchestrator
 * @param synapse_id Synapse identifier
 * @param weight New weight
 * @return 0 on success, -1 on error
 */
int plasticity_orchestrator_set_weight(
    plasticity_orchestrator_t* orchestrator,
    uint32_t synapse_id,
    float weight
);

/**
 * Get current ATP level
 *
 * @param orchestrator Orchestrator
 * @return ATP level [0, 1], or -1 on error
 */
float plasticity_orchestrator_get_atp_level(
    const plasticity_orchestrator_t* orchestrator
);

/**
 * Get current calcium concentration
 *
 * @param orchestrator Orchestrator
 * @param compartment_id Dendritic compartment
 * @return [Ca²⁺] in μM, or -1 on error
 */
float plasticity_orchestrator_get_calcium(
    const plasticity_orchestrator_t* orchestrator,
    uint32_t compartment_id
);

/**
 * Get metaplasticity threshold
 *
 * @param orchestrator Orchestrator
 * @param neuron_id Neuron identifier
 * @return BCM threshold, or -1 on error
 */
float plasticity_orchestrator_get_threshold(
    const plasticity_orchestrator_t* orchestrator,
    uint32_t neuron_id
);

/* ============================================================================
 * Statistics and Monitoring
 * ============================================================================ */

/**
 * Plasticity statistics
 */
typedef struct {
    /* Event counts */
    uint64_t ltp_count;
    uint64_t ltd_count;
    uint64_t spines_formed;
    uint64_t spines_eliminated;
    uint64_t consolidation_events;

    /* Weight statistics */
    float mean_weight;
    float std_weight;
    float min_weight;
    float max_weight;

    /* Energy statistics */
    float mean_atp;
    float min_atp;
    uint64_t energy_blocked_events;

    /* Timing */
    uint64_t total_updates;
    uint64_t last_update_ms;
    float mean_update_time_us;

} plasticity_stats_t;

/**
 * Get current statistics
 *
 * @param orchestrator Orchestrator
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int plasticity_orchestrator_get_stats(
    const plasticity_orchestrator_t* orchestrator,
    plasticity_stats_t* stats
);

/**
 * Reset statistics counters
 *
 * @param orchestrator Orchestrator
 * @return 0 on success, -1 on error
 */
int plasticity_orchestrator_reset_stats(
    plasticity_orchestrator_t* orchestrator
);

/* ============================================================================
 * State Persistence
 * ============================================================================ */

/**
 * Serialize orchestrator state
 *
 * @param orchestrator Orchestrator
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @param bytes_written Actual bytes written
 * @return 0 on success, -1 on error
 */
int plasticity_orchestrator_serialize(
    const plasticity_orchestrator_t* orchestrator,
    uint8_t* buffer,
    size_t buffer_size,
    size_t* bytes_written
);

/**
 * Deserialize orchestrator state
 *
 * @param orchestrator Orchestrator
 * @param buffer Input buffer
 * @param buffer_size Buffer size
 * @return 0 on success, -1 on error
 */
int plasticity_orchestrator_deserialize(
    plasticity_orchestrator_t* orchestrator,
    const uint8_t* buffer,
    size_t buffer_size
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PLASTICITY_ORCHESTRATOR_H */
