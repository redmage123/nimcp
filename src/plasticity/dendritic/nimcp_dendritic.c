/**
 * @file nimcp_dendritic.c
 * @brief Implementation of Dendritic Nonlinearities module
 *
 * ARCHITECTURAL OVERVIEW:
 * - Composite Pattern: Dendritic tree with branches and compartments
 * - Strategy Pattern: Different receptor types (NMDA, AMPA)
 * - Observer Pattern: Dendritic spike notifications
 *
 * PERFORMANCE OPTIMIZATIONS:
 * - SIMD-friendly loops for compartment updates
 * - Cache-coherent memory layout for branches
 * - Inline helpers for NMDA computations
 * - Branchless math in hot paths
 *
 * COMPLEXITY ANALYSIS:
 * - nmda_compute_block: O(1) per synapse
 * - compartment_integrate: O(1) per compartment
 * - dendritic_tree_update: O(b × c) branches × compartments
 *
 * BIOLOGICAL BASIS:
 * - NMDA kinetics from Jahr & Stevens 1990
 * - Dendritic spike thresholds from Larkum et al. 1999
 * - Compartmental modeling from Rall 1964
 */

#include "plasticity/dendritic/nimcp_dendritic.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "security/nimcp_security.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>

#define LOG_MODULE "plasticity_dendritic"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dendritic)

//=============================================================================
// Constants
//=============================================================================

#define EPSILON DENDRITIC_EPSILON
#define RESTING_VOLTAGE -70.0f       /**< Default resting potential (mV) */
#define CALCIUM_DECAY_TAU 50.0f      /**< Calcium decay time constant (ms) */
#define CALCIUM_SCALE 0.001f         /**< Scaling factor for calcium influx */

//=============================================================================
// Internal Dendritic Tree Structure
//=============================================================================

/**
 * @brief Internal dendritic tree structure
 *
 * WHAT: Complete state for dendritic tree computation
 * WHY:  Manage hierarchical dendritic compartments
 */
struct dendritic_tree_struct {
    dendritic_tree_config_t config;
    uint32_t num_branches;
    dendritic_branch_t* branches;

    /* NMDA receptor states (one per synapse location) */
    dendritic_nmda_state_t** nmda_states;      /**< [branch][compartment] */
    nmda_params_t nmda_params;

    /* Soma compartment (root of tree) */
    compartment_params_t soma_params;
    compartment_state_t soma_state;

    /* Statistics */
    dendritic_tree_stats_t stats;
};

//=============================================================================
// Inline Helper Functions
//=============================================================================

/**
 * @brief Safe division with epsilon guard
 */
static inline float safe_divide(float numerator, float denominator) {
    return numerator / (denominator + EPSILON);
}

/**
 * @brief Exponential decay factor
 */
static inline float decay_factor(float dt, float tau) {
    return 1.0F - expf(-dt / (tau + EPSILON));
}

//=============================================================================
// Factory Functions - NMDA Parameters
//=============================================================================

nmda_params_t nmda_params_default(void) {
    /* WHAT: Default NMDA parameters
     * WHY:  Based on Jahr & Stevens 1990 measurements
     *
     * BIOLOGICAL: Mixed NR2A/NR2B subunit composition
     */
    nmda_params_t params = {
        .g_max = 0.5F,                  /* nS - maximum conductance */
        .tau_rise = 2.0F,               /* ms - rise time constant */
        .tau_decay = 100.0F,            /* ms - decay time constant */
        .mg_concentration = NMDA_MG_CONCENTRATION,
        .mg_sensitivity = NMDA_MG_SENSITIVITY,
        .voltage_slope = NMDA_VOLTAGE_SLOPE,
        .ca_permeability = 0.1F         /* Relative Ca²⁺ permeability */
    };
    return params;
}

nmda_params_t nmda_params_nr2a(void) {
    /* WHAT: Fast NR2A-containing NMDA parameters
     * WHY:  Mature synapses have faster kinetics
     *
     * BIOLOGICAL: NR2A has τ_decay ≈ 50ms
     */
    nmda_params_t params = {
        .g_max = 0.5F,
        .tau_rise = 1.5F,               /* Faster rise */
        .tau_decay = 50.0F,             /* Faster decay than NR2B */
        .mg_concentration = NMDA_MG_CONCENTRATION,
        .mg_sensitivity = NMDA_MG_SENSITIVITY,
        .voltage_slope = NMDA_VOLTAGE_SLOPE,
        .ca_permeability = 0.08F        /* Lower Ca²⁺ permeability */
    };
    return params;
}

nmda_params_t nmda_params_nr2b(void) {
    /* WHAT: Slow NR2B-containing NMDA parameters
     * WHY:  Developmental/plastic synapses have slower kinetics
     *
     * BIOLOGICAL: NR2B has τ_decay ≈ 200ms, more Ca²⁺ influx
     */
    nmda_params_t params = {
        .g_max = 0.6F,                  /* Slightly higher conductance */
        .tau_rise = 3.0F,               /* Slower rise */
        .tau_decay = 200.0F,            /* Much slower decay */
        .mg_concentration = NMDA_MG_CONCENTRATION,
        .mg_sensitivity = NMDA_MG_SENSITIVITY,
        .voltage_slope = NMDA_VOLTAGE_SLOPE,
        .ca_permeability = 0.15F        /* Higher Ca²⁺ permeability */
    };
    return params;
}

//=============================================================================
// Factory Functions - Compartment Parameters
//=============================================================================

compartment_params_t compartment_params_default(compartment_type_t type) {
    /* WHAT: Default compartment parameters based on type
     * WHY:  Different compartments have different biophysical properties
     */
    compartment_params_t params = {
        .type = type,
        .membrane_capacitance = 1.0F,   /* pF/μm² - typical value */
        .axial_resistance = 150.0F,     /* Ω·cm - cytoplasmic resistance */
        .leak_conductance = 0.1F,       /* nS */
        .spike_threshold = -30.0F,      /* mV */
        .spike_amplitude = 40.0F,       /* mV */
        .spike_duration = 2.0F,         /* ms */
        .supralinearity_factor = 0.0F   /* Default: no supralinearity */
    };

    /* Type-specific parameters */
    switch (type) {
        case COMPARTMENT_SOMA:
            params.length = 20.0F;
            params.diameter = 20.0F;
            params.spike_threshold = -50.0F;  /* Lower threshold at soma */
            break;

        case COMPARTMENT_PROXIMAL:
            params.length = 50.0F;
            params.diameter = 3.0F;
            params.supralinearity_factor = 0.2F;
            break;

        case COMPARTMENT_DISTAL:
            params.length = 100.0F;
            params.diameter = 1.0F;
            params.supralinearity_factor = 0.5F;  /* More supralinearity distally */
            break;

        case COMPARTMENT_APICAL_TRUNK:
            params.length = 200.0F;
            params.diameter = 4.0F;
            params.supralinearity_factor = 0.3F;
            break;

        case COMPARTMENT_APICAL_TUFT:
            params.length = 150.0F;
            params.diameter = 0.8F;
            params.spike_threshold = -25.0F;  /* Higher threshold */
            params.supralinearity_factor = 0.6F;  /* Strong supralinearity */
            break;

        case COMPARTMENT_BASAL:
            params.length = 100.0F;
            params.diameter = 1.5F;
            params.supralinearity_factor = 0.4F;
            break;
    }

    return params;
}

dendritic_tree_config_t dendritic_tree_config_default(void) {
    /* WHAT: Default dendritic tree configuration
     * WHY:  Standard tree with reasonable complexity
     */
    dendritic_tree_config_t config = {
        .num_branches = 8,              /* 8 main branches */
        .compartments_per_branch = 10,  /* 10 compartments per branch */
        .default_type = COMPARTMENT_DISTAL,
        .default_spike_threshold = -30.0F,
        .default_supralinearity = 0.3F,
        .enable_nmda = true,
        .enable_dendritic_spikes = true,
        .enable_calcium_dynamics = true
    };
    return config;
}

//=============================================================================
// NMDA Receptor Functions
//=============================================================================

dendritic_nmda_state_t nmda_state_init(void) {
    /* WHAT: Initialize NMDA receptor state
     * WHY:  Factory method ensures valid initial state
     */
    dendritic_nmda_state_t state = {
        .s = 0.0F,
        .s_rise = 0.0F,
        .conductance = 0.0F,
        .calcium_influx = 0.0F,
        .active = false
    };
    return state;
}

float nmda_compute_block(float voltage, const nmda_params_t* params) {
    /* WHAT: Compute Mg²⁺ block factor B(V)
     * WHY:  NMDA receptors are voltage-dependent due to Mg²⁺ block
     *
     * FORMULA: B(V) = 1 / (1 + [Mg²⁺]/3.57 × exp(-0.062 × V))
     *
     * COMPLEXITY: O(1)
     */

    /* Guard: Validate input */
    if (!params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nmda_compute_block: params is NULL");
        return 0.0F;
    }

    /* Compute Mg²⁺ block using Jahr & Stevens 1990 formula */
    float mg_term = params->mg_concentration / params->mg_sensitivity;
    float voltage_term = expf(-params->voltage_slope * voltage);
    float block = 1.0F / (1.0F + mg_term * voltage_term);

    return nimcp_clampf(block, 0.0F, 1.0F);
}

void nmda_update_kinetics(dendritic_nmda_state_t* state,
                          float glutamate,
                          float dt,
                          const nmda_params_t* params) {
    /* WHAT: Update NMDA gating variable kinetics
     * WHY:  NMDA has slow, dual-exponential kinetics
     *
     * FORMULA: ds/dt = (1-s)/τ_rise × glutamate - s/τ_decay
     *
     * COMPLEXITY: O(1)
     */

    /* Guard: Validate inputs */
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nmda_update_kinetics: state is NULL");
        return;
    }
    if (!params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nmda_update_kinetics: params is NULL");
        return;
    }
    if (dt <= 0.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nmda_update_kinetics: dt must be positive");
        return;
    }

    /* Clamp glutamate to [0,1] */
    glutamate = nimcp_clampf(glutamate, 0.0F, 1.0F);

    /* Update active state */
    state->active = (glutamate > 0.1F);

    /* Rise phase: (1-s)/τ_rise × glutamate */
    float rise_decay = decay_factor(dt, params->tau_rise);
    float rise_term = (1.0F - state->s) * glutamate * rise_decay;

    /* Decay phase: s/τ_decay */
    float decay_decay_f = decay_factor(dt, params->tau_decay);
    float decay_term = state->s * decay_decay_f;

    /* Update gating variable */
    state->s += rise_term - decay_term;
    state->s = nimcp_clampf(state->s, 0.0F, 1.0F);

    /* Track rising component separately for dual-exponential */
    state->s_rise += rise_decay * (glutamate - state->s_rise);
}

float nmda_compute_current(const dendritic_nmda_state_t* state,
                           float voltage,
                           const nmda_params_t* params) {
    /* WHAT: Compute NMDA current
     * WHY:  Combine kinetics and voltage dependence
     *
     * FORMULA: I_NMDA = g_max × B(V) × s × (V - E_NMDA)
     *
     * COMPLEXITY: O(1)
     */

    /* Guard: Validate inputs */
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nmda_compute_current: state is NULL");
        return 0.0F;
    }
    if (!params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nmda_compute_current: params is NULL");
        return 0.0F;
    }

    /* Compute voltage-dependent block */
    float block = nmda_compute_block(voltage, params);

    /* Compute effective conductance */
    float g_nmda = params->g_max * block * state->s;

    /* Compute current (negative = inward current for excitation) */
    float current = g_nmda * (voltage - E_NMDA);

    return current;
}

float nmda_compute_calcium_influx(const dendritic_nmda_state_t* state,
                                  float voltage,
                                  const nmda_params_t* params) {
    /* WHAT: Compute Ca²⁺ influx through NMDA
     * WHY:  Calcium is crucial for plasticity signaling
     *
     * FORMULA: Ca_influx ∝ g_NMDA × driving_force × f_Ca
     *
     * COMPLEXITY: O(1)
     */

    /* Guard: Validate inputs */
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nmda_compute_calcium_influx: state is NULL");
        return 0.0F;
    }
    if (!params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nmda_compute_calcium_influx: params is NULL");
        return 0.0F;
    }

    /* Compute block and conductance */
    float block = nmda_compute_block(voltage, params);
    float g_nmda = params->g_max * block * state->s;

    /* Ca²⁺ driving force (reversal around +130 mV) */
    float e_ca = 130.0F;
    float driving_force = voltage - e_ca;

    /* Calcium influx (inward = negative voltage × negative driving_force = positive) */
    float influx = -g_nmda * driving_force * params->ca_permeability * CALCIUM_SCALE;

    return fmaxf(influx, 0.0F);  /* Only positive influx */
}

//=============================================================================
// Dendritic Compartment Functions
//=============================================================================

compartment_state_t compartment_state_init(float resting_voltage) {
    /* WHAT: Initialize compartment state
     * WHY:  Factory method ensures valid initial state
     */
    compartment_state_t state = {
        .voltage = resting_voltage,
        .voltage_prev = resting_voltage,
        .total_excitatory = 0.0F,
        .total_inhibitory = 0.0F,
        .calcium_concentration = 0.0F,
        .spike_active = false,
        .spike_time_remaining = 0.0F,
        .spike_count = 0
    };
    return state;
}

void compartment_integrate(compartment_state_t* state,
                           float excitatory_input,
                           float inhibitory_input,
                           const compartment_params_t* params,
                           float dt) {
    /* WHAT: Integrate synaptic inputs in compartment
     * WHY:  Core dendritic computation
     *
     * FORMULA: dV/dt = (g_leak × (E_leak - V) + g_exc × (E_exc - V) + g_inh × (E_inh - V)) / C
     *
     * COMPLEXITY: O(1)
     */

    /* Guard: Validate inputs */
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "compartment_integrate: state is NULL");
        return;
    }
    if (!params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "compartment_integrate: params is NULL");
        return;
    }
    if (dt <= 0.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "compartment_integrate: dt must be positive");
        return;
    }

    /* Store previous voltage for spike detection */
    state->voltage_prev = state->voltage;

    /* If spike is active, maintain spike voltage */
    if (state->spike_active) {
        state->spike_time_remaining -= dt;
        if (state->spike_time_remaining <= 0.0F) {
            state->spike_active = false;
            state->spike_time_remaining = 0.0F;
        }
        /* During spike, don't integrate - voltage is elevated */
        return;
    }

    /* Accumulate inputs */
    state->total_excitatory = excitatory_input;
    state->total_inhibitory = inhibitory_input;

    /* Compute conductance-based currents */
    float i_leak = params->leak_conductance * (E_LEAK - state->voltage);
    float i_exc = excitatory_input * (E_AMPA - state->voltage);
    float i_inh = inhibitory_input * (E_GABA_A - state->voltage);

    /* Total current */
    float i_total = i_leak + i_exc + i_inh;

    /* Apply supralinear summation if threshold exceeded */
    float total_input = excitatory_input + inhibitory_input;
    if (params->supralinearity_factor > 0.0F && total_input > 0.0F) {
        float supralinear = compartment_supralinear_factor(
            excitatory_input,
            0.5F,  /* threshold */
            params->supralinearity_factor
        );
        i_total *= supralinear;
    }

    /* Integrate voltage (Euler method) */
    float capacitance = params->membrane_capacitance * params->length * params->diameter * 3.14159F;
    capacitance = fmaxf(capacitance, EPSILON);  /* Prevent division by zero */
    float dv = (i_total / capacitance) * dt;
    state->voltage += dv;

    /* Clamp voltage to physiological range */
    state->voltage = nimcp_clampf(state->voltage, -100.0F, 50.0F);

    /* Decay calcium */
    float ca_decay = decay_factor(dt, CALCIUM_DECAY_TAU);
    state->calcium_concentration *= (1.0F - ca_decay);
}

bool compartment_check_spike(compartment_state_t* state,
                             const compartment_params_t* params) {
    /* WHAT: Detect and generate dendritic spike
     * WHY:  Dendritic spikes are local regenerative events
     *
     * COMPLEXITY: O(1)
     */

    /* Guard: Validate inputs */
    if (!state) {
        return false;
    }
    if (!params) {
        return false;
    }

    /* Already in spike state */
    if (state->spike_active) return false;

    /* Check threshold crossing (rising edge) */
    bool crossed = (state->voltage >= params->spike_threshold &&
                   state->voltage_prev < params->spike_threshold);

    if (crossed) {
        /* Generate dendritic spike */
        state->voltage += params->spike_amplitude;
        state->spike_active = true;
        state->spike_time_remaining = params->spike_duration;
        state->spike_count++;

        /* Calcium influx during spike */
        state->calcium_concentration += 0.5F;  /* μM */

        return true;
    }

    return false;
}

float compartment_supralinear_factor(float total_input,
                                     float threshold,
                                     float supralinearity_factor) {
    /* WHAT: Compute supralinear summation boost
     * WHY:  NMDA creates supralinear summation for clustered inputs
     *
     * FORMULA: factor = 1 + supralinearity × max(0, input - threshold)
     *
     * COMPLEXITY: O(1)
     */

    if (total_input <= threshold) {
        return 1.0F;  /* Linear region */
    }

    /* Supralinear boost */
    float excess = total_input - threshold;
    float boost = 1.0F + supralinearity_factor * excess;

    /* Clamp to reasonable range */
    return nimcp_clampf(boost, 1.0F, 3.0F);
}

//=============================================================================
// Dendritic Tree Functions
//=============================================================================

dendritic_tree_t dendritic_tree_create(const dendritic_tree_config_t* config) {
    /* WHAT: Create dendritic tree
     * WHY:  Factory method for complete dendritic arbor
     *
     * COMPLEXITY: O(branches × compartments)
     */

    /* Guard: Validate config */
    if (!config) {
        NIMCP_LOGGING_ERROR("Null config in dendritic_tree_create");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;
    }
    if (config->num_branches == 0 || config->num_branches > DENDRITIC_MAX_BRANCHES) {
        NIMCP_LOGGING_ERROR("Invalid num_branches: %u", config->num_branches);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dendritic_tree_create: invalid num_branches");
        return NULL;
    }
    if (config->compartments_per_branch == 0 ||
        config->compartments_per_branch > DENDRITIC_MAX_COMPARTMENTS) {
        NIMCP_LOGGING_ERROR("Invalid compartments_per_branch: %u", config->compartments_per_branch);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dendritic_tree_create: invalid compartments_per_branch");
        return NULL;
    }

    /* Allocate tree structure */
    dendritic_tree_t tree = nimcp_calloc(1, sizeof(struct dendritic_tree_struct));
    if (!tree) {
        NIMCP_LOGGING_ERROR("Failed to allocate dendritic tree");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dendritic_tree_create: failed to allocate tree");
        return NULL;
    }

    /* Copy configuration */
    memcpy(&tree->config, config, sizeof(dendritic_tree_config_t));
    tree->num_branches = config->num_branches;

    /* Set NMDA parameters */
    tree->nmda_params = nmda_params_default();

    /* Initialize soma */
    tree->soma_params = compartment_params_default(COMPARTMENT_SOMA);
    tree->soma_state = compartment_state_init(RESTING_VOLTAGE);

    /* Allocate branches */
    tree->branches = nimcp_calloc(config->num_branches, sizeof(dendritic_branch_t));
    if (!tree->branches) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dendritic_tree_create: failed to allocate branches");
        dendritic_tree_destroy(tree);
        return NULL;
    }

    /* Allocate NMDA states array if enabled */
    if (config->enable_nmda) {
        tree->nmda_states = nimcp_calloc(config->num_branches, sizeof(dendritic_nmda_state_t*));
        if (!tree->nmda_states) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dendritic_tree_create: failed to allocate nmda_states");
            dendritic_tree_destroy(tree);
            return NULL;
        }
    }

    /* Initialize each branch */
    for (uint32_t b = 0; b < config->num_branches; b++) {
        dendritic_branch_t* branch = &tree->branches[b];
        branch->id = b;
        branch->parent_id = 0;  /* All connect to soma */
        branch->num_compartments = config->compartments_per_branch;
        branch->coupling_resistance = 100.0F;  /* Ω·cm */

        /* Allocate compartment parameters */
        branch->params = nimcp_calloc(config->compartments_per_branch,
                                      sizeof(compartment_params_t));
        if (!branch->params) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dendritic_tree_create: failed to allocate branch params");
            dendritic_tree_destroy(tree);
            return NULL;
        }

        /* Allocate compartment states */
        branch->states = nimcp_calloc(config->compartments_per_branch,
                                      sizeof(compartment_state_t));
        if (!branch->states) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dendritic_tree_create: failed to allocate branch states");
            dendritic_tree_destroy(tree);
            return NULL;
        }

        /* Initialize compartments with gradient from proximal to distal */
        for (uint32_t c = 0; c < config->compartments_per_branch; c++) {
            /* Determine compartment type based on position */
            compartment_type_t type;
            if (c < config->compartments_per_branch / 3) {
                type = COMPARTMENT_PROXIMAL;
            } else if (c < 2 * config->compartments_per_branch / 3) {
                type = config->default_type;
            } else {
                type = COMPARTMENT_DISTAL;
            }

            branch->params[c] = compartment_params_default(type);
            branch->params[c].spike_threshold = config->default_spike_threshold;
            branch->params[c].supralinearity_factor = config->default_supralinearity;
            branch->states[c] = compartment_state_init(RESTING_VOLTAGE);
        }

        /* Allocate NMDA states for this branch */
        if (config->enable_nmda && tree->nmda_states) {
            tree->nmda_states[b] = nimcp_calloc(config->compartments_per_branch,
                                                sizeof(dendritic_nmda_state_t));
            if (!tree->nmda_states[b]) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dendritic_tree_create: failed to allocate nmda_states for branch");
                dendritic_tree_destroy(tree);
                return NULL;
            }
            for (uint32_t c = 0; c < config->compartments_per_branch; c++) {
                tree->nmda_states[b][c] = nmda_state_init();
            }
        }
    }

    /* Initialize statistics */
    memset(&tree->stats, 0, sizeof(dendritic_tree_stats_t));

    NIMCP_LOGGING_INFO("Created dendritic tree: branches=%u, compartments=%u, nmda=%d",
                       config->num_branches, config->compartments_per_branch, config->enable_nmda);

    return tree;
}

void dendritic_tree_destroy(dendritic_tree_t tree) {
    /* WHAT: Free tree resources
     * WHY:  Prevent memory leaks
     */

    if (!tree) return;

    /* Free NMDA states */
    if (tree->nmda_states) {
        for (uint32_t b = 0; b < tree->num_branches; b++) {
            nimcp_free(tree->nmda_states[b]);
        }
        nimcp_free(tree->nmda_states);
    }

    /* Free branches */
    if (tree->branches) {
        for (uint32_t b = 0; b < tree->num_branches; b++) {
            nimcp_free(tree->branches[b].params);
            nimcp_free(tree->branches[b].states);
        }
        nimcp_free(tree->branches);
    }

    nimcp_free(tree);
}

void dendritic_tree_inject_input(dendritic_tree_t tree,
                                 uint32_t branch_id,
                                 uint32_t compartment_id,
                                 float excitatory,
                                 float inhibitory,
                                 float nmda_glutamate) {
    /* WHAT: Inject synaptic input to specific compartment
     * WHY:  Synapses target specific dendritic locations
     *
     * COMPLEXITY: O(1)
     */

    /* Guard: Validate inputs */
    if (!tree) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_tree_inject_input: tree is NULL");
        return;
    }
    if (branch_id >= tree->num_branches) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dendritic_tree_inject_input: invalid branch_id");
        return;
    }
    if (compartment_id >= tree->branches[branch_id].num_compartments) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dendritic_tree_inject_input: invalid compartment_id");
        return;
    }

    dendritic_branch_t* branch = &tree->branches[branch_id];
    compartment_state_t* state = &branch->states[compartment_id];

    /* Accumulate inputs */
    state->total_excitatory += excitatory;
    state->total_inhibitory += inhibitory;

    /* Update NMDA if enabled */
    if (tree->config.enable_nmda && tree->nmda_states && tree->nmda_states[branch_id]) {
        dendritic_nmda_state_t* nmda = &tree->nmda_states[branch_id][compartment_id];
        nmda_update_kinetics(nmda, nmda_glutamate, 1.0F, &tree->nmda_params);

        if (nmda->active) {
            tree->stats.nmda_activations++;
        }
    }
}

void dendritic_tree_update(dendritic_tree_t tree, float dt) {
    /* WHAT: Update dendritic tree for one timestep
     * WHY:  Propagate voltages and update all compartments
     *
     * ALGORITHM:
     * 1. Update each branch's compartments (distal to proximal)
     * 2. Propagate to soma
     * 3. Update NMDA states
     * 4. Check for dendritic spikes
     *
     * COMPLEXITY: O(branches × compartments)
     */

    /* Guard: Validate inputs */
    if (!tree) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_tree_update: tree is NULL");
        return;
    }
    if (dt <= 0.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dendritic_tree_update: dt must be positive");
        return;
    }

    tree->stats.total_updates++;
    float total_voltage = 0.0F;
    float max_calcium = 0.0F;

    /* Update each branch */
    for (uint32_t b = 0; b < tree->num_branches; b++) {
        dendritic_branch_t* branch = &tree->branches[b];

        /* Process compartments from distal to proximal */
        for (int32_t c = (int32_t)branch->num_compartments - 1; c >= 0; c--) {
            compartment_state_t* state = &branch->states[c];
            const compartment_params_t* params = &branch->params[c];

            /* Get excitatory input including NMDA */
            float exc_input = state->total_excitatory;

            if (tree->config.enable_nmda && tree->nmda_states && tree->nmda_states[b]) {
                dendritic_nmda_state_t* nmda = &tree->nmda_states[b][c];
                float nmda_current = nmda_compute_current(nmda, state->voltage, &tree->nmda_params);
                exc_input += fabsf(nmda_current);  /* Add NMDA contribution */

                /* Update calcium */
                if (tree->config.enable_calcium_dynamics) {
                    float ca_influx = nmda_compute_calcium_influx(nmda, state->voltage, &tree->nmda_params);
                    state->calcium_concentration += ca_influx * dt;
                }
            }

            /* Integrate compartment */
            compartment_integrate(state, exc_input, state->total_inhibitory, params, dt);

            /* Check for dendritic spike */
            if (tree->config.enable_dendritic_spikes) {
                if (compartment_check_spike(state, params)) {
                    tree->stats.dendritic_spikes++;

                    /* Check for supralinear event */
                    if (params->supralinearity_factor > 0.0F && exc_input > 0.5F) {
                        tree->stats.supralinear_events += 1.0F;
                    }
                }
            }

            /* Propagate voltage to parent compartment (toward soma) */
            if (c > 0) {
                compartment_state_t* parent = &branch->states[c - 1];
                float coupling = safe_divide(1.0F, branch->coupling_resistance);
                parent->total_excitatory += coupling * (state->voltage - parent->voltage);
            }

            /* Clear accumulated inputs for next timestep */
            state->total_excitatory = 0.0F;
            state->total_inhibitory = 0.0F;

            /* Track statistics */
            total_voltage += state->voltage;
            if (state->calcium_concentration > max_calcium) {
                max_calcium = state->calcium_concentration;
            }
        }

        /* Couple branch to soma */
        if (branch->num_compartments > 0) {
            compartment_state_t* proximal = &branch->states[0];
            float coupling = safe_divide(1.0F, branch->coupling_resistance);
            float current_to_soma = coupling * (proximal->voltage - tree->soma_state.voltage);
            tree->soma_state.total_excitatory += current_to_soma;
        }
    }

    /* Update soma */
    compartment_integrate(&tree->soma_state,
                          tree->soma_state.total_excitatory,
                          tree->soma_state.total_inhibitory,
                          &tree->soma_params, dt);
    tree->soma_state.total_excitatory = 0.0F;
    tree->soma_state.total_inhibitory = 0.0F;

    /* Update statistics */
    uint32_t total_compartments = tree->num_branches * tree->config.compartments_per_branch;
    tree->stats.mean_voltage = total_voltage / (float)(total_compartments + 1);
    tree->stats.max_calcium = max_calcium;
}

float dendritic_tree_get_soma_voltage(dendritic_tree_t tree) {
    /* WHAT: Read somatic voltage
     * WHY:  Soma voltage determines action potential generation
     */
    if (!tree) return RESTING_VOLTAGE;
    return tree->soma_state.voltage;
}

float dendritic_tree_get_total_calcium(dendritic_tree_t tree) {
    /* WHAT: Sum calcium across all compartments
     * WHY:  Total calcium affects plasticity
     */
    if (!tree) return 0.0F;

    float total = tree->soma_state.calcium_concentration;

    for (uint32_t b = 0; b < tree->num_branches; b++) {
        for (uint32_t c = 0; c < tree->branches[b].num_compartments; c++) {
            total += tree->branches[b].states[c].calcium_concentration;
        }
    }

    return total;
}

bool dendritic_tree_get_stats(dendritic_tree_t tree,
                              dendritic_tree_stats_t* stats) {
    /* WHAT: Retrieve monitoring metrics
     * WHY:  Track dendritic activity
     */
    if (!tree) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_tree_get_stats: tree is NULL");
        return false;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_tree_get_stats: stats is NULL");
        return false;
    }

    memcpy(stats, &tree->stats, sizeof(dendritic_tree_stats_t));
    return true;
}

void dendritic_tree_reset(dendritic_tree_t tree) {
    /* WHAT: Reset tree to resting state
     * WHY:  Clear activity for new stimulus
     */
    if (!tree) return;

    /* Reset soma */
    tree->soma_state = compartment_state_init(RESTING_VOLTAGE);

    /* Reset all branches */
    for (uint32_t b = 0; b < tree->num_branches; b++) {
        for (uint32_t c = 0; c < tree->branches[b].num_compartments; c++) {
            tree->branches[b].states[c] = compartment_state_init(RESTING_VOLTAGE);

            if (tree->nmda_states && tree->nmda_states[b]) {
                tree->nmda_states[b][c] = nmda_state_init();
            }
        }
    }

    /* Reset statistics (keep total_updates) */
    uint64_t updates = tree->stats.total_updates;
    memset(&tree->stats, 0, sizeof(dendritic_tree_stats_t));
    tree->stats.total_updates = updates;
}
