/**
 * @file nimcp_reasoning_calibration.c
 * @brief Confidence Calibration Learning — Implementation
 *
 * WHAT: Online calibration of per-contributor confidence accuracy
 * WHY:  Learn which reasoning contributors are reliable and adjust their
 *       confidence scaling to improve overall reasoning quality
 * HOW:  Per-contributor EMA error tracking with bounded scale/bias adjustment
 *
 * ALGORITHM:
 *   On each record(contributor, predicted, actual):
 *     1. Find or create contributor entry (linear scan, N <= 128)
 *     2. total_predictions++
 *     3. if actual > 0.5: correct_predictions++
 *     4. error = |predicted - actual|
 *     5. ema_error = (1 - lr) * ema_error + lr * error
 *     6. reliability = correct / total
 *     7. After min_predictions_before_adjust:
 *        scale = clamp(1.0 - ema_error, min_scale, max_scale)
 *        bias += lr * (actual - predicted) / total (accumulated)
 *
 * @version 1.0.0
 * @date 2026-02-26
 */

#include "cognitive/reasoning/nimcp_reasoning_calibration.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>
#include <math.h>

#define LOG_MODULE "reasoning_calibration"

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Internal calibration system state
 *
 * WHAT: Holds per-contributor calibration data and configuration
 * WHY:  Opaque struct encapsulates state from callers
 */
struct reasoning_calibration {
    calibration_config_t config;
    contributor_calibration_t contributors[REASONING_MAX_CALIBRATED_CONTRIBUTORS];
    uint32_t num_contributors;
    uint32_t total_records;
    nimcp_mutex_t* mutex;
};

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Find a contributor by name (linear scan)
 *
 * @param cal Calibration system (must be non-NULL, caller ensures)
 * @param name Contributor name
 * @return Index into contributors array, or -1 if not found
 */
static int find_contributor(const reasoning_calibration_t* cal, const char* name)
{
    for (uint32_t i = 0; i < cal->num_contributors; i++) {
        if (strncmp(cal->contributors[i].contributor_name, name,
                    REASONING_CALIBRATION_NAME_LEN) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/**
 * @brief Find or create a contributor entry
 *
 * @param cal Calibration system
 * @param name Contributor name
 * @return Index into contributors array, or -1 if full
 */
static int find_or_create_contributor(reasoning_calibration_t* cal, const char* name)
{
    int idx = find_contributor(cal, name);
    if (idx >= 0) return idx;

    /* Create new entry */
    if (cal->num_contributors >= REASONING_MAX_CALIBRATED_CONTRIBUTORS) {
        NIMCP_LOGGING_WARN("calibration: contributor table full (%u), "
                           "cannot track '%s'",
                           cal->num_contributors, name);
        return -1;
    }

    idx = (int)cal->num_contributors;
    contributor_calibration_t* entry = &cal->contributors[idx];
    memset(entry, 0, sizeof(contributor_calibration_t));

    strncpy(entry->contributor_name, name, REASONING_CALIBRATION_NAME_LEN - 1);
    entry->contributor_name[REASONING_CALIBRATION_NAME_LEN - 1] = '\0';

    entry->confidence_scale = 1.0f;
    entry->confidence_bias = 0.0f;
    entry->ema_error = 0.5f;  /* Start at 50% error (uncertain) */
    entry->reliability_score = 0.0f;
    entry->total_predictions = 0;
    entry->correct_predictions = 0;

    cal->num_contributors++;
    return idx;
}

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

calibration_config_t reasoning_calibration_default_config(void)
{
    calibration_config_t config;
    memset(&config, 0, sizeof(config));

    config.enabled = true;
    config.learning_rate = REASONING_DEFAULT_CALIBRATION_LEARNING_RATE;
    config.history_size = REASONING_DEFAULT_CALIBRATION_HISTORY;
    config.min_predictions_before_adjust = 5;
    config.max_scale = 2.0f;
    config.min_scale = 0.1f;

    return config;
}

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

reasoning_calibration_t* reasoning_calibration_create(
    const calibration_config_t* config)
{
    reasoning_calibration_t* cal = (reasoning_calibration_t*)nimcp_calloc(
        1, sizeof(reasoning_calibration_t));
    if (!cal) {
        NIMCP_LOGGING_ERROR("calibration: failed to allocate calibration system");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        cal->config = *config;
    } else {
        cal->config = reasoning_calibration_default_config();
    }

    /* Initialize mutex for thread safety */
    mutex_attr_t attr;
    memset(&attr, 0, sizeof(attr));
    attr.type = MUTEX_TYPE_NORMAL;
    cal->mutex = nimcp_mutex_create(&attr);
    if (!cal->mutex) {
        NIMCP_LOGGING_ERROR("calibration: failed to create mutex");
        nimcp_free(cal);
        cal = NULL;
        return NULL;
    }

    cal->num_contributors = 0;
    cal->total_records = 0;

    NIMCP_LOGGING_INFO("calibration: created (lr=%.3f, min_pred=%u, "
                       "scale=[%.2f, %.2f])",
                       (double)cal->config.learning_rate,
                       cal->config.min_predictions_before_adjust,
                       (double)cal->config.min_scale,
                       (double)cal->config.max_scale);

    return cal;
}

void reasoning_calibration_destroy(reasoning_calibration_t* cal)
{
    if (!cal) return;

    if (cal->mutex) {
        nimcp_mutex_destroy(cal->mutex);
        cal->mutex = NULL;
    }

    NIMCP_LOGGING_INFO("calibration: destroyed (contributors=%u, records=%u)",
                       cal->num_contributors, cal->total_records);

    nimcp_free(cal);
    cal = NULL;
}

/*=============================================================================
 * CORE OPERATIONS
 *===========================================================================*/

int reasoning_calibration_record(reasoning_calibration_t* cal,
                                  const char* contributor_name,
                                  float predicted_confidence,
                                  float actual_outcome)
{
    if (!cal || !contributor_name) return -1;

    nimcp_mutex_lock(cal->mutex);

    int idx = find_or_create_contributor(cal, contributor_name);
    if (idx < 0) {
        nimcp_mutex_unlock(cal->mutex);
        return -1;
    }

    contributor_calibration_t* entry = &cal->contributors[idx];
    float lr = cal->config.learning_rate;

    /* Update prediction counts */
    entry->total_predictions++;
    if (actual_outcome > 0.5f) {
        entry->correct_predictions++;
    }

    /* Compute error and update EMA */
    float error = fabsf(predicted_confidence - actual_outcome);
    entry->ema_error = (1.0f - lr) * entry->ema_error + lr * error;

    /* Update reliability score */
    entry->reliability_score = (float)entry->correct_predictions /
                               (float)entry->total_predictions;

    /* Update scale and bias after minimum observations */
    if (entry->total_predictions >= cal->config.min_predictions_before_adjust) {
        /* Scale: reduce proportionally to EMA error */
        float new_scale = 1.0f - entry->ema_error;
        if (new_scale < cal->config.min_scale) {
            new_scale = cal->config.min_scale;
        }
        if (new_scale > cal->config.max_scale) {
            new_scale = cal->config.max_scale;
        }
        entry->confidence_scale = new_scale;

        /* Bias: accumulated correction towards actual outcomes */
        float bias_delta = lr * (actual_outcome - predicted_confidence);
        entry->confidence_bias += bias_delta;
    }

    cal->total_records++;

    nimcp_mutex_unlock(cal->mutex);
    return 0;
}

int reasoning_calibration_get_adjustment(const reasoning_calibration_t* cal,
                                          const char* contributor_name,
                                          float* scale_out,
                                          float* bias_out)
{
    if (!cal || !contributor_name) return -1;

    /* Default values for unknown contributors */
    float scale = 1.0f;
    float bias = 0.0f;

    nimcp_mutex_lock(cal->mutex);

    int idx = find_contributor(cal, contributor_name);
    if (idx >= 0) {
        const contributor_calibration_t* entry = &cal->contributors[idx];
        scale = entry->confidence_scale;
        bias = entry->confidence_bias;
    }

    nimcp_mutex_unlock(cal->mutex);

    if (scale_out) *scale_out = scale;
    if (bias_out) *bias_out = bias;

    return 0;
}

int reasoning_calibration_get_contributor_stats(
    const reasoning_calibration_t* cal,
    const char* contributor_name,
    contributor_calibration_t* out)
{
    if (!cal || !contributor_name || !out) return -1;

    nimcp_mutex_lock(cal->mutex);

    int idx = find_contributor(cal, contributor_name);
    if (idx < 0) {
        nimcp_mutex_unlock(cal->mutex);
        return -1;
    }

    *out = cal->contributors[idx];

    nimcp_mutex_unlock(cal->mutex);
    return 0;
}

int reasoning_calibration_get_stats(const reasoning_calibration_t* cal,
                                     calibration_stats_t* stats)
{
    if (!cal || !stats) return -1;

    nimcp_mutex_lock(cal->mutex);

    memset(stats, 0, sizeof(calibration_stats_t));
    stats->total_records = cal->total_records;
    stats->total_contributors_tracked = cal->num_contributors;

    if (cal->num_contributors == 0) {
        nimcp_mutex_unlock(cal->mutex);
        return 0;
    }

    /* Compute aggregates */
    float sum_reliability = 0.0f;
    float sum_scale = 0.0f;
    float best_reliability = -1.0f;
    float worst_reliability = 2.0f;
    int best_idx = 0;
    int worst_idx = 0;

    for (uint32_t i = 0; i < cal->num_contributors; i++) {
        const contributor_calibration_t* entry = &cal->contributors[i];
        sum_reliability += entry->reliability_score;
        sum_scale += entry->confidence_scale;

        if (entry->reliability_score > best_reliability) {
            best_reliability = entry->reliability_score;
            best_idx = (int)i;
        }
        if (entry->reliability_score < worst_reliability) {
            worst_reliability = entry->reliability_score;
            worst_idx = (int)i;
        }
    }

    stats->avg_reliability = sum_reliability / (float)cal->num_contributors;
    stats->avg_scale_factor = sum_scale / (float)cal->num_contributors;

    strncpy(stats->best_contributor_name,
            cal->contributors[best_idx].contributor_name,
            REASONING_CALIBRATION_NAME_LEN - 1);
    stats->best_contributor_name[REASONING_CALIBRATION_NAME_LEN - 1] = '\0';

    strncpy(stats->worst_contributor_name,
            cal->contributors[worst_idx].contributor_name,
            REASONING_CALIBRATION_NAME_LEN - 1);
    stats->worst_contributor_name[REASONING_CALIBRATION_NAME_LEN - 1] = '\0';

    nimcp_mutex_unlock(cal->mutex);
    return 0;
}

int reasoning_calibration_reset(reasoning_calibration_t* cal)
{
    if (!cal) return -1;

    nimcp_mutex_lock(cal->mutex);

    memset(cal->contributors, 0,
           sizeof(contributor_calibration_t) * REASONING_MAX_CALIBRATED_CONTRIBUTORS);
    cal->num_contributors = 0;
    cal->total_records = 0;

    nimcp_mutex_unlock(cal->mutex);

    NIMCP_LOGGING_INFO("calibration: reset — all contributor data cleared");
    return 0;
}
