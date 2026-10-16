#ifndef NIMCP_DYNAMIC_ARCH_H
#define NIMCP_DYNAMIC_ARCH_H

/**
 * @file nimcp_dynamic_arch.h
 * @brief Dynamic Neural Architecture Search — monitor region utilisation
 *        and recommend grow / shrink actions.
 *
 * This module is advisory only: it tracks neuron activations per brain
 * region and produces recommendations.  Actual resizing is handled
 * externally (e.g. by the dynamic-synapse system).
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Configuration
 * ============================================================================ */

typedef struct {
    uint32_t monitor_interval;     /* Analyse every N steps (default: 1000) */
    uint32_t utilization_window;   /* Window size in steps (default: 500)   */
    float    grow_threshold;       /* Above → recommend growth (0.9)        */
    float    shrink_threshold;     /* Below → recommend shrinking (0.1)     */
    uint32_t max_recommendations;  /* Cap per analysis run (5)              */
} nimcp_dynamic_arch_config_t;

/* ============================================================================
 * Recommendation types
 * ============================================================================ */

typedef enum {
    NIMCP_ARCH_GROW   = 0,
    NIMCP_ARCH_SHRINK = 1,
    NIMCP_ARCH_NONE   = 2
} nimcp_arch_action_t;

typedef struct {
    char                region_name[64];
    nimcp_arch_action_t action;
    int32_t             suggested_delta;  /* +N to add, -N to remove */
    float               utilization;
} nimcp_arch_recommendation_t;

/* ============================================================================
 * Limits
 * ============================================================================ */

#define NIMCP_DYNARCH_MAX_REGIONS  64

/* Opaque handle */
typedef struct nimcp_dynamic_arch nimcp_dynamic_arch_t;

/* ============================================================================
 * API
 * ============================================================================ */

nimcp_dynamic_arch_config_t nimcp_dynamic_arch_config_default(void);

nimcp_dynamic_arch_t* nimcp_dynamic_arch_create(
    const nimcp_dynamic_arch_config_t* config);

void nimcp_dynamic_arch_destroy(nimcp_dynamic_arch_t* handle);

/**
 * @brief Register a brain region to monitor.
 * @return 0 on success, -1 if max regions reached or invalid args.
 */
int nimcp_dynamic_arch_register_region(
    nimcp_dynamic_arch_t* handle,
    const char* name,
    uint32_t neuron_start,
    uint32_t neuron_count);

/**
 * @brief Record a single neuron activation for a named region.
 */
int nimcp_dynamic_arch_record_activation(
    nimcp_dynamic_arch_t* handle,
    const char* region_name,
    uint32_t neuron_idx,
    float activation);

/**
 * @brief Compute utilisation per region and generate recommendations.
 * @return Number of recommendations generated, or -1 on error.
 */
int nimcp_dynamic_arch_analyze(nimcp_dynamic_arch_t* handle);

/**
 * @brief Retrieve the Nth recommendation from the last analysis.
 * @return 0 on success, -1 if idx is out of range.
 */
int nimcp_dynamic_arch_get_recommendation(
    const nimcp_dynamic_arch_t* handle,
    uint32_t idx,
    nimcp_arch_recommendation_t* recommendation_out);

/**
 * @brief Return current utilisation for a named region.
 * @return Utilisation [0,1], or -1.0f if the region is not found.
 */
float nimcp_dynamic_arch_get_utilization(
    const nimcp_dynamic_arch_t* handle,
    const char* region_name);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DYNAMIC_ARCH_H */
