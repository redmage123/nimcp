//=============================================================================
// nimcp_synapse_types.h - Biologically Realistic Synapse Type System
//=============================================================================
/**
 * @file nimcp_synapse_types.h
 * @brief Synapse type system with biologically realistic receptor dynamics
 *
 * NIMCP PHASE 8.7: Synapse Type System
 *
 * BIOLOGICAL MOTIVATION:
 * Real synapses are not homogeneous - they differ in:
 * - Neurotransmitter type (glutamate, GABA, dopamine, serotonin, ACh)
 * - Receptor kinetics (AMPA: τ=2ms, NMDA: τ=100ms, GABA-A: τ=10ms, GABA-B: τ=150ms)
 * - Voltage dependence (NMDA Mg2+ block, electrical gap junctions)
 * - Signaling pathways (ionotropic vs metabotropic)
 * - Plasticity mechanisms (NMDA calcium influx for LTP/LTD)
 *
 * This diversity is critical for:
 * - Fast excitation (AMPA) vs sustained excitation (NMDA)
 * - Fast inhibition (GABA-A) vs slow inhibition (GABA-B)
 * - Neuromodulation (dopamine/serotonin for reward, mood, stability)
 * - Attention gating (acetylcholine)
 * - Synchronization (electrical gap junctions)
 *
 * ARCHITECTURE:
 * - Strategy Pattern: Type-specific compute functions
 * - State Pattern: Synapse dynamics depend on type-specific state variables
 * - Template Method: Common interface, specialized implementations
 *
 * PERFORMANCE CHARACTERISTICS:
 * - Type lookup: O(1) via function pointer table
 * - Memory overhead: ~32 bytes per synapse for type-specific state
 * - Computation: 10-50 cycles per synapse depending on type complexity
 * - Cache efficiency: Type-specific state hot in L1/L2 cache
 *
 * DESIGN DECISIONS:
 * - Enum-based type system: Fast dispatch via switch/table
 * - Union-based state: Zero allocation overhead for type-specific state
 * - Inline functions: Most compute functions inlined for performance
 * - Biological time constants: Derived from experimental neuroscience
 *
 * KEY REFERENCES:
 * - Dayan & Abbott (2001): Theoretical Neuroscience
 * - Koch (1999): Biophysics of Computation
 * - Destexhe et al. (1994): AMPA/NMDA/GABA kinetics
 * - Jahr & Stevens (1990): NMDA voltage dependence
 * - Connors & Long (2004): Electrical synapses
 * - Seamans & Yang (2004): Dopamine modulation
 * - Hasselmo (1999): Acetylcholine modulation
 *
 * @author NIMCP Development Team
 * @date 2025-11-08
 * @version 2.8.7
 */

#ifndef NIMCP_SYNAPSE_TYPES_H
#define NIMCP_SYNAPSE_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "utils/ternary/nimcp_ternary.h"  // Ternary modulation support

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

// Forward declare neuron and synapse types from neuralnet.h
// WHY: Avoid circular dependency - neuralnet.h will include this file
typedef struct neuron_struct neuron_t;
typedef struct synapse_t synapse_t;
typedef struct synapse_cold_t synapse_cold_t;
struct synapse_compute_context_t;

//=============================================================================
// Synapse Type Enumeration
//=============================================================================

/**
 * @brief Synapse type enumeration
 *
 * WHAT: Classification of synapses by neurotransmitter/receptor type
 * WHY: Different synapse types have vastly different dynamics
 * HOW: Used to dispatch to type-specific compute functions
 *
 * BIOLOGICAL HIERARCHY:
 * 1. Excitatory (depolarizing, increases firing probability):
 *    - AMPA: Fast excitation, glutamate ionotropic
 *    - NMDA: Slow excitation, glutamate ionotropic + voltage-gated
 *
 * 2. Inhibitory (hyperpolarizing, decreases firing probability):
 *    - GABA-A: Fast inhibition, GABA ionotropic (Cl- channel)
 *    - GABA-B: Slow inhibition, GABA metabotropic (K+ channel)
 *
 * 3. Neuromodulatory (modulate other synapses, not direct transmission):
 *    - DOPAMINE: Reward/learning (D1/D2 receptors)
 *    - SEROTONIN: Mood/stability (5-HT receptors)
 *    - ACETYLCHOLINE: Attention/arousal (nicotinic/muscarinic)
 *
 * 4. Electrical (direct gap junction coupling):
 *    - ELECTRICAL: Bidirectional, instantaneous
 *
 * 5. Generic (default):
 *    - GENERIC: Simple weighted sum, no dynamics
 */
typedef enum synapse_type_t {
    SYNAPSE_GENERIC = 0,      /**< Generic synapse (no dynamics, baseline) */
    SYNAPSE_AMPA,             /**< AMPA receptor (fast excitatory, τ=2ms) */
    SYNAPSE_NMDA,             /**< NMDA receptor (slow excitatory, τ=100ms, voltage-gated) */
    SYNAPSE_GABA_A,           /**< GABA-A receptor (fast inhibitory, τ=10ms) */
    SYNAPSE_GABA_B,           /**< GABA-B receptor (slow inhibitory, τ=150ms) */
    SYNAPSE_DOPAMINE,         /**< Dopamine receptor (reward modulation) */
    SYNAPSE_SEROTONIN,        /**< Serotonin receptor (mood modulation) */
    SYNAPSE_ACETYLCHOLINE,    /**< Acetylcholine receptor (attention modulation) */
    SYNAPSE_ELECTRICAL,       /**< Gap junction (electrical synapse) */
    SYNAPSE_TYPE_COUNT        /**< Total number of synapse types */
} synapse_type_t;

//=============================================================================
// Type-Specific State Structures
//=============================================================================

/**
 * @brief AMPA receptor state
 *
 * BIOLOGY: AMPA receptors are fast glutamate-gated ion channels
 * - Permeable to Na+ and K+, causing depolarization
 * - Rise time: ~0.5 ms, decay time: ~2 ms
 * - Responsible for fast excitatory transmission
 * - Non-voltage dependent (always active when glutamate present)
 *
 * KINETICS: First-order exponential decay
 * dg/dt = -g/τ_decay + δ(t_spike)
 *
 * REFERENCES:
 * - Destexhe et al. (1994): J Neurophysiol 72:803-818
 * - Jonas & Spruston (1994): J Physiol 472:615-663
 */
typedef struct ampa_state_t {
    float conductance;      /**< Current conductance (nS), range [0, g_max] */
    float g_max;            /**< Maximum conductance (nS), typically 0.5-2.0 nS */
    float tau_rise;         /**< Rise time constant (ms), typically 0.5 ms */
    float tau_decay;        /**< Decay time constant (ms), typically 2 ms */
    float reversal_potential; /**< Reversal potential (mV), typically 0 mV */
} ampa_state_t;

/**
 * @brief NMDA receptor state
 *
 * BIOLOGY: NMDA receptors are slow glutamate-gated ion channels
 * - Permeable to Na+, K+, and Ca2+ (critical for LTP/LTD)
 * - Rise time: ~10 ms, decay time: ~100 ms
 * - Voltage-dependent Mg2+ block (removed at depolarization)
 * - Calcium influx triggers long-term plasticity
 *
 * KINETICS: Dual exponential with voltage-dependent Mg2+ block
 * dg/dt = -g/τ_decay + δ(t_spike)
 * I_NMDA = g * (V - E_rev) * B(V)
 * B(V) = 1 / (1 + [Mg2+]_o * exp(-0.062*V) / 3.57)
 *
 * REFERENCES:
 * - Jahr & Stevens (1990): J Neurosci 10:3178-3182
 * - Destexhe et al. (1994): J Neurophysiol 72:803-818
 */
typedef struct nmda_state_t {
    float conductance;      /**< Current conductance (nS) */
    float g_max;            /**< Maximum conductance (nS), typically 0.1-0.5 nS */
    float tau_rise;         /**< Rise time constant (ms), typically 10 ms */
    float tau_decay;        /**< Decay time constant (ms), typically 100 ms */
    float reversal_potential; /**< Reversal potential (mV), typically 0 mV */
    float mg_concentration; /**< Extracellular Mg2+ concentration (mM), typically 1.0 mM */
    float calcium_influx;   /**< Accumulated calcium influx (for plasticity) */
} nmda_state_t;

/**
 * @brief GABA-A receptor state
 *
 * BIOLOGY: GABA-A receptors are fast GABA-gated chloride channels
 * - Permeable to Cl-, causing hyperpolarization
 * - Rise time: ~1 ms, decay time: ~10 ms
 * - Responsible for fast inhibition (phasic inhibition)
 * - Critical for spike timing and network oscillations
 *
 * KINETICS: First-order exponential decay
 * dg/dt = -g/τ_decay + δ(t_spike)
 *
 * REFERENCES:
 * - Destexhe et al. (1994): J Neurophysiol 72:803-818
 * - Galarreta & Hestrin (1997): J Neurosci 17:7503-7514
 */
typedef struct gaba_a_state_t {
    float conductance;      /**< Current conductance (nS) */
    float g_max;            /**< Maximum conductance (nS), typically 1.0-5.0 nS */
    float tau_rise;         /**< Rise time constant (ms), typically 1 ms */
    float tau_decay;        /**< Decay time constant (ms), typically 10 ms */
    float reversal_potential; /**< Reversal potential (mV), typically -70 mV */
} gaba_a_state_t;

/**
 * @brief GABA-B receptor state
 *
 * BIOLOGY: GABA-B receptors are slow GABA-gated potassium channels
 * - Metabotropic (G-protein coupled), not ionotropic
 * - Rise time: ~50 ms, decay time: ~150 ms
 * - Responsible for slow inhibition (tonic inhibition)
 * - Modulates network excitability over longer timescales
 *
 * KINETICS: Dual exponential with G-protein cascade
 * dg/dt = -g/τ_decay + δ(t_spike)
 *
 * REFERENCES:
 * - Destexhe & Sejnowski (1995): J Neurophysiol 73:2608-2623
 * - Connors et al. (1988): J Neurosci 8:4033-4053
 */
typedef struct gaba_b_state_t {
    float conductance;      /**< Current conductance (nS) */
    float g_max;            /**< Maximum conductance (nS), typically 0.2-1.0 nS */
    float tau_rise;         /**< Rise time constant (ms), typically 50 ms */
    float tau_decay;        /**< Decay time constant (ms), typically 150 ms */
    float reversal_potential; /**< Reversal potential (mV), typically -95 mV (K+) */
} gaba_b_state_t;

/**
 * @brief Dopamine receptor state
 *
 * BIOLOGY: Dopamine receptors modulate synaptic transmission
 * - D1 receptors: Increase cAMP, enhance excitability (long τ)
 * - D2 receptors: Decrease cAMP, reduce excitability (short τ)
 * - Critical for reward learning, motivation, movement
 * - Modulates plasticity via PKA/DARPP-32 pathway
 *
 * DYNAMICS: Slow modulation via second messenger cascades
 * D1: τ = 100-500 ms (potentiation)
 * D2: τ = 50-200 ms (depression)
 *
 * REFERENCES:
 * - Seamans & Yang (2004): Neuron 44:317-333
 * - Surmeier et al. (2007): Neuron 56:1022-1039
 */
typedef struct dopamine_state_t {
    float d1_level;         /**< D1 receptor activation level [0,1] */
    float d2_level;         /**< D2 receptor activation level [0,1] */
    float tau_d1;           /**< D1 time constant (ms), typically 200 ms */
    float tau_d2;           /**< D2 time constant (ms), typically 100 ms */
    float modulation;       /**< Net modulation factor [-1,1] (D1-D2 competition) */
    float baseline;         /**< Baseline dopamine level [0,1] */

    // NIMCP 2.10: Ternary modulation mode
    // WHAT: Discrete modulation states for efficient decision-making
    // WHY:  Many dopamine signals are binary/ternary (reward/neutral/punishment)
    // HOW:  Threshold continuous modulation to discrete state
    trit_t ternary_modulation;      /**< Discrete modulation {LTD=-1, STABLE=0, LTP=+1} */
    bool use_ternary_modulation;    /**< Use ternary instead of continuous modulation */
    float ternary_threshold;        /**< Threshold for ternary quantization (default 0.3) */
} dopamine_state_t;

/**
 * @brief Serotonin receptor state
 *
 * BIOLOGY: Serotonin receptors modulate mood and stability
 * - 5-HT1A: Inhibitory, reduces firing (hyperpolarization)
 * - 5-HT2A: Excitatory, increases firing (depolarization)
 * - Critical for mood regulation, anxiety, depression
 * - Modulates learning rates and network stability
 *
 * DYNAMICS: Slow modulation via G-protein cascades
 * τ = 200-1000 ms (very slow compared to ionotropic)
 *
 * REFERENCES:
 * - Barnes & Sharp (1999): Neuropharmacology 38:1083-1152
 * - Kiser et al. (2012): Neurosci Biobehav Rev 36:2146-2173
 */
typedef struct serotonin_state_t {
    float ht1a_level;       /**< 5-HT1A receptor activation [0,1] (inhibitory) */
    float ht2a_level;       /**< 5-HT2A receptor activation [0,1] (excitatory) */
    float tau_ht1a;         /**< 5-HT1A time constant (ms), typically 500 ms */
    float tau_ht2a;         /**< 5-HT2A time constant (ms), typically 300 ms */
    float modulation;       /**< Net modulation factor [-1,1] */
    float baseline;         /**< Baseline serotonin level [0,1] */

    // NIMCP 2.10: Ternary modulation mode
    // WHAT: Discrete mood states for efficient emotional processing
    // WHY:  Mood regulation often involves discrete states (low/normal/elevated)
    // HOW:  Threshold continuous modulation to discrete state
    trit_t ternary_modulation;      /**< Discrete modulation {DEPRESSIVE=-1, NEUTRAL=0, ELEVATED=+1} */
    bool use_ternary_modulation;    /**< Use ternary instead of continuous modulation */
    float ternary_threshold;        /**< Threshold for ternary quantization (default 0.3) */
} serotonin_state_t;

/**
 * @brief Acetylcholine receptor state
 *
 * BIOLOGY: Acetylcholine receptors modulate attention and arousal
 * - Nicotinic: Ionotropic, fast (τ = 10-50 ms)
 * - Muscarinic: Metabotropic, slow (τ = 100-500 ms)
 * - Critical for attention, memory formation, arousal
 * - Enhances signal-to-noise ratio in cortex
 *
 * DYNAMICS: Dual-component (fast nicotinic + slow muscarinic)
 *
 * REFERENCES:
 * - Hasselmo (1999): Trends Cogn Sci 3:351-359
 * - Picciotto et al. (2012): Neuron 76:116-129
 */
typedef struct acetylcholine_state_t {
    float nicotinic_level;  /**< Nicotinic receptor activation [0,1] */
    float muscarinic_level; /**< Muscarinic receptor activation [0,1] */
    float tau_nicotinic;    /**< Nicotinic time constant (ms), typically 20 ms */
    float tau_muscarinic;   /**< Muscarinic time constant (ms), typically 200 ms */
    float modulation;       /**< Net modulation factor [0,2] (always positive) */
    float baseline;         /**< Baseline ACh level [0,1] */
} acetylcholine_state_t;

/**
 * @brief Electrical synapse (gap junction) state
 *
 * BIOLOGY: Gap junctions are direct cytoplasmic connections
 * - Bidirectional current flow proportional to voltage difference
 * - Instantaneous (no delay, no rise/decay)
 * - Critical for network synchronization (gamma oscillations)
 * - Found in inhibitory interneuron networks
 *
 * DYNAMICS: Ohmic (I = g * (V_pre - V_post))
 * No temporal dynamics, purely resistive coupling
 *
 * REFERENCES:
 * - Connors & Long (2004): Annu Rev Neurosci 27:393-418
 * - Galarreta & Hestrin (1999): Science 286:1565-1568
 */
typedef struct electrical_state_t {
    float conductance;      /**< Gap junction conductance (nS), typically 0.1-1.0 nS */
    bool bidirectional;     /**< Allow reverse current flow (typically true) */
} electrical_state_t;

/**
 * @brief Union of all synapse type states
 *
 * WHAT: Space-efficient storage for type-specific state
 * WHY: Only one state active per synapse (union saves memory)
 * HOW: Discriminated union - type field determines which member is valid
 *
 * MEMORY: sizeof(largest state) = sizeof(nmda_state_t) ≈ 32 bytes
 */
typedef union synapse_type_state_t {
    ampa_state_t ampa;
    nmda_state_t nmda;
    gaba_a_state_t gaba_a;
    gaba_b_state_t gaba_b;
    dopamine_state_t dopamine;
    serotonin_state_t serotonin;
    acetylcholine_state_t acetylcholine;
    electrical_state_t electrical;
} synapse_type_state_t;

//=============================================================================
// Type-Specific Compute Functions
//=============================================================================

/**
 * @brief AMPA synapse computation
 *
 * WHAT: Compute AMPA receptor current
 * WHY: Fast excitatory transmission (glutamate)
 * HOW: Exponential decay conductance model
 *
 * ALGORITHM:
 * 1. Update conductance: g(t) = g(t-dt) * exp(-dt/τ_decay)
 * 2. On spike: g += g_max
 * 3. Compute current: I = g * (V_post - E_rev) * weight
 *
 * COMPLEXITY: O(1), ~20 cycles
 *
 * @param syn Synapse structure (contains weight, type_state)
 * @param pre_neuron Presynaptic neuron (for spike detection)
 * @param post_neuron Postsynaptic neuron (for membrane potential)
 * @param pre_spike Presynaptic spike indicator (1.0 if spike, 0.0 otherwise)
 * @param dt Time step (ms)
 * @return Synaptic current contribution
 */
float synapse_compute_ampa(
    synapse_t* syn,
    synapse_cold_t* cold,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_spike,
    float dt
);

/**
 * @brief NMDA synapse computation
 *
 * WHAT: Compute NMDA receptor current with voltage-dependent Mg2+ block
 * WHY: Slow excitatory transmission + LTP/LTD calcium signal
 * HOW: Exponential decay + Jahr-Stevens Mg2+ block function
 *
 * ALGORITHM:
 * 1. Update conductance: g(t) = g(t-dt) * exp(-dt/τ_decay)
 * 2. On spike: g += g_max
 * 3. Compute Mg2+ block: B(V) = 1 / (1 + [Mg2+] * exp(-0.062*V) / 3.57)
 * 4. Compute current: I = g * (V_post - E_rev) * B(V_post) * weight
 * 5. Update calcium: Ca2+ += I * (V_post > -20mV ? 1.0 : 0.0)
 *
 * COMPLEXITY: O(1), ~40 cycles (exp() dominates)
 *
 * @param syn Synapse structure
 * @param pre_neuron Presynaptic neuron
 * @param post_neuron Postsynaptic neuron
 * @param pre_spike Presynaptic spike indicator
 * @param dt Time step (ms)
 * @return Synaptic current contribution
 */
float synapse_compute_nmda(
    synapse_t* syn,
    synapse_cold_t* cold,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_spike,
    float dt
);

/**
 * @brief GABA-A synapse computation
 *
 * WHAT: Compute GABA-A receptor current
 * WHY: Fast inhibitory transmission
 * HOW: Exponential decay conductance model (same as AMPA but inhibitory)
 *
 * COMPLEXITY: O(1), ~20 cycles
 */
float synapse_compute_gaba_a(
    synapse_t* syn,
    synapse_cold_t* cold,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_spike,
    float dt
);

/**
 * @brief GABA-B synapse computation
 *
 * WHAT: Compute GABA-B receptor current
 * WHY: Slow inhibitory modulation
 * HOW: Dual exponential model (slower than GABA-A)
 *
 * COMPLEXITY: O(1), ~20 cycles
 */
float synapse_compute_gaba_b(
    synapse_t* syn,
    synapse_cold_t* cold,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_spike,
    float dt
);

/**
 * @brief Dopamine synapse computation
 *
 * WHAT: Compute dopamine receptor modulation
 * WHY: Reward-based plasticity modulation
 * HOW: D1/D2 receptor balance modulates weight
 *
 * ALGORITHM:
 * 1. Update D1: d1(t) = d1(t-dt) * exp(-dt/τ_d1)
 * 2. Update D2: d2(t) = d2(t-dt) * exp(-dt/τ_d2)
 * 3. On reward: d1 += reward, d2 += (1-reward)
 * 4. Modulation = d1 - d2 (range [-1, +1])
 * 5. Effective weight = weight * (1 + modulation)
 *
 * COMPLEXITY: O(1), ~30 cycles
 */
float synapse_compute_dopamine(
    synapse_t* syn,
    synapse_cold_t* cold,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_spike,
    float dt
);

/**
 * @brief Serotonin synapse computation
 *
 * WHAT: Compute serotonin receptor modulation
 * WHY: Mood/stability modulation of transmission
 * HOW: 5-HT1A/5-HT2A receptor balance
 *
 * COMPLEXITY: O(1), ~30 cycles
 */
float synapse_compute_serotonin(
    synapse_t* syn,
    synapse_cold_t* cold,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_spike,
    float dt
);

/**
 * @brief Acetylcholine synapse computation
 *
 * WHAT: Compute acetylcholine receptor modulation
 * WHY: Attention-dependent transmission gating
 * HOW: Nicotinic/muscarinic receptor activation
 *
 * COMPLEXITY: O(1), ~30 cycles
 */
float synapse_compute_acetylcholine(
    synapse_t* syn,
    synapse_cold_t* cold,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_spike,
    float dt
);

/**
 * @brief Electrical synapse computation
 *
 * WHAT: Compute gap junction current
 * WHY: Direct electrical coupling between neurons
 * HOW: Ohmic current proportional to voltage difference
 *
 * ALGORITHM:
 * I = g * (V_pre - V_post)
 *
 * NOTE: Bidirectional - current flows both ways
 * COMPLEXITY: O(1), ~5 cycles (no dynamics)
 */
float synapse_compute_electrical(
    synapse_t* syn,
    synapse_cold_t* cold,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_spike,
    float dt
);

/**
 * @brief Generic synapse computation (baseline)
 *
 * WHAT: Simple weighted sum (no dynamics)
 * WHY: Baseline for comparison, minimal overhead
 * HOW: output = weight * pre_spike
 *
 * COMPLEXITY: O(1), ~5 cycles
 */
float synapse_compute_generic(
    synapse_t* syn,
    synapse_cold_t* cold,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_spike,
    float dt
);

//=============================================================================
// Type-Specific Initialization Functions
//=============================================================================

/**
 * @brief Initialize AMPA synapse state
 *
 * WHAT: Set default AMPA receptor parameters
 * WHY: Consistent initialization with biological defaults
 * HOW: Set g_max, τ_rise, τ_decay, E_rev from literature
 *
 * DEFAULT PARAMETERS:
 * - g_max = 1.0 nS
 * - τ_rise = 0.5 ms
 * - τ_decay = 2.0 ms
 * - E_rev = 0.0 mV
 *
 * @param state Pointer to AMPA state structure
 */
void synapse_init_ampa(ampa_state_t* state);

/**
 * @brief Initialize NMDA synapse state
 *
 * DEFAULT PARAMETERS:
 * - g_max = 0.3 nS
 * - τ_rise = 10.0 ms
 * - τ_decay = 100.0 ms
 * - E_rev = 0.0 mV
 * - [Mg2+] = 1.0 mM
 */
void synapse_init_nmda(nmda_state_t* state);

/**
 * @brief Initialize GABA-A synapse state
 *
 * DEFAULT PARAMETERS:
 * - g_max = 2.0 nS
 * - τ_rise = 1.0 ms
 * - τ_decay = 10.0 ms
 * - E_rev = -70.0 mV
 */
void synapse_init_gaba_a(gaba_a_state_t* state);

/**
 * @brief Initialize GABA-B synapse state
 *
 * DEFAULT PARAMETERS:
 * - g_max = 0.5 nS
 * - τ_rise = 50.0 ms
 * - τ_decay = 150.0 ms
 * - E_rev = -95.0 mV
 */
void synapse_init_gaba_b(gaba_b_state_t* state);

/**
 * @brief Initialize dopamine synapse state
 *
 * DEFAULT PARAMETERS:
 * - τ_d1 = 200.0 ms
 * - τ_d2 = 100.0 ms
 * - baseline = 0.5
 */
void synapse_init_dopamine(dopamine_state_t* state);

/**
 * @brief Initialize serotonin synapse state
 *
 * DEFAULT PARAMETERS:
 * - τ_ht1a = 500.0 ms
 * - τ_ht2a = 300.0 ms
 * - baseline = 0.5
 */
void synapse_init_serotonin(serotonin_state_t* state);

/**
 * @brief Initialize acetylcholine synapse state
 *
 * DEFAULT PARAMETERS:
 * - τ_nicotinic = 20.0 ms
 * - τ_muscarinic = 200.0 ms
 * - baseline = 0.3
 */
void synapse_init_acetylcholine(acetylcholine_state_t* state);

/**
 * @brief Initialize electrical synapse state
 *
 * DEFAULT PARAMETERS:
 * - conductance = 0.5 nS
 * - bidirectional = true
 */
void synapse_init_electrical(electrical_state_t* state);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get synapse type name as string
 *
 * @param type Synapse type enum
 * @return String name (e.g., "AMPA", "NMDA", etc.)
 */
const char* synapse_type_name(synapse_type_t type);

/**
 * @brief Get synapse type time constant
 *
 * WHAT: Return characteristic time constant for synapse type
 * WHY: Useful for debugging and adaptive timestep selection
 * HOW: Returns τ_decay for ionotropic, τ_d1 for neuromodulatory
 *
 * @param type Synapse type
 * @param state Type-specific state (to read actual τ value)
 * @return Time constant in milliseconds
 */
float synapse_type_time_constant(synapse_type_t type, const synapse_type_state_t* state);

/**
 * @brief Check if synapse type is excitatory
 *
 * @param type Synapse type
 * @return true if excitatory (AMPA, NMDA, ACh-nicotinic)
 */
bool synapse_type_is_excitatory(synapse_type_t type);

/**
 * @brief Check if synapse type is inhibitory
 *
 * @param type Synapse type
 * @return true if inhibitory (GABA-A, GABA-B)
 */
bool synapse_type_is_inhibitory(synapse_type_t type);

/**
 * @brief Check if synapse type is modulatory
 *
 * @param type Synapse type
 * @return true if modulatory (dopamine, serotonin, ACh)
 */
bool synapse_type_is_modulatory(synapse_type_t type);

//=============================================================================
// Ternary Modulation Functions (NIMCP 2.10)
//=============================================================================

/**
 * @brief Enable ternary modulation for dopamine state
 *
 * WHAT: Switch dopamine state to discrete modulation mode
 * WHY:  Efficient reward processing with discrete states
 * HOW:  Set flag and quantize current modulation
 *
 * @param state Dopamine state to modify
 * @param threshold Quantization threshold (default 0.3)
 */
void dopamine_enable_ternary_modulation(dopamine_state_t* state, float threshold);

/**
 * @brief Disable ternary modulation for dopamine state
 *
 * WHAT: Switch dopamine state back to continuous modulation
 * WHY:  Enable fine-grained modulation adjustments
 * HOW:  Clear flag, modulation remains continuous
 *
 * @param state Dopamine state to modify
 */
void dopamine_disable_ternary_modulation(dopamine_state_t* state);

/**
 * @brief Update ternary modulation from continuous value
 *
 * WHAT: Quantize continuous modulation to ternary
 * WHY:  Keep ternary state synchronized with continuous dynamics
 * HOW:  Apply threshold-based quantization
 *
 * @param state Dopamine state
 */
void dopamine_update_ternary_modulation(dopamine_state_t* state);

/**
 * @brief Get effective modulation (ternary or continuous)
 *
 * WHAT: Return modulation value regardless of mode
 * WHY:  Unified interface for synapse computation
 * HOW:  Check use_ternary_modulation flag
 *
 * @param state Dopamine state
 * @return Effective modulation value [-1, 1]
 */
float dopamine_get_effective_modulation(const dopamine_state_t* state);

/**
 * @brief Enable ternary modulation for serotonin state
 *
 * WHAT: Switch serotonin state to discrete modulation mode
 * WHY:  Efficient mood processing with discrete states
 * HOW:  Set flag and quantize current modulation
 *
 * @param state Serotonin state to modify
 * @param threshold Quantization threshold (default 0.3)
 */
void serotonin_enable_ternary_modulation(serotonin_state_t* state, float threshold);

/**
 * @brief Disable ternary modulation for serotonin state
 *
 * WHAT: Switch serotonin state back to continuous modulation
 * WHY:  Enable fine-grained modulation adjustments
 * HOW:  Clear flag, modulation remains continuous
 *
 * @param state Serotonin state to modify
 */
void serotonin_disable_ternary_modulation(serotonin_state_t* state);

/**
 * @brief Update ternary modulation from continuous value
 *
 * WHAT: Quantize continuous modulation to ternary
 * WHY:  Keep ternary state synchronized with continuous dynamics
 * HOW:  Apply threshold-based quantization
 *
 * @param state Serotonin state
 */
void serotonin_update_ternary_modulation(serotonin_state_t* state);

/**
 * @brief Get effective modulation (ternary or continuous)
 *
 * WHAT: Return modulation value regardless of mode
 * WHY:  Unified interface for synapse computation
 * HOW:  Check use_ternary_modulation flag
 *
 * @param state Serotonin state
 * @return Effective modulation value [-1, 1]
 */
float serotonin_get_effective_modulation(const serotonin_state_t* state);

/**
 * @brief Convert continuous modulation to ternary
 *
 * WHAT: Quantize modulation value to {-1, 0, +1}
 * WHY:  Utility for all neuromodulatory systems
 * HOW:  Threshold-based quantization
 *
 * @param modulation Continuous modulation [-1, 1]
 * @param threshold Quantization threshold
 * @return Ternary modulation
 */
trit_t modulation_to_ternary(float modulation, float threshold);

/**
 * @brief Convert ternary modulation to continuous
 *
 * WHAT: Expand ternary to continuous value
 * WHY:  Interface with continuous computations
 * HOW:  Map {-1, 0, +1} to {-1.0, 0.0, +1.0}
 *
 * @param ternary_modulation Ternary modulation
 * @return Continuous modulation value
 */
float ternary_to_modulation(trit_t ternary_modulation);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SYNAPSE_TYPES_H
