/**
 * @file nimcp_chemistry.h
 * @brief Chemistry reasoning module for parietal lobe
 *
 * Implements chemical reasoning capabilities:
 * - Molecular structure representation and analysis
 * - Chemical equation balancing
 * - Stoichiometry calculations
 * - Reaction prediction (basic thermodynamics)
 * - Periodic table property lookup
 *
 * BIOLOGICAL BASIS:
 * Chemistry reasoning recruits parietal numerical processing for
 * stoichiometric calculations combined with spatial reasoning for
 * molecular geometry. The integration of quantitative and spatial
 * cognition enables intuitive chemical reasoning.
 *
 * USAGE:
 * ```c
 * chemistry_t* chem = chemistry_create();
 *
 * // Balance equation
 * reaction_t rxn = chemistry_parse_reaction(chem, "H2 + O2 -> H2O");
 * bool balanced = chemistry_balance_equation(chem, &rxn);
 *
 * // Calculate molar mass
 * molecule_t mol = chemistry_parse_molecule(chem, "H2O");
 * float mass = chemistry_molar_mass(chem, &mol);
 *
 * chemistry_destroy(chem);
 * ```
 */

#ifndef NIMCP_CHEMISTRY_H
#define NIMCP_CHEMISTRY_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Maximum atoms in a molecule */
#define CHEMISTRY_MAX_ATOMS             64

/** Maximum species in a reaction */
#define CHEMISTRY_MAX_SPECIES           16

/** Maximum formula length */
#define CHEMISTRY_MAX_FORMULA           128

/** Maximum element symbol length */
#define CHEMISTRY_ELEMENT_SYMBOL_LEN    4

/** Number of elements in periodic table */
#define CHEMISTRY_NUM_ELEMENTS          118

/** Bio-async module ID for chemistry */
#define BIO_MODULE_CHEMISTRY            0x0385

/* ============================================================================
 * ELEMENT TYPES
 * ============================================================================ */

/**
 * @brief Element categories
 */
typedef enum {
    ELEMENT_CATEGORY_NONMETAL,
    ELEMENT_CATEGORY_NOBLE_GAS,
    ELEMENT_CATEGORY_ALKALI_METAL,
    ELEMENT_CATEGORY_ALKALINE_EARTH,
    ELEMENT_CATEGORY_METALLOID,
    ELEMENT_CATEGORY_HALOGEN,
    ELEMENT_CATEGORY_TRANSITION,
    ELEMENT_CATEGORY_POST_TRANSITION,
    ELEMENT_CATEGORY_LANTHANIDE,
    ELEMENT_CATEGORY_ACTINIDE
} element_category_t;

/**
 * @brief Bond types
 */
typedef enum {
    BOND_SINGLE,
    BOND_DOUBLE,
    BOND_TRIPLE,
    BOND_AROMATIC,
    BOND_IONIC,
    BOND_HYDROGEN,
    BOND_METALLIC
} bond_type_t;

/**
 * @brief Molecular geometry types
 */
typedef enum {
    GEOMETRY_LINEAR,
    GEOMETRY_BENT,
    GEOMETRY_TRIGONAL_PLANAR,
    GEOMETRY_TETRAHEDRAL,
    GEOMETRY_TRIGONAL_BIPYRAMIDAL,
    GEOMETRY_OCTAHEDRAL,
    GEOMETRY_TRIGONAL_PYRAMIDAL,
    GEOMETRY_SEESAW,
    GEOMETRY_T_SHAPED,
    GEOMETRY_SQUARE_PLANAR,
    GEOMETRY_SQUARE_PYRAMIDAL
} molecular_geometry_t;

/**
 * @brief Reaction types
 */
typedef enum {
    REACTION_SYNTHESIS,          /**< A + B -> AB */
    REACTION_DECOMPOSITION,      /**< AB -> A + B */
    REACTION_SINGLE_REPLACEMENT, /**< A + BC -> AC + B */
    REACTION_DOUBLE_REPLACEMENT, /**< AB + CD -> AD + CB */
    REACTION_COMBUSTION,         /**< Fuel + O2 -> CO2 + H2O */
    REACTION_ACID_BASE,          /**< HA + BOH -> BA + H2O */
    REACTION_REDOX,              /**< Electron transfer */
    REACTION_UNKNOWN
} reaction_type_t;

/* ============================================================================
 * TYPES
 * ============================================================================ */

/** Opaque handle for chemistry processor */
typedef struct chemistry chemistry_t;

/**
 * @brief Element properties from periodic table
 */
typedef struct {
    uint8_t atomic_number;              /**< Atomic number (Z) */
    char symbol[CHEMISTRY_ELEMENT_SYMBOL_LEN]; /**< Element symbol */
    char name[32];                      /**< Element name */
    float atomic_mass;                  /**< Atomic mass (g/mol) */
    uint8_t period;                     /**< Period (row) */
    uint8_t group;                      /**< Group (column) */
    element_category_t category;        /**< Element category */
    float electronegativity;            /**< Pauling electronegativity */
    int common_oxidation_states[8];     /**< Common oxidation states */
    uint8_t num_oxidation_states;       /**< Number of common states */
    float electron_affinity;            /**< Electron affinity (kJ/mol) */
    float ionization_energy;            /**< First ionization energy (kJ/mol) */
    float atomic_radius_pm;             /**< Atomic radius (pm) */
} element_properties_t;

/**
 * @brief Atom instance in a molecule
 */
typedef struct {
    uint8_t element;                    /**< Atomic number */
    int8_t formal_charge;               /**< Formal charge */
    uint8_t valence_electrons;          /**< Current valence electrons */
    float x, y, z;                      /**< 3D coordinates (optional) */
} atom_t;

/**
 * @brief Chemical bond between atoms
 */
typedef struct {
    uint8_t atom1_idx;                  /**< First atom index */
    uint8_t atom2_idx;                  /**< Second atom index */
    bond_type_t type;                   /**< Bond type */
    uint8_t order;                      /**< Bond order (1, 2, 3) */
    float length_pm;                    /**< Bond length in pm (optional) */
    float energy_kj_mol;                /**< Bond energy in kJ/mol (optional) */
} bond_t;

/**
 * @brief Molecule structure
 */
typedef struct {
    char formula[CHEMISTRY_MAX_FORMULA]; /**< Molecular formula */
    atom_t atoms[CHEMISTRY_MAX_ATOMS];   /**< Atoms in molecule */
    uint32_t num_atoms;                  /**< Number of atoms */
    bond_t bonds[CHEMISTRY_MAX_ATOMS * 2]; /**< Bonds (max 2 per atom avg) */
    uint32_t num_bonds;                  /**< Number of bonds */
    float molar_mass;                    /**< Molar mass (g/mol) */
    int net_charge;                      /**< Net molecular charge */
    molecular_geometry_t geometry;       /**< Molecular geometry */
} molecule_t;

/**
 * @brief Stoichiometric coefficient
 */
typedef struct {
    molecule_t molecule;                 /**< Molecule */
    uint32_t coefficient;                /**< Stoichiometric coefficient */
} species_t;

/**
 * @brief Chemical reaction
 */
typedef struct {
    species_t reactants[CHEMISTRY_MAX_SPECIES]; /**< Reactant species */
    uint32_t num_reactants;              /**< Number of reactants */
    species_t products[CHEMISTRY_MAX_SPECIES];  /**< Product species */
    uint32_t num_products;               /**< Number of products */
    bool is_balanced;                    /**< Whether balanced */
    reaction_type_t type;                /**< Reaction type */
    float enthalpy_kj;                   /**< Reaction enthalpy (kJ/mol) */
    float entropy_j_k;                   /**< Reaction entropy (J/mol*K) */
    float gibbs_free_energy_kj;          /**< Gibbs free energy (kJ/mol) */
} reaction_t;

/**
 * @brief Stoichiometry calculation result
 */
typedef struct {
    float moles_needed[CHEMISTRY_MAX_SPECIES];   /**< Moles needed for each */
    float moles_produced[CHEMISTRY_MAX_SPECIES]; /**< Moles produced for each */
    float grams_needed[CHEMISTRY_MAX_SPECIES];   /**< Grams needed */
    float grams_produced[CHEMISTRY_MAX_SPECIES]; /**< Grams produced */
    uint32_t limiting_reagent_idx;       /**< Index of limiting reagent */
    float theoretical_yield_g;           /**< Theoretical yield (g) */
    float percent_yield;                 /**< Percent yield (if actual given) */
} stoichiometry_result_t;

/**
 * @brief Chemistry configuration
 */
typedef struct {
    bool enable_3d_coordinates;          /**< Calculate 3D geometry */
    bool enable_thermodynamics;          /**< Calculate thermodynamic properties */
    bool enable_bond_energies;           /**< Look up bond energies */
    bool enable_bio_async;               /**< Enable bio-async messaging */
    float temperature_k;                 /**< Temperature for calculations (default: 298.15 K) */
    float pressure_atm;                  /**< Pressure for calculations (default: 1.0 atm) */
    float inflammation_sensitivity;      /**< Immune modulation sensitivity */
    float sleep_deprivation_factor;      /**< Sleep modulation factor */
} chemistry_config_t;

/**
 * @brief Chemistry statistics
 */
typedef struct {
    uint64_t molecules_parsed;           /**< Molecules parsed */
    uint64_t reactions_balanced;         /**< Reactions balanced */
    uint64_t stoichiometry_calcs;        /**< Stoichiometry calculations */
    uint64_t property_lookups;           /**< Element property lookups */
    float avg_processing_time_us;        /**< Average processing time */
} chemistry_stats_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

/**
 * @brief Create chemistry processor with default configuration
 *
 * @return Handle or NULL on error
 */
chemistry_t* chemistry_create(void);

/**
 * @brief Create chemistry processor with custom configuration
 *
 * @param config Configuration (NULL for defaults)
 * @return Handle or NULL on error
 */
chemistry_t* chemistry_create_custom(const chemistry_config_t* config);

/**
 * @brief Destroy chemistry processor
 *
 * @param chem Handle (NULL safe)
 */
void chemistry_destroy(chemistry_t* chem);

/**
 * @brief Get default configuration
 *
 * @return Default configuration struct
 */
chemistry_config_t chemistry_default_config(void);

/**
 * @brief Validate configuration
 *
 * @param config Configuration to validate
 * @return true if valid
 */
bool chemistry_validate_config(const chemistry_config_t* config);

/* ============================================================================
 * PERIODIC TABLE API
 * ============================================================================ */

/**
 * @brief Get element properties by atomic number
 *
 * @param chem Chemistry handle
 * @param atomic_number Atomic number (1-118)
 * @param props Output properties
 * @return 0 on success
 */
int chemistry_get_element(
    const chemistry_t* chem,
    uint8_t atomic_number,
    element_properties_t* props
);

/**
 * @brief Get element properties by symbol
 *
 * @param chem Chemistry handle
 * @param symbol Element symbol (e.g., "H", "Na", "Cl")
 * @param props Output properties
 * @return 0 on success
 */
int chemistry_get_element_by_symbol(
    const chemistry_t* chem,
    const char* symbol,
    element_properties_t* props
);

/**
 * @brief Check if two elements can form ionic bond
 *
 * @param chem Chemistry handle
 * @param element1 First atomic number
 * @param element2 Second atomic number
 * @return true if ionic bond is favored
 */
bool chemistry_can_form_ionic_bond(
    const chemistry_t* chem,
    uint8_t element1,
    uint8_t element2
);

/**
 * @brief Predict bond type between elements
 *
 * @param chem Chemistry handle
 * @param element1 First atomic number
 * @param element2 Second atomic number
 * @return Predicted bond type
 */
bond_type_t chemistry_predict_bond_type(
    const chemistry_t* chem,
    uint8_t element1,
    uint8_t element2
);

/* ============================================================================
 * MOLECULE API
 * ============================================================================ */

/**
 * @brief Parse molecular formula string
 *
 * @param chem Chemistry handle
 * @param formula Molecular formula (e.g., "H2O", "C6H12O6")
 * @param molecule Output molecule
 * @return 0 on success
 */
int chemistry_parse_molecule(
    chemistry_t* chem,
    const char* formula,
    molecule_t* molecule
);

/**
 * @brief Calculate molar mass of molecule
 *
 * @param chem Chemistry handle
 * @param molecule Molecule
 * @return Molar mass in g/mol
 */
float chemistry_molar_mass(
    const chemistry_t* chem,
    const molecule_t* molecule
);

/**
 * @brief Predict molecular geometry using VSEPR
 *
 * @param chem Chemistry handle
 * @param molecule Molecule (with bonds)
 * @param central_atom_idx Index of central atom
 * @return Predicted geometry
 */
molecular_geometry_t chemistry_predict_geometry(
    chemistry_t* chem,
    const molecule_t* molecule,
    uint32_t central_atom_idx
);

/**
 * @brief Count atoms of specific element in molecule
 *
 * @param molecule Molecule
 * @param atomic_number Element atomic number
 * @return Count of that element
 */
uint32_t chemistry_count_element(
    const molecule_t* molecule,
    uint8_t atomic_number
);

/**
 * @brief Generate molecular formula string
 *
 * @param molecule Molecule
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return 0 on success
 */
int chemistry_molecule_to_string(
    const molecule_t* molecule,
    char* buffer,
    size_t buffer_size
);

/* ============================================================================
 * REACTION API
 * ============================================================================ */

/**
 * @brief Parse chemical reaction string
 *
 * @param chem Chemistry handle
 * @param equation Reaction equation (e.g., "H2 + O2 -> H2O")
 * @param reaction Output reaction
 * @return 0 on success
 */
int chemistry_parse_reaction(
    chemistry_t* chem,
    const char* equation,
    reaction_t* reaction
);

/**
 * @brief Balance chemical equation
 *
 * Uses algebraic method to find integer coefficients.
 *
 * @param chem Chemistry handle
 * @param reaction Reaction to balance (modified in place)
 * @return true if balanced successfully
 */
bool chemistry_balance_equation(
    chemistry_t* chem,
    reaction_t* reaction
);

/**
 * @brief Check if reaction is balanced
 *
 * @param reaction Reaction to check
 * @return true if balanced
 */
bool chemistry_is_balanced(const reaction_t* reaction);

/**
 * @brief Classify reaction type
 *
 * @param chem Chemistry handle
 * @param reaction Reaction to classify
 * @return Reaction type
 */
reaction_type_t chemistry_classify_reaction(
    chemistry_t* chem,
    const reaction_t* reaction
);

/**
 * @brief Calculate reaction thermodynamics
 *
 * @param chem Chemistry handle
 * @param reaction Reaction (enthalpy/entropy populated)
 * @return 0 on success
 */
int chemistry_calculate_thermodynamics(
    chemistry_t* chem,
    reaction_t* reaction
);

/**
 * @brief Check if reaction is spontaneous at given temperature
 *
 * @param reaction Reaction (with thermodynamic data)
 * @param temperature_k Temperature in Kelvin
 * @return true if spontaneous (delta G < 0)
 */
bool chemistry_is_spontaneous(
    const reaction_t* reaction,
    float temperature_k
);

/* ============================================================================
 * STOICHIOMETRY API
 * ============================================================================ */

/**
 * @brief Calculate stoichiometry for given moles of limiting reagent
 *
 * @param chem Chemistry handle
 * @param reaction Balanced reaction
 * @param limiting_reagent_moles Moles of limiting reagent
 * @param limiting_reagent_idx Index of limiting reagent
 * @param result Output result
 * @return 0 on success
 */
int chemistry_calculate_stoichiometry(
    chemistry_t* chem,
    const reaction_t* reaction,
    float limiting_reagent_moles,
    uint32_t limiting_reagent_idx,
    stoichiometry_result_t* result
);

/**
 * @brief Find limiting reagent given moles of each reactant
 *
 * @param chem Chemistry handle
 * @param reaction Balanced reaction
 * @param reactant_moles Array of moles for each reactant
 * @return Index of limiting reagent
 */
uint32_t chemistry_find_limiting_reagent(
    chemistry_t* chem,
    const reaction_t* reaction,
    const float* reactant_moles
);

/**
 * @brief Calculate percent yield
 *
 * @param theoretical_yield Theoretical yield (g)
 * @param actual_yield Actual yield (g)
 * @return Percent yield
 */
float chemistry_percent_yield(
    float theoretical_yield,
    float actual_yield
);

/**
 * @brief Convert moles to grams
 *
 * @param moles Number of moles
 * @param molar_mass Molar mass (g/mol)
 * @return Grams
 */
float chemistry_moles_to_grams(float moles, float molar_mass);

/**
 * @brief Convert grams to moles
 *
 * @param grams Grams
 * @param molar_mass Molar mass (g/mol)
 * @return Moles
 */
float chemistry_grams_to_moles(float grams, float molar_mass);

/* ============================================================================
 * MODULATION API
 * ============================================================================ */

/**
 * @brief Set inflammation level
 *
 * @param chem Chemistry handle
 * @param level Inflammation level [0,1]
 * @return 0 on success
 */
int chemistry_set_inflammation(chemistry_t* chem, float level);

/**
 * @brief Set sleep deprivation level
 *
 * @param chem Chemistry handle
 * @param level Deprivation level [0,1]
 * @return 0 on success
 */
int chemistry_set_sleep_deprivation(chemistry_t* chem, float level);

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

/**
 * @brief Get statistics
 *
 * @param chem Chemistry handle
 * @param stats Output statistics
 * @return 0 on success
 */
int chemistry_get_stats(chemistry_t* chem, chemistry_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param chem Chemistry handle
 */
void chemistry_reset_stats(chemistry_t* chem);

/**
 * @brief Get last error message
 *
 * @return Thread-local error message
 */
const char* chemistry_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CHEMISTRY_H */
