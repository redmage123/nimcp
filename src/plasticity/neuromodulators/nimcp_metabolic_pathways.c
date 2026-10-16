/**
 * @file nimcp_metabolic_pathways.c
 * @brief Implementation of neurotransmitter metabolism (Phase C2.4 Enhancement #4)
 *
 * WHAT: Complete metabolic lifecycle - synthesis, degradation, reuptake
 * WHY:  Enables realistic pharmacology and homeostatic regulation
 * HOW:  Enzyme kinetics + Michaelis-Menten transporter dynamics
 *
 * @version Phase C2.4 Enhancement #4
 * @date 2025-11-13
 */

#include "plasticity/neuromodulators/nimcp_metabolic_pathways.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "security/nimcp_security.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>

#define LOG_MODULE "plasticity_metabolic_pathways"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(metabolic_pathways)

//=============================================================================
// HELPER FUNCTIONS
//=============================================================================

/**
 * @brief Exponential moving average update
 *
 * WHAT: Update moving average with new sample
 * WHY:  Track statistics without storing full history
 * HOW:  EMA formula: avg_new = α × sample + (1-α) × avg_old
 */
static inline float update_ema(float current_avg, float new_sample, float alpha) {
    return alpha * new_sample + (1.0F - alpha) * current_avg;
}

//=============================================================================
// LIFECYCLE FUNCTIONS
//=============================================================================

void metabolic_state_init(metabolic_state_t* state) {
    // WHAT: Initialize with dopamine defaults
    // WHY:  Provides sensible baseline for most common neurotransmitter
    // HOW:  Call init_with_config using dopamine parameters

    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_state_init: null state pointer");
        return;
    }

    metabolic_config_t config = metabolic_config_dopamine_default();
    metabolic_state_init_with_config(state, &config);
}

void metabolic_state_init_with_config(metabolic_state_t* state,
                                      const metabolic_config_t* config) {
    // WHAT: Initialize metabolic state with custom parameters
    // WHY:  Different neurotransmitters have different kinetics
    // HOW:  Set up synthesis, degradation, reuptake from config

    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_state_init_with_config: null state pointer");
        return;
    }
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_state_init_with_config: null config pointer");
        return;
    }

    memset(state, 0, sizeof(metabolic_state_t));

    // Initialize synthesis pathway
    state->synthesis.precursor_concentration = config->precursor_level;
    state->synthesis.precursor_influx_rate = 0.0F;  // No external influx by default
    state->synthesis.enzyme_activity = config->enzyme_activity;
    state->synthesis.cofactor_availability = 1.0F;  // Full cofactor availability
    state->synthesis.base_synthesis_rate = config->synthesis_rate;
    state->synthesis.current_synthesis_rate = config->synthesis_rate;
    state->synthesis.total_synthesized = 0;
    state->synthesis.avg_synthesis_rate = 0.0F;

    // Initialize degradation pathway
    state->degradation.enzyme_activity = config->enzyme_expression;
    state->degradation.degradation_rate = config->degradation_rate;
    state->degradation.inhibitor_blockade = 0.0F;  // No inhibition
    state->degradation.metabolite_concentration = 0.0F;
    state->degradation.total_degraded = 0;
    state->degradation.avg_degradation_rate = 0.0F;

    // Initialize reuptake transporter
    state->reuptake.km = config->reuptake_km;
    state->reuptake.vmax = config->reuptake_vmax;
    state->reuptake.transporter_density = config->transporter_density;
    state->reuptake.inhibitor_concentration = 0.0F;
    state->reuptake.inhibitor_ki = 0.0F;
    state->reuptake.is_reversed = false;
    state->reuptake.reversal_magnitude = 0.0F;
    state->reuptake.total_reuptake_events = 0;
    state->reuptake.avg_reuptake_rate = 0.0F;

    // Initialize concentrations
    state->concentration = 0.0F;
    state->vesicular_concentration = 0.0F;
    state->vesicular_uptake_rate = 0.001F;  // 1 µM/s typical VMAT rate
    state->last_update_time_us = 0;
}

void metabolic_state_reset(metabolic_state_t* state) {
    // WHAT: Reset to initial conditions while preserving configuration
    // WHY:  Allows reuse without full reinitialization
    // HOW:  Zero dynamic state, preserve kinetic parameters

    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_state_reset: null state pointer");
        return;
    }

    // Preserve configuration
    metabolic_config_t config = {
        .synthesis_rate = state->synthesis.base_synthesis_rate,
        .precursor_level = state->synthesis.precursor_concentration,
        .enzyme_activity = state->synthesis.enzyme_activity,
        .degradation_rate = state->degradation.degradation_rate,
        .enzyme_expression = state->degradation.enzyme_activity,
        .reuptake_km = state->reuptake.km,
        .reuptake_vmax = state->reuptake.vmax,
        .transporter_density = state->reuptake.transporter_density,
        .enable_synthesis = true,
        .enable_degradation = true,
        .enable_reuptake = true
    };

    // Reinitialize with preserved config
    metabolic_state_init_with_config(state, &config);
}

//=============================================================================
// DEFAULT CONFIGURATIONS
//=============================================================================

metabolic_config_t metabolic_config_dopamine_default(void) {
    // WHAT: Default dopamine metabolism parameters
    // WHY:  DA is prototypical catecholamine
    // HOW:  Literature values from Cooper et al. (2003)

    metabolic_config_t config = {
        .synthesis_rate = METABOLISM_SYNTHESIS_RATE_DOPAMINE,
        .precursor_level = METABOLISM_PRECURSOR_TYROSINE,
        .enzyme_activity = 1.0F,
        .degradation_rate = METABOLISM_DEGRADATION_RATE_MAO,
        .enzyme_expression = 1.0F,
        .reuptake_km = METABOLISM_REUPTAKE_KM_DAT,
        .reuptake_vmax = METABOLISM_REUPTAKE_VMAX_DAT,
        .transporter_density = 1.0F,
        .enable_synthesis = true,
        .enable_degradation = true,
        .enable_reuptake = true
    };
    return config;
}

metabolic_config_t metabolic_config_serotonin_default(void) {
    // WHAT: Default serotonin metabolism parameters
    // WHY:  5-HT has unique tryptophan precursor
    // HOW:  TPH-limited synthesis, MAO degradation, SERT reuptake

    metabolic_config_t config = {
        .synthesis_rate = METABOLISM_SYNTHESIS_RATE_SEROTONIN,
        .precursor_level = METABOLISM_PRECURSOR_TRYPTOPHAN,
        .enzyme_activity = 1.0F,
        .degradation_rate = METABOLISM_DEGRADATION_RATE_MAO,
        .enzyme_expression = 1.0F,
        .reuptake_km = METABOLISM_REUPTAKE_KM_SERT,
        .reuptake_vmax = METABOLISM_REUPTAKE_VMAX_SERT,
        .transporter_density = 1.0F,
        .enable_synthesis = true,
        .enable_degradation = true,
        .enable_reuptake = true
    };
    return config;
}

metabolic_config_t metabolic_config_norepinephrine_default(void) {
    // WHAT: Default norepinephrine metabolism parameters
    // WHY:  NE synthesized from DA via DBH
    // HOW:  DBH rate-limiting, MAO+COMT dual degradation, NET reuptake

    metabolic_config_t config = {
        .synthesis_rate = METABOLISM_SYNTHESIS_RATE_NOREPINEPHRINE,
        .precursor_level = METABOLISM_PRECURSOR_TYROSINE,  // Via DA intermediate
        .enzyme_activity = 1.0F,
        .degradation_rate = (METABOLISM_DEGRADATION_RATE_MAO + METABOLISM_DEGRADATION_RATE_COMT) / 2.0F,
        .enzyme_expression = 1.0F,
        .reuptake_km = METABOLISM_REUPTAKE_KM_NET,
        .reuptake_vmax = METABOLISM_REUPTAKE_VMAX_NET,
        .transporter_density = 1.0F,
        .enable_synthesis = true,
        .enable_degradation = true,
        .enable_reuptake = true
    };
    return config;
}

metabolic_config_t metabolic_config_acetylcholine_default(void) {
    // WHAT: Default acetylcholine metabolism parameters
    // WHY:  ACh has unique metabolism (very fast AChE degradation)
    // HOW:  ChAT synthesis, rapid AChE breakdown, ChT reuptake

    metabolic_config_t config = {
        .synthesis_rate = METABOLISM_SYNTHESIS_RATE_ACETYLCHOLINE,
        .precursor_level = METABOLISM_PRECURSOR_CHOLINE,
        .enzyme_activity = 1.0F,
        .degradation_rate = METABOLISM_DEGRADATION_RATE_ACHE,  // Very fast
        .enzyme_expression = 1.0F,
        .reuptake_km = METABOLISM_REUPTAKE_KM_CHT,
        .reuptake_vmax = METABOLISM_REUPTAKE_VMAX_CHT,
        .transporter_density = 1.0F,
        .enable_synthesis = true,
        .enable_degradation = true,
        .enable_reuptake = true
    };
    return config;
}

//=============================================================================
// SYNTHESIS PATHWAYS
//=============================================================================

float metabolic_synthesize(metabolic_state_t* state, float dt) {
    // WHAT: Compute neurotransmitter synthesis from precursor
    // WHY:  Synthesis maintains neurotransmitter stores
    // HOW:  Rate limited by enzyme activity and precursor availability

    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_synthesize: null state pointer");
        return 0.0F;
    }
    if (dt <= 0.0F) return 0.0F;

    synthesis_pathway_state_t* syn = &state->synthesis;

    // Check precursor availability (can't synthesize without substrate)
    if (syn->precursor_concentration <= 0.0F) {
        syn->current_synthesis_rate = 0.0F;
        return 0.0F;
    }

    // Compute effective synthesis rate
    // Rate = base_rate × enzyme_activity × cofactor × precursor_saturation
    float precursor_saturation = syn->precursor_concentration /
                                 (syn->precursor_concentration + 10.0F);  // Km ~10 µM

    syn->current_synthesis_rate = syn->base_synthesis_rate *
                                  syn->enzyme_activity *
                                  syn->cofactor_availability *
                                  precursor_saturation;

    // Synthesize amount based on time step
    float amount_synthesized = syn->current_synthesis_rate * dt;

    // Consume precursor (1:1 stoichiometry assumed)
    syn->precursor_concentration -= amount_synthesized;
    if (syn->precursor_concentration < 0.0F) {
        syn->precursor_concentration = 0.0F;
    }

    // Update statistics
    syn->total_synthesized++;
    syn->avg_synthesis_rate = update_ema(syn->avg_synthesis_rate,
                                         syn->current_synthesis_rate,
                                         0.1F);

    return amount_synthesized;
}

void metabolic_set_precursor(metabolic_state_t* state, float precursor_level) {
    // WHAT: Update precursor concentration
    // WHY:  Models dietary intake or blood-brain barrier transport
    // HOW:  Direct assignment with bounds check

    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_set_precursor: null state pointer");
        return;
    }

    state->synthesis.precursor_concentration = nimcp_clampf(precursor_level, 0.0F, 1000.0F);
}

void metabolic_set_enzyme_activity(metabolic_state_t* state, float activity) {
    // WHAT: Modulate enzyme expression/activity
    // WHY:  Models transcriptional regulation or feedback inhibition
    // HOW:  Normalized 0-1 activity level

    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_set_enzyme_activity: null state pointer");
        return;
    }

    state->synthesis.enzyme_activity = nimcp_clampf(activity, 0.0F, 2.0F);  // Allow up to 2x upregulation
}

//=============================================================================
// DEGRADATION PATHWAYS
//=============================================================================

float metabolic_degrade(metabolic_state_t* state, float dt) {
    // WHAT: Compute enzymatic degradation of neurotransmitter
    // WHY:  Clearance terminates neurotransmitter action
    // HOW:  First-order kinetics: dC/dt = -k × C

    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_degrade: null state pointer");
        return 0.0F;
    }
    if (dt <= 0.0F) return 0.0F;

    degradation_pathway_state_t* deg = &state->degradation;

    // No degradation if no neurotransmitter present
    if (state->concentration <= 0.0F) {
        return 0.0F;
    }

    // Effective degradation rate with inhibitor blockade
    float effective_rate = deg->degradation_rate *
                          deg->enzyme_activity *
                          (1.0F - deg->inhibitor_blockade);

    // First-order kinetics: amount = C × (1 - e^(-k×dt))
    float decay_factor = 1.0F - expf(-effective_rate * dt);
    float amount_degraded = state->concentration * decay_factor;

    // Update metabolite concentration
    deg->metabolite_concentration += amount_degraded;

    // Update statistics
    if (amount_degraded > 0.0F) {
        deg->total_degraded++;
    }
    deg->avg_degradation_rate = update_ema(deg->avg_degradation_rate,
                                           effective_rate,
                                           0.1F);

    return amount_degraded;
}

void metabolic_apply_mao_inhibitor(metabolic_state_t* state, float inhibition) {
    // WHAT: Block MAO enzyme activity
    // WHY:  Antidepressant mechanism (selegiline, phenelzine)
    // HOW:  Reduce degradation rate proportionally

    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_apply_mao_inhibitor: null state pointer");
        return;
    }

    state->degradation.inhibitor_blockade = nimcp_clampf(inhibition, 0.0F, 1.0F);
}

void metabolic_apply_comt_inhibitor(metabolic_state_t* state, float inhibition) {
    // WHAT: Block COMT enzyme activity
    // WHY:  Parkinson's treatment (tolcapone, entacapone)
    // HOW:  Reduce catecholamine degradation

    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_apply_comt_inhibitor: null state pointer");
        return;
    }

    // COMT inhibition reduces effective degradation rate
    // For simplicity, treat similarly to MAO inhibition
    state->degradation.inhibitor_blockade = nimcp_clampf(inhibition, 0.0F, 1.0F);
}

//=============================================================================
// REUPTAKE MECHANISMS
//=============================================================================

float metabolic_reuptake(metabolic_state_t* state, float concentration, float dt) {
    // WHAT: Compute transporter-mediated clearance
    // WHY:  Reuptake is primary inactivation mechanism (>80%)
    // HOW:  Michaelis-Menten kinetics with competitive inhibition

    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_reuptake: null state pointer");
        return 0.0F;
    }
    if (dt <= 0.0F || concentration <= 0.0F) return 0.0F;

    reuptake_transporter_state_t* rpt = &state->reuptake;

    // Handle transporter reversal (amphetamine)
    if (rpt->is_reversed) {
        // Reversed transporter causes efflux, not influx
        float efflux = rpt->reversal_magnitude * dt;
        return -efflux;  // Negative = increases cleft concentration
    }

    // Michaelis-Menten kinetics: v = Vmax × [S] / (Km + [S])
    // With competitive inhibition: Km_app = Km × (1 + [I]/Ki)
    float km_apparent = rpt->km;
    if (rpt->inhibitor_concentration > 0.0F && rpt->inhibitor_ki > 0.0F) {
        km_apparent *= (1.0F + rpt->inhibitor_concentration / rpt->inhibitor_ki);
    }

    // Compute reuptake rate
    float reuptake_rate = (rpt->vmax * rpt->transporter_density * concentration) /
                         (km_apparent + concentration);

    // Amount removed in time step
    float amount_removed = reuptake_rate * dt;

    // Can't remove more than available
    if (amount_removed > concentration) {
        amount_removed = concentration;
    }

    // Update statistics
    if (amount_removed > 0.0F) {
        rpt->total_reuptake_events++;
    }
    rpt->avg_reuptake_rate = update_ema(rpt->avg_reuptake_rate,
                                        reuptake_rate,
                                        0.1F);

    return amount_removed;
}

void metabolic_apply_reuptake_inhibitor(metabolic_state_t* state,
                                        float inhibitor_concentration,
                                        float inhibitor_ki) {
    // WHAT: Apply competitive reuptake inhibitor
    // WHY:  Models SSRIs, cocaine, other blockers
    // HOW:  Increase apparent Km via competitive inhibition

    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_apply_reuptake_inhibitor: null state pointer");
        return;
    }

    state->reuptake.inhibitor_concentration = nimcp_clampf(inhibitor_concentration, 0.0F, 100.0F);
    state->reuptake.inhibitor_ki = nimcp_clampf(inhibitor_ki, 0.001F, 10.0F);
}

void metabolic_reverse_transporter(metabolic_state_t* state, float magnitude) {
    // WHAT: Reverse transporter direction (efflux)
    // WHY:  Amphetamine mechanism - releases stored neurotransmitter
    // HOW:  Set reversal flag and magnitude

    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_reverse_transporter: null state pointer");
        return;
    }

    state->reuptake.is_reversed = (magnitude > 0.0F);
    state->reuptake.reversal_magnitude = nimcp_clampf(magnitude, 0.0F, 0.01F);  // Max 0.01 µM/s
}

//=============================================================================
// INTEGRATED UPDATE
//=============================================================================

float metabolic_update(metabolic_state_t* state, float dt, float release_amount) {
    // WHAT: Update complete metabolic state
    // WHY:  Integrates all pathways into unified dynamics
    // HOW:  dC/dt = synthesis + release - degradation - reuptake

    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_update: null state pointer");
        return 0.0F;
    }
    if (dt <= 0.0F) return state->concentration;

    // Synthesis: produces new neurotransmitter
    // Newly synthesized neurotransmitter goes into vesicular stores
    float synthesized = metabolic_synthesize(state, dt);
    state->vesicular_concentration += synthesized;

    // Vesicular uptake: cytoplasmic → vesicular
    // This removes from cytoplasm and stores in vesicles
    float vesicular_uptake = state->vesicular_uptake_rate * dt * state->concentration;
    vesicular_uptake = nimcp_clampf(vesicular_uptake, 0.0F, state->concentration);
    state->vesicular_concentration += vesicular_uptake;

    // Release: vesicular → synaptic cleft
    // This comes from vesicle pool, adds to cleft
    state->concentration += release_amount;

    // Reuptake: cleft → cytoplasm
    float reuptaken = metabolic_reuptake(state, state->concentration, dt);
    state->concentration -= reuptaken;

    // Degradation: enzymatic breakdown
    float degraded = metabolic_degrade(state, dt);
    state->concentration -= degraded;

    // Clamp concentration to valid range
    state->concentration = nimcp_clampf(state->concentration, 0.0F, 100.0F);
    state->vesicular_concentration = nimcp_clampf(state->vesicular_concentration, 0.0F, 1000.0F);

    return state->concentration;
}

//=============================================================================
// STATISTICS & MONITORING
//=============================================================================

void metabolic_get_synthesis_stats(const metabolic_state_t* state,
                                   uint64_t* total_synthesized,
                                   float* avg_rate) {
    // WHAT: Retrieve synthesis statistics
    // WHY:  Monitor neurotransmitter production
    // HOW:  Return counters and averages

    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_get_synthesis_stats: null state pointer");
        return;
    }

    if (total_synthesized) {
        *total_synthesized = state->synthesis.total_synthesized;
    }
    if (avg_rate) {
        *avg_rate = state->synthesis.avg_synthesis_rate;
    }
}

void metabolic_get_degradation_stats(const metabolic_state_t* state,
                                     uint64_t* total_degraded,
                                     float* avg_rate) {
    // WHAT: Retrieve degradation statistics
    // WHY:  Monitor enzymatic clearance
    // HOW:  Return counters and averages

    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_get_degradation_stats: null state pointer");
        return;
    }

    if (total_degraded) {
        *total_degraded = state->degradation.total_degraded;
    }
    if (avg_rate) {
        *avg_rate = state->degradation.avg_degradation_rate;
    }
}

void metabolic_get_reuptake_stats(const metabolic_state_t* state,
                                  uint64_t* total_events,
                                  float* avg_rate) {
    // WHAT: Retrieve reuptake statistics
    // WHY:  Monitor transporter activity
    // HOW:  Return counters and averages

    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_get_reuptake_stats: null state pointer");
        return;
    }

    if (total_events) {
        *total_events = state->reuptake.total_reuptake_events;
    }
    if (avg_rate) {
        *avg_rate = state->reuptake.avg_reuptake_rate;
    }
}
