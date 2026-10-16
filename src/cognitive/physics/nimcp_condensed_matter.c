/**
 * @file nimcp_condensed_matter.c
 * @brief Condensed Matter Physics simulation — crystals, bands, superconductivity
 *
 * WHAT: Crystal structure calculations, Fermi-Dirac statistics, semiconductor
 *       carrier concentrations, BCS superconductivity, 2D Ising model with
 *       Metropolis Monte Carlo, Debye heat capacity.
 * WHY:  Reasoning about materials, electronics, magnets, phase transitions.
 * HOW:  Analytic formulas for band theory, MC sampling for Ising model,
 *       Debye integral approximation for heat capacity.
 */

#include "cognitive/physics/nimcp_condensed_matter.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "CONDENSED_MATTER"

/* ============================================================================
 * Helpers
 * ============================================================================ */

/** Simple xorshift32 PRNG */
static inline uint32_t xorshift32(uint32_t* state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static inline float randf(uint32_t* state) {
    return (float)(xorshift32(state) & 0x00FFFFFF) / (float)0x01000000;
}

/* ============================================================================
 * Default Config
 * ============================================================================ */

cm_config_t cm_default_config(void) {
    cm_config_t c;
    memset(&c, 0, sizeof(c));
    c.temperature   = 300.0f;       /* room temperature */
    c.band_gap      = CM_EG_SI;     /* silicon */
    c.fermi_energy  = 0.56f;        /* mid-gap for Si */
    c.lattice       = CM_LATTICE_DIAMOND;
    c.lattice_param = CM_A_SI;
    c.doping        = CM_DOPING_NONE;
    c.doping_conc   = 0.0f;
    c.ising_dim     = 32;
    c.ising_J       = 1.0f;
    c.ising_B       = 0.0f;
    c.Tc            = 0.0f;
    c.enable_ising  = false;
    c.enable_superconductor = false;
    return c;
}

/* ============================================================================
 * Create / Destroy
 * ============================================================================ */

condensed_matter_sim_t* cm_create(const cm_config_t* config) {
    cm_config_t cfg = config ? *config : cm_default_config();

    condensed_matter_sim_t* sim = nimcp_calloc(1, sizeof(condensed_matter_sim_t));
    if (!sim) return NULL;

    sim->config = cfg;
    sim->rng_state = 12345u;    /* deterministic seed */

    /* Setup crystal */
    sim->crystal.lattice = cfg.lattice;
    sim->crystal.lattice_param = cfg.lattice_param;
    sim->crystal.atoms_per_cell = cm_atoms_per_cell(cfg.lattice);
    sim->crystal.c_over_a = 1.633f;     /* ideal HCP */
    snprintf(sim->crystal.name, sizeof(sim->crystal.name), "Default");

    /* Setup band structure */
    sim->band.band_gap = cfg.band_gap;
    sim->band.fermi_energy = cfg.fermi_energy;
    sim->band.electron_eff_mass = CM_MDE_SI;
    sim->band.hole_eff_mass = CM_MDH_SI;
    sim->band.doping = cfg.doping;
    sim->band.doping_concentration = cfg.doping_conc;
    if (cfg.band_gap <= 0.0f) {
        sim->band.class_type = CM_MATERIAL_METAL;
    } else if (cfg.band_gap < 4.0f) {
        sim->band.class_type = CM_MATERIAL_SEMICONDUCTOR;
    } else {
        sim->band.class_type = CM_MATERIAL_INSULATOR;
    }

    /* Compute effective DOS at config temperature */
    sim->band.Nc = (float)cm_effective_dos(sim->band.electron_eff_mass, cfg.temperature);
    sim->band.Nv = (float)cm_effective_dos(sim->band.hole_eff_mass, cfg.temperature);

    /* Setup superconductor */
    if (cfg.enable_superconductor && cfg.Tc > 0.0f) {
        sim->sc.Tc = cfg.Tc;
        sim->sc.delta_0 = cm_bcs_delta0(cfg.Tc);
        sim->sc.Hc_0 = 0.1f;       /* placeholder */
        sim->sc.lambda_L = 50.0f;   /* nm, placeholder */
        sim->sc.xi_0 = 100.0f;      /* nm, placeholder */
        sim->sc.type_II = (cfg.Tc > 30.0f);
        sim->band.class_type = CM_MATERIAL_SUPERCONDUCTOR;
    }

    /* Initialize Ising model */
    if (cfg.enable_ising) {
        uint32_t dim = cfg.ising_dim;
        if (dim > CM_MAX_ISING_DIM) dim = CM_MAX_ISING_DIM;
        cm_ising_init(&sim->ising, dim, dim, cfg.ising_J, cfg.ising_B, true);
    }

    sim->initialized = true;
    LOG_INFO(LOG_TAG, "Created condensed matter sim (T=%.0fK, Eg=%.2feV, lattice=%d)",
             cfg.temperature, cfg.band_gap, cfg.lattice);
    return sim;
}

void cm_destroy(condensed_matter_sim_t* sim) {
    if (!sim) return;
    if (sim->ising.spins) nimcp_free(sim->ising.spins);
    nimcp_free(sim);
}

/* ============================================================================
 * Crystal Functions
 * ============================================================================ */

uint32_t cm_atoms_per_cell(cm_lattice_type_t lattice) {
    switch (lattice) {
        case CM_LATTICE_SC:      return 1;
        case CM_LATTICE_BCC:     return 2;
        case CM_LATTICE_FCC:     return 4;
        case CM_LATTICE_HCP:     return 6;
        case CM_LATTICE_DIAMOND: return 8;
        default:                 return 1;
    }
}

float cm_nearest_neighbor_distance(cm_lattice_type_t lattice, float a) {
    switch (lattice) {
        case CM_LATTICE_SC:      return a;
        case CM_LATTICE_BCC:     return a * sqrtf(3.0f) / 2.0f;
        case CM_LATTICE_FCC:     return a * sqrtf(2.0f) / 2.0f;
        case CM_LATTICE_HCP:     return a;
        case CM_LATTICE_DIAMOND: return a * sqrtf(3.0f) / 4.0f;
        default:                 return a;
    }
}

float cm_packing_fraction(cm_lattice_type_t lattice) {
    switch (lattice) {
        case CM_LATTICE_SC:      return (float)M_PI / 6.0f;                        /* 0.5236 */
        case CM_LATTICE_BCC:     return (float)M_PI * sqrtf(3.0f) / 8.0f;          /* 0.6802 */
        case CM_LATTICE_FCC:     return (float)M_PI * sqrtf(2.0f) / 6.0f;          /* 0.7405 */
        case CM_LATTICE_HCP:     return (float)M_PI * sqrtf(2.0f) / 6.0f;          /* 0.7405 */
        case CM_LATTICE_DIAMOND: return (float)M_PI * sqrtf(3.0f) / 16.0f;         /* 0.3401 */
        default:                 return 0.5f;
    }
}

float cm_number_density(cm_lattice_type_t lattice, float lattice_param_angstrom) {
    float a_m = lattice_param_angstrom * 1e-10f;   /* Angstrom to meters */
    float V = a_m * a_m * a_m;
    uint32_t n = cm_atoms_per_cell(lattice);
    return (float)n / V;
}

/* ============================================================================
 * Band Theory / Semiconductors
 * ============================================================================ */

float cm_fermi_dirac(float energy_eV, float fermi_energy_eV, float temperature_K) {
    if (temperature_K < 1e-10f) {
        return (energy_eV < fermi_energy_eV) ? 1.0f : 0.0f;
    }
    float kT = CM_BOLTZMANN_EV * temperature_K;
    float x = (energy_eV - fermi_energy_eV) / kT;
    if (x > 40.0f) return 0.0f;
    if (x < -40.0f) return 1.0f;
    return 1.0f / (expf(x) + 1.0f);
}

double cm_effective_dos(float eff_mass_ratio, float temperature_K) {
    /* Nc = 2 * (2*pi*m*kT / h^2)^(3/2) */
    /* In convenient units: Nc = 2.51e19 * (m_eff/m_e)^(3/2) * (T/300)^(3/2) cm^-3 */
    double m32 = pow((double)eff_mass_ratio, 1.5);
    double T32 = pow((double)temperature_K / 300.0, 1.5);
    return 2.51e19 * m32 * T32;     /* cm^-3 */
}

double cm_intrinsic_carriers(float Eg_eV, float temperature_K,
                              float electron_eff_mass, float hole_eff_mass) {
    /* ni = sqrt(Nc * Nv) * exp(-Eg / (2kT)) */
    double Nc = cm_effective_dos(electron_eff_mass, temperature_K);
    double Nv = cm_effective_dos(hole_eff_mass, temperature_K);
    double kT = (double)CM_BOLTZMANN_EV * (double)temperature_K;
    if (kT < 1e-30) return 0.0;
    return sqrt(Nc * Nv) * exp(-(double)Eg_eV / (2.0 * kT));
}

double cm_doped_carriers(float Eg_eV, float temperature_K,
                          cm_doping_type_t doping, float doping_conc,
                          float electron_eff_mass, float hole_eff_mass) {
    double ni = cm_intrinsic_carriers(Eg_eV, temperature_K,
                                       electron_eff_mass, hole_eff_mass);
    if (doping == CM_DOPING_NONE || doping_conc <= 0.0f) return ni;

    double Nd = (double)doping_conc;
    /* For strong doping (Nd >> ni): n ~ Nd (n-type) or p ~ Na (p-type) */
    /* For weak doping: n ~ sqrt(ni^2 + (Nd/2)^2) + Nd/2 */
    if (Nd > ni * 10.0) return Nd;  /* fully ionized */
    return sqrt(ni * ni + 0.25 * Nd * Nd) + 0.5 * Nd;
}

float cm_doped_fermi_level(float Eg_eV, float temperature_K,
                            cm_doping_type_t doping, float doping_conc,
                            float electron_eff_mass, float hole_eff_mass) {
    /* Ef relative to valence band top */
    double kT = (double)CM_BOLTZMANN_EV * (double)temperature_K;
    if (kT < 1e-30) return Eg_eV / 2.0f;   /* intrinsic mid-gap */

    if (doping == CM_DOPING_NONE) return Eg_eV / 2.0f;

    double Nc = cm_effective_dos(electron_eff_mass, temperature_K);
    double Nv = cm_effective_dos(hole_eff_mass, temperature_K);

    if (doping == CM_DOPING_N_TYPE) {
        /* Ef = Ec - kT * ln(Nc / n) */
        double n = cm_doped_carriers(Eg_eV, temperature_K, doping, doping_conc,
                                      electron_eff_mass, hole_eff_mass);
        if (n <= 0.0) return Eg_eV / 2.0f;
        return (float)((double)Eg_eV - kT * log(Nc / n));
    } else {
        /* Ef = Ev + kT * ln(Nv / p) */
        double p = cm_doped_carriers(Eg_eV, temperature_K, doping, doping_conc,
                                      electron_eff_mass, hole_eff_mass);
        if (p <= 0.0) return Eg_eV / 2.0f;
        return (float)(kT * log(Nv / p));
    }
}

float cm_drude_conductivity(float carrier_conc, float scattering_time,
                              float eff_mass_ratio) {
    /* sigma = n * e^2 * tau / m* */
    /* carrier_conc in cm^-3, convert to m^-3 */
    double n = (double)carrier_conc * 1e6;      /* cm^-3 -> m^-3 */
    double e = (double)CM_ELECTRON_CHARGE;
    double m = (double)eff_mass_ratio * (double)CM_ELECTRON_MASS;
    if (m < 1e-50) return 0.0f;
    return (float)(n * e * e * (double)scattering_time / m);
}

/* ============================================================================
 * Superconductivity
 * ============================================================================ */

float cm_bcs_gap(float delta_0_meV, float Tc, float temperature) {
    /* Delta(T) = Delta_0 * tanh(1.74 * sqrt(Tc/T - 1)) */
    if (temperature >= Tc || temperature <= 0.0f) return 0.0f;
    if (Tc <= 0.0f) return 0.0f;
    float ratio = Tc / temperature - 1.0f;
    if (ratio < 0.0f) return 0.0f;
    return delta_0_meV * tanhf(1.74f * sqrtf(ratio));
}

float cm_bcs_delta0(float Tc) {
    /* Delta_0 = 1.764 * kB * Tc (in meV) */
    /* kB in eV/K = 8.617e-5, so kB*Tc in eV, then *1000 for meV */
    return 1.764f * CM_BOLTZMANN_EV * Tc * 1000.0f;
}

float cm_critical_field(float Hc0, float Tc, float temperature) {
    /* Hc(T) = Hc(0) * (1 - (T/Tc)^2) */
    if (temperature >= Tc) return 0.0f;
    float t2 = (temperature / Tc) * (temperature / Tc);
    return Hc0 * (1.0f - t2);
}

float cm_london_penetration(float lambda_0, float Tc, float temperature) {
    /* lambda(T) = lambda_0 / sqrt(1 - (T/Tc)^4) */
    if (temperature >= Tc) return 1e10f;
    float t4 = powf(temperature / Tc, 4.0f);
    float denom = 1.0f - t4;
    if (denom < 1e-10f) return 1e10f;
    return lambda_0 / sqrtf(denom);
}

/* ============================================================================
 * Ising Model
 * ============================================================================ */

void cm_ising_init(cm_ising_t* ising, uint32_t nx, uint32_t ny,
                    float J, float B, bool random_init) {
    if (!ising) return;
    ising->nx = nx; ising->ny = ny;
    ising->J = J; ising->B = B;
    ising->mc_steps = 0;
    uint32_t total = nx * ny;
    ising->spins = nimcp_calloc(total, sizeof(int8_t));
    if (!ising->spins) return;

    uint32_t rng = 42u;
    for (uint32_t i = 0; i < total; i++) {
        if (random_init) {
            ising->spins[i] = (xorshift32(&rng) & 1) ? 1 : -1;
        } else {
            ising->spins[i] = 1;    /* all spin up */
        }
    }
    cm_ising_compute(ising);
}

void cm_ising_compute(cm_ising_t* ising) {
    if (!ising || !ising->spins) return;
    uint32_t nx = ising->nx, ny = ising->ny;
    float energy = 0.0f;
    float mag = 0.0f;
    uint32_t total = nx * ny;

    for (uint32_t iy = 0; iy < ny; iy++) {
        for (uint32_t ix = 0; ix < nx; ix++) {
            int8_t s = ising->spins[iy * nx + ix];
            mag += (float)s;
            /* Sum over neighbors (right and down only to avoid double counting) */
            int8_t sr = ising->spins[iy * nx + ((ix + 1) % nx)];
            int8_t sd = ising->spins[((iy + 1) % ny) * nx + ix];
            energy -= ising->J * (float)(s * sr + s * sd);
            energy -= ising->B * (float)s * 0.5f;  /* halved for double counting */
        }
    }
    ising->energy = energy;
    ising->magnetization = mag / (float)total;
}

void cm_ising_sweep(cm_ising_t* ising, float temperature, uint32_t sweeps,
                     uint32_t* rng_state) {
    if (!ising || !ising->spins || temperature <= 0.0f) return;
    uint32_t nx = ising->nx, ny = ising->ny;
    uint32_t total = nx * ny;
    float beta = 1.0f / (CM_BOLTZMANN_EV * temperature);
    /* For Ising model, energy in units of J, so beta*J is dimensionless */
    /* Use reduced beta: beta_red = J / (kB * T) */
    float beta_J = ising->J / (CM_BOLTZMANN_EV * temperature);

    for (uint32_t sweep = 0; sweep < sweeps; sweep++) {
        for (uint32_t i = 0; i < total; i++) {
            /* Pick random site */
            uint32_t site = xorshift32(rng_state) % total;
            uint32_t ix = site % nx;
            uint32_t iy = site / nx;
            int8_t s = ising->spins[site];

            /* Sum of neighbor spins (periodic BC) */
            int sum_nn = ising->spins[iy * nx + ((ix + 1) % nx)]
                       + ising->spins[iy * nx + ((ix + nx - 1) % nx)]
                       + ising->spins[((iy + 1) % ny) * nx + ix]
                       + ising->spins[((iy + ny - 1) % ny) * nx + ix];

            /* Delta E for flipping s -> -s */
            float dE = 2.0f * ising->J * (float)(s * sum_nn) + 2.0f * ising->B * (float)s;

            /* Metropolis acceptance */
            if (dE <= 0.0f || randf(rng_state) < expf(-dE * beta_J / ising->J)) {
                ising->spins[site] = -s;
            }
        }
        ising->mc_steps++;
    }
    (void)beta;
    cm_ising_compute(ising);
}

float cm_ising_curie_temperature(float J) {
    /* Tc = 2J / (kB * ln(1 + sqrt(2))) */
    float ln_arg = logf(1.0f + sqrtf(2.0f));   /* ~0.8814 */
    return 2.0f * fabsf(J) / (CM_BOLTZMANN_EV * ln_arg);
}

/* ============================================================================
 * Thermal Properties
 * ============================================================================ */

float cm_debye_heat_capacity(float temperature, float debye_temperature) {
    /* Cv / (3NkB) using Debye model
     * At high T: Cv -> 3NkB (Dulong-Petit)
     * At low T: Cv -> (12/5)*pi^4*NkB*(T/Theta_D)^3
     * Use Pade approximation for intermediate: */
    if (temperature <= 0.0f) return 0.0f;
    float x = debye_temperature / temperature;

    if (x < 0.1f) {
        /* High temperature limit: Cv = 3NkB = 3R per mole */
        return 3.0f * 8.314f;   /* J/(mol*K) */
    }
    if (x > 20.0f) {
        /* Low temperature (T^3 law) */
        float ratio3 = (temperature / debye_temperature);
        ratio3 = ratio3 * ratio3 * ratio3;
        return 3.0f * 8.314f * (12.0f / 5.0f) * (float)(M_PI * M_PI * M_PI * M_PI)
               / (debye_temperature * debye_temperature * debye_temperature)
               * temperature * temperature * temperature;
        /* Simplified: 234 * NkB * (T/Theta_D)^3 */
    }

    /* Intermediate: numerical approximation using 3-term series */
    /* Cv/3NkB ~ 1 - x^2/20 + x^4/1680 - ... (low x expansion of Debye function) */
    /* Better: use tabulated Debye function fit */
    float d = x * x / 20.0f;
    float cv_ratio = 1.0f - d + d * d * 0.084f;
    if (cv_ratio < 0.0f) cv_ratio = 0.0f;
    if (cv_ratio > 1.0f) cv_ratio = 1.0f;
    return 3.0f * 8.314f * cv_ratio;
}

float cm_einstein_heat_capacity(float temperature, float einstein_temperature) {
    /* Cv/3NkB = (Theta_E/T)^2 * exp(Theta_E/T) / (exp(Theta_E/T) - 1)^2 */
    if (temperature <= 0.0f) return 0.0f;
    float x = einstein_temperature / temperature;
    if (x > 40.0f) return 0.0f;
    float ex = expf(x);
    float denom = (ex - 1.0f) * (ex - 1.0f);
    if (denom < 1e-30f) return 0.0f;
    return 3.0f * 8.314f * x * x * ex / denom;
}

float cm_phonon_dispersion(float spring_const, float atom_mass,
                            float k_wavevector, float lattice_param) {
    /* omega = 2*sqrt(K/M) * |sin(k*a/2)| */
    if (atom_mass < 1e-30f) return 0.0f;
    float omega_max = 2.0f * sqrtf(spring_const / atom_mass);
    return omega_max * fabsf(sinf(k_wavevector * lattice_param * 0.5f));
}

float cm_debye_frequency(float debye_temperature) {
    /* omega_D = kB * Theta_D / hbar */
    return CM_BOLTZMANN_K * debye_temperature / CM_HBAR;
}

/* ============================================================================
 * Simulation Step
 * ============================================================================ */

int cm_step(condensed_matter_sim_t* sim, float dt) {
    if (!sim || !sim->initialized) return -1;
    (void)dt;

    float T = sim->config.temperature;

    /* Update semiconductor carrier concentration */
    double ni = cm_intrinsic_carriers(sim->band.band_gap, T,
                                       sim->band.electron_eff_mass,
                                       sim->band.hole_eff_mass);
    double carriers = cm_doped_carriers(sim->band.band_gap, T,
                                         sim->band.doping,
                                         sim->band.doping_concentration,
                                         sim->band.electron_eff_mass,
                                         sim->band.hole_eff_mass);
    sim->stats.carrier_concentration = (float)carriers;

    /* Update Drude conductivity (assume scattering time ~1e-14 s for metals/semiconductors) */
    float tau = 1e-14f;
    sim->stats.conductivity = cm_drude_conductivity((float)carriers, tau,
                                                      sim->band.electron_eff_mass);

    /* Update superconductor gap */
    if (sim->config.enable_superconductor && sim->sc.Tc > 0.0f) {
        sim->stats.bcs_gap = cm_bcs_gap(sim->sc.delta_0, sim->sc.Tc, T);
    }

    /* Run Ising Monte Carlo sweeps */
    if (sim->config.enable_ising && sim->ising.spins) {
        cm_ising_sweep(&sim->ising, T, 10, &sim->rng_state);
        sim->stats.magnetization = sim->ising.magnetization;
        sim->stats.ising_energy = sim->ising.energy;

        /* Susceptibility estimate from fluctuations: chi ~ N * (<M^2> - <M>^2) / (kT) */
        float M = sim->ising.magnetization;
        uint32_t N = sim->ising.nx * sim->ising.ny;
        sim->stats.susceptibility = (float)N * M * M / (CM_BOLTZMANN_EV * T + 1e-30f);
    }

    /* Heat capacity */
    sim->stats.heat_capacity = cm_debye_heat_capacity(T, CM_THETA_D_SI);

    sim->stats.step_count++;
    sim->stats.temperature = T;
    sim->time += dt;

    return 0;
}

cm_stats_t cm_get_stats(const condensed_matter_sim_t* sim) {
    if (!sim) { cm_stats_t s; memset(&s, 0, sizeof(s)); return s; }
    return sim->stats;
}

/* ============================================================================
 * Legacy API
 * ============================================================================ */

condensed_matter_sim_t* condensed_matter_create(const condensed_matter_config_t* c) { return cm_create(c); }
void condensed_matter_destroy(condensed_matter_sim_t* s) { cm_destroy(s); }
int condensed_matter_step(condensed_matter_sim_t* s, float dt) { return cm_step(s, dt); }
condensed_matter_config_t condensed_matter_default_config(void) { return cm_default_config(); }
condensed_matter_stats_t condensed_matter_get_stats(const condensed_matter_sim_t* s) { return cm_get_stats(s); }
