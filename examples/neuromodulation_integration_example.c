/**
 * @file neuromodulation_integration_example.c
 * @brief Example demonstrating integration of phasic-tonic dynamics with receptor subtypes
 *
 * This example shows how Phase C2.2 Enhancements #1 and #2 work together:
 * - Phasic-tonic system generates dopamine concentrations
 * - Receptor subtypes process these concentrations into neural modulation
 * - TD error encoding drives burst/dip dynamics
 * - Receptor-specific effects (D1 vs D2) modulate learning
 *
 * SCENARIO: Reward learning task with unexpected reward delivery
 *
 * @version Phase C2.2
 * @date 2025-11-12
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include "plasticity/neuromodulators/nimcp_phasic_tonic.h"
#include "plasticity/neuromodulators/nimcp_receptor_subtypes.h"

// ============================================================================
// Helper Functions
// ============================================================================

static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static void print_state(
    const char* label,
    float td_error,
    const phasic_tonic_state_t* da_state,
    const dopamine_receptor_system_t* cortical_receptors,
    const dopamine_receptor_system_t* striatal_receptors
) {
    printf("\n=== %s ===\n", label);
    printf("TD Error: %.3f\n", td_error);
    printf("\nDopamine State:\n");
    float tonic = phasic_tonic_get_tonic_level(da_state);
    float phasic = phasic_tonic_get_phasic_burst(da_state);
    float total = phasic_tonic_get_total_concentration(da_state);
    printf("  Tonic:  %.6f µM (%6.1f nM)\n", tonic, tonic * 1000000.0f);
    printf("  Phasic: %.6f µM (%6.1f nM)\n", phasic, phasic * 1000000.0f);
    printf("  Total:  %.6f µM (%6.1f nM)\n", total, total * 1000000.0f);
    printf("  Burst State: %s\n", da_state->in_burst_state ? "ACTIVE" : "inactive");

    printf("\nCortical Receptors (D1-dominant):\n");
    printf("  Excitation (D1/D5): %.3f\n", cortical_receptors->total_excitation);
    printf("  Inhibition (D2/D3/D4): %.3f\n", cortical_receptors->total_inhibition);
    printf("  Net Modulation: %.3f\n", cortical_receptors->net_modulation);

    printf("\nStriatal Receptors (D2-dominant):\n");
    printf("  Excitation (D1/D5): %.3f\n", striatal_receptors->total_excitation);
    printf("  Inhibition (D2/D3/D4): %.3f\n", striatal_receptors->total_inhibition);
    printf("  Net Modulation: %.3f\n", striatal_receptors->net_modulation);
}

// ============================================================================
// Main Example
// ============================================================================

int main(void) {
    printf("==============================================================================\n");
    printf("Neuromodulation Integration Example: Phasic-Tonic + Receptor Subtypes\n");
    printf("Phase C2.2 Enhancements #1 & #2\n");
    printf("==============================================================================\n");

    // Initialize phasic-tonic dopamine system
    phasic_tonic_state_t dopamine_state;
    phasic_tonic_config_t da_config = phasic_tonic_config_dopamine_default();
    uint64_t current_time = get_time_us();
    phasic_tonic_init(&dopamine_state, &da_config, current_time);

    // Initialize receptor systems for two brain regions
    neuron_receptor_profile_t cortical_profile = receptor_profile_cortical();
    neuron_receptor_profile_t striatal_profile = receptor_profile_striatal();

    dopamine_receptor_system_t cortical_receptors = cortical_profile.dopamine;
    dopamine_receptor_system_t striatal_receptors = striatal_profile.dopamine;

    printf("\nInitialized Systems:\n");
    printf("  - Dopamine phasic-tonic system (VTA)\n");
    printf("  - Cortical receptors (D1-dominant, high excitation)\n");
    printf("  - Striatal receptors (D2-dominant, high inhibition)\n");

    // ========================================================================
    // SCENARIO 1: Baseline State (No Stimulation)
    // ========================================================================

    print_state("Baseline (No Stimulation)", 0.0f, &dopamine_state,
                &cortical_receptors, &striatal_receptors);

    // ========================================================================
    // SCENARIO 2: Unexpected Reward (Positive TD Error)
    // ========================================================================

    printf("\n\n");
    printf("==============================================================================\n");
    printf("EVENT: Unexpected Reward Delivered (TD Error = +0.8)\n");
    printf("==============================================================================\n");

    float td_error = 0.8f;  // Strong positive prediction error
    phasic_tonic_encode_td_error(&dopamine_state, td_error, current_time);

    // Update dopamine dynamics (1ms step)
    phasic_tonic_update(&dopamine_state, 0.001f, current_time);
    current_time += 1000;

    // Get new dopamine concentration
    float da_conc = phasic_tonic_get_concentration(&dopamine_state);

    // Update receptors with burst concentration
    dopamine_receptor_compute_modulation(&cortical_receptors, da_conc, 0.001f);
    dopamine_receptor_compute_modulation(&striatal_receptors, da_conc, 0.001f);

    print_state("1ms After Reward", td_error, &dopamine_state,
                &cortical_receptors, &striatal_receptors);

    printf("\nInterpretation:\n");
    printf("  → Dopamine burst (phasic) triggered by positive TD error\n");
    printf("  → Cortex: Strong D1/D5 activation → learning signal AMPLIFIED\n");
    printf("  → Striatum: D2 activation → action selection modulated\n");
    printf("  → Net effect: Reward-related synapses STRENGTHENED\n");

    // ========================================================================
    // SCENARIO 3: Burst Decay (50ms Later)
    // ========================================================================

    printf("\n\n");
    printf("==============================================================================\n");
    printf("TIMECOURSE: Burst Decay Over 50ms\n");
    printf("==============================================================================\n");

    printf("\nTime (ms) | Tonic (nM) | Phasic (nM) | Total (nM) | Cortex Net | Striatum Net\n");
    printf("----------|------------|-------------|------------|------------|-------------\n");

    for (int t = 0; t <= 50; t += 10) {
        phasic_tonic_update(&dopamine_state, 0.010f, current_time);
        current_time += 10000;  // 10ms

        da_conc = phasic_tonic_get_concentration(&dopamine_state);
        dopamine_receptor_compute_modulation(&cortical_receptors, da_conc, 0.010f);
        dopamine_receptor_compute_modulation(&striatal_receptors, da_conc, 0.010f);

        printf("%9d | %10.1f | %11.1f | %10.1f | %10.3f | %12.3f\n",
               t,
               phasic_tonic_get_tonic_level(&dopamine_state) * 1000000.0f,
               phasic_tonic_get_phasic_burst(&dopamine_state) * 1000000.0f,
               phasic_tonic_get_total_concentration(&dopamine_state) * 1000000.0f,
               cortical_receptors.net_modulation,
               striatal_receptors.net_modulation);
    }

    printf("\nInterpretation:\n");
    printf("  → Phasic burst decays exponentially (τ = 150ms)\n");
    printf("  → Tonic baseline remains stable\n");
    printf("  → Receptor modulation decreases as burst subsides\n");
    printf("  → Learning window closes after ~200ms\n");

    // ========================================================================
    // SCENARIO 4: Expected Outcome (Zero TD Error)
    // ========================================================================

    printf("\n\n");
    printf("==============================================================================\n");
    printf("EVENT: Expected Outcome (TD Error = 0.0)\n");
    printf("==============================================================================\n");

    // Let system return to baseline (simulate 1 second)
    for (int i = 0; i < 1000; i++) {
        phasic_tonic_update(&dopamine_state, 0.001f, current_time);
        current_time += 1000;
    }

    td_error = 0.0f;
    phasic_tonic_encode_td_error(&dopamine_state, td_error, current_time);
    phasic_tonic_update(&dopamine_state, 0.001f, current_time);

    da_conc = phasic_tonic_get_concentration(&dopamine_state);
    dopamine_receptor_compute_modulation(&cortical_receptors, da_conc, 0.001f);
    dopamine_receptor_compute_modulation(&striatal_receptors, da_conc, 0.001f);

    print_state("Expected Outcome", td_error, &dopamine_state,
                &cortical_receptors, &striatal_receptors);

    printf("\nInterpretation:\n");
    printf("  → No change in dopamine (as expected)\n");
    printf("  → Receptor modulation remains at tonic baseline\n");
    printf("  → No learning signal → synaptic weights MAINTAINED\n");

    // ========================================================================
    // SCENARIO 5: Worse Than Expected (Negative TD Error)
    // ========================================================================

    printf("\n\n");
    printf("==============================================================================\n");
    printf("EVENT: Reward Omission (TD Error = -0.6)\n");
    printf("==============================================================================\n");

    td_error = -0.6f;  // Negative prediction error
    phasic_tonic_encode_td_error(&dopamine_state, td_error, current_time);
    phasic_tonic_update(&dopamine_state, 0.001f, current_time);

    da_conc = phasic_tonic_get_concentration(&dopamine_state);
    dopamine_receptor_compute_modulation(&cortical_receptors, da_conc, 0.001f);
    dopamine_receptor_compute_modulation(&striatal_receptors, da_conc, 0.001f);

    print_state("Reward Omission", td_error, &dopamine_state,
                &cortical_receptors, &striatal_receptors);

    printf("\nInterpretation:\n");
    printf("  → Tonic dopamine DIP (no phasic burst)\n");
    printf("  → Reduced receptor activation\n");
    printf("  → Learning signal: SUPPRESS previous associations\n");
    printf("  → Action that led to omission will be avoided\n");

    // ========================================================================
    // SCENARIO 6: Antipsychotic Drug Effect (D2 Blockade)
    // ========================================================================

    printf("\n\n");
    printf("==============================================================================\n");
    printf("PHARMACOLOGY: Antipsychotic Drug (80%% D2 Blockade)\n");
    printf("==============================================================================\n");

    // Apply D2 blockade to striatal receptors
    dopamine_receptor_apply_d2_blockade(&striatal_receptors, 0.8f);

    // Trigger another reward
    td_error = 0.8f;
    phasic_tonic_encode_td_error(&dopamine_state, td_error, current_time);
    phasic_tonic_update(&dopamine_state, 0.001f, current_time);

    da_conc = phasic_tonic_get_concentration(&dopamine_state);
    dopamine_receptor_compute_modulation(&cortical_receptors, da_conc, 0.001f);
    dopamine_receptor_compute_modulation(&striatal_receptors, da_conc, 0.001f);

    print_state("With D2 Blockade", td_error, &dopamine_state,
                &cortical_receptors, &striatal_receptors);

    printf("\nInterpretation:\n");
    printf("  → Same dopamine burst as before\n");
    printf("  → Cortical D1 activation unchanged (no blockade)\n");
    printf("  → Striatal D2 inhibition REDUCED (80%% blocked)\n");
    printf("  → Net striatal modulation becomes more POSITIVE\n");
    printf("  → Clinical effect: Reduced negative symptoms, impaired reward learning\n");

    // ========================================================================
    // Summary Statistics
    // ========================================================================

    printf("\n\n");
    printf("==============================================================================\n");
    printf("SUMMARY STATISTICS\n");
    printf("==============================================================================\n");

    uint32_t burst_count;
    float avg_interval, time_since_last;
    phasic_tonic_get_burst_statistics(&dopamine_state, &burst_count,
                                      &avg_interval, &time_since_last,
                                      current_time);

    printf("\nDopamine System:\n");
    printf("  Total Bursts: %u\n", burst_count);
    printf("  Avg Inter-Burst Interval: %.3f s\n", avg_interval);
    printf("  Time Since Last Burst: %.3f s\n", time_since_last);

    printf("\nReceptor Expression (Striatum with D2 blockade):\n");
    printf("  D1: %.2f (baseline)\n", striatal_receptors.config[DOPAMINE_D1].expression_level);
    printf("  D2: %.2f (80%% blocked from 0.95)\n", striatal_receptors.config[DOPAMINE_D2].expression_level);
    printf("  D3: %.2f (56%% blocked from 0.60)\n", striatal_receptors.config[DOPAMINE_D3].expression_level);
    printf("  D4: %.2f (40%% blocked from 0.40)\n", striatal_receptors.config[DOPAMINE_D4].expression_level);

    printf("\n\n");
    printf("==============================================================================\n");
    printf("KEY INSIGHTS\n");
    printf("==============================================================================\n");
    printf("\n1. PHASIC-TONIC SEPARATION:\n");
    printf("   - Tonic dopamine (~50 nM) provides sustained motivation/mood\n");
    printf("   - Phasic bursts (~1000 nM peak) encode learning signals\n");
    printf("   - 20x concentration difference enables distinct functions\n");
    printf("\n2. TD ERROR ENCODING:\n");
    printf("   - Positive errors → bursts → strengthen associations\n");
    printf("   - Negative errors → dips → weaken associations\n");
    printf("   - Zero errors → no change → maintain current policy\n");
    printf("\n3. REGIONAL SPECIALIZATION:\n");
    printf("   - Cortex (D1-dominant): Amplifies reward signals for learning\n");
    printf("   - Striatum (D2-dominant): Balances action selection via inhibition\n");
    printf("   - Same dopamine burst → different effects via receptor expression\n");
    printf("\n4. PHARMACOLOGICAL INTERVENTIONS:\n");
    printf("   - D2 blockade (antipsychotics): Reduces inhibitory signaling\n");
    printf("   - Clinical trade-off: Reduces psychosis but impairs reward learning\n");
    printf("   - Regional receptor profiles determine drug effects\n");
    printf("\n5. TEMPORAL DYNAMICS:\n");
    printf("   - Burst onset: <1 ms (fast learning signal)\n");
    printf("   - Burst decay: ~150 ms (defines learning window)\n");
    printf("   - Homeostatic regulation: ~60 s (long-term stability)\n");
    printf("\n==============================================================================\n");

    return 0;
}
