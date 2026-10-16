/**
 * @file nimcp_physical_chemistry.c
 * @brief Physical Chemistry simulator — thermodynamics, stat mech, spectroscopy
 *
 * Implements Gibbs free energy, equilibrium constants, Van't Hoff equation,
 * Clausius-Clapeyron phase transitions, Boltzmann distribution, partition
 * functions, ideal gas and Van der Waals EOS, Beer-Lambert, Planck radiation,
 * Wien displacement, Debye-Huckel activity coefficients.
 */

#include "cognitive/physics/nimcp_physical_chemistry.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <float.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define LOG_TAG "PCHEM"

/** Minimum temperature to avoid division by zero */
#define TEMP_EPSILON    0.01f

/** Wien displacement constant (m*K) */
#define WIEN_B          2.898e-3f

/** Debye-Huckel A constant at 25C in water */
#define DEBYE_A         0.509f

/** Minimum volume to avoid singularity */
#define VOL_EPSILON     1e-10f

/* ============================================================================
 * Default config
 * ============================================================================ */

pchem_config_t physical_chemistry_default_config(void)
{
    pchem_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.temperature         = 298.15f;  /* 25 C */
    cfg.pressure            = 101325.0f; /* 1 atm in Pa */
    cfg.volume              = 0.001f;   /* 1 L = 0.001 m^3 */
    cfg.dt                  = 0.01f;
    cfg.enable_quantum      = false;
    cfg.enable_spectroscopy = false;
    return cfg;
}

/* ============================================================================
 * Create / Destroy
 * ============================================================================ */

physical_chemistry_sim_t* physical_chemistry_create(const pchem_config_t* config)
{
    physical_chemistry_sim_t* sim =
        (physical_chemistry_sim_t*)nimcp_calloc(1, sizeof(physical_chemistry_sim_t));
    if (!sim) {
        LOG_ERROR(LOG_TAG, "Failed to allocate physical_chemistry_sim_t");
        return NULL;
    }
    sim->config = config ? *config : physical_chemistry_default_config();
    sim->initialized = true;
    LOG_INFO(LOG_TAG, "Physical chemistry sim created (T=%.1fK, P=%.0fPa)",
             sim->config.temperature, sim->config.pressure);
    return sim;
}

void physical_chemistry_destroy(physical_chemistry_sim_t* sim)
{
    if (!sim) return;
    LOG_INFO(LOG_TAG, "Destroying physical chemistry sim (steps=%lu)",
             (unsigned long)sim->stats.step_count);
    nimcp_free(sim);
}

/* ============================================================================
 * Add species
 * ============================================================================ */

uint32_t physical_chemistry_add_species(physical_chemistry_sim_t* sim,
                                         const pchem_species_t* sp)
{
    if (!sim || !sp) return UINT32_MAX;
    if (sim->num_species >= PCHEM_MAX_SPECIES) {
        LOG_WARN(LOG_TAG, "Max species reached (%d)", PCHEM_MAX_SPECIES);
        return UINT32_MAX;
    }
    uint32_t idx = sim->num_species++;
    sim->species[idx] = *sp;
    sim->species[idx].id = idx;
    sim->species[idx].active = true;
    return idx;
}

/* ============================================================================
 * Analytical functions — Thermodynamics
 * ============================================================================ */

/**
 * Gibbs free energy: dG = dH - T*dS
 * @param dH  Enthalpy change (kJ/mol)
 * @param T   Temperature (K)
 * @param dS  Entropy change (J/(mol*K)) — note: converts to kJ internally
 */
float pchem_gibbs_free_energy(float dH, float T, float dS)
{
    return dH - T * (dS / 1000.0f);  /* dS in J/(mol*K) -> kJ/(mol*K) */
}

/**
 * Equilibrium constant from Gibbs energy: K = exp(-dG/(RT))
 * @param dG  Standard Gibbs energy (kJ/mol)
 * @param T   Temperature (K)
 */
float pchem_equilibrium_constant(float dG, float T)
{
    if (T < TEMP_EPSILON) return 0.0f;
    float RT = (PCHEM_R / 1000.0f) * T;  /* R in kJ/(mol*K) */
    if (fabsf(RT) < 1e-10f) return 0.0f;
    float exponent = -dG / RT;
    /* Clamp to prevent overflow */
    if (exponent > 80.0f) exponent = 80.0f;
    if (exponent < -80.0f) exponent = -80.0f;
    return expf(exponent);
}

/**
 * Van't Hoff equation: K2 = K1 * exp(-dH/R * (1/T2 - 1/T1))
 * Temperature dependence of equilibrium constant.
 */
float pchem_vant_hoff(float K1, float dH, float T1, float T2)
{
    if (T1 < TEMP_EPSILON || T2 < TEMP_EPSILON) return K1;
    float exponent = -(dH * 1000.0f) / PCHEM_R * (1.0f / T2 - 1.0f / T1);
    if (exponent > 80.0f) exponent = 80.0f;
    if (exponent < -80.0f) exponent = -80.0f;
    return K1 * expf(exponent);
}

/**
 * Clausius-Clapeyron: ln(P2/P1) = -dH_vap/R * (1/T2 - 1/T1)
 * Relates vapor pressure to temperature.
 */
float pchem_clausius_clapeyron(float P1, float dH_vap, float T1, float T2)
{
    if (T1 < TEMP_EPSILON || T2 < TEMP_EPSILON || P1 <= 0.0f) return P1;
    float exponent = -(dH_vap * 1000.0f) / PCHEM_R * (1.0f / T2 - 1.0f / T1);
    if (exponent > 80.0f) exponent = 80.0f;
    if (exponent < -80.0f) exponent = -80.0f;
    return P1 * expf(exponent);
}

/* ============================================================================
 * Analytical functions — Statistical Mechanics
 * ============================================================================ */

/**
 * Boltzmann probability: P(E) = exp(-E/(kT)) / Z
 */
float pchem_boltzmann_probability(float energy, float T, float partition_Z)
{
    if (T < TEMP_EPSILON || partition_Z < 1e-30f) return 0.0f;
    float boltz = expf(-energy / (PCHEM_KB * T));
    return boltz / partition_Z;
}

/**
 * Partition function for quantum harmonic oscillator:
 * Z = 1 / (1 - exp(-h*nu/(kT)))
 */
float pchem_partition_harmonic(float frequency, float T)
{
    if (T < TEMP_EPSILON || frequency <= 0.0f) return 1.0f;
    float x = PCHEM_H * frequency / (PCHEM_KB * T);
    float denom = 1.0f - expf(-x);
    if (fabsf(denom) < 1e-30f) return 1e30f;  /* high-T limit */
    return 1.0f / denom;
}

/**
 * Translational partition function:
 * Z_trans = (2*pi*m*kT/h^2)^(3/2) * V
 */
float pchem_partition_translational(float mass, float T, float V)
{
    if (T < TEMP_EPSILON || mass <= 0.0f || V < VOL_EPSILON) return 1.0f;
    float thermal_wavelength_sq = (PCHEM_H * PCHEM_H) / (2.0f * (float)M_PI * mass * PCHEM_KB * T);
    /* Use FLT_MIN (~1.2e-38) as lower bound — values below are subnormal/zero on float */
    if (thermal_wavelength_sq < FLT_MIN) return 1e30f;
    float lambda_cubed = powf(thermal_wavelength_sq, 1.5f);
    if (lambda_cubed < FLT_MIN) return 1e30f;
    return V / lambda_cubed;
}

/* ============================================================================
 * Analytical functions — Equations of State
 * ============================================================================ */

/**
 * Ideal gas law: P = nRT/V
 */
float pchem_ideal_gas_pressure(float n, float T, float V)
{
    if (V < VOL_EPSILON) return 1e30f;
    return n * PCHEM_R * T / V;
}

/**
 * Van der Waals equation of state (per mole):
 * P = RT/(V-b) - a/V^2
 * Accounts for intermolecular attraction (a) and finite molecular volume (b).
 */
float pchem_van_der_waals_pressure(float T, float V, float a, float b)
{
    float Veff = V - b;
    if (Veff < VOL_EPSILON) Veff = VOL_EPSILON;
    float P_repulsive = PCHEM_R * T / Veff;
    float P_attractive = a / (V * V + VOL_EPSILON);
    return P_repulsive - P_attractive;
}

/* ============================================================================
 * Analytical functions — Spectroscopy & Radiation
 * ============================================================================ */

/**
 * Beer-Lambert law: A = epsilon * l * c
 * @param epsilon  Molar absorptivity (L/(mol*cm))
 * @param path_length  Optical path length (cm)
 * @param concentration  Molar concentration (mol/L)
 */
float pchem_beer_lambert(float epsilon, float path_length, float concentration)
{
    return epsilon * path_length * concentration;
}

/**
 * Planck radiation law (spectral radiance):
 * B(nu,T) = 2*h*nu^3/c^2 * 1/(exp(h*nu/(kT)) - 1)
 */
float pchem_planck_radiation(float frequency, float T)
{
    if (T < TEMP_EPSILON || frequency <= 0.0f) return 0.0f;
    float x = PCHEM_H * frequency / (PCHEM_KB * T);
    if (x > 80.0f) return 0.0f;  /* exponentially suppressed */
    float denom = expf(x) - 1.0f;
    if (fabsf(denom) < 1e-30f) denom = 1e-30f;
    float prefactor = 2.0f * PCHEM_H * frequency * frequency * frequency /
                      (PCHEM_C * PCHEM_C);
    return prefactor / denom;
}

/**
 * Wien's displacement law: lambda_max = b / T
 * where b = 2.898e-3 m*K
 */
float pchem_wien_displacement(float T)
{
    if (T < TEMP_EPSILON) return 0.0f;
    return WIEN_B / T;
}

/**
 * Debye-Huckel limiting law for activity coefficients:
 * log10(gamma) = -A * z^2 * sqrt(I)
 * @param z  Ion charge number
 * @param ionic_strength  Ionic strength (mol/L)
 */
float pchem_debye_huckel(float z, float ionic_strength)
{
    if (ionic_strength < 0.0f) ionic_strength = 0.0f;
    float log_gamma = -DEBYE_A * z * z * sqrtf(ionic_strength);
    return powf(10.0f, log_gamma);
}

/* ============================================================================
 * Step — evolve the system
 * ============================================================================ */

int physical_chemistry_step(physical_chemistry_sim_t* sim, float dt)
{
    if (!sim || !sim->initialized) return -1;
    if (dt <= 0.0f) dt = sim->config.dt;
    if (dt <= 0.0f) dt = 0.01f;

    float T = sim->config.temperature;
    float total_gibbs = 0.0f;
    float total_entropy = 0.0f;

    /* Compute thermodynamic properties for each species */
    for (uint32_t i = 0; i < sim->num_species; i++) {
        pchem_species_t* sp = &sim->species[i];
        if (!sp->active) continue;

        /* Update Gibbs from H and S */
        sp->gibbs = sp->enthalpy - T * (sp->entropy / 1000.0f);
        total_gibbs += sp->gibbs * sp->concentration;
        total_entropy += sp->entropy * sp->concentration;
    }

    /* Compute equilibrium constant from total reaction Gibbs energy */
    float dG_rxn = total_gibbs;
    float K_eq = pchem_equilibrium_constant(dG_rxn, T);

    /* Compute reaction quotient Q from concentrations */
    /* Simplified: Q = product(products) / product(reactants) */
    /* For generic step, we track the ratio for stats */
    float Q = 1.0f;
    for (uint32_t i = 0; i < sim->num_species; i++) {
        if (sim->species[i].active && sim->species[i].concentration > 1e-15f) {
            Q *= sim->species[i].concentration;
        }
    }

    /* Drive concentrations toward equilibrium */
    if (sim->num_species >= 2 && K_eq > 0.0f) {
        float ratio = Q / K_eq;
        float driving_force = -logf(ratio + 1e-30f);  /* negative if Q > K */
        float adjustment = driving_force * 0.001f * dt;

        /* Shift first species (reactant-like) and second (product-like) */
        sim->species[0].concentration -= adjustment;
        if (sim->species[0].concentration < 0.0f) sim->species[0].concentration = 0.0f;
        if (sim->num_species > 1) {
            sim->species[1].concentration += adjustment;
            if (sim->species[1].concentration < 0.0f) sim->species[1].concentration = 0.0f;
        }
    }

    /* Update stats */
    sim->stats.step_count++;
    sim->stats.total_gibbs = total_gibbs;
    sim->stats.total_entropy = total_entropy;
    sim->stats.equilibrium_constant = K_eq;
    sim->stats.reaction_quotient = Q;
    sim->time += dt;

    return 0;
}

/* ============================================================================
 * Stats
 * ============================================================================ */

pchem_stats_t physical_chemistry_get_stats(const physical_chemistry_sim_t* sim)
{
    pchem_stats_t zero;
    memset(&zero, 0, sizeof(zero));
    if (!sim) return zero;
    return sim->stats;
}
