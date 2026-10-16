/**
 * @file nimcp_receptor_subtypes.c
 * @brief Implementation of receptor subtype binding kinetics (Phase C2.2)
 *
 * WHAT: Implements Hill equation binding, receptor profiles, drug interactions
 * WHY:  Model differential receptor effects (D1 excitatory, D2 inhibitory)
 * HOW:  Mass action kinetics with per-neuron receptor expression
 *
 * @version Phase C2.2 Enhancement #1
 * @date 2025-11-12
 */

#include "plasticity/neuromodulators/nimcp_receptor_subtypes.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "security/nimcp_security.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>

#define LOG_MODULE "plasticity_receptor_subtypes"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(receptor_subtypes)

// ============================================================================
// Default Receptor Configurations
// ============================================================================

receptor_config_t dopamine_receptor_default_config(dopamine_receptor_subtype_t subtype) {
    receptor_config_t config = {0};
    config.hill_coefficient = 1.0F;  // Non-cooperative binding (typical for GPCRs)
    config.desensitization_rate = 0.1F;  // Slow desensitization (10s timescale)
    config.max_effect = 1.0F;

    switch (subtype) {
        case DOPAMINE_D1:
            config.kd = DOPAMINE_D1_KD;
            config.expression_level = 0.7F;  // High cortical expression
            config.is_excitatory = true;     // Gs-coupled, increases cAMP
            break;

        case DOPAMINE_D2:
            config.kd = DOPAMINE_D2_KD;
            config.expression_level = 0.8F;  // High striatal expression
            config.is_excitatory = false;    // Gi-coupled, decreases cAMP
            break;

        case DOPAMINE_D3:
            config.kd = DOPAMINE_D3_KD;
            config.expression_level = 0.3F;  // Moderate limbic expression
            config.is_excitatory = false;
            break;

        case DOPAMINE_D4:
            config.kd = DOPAMINE_D4_KD;
            config.expression_level = 0.4F;  // Moderate PFC expression
            config.is_excitatory = false;
            break;

        case DOPAMINE_D5:
            config.kd = DOPAMINE_D5_KD;
            config.expression_level = 0.5F;  // Moderate hippocampal expression
            config.is_excitatory = true;     // Gs-coupled
            break;

        default:
            config.kd = 1.0e-3F;  // Default 1 nM
            config.expression_level = 0.5F;
            config.is_excitatory = true;
            break;
    }

    return config;
}

receptor_config_t serotonin_receptor_default_config(serotonin_receptor_subtype_t subtype) {
    receptor_config_t config = {0};
    config.hill_coefficient = 1.0F;
    config.desensitization_rate = 0.05F;  // Slower than dopamine
    config.max_effect = 1.0F;

    switch (subtype) {
        case SEROTONIN_5HT1A:
            config.kd = SEROTONIN_5HT1A_KD;
            config.expression_level = 0.6F;
            config.is_excitatory = false;  // Gi-coupled
            break;

        case SEROTONIN_5HT2A:
            config.kd = SEROTONIN_5HT2A_KD;
            config.expression_level = 0.7F;
            config.is_excitatory = true;   // Gq-coupled
            break;

        case SEROTONIN_5HT1B:
            config.kd = 2.0e-3F;
            config.expression_level = 0.4F;
            config.is_excitatory = false;
            break;

        case SEROTONIN_5HT2C:
            config.kd = 3.0e-3F;
            config.expression_level = 0.5F;
            config.is_excitatory = true;
            break;

        case SEROTONIN_5HT3:
            config.kd = 5.0e-3F;  // Ionotropic, lower affinity
            config.expression_level = 0.3F;
            config.is_excitatory = true;
            break;

        case SEROTONIN_5HT4:
            config.kd = 4.0e-3F;
            config.expression_level = 0.3F;
            config.is_excitatory = true;
            break;

        case SEROTONIN_5HT7:
            config.kd = 6.0e-3F;
            config.expression_level = 0.4F;
            config.is_excitatory = true;
            break;

        default:
            config.kd = 2.0e-3F;
            config.expression_level = 0.5F;
            config.is_excitatory = true;
            break;
    }

    return config;
}

// ============================================================================
// Hill Equation Binding Kinetics
// ============================================================================

/**
 * @brief Compute receptor occupancy using Hill equation
 *
 * Hill equation: occupancy = [L]^n / (Kd^n + [L]^n)
 * Where:
 *   [L] = free ligand (neurotransmitter) concentration
 *   Kd = dissociation constant (half-maximal binding)
 *   n = Hill coefficient (cooperativity)
 *
 * For n=1 (non-cooperative), reduces to Michaelis-Menten form
 */
static float hill_equation(float concentration, float kd, float hill_coef) {
    if (concentration <= 0.0F) {
        return 0.0F;
    }

    float conc_n = powf(concentration, hill_coef);
    float kd_n = powf(kd, hill_coef);

    return conc_n / (kd_n + conc_n);
}

void receptor_update_binding(
    const receptor_config_t* config,
    receptor_state_t* state,
    float free_concentration,
    float dt
) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "receptor_update_binding: null config pointer");
        return;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "receptor_update_binding: null state pointer");
        return;
    }

    // Compute equilibrium occupancy
    float target_occupancy = hill_equation(
        free_concentration,
        config->kd,
        config->hill_coefficient
    );

    // Apply expression level (some neurons express more receptors)
    target_occupancy *= config->expression_level;

    // Exponential relaxation to equilibrium (fast timescale ~100ms)
    float tau_binding = 0.1F;  // 100ms time constant
    float alpha = expf(-dt / tau_binding);
    state->occupancy = alpha * state->occupancy + (1.0F - alpha) * target_occupancy;

    // Update desensitization (slow process)
    // High occupancy → increased desensitization
    float desensitization_target = state->occupancy * 0.5F;  // Max 50% desensitization
    float tau_desensitization = 10.0F;  // 10s time constant
    float beta = expf(-dt / tau_desensitization);
    state->desensitization = beta * state->desensitization +
                            (1.0F - beta) * desensitization_target;

    // Functional output = occupancy * (1 - desensitization)
    state->functional_output = state->occupancy * (1.0F - state->desensitization);
}

// ============================================================================
// Dopamine Receptor System
// ============================================================================

void dopamine_receptor_system_init(dopamine_receptor_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dopamine_receptor_system_init: null system pointer");
        return;
    }
    memset(system, 0, sizeof(*system));

    // Initialize all receptor subtypes
    for (int i = 0; i < DOPAMINE_RECEPTOR_COUNT; i++) {
        system->config[i] = dopamine_receptor_default_config((dopamine_receptor_subtype_t)i);
    }
}

float dopamine_receptor_compute_modulation(
    dopamine_receptor_system_t* system,
    float free_concentration,
    float dt
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dopamine_receptor_compute_modulation: null system pointer");
        return 0.0f;
    }
    system->free_concentration = free_concentration;
    system->total_excitation = 0.0F;
    system->total_inhibition = 0.0F;

    // Update each receptor subtype
    for (int i = 0; i < DOPAMINE_RECEPTOR_COUNT; i++) {
        receptor_update_binding(
            &system->config[i],
            &system->state[i],
            free_concentration,
            dt
        );

        float effect = system->state[i].functional_output * system->config[i].max_effect;

        if (system->config[i].is_excitatory) {
            system->total_excitation += effect;
        } else {
            system->total_inhibition += effect;
        }
    }

    // Net modulation = excitation - inhibition
    system->net_modulation = system->total_excitation - system->total_inhibition;

    return system->net_modulation;
}

void dopamine_receptor_apply_d2_blockade(
    dopamine_receptor_system_t* system,
    float blockade_fraction
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dopamine_receptor_apply_d2_blockade: null system pointer");
        return;
    }
    // Antipsychotic drugs block D2 receptors
    // Reduce effective expression level
    system->config[DOPAMINE_D2].expression_level *= (1.0F - blockade_fraction);

    // Also affects D3 and D4 (structurally similar)
    system->config[DOPAMINE_D3].expression_level *= (1.0F - blockade_fraction * 0.7F);
    system->config[DOPAMINE_D4].expression_level *= (1.0F - blockade_fraction * 0.5F);
}

// ============================================================================
// Serotonin Receptor System
// ============================================================================

void serotonin_receptor_system_init(serotonin_receptor_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "serotonin_receptor_system_init: null system pointer");
        return;
    }
    memset(system, 0, sizeof(*system));

    for (int i = 0; i < SEROTONIN_RECEPTOR_COUNT; i++) {
        system->config[i] = serotonin_receptor_default_config((serotonin_receptor_subtype_t)i);
    }
}

float serotonin_receptor_compute_modulation(
    serotonin_receptor_system_t* system,
    float free_concentration,
    float dt
);  // Forward declaration - implemented below

float serotonin_receptor_compute_modulation(
    serotonin_receptor_system_t* system,
    float free_concentration,
    float dt
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "serotonin_receptor_compute_modulation: null system pointer");
        return 0.0f;
    }
    system->free_concentration = free_concentration;
    system->total_excitation = 0.0F;
    system->total_inhibition = 0.0F;

    for (int i = 0; i < SEROTONIN_RECEPTOR_COUNT; i++) {
        receptor_update_binding(
            &system->config[i],
            &system->state[i],
            free_concentration,
            dt
        );

        float effect = system->state[i].functional_output * system->config[i].max_effect;

        if (system->config[i].is_excitatory) {
            system->total_excitation += effect;
        } else {
            system->total_inhibition += effect;
        }
    }

    system->net_modulation = system->total_excitation - system->total_inhibition;
    return system->net_modulation;
}

float serotonin_receptor_apply_ssri(
    serotonin_receptor_system_t* system,
    float reuptake_inhibition,
    float baseline_concentration
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "serotonin_receptor_apply_ssri: null system pointer");
        return 0.0f;
    }
    // SSRIs block serotonin reuptake, increasing synaptic concentration
    // Model: new_conc = baseline / (1 - reuptake_inhibition)
    // Example: 0.9 inhibition → 10x increase
    float inhibition_capped = fminf(reuptake_inhibition, 0.99F);  // Prevent division by zero
    float concentration_multiplier = 1.0F / (1.0F - inhibition_capped);

    return baseline_concentration * concentration_multiplier;
}

// ============================================================================
// Acetylcholine Receptor System
// ============================================================================

void acetylcholine_receptor_system_init(acetylcholine_receptor_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "acetylcholine_receptor_system_init: null system pointer");
        return;
    }
    memset(system, 0, sizeof(*system));

    // Simplified ACh receptor configurations
    system->config[ACH_NICOTINIC].kd = 1.0e-3F;
    system->config[ACH_NICOTINIC].expression_level = 0.6F;
    system->config[ACH_NICOTINIC].is_excitatory = true;
    system->config[ACH_NICOTINIC].hill_coefficient = 1.5F;  // Cooperative binding

    system->config[ACH_MUSCARINIC_M1].kd = 0.5e-3F;
    system->config[ACH_MUSCARINIC_M1].expression_level = 0.7F;
    system->config[ACH_MUSCARINIC_M1].is_excitatory = true;
    system->config[ACH_MUSCARINIC_M1].hill_coefficient = 1.0F;

    system->config[ACH_MUSCARINIC_M2].kd = 0.8e-3F;
    system->config[ACH_MUSCARINIC_M2].expression_level = 0.5F;
    system->config[ACH_MUSCARINIC_M2].is_excitatory = false;
    system->config[ACH_MUSCARINIC_M2].hill_coefficient = 1.0F;

    for (int i = 0; i < ACH_RECEPTOR_COUNT; i++) {
        system->config[i].max_effect = 1.0F;
        system->config[i].desensitization_rate = 0.15F;
    }
}

// ============================================================================
// Norepinephrine Receptor System
// ============================================================================

void norepinephrine_receptor_system_init(norepinephrine_receptor_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "norepinephrine_receptor_system_init: null system pointer");
        return;
    }
    memset(system, 0, sizeof(*system));

    // α1: excitatory
    system->config[NOREPINEPHRINE_ALPHA1].kd = 0.3e-3F;
    system->config[NOREPINEPHRINE_ALPHA1].expression_level = 0.6F;
    system->config[NOREPINEPHRINE_ALPHA1].is_excitatory = true;

    // α2: inhibitory (autoreceptor)
    system->config[NOREPINEPHRINE_ALPHA2].kd = 0.2e-3F;  // High affinity
    system->config[NOREPINEPHRINE_ALPHA2].expression_level = 0.4F;
    system->config[NOREPINEPHRINE_ALPHA2].is_excitatory = false;

    // β1, β2, β3: excitatory
    system->config[NOREPINEPHRINE_BETA1].kd = 0.5e-3F;
    system->config[NOREPINEPHRINE_BETA1].expression_level = 0.5F;
    system->config[NOREPINEPHRINE_BETA1].is_excitatory = true;

    system->config[NOREPINEPHRINE_BETA2].kd = 0.6e-3F;
    system->config[NOREPINEPHRINE_BETA2].expression_level = 0.7F;  // High in cortex
    system->config[NOREPINEPHRINE_BETA2].is_excitatory = true;

    system->config[NOREPINEPHRINE_BETA3].kd = 1.0e-3F;
    system->config[NOREPINEPHRINE_BETA3].expression_level = 0.3F;
    system->config[NOREPINEPHRINE_BETA3].is_excitatory = true;

    for (int i = 0; i < NOREPINEPHRINE_RECEPTOR_COUNT; i++) {
        system->config[i].max_effect = 1.0F;
        system->config[i].hill_coefficient = 1.0F;
        system->config[i].desensitization_rate = 0.08F;
    }
}

// ============================================================================
// Regional Receptor Profiles
// ============================================================================

neuron_receptor_profile_t receptor_profile_cortical(void) {
    neuron_receptor_profile_t profile = {0};

    // Initialize all systems
    dopamine_receptor_system_init(&profile.dopamine);
    serotonin_receptor_system_init(&profile.serotonin);
    acetylcholine_receptor_system_init(&profile.acetylcholine);
    norepinephrine_receptor_system_init(&profile.norepinephrine);

    // Cortical pattern: High D1, low D2
    profile.dopamine.config[DOPAMINE_D1].expression_level = 0.9F;  // Very high
    profile.dopamine.config[DOPAMINE_D2].expression_level = 0.3F;  // Low
    profile.dopamine.config[DOPAMINE_D4].expression_level = 0.6F;  // Moderate

    // High serotonin 5-HT2A (cortical layer 5)
    profile.serotonin.config[SEROTONIN_5HT2A].expression_level = 0.9F;

    // High ACh (cortical arousal)
    profile.acetylcholine.config[ACH_NICOTINIC].expression_level = 0.8F;
    profile.acetylcholine.config[ACH_MUSCARINIC_M1].expression_level = 0.8F;

    // High NE beta-2 (learning/plasticity)
    profile.norepinephrine.config[NOREPINEPHRINE_BETA2].expression_level = 0.9F;

    return profile;
}

neuron_receptor_profile_t receptor_profile_striatal(void) {
    neuron_receptor_profile_t profile = {0};

    dopamine_receptor_system_init(&profile.dopamine);
    serotonin_receptor_system_init(&profile.serotonin);
    acetylcholine_receptor_system_init(&profile.acetylcholine);
    norepinephrine_receptor_system_init(&profile.norepinephrine);

    // Striatal pattern: Very high D2, moderate D1
    profile.dopamine.config[DOPAMINE_D1].expression_level = 0.5F;  // Moderate
    profile.dopamine.config[DOPAMINE_D2].expression_level = 0.95F; // Very high (MSNs)
    profile.dopamine.config[DOPAMINE_D3].expression_level = 0.6F;  // High in ventral striatum

    // Moderate serotonin
    profile.serotonin.config[SEROTONIN_5HT1B].expression_level = 0.7F;

    // Very high ACh (striatal interneurons)
    profile.acetylcholine.config[ACH_NICOTINIC].expression_level = 0.9F;
    profile.acetylcholine.config[ACH_MUSCARINIC_M4].expression_level = 0.8F;

    // Moderate NE
    profile.norepinephrine.config[NOREPINEPHRINE_BETA1].expression_level = 0.4F;

    return profile;
}

void neuron_receptor_profile_init(neuron_receptor_profile_t* profile, brain_region_t region) {
    if (!profile) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuron_receptor_profile_init: null profile pointer");
        return;
    }
    switch (region) {
        case BRAIN_REGION_CORTEX:
            *profile = receptor_profile_cortical();
            break;

        case BRAIN_REGION_STRIATUM:
            *profile = receptor_profile_striatal();
            break;

        case BRAIN_REGION_HIPPOCAMPUS:
            // High D5, high 5-HT1A
            *profile = receptor_profile_cortical();  // Start with cortical
            profile->dopamine.config[DOPAMINE_D5].expression_level = 0.8F;
            profile->serotonin.config[SEROTONIN_5HT1A].expression_level = 0.9F;
            break;

        case BRAIN_REGION_THALAMUS:
            // Balanced receptor expression
            *profile = receptor_profile_cortical();
            for (int i = 0; i < DOPAMINE_RECEPTOR_COUNT; i++) {
                profile->dopamine.config[i].expression_level = 0.5F;
            }
            break;

        case BRAIN_REGION_GENERIC:
        default:
            // Generic balanced profile
            dopamine_receptor_system_init(&profile->dopamine);
            serotonin_receptor_system_init(&profile->serotonin);
            acetylcholine_receptor_system_init(&profile->acetylcholine);
            norepinephrine_receptor_system_init(&profile->norepinephrine);
            break;
    }
}
