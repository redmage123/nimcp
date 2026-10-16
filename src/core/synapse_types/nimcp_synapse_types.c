#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_synapse_types.c - Synapse Type System Implementation
//=============================================================================

#include "core/synapse_types/nimcp_synapse_types.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "core/neuralnet/nimcp_neuralnet.h"
#include <math.h>
#include <string.h>
#include "utils/math/nimcp_math_helpers.h"

// === BIO-ASYNC + LOGGING + UNIFIED MEMORY INTEGRATION ===
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "synapse_types"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(synapse_types)

#define BIO_MODULE_ID 0x0134


//=============================================================================
// Constants
//=============================================================================

// Voltage-dependent Mg2+ block parameters (Jahr & Stevens 1990)
#define NMDA_MG_SLOPE 0.062f      // Voltage dependence slope (1/mV)
#define NMDA_MG_IC50 3.57f        // Half-blocking concentration (mM)
#define NMDA_CA_THRESHOLD -20.0f  // Voltage threshold for Ca2+ influx (mV)

//=============================================================================
// AMPA Receptor Functions
//=============================================================================

/**
 * @brief Initialize AMPA receptor state
 *
 * WHAT: Set biologically realistic AMPA parameters
 * WHY: AMPA mediates fast excitatory transmission
 * HOW: Parameters from Destexhe et al. (1994)
 *
 * BIOLOGICAL JUSTIFICATION:
 * - g_max = 1.0 nS: Typical AMPA conductance per synapse
 * - τ_rise = 0.5 ms: Fast activation (glutamate binding)
 * - τ_decay = 2.0 ms: Fast deactivation (glutamate unbinding)
 * - E_rev = 0 mV: Non-selective cation channel (Na+/K+)
 */
void synapse_init_ampa(ampa_state_t* state) {
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synapse_init_ampa: state is NULL");
        return;
    }

    state->conductance = 0.0F;
    state->g_max = 1.0F;           // 1.0 nS
    state->tau_rise = 0.5F;        // 0.5 ms
    state->tau_decay = 2.0F;       // 2.0 ms
    state->reversal_potential = 0.0F; // 0 mV
}

/**
 * @brief Compute AMPA receptor current
 *
 * WHAT: Calculate AMPA-mediated synaptic current
 * WHY: Fast excitatory postsynaptic potential (EPSP)
 * HOW: Exponential decay conductance model
 *
 * ALGORITHM:
 * 1. Decay conductance: g *= exp(-dt/τ_decay)
 * 2. Add spike contribution: g += pre_spike * g_max
 * 3. Compute current: I = g * (V_post - E_rev) * weight
 *
 * BIOLOGICAL ACCURACY:
 * - Single exponential sufficient for AMPA (fast kinetics)
 * - Current proportional to driving force (V - E_rev)
 * - Weight scales synaptic strength (receptor count)
 *
 * COMPLEXITY: O(1), ~15 cycles
 * REFERENCES: Destexhe et al. (1994) J Neurophysiol 72:803-818
 */
float synapse_compute_ampa(
    synapse_t* syn,
    synapse_cold_t* cold,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_spike,
    float dt
) {
    (void)pre_neuron;  // Unused - AMPA current depends only on post_neuron voltage
    if (!syn || !cold || !post_neuron) return 0.0F;

    ampa_state_t* state = &cold->type_state.ampa;

    // 1. Exponential decay of conductance
    state->conductance *= nimcp_safe_expf(-dt / state->tau_decay);
    NIMCP_EMA_GUARD_ZERO(state->conductance);

    // 2. Add spike-triggered conductance increment
    if (pre_spike > 0.5F) {
        state->conductance += state->g_max;
    }

    // 3. Compute synaptic current: I = g * (V - E_rev) * weight
    float driving_force = post_neuron->state - state->reversal_potential;
    float current = state->conductance * driving_force * syn->weight;

    return current;
}

//=============================================================================
// NMDA Receptor Functions
//=============================================================================

/**
 * @brief Initialize NMDA receptor state
 *
 * BIOLOGICAL JUSTIFICATION:
 * - g_max = 0.3 nS: Lower than AMPA (slower kinetics)
 * - τ_rise = 10 ms: Slow activation (Mg2+ unbinding)
 * - τ_decay = 100 ms: Very slow deactivation
 * - E_rev = 0 mV: Cation channel (Na+/K+/Ca2+)
 * - [Mg2+] = 1.0 mM: Physiological extracellular concentration
 */
void synapse_init_nmda(nmda_state_t* state) {
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synapse_init_nmda: state is NULL");
        return;
    }

    state->conductance = 0.0F;
    state->g_max = 0.3F;           // 0.3 nS
    state->tau_rise = 10.0F;       // 10 ms
    state->tau_decay = 100.0F;     // 100 ms
    state->reversal_potential = 0.0F;  // 0 mV
    state->mg_concentration = 1.0F;    // 1.0 mM
    state->calcium_influx = 0.0F;
}

/**
 * @brief Compute NMDA receptor current with voltage-dependent Mg2+ block
 *
 * WHAT: Calculate NMDA-mediated current with Mg2+ gating
 * WHY: NMDA provides slow excitation + Ca2+ signal for LTP/LTD
 * HOW: Jahr-Stevens model for voltage-dependent Mg2+ block
 *
 * ALGORITHM:
 * 1. Decay conductance: g *= exp(-dt/τ_decay)
 * 2. Add spike contribution: g += pre_spike * g_max
 * 3. Compute Mg2+ block: B(V) = 1 / (1 + [Mg2+] * exp(-0.062*V) / 3.57)
 * 4. Current: I = g * (V - E_rev) * B(V) * weight
 * 5. Update Ca2+ influx (for plasticity): Ca2+ += I * H(V - V_thresh)
 *
 * BIOLOGICAL ACCURACY:
 * - Mg2+ block removed at depolarization (B(V) increases with V)
 * - Ca2+ influx only at strong depolarization (V > -20 mV)
 * - Ca2+ influx triggers LTP/LTD (Hebbian learning)
 *
 * COMPLEXITY: O(1), ~35 cycles (exp() dominates)
 * REFERENCES: Jahr & Stevens (1990) J Neurosci 10:3178-3182
 */
float synapse_compute_nmda(
    synapse_t* syn,
    synapse_cold_t* cold,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_spike,
    float dt
) {
    (void)pre_neuron;  // Unused - NMDA current depends only on post_neuron voltage
    if (!syn || !cold || !post_neuron) return 0.0F;

    nmda_state_t* state = &cold->type_state.nmda;

    // 1. Exponential decay of conductance
    state->conductance *= nimcp_safe_expf(-dt / state->tau_decay);
    NIMCP_EMA_GUARD_ZERO(state->conductance);

    // 2. Add spike-triggered conductance increment
    if (pre_spike > 0.5F) {
        state->conductance += state->g_max;
    }

    // 3. Voltage-dependent Mg2+ block (Jahr-Stevens model)
    // B(V) = 1 / (1 + [Mg2+]_o * exp(-0.062*V) / 3.57)
    float voltage = post_neuron->state;
    float mg_factor = state->mg_concentration * nimcp_safe_expf(-NMDA_MG_SLOPE * voltage) / NMDA_MG_IC50;
    if (!isfinite(mg_factor)) mg_factor = 1.0F;
    float mg_block = 1.0F / (1.0F + mg_factor);

    // 4. Compute synaptic current with Mg2+ gating
    float driving_force = voltage - state->reversal_potential;
    float current = state->conductance * driving_force * mg_block * syn->weight;

    // 5. Update calcium influx (for LTP/LTD)
    // Ca2+ influx only occurs at strong depolarization
    if (voltage > NMDA_CA_THRESHOLD && current > 0.0F) {
        state->calcium_influx += current * dt;
    }

    // Decay calcium over time (τ_ca ≈ 100 ms)
    state->calcium_influx *= nimcp_safe_expf(-dt / 100.0F);
    NIMCP_EMA_GUARD_ZERO(state->calcium_influx);

    return current;
}

//=============================================================================
// GABA-A Receptor Functions
//=============================================================================

/**
 * @brief Initialize GABA-A receptor state
 *
 * BIOLOGICAL JUSTIFICATION:
 * - g_max = 2.0 nS: Higher than AMPA (strong inhibition)
 * - τ_rise = 1.0 ms: Fast activation
 * - τ_decay = 10.0 ms: Medium deactivation (faster than NMDA)
 * - E_rev = -70 mV: Chloride reversal potential
 */
void synapse_init_gaba_a(gaba_a_state_t* state) {
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synapse_init_gaba_a: state is NULL");
        return;
    }

    state->conductance = 0.0F;
    state->g_max = 2.0F;           // 2.0 nS
    state->tau_rise = 1.0F;        // 1.0 ms
    state->tau_decay = 10.0F;      // 10 ms
    state->reversal_potential = -70.0F; // -70 mV (Cl-)
}

/**
 * @brief Compute GABA-A receptor current
 *
 * WHAT: Fast inhibitory postsynaptic current (IPSC)
 * WHY: Phasic inhibition for spike timing and oscillations
 * HOW: Same dynamics as AMPA but with negative reversal potential
 *
 * COMPLEXITY: O(1), ~15 cycles
 * REFERENCES: Destexhe et al. (1994) J Neurophysiol 72:803-818
 */
float synapse_compute_gaba_a(
    synapse_t* syn,
    synapse_cold_t* cold,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_spike,
    float dt
) {
    (void)pre_neuron;  // Unused - GABA-A current depends only on post_neuron voltage
    if (!syn || !cold || !post_neuron) return 0.0F;

    gaba_a_state_t* state = &cold->type_state.gaba_a;

    // 1. Exponential decay
    state->conductance *= nimcp_safe_expf(-dt / state->tau_decay);
    NIMCP_EMA_GUARD_ZERO(state->conductance);

    // 2. Spike-triggered increment
    if (pre_spike > 0.5F) {
        state->conductance += state->g_max;
    }

    // 3. Compute current (inhibitory due to E_rev < V_rest)
    float driving_force = post_neuron->state - state->reversal_potential;
    float current = state->conductance * driving_force * syn->weight;

    return current;
}

//=============================================================================
// GABA-B Receptor Functions
//=============================================================================

/**
 * @brief Initialize GABA-B receptor state
 *
 * BIOLOGICAL JUSTIFICATION:
 * - g_max = 0.5 nS: Lower than GABA-A (metabotropic)
 * - τ_rise = 50 ms: Very slow activation (G-protein cascade)
 * - τ_decay = 150 ms: Very slow deactivation
 * - E_rev = -95 mV: Potassium reversal potential
 */
void synapse_init_gaba_b(gaba_b_state_t* state) {
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synapse_init_gaba_b: state is NULL");
        return;
    }

    state->conductance = 0.0F;
    state->g_max = 0.5F;           // 0.5 nS
    state->tau_rise = 50.0F;       // 50 ms
    state->tau_decay = 150.0F;     // 150 ms
    state->reversal_potential = -95.0F; // -95 mV (K+)
}

/**
 * @brief Compute GABA-B receptor current
 *
 * WHAT: Slow inhibitory modulation
 * WHY: Tonic inhibition for long-term excitability control
 * HOW: Slow exponential dynamics (metabotropic pathway)
 *
 * COMPLEXITY: O(1), ~15 cycles
 * REFERENCES: Destexhe & Sejnowski (1995) J Neurophysiol 73:2608-2623
 */
float synapse_compute_gaba_b(
    synapse_t* syn,
    synapse_cold_t* cold,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_spike,
    float dt
) {
    (void)pre_neuron;  // Unused - GABA-B current depends only on post_neuron voltage
    if (!syn || !cold || !post_neuron) return 0.0F;

    gaba_b_state_t* state = &cold->type_state.gaba_b;

    // 1. Exponential decay (much slower than GABA-A)
    state->conductance *= nimcp_safe_expf(-dt / state->tau_decay);
    NIMCP_EMA_GUARD_ZERO(state->conductance);

    // 2. Spike-triggered increment
    if (pre_spike > 0.5F) {
        state->conductance += state->g_max;
    }

    // 3. Compute current
    float driving_force = post_neuron->state - state->reversal_potential;
    float current = state->conductance * driving_force * syn->weight;

    return current;
}

//=============================================================================
// Dopamine Receptor Functions
//=============================================================================

/**
 * @brief Initialize dopamine receptor state
 *
 * BIOLOGICAL JUSTIFICATION:
 * - τ_d1 = 200 ms: D1 receptor (Gs-coupled, slow cAMP increase)
 * - τ_d2 = 100 ms: D2 receptor (Gi-coupled, faster cAMP decrease)
 * - baseline = 0.5: Tonic dopamine level in healthy brain
 */
void synapse_init_dopamine(dopamine_state_t* state) {
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synapse_init_dopamine: state is NULL");
        return;
    }

    state->d1_level = 0.0F;
    state->d2_level = 0.0F;
    state->tau_d1 = 200.0F;        // 200 ms
    state->tau_d2 = 100.0F;        // 100 ms
    state->modulation = 0.0F;
    state->baseline = 0.5F;        // 50% tonic level
}

/**
 * @brief Compute dopamine receptor modulation
 *
 * WHAT: Modulate synaptic weight by dopamine receptor activation
 * WHY: Reward-based learning (reinforcement learning)
 * HOW: D1-D2 receptor competition modulates effective weight
 *
 * ALGORITHM:
 * 1. Decay D1/D2 levels: d1 *= exp(-dt/τ_d1), d2 *= exp(-dt/τ_d2)
 * 2. On reward: d1 += reward_signal, d2 += (1 - reward_signal)
 * 3. Compute modulation: mod = d1 - d2 (range [-1, +1])
 * 4. Effective weight: w_eff = w * (1 + mod)
 * 5. Compute current: I = w_eff * pre_spike
 *
 * BIOLOGICAL ACCURACY:
 * - D1 activation enhances transmission (LTP-like)
 * - D2 activation suppresses transmission (LTD-like)
 * - Competition between D1/D2 determines net effect
 *
 * COMPLEXITY: O(1), ~25 cycles
 * REFERENCES: Seamans & Yang (2004) Neuron 44:317-333
 */
float synapse_compute_dopamine(
    synapse_t* syn,
    synapse_cold_t* cold,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_spike,
    float dt
) {
    (void)pre_neuron;   // Unused - modulation computed from internal D1/D2 state
    (void)post_neuron;  // Unused - dopamine modulates weight, not voltage-dependent
    if (!syn || !cold) return 0.0F;

    dopamine_state_t* state = &cold->type_state.dopamine;

    // 1. Exponential decay of D1/D2 receptor activation
    state->d1_level *= nimcp_safe_expf(-dt / state->tau_d1);
    NIMCP_EMA_GUARD_ZERO(state->d1_level);
    state->d2_level *= nimcp_safe_expf(-dt / state->tau_d2);
    NIMCP_EMA_GUARD_ZERO(state->d2_level);

    // 2. Compute net modulation (D1 - D2 competition)
    state->modulation = state->d1_level - state->d2_level;

    // 3. Modulate weight (baseline + modulation)
    float effective_weight = syn->weight * (state->baseline + state->modulation);

    // 4. Compute modulated transmission
    float current = effective_weight * pre_spike;

    return current;
}

//=============================================================================
// Serotonin Receptor Functions
//=============================================================================

/**
 * @brief Initialize serotonin receptor state
 *
 * BIOLOGICAL JUSTIFICATION:
 * - τ_ht1a = 500 ms: 5-HT1A receptor (Gi-coupled, inhibitory)
 * - τ_ht2a = 300 ms: 5-HT2A receptor (Gq-coupled, excitatory)
 * - baseline = 0.5: Tonic serotonin level
 */
void synapse_init_serotonin(serotonin_state_t* state) {
    if (!state) return;

    state->ht1a_level = 0.0F;
    state->ht2a_level = 0.0F;
    state->tau_ht1a = 500.0F;      // 500 ms
    state->tau_ht2a = 300.0F;      // 300 ms
    state->modulation = 0.0F;
    state->baseline = 0.5F;
}

/**
 * @brief Compute serotonin receptor modulation
 *
 * WHAT: Modulate synaptic transmission by serotonin level
 * WHY: Mood/stability regulation (anxiety, depression)
 * HOW: 5-HT1A/5-HT2A receptor balance
 *
 * ALGORITHM:
 * 1. Decay 5-HT1A/2A levels
 * 2. Modulation = 5-HT2A - 5-HT1A (excitatory - inhibitory)
 * 3. Effective weight = w * (baseline + modulation)
 *
 * COMPLEXITY: O(1), ~25 cycles
 * REFERENCES: Barnes & Sharp (1999) Neuropharmacology 38:1083-1152
 */
float synapse_compute_serotonin(
    synapse_t* syn,
    synapse_cold_t* cold,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_spike,
    float dt
) {
    (void)pre_neuron;   // Unused - modulation computed from internal 5-HT state
    (void)post_neuron;  // Unused - serotonin modulates weight, not voltage-dependent
    if (!syn || !cold) return 0.0F;

    serotonin_state_t* state = &cold->type_state.serotonin;

    // 1. Exponential decay
    state->ht1a_level *= nimcp_safe_expf(-dt / state->tau_ht1a);
    NIMCP_EMA_GUARD_ZERO(state->ht1a_level);
    state->ht2a_level *= nimcp_safe_expf(-dt / state->tau_ht2a);
    NIMCP_EMA_GUARD_ZERO(state->ht2a_level);

    // 2. Compute net modulation (5-HT2A excitatory, 5-HT1A inhibitory)
    state->modulation = state->ht2a_level - state->ht1a_level;

    // 3. Modulate weight
    float effective_weight = syn->weight * (state->baseline + state->modulation);

    // 4. Compute modulated transmission
    float current = effective_weight * pre_spike;

    return current;
}

//=============================================================================
// Acetylcholine Receptor Functions
//=============================================================================

/**
 * @brief Initialize acetylcholine receptor state
 *
 * BIOLOGICAL JUSTIFICATION:
 * - τ_nicotinic = 20 ms: Fast ionotropic (nAChR)
 * - τ_muscarinic = 200 ms: Slow metabotropic (mAChR)
 * - baseline = 0.3: Low tonic ACh (high during attention)
 */
void synapse_init_acetylcholine(acetylcholine_state_t* state) {
    if (!state) return;

    state->nicotinic_level = 0.0F;
    state->muscarinic_level = 0.0F;
    state->tau_nicotinic = 20.0F;  // 20 ms
    state->tau_muscarinic = 200.0F; // 200 ms
    state->modulation = 0.0F;
    state->baseline = 0.3F;        // Low baseline (attention-dependent)
}

/**
 * @brief Compute acetylcholine receptor modulation
 *
 * WHAT: Attention-dependent synaptic gating
 * WHY: Enhance signal-to-noise ratio during attention
 * HOW: Nicotinic + muscarinic receptor activation
 *
 * ALGORITHM:
 * 1. Decay nicotinic/muscarinic levels
 * 2. Modulation = nicotinic + muscarinic (both enhance)
 * 3. Effective weight = w * (baseline + modulation)
 *
 * NOTE: Unlike dopamine/serotonin, both components are additive
 *
 * COMPLEXITY: O(1), ~25 cycles
 * REFERENCES: Hasselmo (1999) Trends Cogn Sci 3:351-359
 */
float synapse_compute_acetylcholine(
    synapse_t* syn,
    synapse_cold_t* cold,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_spike,
    float dt
) {
    (void)pre_neuron;   // Unused - modulation computed from internal ACh state
    (void)post_neuron;  // Unused - acetylcholine modulates weight, not voltage-dependent
    if (!syn || !cold) return 0.0F;

    acetylcholine_state_t* state = &cold->type_state.acetylcholine;

    // 1. Exponential decay
    state->nicotinic_level *= nimcp_safe_expf(-dt / state->tau_nicotinic);
    NIMCP_EMA_GUARD_ZERO(state->nicotinic_level);
    state->muscarinic_level *= nimcp_safe_expf(-dt / state->tau_muscarinic);
    NIMCP_EMA_GUARD_ZERO(state->muscarinic_level);

    // 2. Compute net modulation (both enhance transmission)
    state->modulation = state->nicotinic_level + state->muscarinic_level;

    // 3. Modulate weight
    float effective_weight = syn->weight * (state->baseline + state->modulation);

    // 4. Compute modulated transmission
    float current = effective_weight * pre_spike;

    return current;
}

//=============================================================================
// Electrical Synapse Functions
//=============================================================================

/**
 * @brief Initialize electrical synapse state
 *
 * BIOLOGICAL JUSTIFICATION:
 * - conductance = 0.5 nS: Typical gap junction conductance
 * - bidirectional = true: Current flows both ways
 */
void synapse_init_electrical(electrical_state_t* state) {
    if (!state) return;

    state->conductance = 0.5F;     // 0.5 nS
    state->bidirectional = true;
}

/**
 * @brief Compute electrical synapse current (gap junction)
 *
 * WHAT: Direct electrical coupling between neurons
 * WHY: Synchronization of neural populations (gamma oscillations)
 * HOW: Ohmic current proportional to voltage difference
 *
 * ALGORITHM:
 * I = g * (V_pre - V_post)
 *
 * BIOLOGICAL ACCURACY:
 * - Instantaneous (no delay, no dynamics)
 * - Bidirectional (current flows both ways)
 * - Linear (Ohmic, no rectification)
 *
 * NOTE: For bidirectional coupling, this function computes pre→post current.
 * The network must also compute post→pre current separately.
 *
 * COMPLEXITY: O(1), ~5 cycles
 * REFERENCES: Connors & Long (2004) Annu Rev Neurosci 27:393-418
 */
float synapse_compute_electrical(
    synapse_t* syn,
    synapse_cold_t* cold,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_spike,
    float dt
) {
    (void)pre_spike;  // Unused - electrical synapses transmit voltage, not spikes
    (void)dt;         // Unused - instantaneous coupling (no dynamics)
    if (!syn || !cold || !pre_neuron || !post_neuron) return 0.0F;

    electrical_state_t* state = &cold->type_state.electrical;

    // Ohmic current: I = g * (V_pre - V_post)
    float voltage_diff = pre_neuron->state - post_neuron->state;
    float current = state->conductance * voltage_diff * syn->weight;

    return current;
}

//=============================================================================
// Generic Synapse Functions
//=============================================================================

/**
 * @brief Compute generic synapse (baseline)
 *
 * WHAT: Simple weighted sum (no dynamics)
 * WHY: Baseline for comparison, minimal overhead
 * HOW: output = weight * pre_spike
 *
 * COMPLEXITY: O(1), ~3 cycles
 */
float synapse_compute_generic(
    synapse_t* syn,
    synapse_cold_t* cold,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_spike,
    float dt
) {
    (void)cold;         // Unused - generic synapse has no type_state
    (void)pre_neuron;   // Unused - generic synapse is a simple weighted sum
    (void)post_neuron;  // Unused - no voltage dependence
    (void)dt;           // Unused - no dynamics
    if (!syn) return 0.0F;

    return syn->weight * pre_spike;
}

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get synapse type name as string
 *
 * WHAT: Convert synapse type enum to human-readable string
 * WHY: Debugging, logging, visualization
 * HOW: Static lookup table
 *
 * COMPLEXITY: O(1)
 */
const char* synapse_type_name(synapse_type_t type) {
    static const char* names[] = {
        "GENERIC",
        "AMPA",
        "NMDA",
        "GABA-A",
        "GABA-B",
        "DOPAMINE",
        "SEROTONIN",
        "ACETYLCHOLINE",
        "ELECTRICAL"
    };

    if (type < 0 || type >= SYNAPSE_TYPE_COUNT) {
        return "UNKNOWN";
    }

    return names[type];
}

/**
 * @brief Get characteristic time constant for synapse type
 *
 * WHAT: Return τ_decay or equivalent time constant
 * WHY: Useful for adaptive timestep selection and debugging
 * HOW: Extract from type-specific state
 *
 * COMPLEXITY: O(1)
 */
float synapse_type_time_constant(synapse_type_t type, const synapse_type_state_t* state) {
    if (!state) return 0.0F;

    switch (type) {
        case SYNAPSE_AMPA:
            return state->ampa.tau_decay;
        case SYNAPSE_NMDA:
            return state->nmda.tau_decay;
        case SYNAPSE_GABA_A:
            return state->gaba_a.tau_decay;
        case SYNAPSE_GABA_B:
            return state->gaba_b.tau_decay;
        case SYNAPSE_DOPAMINE:
            return state->dopamine.tau_d1;
        case SYNAPSE_SEROTONIN:
            return state->serotonin.tau_ht1a;
        case SYNAPSE_ACETYLCHOLINE:
            return state->acetylcholine.tau_muscarinic;
        case SYNAPSE_ELECTRICAL:
            return 0.0F;  // Instantaneous
        case SYNAPSE_GENERIC:
        default:
            return 0.0F;
    }
}

/**
 * @brief Check if synapse type is excitatory
 *
 * COMPLEXITY: O(1)
 */
bool synapse_type_is_excitatory(synapse_type_t type) {
    return (type == SYNAPSE_AMPA || type == SYNAPSE_NMDA);
}

/**
 * @brief Check if synapse type is inhibitory
 *
 * COMPLEXITY: O(1)
 */
bool synapse_type_is_inhibitory(synapse_type_t type) {
    return (type == SYNAPSE_GABA_A || type == SYNAPSE_GABA_B);
}

/**
 * @brief Check if synapse type is modulatory
 *
 * COMPLEXITY: O(1)
 */
bool synapse_type_is_modulatory(synapse_type_t type) {
    return (type == SYNAPSE_DOPAMINE ||
            type == SYNAPSE_SEROTONIN ||
            type == SYNAPSE_ACETYLCHOLINE);
}

//=============================================================================
// Ternary Modulation Functions (NIMCP 2.10)
//=============================================================================

/**
 * @brief Convert continuous modulation to ternary
 */
trit_t modulation_to_ternary(float modulation, float threshold) {
    if (modulation >= threshold) {
        return TRIT_POSITIVE;
    } else if (modulation <= -threshold) {
        return TRIT_NEGATIVE;
    }
    return TRIT_UNKNOWN;
}

/**
 * @brief Convert ternary modulation to continuous
 */
float ternary_to_modulation(trit_t ternary_modulation) {
    return (float)ternary_modulation;
}

/**
 * @brief Enable ternary modulation for dopamine state
 */
void dopamine_enable_ternary_modulation(dopamine_state_t* state, float threshold) {
    if (!state) return;

    state->ternary_threshold = threshold;
    state->use_ternary_modulation = true;
    dopamine_update_ternary_modulation(state);
}

/**
 * @brief Disable ternary modulation for dopamine state
 */
void dopamine_disable_ternary_modulation(dopamine_state_t* state) {
    if (!state) return;

    state->use_ternary_modulation = false;
}

/**
 * @brief Update ternary modulation from continuous value
 */
void dopamine_update_ternary_modulation(dopamine_state_t* state) {
    if (!state) return;

    state->ternary_modulation = modulation_to_ternary(
        state->modulation,
        state->ternary_threshold
    );
}

/**
 * @brief Get effective modulation (ternary or continuous)
 */
float dopamine_get_effective_modulation(const dopamine_state_t* state) {
    if (!state) return 0.0f;

    if (state->use_ternary_modulation) {
        return ternary_to_modulation(state->ternary_modulation);
    }

    return state->modulation;
}

/**
 * @brief Enable ternary modulation for serotonin state
 */
void serotonin_enable_ternary_modulation(serotonin_state_t* state, float threshold) {
    if (!state) return;

    state->ternary_threshold = threshold;
    state->use_ternary_modulation = true;
    serotonin_update_ternary_modulation(state);
}

/**
 * @brief Disable ternary modulation for serotonin state
 */
void serotonin_disable_ternary_modulation(serotonin_state_t* state) {
    if (!state) return;

    state->use_ternary_modulation = false;
}

/**
 * @brief Update ternary modulation from continuous value
 */
void serotonin_update_ternary_modulation(serotonin_state_t* state) {
    if (!state) return;

    state->ternary_modulation = modulation_to_ternary(
        state->modulation,
        state->ternary_threshold
    );
}

/**
 * @brief Get effective modulation (ternary or continuous)
 */
float serotonin_get_effective_modulation(const serotonin_state_t* state) {
    if (!state) return 0.0f;

    if (state->use_ternary_modulation) {
        return ternary_to_modulation(state->ternary_modulation);
    }

    return state->modulation;
}
