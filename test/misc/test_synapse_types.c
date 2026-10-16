/**
 * @file test_synapse_types.c
 * @brief Simple test for Phase 8.7 synapse type system
 */

#include <stdio.h>
#include <stdlib.h>
#include "src/core/synapse_types/nimcp_synapse_types.h"

int main() {
    printf("=== NIMCP Phase 8.7: Synapse Type System Test ===\n\n");

    // Test 1: Initialize all synapse types
    printf("Test 1: Initialize all synapse types\n");

    ampa_state_t ampa;
    synapse_init_ampa(&ampa);
    printf("  AMPA: g_max=%.2f nS, tau_decay=%.2f ms\n", ampa.g_max, ampa.tau_decay);

    nmda_state_t nmda;
    synapse_init_nmda(&nmda);
    printf("  NMDA: g_max=%.2f nS, tau_decay=%.2f ms, [Mg2+]=%.2f mM\n",
           nmda.g_max, nmda.tau_decay, nmda.mg_concentration);

    gaba_a_state_t gaba_a;
    synapse_init_gaba_a(&gaba_a);
    printf("  GABA-A: g_max=%.2f nS, tau_decay=%.2f ms, E_rev=%.2f mV\n",
           gaba_a.g_max, gaba_a.tau_decay, gaba_a.reversal_potential);

    gaba_b_state_t gaba_b;
    synapse_init_gaba_b(&gaba_b);
    printf("  GABA-B: g_max=%.2f nS, tau_decay=%.2f ms, E_rev=%.2f mV\n",
           gaba_b.g_max, gaba_b.tau_decay, gaba_b.reversal_potential);

    dopamine_state_t dopamine;
    synapse_init_dopamine(&dopamine);
    printf("  Dopamine: tau_d1=%.2f ms, tau_d2=%.2f ms, baseline=%.2f\n",
           dopamine.tau_d1, dopamine.tau_d2, dopamine.baseline);

    serotonin_state_t serotonin;
    synapse_init_serotonin(&serotonin);
    printf("  Serotonin: tau_ht1a=%.2f ms, tau_ht2a=%.2f ms, baseline=%.2f\n",
           serotonin.tau_ht1a, serotonin.tau_ht2a, serotonin.baseline);

    acetylcholine_state_t ach;
    synapse_init_acetylcholine(&ach);
    printf("  Acetylcholine: tau_nicotinic=%.2f ms, tau_muscarinic=%.2f ms, baseline=%.2f\n",
           ach.tau_nicotinic, ach.tau_muscarinic, ach.baseline);

    electrical_state_t electrical;
    synapse_init_electrical(&electrical);
    printf("  Electrical: conductance=%.2f nS, bidirectional=%s\n",
           electrical.conductance, electrical.bidirectional ? "true" : "false");

    // Test 2: Type names
    printf("\nTest 2: Synapse type names\n");
    for (int i = 0; i < SYNAPSE_TYPE_COUNT; i++) {
        printf("  Type %d: %s\n", i, synapse_type_name((synapse_type_t)i));
    }

    // Test 3: Type classification
    printf("\nTest 3: Type classification\n");
    printf("  AMPA is excitatory: %s\n", synapse_type_is_excitatory(SYNAPSE_AMPA) ? "YES" : "NO");
    printf("  GABA-A is inhibitory: %s\n", synapse_type_is_inhibitory(SYNAPSE_GABA_A) ? "YES" : "NO");
    printf("  Dopamine is modulatory: %s\n", synapse_type_is_modulatory(SYNAPSE_DOPAMINE) ? "YES" : "NO");

    // Test 4: Time constants
    printf("\nTest 4: Time constants\n");
    synapse_type_state_t state;

    state.ampa = ampa;
    printf("  AMPA time constant: %.2f ms\n", synapse_type_time_constant(SYNAPSE_AMPA, &state));

    state.nmda = nmda;
    printf("  NMDA time constant: %.2f ms\n", synapse_type_time_constant(SYNAPSE_NMDA, &state));

    state.gaba_a = gaba_a;
    printf("  GABA-A time constant: %.2f ms\n", synapse_type_time_constant(SYNAPSE_GABA_A, &state));

    printf("\n=== All tests passed! ===\n");

    return 0;
}
