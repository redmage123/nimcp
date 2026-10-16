/**
 * @file nimcp_physical_chemistry.h
 * @brief Physical Chemistry — thermodynamics, stat mech, quantum chem, spectroscopy
 *
 * Gibbs free energy, partition functions, Boltzmann distributions, molecular
 * orbital theory, Beer-Lambert law, blackbody radiation.
 */

#ifndef NIMCP_PHYSICAL_CHEMISTRY_H
#define NIMCP_PHYSICAL_CHEMISTRY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PCHEM_MAX_SPECIES       64
#define PCHEM_MAX_NAME          32
#define PCHEM_R                 8.314f      /* J/(mol·K) */
#define PCHEM_KB                1.381e-23f  /* J/K */
#define PCHEM_H                 6.626e-34f  /* J·s */
#define PCHEM_NA                6.022e23f
#define PCHEM_C                 299792458.0f

typedef struct {
    uint32_t    id;
    char        name[PCHEM_MAX_NAME];
    float       enthalpy;           /* H° kJ/mol (formation) */
    float       entropy;            /* S° J/(mol·K) */
    float       gibbs;              /* G° kJ/mol */
    float       heat_capacity_cp;   /* J/(mol·K) */
    float       molar_mass;
    float       concentration;
    /* Molecular properties */
    float       dipole_moment;      /* Debye */
    float       polarizability;     /* Å³ */
    float       ionization_energy;  /* eV */
    uint32_t    degrees_of_freedom; /* 3N-5 (linear) or 3N-6 (nonlinear) */
    bool        active;
} pchem_species_t;

typedef struct {
    float       temperature;
    float       pressure;
    float       volume;
    float       dt;
    bool        enable_quantum;     /* molecular orbital calculations */
    bool        enable_spectroscopy;
} pchem_config_t;

typedef struct {
    uint64_t    step_count;
    float       total_gibbs;
    float       total_entropy;
    float       equilibrium_constant;
    float       reaction_quotient;
} pchem_stats_t;

typedef struct physical_chemistry_sim {
    pchem_species_t species[PCHEM_MAX_SPECIES];
    uint32_t        num_species;
    pchem_config_t  config;
    pchem_stats_t   stats;
    float           time;
    bool            initialized;
} physical_chemistry_sim_t;

physical_chemistry_sim_t* physical_chemistry_create(const pchem_config_t* config);
void physical_chemistry_destroy(physical_chemistry_sim_t* sim);
uint32_t physical_chemistry_add_species(physical_chemistry_sim_t* sim, const pchem_species_t* sp);
int physical_chemistry_step(physical_chemistry_sim_t* sim, float dt);

/** Gibbs free energy: ΔG = ΔH - TΔS */
float pchem_gibbs_free_energy(float dH, float T, float dS);
/** Equilibrium constant: K = exp(-ΔG°/RT) */
float pchem_equilibrium_constant(float dG, float T);
/** Van't Hoff: d(ln K)/dT = ΔH°/(RT²) → K₂ = K₁·exp(-ΔH/R·(1/T₂-1/T₁)) */
float pchem_vant_hoff(float K1, float dH, float T1, float T2);
/** Clausius-Clapeyron: ln(P₂/P₁) = -ΔH_vap/R·(1/T₂-1/T₁) */
float pchem_clausius_clapeyron(float P1, float dH_vap, float T1, float T2);
/** Boltzmann distribution: P(E) = exp(-E/kT) / Z */
float pchem_boltzmann_probability(float energy, float T, float partition_Z);
/** Partition function (harmonic oscillator): Z = 1/(1-exp(-hν/kT)) */
float pchem_partition_harmonic(float frequency, float T);
/** Partition function (translational): Z = (2πmkT/h²)^(3/2) · V */
float pchem_partition_translational(float mass, float T, float V);
/** Ideal gas: PV = nRT */
float pchem_ideal_gas_pressure(float n, float T, float V);
/** Van der Waals: (P + a/V²)(V - b) = RT (per mole) */
float pchem_van_der_waals_pressure(float T, float V, float a, float b);
/** Beer-Lambert law: A = εlc (absorbance) */
float pchem_beer_lambert(float epsilon, float path_length, float concentration);
/** Blackbody spectral radiance: B(ν,T) = 2hν³/c² · 1/(exp(hν/kT)-1) */
float pchem_planck_radiation(float frequency, float T);
/** Wien's displacement: λ_max = b/T where b = 2.898e-3 m·K */
float pchem_wien_displacement(float T);
/** Debye-Hückel activity: log γ = -A·z²·√I */
float pchem_debye_huckel(float z, float ionic_strength);

pchem_config_t physical_chemistry_default_config(void);
pchem_stats_t physical_chemistry_get_stats(const physical_chemistry_sim_t* sim);

#ifdef __cplusplus
}
#endif
#endif
