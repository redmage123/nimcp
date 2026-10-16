/**
 * @file nimcp_temporal_discounting.h
 * @brief Temporal discounting system for Raphe Nuclei
 * @date 2026-01-11
 *
 * Models the serotonergic contribution to intertemporal choice:
 * - Hyperbolic discounting of delayed rewards
 * - 5-HT modulation of patience for future rewards
 * - Future orientation vs immediate gratification
 * - Integration with impulse control
 *
 * Higher 5-HT levels promote:
 * - Waiting for larger delayed rewards
 * - Lower discount rates (more patient)
 * - Better future planning
 */

#ifndef NIMCP_TEMPORAL_DISCOUNTING_H
#define NIMCP_TEMPORAL_DISCOUNTING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/*=============================================================================
 * Constants
 *===========================================================================*/

#define TEMPORAL_DEFAULT_K            0.1f    /* Hyperbolic discount rate */
#define TEMPORAL_DEFAULT_ORIENTATION  0.5f    /* Future orientation */
#define TEMPORAL_5HT_DISCOUNT_GAIN    0.5f    /* 5-HT -> discount modulation */

/*=============================================================================
 * Structures
 *===========================================================================*/

/**
 * @brief Temporal discounting configuration
 */
typedef struct {
    float baseline_k;               /**< Baseline discount rate */
    float baseline_orientation;     /**< Baseline future orientation */
    float ht_discount_gain;         /**< 5-HT -> discount rate gain */
    float ht_orientation_gain;      /**< 5-HT -> orientation gain */
    float min_k;                    /**< Minimum discount rate */
    float max_k;                    /**< Maximum discount rate */
} nimcp_temporal_config_t;

/**
 * @brief Intertemporal choice result
 */
typedef struct {
    bool prefer_delayed;            /**< True = choose delayed reward */
    float discounted_value;         /**< Subjective value of delayed */
    float effective_k;              /**< Current discount rate used */
    float indifference_delay;       /**< Delay where options are equal */
} nimcp_temporal_choice_t;

/**
 * @brief Temporal discounting system
 */
typedef struct {
    bool initialized;

    /* State */
    float discount_rate;            /**< Current k value (lower = patient) */
    float future_orientation;       /**< Long-term thinking [0-1] */
    float delay_tolerance;          /**< Capacity for waiting [0-1] */

    /* 5-HT state */
    float current_5ht;
    float baseline_5ht;

    /* Statistics */
    uint32_t choices_made;
    uint32_t delayed_chosen;
    uint32_t immediate_chosen;
    float avg_k;

    /* Configuration */
    nimcp_temporal_config_t config;

} nimcp_temporal_system_t;

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

int nimcp_temporal_init(nimcp_temporal_system_t* system, const nimcp_temporal_config_t* config);
int nimcp_temporal_shutdown(nimcp_temporal_system_t* system);
int nimcp_temporal_reset(nimcp_temporal_system_t* system);
nimcp_temporal_config_t nimcp_temporal_default_config(void);

/*=============================================================================
 * Update API
 *===========================================================================*/

int nimcp_temporal_update(nimcp_temporal_system_t* system, float ht_level, float dt);

/*=============================================================================
 * Discounting API
 *===========================================================================*/

/**
 * @brief Compute discounted value of delayed reward
 * Uses hyperbolic discounting: V = A / (1 + k*D)
 */
int nimcp_temporal_discount_value(
    nimcp_temporal_system_t* system,
    float value,
    float delay,
    float* discounted_value
);

/**
 * @brief Compute discount rate for current 5-HT state
 */
int nimcp_temporal_get_current_k(
    nimcp_temporal_system_t* system,
    float* k
);

/**
 * @brief Evaluate intertemporal choice
 * @param system Temporal system
 * @param immediate_value Value of immediate reward
 * @param delayed_value Value of delayed reward
 * @param delay Delay to delayed reward (ms)
 * @param result Output choice result
 */
int nimcp_temporal_evaluate_choice(
    nimcp_temporal_system_t* system,
    float immediate_value,
    float delayed_value,
    float delay,
    nimcp_temporal_choice_t* result
);

/**
 * @brief Find indifference delay
 * Delay at which immediate and delayed rewards have equal subjective value
 */
int nimcp_temporal_find_indifference(
    nimcp_temporal_system_t* system,
    float immediate_value,
    float delayed_value,
    float* indifference_delay
);

/*=============================================================================
 * Query API
 *===========================================================================*/

int nimcp_temporal_get_discount_rate(nimcp_temporal_system_t* system, float* rate);
int nimcp_temporal_get_future_orientation(nimcp_temporal_system_t* system, float* orientation);
int nimcp_temporal_get_delay_tolerance(nimcp_temporal_system_t* system, float* tolerance);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TEMPORAL_DISCOUNTING_H */
