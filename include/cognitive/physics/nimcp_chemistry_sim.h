/**
 * @file nimcp_chemistry_sim.h
 * @brief Chemistry Simulator — reaction dynamics, conservation of mass/atoms
 *
 * WHAT: Simulates chemical systems: substances, reactions, equilibria, phase
 *       transitions, and energy changes.
 * WHY:  Provides chemistry prior for world model. "Mixing bleach and ammonia
 *       produces toxic gas" requires knowing reaction rules. Conservation of
 *       atoms constrains what's physically possible.
 * HOW:  Rule-based reaction engine with mass-action kinetics, atom conservation
 *       checks, Le Chatelier equilibrium, enthalpy tracking.
 *
 * THEORETICAL FOUNDATION:
 *   - Law of Conservation of Mass (Lavoisier, 1789)
 *   - Mass-action kinetics: rate = k * [A]^a * [B]^b
 *   - Le Chatelier's principle for equilibrium perturbation
 *   - Gibbs free energy determines spontaneity
 */

#ifndef NIMCP_CHEMISTRY_SIM_H
#define NIMCP_CHEMISTRY_SIM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define CHEM_MAX_SUBSTANCES     64
#define CHEM_MAX_REACTIONS      128
#define CHEM_MAX_ELEMENTS       32
#define CHEM_MAX_NAME_LEN       32
#define CHEM_MAX_REACTANTS      4
#define CHEM_MAX_PRODUCTS       4

/* ============================================================================
 * Phase / State
 * ============================================================================ */

typedef enum {
    CHEM_PHASE_SOLID    = 0,
    CHEM_PHASE_LIQUID   = 1,
    CHEM_PHASE_GAS      = 2,
    CHEM_PHASE_AQUEOUS  = 3,    /* dissolved in water */
    CHEM_PHASE_PLASMA   = 4,
    CHEM_PHASE_COUNT
} chem_phase_t;

/* ============================================================================
 * Element (atom type)
 * ============================================================================ */

typedef struct {
    char        symbol[4];      /* "H", "O", "C", "Na", etc. */
    float       atomic_mass;    /* atomic mass units */
    uint32_t    atomic_number;
    bool        active;
} chem_element_t;

/* ============================================================================
 * Substance (molecule or compound)
 * ============================================================================ */

typedef struct {
    uint32_t    id;
    char        name[CHEM_MAX_NAME_LEN];    /* "water", "glucose", "NaCl" */
    char        formula[CHEM_MAX_NAME_LEN]; /* "H2O", "C6H12O6", "NaCl" */
    /* Elemental composition: count of each element */
    uint32_t    element_ids[CHEM_MAX_ELEMENTS];
    uint32_t    element_counts[CHEM_MAX_ELEMENTS];
    uint32_t    num_elements;
    /* Properties */
    float       molar_mass;         /* g/mol */
    chem_phase_t default_phase;
    float       melting_point;      /* Kelvin */
    float       boiling_point;      /* Kelvin */
    float       standard_enthalpy;  /* kJ/mol formation enthalpy */
    bool        active;
} chem_substance_t;

/* ============================================================================
 * Reaction
 * ============================================================================ */

typedef struct {
    uint32_t    id;
    /* Reactants and products (substance indices + stoichiometric coefficients) */
    uint32_t    reactant_ids[CHEM_MAX_REACTANTS];
    uint32_t    reactant_coeffs[CHEM_MAX_REACTANTS];
    uint32_t    num_reactants;
    uint32_t    product_ids[CHEM_MAX_PRODUCTS];
    uint32_t    product_coeffs[CHEM_MAX_PRODUCTS];
    uint32_t    num_products;
    /* Kinetics */
    float       rate_constant;      /* k */
    float       activation_energy;  /* Ea, kJ/mol */
    float       enthalpy_change;    /* delta_H, kJ/mol (neg = exothermic) */
    bool        reversible;
    float       reverse_rate;       /* k_reverse for equilibrium */
    bool        active;
} chem_reaction_t;

/* ============================================================================
 * System State
 * ============================================================================ */

typedef struct {
    float       concentrations[CHEM_MAX_SUBSTANCES]; /* mol/L */
    float       amounts[CHEM_MAX_SUBSTANCES];        /* mol (absolute) */
    float       temperature;        /* Kelvin */
    float       pressure;           /* atm */
    float       volume;             /* liters */
    float       pH;                 /* -log10([H+]) */
    float       total_energy;       /* kJ */
    chem_phase_t phases[CHEM_MAX_SUBSTANCES]; /* current phase of each */
} chem_state_t;

/* ============================================================================
 * Violation flags
 * ============================================================================ */

typedef enum {
    CHEM_VIOLATION_NONE             = 0,
    CHEM_VIOLATION_MASS_NOT_CONSERVED = (1 << 0),
    CHEM_VIOLATION_ATOMS_NOT_BALANCED = (1 << 1),
    CHEM_VIOLATION_NEGATIVE_CONC    = (1 << 2),
    CHEM_VIOLATION_IMPOSSIBLE_PHASE = (1 << 3),
    CHEM_VIOLATION_ENERGY_MISMATCH  = (1 << 4),
} chem_violation_t;

/* ============================================================================
 * Config
 * ============================================================================ */

typedef struct {
    float       default_temperature;    /* Kelvin (default: 298.15 = 25°C) */
    float       default_pressure;       /* atm (default: 1.0) */
    float       default_volume;         /* liters (default: 1.0) */
    float       dt;                     /* time step (seconds) */
} chem_config_t;

/* ============================================================================
 * Engine
 * ============================================================================ */

typedef struct chemistry_sim {
    chem_element_t      elements[CHEM_MAX_ELEMENTS];
    uint32_t            num_elements;
    chem_substance_t    substances[CHEM_MAX_SUBSTANCES];
    uint32_t            num_substances;
    chem_reaction_t     reactions[CHEM_MAX_REACTIONS];
    uint32_t            num_reactions;
    chem_state_t        state;
    chem_config_t       config;
    /* Conservation tracking */
    float               initial_atom_counts[CHEM_MAX_ELEMENTS];
    float               initial_total_mass;
    float               mass_drift;
    /* Statistics */
    uint64_t            step_count;
    uint64_t            reactions_fired;
    bool                initialized;
} chemistry_sim_t;

/* ============================================================================
 * API
 * ============================================================================ */

chemistry_sim_t* chemistry_sim_create(const chem_config_t* config);
void chemistry_sim_destroy(chemistry_sim_t* sim);

/** Register an element (returns element id) */
uint32_t chemistry_sim_add_element(chemistry_sim_t* sim, const char* symbol,
                                    float atomic_mass, uint32_t atomic_number);

/** Register a substance (returns substance id) */
uint32_t chemistry_sim_add_substance(chemistry_sim_t* sim, const chem_substance_t* sub);

/** Register a reaction (returns reaction id) */
uint32_t chemistry_sim_add_reaction(chemistry_sim_t* sim, const chem_reaction_t* rxn);

/** Set concentration of a substance (mol/L) */
void chemistry_sim_set_concentration(chemistry_sim_t* sim, uint32_t substance_id, float conc);

/** Step the simulation (apply reactions, update concentrations) */
int chemistry_sim_step(chemistry_sim_t* sim, float dt);

/** Check if a predicted state violates chemistry laws */
chem_violation_t chemistry_sim_check_violations(const chemistry_sim_t* sim,
                                                 const chem_state_t* predicted);

/** Compute total mass in system (should be conserved) */
float chemistry_sim_total_mass(const chemistry_sim_t* sim);

/** Compute atom counts for conservation check */
void chemistry_sim_atom_counts(const chemistry_sim_t* sim, float* counts, uint32_t max_elements);

/** Get current pH */
float chemistry_sim_get_ph(const chemistry_sim_t* sim);

/** Set temperature (may trigger phase transitions) */
void chemistry_sim_set_temperature(chemistry_sim_t* sim, float kelvin);

/** Load common elements (H, C, N, O, Na, Cl, Fe, Ca, K, P, S) */
void chemistry_sim_load_common_elements(chemistry_sim_t* sim);

/** Load common substances (water, CO2, O2, glucose, NaCl, etc.) */
void chemistry_sim_load_common_substances(chemistry_sim_t* sim);

/** Load common reactions (combustion, photosynthesis, acid-base, etc.) */
void chemistry_sim_load_common_reactions(chemistry_sim_t* sim);

/** Default config */
chem_config_t chemistry_sim_default_config(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CHEMISTRY_SIM_H */
