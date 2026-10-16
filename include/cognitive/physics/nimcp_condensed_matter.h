/**
 * @file nimcp_condensed_matter.h
 * @brief Condensed Matter Physics simulation engine
 *
 * WHAT: Crystal structures, band theory, semiconductors, superconductivity,
 *       magnetism, phonons, Fermi-Dirac statistics, Debye model.
 * WHY:  Enables reasoning about materials science, electronics, magnets,
 *       superconductors, phase transitions, thermal properties.
 * HOW:  Analytic band structure, Fermi-Dirac statistics, BCS gap equation,
 *       2D Ising model with Metropolis algorithm, Debye model for heat capacity.
 *
 * THEORETICAL FOUNDATION:
 *   Fermi-Dirac:     f(E) = 1 / (exp((E - Ef) / (kT)) + 1)
 *   Intrinsic ni:    ni = sqrt(Nc * Nv) * exp(-Eg / (2kT))
 *   BCS Gap:         Delta(T) = Delta_0 * tanh(1.74 * sqrt(Tc/T - 1))
 *   Debye Cv:        Cv = 9*N*kB*(T/Theta_D)^3 * integral(x^4*e^x/(e^x-1)^2, 0, Theta_D/T)
 *   Ising Energy:    H = -J * sum(s_i * s_j) - B * sum(s_i)
 *   Phonon:          omega = 2*sqrt(K/M) * |sin(k*a/2)|
 *   Drude:           sigma = n*e^2*tau / m
 */

#ifndef NIMCP_CONDENSED_MATTER_H
#define NIMCP_CONDENSED_MATTER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define CM_BOLTZMANN_K          1.380649e-23f   /* J/K */
#define CM_BOLTZMANN_EV         8.617333e-5f    /* eV/K */
#define CM_PLANCK               6.626070e-34f   /* J*s */
#define CM_HBAR                 1.054572e-34f   /* J*s */
#define CM_ELECTRON_MASS        9.109384e-31f   /* kg */
#define CM_ELECTRON_CHARGE      1.602176e-19f   /* C */
#define CM_AVOGADRO             6.022141e23f    /* /mol */
#define CM_PI                   3.14159265f

/* Band gaps (eV at 300K) */
#define CM_EG_SI                1.12f
#define CM_EG_GE                0.67f
#define CM_EG_GAAS              1.42f
#define CM_EG_GAN               3.4f
#define CM_EG_DIAMOND           5.47f
#define CM_EG_SIO2              9.0f

/* Effective DOS masses (units of m_e) */
#define CM_MDE_SI               1.08f       /* electron DOS mass Si */
#define CM_MDH_SI               0.56f       /* hole DOS mass Si */

/* Debye temperatures (K) */
#define CM_THETA_D_CU           343.0f
#define CM_THETA_D_AL           428.0f
#define CM_THETA_D_FE           470.0f
#define CM_THETA_D_SI           645.0f
#define CM_THETA_D_DIAMOND      2230.0f

/* BCS superconductor critical temperatures (K) */
#define CM_TC_AL                1.175f
#define CM_TC_NB                9.26f
#define CM_TC_PB                7.19f
#define CM_TC_SN                3.72f
#define CM_TC_YBCO              93.0f       /* high-Tc */
#define CM_TC_MGDIBORIDE        39.0f

/* Lattice parameters (Angstroms) */
#define CM_A_CU                 3.615f      /* FCC */
#define CM_A_FE                 2.870f      /* BCC */
#define CM_A_SI                 5.431f      /* diamond cubic */
#define CM_A_AL                 4.050f      /* FCC */
#define CM_A_NA                 4.290f      /* BCC */

#define CM_MAX_ISING_DIM        64          /* per axis for 2D Ising */
#define CM_MAX_BANDS            8
#define CM_MAX_DOPANTS          4

/* ============================================================================
 * Enums
 * ============================================================================ */

typedef enum {
    CM_LATTICE_FCC      = 0,    /* face-centered cubic */
    CM_LATTICE_BCC      = 1,    /* body-centered cubic */
    CM_LATTICE_HCP      = 2,    /* hexagonal close-packed */
    CM_LATTICE_SC       = 3,    /* simple cubic */
    CM_LATTICE_DIAMOND  = 4,    /* diamond cubic (Si, Ge) */
} cm_lattice_type_t;

typedef enum {
    CM_MATERIAL_METAL           = 0,
    CM_MATERIAL_SEMICONDUCTOR   = 1,
    CM_MATERIAL_INSULATOR       = 2,
    CM_MATERIAL_SUPERCONDUCTOR  = 3,
} cm_material_class_t;

typedef enum {
    CM_DOPING_NONE      = 0,
    CM_DOPING_N_TYPE    = 1,    /* donor impurities */
    CM_DOPING_P_TYPE    = 2,    /* acceptor impurities */
} cm_doping_type_t;

/* ============================================================================
 * Crystal Structure
 * ============================================================================ */

typedef struct {
    cm_lattice_type_t   lattice;
    float               lattice_param;      /* Angstroms */
    float               c_over_a;           /* for HCP (default 1.633) */
    uint32_t            atoms_per_cell;     /* FCC=4, BCC=2, HCP=6, SC=1 */
    float               atomic_mass;        /* amu */
    char                name[32];
} cm_crystal_t;

/* ============================================================================
 * Band Structure
 * ============================================================================ */

typedef struct {
    cm_material_class_t class_type;
    float               band_gap;           /* eV */
    float               fermi_energy;       /* eV (from band edge) */
    float               electron_eff_mass;  /* units of m_e */
    float               hole_eff_mass;      /* units of m_e */
    float               Nc;                 /* conduction band DOS (cm^-3) */
    float               Nv;                 /* valence band DOS (cm^-3) */
    /* Doping */
    cm_doping_type_t    doping;
    float               doping_concentration; /* cm^-3 */
    float               donor_energy;       /* eV below conduction band */
    float               acceptor_energy;    /* eV above valence band */
} cm_band_structure_t;

/* ============================================================================
 * Superconductor
 * ============================================================================ */

typedef struct {
    float               Tc;                 /* critical temperature (K) */
    float               delta_0;            /* BCS gap at T=0 (meV) */
    float               Hc_0;              /* critical field at T=0 (T) */
    float               lambda_L;           /* London penetration depth (nm) */
    float               xi_0;              /* coherence length (nm) */
    bool                type_II;            /* true for type-II */
} cm_superconductor_t;

/* ============================================================================
 * 2D Ising Model
 * ============================================================================ */

typedef struct {
    int8_t*             spins;              /* [nx * ny], +1 or -1 */
    uint32_t            nx, ny;
    float               J;                  /* exchange coupling (positive = ferromagnetic) */
    float               B;                  /* external field (T) */
    float               temperature;        /* K */
    float               magnetization;      /* <s> average */
    float               energy;             /* total energy */
    uint64_t            mc_steps;
} cm_ising_t;

/* ============================================================================
 * Config
 * ============================================================================ */

typedef struct {
    float               temperature;        /* K */
    float               band_gap;           /* eV (0 = metal) */
    float               fermi_energy;       /* eV */
    cm_lattice_type_t   lattice;
    float               lattice_param;      /* Angstroms */
    cm_doping_type_t    doping;
    float               doping_conc;        /* cm^-3 */
    /* Ising */
    uint32_t            ising_dim;          /* grid size */
    float               ising_J;            /* exchange coupling */
    float               ising_B;            /* external field */
    /* Superconductor */
    float               Tc;                 /* 0 = not superconducting */
    bool                enable_ising;
    bool                enable_superconductor;
} cm_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

typedef struct {
    uint64_t    step_count;
    float       magnetization;          /* Ising <M> */
    float       susceptibility;         /* Ising chi */
    float       ising_energy;
    float       carrier_concentration;  /* cm^-3 */
    float       conductivity;           /* S/m */
    float       bcs_gap;               /* meV at current T */
    float       heat_capacity;          /* J/(mol*K) */
    float       temperature;
} cm_stats_t;

/* ============================================================================
 * Engine
 * ============================================================================ */

typedef struct condensed_matter_sim {
    cm_crystal_t        crystal;
    cm_band_structure_t band;
    cm_superconductor_t sc;
    cm_ising_t          ising;

    cm_config_t         config;
    cm_stats_t          stats;
    float               time;
    uint32_t            rng_state;      /* simple PRNG for Ising MC */
    bool                initialized;
} condensed_matter_sim_t;

/* ============================================================================
 * Core API
 * ============================================================================ */

condensed_matter_sim_t* cm_create(const cm_config_t* config);
void cm_destroy(condensed_matter_sim_t* sim);
int cm_step(condensed_matter_sim_t* sim, float dt);
cm_config_t cm_default_config(void);
cm_stats_t cm_get_stats(const condensed_matter_sim_t* sim);

/* ============================================================================
 * Crystal Functions
 * ============================================================================ */

/** Number of atoms per unit cell for a lattice type */
uint32_t cm_atoms_per_cell(cm_lattice_type_t lattice);

/** Nearest-neighbor distance */
float cm_nearest_neighbor_distance(cm_lattice_type_t lattice, float a);

/** Packing fraction */
float cm_packing_fraction(cm_lattice_type_t lattice);

/** Number density: N/V (atoms per m^3) */
float cm_number_density(cm_lattice_type_t lattice, float lattice_param_angstrom);

/* ============================================================================
 * Band Theory / Semiconductors
 * ============================================================================ */

/** Fermi-Dirac distribution: f(E) = 1 / (exp((E-Ef)/(kT)) + 1) */
float cm_fermi_dirac(float energy_eV, float fermi_energy_eV, float temperature_K);

/** Intrinsic carrier concentration: ni = sqrt(Nc*Nv)*exp(-Eg/(2kT)) */
double cm_intrinsic_carriers(float Eg_eV, float temperature_K,
                              float electron_eff_mass, float hole_eff_mass);

/** Effective density of states: Nc = 2*(2*pi*m*kT/h^2)^(3/2) */
double cm_effective_dos(float eff_mass_ratio, float temperature_K);

/** Doped carrier concentration (simplified): n = Nd for n-type at room T */
double cm_doped_carriers(float Eg_eV, float temperature_K,
                          cm_doping_type_t doping, float doping_conc,
                          float electron_eff_mass, float hole_eff_mass);

/** Fermi level for doped semiconductor */
float cm_doped_fermi_level(float Eg_eV, float temperature_K,
                            cm_doping_type_t doping, float doping_conc,
                            float electron_eff_mass, float hole_eff_mass);

/** Drude conductivity: sigma = n*e^2*tau / m */
float cm_drude_conductivity(float carrier_conc, float scattering_time,
                              float eff_mass_ratio);

/* ============================================================================
 * Superconductivity
 * ============================================================================ */

/** BCS gap: Delta(T) = Delta_0 * tanh(1.74 * sqrt(Tc/T - 1)) */
float cm_bcs_gap(float delta_0_meV, float Tc, float temperature);

/** BCS gap at T=0: Delta_0 = 1.764 * kB * Tc (in meV) */
float cm_bcs_delta0(float Tc);

/** Critical field: Hc(T) = Hc(0) * (1 - (T/Tc)^2) */
float cm_critical_field(float Hc0, float Tc, float temperature);

/** London penetration depth: lambda(T) = lambda_0 / sqrt(1 - (T/Tc)^4) */
float cm_london_penetration(float lambda_0, float Tc, float temperature);

/* ============================================================================
 * Ising Model
 * ============================================================================ */

/** Initialize Ising spins (random or aligned) */
void cm_ising_init(cm_ising_t* ising, uint32_t nx, uint32_t ny,
                    float J, float B, bool random_init);

/** Perform N Metropolis Monte Carlo sweeps */
void cm_ising_sweep(cm_ising_t* ising, float temperature, uint32_t sweeps,
                     uint32_t* rng_state);

/** Compute magnetization and energy */
void cm_ising_compute(cm_ising_t* ising);

/** Exact Curie temperature for 2D Ising: Tc = 2J / (kB * ln(1 + sqrt(2))) */
float cm_ising_curie_temperature(float J);

/* ============================================================================
 * Thermal Properties
 * ============================================================================ */

/** Debye heat capacity: Cv/3NkB approximation */
float cm_debye_heat_capacity(float temperature, float debye_temperature);

/** Einstein heat capacity */
float cm_einstein_heat_capacity(float temperature, float einstein_temperature);

/** Phonon dispersion: omega = 2*sqrt(K/M)*|sin(k*a/2)| */
float cm_phonon_dispersion(float spring_const, float atom_mass,
                            float k_wavevector, float lattice_param);

/** Debye frequency: omega_D */
float cm_debye_frequency(float debye_temperature);

/* Legacy API */
typedef cm_config_t condensed_matter_config_t;
typedef cm_stats_t  condensed_matter_stats_t;

condensed_matter_sim_t* condensed_matter_create(const condensed_matter_config_t* config);
void condensed_matter_destroy(condensed_matter_sim_t* sim);
int condensed_matter_step(condensed_matter_sim_t* sim, float dt);
condensed_matter_config_t condensed_matter_default_config(void);
condensed_matter_stats_t condensed_matter_get_stats(const condensed_matter_sim_t* sim);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CONDENSED_MATTER_H */
