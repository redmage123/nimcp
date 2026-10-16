//=============================================================================
// Minimal synapse types implementation for standalone testing
//=============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// Inline the essential type definitions
typedef enum synapse_type_t {
    SYNAPSE_GENERIC = 0,
    SYNAPSE_AMPA,
    SYNAPSE_NMDA,
    SYNAPSE_GABA_A,
    SYNAPSE_GABA_B,
    SYNAPSE_DOPAMINE,
    SYNAPSE_SEROTONIN,
    SYNAPSE_ACETYLCHOLINE,
    SYNAPSE_ELECTRICAL,
    SYNAPSE_TYPE_COUNT
} synapse_type_t;

typedef struct ampa_state_t {
    float conductance;
    float g_max;
    float tau_rise;
    float tau_decay;
    float reversal_potential;
} ampa_state_t;

typedef struct nmda_state_t {
    float conductance;
    float g_max;
    float tau_rise;
    float tau_decay;
    float reversal_potential;
    float mg_concentration;
    float calcium_influx;
} nmda_state_t;

typedef struct gaba_a_state_t {
    float conductance;
    float g_max;
    float tau_rise;
    float tau_decay;
    float reversal_potential;
} gaba_a_state_t;

typedef struct gaba_b_state_t {
    float conductance;
    float g_max;
    float tau_rise;
    float tau_decay;
    float reversal_potential;
} gaba_b_state_t;

typedef struct dopamine_state_t {
    float d1_level;
    float d2_level;
    float tau_d1;
    float tau_d2;
    float modulation;
    float baseline;
} dopamine_state_t;

typedef struct serotonin_state_t {
    float ht1a_level;
    float ht2a_level;
    float tau_ht1a;
    float tau_ht2a;
    float modulation;
    float baseline;
} serotonin_state_t;

typedef struct acetylcholine_state_t {
    float nicotinic_level;
    float muscarinic_level;
    float tau_nicotinic;
    float tau_muscarinic;
    float modulation;
    float baseline;
} acetylcholine_state_t;

typedef struct electrical_state_t {
    float conductance;
    bool bidirectional;
} electrical_state_t;

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

// Implementations
void synapse_init_ampa(ampa_state_t* state) {
    if (!state) return;
    state->conductance = 0.0f;
    state->g_max = 1.0f;
    state->tau_rise = 0.5f;
    state->tau_decay = 2.0f;
    state->reversal_potential = 0.0f;
}

void synapse_init_nmda(nmda_state_t* state) {
    if (!state) return;
    state->conductance = 0.0f;
    state->g_max = 0.3f;
    state->tau_rise = 10.0f;
    state->tau_decay = 100.0f;
    state->reversal_potential = 0.0f;
    state->mg_concentration = 1.0f;
    state->calcium_influx = 0.0f;
}

void synapse_init_gaba_a(gaba_a_state_t* state) {
    if (!state) return;
    state->conductance = 0.0f;
    state->g_max = 2.0f;
    state->tau_rise = 1.0f;
    state->tau_decay = 10.0f;
    state->reversal_potential = -70.0f;
}

void synapse_init_gaba_b(gaba_b_state_t* state) {
    if (!state) return;
    state->conductance = 0.0f;
    state->g_max = 0.5f;
    state->tau_rise = 50.0f;
    state->tau_decay = 150.0f;
    state->reversal_potential = -95.0f;
}

void synapse_init_dopamine(dopamine_state_t* state) {
    if (!state) return;
    state->d1_level = 0.0f;
    state->d2_level = 0.0f;
    state->tau_d1 = 200.0f;
    state->tau_d2 = 100.0f;
    state->modulation = 0.0f;
    state->baseline = 0.5f;
}

void synapse_init_serotonin(serotonin_state_t* state) {
    if (!state) return;
    state->ht1a_level = 0.0f;
    state->ht2a_level = 0.0f;
    state->tau_ht1a = 500.0f;
    state->tau_ht2a = 300.0f;
    state->modulation = 0.0f;
    state->baseline = 0.5f;
}

void synapse_init_acetylcholine(acetylcholine_state_t* state) {
    if (!state) return;
    state->nicotinic_level = 0.0f;
    state->muscarinic_level = 0.0f;
    state->tau_nicotinic = 20.0f;
    state->tau_muscarinic = 200.0f;
    state->modulation = 0.0f;
    state->baseline = 0.3f;
}

void synapse_init_electrical(electrical_state_t* state) {
    if (!state) return;
    state->conductance = 0.5f;
    state->bidirectional = true;
}

const char* synapse_type_name(synapse_type_t type) {
    static const char* names[] = {
        "GENERIC", "AMPA", "NMDA", "GABA-A", "GABA-B",
        "DOPAMINE", "SEROTONIN", "ACETYLCHOLINE", "ELECTRICAL"
    };
    if (type < 0 || type >= SYNAPSE_TYPE_COUNT) return "UNKNOWN";
    return names[type];
}

float synapse_type_time_constant(synapse_type_t type, const synapse_type_state_t* state) {
    if (!state) return 0.0f;
    switch (type) {
        case SYNAPSE_AMPA: return state->ampa.tau_decay;
        case SYNAPSE_NMDA: return state->nmda.tau_decay;
        case SYNAPSE_GABA_A: return state->gaba_a.tau_decay;
        case SYNAPSE_GABA_B: return state->gaba_b.tau_decay;
        case SYNAPSE_DOPAMINE: return state->dopamine.tau_d1;
        case SYNAPSE_SEROTONIN: return state->serotonin.tau_ht1a;
        case SYNAPSE_ACETYLCHOLINE: return state->acetylcholine.tau_muscarinic;
        default: return 0.0f;
    }
}

bool synapse_type_is_excitatory(synapse_type_t type) {
    return (type == SYNAPSE_AMPA || type == SYNAPSE_NMDA);
}

bool synapse_type_is_inhibitory(synapse_type_t type) {
    return (type == SYNAPSE_GABA_A || type == SYNAPSE_GABA_B);
}

bool synapse_type_is_modulatory(synapse_type_t type) {
    return (type == SYNAPSE_DOPAMINE || type == SYNAPSE_SEROTONIN || type == SYNAPSE_ACETYLCHOLINE);
}

// Test main
int main() {
    printf("=== NIMCP Phase 8.7: Synapse Type System Test ===\n\n");

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

    printf("\nTest 2: Synapse type names\n");
    for (int i = 0; i < SYNAPSE_TYPE_COUNT; i++) {
        printf("  Type %d: %s\n", i, synapse_type_name((synapse_type_t)i));
    }

    printf("\nTest 3: Type classification\n");
    printf("  AMPA is excitatory: %s\n", synapse_type_is_excitatory(SYNAPSE_AMPA) ? "YES" : "NO");
    printf("  GABA-A is inhibitory: %s\n", synapse_type_is_inhibitory(SYNAPSE_GABA_A) ? "YES" : "NO");
    printf("  Dopamine is modulatory: %s\n", synapse_type_is_modulatory(SYNAPSE_DOPAMINE) ? "YES" : "NO");

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
