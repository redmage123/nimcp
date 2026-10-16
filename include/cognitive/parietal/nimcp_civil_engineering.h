/**
 * @file nimcp_civil_engineering.h
 * @brief Civil engineering reasoning module for parietal lobe
 *
 * WHAT: Civil engineering analysis and reasoning capabilities
 * WHY:  Enable intelligent reasoning about structures, geotechnics, and infrastructure
 * HOW:  Domain-specific analysis with quantum optimization support
 *
 * CAPABILITIES:
 * - Structural analysis (beams, frames, trusses)
 * - Geotechnical analysis (soil mechanics, foundations)
 * - Hydraulics and hydrology
 * - Transportation engineering
 * - Construction management
 * - Infrastructure optimization
 *
 * QUANTUM INTEGRATION:
 * - Topology optimization via quantum annealing
 * - Scheduling optimization via QAOA
 * - Foundation design optimization
 *
 * @author NIMCP Development Team
 * @date 2025-12-29
 * @version 1.0.0
 */

#ifndef NIMCP_CIVIL_ENGINEERING_H
#define NIMCP_CIVIL_ENGINEERING_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Maximum structural nodes */
#define CE_MAX_NODES                    2048

/** Maximum structural members */
#define CE_MAX_MEMBERS                  4096

/** Maximum soil layers */
#define CE_MAX_SOIL_LAYERS              32

/** Bio-async module ID */
#define BIO_MODULE_CIVIL_ENG            0x0392

/* ============================================================================
 * TYPES
 * ============================================================================ */

/** Opaque handle for civil engineering processor */
typedef struct civil_eng civil_eng_t;

/**
 * @brief 2D/3D vector
 */
typedef struct {
    float x, y, z;
} ce_vec3_t;

/**
 * @brief Material types
 */
typedef enum {
    CE_MATERIAL_CONCRETE,
    CE_MATERIAL_STEEL,
    CE_MATERIAL_TIMBER,
    CE_MATERIAL_MASONRY,
    CE_MATERIAL_COMPOSITE,
    CE_MATERIAL_REBAR,
    CE_MATERIAL_PRESTRESSED
} ce_material_type_t;

/**
 * @brief Structural element types
 */
typedef enum {
    CE_ELEMENT_BEAM,
    CE_ELEMENT_COLUMN,
    CE_ELEMENT_TRUSS,
    CE_ELEMENT_SLAB,
    CE_ELEMENT_WALL,
    CE_ELEMENT_FOUNDATION,
    CE_ELEMENT_CABLE,
    CE_ELEMENT_ARCH
} ce_element_type_t;

/**
 * @brief Soil classification (USCS)
 */
typedef enum {
    CE_SOIL_GW,                         /**< Well-graded gravel */
    CE_SOIL_GP,                         /**< Poorly-graded gravel */
    CE_SOIL_SW,                         /**< Well-graded sand */
    CE_SOIL_SP,                         /**< Poorly-graded sand */
    CE_SOIL_SM,                         /**< Silty sand */
    CE_SOIL_SC,                         /**< Clayey sand */
    CE_SOIL_ML,                         /**< Low plasticity silt */
    CE_SOIL_CL,                         /**< Low plasticity clay */
    CE_SOIL_MH,                         /**< High plasticity silt */
    CE_SOIL_CH,                         /**< High plasticity clay */
    CE_SOIL_OL,                         /**< Organic silt */
    CE_SOIL_OH,                         /**< Organic clay */
    CE_SOIL_PT,                         /**< Peat */
    CE_SOIL_ROCK                        /**< Bedrock */
} ce_soil_type_t;

/**
 * @brief Foundation types
 */
typedef enum {
    CE_FOUND_SPREAD,                    /**< Spread/isolated footing */
    CE_FOUND_STRIP,                     /**< Strip/continuous footing */
    CE_FOUND_MAT,                       /**< Mat/raft foundation */
    CE_FOUND_PILE,                      /**< Pile foundation */
    CE_FOUND_DRILLED_SHAFT,             /**< Drilled shaft/caisson */
    CE_FOUND_MICROPILE                  /**< Micropile */
} ce_foundation_type_t;

/**
 * @brief Load combination types
 */
typedef enum {
    CE_LOAD_DEAD,                       /**< Dead load */
    CE_LOAD_LIVE,                       /**< Live load */
    CE_LOAD_WIND,                       /**< Wind load */
    CE_LOAD_SEISMIC,                    /**< Seismic load */
    CE_LOAD_SNOW,                       /**< Snow load */
    CE_LOAD_RAIN,                       /**< Rain load */
    CE_LOAD_THERMAL,                    /**< Thermal load */
    CE_LOAD_IMPACT                      /**< Impact load */
} ce_load_type_t;

/**
 * @brief Design code
 */
typedef enum {
    CE_CODE_ACI_318,                    /**< ACI 318 - Concrete */
    CE_CODE_AISC_360,                   /**< AISC 360 - Steel */
    CE_CODE_AASHTO,                     /**< AASHTO - Bridges */
    CE_CODE_IBC,                        /**< International Building Code */
    CE_CODE_EUROCODE                    /**< Eurocode */
} ce_design_code_t;

/**
 * @brief Material properties
 */
typedef struct {
    ce_material_type_t type;
    char name[64];
    float compressive_strength;         /**< f'c (Pa) */
    float tensile_strength;             /**< f't (Pa) */
    float yield_strength;               /**< fy (Pa) */
    float elastic_modulus;              /**< E (Pa) */
    float poisson_ratio;
    float density;                      /**< kg/m³ */
    float thermal_expansion;            /**< 1/K */
} ce_material_t;

/**
 * @brief Structural node
 */
typedef struct {
    uint32_t id;
    ce_vec3_t position;
    bool fixed[6];                      /**< DOF constraints (x,y,z,rx,ry,rz) */
    float settlement;                   /**< Support settlement */
} ce_node_t;

/**
 * @brief Structural member
 */
typedef struct {
    uint32_t id;
    ce_element_type_t type;
    uint32_t start_node;
    uint32_t end_node;
    const ce_material_t* material;
    float area;                         /**< Cross-sectional area (m²) */
    float moment_of_inertia_y;          /**< Iy (m⁴) */
    float moment_of_inertia_z;          /**< Iz (m⁴) */
    float torsional_constant;           /**< J (m⁴) */
    float width;                        /**< Section width */
    float depth;                        /**< Section depth */
} ce_member_t;

/**
 * @brief Applied load
 */
typedef struct {
    ce_load_type_t type;
    uint32_t node_id;                   /**< For point loads */
    uint32_t member_id;                 /**< For distributed loads */
    ce_vec3_t force;                    /**< Force vector */
    ce_vec3_t moment;                   /**< Moment vector */
    float magnitude;                    /**< For distributed loads (N/m) */
    float load_factor;                  /**< Load factor */
} ce_load_t;

/**
 * @brief Soil layer
 */
typedef struct {
    ce_soil_type_t type;
    float top_depth;                    /**< Depth to top of layer (m) */
    float thickness;                    /**< Layer thickness (m) */
    float unit_weight;                  /**< γ (kN/m³) */
    float friction_angle;               /**< φ (degrees) */
    float cohesion;                     /**< c (kPa) */
    float elastic_modulus;              /**< E (MPa) */
    float poisson_ratio;
    float spt_n;                        /**< SPT N-value */
    float water_content;                /**< % */
    float compression_index;            /**< Cc */
    float void_ratio;                   /**< e0 */
} ce_soil_layer_t;

/**
 * @brief Soil profile
 */
typedef struct {
    ce_soil_layer_t layers[CE_MAX_SOIL_LAYERS];
    uint32_t num_layers;
    float water_table_depth;            /**< Depth to water table (m) */
    float seismic_zone;                 /**< Seismic zone factor */
} ce_soil_profile_t;

/**
 * @brief Structural analysis result
 */
typedef struct {
    ce_vec3_t* displacements;           /**< Node displacements */
    ce_vec3_t* rotations;               /**< Node rotations */
    float* axial_forces;                /**< Member axial forces */
    float* shear_forces_y;              /**< Member shear Vy */
    float* shear_forces_z;              /**< Member shear Vz */
    float* bending_moments_y;           /**< Member moment My */
    float* bending_moments_z;           /**< Member moment Mz */
    float* torsion;                     /**< Member torsion */
    uint32_t num_nodes;
    uint32_t num_members;
    float max_displacement;
    float max_stress;
    float max_utilization;              /**< Max demand/capacity */
    bool is_stable;
    float weight;                       /**< Total structural weight */
} ce_structural_result_t;

/**
 * @brief Foundation analysis result
 */
typedef struct {
    float bearing_capacity;             /**< Ultimate bearing capacity (kPa) */
    float allowable_pressure;           /**< Allowable pressure (kPa) */
    float safety_factor;
    float immediate_settlement;         /**< Immediate settlement (mm) */
    float consolidation_settlement;     /**< Long-term settlement (mm) */
    float total_settlement;             /**< Total settlement (mm) */
    float differential_settlement;      /**< Differential settlement (mm) */
    float lateral_capacity;             /**< Lateral load capacity (kN) */
    float uplift_capacity;              /**< Uplift capacity (kN) */
    bool is_adequate;
    char recommendation[256];
} ce_foundation_result_t;

/**
 * @brief Hydraulic analysis result
 */
typedef struct {
    float* flow_rates;                  /**< Flow at each node (m³/s) */
    float* pressures;                   /**< Pressure at each node (Pa) */
    float* velocities;                  /**< Velocity in each pipe (m/s) */
    float* head_losses;                 /**< Head loss in each pipe (m) */
    uint32_t num_nodes;
    uint32_t num_pipes;
    float total_head_loss;
    float pump_power_required;          /**< Required pump power (kW) */
} ce_hydraulic_result_t;

/**
 * @brief Configuration
 */
typedef struct {
    ce_design_code_t code;
    float safety_factor_dead;
    float safety_factor_live;
    float safety_factor_wind;
    float safety_factor_seismic;
    float deflection_limit_ratio;       /**< L/limit (e.g., 360) */
    float settlement_limit;             /**< Max settlement (mm) */
    bool use_quantum_optimization;      /**< Enable quantum optimization */
    float inflammation_sensitivity;
    float fatigue_sensitivity;
} ce_config_t;

/**
 * @brief Statistics
 */
typedef struct {
    uint64_t structural_analyses;
    uint64_t foundation_analyses;
    uint64_t hydraulic_analyses;
    uint64_t optimizations;
    float avg_processing_time_us;
} ce_stats_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

civil_eng_t* civil_eng_create(void);
civil_eng_t* civil_eng_create_custom(const ce_config_t* config);
void civil_eng_destroy(civil_eng_t* ce);
ce_config_t civil_eng_default_config(void);

/* ============================================================================
 * MATERIAL API
 * ============================================================================ */

/**
 * @brief Get predefined material
 */
int civil_eng_get_material(ce_material_type_t type, ce_material_t* material);

/**
 * @brief Create concrete material with specified f'c
 */
int civil_eng_create_concrete(float fc_mpa, ce_material_t* material);

/**
 * @brief Create steel material with specified fy
 */
int civil_eng_create_steel(float fy_mpa, ce_material_t* material);

/* ============================================================================
 * STRUCTURAL ANALYSIS API
 * ============================================================================ */

/**
 * @brief Analyze 2D/3D frame structure
 */
int civil_eng_analyze_frame(
    civil_eng_t* ce,
    const ce_node_t* nodes,
    uint32_t num_nodes,
    const ce_member_t* members,
    uint32_t num_members,
    const ce_load_t* loads,
    uint32_t num_loads,
    ce_structural_result_t* result
);

/**
 * @brief Analyze truss structure
 */
int civil_eng_analyze_truss(
    civil_eng_t* ce,
    const ce_node_t* nodes,
    uint32_t num_nodes,
    const ce_member_t* members,
    uint32_t num_members,
    const ce_load_t* loads,
    uint32_t num_loads,
    ce_structural_result_t* result
);

/**
 * @brief Design concrete beam
 */
int civil_eng_design_beam(
    civil_eng_t* ce,
    float moment_kn_m,
    float shear_kn,
    float fc_mpa,
    float fy_mpa,
    float width_m,
    float* depth_m,
    float* as_mm2,
    float* av_s_mm2_m
);

/**
 * @brief Design concrete column
 */
int civil_eng_design_column(
    civil_eng_t* ce,
    float axial_kn,
    float moment_y_kn_m,
    float moment_z_kn_m,
    float fc_mpa,
    float fy_mpa,
    float* width_m,
    float* depth_m,
    float* as_mm2
);

/**
 * @brief Optimize structural topology (uses quantum if enabled)
 */
int civil_eng_topology_optimization(
    civil_eng_t* ce,
    float* domain,
    uint32_t nx, uint32_t ny, uint32_t nz,
    const ce_load_t* loads,
    uint32_t num_loads,
    float volume_fraction,
    float* optimized_density
);

/**
 * @brief Free structural result
 */
void civil_eng_free_structural_result(ce_structural_result_t* result);

/* ============================================================================
 * GEOTECHNICAL API
 * ============================================================================ */

/**
 * @brief Calculate bearing capacity
 */
int civil_eng_bearing_capacity(
    civil_eng_t* ce,
    const ce_soil_profile_t* soil,
    float foundation_width,
    float foundation_length,
    float foundation_depth,
    float applied_load,
    ce_foundation_result_t* result
);

/**
 * @brief Design pile foundation
 */
int civil_eng_design_pile(
    civil_eng_t* ce,
    const ce_soil_profile_t* soil,
    float pile_diameter,
    float pile_length,
    float applied_load,
    uint32_t* num_piles_required,
    ce_foundation_result_t* result
);

/**
 * @brief Calculate settlement
 */
int civil_eng_settlement(
    civil_eng_t* ce,
    const ce_soil_profile_t* soil,
    float foundation_width,
    float applied_pressure,
    float* immediate_mm,
    float* consolidation_mm
);

/**
 * @brief Slope stability analysis
 */
int civil_eng_slope_stability(
    civil_eng_t* ce,
    const ce_soil_profile_t* soil,
    float slope_height,
    float slope_angle,
    float* factor_of_safety
);

/**
 * @brief Earth pressure calculation (Rankine)
 */
int civil_eng_earth_pressure(
    civil_eng_t* ce,
    const ce_soil_layer_t* soil,
    float wall_height,
    float* active_pressure,
    float* passive_pressure,
    float* at_rest_pressure
);

/* ============================================================================
 * HYDRAULICS API
 * ============================================================================ */

/**
 * @brief Analyze pipe network (Hardy-Cross)
 */
int civil_eng_pipe_network(
    civil_eng_t* ce,
    const float* node_demands,
    const float* pipe_diameters,
    const float* pipe_lengths,
    const float* pipe_roughness,
    uint32_t num_nodes,
    uint32_t num_pipes,
    const uint32_t* pipe_connectivity,  /**< [2*num_pipes] start,end pairs */
    ce_hydraulic_result_t* result
);

/**
 * @brief Open channel flow (Manning's equation)
 */
int civil_eng_channel_flow(
    civil_eng_t* ce,
    float width,
    float depth,
    float slope,
    float manning_n,
    float* flow_rate,
    float* velocity
);

/**
 * @brief Stormwater runoff (Rational method)
 */
int civil_eng_rational_runoff(
    civil_eng_t* ce,
    float catchment_area_ha,
    float runoff_coefficient,
    float rainfall_intensity_mm_hr,
    float* peak_flow_m3_s
);

/**
 * @brief Free hydraulic result
 */
void civil_eng_free_hydraulic_result(ce_hydraulic_result_t* result);

/* ============================================================================
 * MODULATION API
 * ============================================================================ */

int civil_eng_set_inflammation(civil_eng_t* ce, float level);
int civil_eng_set_fatigue(civil_eng_t* ce, float level);

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

int civil_eng_get_stats(const civil_eng_t* ce, ce_stats_t* stats);
void civil_eng_reset_stats(civil_eng_t* ce);
const char* civil_eng_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CIVIL_ENGINEERING_H */
