/**
 * @file nimcp_number_sense.h
 * @brief Approximate Number System (ANS) implementation for parietal lobe
 *
 * Implements the biological Approximate Number System with:
 * - Weber-Fechner law (JND = k * magnitude, k ≈ 0.15)
 * - Subitizing (instant recognition of 1-4 items)
 * - Approximate arithmetic intuition
 * - Order of magnitude estimation
 *
 * BIOLOGICAL BASIS:
 * The intraparietal sulcus (IPS) contains neurons that respond to numerical
 * magnitudes with tuning curves that follow Weber's law - discrimination
 * accuracy is proportional to the ratio between quantities, not absolute
 * difference.
 *
 * USAGE:
 * ```c
 * number_sense_t* ns = number_sense_create();
 *
 * // Estimate quantity from visual input
 * number_estimate_t est = number_sense_estimate(ns, dot_array, num_dots);
 *
 * // Compare two quantities
 * number_comparison_t cmp = number_sense_compare(ns, 7.0f, 9.0f);
 *
 * number_sense_destroy(ns);
 * ```
 */

#ifndef NIMCP_NUMBER_SENSE_H
#define NIMCP_NUMBER_SENSE_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Default Weber fraction (typical adult human) */
#define NUMBER_SENSE_DEFAULT_WEBER_FRACTION     0.15f

/** Subitizing limit (instant recognition without counting) */
#define NUMBER_SENSE_SUBITIZING_LIMIT           4

/** Maximum number for reliable estimation */
#define NUMBER_SENSE_MAX_MAGNITUDE              10000.0f

/** Minimum magnitude for estimation (avoid log(0)) */
#define NUMBER_SENSE_MIN_MAGNITUDE              0.001f

/** Bio-async module ID for number sense */
#define BIO_MODULE_NUMBER_SENSE                 0x0381

/* ============================================================================
 * TYPES
 * ============================================================================ */

/** Opaque handle for number sense processor */
typedef struct number_sense number_sense_t;

/**
 * @brief Number sense configuration
 */
typedef struct {
    float weber_fraction;           /**< Weber fraction (default: 0.15) */
    uint32_t subitizing_limit;      /**< Instant recognition limit (default: 4) */
    float estimation_noise;         /**< Gaussian noise sigma (default: 0.1) */
    bool enable_logarithmic_scale;  /**< Use logarithmic mental number line (default: true) */
    bool enable_subitizing;         /**< Enable subitizing for small numbers (default: true) */
    bool enable_bio_async;          /**< Enable bio-async messaging (default: false) */

    /** Immune integration (optional) */
    float inflammation_sensitivity; /**< How much inflammation affects precision (0-1) */

    /** Sleep modulation (optional) */
    float sleep_deprivation_factor; /**< Precision degradation from sleep loss (0-1) */
} number_sense_config_t;

/**
 * @brief Result of number estimation
 *
 * Mental representation of a numerical magnitude with uncertainty
 */
typedef struct {
    float magnitude;                /**< Estimated magnitude */
    float uncertainty;              /**< Estimation uncertainty (from Weber law) */
    float confidence;               /**< Confidence in estimate [0,1] */
    bool is_subitized;              /**< Was this instantly perceived (1-4 items)? */
    uint64_t processing_time_us;    /**< Processing time in microseconds */
} number_estimate_t;

/**
 * @brief Result of numerical comparison
 */
typedef struct {
    int direction;                  /**< -1: a<b, 0: uncertain, 1: a>b */
    float confidence;               /**< Confidence in comparison [0,1] */
    float perceived_ratio;          /**< Perceived ratio a/b */
    float discriminability;         /**< d' (d-prime) for this comparison */
} number_comparison_t;

/**
 * @brief Approximate arithmetic result
 */
typedef struct {
    float result;                   /**< Approximate result */
    float uncertainty;              /**< Uncertainty in result */
    float confidence;               /**< Confidence [0,1] */
} approx_arithmetic_t;

/**
 * @brief Number sense statistics
 */
typedef struct {
    uint64_t estimates_performed;   /**< Total estimation operations */
    uint64_t subitizing_count;      /**< Times subitizing was used */
    uint64_t comparisons_performed; /**< Total comparison operations */
    uint64_t arithmetic_operations; /**< Total approx arithmetic ops */
    float avg_estimation_error;     /**< Average estimation error (if known) */
    float avg_processing_time_us;   /**< Average processing time */
    float current_weber_fraction;   /**< Current effective Weber fraction */
} number_sense_stats_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

/**
 * @brief Create number sense processor with default configuration
 *
 * @return Handle or NULL on error
 */
number_sense_t* number_sense_create(void);

/**
 * @brief Create number sense processor with custom configuration
 *
 * @param config Configuration (NULL for defaults)
 * @return Handle or NULL on error
 */
number_sense_t* number_sense_create_custom(const number_sense_config_t* config);

/**
 * @brief Destroy number sense processor
 *
 * @param ns Handle (NULL safe)
 */
void number_sense_destroy(number_sense_t* ns);

/**
 * @brief Get default configuration
 *
 * @return Default configuration struct
 */
number_sense_config_t number_sense_default_config(void);

/**
 * @brief Validate configuration
 *
 * @param config Configuration to validate
 * @return true if valid
 */
bool number_sense_validate_config(const number_sense_config_t* config);

/* ============================================================================
 * ESTIMATION API
 * ============================================================================ */

/**
 * @brief Estimate numerical magnitude from input representation
 *
 * Uses the Approximate Number System (ANS) with Weber-Fechner scaling.
 * For small quantities (1-4), uses subitizing for instant recognition.
 *
 * @param ns Number sense handle
 * @param input Input representation (e.g., dot array activations)
 * @param input_size Size of input array
 * @return Estimation result
 */
number_estimate_t number_sense_estimate(
    number_sense_t* ns,
    const float* input,
    uint32_t input_size
);

/**
 * @brief Estimate from a single magnitude value
 *
 * Adds Weber-law noise to a known magnitude to simulate
 * internal number representation.
 *
 * @param ns Number sense handle
 * @param actual_magnitude Known magnitude
 * @return Estimated representation
 */
number_estimate_t number_sense_estimate_from_magnitude(
    number_sense_t* ns,
    float actual_magnitude
);

/**
 * @brief Subitize small quantity (1-4 items)
 *
 * Fast, parallel recognition of small quantities without counting.
 * Returns high confidence for 1-4 items, degrades for larger.
 *
 * @param ns Number sense handle
 * @param input Input representation
 * @param input_size Size of input
 * @return Subitizing result
 */
number_estimate_t number_sense_subitize(
    number_sense_t* ns,
    const float* input,
    uint32_t input_size
);

/* ============================================================================
 * COMPARISON API
 * ============================================================================ */

/**
 * @brief Compare two quantities
 *
 * Comparison accuracy follows Weber's law - easier to discriminate
 * quantities with larger ratios (e.g., 1 vs 2 easier than 7 vs 8).
 *
 * @param ns Number sense handle
 * @param magnitude_a First magnitude
 * @param magnitude_b Second magnitude
 * @return Comparison result
 */
number_comparison_t number_sense_compare(
    number_sense_t* ns,
    float magnitude_a,
    float magnitude_b
);

/**
 * @brief Compute Weber fraction for given magnitude
 *
 * Returns the just-noticeable difference as a fraction of magnitude.
 *
 * @param ns Number sense handle
 * @param magnitude Magnitude to compute Weber fraction for
 * @return Weber fraction (typically ~0.15 for adults)
 */
float number_sense_get_weber_fraction(
    const number_sense_t* ns,
    float magnitude
);

/**
 * @brief Compute discriminability (d') between two magnitudes
 *
 * Higher d' means easier discrimination.
 *
 * @param ns Number sense handle
 * @param magnitude_a First magnitude
 * @param magnitude_b Second magnitude
 * @return d' value
 */
float number_sense_discriminability(
    const number_sense_t* ns,
    float magnitude_a,
    float magnitude_b
);

/* ============================================================================
 * APPROXIMATE ARITHMETIC API
 * ============================================================================ */

/**
 * @brief Approximate addition
 *
 * Models intuitive addition with uncertainty propagation.
 *
 * @param ns Number sense handle
 * @param a First operand
 * @param b Second operand
 * @return Approximate sum
 */
approx_arithmetic_t number_sense_approximate_add(
    number_sense_t* ns,
    float a,
    float b
);

/**
 * @brief Approximate subtraction
 *
 * @param ns Number sense handle
 * @param a First operand (minuend)
 * @param b Second operand (subtrahend)
 * @return Approximate difference
 */
approx_arithmetic_t number_sense_approximate_sub(
    number_sense_t* ns,
    float a,
    float b
);

/**
 * @brief Approximate multiplication
 *
 * @param ns Number sense handle
 * @param a First operand
 * @param b Second operand
 * @return Approximate product
 */
approx_arithmetic_t number_sense_approximate_mul(
    number_sense_t* ns,
    float a,
    float b
);

/**
 * @brief Approximate division
 *
 * @param ns Number sense handle
 * @param a Dividend
 * @param b Divisor (must be non-zero)
 * @return Approximate quotient
 */
approx_arithmetic_t number_sense_approximate_div(
    number_sense_t* ns,
    float a,
    float b
);

/**
 * @brief Get order of magnitude
 *
 * Returns floor(log10(value)) with noise based on Weber fraction.
 *
 * @param ns Number sense handle
 * @param value Value to estimate
 * @return Order of magnitude (power of 10)
 */
int number_sense_order_of_magnitude(
    number_sense_t* ns,
    float value
);

/* ============================================================================
 * MODULATION API
 * ============================================================================ */

/**
 * @brief Set inflammation level for immune modulation
 *
 * High inflammation degrades number sense precision.
 *
 * @param ns Number sense handle
 * @param level Inflammation level [0,1]
 * @return 0 on success
 */
int number_sense_set_inflammation(
    number_sense_t* ns,
    float level
);

/**
 * @brief Set sleep deprivation level
 *
 * Sleep deprivation increases Weber fraction.
 *
 * @param ns Number sense handle
 * @param level Deprivation level [0,1] (0=rested, 1=severely deprived)
 * @return 0 on success
 */
int number_sense_set_sleep_deprivation(
    number_sense_t* ns,
    float level
);

/**
 * @brief Get effective Weber fraction after modulation
 *
 * Returns Weber fraction adjusted for inflammation and sleep.
 *
 * @param ns Number sense handle
 * @return Effective Weber fraction
 */
float number_sense_get_effective_weber_fraction(const number_sense_t* ns);

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

/**
 * @brief Get statistics
 *
 * @param ns Number sense handle
 * @param stats Output statistics
 * @return 0 on success
 */
int number_sense_get_stats(
    const number_sense_t* ns,
    number_sense_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param ns Number sense handle
 */
void number_sense_reset_stats(number_sense_t* ns);

/**
 * @brief Get last error message
 *
 * @return Thread-local error message
 */
const char* number_sense_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NUMBER_SENSE_H */
