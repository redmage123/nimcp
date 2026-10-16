/**
 * @file nimcp_neuroscience_sim.h
 * @brief Neuroscience Simulation engine for world model
 *
 * WHAT: Simulates neural circuits with Wilson-Cowan population dynamics,
 *       synaptic plasticity (STDP), neurotransmitter kinetics, and cortical
 *       oscillations (alpha/beta/gamma/theta).
 * WHY:  Provides neuroscience prior for world model. Understanding brain
 *       circuits, learning mechanisms, and neural oscillations enables
 *       reasoning about cognition and behavior.
 * HOW:  Wilson-Cowan equations for E/I populations, STDP plasticity windows,
 *       neurotransmitter release-diffusion-binding-reuptake kinetics,
 *       coupled oscillator model for cortical rhythms.
 *
 * THEORETICAL FOUNDATION:
 *   - Wilson-Cowan: dE/dt = -E + S(w_EE*E - w_EI*I + I_ext)
 *   - STDP: dw = A+ * exp(-dt/tau+) for pre-before-post (LTP)
 *           dw = -A- * exp(dt/tau-) for post-before-pre (LTD)
 *   - Neurotransmitter: release -> diffusion -> binding -> reuptake
 *   - Oscillations: coupled Kuramoto model for phase synchrony
 */

#ifndef NIMCP_NEUROSCIENCE_SIM_H
#define NIMCP_NEUROSCIENCE_SIM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define NEUROSIM_MAX_POPULATIONS     32
#define NEUROSIM_MAX_CONNECTIONS     128
#define NEUROSIM_MAX_TRANSMITTERS    8
#define NEUROSIM_MAX_OSCILLATORS     8
#define NEUROSIM_MAX_NAME_LEN        32
#define NEUROSIM_EEG_BINS            128     /* Power spectrum bins */

/* STDP timing constants (ms) */
#define NEUROSIM_STDP_TAU_PLUS       20.0f   /* LTP time constant */
#define NEUROSIM_STDP_TAU_MINUS      20.0f   /* LTD time constant */
#define NEUROSIM_STDP_A_PLUS         0.01f   /* LTP amplitude */
#define NEUROSIM_STDP_A_MINUS        0.012f  /* LTD amplitude (slightly larger) */

/* Wilson-Cowan default parameters */
#define NEUROSIM_WC_TAU_E            10.0f   /* Excitatory time constant (ms) */
#define NEUROSIM_WC_TAU_I            20.0f   /* Inhibitory time constant (ms) */
#define NEUROSIM_WC_GAIN             1.0f    /* Sigmoid gain */
#define NEUROSIM_WC_THRESHOLD        2.0f    /* Sigmoid threshold */

/* Oscillation frequency bands (Hz) */
#define NEUROSIM_DELTA_LOW           0.5f
#define NEUROSIM_DELTA_HIGH          4.0f
#define NEUROSIM_THETA_LOW           4.0f
#define NEUROSIM_THETA_HIGH          8.0f
#define NEUROSIM_ALPHA_LOW           8.0f
#define NEUROSIM_ALPHA_HIGH          13.0f
#define NEUROSIM_BETA_LOW            13.0f
#define NEUROSIM_BETA_HIGH           30.0f
#define NEUROSIM_GAMMA_LOW           30.0f
#define NEUROSIM_GAMMA_HIGH          100.0f

/* Neurotransmitter constants */
#define NEUROSIM_VESICLE_RELEASE_P   0.3f    /* Release probability per AP */
#define NEUROSIM_SYNAPTIC_CLEFT_UM   0.02f   /* Synaptic cleft width (um) */
#define NEUROSIM_REUPTAKE_TAU_MS     5.0f    /* Reuptake time constant */

/* LTP/LTD thresholds */
#define NEUROSIM_LTP_THRESHOLD       0.5f    /* Post-synaptic depolarization for LTP */
#define NEUROSIM_LTD_THRESHOLD       0.2f    /* Moderate depolarization for LTD */
#define NEUROSIM_BCM_THETA_INIT      0.35f   /* Initial BCM sliding threshold */

/* ============================================================================
 * Enums
 * ============================================================================ */

typedef enum {
    NEUROSIM_POP_EXCITATORY = 0,    /* Glutamatergic pyramidal */
    NEUROSIM_POP_INHIBITORY,        /* GABAergic interneurons */
    NEUROSIM_POP_MODULATORY,        /* Dopaminergic/serotonergic */
    NEUROSIM_POP_TYPE_COUNT
} neurosim_pop_type_t;

typedef enum {
    NEUROSIM_NT_GLUTAMATE = 0,      /* Primary excitatory */
    NEUROSIM_NT_GABA,               /* Primary inhibitory */
    NEUROSIM_NT_DOPAMINE,           /* Reward/motivation */
    NEUROSIM_NT_SEROTONIN,          /* Mood/arousal */
    NEUROSIM_NT_ACETYLCHOLINE,      /* Attention/muscle */
    NEUROSIM_NT_NOREPINEPHRINE,     /* Arousal/alertness */
    NEUROSIM_NT_TYPE_COUNT
} neurosim_nt_type_t;

typedef enum {
    NEUROSIM_PLASTICITY_STDP = 0,       /* Spike-timing dependent */
    NEUROSIM_PLASTICITY_BCM,            /* Bienenstock-Cooper-Munro */
    NEUROSIM_PLASTICITY_HEBBIAN,        /* Classical Hebbian */
    NEUROSIM_PLASTICITY_HOMEOSTATIC,    /* Synaptic scaling */
    NEUROSIM_PLASTICITY_TYPE_COUNT
} neurosim_plasticity_type_t;

typedef enum {
    NEUROSIM_BAND_DELTA = 0,    /* 0.5-4 Hz: deep sleep */
    NEUROSIM_BAND_THETA,        /* 4-8 Hz: memory, navigation */
    NEUROSIM_BAND_ALPHA,        /* 8-13 Hz: relaxed wakefulness */
    NEUROSIM_BAND_BETA,         /* 13-30 Hz: active thinking */
    NEUROSIM_BAND_GAMMA,        /* 30-100 Hz: binding, attention */
    NEUROSIM_BAND_COUNT
} neurosim_freq_band_t;

/* ============================================================================
 * Structs
 * ============================================================================ */

/** Neural population (Wilson-Cowan unit) */
typedef struct {
    char                name[NEUROSIM_MAX_NAME_LEN];
    neurosim_pop_type_t type;
    float               activity;       /* [0..1] mean firing rate */
    float               tau;            /* time constant (ms) */
    float               gain;           /* sigmoid gain */
    float               threshold;      /* sigmoid threshold */
    float               external_input; /* I_ext */
    float               adaptation;     /* spike-frequency adaptation */
    uint32_t            neuron_count;   /* number of neurons in population */
    bool                active;
} neurosim_population_t;

/** Connection between populations */
typedef struct {
    uint32_t    src_pop;        /* source population index */
    uint32_t    tgt_pop;        /* target population index */
    float       weight;         /* connection strength */
    float       delay_ms;       /* axonal conduction delay */
    neurosim_plasticity_type_t plasticity;
    float       stdp_a_plus;    /* LTP amplitude */
    float       stdp_a_minus;   /* LTD amplitude */
    float       stdp_tau_plus;  /* LTP time constant */
    float       stdp_tau_minus; /* LTD time constant */
    float       bcm_theta;      /* BCM sliding threshold */
    bool        active;
} neurosim_connection_t;

/** Neurotransmitter state */
typedef struct {
    neurosim_nt_type_t  type;
    char                name[NEUROSIM_MAX_NAME_LEN];
    float               synaptic_conc;      /* concentration in cleft (uM) */
    float               release_rate;       /* vesicles/ms */
    float               reuptake_rate;      /* 1/ms */
    float               degradation_rate;   /* enzymatic breakdown 1/ms */
    float               receptor_occupancy; /* [0..1] */
    float               baseline_conc;      /* tonic level (uM) */
    /* Receptor types */
    float               receptor_density;   /* receptors/um^2 */
    float               receptor_kd;        /* dissociation constant (uM) */
} neurosim_transmitter_t;

/** Cortical oscillation band */
typedef struct {
    neurosim_freq_band_t band;
    float               frequency;      /* center frequency (Hz) */
    float               power;          /* spectral power (uV^2/Hz) */
    float               phase;          /* current phase (radians) */
    float               amplitude;      /* oscillation amplitude */
    float               coupling_strength; /* Kuramoto coupling */
} neurosim_oscillation_t;

/** EEG power spectrum */
typedef struct {
    float   power[NEUROSIM_EEG_BINS];       /* power at each frequency bin */
    float   freq_resolution;                 /* Hz per bin */
    float   dominant_frequency;              /* peak frequency */
    float   total_power;
    float   band_power[NEUROSIM_BAND_COUNT]; /* power per band */
} neurosim_eeg_spectrum_t;

/* ============================================================================
 * Config
 * ============================================================================ */

typedef struct {
    float   dt;                 /* time step in ms */
    float   temperature;        /* not used for neuro, kept for API compat */
    bool    enable_plasticity;
    bool    enable_oscillations;
    bool    enable_transmitter_kinetics;
    float   noise_amplitude;    /* background noise level */
    neurosim_plasticity_type_t default_plasticity;
} neuroscience_sim_config_t;

/* ============================================================================
 * Stats
 * ============================================================================ */

typedef struct {
    uint64_t    step_count;
    uint32_t    num_populations;
    uint32_t    num_connections;
    float       mean_excitatory_rate;
    float       mean_inhibitory_rate;
    float       ei_balance;             /* E/I ratio */
    float       total_synaptic_weight;
    float       dominant_frequency;
    float       synchrony_index;        /* phase coherence [0..1] */
} neuroscience_sim_stats_t;

/* ============================================================================
 * Engine
 * ============================================================================ */

typedef struct neuroscience_sim_sim {
    neurosim_population_t   populations[NEUROSIM_MAX_POPULATIONS];
    uint32_t                num_populations;
    neurosim_connection_t   connections[NEUROSIM_MAX_CONNECTIONS];
    uint32_t                num_connections;
    neurosim_transmitter_t  transmitters[NEUROSIM_MAX_TRANSMITTERS];
    uint32_t                num_transmitters;
    neurosim_oscillation_t  oscillations[NEUROSIM_MAX_OSCILLATORS];
    uint32_t                num_oscillations;
    neurosim_eeg_spectrum_t eeg;
    neuroscience_sim_config_t config;
    neuroscience_sim_stats_t stats;
    float                   sim_time_ms;
    bool                    initialized;
} neuroscience_sim_sim_t;

/* ============================================================================
 * API
 * ============================================================================ */

neuroscience_sim_sim_t* neuroscience_sim_create(const neuroscience_sim_config_t* config);
void neuroscience_sim_destroy(neuroscience_sim_sim_t* sim);
int neuroscience_sim_step(neuroscience_sim_sim_t* sim, float dt);
neuroscience_sim_config_t neuroscience_sim_default_config(void);
neuroscience_sim_stats_t neuroscience_sim_get_stats(const neuroscience_sim_sim_t* sim);

/** Population management */
int neuroscience_sim_add_population(neuroscience_sim_sim_t* sim,
                                     const neurosim_population_t* pop);
int neuroscience_sim_add_connection(neuroscience_sim_sim_t* sim,
                                     const neurosim_connection_t* conn);

/** Wilson-Cowan dynamics */
float neuroscience_sim_sigmoid(float x, float gain, float threshold);
int neuroscience_sim_step_wilson_cowan(neuroscience_sim_sim_t* sim, float dt);

/** Plasticity */
float neuroscience_sim_stdp_window(float delta_t, float a_plus, float a_minus,
                                    float tau_plus, float tau_minus);
int neuroscience_sim_step_plasticity(neuroscience_sim_sim_t* sim, float dt);

/** Neurotransmitter kinetics */
int neuroscience_sim_add_transmitter(neuroscience_sim_sim_t* sim,
                                      const neurosim_transmitter_t* nt);
int neuroscience_sim_step_transmitters(neuroscience_sim_sim_t* sim, float dt);

/** Oscillations */
int neuroscience_sim_step_oscillations(neuroscience_sim_sim_t* sim, float dt);
int neuroscience_sim_compute_eeg(neuroscience_sim_sim_t* sim);

/** Load preset: cortical column model */
void neuroscience_sim_load_cortical_column(neuroscience_sim_sim_t* sim);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEUROSCIENCE_SIM_H */
