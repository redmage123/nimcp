/**
 * @file nimcp_training_diagnosis.c
 * @brief Abductive Training Diagnosis -- root cause analysis for training failures
 *
 * WHAT: Uses abductive reasoning to diagnose WHY training went wrong
 * WHY:  Current system detects symptoms (NaN, grad explosion) but doesn't reason about causes
 * HOW:  Collect observations from continuous metrics, generate hypotheses, rank by plausibility
 *
 * ALGORITHM:
 * 1. observe_from_metrics(): Threshold-based detection converts raw numbers into symbolic
 *    observations (e.g., grad_norm doubling -> "gradient norm increasing rapidly")
 * 2. Each detected observation is fed to the abduction engine with confidence 0.9
 * 3. diagnose(): Abduction engine generates hypotheses via keyword extraction and pattern
 *    matching, scores by plausibility, returns ranked explanations
 * 4. Recommended actions are derived from the primary diagnosis text
 *
 * @version 1.0.0
 * @date 2026-02-26
 */

#include "middleware/training/nimcp_training_diagnosis.h"
#include "cognitive/reasoning/nimcp_reasoning_abduction.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#define LOG_MODULE "training_diagnosis"

/* Mesh registration boilerplate */
BRIDGE_BOILERPLATE(training_diagnosis, MESH_ADAPTER_CATEGORY_PLASTICITY)

/*=============================================================================
 * OBSERVATION THRESHOLDS
 *===========================================================================*/

/** Gradient norm growth factor to trigger GRADIENT_INCREASING */
#define GRAD_INCREASING_FACTOR  1.5f

/** Gradient norm below this is GRADIENT_VANISHING */
#define GRAD_VANISHING_THRESHOLD 0.001f

/** Loss increase factor to trigger LOSS_INCREASING */
#define LOSS_INCREASING_FACTOR  1.1f

/** Loss volatility above this triggers LOSS_OSCILLATING */
#define LOSS_VOLATILITY_THRESHOLD 0.3f

/** Loss volatility below this with high loss triggers LOSS_PLATEAU */
#define LOSS_PLATEAU_VOLATILITY  0.01f

/** Loss above this considered "high" for plateau detection */
#define LOSS_PLATEAU_FLOOR       0.5f

/** Gradient variance above this triggers HIGH_VARIANCE */
#define GRAD_VARIANCE_THRESHOLD  0.5f

/** Arousal above/below these extremes triggers AROUSAL_EXTREME */
#define AROUSAL_HIGH_THRESHOLD   0.85f
#define AROUSAL_LOW_THRESHOLD    0.15f

/** Inflammation above this triggers INFLAMMATION_HIGH */
#define INFLAMMATION_THRESHOLD   0.5f

/** Resource pressure above this triggers RESOURCE_PRESSURE_HIGH */
#define RESOURCE_PRESSURE_THRESHOLD 0.7f

/** Default observation confidence for detected symptoms */
#define DEFAULT_OBS_CONFIDENCE   0.9f

/*=============================================================================
 * OBSERVATION NAMES
 *===========================================================================*/

static const char* observation_names[TRAIN_OBS_COUNT] = {
    "gradient_increasing",
    "gradient_vanishing",
    "loss_increasing",
    "loss_oscillating",
    "loss_plateau",
    "loss_nan",
    "high_variance",
    "low_throughput",
    "high_memory",
    "arousal_extreme",
    "inflammation_high",
    "resource_pressure_high"
};

/*=============================================================================
 * OBSERVATION DESCRIPTION TEMPLATES
 *===========================================================================*/

static const char* observation_descriptions[TRAIN_OBS_COUNT] = {
    "gradient norm increasing rapidly suggesting learning rate too high or gradient explosion",
    "gradient vanishing near zero suggesting deep network saturation or dead neurons",
    "loss increasing suggesting learning rate too high or data distribution shift",
    "loss oscillating with high volatility suggesting learning rate too high or batch size too small",
    "loss plateau with no improvement suggesting learning rate too low or local minimum",
    "loss is nan or infinity suggesting severe numerical instability or gradient divergence",
    "high gradient variance suggesting batch size too small or noisy gradients",
    "low training throughput suggesting resource contention or memory pressure",
    "high memory usage suggesting batch size too large or memory leak",
    "arousal level at extreme suggesting dysregulated brain state affecting training",
    "inflammation high suggesting immune system response interfering with training plasticity",
    "resource pressure high suggesting portia resource allocation limiting training capacity"
};

/*=============================================================================
 * INTERNAL STRUCTURE
 *===========================================================================*/

struct training_diagnoser {
    reasoning_abduction_t* abduction;     /**< Underlying abductive reasoning engine */
    bool observations_present[TRAIN_OBS_COUNT]; /**< Which observations were detected */
    uint32_t num_observations_detected;   /**< Count of detected observations */
};

/*=============================================================================
 * HELPER: Get current time in microseconds
 *===========================================================================*/

static uint64_t get_time_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/*=============================================================================
 * HELPER: Add a single observation to the abduction engine
 *===========================================================================*/

static int add_observation(training_diagnoser_t* diag, training_observation_type_t type)
{
    if (type >= TRAIN_OBS_COUNT) return -1;

    /* Don't add duplicates */
    if (diag->observations_present[type]) return 0;

    abductive_observation_t obs;
    memset(&obs, 0, sizeof(obs));
    snprintf(obs.description, sizeof(obs.description), "%s", observation_descriptions[type]);
    obs.confidence = DEFAULT_OBS_CONFIDENCE;
    obs.domain = 0;  /* Training domain */
    obs.timestamp_us = get_time_us();

    int rc = reasoning_abduction_add_observation(diag->abduction, &obs);
    if (rc == 0) {
        diag->observations_present[type] = true;
        diag->num_observations_detected++;
    }
    return rc;
}

/*=============================================================================
 * HELPER: Derive recommended actions from diagnosis text
 *===========================================================================*/

static void derive_recommendations(const char* cause, float plausibility,
                                    training_diagnosis_t* result)
{
    if (!cause || cause[0] == '\0') return;

    /* Case-insensitive substring matching for action derivation */
    /* We search for keywords in the explanation to determine actions */
    char lower[256];
    size_t len = strlen(cause);
    if (len >= sizeof(lower)) len = sizeof(lower) - 1;
    for (size_t i = 0; i < len; i++) {
        lower[i] = (char)((cause[i] >= 'A' && cause[i] <= 'Z')
                          ? cause[i] + 32 : cause[i]);
    }
    lower[len] = '\0';

    /* Learning rate related */
    if (strstr(lower, "learning rate") || strstr(lower, " lr ") ||
        strstr(lower, "lr too")) {
        result->recommend_reduce_lr = true;
        result->recommended_lr_factor = 0.5f;
    }

    /* Batch size related */
    if (strstr(lower, "batch")) {
        result->recommend_increase_batch = true;
    }

    /* Gradient clipping related */
    if (strstr(lower, "gradient") && strstr(lower, "clip")) {
        result->recommend_tighter_clip = true;
    }

    /* Gradient explosion without clip mention also suggests LR reduction */
    if (strstr(lower, "gradient") && strstr(lower, "explos")) {
        result->recommend_reduce_lr = true;
        if (result->recommended_lr_factor > 0.3f) {
            result->recommended_lr_factor = 0.3f;
        }
    }

    /* NaN / divergence -> rollback */
    if (strstr(lower, "nan") || strstr(lower, "diverge") ||
        strstr(lower, "infinity") || strstr(lower, "numerical instability")) {
        result->recommend_rollback = true;
    }

    /* Low plausibility -> conservative pause */
    if (plausibility < 0.3f) {
        result->recommend_pause = true;
    }
}

/*=============================================================================
 * PUBLIC API
 *===========================================================================*/

training_diagnoser_t* training_diagnoser_create(void)
{
    training_diagnoser_t* diag = nimcp_calloc(1, sizeof(training_diagnoser_t));
    if (!diag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "training_diagnoser_create: allocation failed");
        return NULL;
    }

    abduction_config_t config = reasoning_abduction_default_config();
    diag->abduction = reasoning_abduction_create(&config);
    if (!diag->abduction) {
        nimcp_free(diag);
        return NULL;
    }

    memset(diag->observations_present, 0, sizeof(diag->observations_present));
    diag->num_observations_detected = 0;

    return diag;
}

void training_diagnoser_destroy(training_diagnoser_t* diag)
{
    if (!diag) return;

    if (diag->abduction) {
        reasoning_abduction_destroy(diag->abduction);
        diag->abduction = NULL;
    }

    nimcp_free(diag);
}

int training_diagnoser_observe_from_metrics(
    training_diagnoser_t* diag,
    float loss_current, float loss_previous,
    float grad_norm, float grad_norm_previous,
    float loss_volatility, float gradient_variance,
    float learning_rate, float batch_size,
    float arousal_level, float inflammation_level,
    float resource_pressure)
{
    if (!diag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_diagnoser_observe_from_metrics: diag is NULL");
        return -1;
    }

    (void)learning_rate;
    (void)batch_size;

    /* Detect gradient increasing: norm grew by more than 50% */
    if (grad_norm_previous > 0.0f && grad_norm > grad_norm_previous * GRAD_INCREASING_FACTOR) {
        add_observation(diag, TRAIN_OBS_GRADIENT_INCREASING);
    }

    /* Detect gradient vanishing: norm near zero */
    if (grad_norm < GRAD_VANISHING_THRESHOLD) {
        add_observation(diag, TRAIN_OBS_GRADIENT_VANISHING);
    }

    /* Detect NaN/Inf loss (check before other loss metrics) */
    if (isnan(loss_current) || isinf(loss_current)) {
        add_observation(diag, TRAIN_OBS_LOSS_NAN);
    } else {
        /* Detect loss increasing: current > previous by 10% */
        if (loss_previous > 0.0f && loss_current > loss_previous * LOSS_INCREASING_FACTOR) {
            add_observation(diag, TRAIN_OBS_LOSS_INCREASING);
        }

        /* Detect loss oscillating: high volatility */
        if (loss_volatility > LOSS_VOLATILITY_THRESHOLD) {
            add_observation(diag, TRAIN_OBS_LOSS_OSCILLATING);
        }

        /* Detect loss plateau: low volatility but still high loss */
        if (loss_volatility < LOSS_PLATEAU_VOLATILITY && loss_current > LOSS_PLATEAU_FLOOR) {
            add_observation(diag, TRAIN_OBS_LOSS_PLATEAU);
        }
    }

    /* Detect high gradient variance */
    if (gradient_variance > GRAD_VARIANCE_THRESHOLD) {
        add_observation(diag, TRAIN_OBS_HIGH_VARIANCE);
    }

    /* Detect extreme arousal */
    if (arousal_level > AROUSAL_HIGH_THRESHOLD || arousal_level < AROUSAL_LOW_THRESHOLD) {
        add_observation(diag, TRAIN_OBS_AROUSAL_EXTREME);
    }

    /* Detect high inflammation */
    if (inflammation_level > INFLAMMATION_THRESHOLD) {
        add_observation(diag, TRAIN_OBS_INFLAMMATION_HIGH);
    }

    /* Detect high resource pressure */
    if (resource_pressure > RESOURCE_PRESSURE_THRESHOLD) {
        add_observation(diag, TRAIN_OBS_RESOURCE_PRESSURE_HIGH);
    }

    return 0;
}

int training_diagnoser_diagnose(training_diagnoser_t* diag, training_diagnosis_t* result)
{
    if (!diag || !result) return -1;

    /* Initialize result to safe defaults */
    memset(result, 0, sizeof(training_diagnosis_t));
    result->primary_plausibility = 0.0f;
    result->secondary_plausibility = 0.0f;
    result->recommended_lr_factor = 1.0f;  /* No change by default */

    result->num_observations = diag->num_observations_detected;

    /* If no observations, return defaults (no diagnosis possible) */
    if (diag->num_observations_detected == 0) {
        snprintf(result->primary_cause, sizeof(result->primary_cause),
                 "no anomalies detected");
        return 0;
    }

    /* Run abductive reasoning */
    abduction_result_t abd_result;
    memset(&abd_result, 0, sizeof(abd_result));

    int rc = reasoning_abduction_generate(diag->abduction, &abd_result);
    if (rc != 0) {
        snprintf(result->primary_cause, sizeof(result->primary_cause),
                 "diagnosis generation failed");
        return -1;
    }

    result->num_hypotheses = abd_result.num_hypotheses;

    /* Extract primary hypothesis */
    if (abd_result.num_hypotheses > 0) {
        const abductive_hypothesis_t* best = reasoning_abduction_select_best(&abd_result);
        if (best) {
            snprintf(result->primary_cause, sizeof(result->primary_cause),
                     "%s", best->explanation);
            result->primary_plausibility = best->plausibility;

            /* Clamp plausibility to [0,1] */
            if (result->primary_plausibility < 0.0f) result->primary_plausibility = 0.0f;
            if (result->primary_plausibility > 1.0f) result->primary_plausibility = 1.0f;
        }

        /* Extract secondary hypothesis (second best) */
        if (abd_result.num_hypotheses > 1) {
            /* Find second best: iterate and pick highest that isn't the best index */
            uint32_t second_idx = 0;
            float second_best = -1.0f;
            for (uint32_t i = 0; i < abd_result.num_hypotheses; i++) {
                if (i == abd_result.best_hypothesis_index) continue;
                if (abd_result.hypotheses[i].plausibility > second_best) {
                    second_best = abd_result.hypotheses[i].plausibility;
                    second_idx = i;
                }
            }
            snprintf(result->secondary_cause, sizeof(result->secondary_cause),
                     "%s", abd_result.hypotheses[second_idx].explanation);
            result->secondary_plausibility = abd_result.hypotheses[second_idx].plausibility;

            /* Clamp */
            if (result->secondary_plausibility < 0.0f) result->secondary_plausibility = 0.0f;
            if (result->secondary_plausibility > 1.0f) result->secondary_plausibility = 1.0f;
        }
    } else {
        snprintf(result->primary_cause, sizeof(result->primary_cause),
                 "no hypotheses generated from observations");
    }

    /* Derive recommended actions from diagnosis */
    derive_recommendations(result->primary_cause, result->primary_plausibility, result);

    /* Also check observations directly for action derivation as a fallback:
     * If the abduction text didn't trigger action keywords, use observation-based rules */
    if (diag->observations_present[TRAIN_OBS_LOSS_NAN]) {
        result->recommend_rollback = true;
    }
    if (diag->observations_present[TRAIN_OBS_GRADIENT_INCREASING] && !result->recommend_reduce_lr) {
        result->recommend_reduce_lr = true;
        result->recommended_lr_factor = 0.5f;
    }
    if (diag->observations_present[TRAIN_OBS_LOSS_OSCILLATING] && !result->recommend_increase_batch) {
        result->recommend_increase_batch = true;
    }

    /* Ensure lr_factor stays in bounds */
    if (result->recommended_lr_factor < 0.01f) result->recommended_lr_factor = 0.01f;
    if (result->recommended_lr_factor > 1.0f) result->recommended_lr_factor = 1.0f;

    return 0;
}

int training_diagnoser_reset(training_diagnoser_t* diag)
{
    if (!diag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_diagnoser_reset: diag is NULL");
        return -1;
    }

    int rc = reasoning_abduction_clear_observations(diag->abduction);
    memset(diag->observations_present, 0, sizeof(diag->observations_present));
    diag->num_observations_detected = 0;

    return rc;
}

const char* training_diagnosis_observation_name(training_observation_type_t type)
{
    if (type >= TRAIN_OBS_COUNT) return "unknown";
    return observation_names[type];
}
