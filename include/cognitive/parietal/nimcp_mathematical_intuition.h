/**
 * @file nimcp_mathematical_intuition.h
 * @brief Pattern recognition and mathematical intuition for parietal lobe
 *
 * Implements mathematical pattern recognition capabilities:
 * - Sequence pattern detection (arithmetic, geometric, Fibonacci, polynomial)
 * - Pattern extrapolation and prediction
 * - Geometric reasoning (parallel, perpendicular, congruent, similar)
 * - Symmetry detection (reflection, rotational)
 * - Mathematical analogies (a:b :: c:?)
 *
 * BIOLOGICAL BASIS:
 * The intraparietal sulcus (IPS) processes numerical and mathematical
 * relationships, with pattern detection emerging from neural ensemble
 * responses to sequential stimuli.
 *
 * USAGE:
 * ```c
 * math_intuition_t* mi = math_intuition_create();
 *
 * // Detect pattern in sequence
 * float seq[] = {2, 4, 6, 8};
 * detected_pattern_t pattern = math_detect_pattern(mi, seq, 4);
 *
 * // Predict next value
 * float next = math_extrapolate(mi, &pattern, 5);
 *
 * // Detect symmetry
 * symmetry_result_t sym = math_detect_symmetry(mi, points, num_points);
 *
 * math_intuition_destroy(mi);
 * ```
 */

#ifndef NIMCP_MATHEMATICAL_INTUITION_H
#define NIMCP_MATHEMATICAL_INTUITION_H

#include "utils/validation/nimcp_common.h"
#include "cognitive/parietal/nimcp_spatial_reasoning.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Maximum sequence length for pattern detection */
#define MATH_MAX_SEQUENCE_LENGTH        256

/** Maximum polynomial degree for pattern fitting */
#define MATH_MAX_POLYNOMIAL_DEGREE      5

/** Maximum points for symmetry detection */
#define MATH_MAX_SYMMETRY_POINTS        1000

/** Bio-async module ID for mathematical intuition */
#define BIO_MODULE_MATH_INTUITION       0x0383

/* ============================================================================
 * TYPES
 * ============================================================================ */

/** Opaque handle for mathematical intuition processor */
typedef struct math_intuition math_intuition_t;

/**
 * @brief Pattern types that can be detected
 */
typedef enum {
    PATTERN_UNKNOWN = 0,        /**< No pattern detected */
    PATTERN_CONSTANT,           /**< Constant sequence (a, a, a, ...) */
    PATTERN_ARITHMETIC,         /**< Arithmetic progression (a, a+d, a+2d, ...) */
    PATTERN_GEOMETRIC,          /**< Geometric progression (a, ar, ar^2, ...) */
    PATTERN_FIBONACCI,          /**< Fibonacci-like (a[n] = a[n-1] + a[n-2]) */
    PATTERN_POLYNOMIAL,         /**< Polynomial fit (degree 2-5) */
    PATTERN_EXPONENTIAL,        /**< Exponential growth/decay */
    PATTERN_LOGARITHMIC,        /**< Logarithmic pattern */
    PATTERN_OSCILLATORY,        /**< Periodic/oscillatory pattern */
    PATTERN_PRIME,              /**< Prime number sequence */
    PATTERN_SQUARE,             /**< Perfect squares (1, 4, 9, 16, ...) */
    PATTERN_TRIANGULAR,         /**< Triangular numbers (1, 3, 6, 10, ...) */
    PATTERN_CUSTOM              /**< Custom/hybrid pattern */
} pattern_type_t;

/**
 * @brief Detected pattern result
 */
typedef struct {
    pattern_type_t type;            /**< Detected pattern type */
    float confidence;               /**< Detection confidence [0,1] */
    float fit_error;                /**< Mean squared error of fit */

    /* Pattern parameters (interpretation depends on type) */
    union {
        struct {
            float value;            /**< Constant value */
        } constant;

        struct {
            float first_term;       /**< First term (a) */
            float difference;       /**< Common difference (d) */
        } arithmetic;

        struct {
            float first_term;       /**< First term (a) */
            float ratio;            /**< Common ratio (r) */
        } geometric;

        struct {
            float a0, a1;           /**< First two terms */
            float alpha, beta;      /**< Generalized coefficients */
        } fibonacci;

        struct {
            float coefficients[MATH_MAX_POLYNOMIAL_DEGREE + 1];
            uint8_t degree;         /**< Polynomial degree */
        } polynomial;

        struct {
            float amplitude;        /**< Oscillation amplitude */
            float frequency;        /**< Oscillation frequency */
            float phase;            /**< Phase offset */
            float offset;           /**< DC offset */
        } oscillatory;

        struct {
            float base;             /**< Exponential base */
            float coefficient;      /**< Coefficient */
            float offset;           /**< Offset */
        } exponential;
    } params;

    uint32_t sequence_length;       /**< Length of analyzed sequence */
} detected_pattern_t;

/**
 * @brief Geometric relationship types
 */
typedef enum {
    GEOM_RELATION_NONE = 0,
    GEOM_RELATION_PARALLEL,         /**< Lines are parallel */
    GEOM_RELATION_PERPENDICULAR,    /**< Lines are perpendicular */
    GEOM_RELATION_INTERSECTING,     /**< Lines intersect */
    GEOM_RELATION_CONGRUENT,        /**< Shapes are congruent */
    GEOM_RELATION_SIMILAR,          /**< Shapes are similar */
    GEOM_RELATION_TANGENT,          /**< Tangent relationship */
    GEOM_RELATION_INSCRIBED,        /**< Inscribed relationship */
} geometric_relation_t;

/**
 * @brief Geometric relationship result
 */
typedef struct {
    geometric_relation_t relation;  /**< Detected relationship */
    float confidence;               /**< Detection confidence [0,1] */
    float parameter;                /**< Relation-specific parameter */
    vec3_t intersection_point;      /**< Intersection point (if applicable) */
    float scale_factor;             /**< Scale factor (for similar shapes) */
    float angle;                    /**< Angle (for intersecting lines) */
} geometric_result_t;

/**
 * @brief Symmetry types
 */
typedef enum {
    SYMMETRY_NONE = 0,
    SYMMETRY_REFLECTION,            /**< Reflection symmetry */
    SYMMETRY_ROTATIONAL,            /**< Rotational symmetry */
    SYMMETRY_POINT,                 /**< Point/central symmetry */
    SYMMETRY_TRANSLATIONAL,         /**< Translational symmetry */
    SYMMETRY_GLIDE,                 /**< Glide reflection */
} symmetry_type_t;

/**
 * @brief Symmetry detection result
 */
typedef struct {
    symmetry_type_t type;           /**< Detected symmetry type */
    float confidence;               /**< Detection confidence [0,1] */

    /* Reflection symmetry */
    bool has_reflection;            /**< Has reflection symmetry */
    vec3_t reflection_axis;         /**< Axis of reflection */
    vec3_t reflection_point;        /**< Point on reflection axis */

    /* Rotational symmetry */
    bool has_rotation;              /**< Has rotational symmetry */
    uint32_t rotation_order;        /**< Order of rotation (e.g., 3 for 120°) */
    vec3_t rotation_center;         /**< Center of rotation */
    vec3_t rotation_axis;           /**< Axis of rotation (3D) */

    /* Point symmetry */
    bool has_point_symmetry;        /**< Has point/central symmetry */
    vec3_t symmetry_center;         /**< Center of point symmetry */
} symmetry_result_t;

/**
 * @brief Mathematical analogy result
 *
 * For analogies of form a:b :: c:?
 */
typedef struct {
    float answer;                   /**< Predicted answer (?) */
    float confidence;               /**< Confidence in prediction [0,1] */
    pattern_type_t relation_type;   /**< Type of relation detected */
    float relation_param;           /**< Relation parameter (e.g., ratio) */
} analogy_result_t;

/**
 * @brief Mathematical intuition configuration
 */
typedef struct {
    float pattern_confidence_threshold;  /**< Min confidence for pattern (0.7) */
    float symmetry_tolerance;            /**< Tolerance for symmetry detection (0.01) */
    uint8_t max_polynomial_degree;       /**< Max polynomial degree to try (5) */
    bool enable_oscillation_detection;   /**< Enable FFT-based oscillation (true) */
    bool enable_bio_async;               /**< Enable bio-async messaging (false) */

    /* Modulation */
    float inflammation_sensitivity;      /**< Inflammation effect (0-1) */
    float fatigue_sensitivity;           /**< Fatigue effect (0-1) */
} math_intuition_config_t;

/**
 * @brief Mathematical intuition statistics
 */
typedef struct {
    uint64_t patterns_detected;         /**< Total patterns detected */
    uint64_t symmetries_detected;       /**< Total symmetries detected */
    uint64_t analogies_solved;          /**< Total analogies solved */
    uint64_t geometric_analyses;        /**< Total geometric analyses */
    float avg_pattern_confidence;       /**< Average pattern confidence */
    float avg_symmetry_confidence;      /**< Average symmetry confidence */
} math_intuition_stats_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

/**
 * @brief Create mathematical intuition processor with default configuration
 * @return Handle or NULL on error
 */
math_intuition_t* math_intuition_create(void);

/**
 * @brief Create mathematical intuition processor with custom configuration
 * @param config Configuration (NULL for defaults)
 * @return Handle or NULL on error
 */
math_intuition_t* math_intuition_create_custom(const math_intuition_config_t* config);

/**
 * @brief Destroy mathematical intuition processor
 * @param mi Handle (NULL safe)
 */
void math_intuition_destroy(math_intuition_t* mi);

/**
 * @brief Get default configuration
 * @return Default configuration struct
 */
math_intuition_config_t math_intuition_default_config(void);

/**
 * @brief Validate configuration
 * @param config Configuration to validate
 * @return true if valid
 */
bool math_intuition_validate_config(const math_intuition_config_t* config);

/* ============================================================================
 * PATTERN DETECTION API
 * ============================================================================ */

/**
 * @brief Detect pattern in numerical sequence
 *
 * Analyzes sequence using finite differences, ratio analysis,
 * and polynomial fitting to identify underlying pattern.
 *
 * @param mi Mathematical intuition handle
 * @param sequence Input sequence
 * @param length Sequence length
 * @return Detected pattern
 */
detected_pattern_t math_detect_pattern(
    math_intuition_t* mi,
    const float* sequence,
    uint32_t length
);

/**
 * @brief Extrapolate pattern to predict next value
 *
 * @param mi Mathematical intuition handle
 * @param pattern Previously detected pattern
 * @param index Index of value to predict (0-based from pattern start)
 * @return Predicted value
 */
float math_extrapolate(
    math_intuition_t* mi,
    const detected_pattern_t* pattern,
    uint32_t index
);

/**
 * @brief Predict next N values in sequence
 *
 * @param mi Mathematical intuition handle
 * @param pattern Previously detected pattern
 * @param predictions Output array for predictions
 * @param num_predictions Number of predictions to make
 * @return Number of predictions made
 */
uint32_t math_predict_sequence(
    math_intuition_t* mi,
    const detected_pattern_t* pattern,
    float* predictions,
    uint32_t num_predictions
);

/**
 * @brief Check if value fits detected pattern
 *
 * @param mi Mathematical intuition handle
 * @param pattern Detected pattern
 * @param value Value to check
 * @param index Expected index of value
 * @return Fit score [0,1]
 */
float math_check_pattern_fit(
    math_intuition_t* mi,
    const detected_pattern_t* pattern,
    float value,
    uint32_t index
);

/**
 * @brief Get pattern type name
 *
 * @param type Pattern type
 * @return Human-readable name
 */
const char* math_pattern_type_name(pattern_type_t type);

/* ============================================================================
 * GEOMETRIC REASONING API
 * ============================================================================ */

/**
 * @brief Analyze geometric relationship between two line segments
 *
 * @param mi Mathematical intuition handle
 * @param line1_start Start of first line
 * @param line1_end End of first line
 * @param line2_start Start of second line
 * @param line2_end End of second line
 * @return Geometric relationship result
 */
geometric_result_t math_analyze_lines(
    math_intuition_t* mi,
    vec3_t line1_start, vec3_t line1_end,
    vec3_t line2_start, vec3_t line2_end
);

/**
 * @brief Check if two shapes are congruent
 *
 * @param mi Mathematical intuition handle
 * @param shape1 First shape vertices
 * @param num_vertices1 Number of vertices in first shape
 * @param shape2 Second shape vertices
 * @param num_vertices2 Number of vertices in second shape
 * @return Congruence result
 */
geometric_result_t math_check_congruent(
    math_intuition_t* mi,
    const vec3_t* shape1, uint32_t num_vertices1,
    const vec3_t* shape2, uint32_t num_vertices2
);

/**
 * @brief Check if two shapes are similar
 *
 * @param mi Mathematical intuition handle
 * @param shape1 First shape vertices
 * @param num_vertices1 Number of vertices in first shape
 * @param shape2 Second shape vertices
 * @param num_vertices2 Number of vertices in second shape
 * @return Similarity result (includes scale factor)
 */
geometric_result_t math_check_similar(
    math_intuition_t* mi,
    const vec3_t* shape1, uint32_t num_vertices1,
    const vec3_t* shape2, uint32_t num_vertices2
);

/* ============================================================================
 * SYMMETRY DETECTION API
 * ============================================================================ */

/**
 * @brief Detect symmetry in point set
 *
 * @param mi Mathematical intuition handle
 * @param points Array of points
 * @param num_points Number of points
 * @return Symmetry detection result
 */
symmetry_result_t math_detect_symmetry(
    math_intuition_t* mi,
    const vec3_t* points,
    uint32_t num_points
);

/**
 * @brief Check for specific symmetry type
 *
 * @param mi Mathematical intuition handle
 * @param points Array of points
 * @param num_points Number of points
 * @param type Symmetry type to check
 * @return Confidence in symmetry [0,1]
 */
float math_check_symmetry_type(
    math_intuition_t* mi,
    const vec3_t* points,
    uint32_t num_points,
    symmetry_type_t type
);

/**
 * @brief Find reflection axis for point set
 *
 * @param mi Mathematical intuition handle
 * @param points Array of points
 * @param num_points Number of points
 * @param axis_point Output: point on axis
 * @param axis_direction Output: axis direction
 * @return Confidence in reflection symmetry [0,1]
 */
float math_find_reflection_axis(
    math_intuition_t* mi,
    const vec3_t* points,
    uint32_t num_points,
    vec3_t* axis_point,
    vec3_t* axis_direction
);

/**
 * @brief Find rotation center and order
 *
 * @param mi Mathematical intuition handle
 * @param points Array of points
 * @param num_points Number of points
 * @param center Output: center of rotation
 * @param order Output: order of rotation
 * @return Confidence in rotational symmetry [0,1]
 */
float math_find_rotation_symmetry(
    math_intuition_t* mi,
    const vec3_t* points,
    uint32_t num_points,
    vec3_t* center,
    uint32_t* order
);

/* ============================================================================
 * ANALOGY API
 * ============================================================================ */

/**
 * @brief Solve mathematical analogy (a:b :: c:?)
 *
 * @param mi Mathematical intuition handle
 * @param a First term
 * @param b Second term (relates to a)
 * @param c Third term
 * @return Analogy result with predicted answer
 */
analogy_result_t math_solve_analogy(
    math_intuition_t* mi,
    float a, float b, float c
);

/**
 * @brief Check if analogy is valid
 *
 * @param mi Mathematical intuition handle
 * @param a First term
 * @param b Second term
 * @param c Third term
 * @param d Fourth term
 * @return Validity score [0,1]
 */
float math_check_analogy(
    math_intuition_t* mi,
    float a, float b, float c, float d
);

/* ============================================================================
 * MODULATION API
 * ============================================================================ */

/**
 * @brief Set inflammation level
 *
 * @param mi Mathematical intuition handle
 * @param level Inflammation level [0,1]
 * @return 0 on success
 */
int math_intuition_set_inflammation(
    math_intuition_t* mi,
    float level
);

/**
 * @brief Set fatigue level
 *
 * @param mi Mathematical intuition handle
 * @param level Fatigue level [0,1]
 * @return 0 on success
 */
int math_intuition_set_fatigue(
    math_intuition_t* mi,
    float level
);

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

/**
 * @brief Get statistics
 *
 * @param mi Mathematical intuition handle
 * @param stats Output statistics
 * @return 0 on success
 */
int math_intuition_get_stats(
    math_intuition_t* mi,
    math_intuition_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param mi Mathematical intuition handle
 */
void math_intuition_reset_stats(math_intuition_t* mi);

/**
 * @brief Get last error message
 * @return Thread-local error message
 */
const char* math_intuition_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MATHEMATICAL_INTUITION_H */
