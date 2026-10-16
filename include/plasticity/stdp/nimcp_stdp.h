/**
 * @file nimcp_stdp.h
 * @brief Spike-Timing-Dependent Plasticity with Dopamine Modulation
 *
 * Implementation of STDP with neuromodulator-gated learning, integrating
 * Phase C2.2 phasic-tonic dopamine dynamics for biologically realistic
 * reward learning.
 *
 * Key Features:
 * - Classic STDP (Bi & Poo, 1998): LTP for pre-before-post, LTD for post-before-pre
 * - Dopamine modulation: Learning rate scaled by DA concentration
 * - Burst amplification: 3x learning during phasic dopamine bursts
 * - Three-factor learning: Hebbian + Timing + Reward
 *
 * NIMCP Phase: Option 2.1 (Integration)
 * Date: 2025-11-12
 */

#ifndef NIMCP_STDP_H
#define NIMCP_STDP_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct neuromodulator_system_struct;
typedef struct neuromodulator_system_struct* neuromodulator_system_t;  /* Opaque pointer (matches brain.h) */

struct nimcp_sec_integration;
typedef struct nimcp_sec_integration nimcp_sec_integration_t;

/**
 * STDP synapse state
 */
typedef struct {
    /* Synaptic weight */
    float weight;               /* Current weight [0, w_max] */
    float w_max;                /* Maximum weight (default: 1.0) */
    float w_min;                /* Minimum weight (default: 0.0) */

    /* Learning parameters */
    float learning_rate;        /* Base learning rate (default: 0.01) */
    float a_plus;               /* LTP amplitude (default: 0.005) */
    float a_minus;              /* LTD amplitude (default: 0.00525) */
    float tau_plus;             /* LTP time constant [ms] (default: 20) */
    float tau_minus;            /* LTD time constant [ms] (default: 20) */

    /* Spike timing traces */
    float pre_trace;            /* Presynaptic trace */
    float post_trace;           /* Postsynaptic trace */

    /* Dopamine modulation parameters */
    bool enable_da_modulation;  /* Use dopamine modulation (default: true) */
    float da_modulation_gain;   /* DA concentration → LR scaling (default: 100.0) */
    float burst_amplification;  /* LR multiplier during bursts (default: 3.0) */

    /* Sleep state modulation */
    sleep_state_t current_sleep_state;  /* Current sleep/wake state */

    /* Thread safety */
    nimcp_spinlock_t lock;      /* Spinlock for weight modifications */

    /* Statistics */
    uint64_t num_potentiation_events;
    uint64_t num_depression_events;
    float total_ltp;            /* Cumulative LTP */
    float total_ltd;            /* Cumulative LTD */

    /* Weight saturation tracking (for detecting potential learning issues) */
    uint64_t num_saturate_max_events;   /* Times weight hit w_max limit */
    uint64_t num_saturate_min_events;   /* Times weight hit w_min limit */

    /* Minimum trace threshold for gating plasticity updates (default: 0.1) */
    float min_trace_threshold;

} stdp_synapse_t;

/**
 * STDP configuration
 */
typedef struct {
    float w_max;
    float learning_rate;
    float a_plus;
    float a_minus;
    float tau_plus;
    float tau_minus;
    bool enable_da_modulation;
    float da_modulation_gain;
    float burst_amplification;
    float min_trace_threshold;  /* Min trace magnitude to trigger update (default: 0.1) */
} stdp_config_t;

/**
 * Initialize STDP synapse with default parameters
 *
 * @param synapse Synapse to initialize
 */
void stdp_synapse_init(stdp_synapse_t* synapse);

/**
 * Initialize STDP synapse with custom configuration
 *
 * @param synapse Synapse to initialize
 * @param config Configuration parameters
 */
void stdp_synapse_init_with_config(stdp_synapse_t* synapse, const stdp_config_t* config);

/**
 * Initialize STDP module (call once at startup)
 *
 * Registers with security system and sets up async infrastructure.
 *
 * @param security_ctx Optional security context (NULL creates own)
 * @return true on success, false on failure
 */
bool stdp_module_init(nimcp_sec_integration_t* security_ctx);

/**
 * Shutdown STDP module
 */
void stdp_module_shutdown(void);

/**
 * Get default STDP configuration
 *
 * @return Default configuration (Bi & Poo parameters)
 */
stdp_config_t stdp_config_default(void);

/**
 * Update STDP traces (called every timestep)
 *
 * @param synapse Synapse to update
 * @param dt Time step [seconds]
 */
void stdp_update_traces(stdp_synapse_t* synapse, float dt);

/**
 * Process presynaptic spike (classic STDP, no neuromodulation)
 *
 * @param synapse Synapse to update
 * @param current_time Current time [ms]
 * @return Weight change applied
 */
float stdp_pre_spike(stdp_synapse_t* synapse, float current_time);

/**
 * Process postsynaptic spike (classic STDP, no neuromodulation)
 *
 * @param synapse Synapse to update
 * @param current_time Current time [ms]
 * @return Weight change applied
 */
float stdp_post_spike(stdp_synapse_t* synapse, float current_time);

/**
 * Process presynaptic spike with dopamine modulation (THREE-FACTOR LEARNING)
 *
 * This is the key function for reward-modulated learning. The weight change
 * is modulated by:
 * 1. Dopamine concentration (tonic + phasic)
 * 2. Burst state (3x amplification during bursts)
 *
 * @param synapse Synapse to update
 * @param current_time Current time [ms]
 * @param neuromod Neuromodulator system (for dopamine level)
 * @return Weight change applied
 */
float stdp_pre_spike_modulated(stdp_synapse_t* synapse,
                                float current_time,
                                neuromodulator_system_t neuromod);

/**
 * Process postsynaptic spike with dopamine modulation
 *
 * @param synapse Synapse to update
 * @param current_time Current time [ms]
 * @param neuromod Neuromodulator system (for dopamine level)
 * @return Weight change applied
 */
float stdp_post_spike_modulated(stdp_synapse_t* synapse,
                                 float current_time,
                                 neuromodulator_system_t neuromod);

/**
 * Apply weight change with dopamine modulation
 *
 * Internal function that applies DA-gated learning. Called by
 * stdp_pre_spike_modulated() and stdp_post_spike_modulated().
 *
 * @param synapse Synapse to update
 * @param base_weight_change Unmodulated weight change (from STDP rule)
 * @param neuromod Neuromodulator system
 * @return Actual weight change applied (after modulation)
 */
float stdp_apply_modulated_weight_change(stdp_synapse_t* synapse,
                                         float base_weight_change,
                                         neuromodulator_system_t neuromod);

/**
 * Get current dopamine modulation factor
 *
 * Computes the multiplicative factor applied to learning rate based on:
 * - Dopamine concentration (tonic + phasic)
 * - Burst state (amplification if bursting)
 *
 * @param synapse Synapse configuration
 * @param neuromod Neuromodulator system
 * @return Modulation factor [0, 10+] (typically 1.0 baseline, 5.0+ during bursts)
 */
float stdp_get_da_modulation_factor(const stdp_synapse_t* synapse,
                                     neuromodulator_system_t neuromod);

/**
 * Set sleep state for STDP synapse
 *
 * Updates the sleep state which will be used to modulate learning rate,
 * LTP/LTD ratio, and timing windows on next spike.
 *
 * @param synapse Synapse to update
 * @param state New sleep state
 */
void stdp_set_sleep_state(stdp_synapse_t* synapse, sleep_state_t state);

/**
 * Reset STDP synapse (clear traces and statistics)
 *
 * @param synapse Synapse to reset
 */
void stdp_synapse_reset(stdp_synapse_t* synapse);

/**
 * Print STDP synapse statistics
 *
 * @param synapse Synapse to print
 */
void stdp_synapse_print_stats(const stdp_synapse_t* synapse);

/**
 * Get global STDP module statistics
 *
 * @param total_ltp Output: total LTP events across all synapses
 * @param total_ltd Output: total LTD events across all synapses
 * @param total_da_queries Output: total dopamine queries
 */
void stdp_module_get_stats(uint64_t* total_ltp, uint64_t* total_ltd, uint64_t* total_da_queries);

/* ============================================================================
 * Phase 8: State Manager Integration for Fault Tolerance
 * ============================================================================ */

/* Forward declaration for state ops (avoid circular include) */
struct nimcp_module_state_ops;
typedef struct nimcp_module_state_ops nimcp_module_state_ops_t;

/**
 * Get STDP state operations for state manager registration
 *
 * Returns a pointer to a static nimcp_module_state_ops_t structure that
 * provides serialize/deserialize/validate/reset/get_size operations for
 * STDP synapse state checkpointing and recovery.
 *
 * Usage:
 *   nimcp_state_manager_register(manager, "stdp_synapse",
 *                                 stdp_get_state_ops(), synapse_ptr);
 *
 * @return Pointer to static state ops structure
 */
const nimcp_module_state_ops_t* stdp_get_state_ops(void);

/**
 * Set health agent for STDP operations (Phase 8: Heartbeat)
 *
 * @param agent Health agent for heartbeat monitoring
 */
struct nimcp_health_agent;
void stdp_set_health_agent(struct nimcp_health_agent* agent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_STDP_H */
