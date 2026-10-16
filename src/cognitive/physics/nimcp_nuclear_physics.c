/**
 * @file nimcp_nuclear_physics.c
 * @brief Nuclear Physics simulation — binding energy, decay chains, fission/fusion
 *
 * WHAT: Semi-empirical mass formula (Weizsacker), radioactive decay with
 *       multi-species chain integration, Q-value calculations, Lawson criterion.
 * WHY:  Reasoning about nuclear energy, radioactivity, stellar nucleosynthesis,
 *       radiation safety, medical isotopes, nuclear dating.
 * HOW:  SEMF for binding energies, exponential decay law with Euler integration,
 *       Bateman equations for decay chains.
 */

#include "cognitive/physics/nimcp_nuclear_physics.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "NUCLEAR_PHYSICS"

/* ============================================================================
 * Constants (local)
 * ============================================================================ */

/* Nuclear radius constant: R = r0 * A^(1/3) */
#define NP_R0_FM            1.25f       /* femtometers */
#define NP_COULOMB_K_MEV_FM 1.4399f     /* e^2/(4*pi*eps_0) in MeV*fm */

/* ============================================================================
 * Default Config
 * ============================================================================ */

np_config_t np_default_config(void) {
    np_config_t c;
    memset(&c, 0, sizeof(c));
    c.dt                = 1.0f;     /* 1 second */
    c.temperature       = 300.0f;   /* room temperature */
    c.enable_decay_chains = true;
    c.enable_fission    = false;
    c.enable_fusion     = false;
    c.max_chain_steps   = NP_MAX_CHAIN_LENGTH;
    return c;
}

/* ============================================================================
 * Create / Destroy
 * ============================================================================ */

nuclear_physics_sim_t* np_create(const np_config_t* config) {
    np_config_t cfg = config ? *config : np_default_config();

    nuclear_physics_sim_t* sim = nimcp_calloc(1, sizeof(nuclear_physics_sim_t));
    if (!sim) return NULL;

    sim->config = cfg;
    /* Initialize daughter indices to -1 (no daughter) */
    for (int i = 0; i < NP_MAX_NUCLIDES; i++) {
        sim->nuclides[i].daughter_idx = -1;
    }
    sim->initialized = true;
    LOG_INFO(LOG_TAG, "Created nuclear physics sim (dt=%.1f s)", cfg.dt);
    return sim;
}

void np_destroy(nuclear_physics_sim_t* sim) {
    if (!sim) return;
    nimcp_free(sim);
}

/* ============================================================================
 * Semi-Empirical Mass Formula (Weizsacker)
 * ============================================================================ */

float np_binding_energy_semf(uint16_t Z, uint16_t A) {
    if (A == 0) return 0.0f;
    uint16_t N = A - Z;
    float Af = (float)A;
    float Zf = (float)Z;

    /* Volume term */
    float B = NP_AV * Af;

    /* Surface term */
    B -= NP_AS * powf(Af, 2.0f / 3.0f);

    /* Coulomb term */
    B -= NP_AC * Zf * Zf / powf(Af, 1.0f / 3.0f);

    /* Asymmetry term */
    float asym = (float)(A - 2 * Z);
    B -= NP_AA * asym * asym / Af;

    /* Pairing term */
    if (Z % 2 == 0 && N % 2 == 0) {
        B += NP_AP / sqrtf(Af);         /* even-even: +delta */
    } else if (Z % 2 == 1 && N % 2 == 1) {
        B -= NP_AP / sqrtf(Af);         /* odd-odd: -delta */
    }
    /* odd-A: delta = 0, no change */

    return B > 0.0f ? B : 0.0f;
}

float np_binding_energy_per_nucleon(uint16_t Z, uint16_t A) {
    if (A == 0) return 0.0f;
    return np_binding_energy_semf(Z, A) / (float)A;
}

float np_nuclear_mass(uint16_t Z, uint16_t A) {
    /* M = Z*Mp + N*Mn - B/c^2,  but in natural units B is already in MeV */
    uint16_t N = A - Z;
    float M = (float)Z * NP_PROTON_MASS + (float)N * NP_NEUTRON_MASS
              - np_binding_energy_semf(Z, A);
    return M;
}

float np_nuclear_radius(uint16_t A) {
    return NP_R0_FM * powf((float)A, 1.0f / 3.0f);
}

/* ============================================================================
 * Decay Calculations
 * ============================================================================ */

float np_decay_constant(float half_life) {
    if (half_life <= 0.0f) return 0.0f;
    return NP_LN2 / half_life;
}

double np_decay_population(double N0, float decay_constant, float time) {
    if (decay_constant <= 0.0f) return N0;
    return N0 * exp(-(double)decay_constant * (double)time);
}

double np_activity(float decay_constant, double population) {
    return (double)decay_constant * population;
}

float np_half_life(float decay_constant) {
    if (decay_constant <= 0.0f) return 1e30f;
    return NP_LN2 / decay_constant;
}

/* ============================================================================
 * Q-Value Calculations
 * ============================================================================ */

float np_q_value(uint16_t parent_Z, uint16_t parent_A,
                  uint16_t product1_Z, uint16_t product1_A,
                  uint16_t product2_Z, uint16_t product2_A) {
    /* Q = (M_parent - M_product1 - M_product2) in MeV */
    float M_parent = np_nuclear_mass(parent_Z, parent_A);
    float M_p1 = np_nuclear_mass(product1_Z, product1_A);
    float M_p2 = np_nuclear_mass(product2_Z, product2_A);
    return M_parent - M_p1 - M_p2;
}

float np_alpha_q_value(uint16_t Z, uint16_t A) {
    /* Alpha decay: (Z,A) -> (Z-2,A-4) + (2,4) */
    if (Z < 2 || A < 4) return 0.0f;
    return np_q_value(Z, A, Z - 2, A - 4, 2, 4);
}

float np_fission_energy(uint16_t Z, uint16_t A) {
    /* Approximate symmetric fission: (Z,A) -> 2*(Z/2, A/2) + ~2n */
    /* Energy ~ B(products) - B(parent) */
    uint16_t Z2 = Z / 2;
    uint16_t A2 = A / 2;
    float B_parent = np_binding_energy_semf(Z, A);
    float B_products = 2.0f * np_binding_energy_semf(Z2, A2);
    return B_products - B_parent;   /* positive = exothermic */
}

float np_coulomb_barrier(uint16_t Z1, uint16_t Z2, float radius_fm) {
    if (radius_fm < 0.1f) radius_fm = 0.1f;
    return NP_COULOMB_K_MEV_FM * (float)Z1 * (float)Z2 / radius_fm;
}

float np_lawson_criterion(float density, float confinement_time,
                           float temperature_keV) {
    /* For D-T fusion, need n*tau_E > 1.5e20 m^-3*s at T ~ 10-20 keV */
    /* Returns the achieved n*tau product */
    (void)temperature_keV;
    return density * confinement_time;
}

/* ============================================================================
 * Nuclide Management
 * ============================================================================ */

uint32_t np_add_nuclide(nuclear_physics_sim_t* sim, const np_nuclide_t* nuclide,
                          double initial_population) {
    if (!sim || sim->num_nuclides >= NP_MAX_NUCLIDES) return UINT32_MAX;
    uint32_t idx = sim->num_nuclides;
    sim->nuclides[idx] = *nuclide;
    sim->populations[idx] = initial_population;
    sim->num_nuclides++;
    return idx;
}

uint32_t np_add_reaction(nuclear_physics_sim_t* sim, const np_reaction_t* reaction) {
    if (!sim || sim->num_reactions >= NP_MAX_NUCLIDES) return UINT32_MAX;
    uint32_t idx = sim->num_reactions;
    sim->reactions[idx] = *reaction;
    sim->num_reactions++;
    return idx;
}

void np_build_decay_chain(nuclear_physics_sim_t* sim, uint32_t parent_idx) {
    if (!sim || parent_idx >= sim->num_nuclides) return;
    if (sim->num_chains >= NP_MAX_NUCLIDES) return;

    np_decay_chain_t* chain = &sim->chains[sim->num_chains];
    memset(chain, 0, sizeof(*chain));

    uint32_t current = parent_idx;
    while (chain->chain_length < NP_MAX_CHAIN_LENGTH && current < sim->num_nuclides) {
        chain->nuclide_idx[chain->chain_length] = current;
        chain->population[chain->chain_length] = sim->populations[current];
        np_nuclide_t* nuc = &sim->nuclides[current];
        if (nuc->half_life > 0.0f) {
            float lambda = np_decay_constant(nuc->half_life);
            chain->activity[chain->chain_length] = np_activity(lambda, sim->populations[current]);
        }
        chain->chain_length++;
        if (nuc->daughter_idx < 0 || nuc->decay_mode == NP_DECAY_STABLE) break;
        current = (uint32_t)nuc->daughter_idx;
    }
    sim->num_chains++;
    LOG_INFO(LOG_TAG, "Built decay chain from %s, length=%u",
             sim->nuclides[parent_idx].symbol, chain->chain_length);
}

void np_load_common_nuclides(nuclear_physics_sim_t* sim) {
    if (!sim) return;

    /* U-235 */
    np_nuclide_t u235 = { .Z = 92, .A = 235, .symbol = "U-235",
        .half_life = 2.221e16f, .decay_mode = NP_DECAY_ALPHA,
        .decay_energy = 4.679f, .daughter_idx = -1 };
    np_add_nuclide(sim, &u235, 0.0);

    /* U-238 */
    np_nuclide_t u238 = { .Z = 92, .A = 238, .symbol = "U-238",
        .half_life = 1.41e17f, .decay_mode = NP_DECAY_ALPHA,
        .decay_energy = 4.270f, .daughter_idx = -1 };
    np_add_nuclide(sim, &u238, 0.0);

    /* Pu-239 */
    np_nuclide_t pu239 = { .Z = 94, .A = 239, .symbol = "Pu-239",
        .half_life = 7.60e11f, .decay_mode = NP_DECAY_ALPHA,
        .decay_energy = 5.244f, .daughter_idx = -1 };
    np_add_nuclide(sim, &pu239, 0.0);

    /* Co-60 (medical/industrial) */
    np_nuclide_t co60 = { .Z = 27, .A = 60, .symbol = "Co-60",
        .half_life = 1.663e8f, .decay_mode = NP_DECAY_BETA_MINUS,
        .decay_energy = 2.824f, .daughter_idx = -1 };
    np_add_nuclide(sim, &co60, 0.0);

    /* I-131 (medical) */
    np_nuclide_t i131 = { .Z = 53, .A = 131, .symbol = "I-131",
        .half_life = 6.93e5f, .decay_mode = NP_DECAY_BETA_MINUS,
        .decay_energy = 0.971f, .daughter_idx = -1 };
    np_add_nuclide(sim, &i131, 0.0);

    /* C-14 (dating) */
    np_nuclide_t c14 = { .Z = 6, .A = 14, .symbol = "C-14",
        .half_life = 1.81e11f, .decay_mode = NP_DECAY_BETA_MINUS,
        .decay_energy = 0.156f, .daughter_idx = -1 };
    np_add_nuclide(sim, &c14, 0.0);

    /* Fe-56 (most stable) */
    np_nuclide_t fe56 = { .Z = 26, .A = 56, .symbol = "Fe-56",
        .half_life = -1.0f, .decay_mode = NP_DECAY_STABLE,
        .decay_energy = 0.0f, .daughter_idx = -1 };
    np_add_nuclide(sim, &fe56, 0.0);

    /* He-4 (alpha particle, fusion product) */
    np_nuclide_t he4 = { .Z = 2, .A = 4, .symbol = "He-4",
        .half_life = -1.0f, .decay_mode = NP_DECAY_STABLE,
        .decay_energy = 0.0f, .daughter_idx = -1 };
    np_add_nuclide(sim, &he4, 0.0);

    LOG_INFO(LOG_TAG, "Loaded %u common nuclides", sim->num_nuclides);
}

/* ============================================================================
 * Simulation Step
 * ============================================================================ */

int np_step(nuclear_physics_sim_t* sim, float dt) {
    if (!sim || !sim->initialized) return -1;
    if (dt <= 0.0f) dt = sim->config.dt;

    double total_activity = 0.0;
    double total_decay_energy = 0.0;
    uint32_t num_decays = 0;

    /* Decay each nuclide: N(t+dt) = N(t) * exp(-lambda*dt) */
    for (uint32_t i = 0; i < sim->num_nuclides; i++) {
        np_nuclide_t* nuc = &sim->nuclides[i];
        if (nuc->decay_mode == NP_DECAY_STABLE || nuc->half_life <= 0.0f) continue;

        float lambda = np_decay_constant(nuc->half_life);
        double N_old = sim->populations[i];
        double N_new = N_old * exp(-(double)lambda * (double)dt);
        double dN = N_old - N_new;  /* number of decays */

        sim->populations[i] = N_new;

        /* Feed daughter */
        if (nuc->daughter_idx >= 0 && (uint32_t)nuc->daughter_idx < sim->num_nuclides) {
            sim->populations[nuc->daughter_idx] += dN;
        }

        /* Accumulate statistics */
        double act = np_activity(lambda, N_new);
        total_activity += act;
        total_decay_energy += dN * (double)nuc->decay_energy;

        if (dN > 0.5) num_decays++;
    }

    /* Update decay chain tracking */
    for (uint32_t ci = 0; ci < sim->num_chains; ci++) {
        np_decay_chain_t* chain = &sim->chains[ci];
        for (uint32_t j = 0; j < chain->chain_length; j++) {
            uint32_t ni = chain->nuclide_idx[j];
            chain->population[j] = sim->populations[ni];
            np_nuclide_t* nuc = &sim->nuclides[ni];
            if (nuc->half_life > 0.0f) {
                float lambda = np_decay_constant(nuc->half_life);
                chain->activity[j] = np_activity(lambda, sim->populations[ni]);
            } else {
                chain->activity[j] = 0.0;
            }
        }
    }

    /* Compute total binding energy */
    double total_be = 0.0;
    for (uint32_t i = 0; i < sim->num_nuclides; i++) {
        if (sim->populations[i] > 0.0) {
            float be = np_binding_energy_semf(sim->nuclides[i].Z, sim->nuclides[i].A);
            total_be += (double)be * sim->populations[i];
        }
    }

    /* Update stats */
    sim->time += dt;
    sim->stats.step_count++;
    sim->stats.time = sim->time;
    sim->stats.total_activity = total_activity;
    sim->stats.total_binding_energy = total_be;
    sim->stats.total_decay_energy = total_decay_energy;
    sim->stats.num_decays = num_decays;

    return 0;
}

np_stats_t np_get_stats(const nuclear_physics_sim_t* sim) {
    if (!sim) { np_stats_t s; memset(&s, 0, sizeof(s)); return s; }
    return sim->stats;
}

/* ============================================================================
 * Legacy API
 * ============================================================================ */

nuclear_physics_sim_t* nuclear_physics_create(const nuclear_physics_config_t* c) { return np_create(c); }
void nuclear_physics_destroy(nuclear_physics_sim_t* s) { np_destroy(s); }
int nuclear_physics_step(nuclear_physics_sim_t* s, float dt) { return np_step(s, dt); }
nuclear_physics_config_t nuclear_physics_default_config(void) { return np_default_config(); }
nuclear_physics_stats_t nuclear_physics_get_stats(const nuclear_physics_sim_t* s) { return np_get_stats(s); }
