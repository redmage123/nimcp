/**
 * @file nimcp_plasticity_orchestrator.c
 * @brief Unified Plasticity Orchestrator - Full Implementation
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
 */

#include "plasticity/nimcp_plasticity_orchestrator.h"
#include "plasticity/stdp/nimcp_triplet_stdp.h"
#include "plasticity/bcm/nimcp_bcm.h"
#include "plasticity/homeostatic/nimcp_homeostatic.h"
#include "plasticity/calcium/nimcp_calcium_dynamics.h"
#include "plasticity/structural/nimcp_structural_plasticity.h"
#include "plasticity/metabolic/nimcp_metabolic_plasticity.h"
#include "plasticity/heterosynaptic/nimcp_heterosynaptic.h"
#include "plasticity/astrocyte/nimcp_astrocyte_plasticity.h"
#include "plasticity/protein/nimcp_protein_synthesis.h"
#include "plasticity/metaplasticity/nimcp_extended_metaplasticity.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/validation/nimcp_common.h"
#include "cognitive/nimcp_sleep_wake.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "plasticity_orchestrator"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(plasticity_orchestrator)

#define MAX_SYNAPSES 100
#define MAX_NEURONS 50
#define MAX_CALLBACKS 32
#define MAX_ASTROCYTES 100

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

typedef struct {
    plasticity_event_callback_t callback;
    void* user_data;
    plasticity_event_type_t event_type;
    int id;
    bool active;
} event_callback_entry_t;

typedef struct {
    plasticity_update_callback_t callback;
    void* user_data;
    bool active;
} update_callback_entry_t;

/**
 * @brief Synapse entry in orchestrator
 *
 * Contains all per-synapse plasticity module states
 */
typedef struct {
    uint32_t id;
    uint32_t pre_neuron_id;
    uint32_t post_neuron_id;
    float weight;
    float w_min;
    float w_max;

    /* Module-specific states */
    triplet_stdp_synapse_t* triplet_stdp;
    bcm_synapse_t bcm_state;
    uint32_t structural_synapse_id;  /* ID in structural plasticity system */

    /* Activity tracking */
    uint64_t last_pre_spike_time;
    uint64_t last_post_spike_time;
    uint64_t total_pre_spikes;
    uint64_t total_post_spikes;
    float recent_activity_hz;
    float eligibility_trace;

    /* Consolidation state */
    bool consolidation_tagged;
    float ltp_accumulator;
    float ltd_accumulator;

    bool active;
} synapse_entry_t;

/**
 * @brief Neuron entry in orchestrator
 */
typedef struct {
    uint32_t id;
    float firing_rate;
    float bcm_threshold;
    float target_rate;

    /* Homeostatic state */
    synaptic_scaling_state_t scaling_state;
    intrinsic_plasticity_state_t ip_state;
    metaplasticity_state_t meta_state;

    /* Associated synapses (indices into synapse array) */
    uint32_t* input_synapse_indices;
    uint32_t num_inputs;
    uint32_t input_capacity;

    bool active;
} neuron_entry_t;

/**
 * @brief Internal orchestrator state
 */
struct plasticity_orchestrator_struct {
    /* Configuration */
    plasticity_orchestrator_config_t config;

    /* Synapse and neuron arrays */
    synapse_entry_t* synapses;
    neuron_entry_t* neurons;
    size_t num_synapses;
    size_t num_neurons;
    size_t synapse_capacity;
    size_t neuron_capacity;

    /* Global plasticity modules */
    metabolic_plasticity_t* metabolic;
    calcium_dynamics_t calcium;
    structural_plasticity_system_t* structural;
    hetero_system_t* heterosynaptic;
    astrocyte_plasticity_t astrocyte;
    protein_synthesis_system_t protein_synthesis;
    homeostatic_controller_t homeostatic;
    metaplasticity_controller_t metaplasticity;

    /* Integration handles */
    struct brain_immune_system* immune;
    struct sleep_system_struct* sleep;
    struct neuromodulator_system_struct* neuromod;

    /* Current modulation state */
    sleep_state_t current_sleep_state;
    float sleep_modulation_factor;
    float immune_modulation_factor;
    neuromodulator_levels_t neuromod_levels;

    /* Callbacks */
    event_callback_entry_t event_callbacks[MAX_CALLBACKS];
    update_callback_entry_t pre_update_callbacks[MAX_CALLBACKS];
    update_callback_entry_t post_update_callbacks[MAX_CALLBACKS];
    int next_callback_id;

    /* Statistics */
    plasticity_stats_t stats;

    /* State */
    uint64_t current_time_ms;
    uint64_t last_consolidation_time;
    uint64_t last_homeostatic_time;
    uint64_t last_structural_time;

    /* Bio-async */
    bool bio_async_connected;

    /* Thread safety */
    nimcp_platform_mutex_t* mutex;
};


// Forward declarations for static functions (SRP split)
static void emit_event( plasticity_orchestrator_t* orchestrator, plasticity_event_type_t type, uint32_t synapse_id, uint32_t neuron_id, float old_val, float new_val );
static synapse_entry_t* find_synapse(plasticity_orchestrator_t* orch, uint32_t id);
static neuron_entry_t* find_neuron(plasticity_orchestrator_t* orch, uint32_t id);
static void structural_change_handler( structural_event_t event, uint32_t synapse_id, synapse_state_t old_state, synapse_state_t new_state, void* user_data );
static synapse_entry_t* get_or_create_synapse(plasticity_orchestrator_t* orch, uint32_t id);
static neuron_entry_t* get_or_create_neuron(plasticity_orchestrator_t* orch, uint32_t id);
static float get_sleep_modulation(sleep_state_t state);
static float get_immune_modulation(brain_inflammation_level_t level);
static void update_weight_statistics(plasticity_orchestrator_t* orch);

//=============================================================================
// SRP Split: Function implementations organized by responsibility
//=============================================================================
#include "nimcp_plasticity_orchestrator_part_helpers.c"  // 6 functions: helpers
#include "nimcp_plasticity_orchestrator_part_processing.c"  // 4 functions: processing
#include "nimcp_plasticity_orchestrator_part_lifecycle.c"  // 5 functions: lifecycle
#include "nimcp_plasticity_orchestrator_part_accessors.c"  // 7 functions: accessors
#include "nimcp_plasticity_orchestrator_part_core.c"  // 9 functions: core
#include "nimcp_plasticity_orchestrator_part_io.c"  // 2 functions: io
