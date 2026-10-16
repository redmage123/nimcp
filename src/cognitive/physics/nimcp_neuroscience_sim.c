/**
 * @file nimcp_neuroscience_sim.c
 * @brief Neuroscience Simulation engine -- Wilson-Cowan dynamics, STDP plasticity,
 *        neurotransmitter kinetics, cortical oscillations, EEG power spectrum
 *
 * WHAT: Simulates neural circuits with real neuroscience equations.
 * WHY:  Neuroscience prior for world model reasoning about brain function.
 * HOW:  Wilson-Cowan E/I population dynamics, STDP plasticity windows,
 *       neurotransmitter release-diffusion-binding-reuptake, Kuramoto oscillators.
 */

#include "cognitive/physics/nimcp_neuroscience_sim.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "NEUROSCI_SIM"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * Helpers
 * ============================================================================ */

static inline float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* ============================================================================
 * Create / Destroy / Config
 * ============================================================================ */

neuroscience_sim_config_t neuroscience_sim_default_config(void) {
    neuroscience_sim_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.dt = 0.5f;                  /* 0.5 ms time step */
    cfg.temperature = 37.0f;
    cfg.enable_plasticity = true;
    cfg.enable_oscillations = true;
    cfg.enable_transmitter_kinetics = true;
    cfg.noise_amplitude = 0.05f;
    cfg.default_plasticity = NEUROSIM_PLASTICITY_STDP;
    return cfg;
}

neuroscience_sim_sim_t* neuroscience_sim_create(const neuroscience_sim_config_t* config) {
    neuroscience_sim_sim_t* sim = (neuroscience_sim_sim_t*)nimcp_calloc(
        1, sizeof(neuroscience_sim_sim_t));
    if (!sim) {
        LOG_ERROR(LOG_TAG, "Failed to allocate neuroscience sim");
        return NULL;
    }
    sim->config = config ? *config : neuroscience_sim_default_config();
    sim->initialized = true;
    LOG_INFO(LOG_TAG, "Neuroscience sim created (dt=%.2fms)", sim->config.dt);
    return sim;
}

void neuroscience_sim_destroy(neuroscience_sim_sim_t* sim) {
    if (!sim) return;
    LOG_INFO(LOG_TAG, "Neuroscience sim destroyed after %lu steps", sim->stats.step_count);
    nimcp_free(sim);
}

neuroscience_sim_stats_t neuroscience_sim_get_stats(const neuroscience_sim_sim_t* sim) {
    neuroscience_sim_stats_t empty;
    memset(&empty, 0, sizeof(empty));
    return sim ? sim->stats : empty;
}

/* ============================================================================
 * Population / Connection Management
 * ============================================================================ */

int neuroscience_sim_add_population(neuroscience_sim_sim_t* sim,
                                     const neurosim_population_t* pop) {
    if (!sim || !pop) return -1;
    if (sim->num_populations >= NEUROSIM_MAX_POPULATIONS) return -1;
    sim->populations[sim->num_populations] = *pop;
    sim->num_populations++;
    return 0;
}

int neuroscience_sim_add_connection(neuroscience_sim_sim_t* sim,
                                     const neurosim_connection_t* conn) {
    if (!sim || !conn) return -1;
    if (sim->num_connections >= NEUROSIM_MAX_CONNECTIONS) return -1;
    sim->connections[sim->num_connections] = *conn;
    sim->num_connections++;
    return 0;
}

int neuroscience_sim_add_transmitter(neuroscience_sim_sim_t* sim,
                                      const neurosim_transmitter_t* nt) {
    if (!sim || !nt) return -1;
    if (sim->num_transmitters >= NEUROSIM_MAX_TRANSMITTERS) return -1;
    sim->transmitters[sim->num_transmitters] = *nt;
    sim->num_transmitters++;
    return 0;
}

/* ============================================================================
 * Wilson-Cowan Dynamics
 * ============================================================================ */

/**
 * Sigmoid activation function: S(x) = 1 / (1 + exp(-gain * (x - threshold)))
 */
float neuroscience_sim_sigmoid(float x, float gain, float threshold) {
    float arg = -gain * (x - threshold);
    arg = clampf(arg, -20.0f, 20.0f); /* prevent overflow */
    return 1.0f / (1.0f + expf(arg));
}

/**
 * Wilson-Cowan population dynamics:
 *   dE/dt = (-E + S(w_EE * E - w_EI * I + I_ext)) / tau_E
 *   dI/dt = (-I + S(w_IE * E - w_II * I + I_inh)) / tau_I
 *
 * Implemented via forward Euler on each population.
 */
int neuroscience_sim_step_wilson_cowan(neuroscience_sim_sim_t* sim, float dt) {
    if (!sim) return -1;

    /* Compute total synaptic input for each population */
    float inputs[NEUROSIM_MAX_POPULATIONS];
    memset(inputs, 0, sizeof(inputs));

    for (uint32_t c = 0; c < sim->num_connections; c++) {
        neurosim_connection_t* conn = &sim->connections[c];
        if (!conn->active) continue;
        if (conn->src_pop >= sim->num_populations ||
            conn->tgt_pop >= sim->num_populations) continue;

        /* Input = weight * source_activity */
        inputs[conn->tgt_pop] += conn->weight * sim->populations[conn->src_pop].activity;
    }

    /* Update each population */
    for (uint32_t i = 0; i < sim->num_populations; i++) {
        neurosim_population_t* pop = &sim->populations[i];
        if (!pop->active) continue;

        float total_input = inputs[i] + pop->external_input;

        /* Add noise */
        /* Simple hash-based noise for reproducibility */
        uint32_t noise_seed = (uint32_t)(sim->sim_time_ms * 100.0f) + i * 7919;
        noise_seed = noise_seed * 1664525u + 1013904223u;
        float noise = ((float)(noise_seed & 0xFFFF) / 32768.0f - 1.0f) *
                       sim->config.noise_amplitude;
        total_input += noise;

        /* Wilson-Cowan: dA/dt = (-A + S(input)) / tau */
        float sigmoid_out = neuroscience_sim_sigmoid(total_input, pop->gain, pop->threshold);

        /* Spike-frequency adaptation */
        sigmoid_out -= pop->adaptation;

        float dA = (-pop->activity + sigmoid_out) / pop->tau;
        pop->activity += dA * dt;
        pop->activity = clampf(pop->activity, 0.0f, 1.0f);

        /* Adaptation dynamics */
        pop->adaptation += (pop->activity * 0.1f - pop->adaptation * 0.05f) * dt;
        pop->adaptation = clampf(pop->adaptation, 0.0f, 0.5f);
    }

    return 0;
}

/* ============================================================================
 * STDP Plasticity
 * ============================================================================ */

/**
 * STDP learning window:
 *   delta_t > 0 (pre before post): dw = A+ * exp(-delta_t / tau+) [LTP]
 *   delta_t < 0 (post before pre): dw = -A- * exp(delta_t / tau-) [LTD]
 */
float neuroscience_sim_stdp_window(float delta_t, float a_plus, float a_minus,
                                    float tau_plus, float tau_minus) {
    if (delta_t > 0.0f) {
        /* Pre-before-post: LTP */
        return a_plus * expf(-delta_t / tau_plus);
    } else if (delta_t < 0.0f) {
        /* Post-before-pre: LTD */
        return -a_minus * expf(delta_t / tau_minus);
    }
    return 0.0f;
}

int neuroscience_sim_step_plasticity(neuroscience_sim_sim_t* sim, float dt) {
    if (!sim || !sim->config.enable_plasticity) return 0;

    for (uint32_t c = 0; c < sim->num_connections; c++) {
        neurosim_connection_t* conn = &sim->connections[c];
        if (!conn->active) continue;
        if (conn->src_pop >= sim->num_populations ||
            conn->tgt_pop >= sim->num_populations) continue;

        float src_act = sim->populations[conn->src_pop].activity;
        float tgt_act = sim->populations[conn->tgt_pop].activity;

        switch (conn->plasticity) {
            case NEUROSIM_PLASTICITY_STDP: {
                /* Use activity correlation as proxy for spike timing */
                /* Pre-post correlation: positive if source precedes target */
                float correlation = src_act * tgt_act;
                float anti_corr = (1.0f - src_act) * tgt_act;

                float dw = conn->stdp_a_plus * correlation -
                           conn->stdp_a_minus * anti_corr;
                conn->weight += dw * dt * 0.01f;
                break;
            }
            case NEUROSIM_PLASTICITY_BCM: {
                /* BCM rule: dw = eta * x * y * (y - theta_m) */
                /* theta_m slides based on postsynaptic activity */
                float y = tgt_act;
                float x = src_act;
                float dw = x * y * (y - conn->bcm_theta) * 0.001f * dt;
                conn->weight += dw;

                /* Sliding threshold: theta tracks <y^2> */
                conn->bcm_theta += (y * y - conn->bcm_theta) * 0.01f * dt;
                conn->bcm_theta = clampf(conn->bcm_theta, 0.01f, 0.9f);
                break;
            }
            case NEUROSIM_PLASTICITY_HEBBIAN: {
                /* Simple Hebbian: dw = eta * pre * post */
                float dw = src_act * tgt_act * 0.001f * dt;
                conn->weight += dw;
                break;
            }
            case NEUROSIM_PLASTICITY_HOMEOSTATIC: {
                /* Synaptic scaling: maintain target firing rate */
                float target_rate = 0.3f;
                float scale = target_rate / (tgt_act + 1e-6f);
                scale = clampf(scale, 0.9f, 1.1f);
                conn->weight *= (1.0f + (scale - 1.0f) * 0.001f * dt);
                break;
            }
            default:
                break;
        }

        /* Weight bounds */
        conn->weight = clampf(conn->weight, -10.0f, 10.0f);
    }

    return 0;
}

/* ============================================================================
 * Neurotransmitter Kinetics
 * ============================================================================ */

int neuroscience_sim_step_transmitters(neuroscience_sim_sim_t* sim, float dt) {
    if (!sim || !sim->config.enable_transmitter_kinetics) return 0;

    for (uint32_t i = 0; i < sim->num_transmitters; i++) {
        neurosim_transmitter_t* nt = &sim->transmitters[i];

        /* Find source population activity for this transmitter */
        float source_activity = 0.0f;
        for (uint32_t p = 0; p < sim->num_populations; p++) {
            neurosim_population_t* pop = &sim->populations[p];
            if (!pop->active) continue;
            /* Excitatory populations release glutamate, inhibitory release GABA, etc. */
            if ((pop->type == NEUROSIM_POP_EXCITATORY && nt->type == NEUROSIM_NT_GLUTAMATE) ||
                (pop->type == NEUROSIM_POP_INHIBITORY && nt->type == NEUROSIM_NT_GABA) ||
                (pop->type == NEUROSIM_POP_MODULATORY &&
                 (nt->type == NEUROSIM_NT_DOPAMINE || nt->type == NEUROSIM_NT_SEROTONIN))) {
                source_activity += pop->activity;
            }
        }

        /* Release: proportional to presynaptic activity * release probability */
        float release = source_activity * nt->release_rate * NEUROSIM_VESICLE_RELEASE_P * dt;

        /* Reuptake: first-order kinetics, time constant tau */
        float reuptake = nt->reuptake_rate * nt->synaptic_conc * dt;

        /* Enzymatic degradation */
        float degraded = nt->degradation_rate * nt->synaptic_conc * dt;

        /* Update concentration: dC/dt = release - reuptake - degradation */
        nt->synaptic_conc += release - reuptake - degraded;
        nt->synaptic_conc = clampf(nt->synaptic_conc, 0.0f, 1000.0f);

        /* Receptor occupancy: Michaelis-Menten binding */
        /* Occupancy = [NT] / (Kd + [NT]) */
        nt->receptor_occupancy = nt->synaptic_conc /
                                 (nt->receptor_kd + nt->synaptic_conc);

        /* Tonic baseline recovery */
        float baseline_pull = (nt->baseline_conc - nt->synaptic_conc) * 0.001f * dt;
        nt->synaptic_conc += baseline_pull;
    }

    return 0;
}

/* ============================================================================
 * Cortical Oscillations (Kuramoto model)
 * ============================================================================ */

int neuroscience_sim_step_oscillations(neuroscience_sim_sim_t* sim, float dt) {
    if (!sim || !sim->config.enable_oscillations) return 0;

    float dt_sec = dt * 0.001f; /* ms to seconds */

    /* Compute mean field (order parameter) for Kuramoto coupling */
    float mean_phase_x = 0.0f, mean_phase_y = 0.0f;
    for (uint32_t i = 0; i < sim->num_oscillations; i++) {
        mean_phase_x += cosf(sim->oscillations[i].phase);
        mean_phase_y += sinf(sim->oscillations[i].phase);
    }
    if (sim->num_oscillations > 0) {
        mean_phase_x /= sim->num_oscillations;
        mean_phase_y /= sim->num_oscillations;
    }
    float mean_phase = atan2f(mean_phase_y, mean_phase_x);
    float sync_r = sqrtf(mean_phase_x * mean_phase_x + mean_phase_y * mean_phase_y);

    /* Update each oscillator */
    for (uint32_t i = 0; i < sim->num_oscillations; i++) {
        neurosim_oscillation_t* osc = &sim->oscillations[i];

        /* Natural frequency */
        float omega = 2.0f * (float)M_PI * osc->frequency;

        /* Kuramoto coupling: d(theta)/dt = omega + K * r * sin(mean_phase - theta) */
        float coupling = osc->coupling_strength * sync_r *
                          sinf(mean_phase - osc->phase);

        /* Neural activity modulates oscillation amplitude */
        float neural_drive = 0.0f;
        for (uint32_t p = 0; p < sim->num_populations; p++) {
            neural_drive += sim->populations[p].activity;
        }
        if (sim->num_populations > 0) neural_drive /= sim->num_populations;

        /* Phase advance */
        osc->phase += (omega + coupling) * dt_sec;

        /* Wrap phase to [0, 2*pi] */
        while (osc->phase > 2.0f * (float)M_PI) osc->phase -= 2.0f * (float)M_PI;
        while (osc->phase < 0.0f) osc->phase += 2.0f * (float)M_PI;

        /* Amplitude modulation by neural activity */
        osc->amplitude = osc->amplitude * 0.99f + neural_drive * 0.01f;
        osc->amplitude = clampf(osc->amplitude, 0.01f, 100.0f);

        /* Power = amplitude^2 */
        osc->power = osc->amplitude * osc->amplitude;
    }

    return 0;
}

/* ============================================================================
 * EEG Power Spectrum
 * ============================================================================ */

int neuroscience_sim_compute_eeg(neuroscience_sim_sim_t* sim) {
    if (!sim) return -1;

    memset(&sim->eeg, 0, sizeof(sim->eeg));
    sim->eeg.freq_resolution = 1.0f; /* 1 Hz per bin */

    /* Compute power spectrum from oscillators */
    float max_power = 0.0f;
    float max_freq = 0.0f;

    for (uint32_t i = 0; i < sim->num_oscillations; i++) {
        neurosim_oscillation_t* osc = &sim->oscillations[i];
        float power = osc->power;

        /* Place power in appropriate frequency bin */
        uint32_t bin = (uint32_t)(osc->frequency / sim->eeg.freq_resolution);
        if (bin < NEUROSIM_EEG_BINS) {
            /* Spread power across nearby bins (Gaussian envelope) */
            for (int b = -3; b <= 3; b++) {
                int idx = (int)bin + b;
                if (idx >= 0 && idx < NEUROSIM_EEG_BINS) {
                    float spread = expf(-0.5f * (float)(b * b));
                    sim->eeg.power[idx] += power * spread;
                }
            }
        }

        if (power > max_power) {
            max_power = power;
            max_freq = osc->frequency;
        }

        /* Accumulate into frequency bands */
        if (osc->frequency >= NEUROSIM_DELTA_LOW && osc->frequency < NEUROSIM_DELTA_HIGH)
            sim->eeg.band_power[NEUROSIM_BAND_DELTA] += power;
        else if (osc->frequency >= NEUROSIM_THETA_LOW && osc->frequency < NEUROSIM_THETA_HIGH)
            sim->eeg.band_power[NEUROSIM_BAND_THETA] += power;
        else if (osc->frequency >= NEUROSIM_ALPHA_LOW && osc->frequency < NEUROSIM_ALPHA_HIGH)
            sim->eeg.band_power[NEUROSIM_BAND_ALPHA] += power;
        else if (osc->frequency >= NEUROSIM_BETA_LOW && osc->frequency < NEUROSIM_BETA_HIGH)
            sim->eeg.band_power[NEUROSIM_BAND_BETA] += power;
        else if (osc->frequency >= NEUROSIM_GAMMA_LOW && osc->frequency < NEUROSIM_GAMMA_HIGH)
            sim->eeg.band_power[NEUROSIM_BAND_GAMMA] += power;
    }

    sim->eeg.dominant_frequency = max_freq;
    sim->eeg.total_power = 0.0f;
    for (uint32_t i = 0; i < NEUROSIM_EEG_BINS; i++) {
        sim->eeg.total_power += sim->eeg.power[i];
    }

    return 0;
}

/* ============================================================================
 * Main Step
 * ============================================================================ */

int neuroscience_sim_step(neuroscience_sim_sim_t* sim, float dt) {
    if (!sim || !sim->initialized) return -1;
    if (dt <= 0.0f) dt = sim->config.dt;

    /* Wilson-Cowan population dynamics */
    neuroscience_sim_step_wilson_cowan(sim, dt);

    /* Synaptic plasticity */
    if (sim->config.enable_plasticity) {
        neuroscience_sim_step_plasticity(sim, dt);
    }

    /* Neurotransmitter kinetics */
    if (sim->config.enable_transmitter_kinetics) {
        neuroscience_sim_step_transmitters(sim, dt);
    }

    /* Cortical oscillations */
    if (sim->config.enable_oscillations) {
        neuroscience_sim_step_oscillations(sim, dt);
        neuroscience_sim_compute_eeg(sim);
    }

    sim->sim_time_ms += dt;

    /* Update stats */
    sim->stats.step_count++;
    sim->stats.num_populations = sim->num_populations;
    sim->stats.num_connections = sim->num_connections;

    float e_sum = 0.0f, i_sum = 0.0f;
    uint32_t e_count = 0, i_count = 0;
    float total_w = 0.0f;

    for (uint32_t p = 0; p < sim->num_populations; p++) {
        if (sim->populations[p].type == NEUROSIM_POP_EXCITATORY) {
            e_sum += sim->populations[p].activity;
            e_count++;
        } else if (sim->populations[p].type == NEUROSIM_POP_INHIBITORY) {
            i_sum += sim->populations[p].activity;
            i_count++;
        }
    }

    sim->stats.mean_excitatory_rate = e_count > 0 ? e_sum / e_count : 0.0f;
    sim->stats.mean_inhibitory_rate = i_count > 0 ? i_sum / i_count : 0.0f;
    sim->stats.ei_balance = (sim->stats.mean_inhibitory_rate > 1e-6f) ?
        sim->stats.mean_excitatory_rate / sim->stats.mean_inhibitory_rate : 0.0f;

    for (uint32_t c = 0; c < sim->num_connections; c++) {
        total_w += fabsf(sim->connections[c].weight);
    }
    sim->stats.total_synaptic_weight = total_w;
    sim->stats.dominant_frequency = sim->eeg.dominant_frequency;

    /* Synchrony index from Kuramoto order parameter */
    float mx = 0.0f, my = 0.0f;
    for (uint32_t o = 0; o < sim->num_oscillations; o++) {
        mx += cosf(sim->oscillations[o].phase);
        my += sinf(sim->oscillations[o].phase);
    }
    if (sim->num_oscillations > 0) {
        mx /= sim->num_oscillations;
        my /= sim->num_oscillations;
        sim->stats.synchrony_index = sqrtf(mx * mx + my * my);
    }

    return 0;
}

/* ============================================================================
 * Preset: Cortical Column Model
 * ============================================================================ */

void neuroscience_sim_load_cortical_column(neuroscience_sim_sim_t* sim) {
    if (!sim) return;

    sim->num_populations = 0;
    sim->num_connections = 0;
    sim->num_transmitters = 0;
    sim->num_oscillations = 0;

    /* Layer 2/3 Excitatory pyramidal population */
    neurosim_population_t l23e = {0};
    strncpy(l23e.name, "L2/3-Exc", NEUROSIM_MAX_NAME_LEN - 1);
    l23e.type = NEUROSIM_POP_EXCITATORY;
    l23e.activity = 0.1f;
    l23e.tau = NEUROSIM_WC_TAU_E;
    l23e.gain = NEUROSIM_WC_GAIN;
    l23e.threshold = NEUROSIM_WC_THRESHOLD;
    l23e.external_input = 0.5f;
    l23e.neuron_count = 2000;
    l23e.active = true;
    neuroscience_sim_add_population(sim, &l23e);

    /* Layer 2/3 Inhibitory interneuron population */
    neurosim_population_t l23i = {0};
    strncpy(l23i.name, "L2/3-Inh", NEUROSIM_MAX_NAME_LEN - 1);
    l23i.type = NEUROSIM_POP_INHIBITORY;
    l23i.activity = 0.05f;
    l23i.tau = NEUROSIM_WC_TAU_I;
    l23i.gain = NEUROSIM_WC_GAIN;
    l23i.threshold = NEUROSIM_WC_THRESHOLD;
    l23i.neuron_count = 500;
    l23i.active = true;
    neuroscience_sim_add_population(sim, &l23i);

    /* Layer 4 Excitatory (thalamic input) */
    neurosim_population_t l4e = {0};
    strncpy(l4e.name, "L4-Exc", NEUROSIM_MAX_NAME_LEN - 1);
    l4e.type = NEUROSIM_POP_EXCITATORY;
    l4e.activity = 0.15f;
    l4e.tau = NEUROSIM_WC_TAU_E;
    l4e.gain = 1.2f;
    l4e.threshold = 1.8f;
    l4e.external_input = 1.0f; /* Strong thalamic drive */
    l4e.neuron_count = 3000;
    l4e.active = true;
    neuroscience_sim_add_population(sim, &l4e);

    /* Layer 5 Excitatory (output to subcortical) */
    neurosim_population_t l5e = {0};
    strncpy(l5e.name, "L5-Exc", NEUROSIM_MAX_NAME_LEN - 1);
    l5e.type = NEUROSIM_POP_EXCITATORY;
    l5e.activity = 0.08f;
    l5e.tau = 15.0f;
    l5e.gain = 0.8f;
    l5e.threshold = 2.5f;
    l5e.neuron_count = 1500;
    l5e.active = true;
    neuroscience_sim_add_population(sim, &l5e);

    /* Connections: E->E (recurrent), E->I, I->E (feedback inhibition) */
    neurosim_connection_t conn;
    memset(&conn, 0, sizeof(conn));
    conn.active = true;
    conn.plasticity = NEUROSIM_PLASTICITY_STDP;
    conn.stdp_a_plus = NEUROSIM_STDP_A_PLUS;
    conn.stdp_a_minus = NEUROSIM_STDP_A_MINUS;
    conn.stdp_tau_plus = NEUROSIM_STDP_TAU_PLUS;
    conn.stdp_tau_minus = NEUROSIM_STDP_TAU_MINUS;
    conn.bcm_theta = NEUROSIM_BCM_THETA_INIT;

    /* L4 -> L2/3 E (feedforward) */
    conn.src_pop = 2; conn.tgt_pop = 0; conn.weight = 3.0f; conn.delay_ms = 1.0f;
    neuroscience_sim_add_connection(sim, &conn);
    /* L2/3 E -> L2/3 E (recurrent excitation) */
    conn.src_pop = 0; conn.tgt_pop = 0; conn.weight = 1.5f; conn.delay_ms = 0.5f;
    neuroscience_sim_add_connection(sim, &conn);
    /* L2/3 E -> L2/3 I */
    conn.src_pop = 0; conn.tgt_pop = 1; conn.weight = 2.0f; conn.delay_ms = 0.5f;
    neuroscience_sim_add_connection(sim, &conn);
    /* L2/3 I -> L2/3 E (feedback inhibition) */
    conn.src_pop = 1; conn.tgt_pop = 0; conn.weight = -3.0f; conn.delay_ms = 1.0f;
    neuroscience_sim_add_connection(sim, &conn);
    /* L2/3 E -> L5 E */
    conn.src_pop = 0; conn.tgt_pop = 3; conn.weight = 2.5f; conn.delay_ms = 2.0f;
    neuroscience_sim_add_connection(sim, &conn);

    /* Neurotransmitters */
    neurosim_transmitter_t glu = {0};
    glu.type = NEUROSIM_NT_GLUTAMATE;
    strncpy(glu.name, "Glutamate", NEUROSIM_MAX_NAME_LEN - 1);
    glu.synaptic_conc = 0.5f;
    glu.baseline_conc = 0.5f;
    glu.release_rate = 10.0f;
    glu.reuptake_rate = 1.0f / NEUROSIM_REUPTAKE_TAU_MS;
    glu.degradation_rate = 0.01f;
    glu.receptor_kd = 5.0f;       /* uM */
    glu.receptor_density = 1000.0f;
    neuroscience_sim_add_transmitter(sim, &glu);

    neurosim_transmitter_t gaba = {0};
    gaba.type = NEUROSIM_NT_GABA;
    strncpy(gaba.name, "GABA", NEUROSIM_MAX_NAME_LEN - 1);
    gaba.synaptic_conc = 0.3f;
    gaba.baseline_conc = 0.3f;
    gaba.release_rate = 8.0f;
    gaba.reuptake_rate = 0.8f / NEUROSIM_REUPTAKE_TAU_MS;
    gaba.degradation_rate = 0.02f;
    gaba.receptor_kd = 3.0f;
    gaba.receptor_density = 800.0f;
    neuroscience_sim_add_transmitter(sim, &gaba);

    /* Oscillation bands */
    neurosim_oscillation_t osc;
    memset(&osc, 0, sizeof(osc));
    osc.coupling_strength = 0.5f;
    osc.amplitude = 1.0f;

    osc.band = NEUROSIM_BAND_ALPHA; osc.frequency = 10.0f; osc.phase = 0.0f;
    sim->oscillations[sim->num_oscillations++] = osc;

    osc.band = NEUROSIM_BAND_BETA; osc.frequency = 20.0f; osc.phase = 1.0f;
    sim->oscillations[sim->num_oscillations++] = osc;

    osc.band = NEUROSIM_BAND_GAMMA; osc.frequency = 40.0f; osc.phase = 2.0f;
    osc.coupling_strength = 0.8f;
    sim->oscillations[sim->num_oscillations++] = osc;

    osc.band = NEUROSIM_BAND_THETA; osc.frequency = 6.0f; osc.phase = 0.5f;
    osc.coupling_strength = 0.3f;
    sim->oscillations[sim->num_oscillations++] = osc;

    LOG_INFO(LOG_TAG, "Loaded cortical column: 4 populations, 5 connections, "
             "2 transmitters, 4 oscillation bands");
}
