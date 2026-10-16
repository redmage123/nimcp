/**
 * @file nimcp_particle_physics.c
 * @brief Particle Physics simulator — Standard Model, decays, conservation laws
 *
 * Implements Standard Model particle table loading, common decay channels,
 * conservation law checking (charge, baryon, lepton number), invariant mass
 * from 4-momenta, COM energy, de Broglie/Compton wavelength, lifetime from
 * width, particle propagation, Monte Carlo decay with branching ratios,
 * and product injection.
 */

#include "cognitive/physics/nimcp_particle_physics.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>
#include <math.h>
#include <stdlib.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define LOG_TAG "PARTICLE"

/** Planck's constant in GeV*s */
#define HBAR_GEV_S      6.582e-25f

/** Speed of light (m/s) */
#define C_LIGHT         299792458.0f

/** h in GeV*m for de Broglie */
#define H_GEV_M         1.240e-6f

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * Default config
 * ============================================================================ */

pp_sim_config_t particle_physics_default_config(void)
{
    pp_sim_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.collision_energy = 13000.0f;  /* 13 TeV (LHC Run 2) */
    cfg.dt = 1e-12f;                  /* 1 picosecond */
    cfg.enable_decays = true;
    cfg.enable_conservation_checks = true;
    return cfg;
}

/* ============================================================================
 * Create / Destroy
 * ============================================================================ */

particle_physics_sim_t* particle_physics_create(const pp_sim_config_t* config)
{
    particle_physics_sim_t* sim =
        (particle_physics_sim_t*)nimcp_calloc(1, sizeof(particle_physics_sim_t));
    if (!sim) {
        LOG_ERROR(LOG_TAG, "Failed to allocate particle_physics_sim_t");
        return NULL;
    }
    sim->config = config ? *config : particle_physics_default_config();
    particle_physics_load_standard_model(sim);
    sim->initialized = true;
    LOG_INFO(LOG_TAG, "Particle physics sim created (sqrt(s)=%.0f GeV)", sim->config.collision_energy);
    return sim;
}

void particle_physics_destroy(particle_physics_sim_t* sim)
{
    if (!sim) return;
    LOG_INFO(LOG_TAG, "Destroying particle physics sim (steps=%lu, decays=%lu)",
             (unsigned long)sim->stats.step_count, (unsigned long)sim->stats.total_decays);
    nimcp_free(sim);
}

/* ============================================================================
 * Load Standard Model
 *
 * All masses in GeV/c^2, charges in units of e, spins in units of hbar.
 * ============================================================================ */

void particle_physics_load_standard_model(particle_physics_sim_t* sim)
{
    if (!sim) return;
    memset(sim->definitions, 0, sizeof(sim->definitions));

    /* Helper macro for readability */
    #define DEF(ID, NAME, SYM, CLASS, MASS, CHG, SPIN, BARY, LEP, LIFE, WIDTH) \
        do { \
            pp_particle_def_t* p = &sim->definitions[ID]; \
            p->id = ID; \
            strncpy(p->name, NAME, PP_MAX_NAME-1); \
            strncpy(p->symbol, SYM, 7); \
            p->pclass = CLASS; p->mass = MASS; p->charge = CHG; \
            p->spin = SPIN; p->baryon_number = BARY; p->lepton_number = LEP; \
            p->lifetime = LIFE; p->width = WIDTH; p->active = true; \
        } while(0)

    /* Quarks (baryon_number = +1 for composite, individual quarks have 1/3 but stored as 0 here) */
    DEF(PP_UP,       "up",       "u",   PP_FERMION_QUARK,  0.0022f,   2.0f/3.0f, 0.5f, 0,0, 0.0f, 0.0f);
    DEF(PP_DOWN,     "down",     "d",   PP_FERMION_QUARK,  0.0047f,  -1.0f/3.0f, 0.5f, 0,0, 0.0f, 0.0f);
    DEF(PP_CHARM,    "charm",    "c",   PP_FERMION_QUARK,  1.275f,    2.0f/3.0f, 0.5f, 0,0, 0.0f, 0.0f);
    DEF(PP_STRANGE,  "strange",  "s",   PP_FERMION_QUARK,  0.095f,   -1.0f/3.0f, 0.5f, 0,0, 0.0f, 0.0f);
    sim->definitions[PP_STRANGE].strangeness = -1;
    DEF(PP_TOP,      "top",      "t",   PP_FERMION_QUARK,  172.76f,   2.0f/3.0f, 0.5f, 0,0, 5e-25f, 1.42f);
    DEF(PP_BOTTOM,   "bottom",   "b",   PP_FERMION_QUARK,  4.18f,    -1.0f/3.0f, 0.5f, 0,0, 0.0f, 0.0f);

    /* Leptons */
    DEF(PP_ELECTRON,          "electron",          "e-",  PP_FERMION_LEPTON, 0.000511f, -1.0f, 0.5f, 0,1, 0.0f, 0.0f);
    DEF(PP_MUON,              "muon",              "mu-", PP_FERMION_LEPTON, 0.1057f,   -1.0f, 0.5f, 0,1, 2.2e-6f, 3.0e-19f);
    DEF(PP_TAU,               "tau",               "tau-",PP_FERMION_LEPTON, 1.777f,    -1.0f, 0.5f, 0,1, 2.9e-13f, 2.27e-12f);
    DEF(PP_ELECTRON_NEUTRINO, "electron_neutrino", "ve",  PP_FERMION_LEPTON, 0.0f,       0.0f, 0.5f, 0,1, 0.0f, 0.0f);
    DEF(PP_MUON_NEUTRINO,     "muon_neutrino",     "vmu", PP_FERMION_LEPTON, 0.0f,       0.0f, 0.5f, 0,1, 0.0f, 0.0f);
    DEF(PP_TAU_NEUTRINO,      "tau_neutrino",      "vt",  PP_FERMION_LEPTON, 0.0f,       0.0f, 0.5f, 0,1, 0.0f, 0.0f);

    /* Gauge bosons */
    DEF(PP_PHOTON,  "photon",  "gamma", PP_BOSON_GAUGE, 0.0f,    0.0f, 1.0f, 0,0, 0.0f, 0.0f);
    DEF(PP_W_PLUS,  "W+",     "W+",    PP_BOSON_GAUGE, 80.379f,  1.0f, 1.0f, 0,0, 3.17e-25f, 2.085f);
    DEF(PP_W_MINUS, "W-",     "W-",    PP_BOSON_GAUGE, 80.379f, -1.0f, 1.0f, 0,0, 3.17e-25f, 2.085f);
    DEF(PP_Z_BOSON, "Z",      "Z0",    PP_BOSON_GAUGE, 91.188f,  0.0f, 1.0f, 0,0, 2.64e-25f, 2.495f);
    DEF(PP_GLUON,   "gluon",  "g",     PP_BOSON_GAUGE, 0.0f,     0.0f, 1.0f, 0,0, 0.0f, 0.0f);

    /* Higgs boson */
    DEF(PP_HIGGS,   "Higgs",  "H0",    PP_BOSON_SCALAR, 125.10f,  0.0f, 0.0f, 0,0, 1.56e-22f, 4.07e-3f);

    /* Composite particles */
    DEF(PP_PROTON,     "proton",   "p",   PP_FERMION_QUARK, 0.9383f,  1.0f, 0.5f, 1,0, 0.0f, 0.0f);
    DEF(PP_NEUTRON,    "neutron",  "n",   PP_FERMION_QUARK, 0.9396f,  0.0f, 0.5f, 1,0, 879.4f, 7.49e-28f);
    DEF(PP_PION_PLUS,  "pion+",   "pi+", PP_BOSON_SCALAR,  0.1396f,  1.0f, 0.0f, 0,0, 2.6e-8f, 2.53e-17f);
    DEF(PP_PION_MINUS, "pion-",   "pi-", PP_BOSON_SCALAR,  0.1396f, -1.0f, 0.0f, 0,0, 2.6e-8f, 2.53e-17f);
    DEF(PP_PION_ZERO,  "pion0",   "pi0", PP_BOSON_SCALAR,  0.1350f,  0.0f, 0.0f, 0,0, 8.5e-17f, 7.73e-9f);
    DEF(PP_KAON_PLUS,  "kaon+",   "K+",  PP_BOSON_SCALAR,  0.4937f,  1.0f, 0.0f, 0,0, 1.24e-8f, 5.32e-17f);
    sim->definitions[PP_KAON_PLUS].strangeness = 1;
    DEF(PP_KAON_ZERO,  "kaon0",   "K0",  PP_BOSON_SCALAR,  0.4976f,  0.0f, 0.0f, 0,0, 5.12e-8f, 1.29e-17f);
    sim->definitions[PP_KAON_ZERO].strangeness = 1;

    #undef DEF

    /* Load common decay channels */
    /* Neutron -> proton + electron + anti-electron-neutrino (beta decay) */
    pp_decay_channel_t d;
    memset(&d, 0, sizeof(d));
    d.parent = PP_NEUTRON;
    d.products[0] = PP_PROTON; d.products[1] = PP_ELECTRON; d.products[2] = PP_ELECTRON_NEUTRINO;
    d.num_products = 3;
    d.branching_ratio = 1.0f;
    d.mediator = PP_FORCE_WEAK;
    d.active = true;
    particle_physics_add_decay(sim, &d);

    /* Muon -> electron + muon-neutrino + anti-electron-neutrino */
    memset(&d, 0, sizeof(d));
    d.parent = PP_MUON;
    d.products[0] = PP_ELECTRON; d.products[1] = PP_MUON_NEUTRINO; d.products[2] = PP_ELECTRON_NEUTRINO;
    d.num_products = 3;
    d.branching_ratio = 0.9998f;
    d.mediator = PP_FORCE_WEAK;
    d.active = true;
    particle_physics_add_decay(sim, &d);

    /* Pion+ -> muon + muon-neutrino */
    memset(&d, 0, sizeof(d));
    d.parent = PP_PION_PLUS;
    d.products[0] = PP_MUON; d.products[1] = PP_MUON_NEUTRINO;
    d.num_products = 2;
    d.branching_ratio = 0.9999f;
    d.mediator = PP_FORCE_WEAK;
    d.active = true;
    particle_physics_add_decay(sim, &d);

    /* Pion- -> muon + anti-muon-neutrino */
    memset(&d, 0, sizeof(d));
    d.parent = PP_PION_MINUS;
    d.products[0] = PP_MUON; d.products[1] = PP_MUON_NEUTRINO;
    d.num_products = 2;
    d.branching_ratio = 0.9999f;
    d.mediator = PP_FORCE_WEAK;
    d.active = true;
    particle_physics_add_decay(sim, &d);

    /* Pion0 -> 2 photons */
    memset(&d, 0, sizeof(d));
    d.parent = PP_PION_ZERO;
    d.products[0] = PP_PHOTON; d.products[1] = PP_PHOTON;
    d.num_products = 2;
    d.branching_ratio = 0.9882f;
    d.mediator = PP_FORCE_EM;
    d.active = true;
    particle_physics_add_decay(sim, &d);

    /* Tau -> electron + neutrinos (simplified) */
    memset(&d, 0, sizeof(d));
    d.parent = PP_TAU;
    d.products[0] = PP_ELECTRON; d.products[1] = PP_TAU_NEUTRINO; d.products[2] = PP_ELECTRON_NEUTRINO;
    d.num_products = 3;
    d.branching_ratio = 0.178f;
    d.mediator = PP_FORCE_WEAK;
    d.active = true;
    particle_physics_add_decay(sim, &d);

    /* Tau -> muon + neutrinos */
    memset(&d, 0, sizeof(d));
    d.parent = PP_TAU;
    d.products[0] = PP_MUON; d.products[1] = PP_TAU_NEUTRINO; d.products[2] = PP_MUON_NEUTRINO;
    d.num_products = 3;
    d.branching_ratio = 0.174f;
    d.mediator = PP_FORCE_WEAK;
    d.active = true;
    particle_physics_add_decay(sim, &d);

    LOG_INFO(LOG_TAG, "Standard Model loaded: %d particles, %u decay channels",
             PP_STANDARD_MODEL_COUNT, sim->num_decays);
}

/* ============================================================================
 * Add decay / interaction / inject
 * ============================================================================ */

uint32_t particle_physics_add_decay(particle_physics_sim_t* sim, const pp_decay_channel_t* d)
{
    if (!sim || !d) return UINT32_MAX;
    if (sim->num_decays >= PP_MAX_DECAYS) {
        LOG_WARN(LOG_TAG, "Max decays reached (%d)", PP_MAX_DECAYS);
        return UINT32_MAX;
    }
    uint32_t idx = sim->num_decays++;
    sim->decays[idx] = *d;
    sim->decays[idx].active = true;
    return idx;
}

uint32_t particle_physics_add_interaction(particle_physics_sim_t* sim,
                                           const pp_interaction_t* inter)
{
    if (!sim || !inter) return UINT32_MAX;
    if (sim->num_interactions >= PP_MAX_INTERACTIONS) {
        LOG_WARN(LOG_TAG, "Max interactions reached (%d)", PP_MAX_INTERACTIONS);
        return UINT32_MAX;
    }
    uint32_t idx = sim->num_interactions++;
    sim->interactions[idx] = *inter;
    sim->interactions[idx].active = true;
    return idx;
}

uint32_t particle_physics_inject(particle_physics_sim_t* sim, pp_particle_id_t type,
                                  rel_four_vector_t four_momentum, wm_parietal_vec3_t position)
{
    if (!sim) return UINT32_MAX;
    if (sim->num_particles >= PP_MAX_PARTICLES) {
        LOG_WARN(LOG_TAG, "Max live particles reached (%d)", PP_MAX_PARTICLES);
        return UINT32_MAX;
    }
    uint32_t idx = sim->num_particles++;
    pp_live_particle_t* lp = &sim->particles[idx];
    memset(lp, 0, sizeof(*lp));
    lp->type = type;
    lp->four_momentum = four_momentum;
    lp->position = position;
    lp->proper_time = 0.0f;
    lp->decayed = false;
    lp->active = true;
    return idx;
}

/* ============================================================================
 * Conservation law checking
 * Returns 0 if charge, baryon number, and lepton number are all conserved.
 * ============================================================================ */

int particle_physics_check_conservation(const particle_physics_sim_t* sim,
                                         const pp_particle_id_t* initial, uint32_t n_initial,
                                         const pp_particle_id_t* final_state, uint32_t n_final)
{
    if (!sim || !initial || !final_state) return -1;

    float charge_i = 0.0f, charge_f = 0.0f;
    int baryon_i = 0, baryon_f = 0;
    int lepton_i = 0, lepton_f = 0;

    for (uint32_t i = 0; i < n_initial; i++) {
        if (initial[i] < PP_STANDARD_MODEL_COUNT) {
            const pp_particle_def_t* p = &sim->definitions[initial[i]];
            charge_i += p->charge;
            baryon_i += p->baryon_number;
            lepton_i += p->lepton_number;
        }
    }
    for (uint32_t i = 0; i < n_final; i++) {
        if (final_state[i] < PP_STANDARD_MODEL_COUNT) {
            const pp_particle_def_t* p = &sim->definitions[final_state[i]];
            charge_f += p->charge;
            baryon_f += p->baryon_number;
            lepton_f += p->lepton_number;
        }
    }

    int violations = 0;
    if (fabsf(charge_i - charge_f) > 0.01f) violations++;
    if (baryon_i != baryon_f) violations++;
    if (lepton_i != lepton_f) violations++;
    return violations;
}

/* ============================================================================
 * Invariant mass from 4-momenta: M^2 = (sum p)^2 = (sum E)^2 - |sum p_vec|^2
 * Uses double precision for the sum.
 * ============================================================================ */

float particle_physics_invariant_mass(const rel_four_vector_t* momenta, uint32_t n)
{
    if (!momenta || n == 0) return 0.0f;
    double Et = 0.0, px = 0.0, py = 0.0, pz = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        Et += (double)momenta[i].t;
        px += (double)momenta[i].x;
        py += (double)momenta[i].y;
        pz += (double)momenta[i].z;
    }
    double M2 = Et * Et - px * px - py * py - pz * pz;
    if (M2 < 0.0) return 0.0f;
    return (float)sqrt(M2);
}

/**
 * Center of mass energy: sqrt(s) = sqrt((p1+p2)^2)
 */
float particle_physics_com_energy(rel_four_vector_t p1, rel_four_vector_t p2)
{
    rel_four_vector_t sum;
    sum.t = p1.t + p2.t;
    sum.x = p1.x + p2.x;
    sum.y = p1.y + p2.y;
    sum.z = p1.z + p2.z;
    return particle_physics_invariant_mass(&sum, 1);
}

/**
 * Lifetime from decay width: tau = hbar / Gamma
 * @param width_gev  Decay width in GeV
 */
float particle_physics_lifetime_from_width(float width_gev)
{
    if (width_gev <= 0.0f) return 0.0f;
    return HBAR_GEV_S / width_gev;
}

/**
 * De Broglie wavelength: lambda = h / p
 * @param momentum_gev  Momentum in GeV/c
 * @return Wavelength in meters
 */
float particle_physics_de_broglie(float momentum_gev)
{
    if (momentum_gev <= 0.0f) return 0.0f;
    /* h*c = 1.240e-6 GeV*m, so lambda = h*c / (p*c) = 1.240e-6 / p (GeV) */
    return H_GEV_M / momentum_gev;
}

/**
 * Compton wavelength: lambda_C = h / (m*c)
 * @param mass_gev  Mass in GeV/c^2
 * @return Wavelength in meters
 */
float particle_physics_compton_wavelength(float mass_gev)
{
    if (mass_gev <= 0.0f) return 0.0f;
    return H_GEV_M / mass_gev;
}

/* ============================================================================
 * Simple PRNG for Monte Carlo decay (deterministic, fast)
 * ============================================================================ */

static _Thread_local uint32_t pp_rng_state = 12345;

static float pp_random_float(void)
{
    pp_rng_state ^= pp_rng_state << 13;
    pp_rng_state ^= pp_rng_state >> 17;
    pp_rng_state ^= pp_rng_state << 5;
    return (float)(pp_rng_state & 0x00FFFFFF) / (float)0x01000000;
}

/* ============================================================================
 * Step: propagate particles, check decays, inject products
 * ============================================================================ */

int particle_physics_step(particle_physics_sim_t* sim, float dt)
{
    if (!sim || !sim->initialized) return -1;
    if (dt <= 0.0f) dt = sim->config.dt;
    if (dt <= 0.0f) dt = 1e-12f;

    float total_energy = 0.0f;
    float total_charge = 0.0f;
    int total_baryon = 0;
    int total_lepton = 0;

    /* --- Phase 1: Propagate live particles --- */
    for (uint32_t i = 0; i < sim->num_particles; i++) {
        pp_live_particle_t* lp = &sim->particles[i];
        if (!lp->active || lp->decayed) continue;

        /* Extract velocity from 4-momentum: v = p*c^2/E */
        float E = lp->four_momentum.t;  /* E/c stored in .t */
        if (E > 0.001f) {
            float vx = lp->four_momentum.x / E;
            float vy = lp->four_momentum.y / E;
            float vz = lp->four_momentum.z / E;
            /* Scale by c for SI positions, but we work in natural units */
            lp->position.x += vx * C_LIGHT * dt;
            lp->position.y += vy * C_LIGHT * dt;
            lp->position.z += vz * C_LIGHT * dt;
        }

        /* Update proper time: dtau = dt / gamma = dt * m / E */
        if (lp->type < PP_STANDARD_MODEL_COUNT) {
            float mass = sim->definitions[lp->type].mass;
            if (E > 0.001f && mass > 0.0f) {
                lp->proper_time += dt * mass / E;
            } else {
                lp->proper_time += dt;
            }
        }

        /* Accumulate conserved quantities */
        if (lp->type < PP_STANDARD_MODEL_COUNT) {
            const pp_particle_def_t* def = &sim->definitions[lp->type];
            total_energy += E;
            total_charge += def->charge;
            total_baryon += def->baryon_number;
            total_lepton += def->lepton_number;
        }
    }

    /* --- Phase 2: Check for decays (Monte Carlo) --- */
    if (sim->config.enable_decays) {
        for (uint32_t i = 0; i < sim->num_particles; i++) {
            pp_live_particle_t* lp = &sim->particles[i];
            if (!lp->active || lp->decayed) continue;
            if (lp->type >= PP_STANDARD_MODEL_COUNT) continue;

            float lifetime = sim->definitions[lp->type].lifetime;
            if (lifetime <= 0.0f) continue;  /* stable particle */

            /* Decay probability: P = 1 - exp(-dt_proper / lifetime) */
            /* Use proper time increment for this step */
            float E = lp->four_momentum.t;
            float mass = sim->definitions[lp->type].mass;
            float dt_proper = dt;
            if (E > 0.001f && mass > 0.0f) dt_proper = dt * mass / E;

            float P_decay = 1.0f - expf(-dt_proper / lifetime);
            float roll = pp_random_float();

            if (roll < P_decay) {
                /* Find applicable decay channel using branching ratios */
                float cumulative = 0.0f;
                float channel_roll = pp_random_float();
                bool decayed = false;

                for (uint32_t d = 0; d < sim->num_decays; d++) {
                    pp_decay_channel_t* dc = &sim->decays[d];
                    if (!dc->active || dc->parent != lp->type) continue;

                    cumulative += dc->branching_ratio;
                    if (channel_roll <= cumulative) {
                        /* Execute decay: mark parent as decayed, inject products */
                        lp->decayed = true;

                        /* Simple isotropic decay in parent rest frame */
                        /* Distribute energy roughly equally among products */
                        float E_parent = lp->four_momentum.t;
                        float E_per_product = E_parent / (float)dc->num_products;

                        for (uint32_t p = 0; p < dc->num_products; p++) {
                            rel_four_vector_t prod_mom;
                            /* Random direction for 3-momentum */
                            float phi = pp_random_float() * 2.0f * (float)M_PI;
                            float cos_theta = 2.0f * pp_random_float() - 1.0f;
                            float sin_theta = sqrtf(1.0f - cos_theta * cos_theta);
                            float prod_mass = 0.0f;
                            if (dc->products[p] < PP_STANDARD_MODEL_COUNT)
                                prod_mass = sim->definitions[dc->products[p]].mass;

                            float p_mag = sqrtf(fabsf(E_per_product * E_per_product -
                                                       prod_mass * prod_mass));

                            prod_mom.t = E_per_product;
                            prod_mom.x = p_mag * sin_theta * cosf(phi);
                            prod_mom.y = p_mag * sin_theta * sinf(phi);
                            prod_mom.z = p_mag * cos_theta;

                            particle_physics_inject(sim, dc->products[p],
                                                    prod_mom, lp->position);
                        }

                        sim->stats.total_decays++;
                        decayed = true;
                        break;
                    }
                }

                /* If no matching channel found but particle is unstable, just mark it */
                if (!decayed && P_decay > 0.5f) {
                    lp->decayed = true;
                }
            }
        }
    }

    /* --- Phase 3: Conservation check --- */
    if (sim->config.enable_conservation_checks && sim->stats.step_count > 0) {
        float prev_charge = sim->stats.total_charge;
        int prev_baryon = sim->stats.total_baryon_number;
        int prev_lepton = sim->stats.total_lepton_number;

        if (fabsf(total_charge - prev_charge) > 0.01f ||
            total_baryon != prev_baryon ||
            total_lepton != prev_lepton) {
            sim->stats.conservation_violations++;
            LOG_WARN(LOG_TAG, "Conservation violation at step %lu: "
                     "dQ=%.2f, dB=%d, dL=%d",
                     (unsigned long)sim->stats.step_count,
                     total_charge - prev_charge,
                     total_baryon - prev_baryon,
                     total_lepton - prev_lepton);
        }
    }

    /* --- Update stats --- */
    sim->stats.step_count++;
    sim->stats.total_energy = total_energy;
    sim->stats.total_charge = total_charge;
    sim->stats.total_baryon_number = total_baryon;
    sim->stats.total_lepton_number = total_lepton;
    sim->time += dt;

    return 0;
}

/* ============================================================================
 * Stats
 * ============================================================================ */

pp_sim_stats_t particle_physics_get_stats(const particle_physics_sim_t* sim)
{
    pp_sim_stats_t zero;
    memset(&zero, 0, sizeof(zero));
    if (!sim) return zero;
    return sim->stats;
}
