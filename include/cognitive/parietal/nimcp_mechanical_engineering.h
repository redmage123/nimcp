/**
 * @file nimcp_mechanical_engineering.h
 * @brief Mechanical engineering reasoning module for parietal lobe
 *
 * WHAT: Mechanical engineering analysis and reasoning capabilities
 * WHY:  Enable intelligent reasoning about mechanics, structures, and machines
 * HOW:  Domain-specific analysis with intuitive extrapolation
 *
 * BIOLOGICAL BASIS:
 * The parietal cortex integrates spatial and force information for
 * intuitive understanding of mechanical systems.
 *
 * CAPABILITIES:
 * - Statics and dynamics analysis
 * - Structural mechanics (stress, strain, deformation)
 * - Thermodynamics and heat transfer
 * - Fluid mechanics basics
 * - Machine element design
 * - Vibration analysis
 *
 * @author NIMCP Development Team
 * @date 2025-12-29
 * @version 1.0.0
 */

#ifndef NIMCP_MECHANICAL_ENGINEERING_H
#define NIMCP_MECHANICAL_ENGINEERING_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Maximum mesh nodes */
#define ME_MAX_NODES                    1024

/** Maximum mesh elements */
#define ME_MAX_ELEMENTS                 4096

/** Maximum degrees of freedom */
#define ME_MAX_DOF                      6

/** Bio-async module ID */
#define BIO_MODULE_MECHANICAL_ENG       0x0391

/* ============================================================================
 * TYPES
 * ============================================================================ */

/** Opaque handle for mechanical engineering processor */
typedef struct mechanical_eng mechanical_eng_t;

/**
 * @brief 3D vector
 */
typedef struct {
    float x, y, z;
} me_vec3_t;

/**
 * @brief 3x3 matrix (for stress/strain tensors)
 */
typedef struct {
    float m[3][3];
} me_mat3_t;

/**
 * @brief Material types
 */
typedef enum {
    ME_MATERIAL_STEEL,
    ME_MATERIAL_ALUMINUM,
    ME_MATERIAL_TITANIUM,
    ME_MATERIAL_COPPER,
    ME_MATERIAL_CONCRETE,
    ME_MATERIAL_WOOD,
    ME_MATERIAL_PLASTIC,
    ME_MATERIAL_COMPOSITE,
    ME_MATERIAL_CUSTOM
} me_material_type_t;

/**
 * @brief Element types
 */
typedef enum {
    ME_ELEMENT_BAR,                     /**< Axial load only */
    ME_ELEMENT_BEAM,                    /**< Bending and axial */
    ME_ELEMENT_PLATE,                   /**< 2D plate element */
    ME_ELEMENT_SHELL,                   /**< 3D shell element */
    ME_ELEMENT_SOLID,                   /**< 3D solid element */
    ME_ELEMENT_SPRING,                  /**< Spring element */
    ME_ELEMENT_DAMPER                   /**< Damper element */
} me_element_type_t;

/**
 * @brief Load types
 */
typedef enum {
    ME_LOAD_POINT_FORCE,
    ME_LOAD_DISTRIBUTED,
    ME_LOAD_MOMENT,
    ME_LOAD_PRESSURE,
    ME_LOAD_THERMAL,
    ME_LOAD_GRAVITY,
    ME_LOAD_CENTRIFUGAL,
    ME_LOAD_DYNAMIC
} me_load_type_t;

/**
 * @brief Constraint types
 */
typedef enum {
    ME_CONSTRAINT_FIXED,                /**< All DOF fixed */
    ME_CONSTRAINT_PINNED,               /**< Translation fixed, rotation free */
    ME_CONSTRAINT_ROLLER,               /**< One translation fixed */
    ME_CONSTRAINT_SLIDER,               /**< Can slide along axis */
    ME_CONSTRAINT_SYMMETRY,             /**< Symmetry boundary */
    ME_CONSTRAINT_CUSTOM                /**< Custom DOF constraints */
} me_constraint_type_t;

/**
 * @brief Failure criteria
 */
typedef enum {
    ME_FAILURE_VON_MISES,
    ME_FAILURE_TRESCA,
    ME_FAILURE_PRINCIPAL_STRESS,
    ME_FAILURE_MOHR_COULOMB,
    ME_FAILURE_DRUCKER_PRAGER,
    ME_FAILURE_TSAI_WU,                 /**< For composites */
    ME_FAILURE_FATIGUE
} me_failure_criterion_t;

/**
 * @brief Material properties
 */
typedef struct {
    me_material_type_t type;
    char name[64];

    /* Elastic properties */
    float elastic_modulus;              /**< Young's modulus (Pa) */
    float poisson_ratio;                /**< Poisson's ratio */
    float shear_modulus;                /**< Shear modulus (Pa) */
    float bulk_modulus;                 /**< Bulk modulus (Pa) */

    /* Strength */
    float yield_strength;               /**< Yield strength (Pa) */
    float ultimate_strength;            /**< Ultimate tensile strength (Pa) */
    float compressive_strength;         /**< Compressive strength (Pa) */
    float fatigue_limit;                /**< Endurance limit (Pa) */

    /* Physical properties */
    float density;                      /**< Density (kg/m^3) */
    float thermal_conductivity;         /**< W/(m*K) */
    float specific_heat;                /**< J/(kg*K) */
    float thermal_expansion;            /**< 1/K */

    /* For composites */
    bool is_anisotropic;
    me_mat3_t stiffness_matrix;         /**< Full stiffness matrix */
} me_material_t;

/**
 * @brief Mesh node
 */
typedef struct {
    uint32_t id;
    me_vec3_t position;
    me_vec3_t displacement;             /**< Computed displacement */
    bool constrained[ME_MAX_DOF];       /**< DOF constraints */
} me_node_t;

/**
 * @brief Mesh element
 */
typedef struct {
    uint32_t id;
    me_element_type_t type;
    uint32_t* node_ids;
    uint32_t num_nodes;
    const me_material_t* material;

    /* Geometry */
    float area;                         /**< Cross-sectional area */
    float moment_of_inertia_y;          /**< I_y for beams */
    float moment_of_inertia_z;          /**< I_z for beams */
    float thickness;                    /**< For shells/plates */
} me_element_t;

/**
 * @brief Load
 */
typedef struct {
    me_load_type_t type;
    uint32_t node_id;                   /**< Node for point loads */
    uint32_t element_id;                /**< Element for distributed */
    me_vec3_t force;                    /**< Force vector */
    me_vec3_t moment;                   /**< Moment vector */
    float magnitude;                    /**< Scalar magnitude */
    float temperature_delta;            /**< For thermal loads */
} me_load_t;

/**
 * @brief Stress result at a point
 */
typedef struct {
    me_mat3_t stress_tensor;            /**< Full stress tensor */
    float sigma_xx, sigma_yy, sigma_zz; /**< Normal stresses */
    float tau_xy, tau_xz, tau_yz;       /**< Shear stresses */
    float sigma_1, sigma_2, sigma_3;    /**< Principal stresses */
    float von_mises;                    /**< Von Mises equivalent stress */
    float tresca;                       /**< Tresca stress */
    float hydrostatic;                  /**< Hydrostatic stress */
    float deviatoric;                   /**< Deviatoric stress */
} me_stress_result_t;

/**
 * @brief Strain result at a point
 */
typedef struct {
    me_mat3_t strain_tensor;            /**< Full strain tensor */
    float epsilon_xx, epsilon_yy, epsilon_zz; /**< Normal strains */
    float gamma_xy, gamma_xz, gamma_yz; /**< Shear strains */
    float epsilon_1, epsilon_2, epsilon_3; /**< Principal strains */
    float volumetric;                   /**< Volumetric strain */
} me_strain_result_t;

/**
 * @brief Displacement result
 */
typedef struct {
    me_vec3_t translation;              /**< Translation vector */
    me_vec3_t rotation;                 /**< Rotation vector */
    float magnitude;                    /**< Total displacement magnitude */
} me_displacement_result_t;

/**
 * @brief Structural analysis result
 */
typedef struct {
    me_displacement_result_t* displacements; /**< Per-node displacements */
    me_stress_result_t* stresses;       /**< Per-element stresses */
    me_strain_result_t* strains;        /**< Per-element strains */
    uint32_t num_nodes;
    uint32_t num_elements;

    float max_displacement;
    float max_von_mises;
    float max_principal;
    float safety_factor;                /**< Minimum safety factor */
    bool converged;
} me_structural_result_t;

/**
 * @brief Vibration mode
 */
typedef struct {
    float frequency;                    /**< Natural frequency (Hz) */
    float* mode_shape;                  /**< Mode shape vector */
    float damping_ratio;                /**< Modal damping ratio */
    float modal_mass;                   /**< Modal mass */
    float participation_factor;         /**< Participation factor */
} me_vibration_mode_t;

/**
 * @brief Vibration analysis result
 */
typedef struct {
    me_vibration_mode_t* modes;
    uint32_t num_modes;
    float fundamental_frequency;
    float critical_speed;               /**< For rotating machinery */
} me_vibration_result_t;

/**
 * @brief Heat transfer result
 */
typedef struct {
    float* temperatures;                /**< Temperature at each node */
    me_vec3_t* heat_flux;               /**< Heat flux vectors */
    uint32_t num_nodes;
    float max_temperature;
    float min_temperature;
    float total_heat_flow;              /**< Total heat flow (W) */
} me_thermal_result_t;

/**
 * @brief Fatigue analysis result
 */
typedef struct {
    float* cycles_to_failure;           /**< Nf for each element */
    float* damage;                      /**< Accumulated damage */
    uint32_t num_elements;
    float min_life;                     /**< Minimum life (cycles) */
    float critical_location;            /**< Most critical element */
    float safety_factor;                /**< Fatigue safety factor */
} me_fatigue_result_t;

/**
 * @brief Mechanical engineering configuration
 */
typedef struct {
    me_failure_criterion_t failure_criterion;
    float safety_factor_target;         /**< Target safety factor */
    float convergence_tolerance;
    uint32_t max_iterations;
    bool include_geometric_nonlinearity;
    bool include_material_nonlinearity;
    uint32_t num_modes_to_compute;      /**< For vibration analysis */
    float ambient_temperature;
    bool enable_intuition;
    float inflammation_sensitivity;
    float fatigue_sensitivity;
} me_config_t;

/**
 * @brief Statistics
 */
typedef struct {
    uint64_t structural_analyses;
    uint64_t vibration_analyses;
    uint64_t thermal_analyses;
    uint64_t fatigue_analyses;
    float avg_processing_time_us;
} me_stats_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

mechanical_eng_t* mechanical_eng_create(void);
mechanical_eng_t* mechanical_eng_create_custom(const me_config_t* config);
void mechanical_eng_destroy(mechanical_eng_t* me);
me_config_t mechanical_eng_default_config(void);

/* ============================================================================
 * MATERIAL API
 * ============================================================================ */

/**
 * @brief Get predefined material properties
 */
int mechanical_eng_get_material(me_material_type_t type, me_material_t* material);

/**
 * @brief Create custom material
 */
int mechanical_eng_create_material(
    const char* name,
    float elastic_modulus,
    float poisson_ratio,
    float density,
    float yield_strength,
    me_material_t* material
);

/* ============================================================================
 * STRUCTURAL ANALYSIS API
 * ============================================================================ */

/**
 * @brief Perform linear static analysis
 */
int mechanical_eng_static_analysis(
    mechanical_eng_t* me,
    const me_node_t* nodes,
    uint32_t num_nodes,
    const me_element_t* elements,
    uint32_t num_elements,
    const me_load_t* loads,
    uint32_t num_loads,
    me_structural_result_t* result
);

/**
 * @brief Calculate stress from displacement
 */
int mechanical_eng_compute_stress(
    mechanical_eng_t* me,
    const me_element_t* element,
    const me_displacement_result_t* displacements,
    me_stress_result_t* stress
);

/**
 * @brief Evaluate failure criterion
 */
float mechanical_eng_evaluate_failure(
    mechanical_eng_t* me,
    const me_stress_result_t* stress,
    const me_material_t* material,
    me_failure_criterion_t criterion
);

/**
 * @brief Calculate beam deflection
 */
int mechanical_eng_beam_deflection(
    mechanical_eng_t* me,
    float length,
    float elastic_modulus,
    float moment_of_inertia,
    const me_load_t* loads,
    uint32_t num_loads,
    float* max_deflection,
    float* max_slope
);

/**
 * @brief Free structural result
 */
void mechanical_eng_free_structural_result(me_structural_result_t* result);

/* ============================================================================
 * VIBRATION ANALYSIS API
 * ============================================================================ */

/**
 * @brief Modal analysis (eigenvalue problem)
 */
int mechanical_eng_modal_analysis(
    mechanical_eng_t* me,
    const me_node_t* nodes,
    uint32_t num_nodes,
    const me_element_t* elements,
    uint32_t num_elements,
    uint32_t num_modes,
    me_vibration_result_t* result
);

/**
 * @brief Harmonic response analysis
 */
int mechanical_eng_harmonic_response(
    mechanical_eng_t* me,
    const me_vibration_result_t* modes,
    float excitation_freq,
    const me_load_t* excitation,
    float damping_ratio,
    me_displacement_result_t* response
);

/**
 * @brief Calculate natural frequency of simple systems
 */
float mechanical_eng_natural_frequency(
    mechanical_eng_t* me,
    float stiffness,
    float mass
);

/**
 * @brief Free vibration result
 */
void mechanical_eng_free_vibration_result(me_vibration_result_t* result);

/* ============================================================================
 * THERMAL ANALYSIS API
 * ============================================================================ */

/**
 * @brief Steady-state heat transfer analysis
 */
int mechanical_eng_thermal_steady(
    mechanical_eng_t* me,
    const me_node_t* nodes,
    uint32_t num_nodes,
    const me_element_t* elements,
    uint32_t num_elements,
    const float* temperatures_bc,       /**< Temperature BCs (NaN if free) */
    const float* heat_sources,          /**< Heat sources per element (W) */
    me_thermal_result_t* result
);

/**
 * @brief Heat conduction through wall
 */
float mechanical_eng_conduction_resistance(
    float thickness,
    float thermal_conductivity,
    float area
);

/**
 * @brief Convection heat transfer coefficient
 */
float mechanical_eng_convection_coefficient(
    mechanical_eng_t* me,
    float velocity,
    float characteristic_length,
    float fluid_thermal_conductivity,
    float fluid_viscosity,
    float fluid_prandtl
);

/**
 * @brief Free thermal result
 */
void mechanical_eng_free_thermal_result(me_thermal_result_t* result);

/* ============================================================================
 * FATIGUE ANALYSIS API
 * ============================================================================ */

/**
 * @brief Fatigue life analysis
 */
int mechanical_eng_fatigue_analysis(
    mechanical_eng_t* me,
    const me_stress_result_t* stresses,
    uint32_t num_elements,
    const me_material_t* material,
    float stress_ratio,                 /**< R = sigma_min / sigma_max */
    me_fatigue_result_t* result
);

/**
 * @brief S-N curve evaluation
 */
float mechanical_eng_sn_life(
    const me_material_t* material,
    float stress_amplitude
);

/**
 * @brief Free fatigue result
 */
void mechanical_eng_free_fatigue_result(me_fatigue_result_t* result);

/* ============================================================================
 * UTILITY API
 * ============================================================================ */

/**
 * @brief Calculate moment of inertia for common shapes
 */
float mechanical_eng_moment_of_inertia_rectangle(float width, float height);
float mechanical_eng_moment_of_inertia_circle(float diameter);
float mechanical_eng_moment_of_inertia_tube(float outer_d, float inner_d);

/**
 * @brief Section modulus
 */
float mechanical_eng_section_modulus(float moment_of_inertia, float c);

/**
 * @brief Stress concentration factor
 */
float mechanical_eng_stress_concentration(
    mechanical_eng_t* me,
    float notch_radius,
    float section_width,
    float notch_depth
);

/* ============================================================================
 * MODULATION API
 * ============================================================================ */

int mechanical_eng_set_inflammation(mechanical_eng_t* me, float level);
int mechanical_eng_set_fatigue(mechanical_eng_t* me, float level);

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

int mechanical_eng_get_stats(const mechanical_eng_t* me, me_stats_t* stats);
void mechanical_eng_reset_stats(mechanical_eng_t* me);
const char* mechanical_eng_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MECHANICAL_ENGINEERING_H */
