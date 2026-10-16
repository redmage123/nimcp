/**
 * @file nimcp_number_sense.c
 * @brief Approximate Number System (ANS) implementation
 *
 * Implements Weber-Fechner law, subitizing, and approximate arithmetic
 * based on biological number processing in the intraparietal sulcus.
 */

#include "cognitive/parietal/nimcp_number_sense.h"
#include "constants/nimcp_buffer_constants.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/rng/nimcp_rand.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(number_sense, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

/**
 * @brief Internal number sense state
 */
struct number_sense {
    /* Configuration */
    number_sense_config_t config;

    /* Modulation state */
    float inflammation_level;       /**< Current inflammation [0,1] */
    float sleep_deprivation_level;  /**< Current sleep deprivation [0,1] */
    float effective_weber_fraction; /**< Weber fraction after modulation */

    /* Statistics */
    uint64_t estimates_performed;
    uint64_t subitizing_count;
    uint64_t comparisons_performed;
    uint64_t arithmetic_operations;
    double total_estimation_error;
    double total_processing_time_us;

    /* Random state for noise generation */
    uint32_t rng_state;

    /* Thread safety */
    nimcp_mutex_t* lock;
};

/* Thread-local error message */
static _Thread_local char g_error_message[NIMCP_ERROR_BUFFER_SIZE] = {0};

/* ============================================================================
 * INTERNAL HELPERS
 * ============================================================================ */

/**
 * @brief Set error message
 */
static void set_error(const char* msg) {
    strncpy(g_error_message, msg, sizeof(g_error_message) - 1);
    g_error_message[sizeof(g_error_message) - 1] = '\0';
}

/**
 * @brief Generate Gaussian noise using centralized RNG module
 *
 * Note: The ns parameter is kept for API compatibility but the internal
 * rng_state is no longer used. Thread-local RNG is now used instead.
 * For strict reproducibility, call nimcp_rand_seed() before operations.
 */
static inline float gaussian_noise(number_sense_t* ns, float sigma) {
    (void)ns;  /* rng_state no longer used - using thread-local RNG */
    return nimcp_rand_normal(0.0f, sigma);
}

/**
 * @brief Get current time in microseconds
 */
static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Update effective Weber fraction based on modulation
 */
static void update_effective_weber(number_sense_t* ns) {
    float base = ns->config.weber_fraction;

    /* Inflammation degrades precision */
    float inflammation_factor = 1.0f +
        ns->inflammation_level * ns->config.inflammation_sensitivity * 0.5f;

    /* Sleep deprivation degrades precision */
    float sleep_factor = 1.0f +
        ns->sleep_deprivation_level * ns->config.sleep_deprivation_factor * 0.3f;

    ns->effective_weber_fraction = base * inflammation_factor * sleep_factor;

    /* Cap at reasonable maximum */
    if (ns->effective_weber_fraction > 0.5f) {
        ns->effective_weber_fraction = 0.5f;
    }
}

/**
 * @brief Clamp value to valid magnitude range
 */
static float clamp_magnitude(float value) {
    if (value < NUMBER_SENSE_MIN_MAGNITUDE) {
        return NUMBER_SENSE_MIN_MAGNITUDE;
    }
    if (value > NUMBER_SENSE_MAX_MAGNITUDE) {
        return NUMBER_SENSE_MAX_MAGNITUDE;
    }
    return value;
}

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

number_sense_config_t number_sense_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    number_sense_heartbeat("number_sense_default_config", 0.0f);


    number_sense_config_t config = {
        .weber_fraction = NUMBER_SENSE_DEFAULT_WEBER_FRACTION,
        .subitizing_limit = NUMBER_SENSE_SUBITIZING_LIMIT,
        .estimation_noise = 0.1f,
        .enable_logarithmic_scale = true,
        .enable_subitizing = true,
        .enable_bio_async = false,
        .inflammation_sensitivity = 0.5f,
        .sleep_deprivation_factor = 0.5f
    };
    return config;
}

bool number_sense_validate_config(const number_sense_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "number_sense_validate_config: config is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    number_sense_heartbeat("number_sense_validate_config", 0.0f);


    if (config->weber_fraction <= 0.0f || config->weber_fraction > 1.0f) {
        set_error("Weber fraction must be in (0, 1]");
        return false;
    }

    if (config->subitizing_limit < 1 || config->subitizing_limit > 10) {
        set_error("Subitizing limit must be in [1, 10]");
        return false;
    }

    if (config->estimation_noise < 0.0f || config->estimation_noise > 1.0f) {
        set_error("Estimation noise must be in [0, 1]");
        return false;
    }

    if (config->inflammation_sensitivity < 0.0f || config->inflammation_sensitivity > 1.0f) {
        set_error("Inflammation sensitivity must be in [0, 1]");
        return false;
    }

    if (config->sleep_deprivation_factor < 0.0f || config->sleep_deprivation_factor > 1.0f) {
        set_error("Sleep deprivation factor must be in [0, 1]");
        return false;
    }

    return true;
}

number_sense_t* number_sense_create(void) {
    /* Phase 8: Heartbeat at operation start */
    number_sense_heartbeat("number_sense_create", 0.0f);


    return number_sense_create_custom(NULL);
}

number_sense_t* number_sense_create_custom(const number_sense_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    number_sense_heartbeat("number_sense_create_custom", 0.0f);


    number_sense_config_t cfg;

    if (config) {
        if (!number_sense_validate_config(config)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "number_sense_create_custom: number_sense_validate_config is NULL");
            return NULL;
        }
        cfg = *config;
    } else {
        cfg = number_sense_default_config();
    }

    number_sense_t* ns = nimcp_calloc(1, sizeof(number_sense_t));
    if (!ns) {
        set_error("Failed to allocate number sense");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate ns");

        return NULL;
    }

    ns->config = cfg;
    ns->inflammation_level = 0.0f;
    ns->sleep_deprivation_level = 0.0f;
    ns->effective_weber_fraction = cfg.weber_fraction;

    /* Initialize RNG with current time */
    ns->rng_state = (uint32_t)(get_time_us() & 0xFFFFFFFF);
    if (ns->rng_state == 0) ns->rng_state = 1;

    /* Create mutex */
    mutex_attr_t attr = {.type = MUTEX_TYPE_NORMAL};
    ns->lock = nimcp_mutex_create(&attr);
    if (!ns->lock) {
        set_error("Failed to create mutex");
        nimcp_free(ns);
        ns = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "number_sense_create_custom: ns->lock is NULL");
        return NULL;
    }

    return ns;
}

void number_sense_destroy(number_sense_t* ns) {
    if (!ns) return;

    /* Phase 8: Heartbeat at operation start */
    number_sense_heartbeat("number_sense_destroy", 0.0f);


    if (ns->lock) {
        nimcp_mutex_destroy(ns->lock);
    }

    nimcp_free(ns);
    ns = NULL;
}

/* ============================================================================
 * ESTIMATION API
 * ============================================================================ */

number_estimate_t number_sense_estimate(
    number_sense_t* ns,
    const float* input,
    uint32_t input_size
) {
    /* Phase 8: Heartbeat at operation start */
    number_sense_heartbeat("number_sense_estimate", 0.0f);


    number_estimate_t result = {0};

    if (!ns) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "number_sense_estimate: ns is NULL");
        result.confidence = -1.0f;
        return result;
    }
    if (!input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "number_sense_estimate: input is NULL");
        result.confidence = -1.0f;
        return result;
    }
    if (input_size == 0) {
        result.confidence = 0.0f;
        return result;
    }

    uint64_t start_time = get_time_us();

    nimcp_mutex_lock(ns->lock);

    /* Sum input activations to get magnitude estimate */
    float sum = 0.0f;
    for (uint32_t i = 0; i < input_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && input_size > 256) {
            number_sense_heartbeat("number_sense_loop",
                             (float)(i + 1) / (float)input_size);
        }

        if (input[i] > 0.0f) {
            sum += input[i];
        }
    }

    /* Check for subitizing range */
    bool can_subitize = ns->config.enable_subitizing &&
                        sum <= (float)ns->config.subitizing_limit;

    if (can_subitize) {
        result = number_sense_subitize(ns, input, input_size);
        /* Note: subitize already updates stats */
        nimcp_mutex_unlock(ns->lock);
        return result;
    }

    /* Use ANS for larger quantities */
    result = number_sense_estimate_from_magnitude(ns, sum);

    /* Update statistics */
    ns->estimates_performed++;
    uint64_t end_time = get_time_us();
    result.processing_time_us = end_time - start_time;
    ns->total_processing_time_us += (double)result.processing_time_us;

    nimcp_mutex_unlock(ns->lock);

    return result;
}

number_estimate_t number_sense_estimate_from_magnitude(
    number_sense_t* ns,
    float actual_magnitude
) {
    /* Phase 8: Heartbeat at operation start */
    number_sense_heartbeat("number_sense_estimate_from_magnit", 0.0f);


    number_estimate_t result = {0};

    if (!ns) {
        result.confidence = 0.0f;
        return result;
    }

    uint64_t start_time = get_time_us();

    /* Clamp to valid range */
    actual_magnitude = clamp_magnitude(actual_magnitude);

    /* Apply logarithmic mental number line if enabled */
    float internal_rep = 0.0f;
    if (ns->config.enable_logarithmic_scale) {
        internal_rep = logf(actual_magnitude);
    } else {
        internal_rep = actual_magnitude;
    }

    /* Add Weber-law noise (proportional to magnitude) */
    float weber = ns->effective_weber_fraction;
    float noise_sigma = weber * fabsf(internal_rep) + ns->config.estimation_noise;
    float noise = gaussian_noise(ns, noise_sigma);

    float noisy_rep = internal_rep + noise;

    /* Convert back from log scale */
    if (ns->config.enable_logarithmic_scale) {
        result.magnitude = expf(noisy_rep);
    } else {
        result.magnitude = noisy_rep;
    }

    /* Clamp result */
    result.magnitude = clamp_magnitude(result.magnitude);

    /* Compute uncertainty (Weber's law) */
    result.uncertainty = result.magnitude * weber;

    /* Compute confidence based on ratio to uncertainty */
    float ratio = result.magnitude / (result.uncertainty + 0.001f);
    result.confidence = 1.0f - expf(-ratio / 5.0f);
    if (result.confidence > 1.0f) result.confidence = 1.0f;
    if (result.confidence < 0.0f) result.confidence = 0.0f;

    result.is_subitized = false;
    result.processing_time_us = get_time_us() - start_time;

    return result;
}

number_estimate_t number_sense_subitize(
    number_sense_t* ns,
    const float* input,
    uint32_t input_size
) {
    /* Phase 8: Heartbeat at operation start */
    number_sense_heartbeat("number_sense_subitize", 0.0f);


    number_estimate_t result = {0};

    if (!ns || !input || input_size == 0) {
        result.confidence = 0.0f;
        return result;
    }

    uint64_t start_time = get_time_us();

    /* Count active inputs (above threshold) */
    uint32_t count = 0;
    for (uint32_t i = 0; i < input_size && count <= ns->config.subitizing_limit + 1; i++) {
        if (input[i] > 0.5f) {
            count++;
        }
    }

    /* Subitizing works perfectly for small quantities */
    if (count <= ns->config.subitizing_limit) {
        result.magnitude = (float)count;
        result.uncertainty = 0.0f;  /* Perfect for subitized */
        result.confidence = 1.0f;
        result.is_subitized = true;

        ns->subitizing_count++;
    } else {
        /* Fall back to estimation for larger quantities */
        result.magnitude = (float)count + gaussian_noise(ns, ns->effective_weber_fraction * (float)count);
        result.uncertainty = ns->effective_weber_fraction * (float)count;
        result.confidence = 0.8f - 0.05f * (float)(count - ns->config.subitizing_limit);
        if (result.confidence < 0.3f) result.confidence = 0.3f;
        result.is_subitized = false;
    }

    result.processing_time_us = get_time_us() - start_time;
    ns->estimates_performed++;
    ns->total_processing_time_us += (double)result.processing_time_us;

    return result;
}

/* ============================================================================
 * COMPARISON API
 * ============================================================================ */

number_comparison_t number_sense_compare(
    number_sense_t* ns,
    float magnitude_a,
    float magnitude_b
) {
    /* Phase 8: Heartbeat at operation start */
    number_sense_heartbeat("number_sense_compare", 0.0f);


    number_comparison_t result = {0};

    if (!ns) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "number_sense_compare: ns is NULL");
        result.confidence = -1.0f;
        return result;
    }

    nimcp_mutex_lock(ns->lock);

    magnitude_a = clamp_magnitude(magnitude_a);
    magnitude_b = clamp_magnitude(magnitude_b);

    /* Compute ratio */
    result.perceived_ratio = magnitude_a / magnitude_b;

    /* Compute discriminability using d' */
    result.discriminability = number_sense_discriminability(ns, magnitude_a, magnitude_b);

    /* Determine comparison direction based on d' */
    if (result.discriminability > 1.0f) {
        result.direction = (magnitude_a > magnitude_b) ? 1 : -1;
        result.confidence = 1.0f - expf(-result.discriminability);
    } else if (result.discriminability > 0.5f) {
        result.direction = (magnitude_a > magnitude_b) ? 1 : -1;
        result.confidence = 0.5f + 0.3f * result.discriminability;
    } else {
        /* Too close to discriminate reliably */
        result.direction = 0;
        result.confidence = 0.5f;
    }

    ns->comparisons_performed++;

    nimcp_mutex_unlock(ns->lock);

    return result;
}

float number_sense_get_weber_fraction(
    const number_sense_t* ns,
    float magnitude
) {
    if (!ns) return NUMBER_SENSE_DEFAULT_WEBER_FRACTION;

    /* Phase 8: Heartbeat at operation start */
    number_sense_heartbeat("number_sense_get_weber_fraction", 0.0f);


    (void)magnitude;  /* Weber fraction is magnitude-independent in this model */

    return ns->effective_weber_fraction;
}

float number_sense_discriminability(
    const number_sense_t* ns,
    float magnitude_a,
    float magnitude_b
) {
    if (!ns) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    number_sense_heartbeat("number_sense_discriminability", 0.0f);


    magnitude_a = clamp_magnitude(magnitude_a);
    magnitude_b = clamp_magnitude(magnitude_b);

    /* d' = |log(a) - log(b)| / (weber * sqrt(2)) */
    /* This follows from signal detection theory with Weber scaling */
    float log_diff = fabsf(logf(magnitude_a) - logf(magnitude_b));
    float noise_sigma = ns->effective_weber_fraction * sqrtf(2.0f);

    return log_diff / (fabsf(noise_sigma) > 1e-7f ? noise_sigma : 1e-7f);
}

/* ============================================================================
 * APPROXIMATE ARITHMETIC API
 * ============================================================================ */

approx_arithmetic_t number_sense_approximate_add(
    number_sense_t* ns,
    float a,
    float b
) {
    /* Phase 8: Heartbeat at operation start */
    number_sense_heartbeat("number_sense_approximate_add", 0.0f);


    approx_arithmetic_t result = {0};

    if (!ns) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "number_sense_approximate_add: ns is NULL");
        result.confidence = -1.0f;
        return result;
    }

    nimcp_mutex_lock(ns->lock);

    a = clamp_magnitude(a);
    b = clamp_magnitude(b);

    float true_sum = a + b;

    /* Add noise proportional to Weber fraction of result */
    float noise = gaussian_noise(ns, ns->effective_weber_fraction * true_sum);
    result.result = true_sum + noise;
    result.result = clamp_magnitude(result.result);

    /* Uncertainty grows with operand sizes */
    result.uncertainty = ns->effective_weber_fraction * true_sum;

    /* Confidence based on relative uncertainty */
    result.confidence = 1.0f / (1.0f + result.uncertainty / result.result);

    ns->arithmetic_operations++;

    nimcp_mutex_unlock(ns->lock);

    return result;
}

approx_arithmetic_t number_sense_approximate_sub(
    number_sense_t* ns,
    float a,
    float b
) {
    /* Phase 8: Heartbeat at operation start */
    number_sense_heartbeat("number_sense_approximate_sub", 0.0f);


    approx_arithmetic_t result = {0};

    if (!ns) {
        result.confidence = 0.0f;
        return result;
    }

    nimcp_mutex_lock(ns->lock);

    a = clamp_magnitude(a);
    b = clamp_magnitude(b);

    float true_diff = a - b;

    /* Subtraction of similar magnitudes has high uncertainty */
    float noise = gaussian_noise(ns, ns->effective_weber_fraction * (a + b));
    result.result = true_diff + noise;

    /* Uncertainty based on both operands */
    result.uncertainty = ns->effective_weber_fraction * (a + b);

    /* Confidence is lower when difference is small relative to operands */
    float relative_diff = fabsf(true_diff) / (a + b);
    result.confidence = 0.5f + 0.5f * relative_diff;
    if (result.confidence > 1.0f) result.confidence = 1.0f;

    ns->arithmetic_operations++;

    nimcp_mutex_unlock(ns->lock);

    return result;
}

approx_arithmetic_t number_sense_approximate_mul(
    number_sense_t* ns,
    float a,
    float b
) {
    /* Phase 8: Heartbeat at operation start */
    number_sense_heartbeat("number_sense_approximate_mul", 0.0f);


    approx_arithmetic_t result = {0};

    if (!ns) {
        result.confidence = 0.0f;
        return result;
    }

    nimcp_mutex_lock(ns->lock);

    a = clamp_magnitude(a);
    b = clamp_magnitude(b);

    float true_product = a * b;

    /* In log space, multiplication becomes addition of logs */
    /* Uncertainty compounds: sigma_product ≈ product * sqrt(2) * weber */
    float relative_noise = gaussian_noise(ns, ns->effective_weber_fraction * sqrtf(2.0f));
    result.result = true_product * (1.0f + relative_noise);
    result.result = clamp_magnitude(result.result);

    result.uncertainty = true_product * ns->effective_weber_fraction * sqrtf(2.0f);

    /* Confidence based on uncertainty */
    result.confidence = 1.0f / (1.0f + ns->effective_weber_fraction * sqrtf(2.0f));

    ns->arithmetic_operations++;

    nimcp_mutex_unlock(ns->lock);

    return result;
}

approx_arithmetic_t number_sense_approximate_div(
    number_sense_t* ns,
    float a,
    float b
) {
    /* Phase 8: Heartbeat at operation start */
    number_sense_heartbeat("number_sense_approximate_div", 0.0f);


    approx_arithmetic_t result = {0};

    if (!ns) {
        result.confidence = 0.0f;
        return result;
    }

    if (fabsf(b) < NUMBER_SENSE_MIN_MAGNITUDE) {
        set_error("Division by zero or near-zero");
        result.confidence = 0.0f;
        return result;
    }

    nimcp_mutex_lock(ns->lock);

    a = clamp_magnitude(a);
    b = clamp_magnitude(b);

    float true_quotient = a / b;

    /* Similar to multiplication, uncertainty compounds */
    float relative_noise = gaussian_noise(ns, ns->effective_weber_fraction * sqrtf(2.0f));
    result.result = true_quotient * (1.0f + relative_noise);
    result.result = clamp_magnitude(result.result);

    result.uncertainty = true_quotient * ns->effective_weber_fraction * sqrtf(2.0f);
    result.confidence = 1.0f / (1.0f + ns->effective_weber_fraction * sqrtf(2.0f));

    ns->arithmetic_operations++;

    nimcp_mutex_unlock(ns->lock);

    return result;
}

int number_sense_order_of_magnitude(
    number_sense_t* ns,
    float value
) {
    if (!ns) return 0;

    /* Phase 8: Heartbeat at operation start */
    number_sense_heartbeat("number_sense_order_of_magnitude", 0.0f);


    value = clamp_magnitude(value);

    /* Compute order of magnitude */
    float log10_value = log10f(value);

    /* Add noise for uncertainty at boundaries */
    float noise = gaussian_noise(ns, ns->effective_weber_fraction);
    log10_value += noise;

    return (int)floorf(log10_value);
}

/* ============================================================================
 * MODULATION API
 * ============================================================================ */

int number_sense_set_inflammation(
    number_sense_t* ns,
    float level
) {
    if (!ns) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ns is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    number_sense_heartbeat("number_sense_set_inflammation", 0.0f);


    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;

    nimcp_mutex_lock(ns->lock);
    ns->inflammation_level = level;
    update_effective_weber(ns);
    nimcp_mutex_unlock(ns->lock);

    return 0;
}

int number_sense_set_sleep_deprivation(
    number_sense_t* ns,
    float level
) {
    if (!ns) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ns is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    number_sense_heartbeat("number_sense_set_sleep_deprivatio", 0.0f);


    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;

    nimcp_mutex_lock(ns->lock);
    ns->sleep_deprivation_level = level;
    update_effective_weber(ns);
    nimcp_mutex_unlock(ns->lock);

    return 0;
}

float number_sense_get_effective_weber_fraction(const number_sense_t* ns) {
    if (!ns) return NUMBER_SENSE_DEFAULT_WEBER_FRACTION;
    /* Phase 8: Heartbeat at operation start */
    number_sense_heartbeat("number_sense_get_effective_weber_", 0.0f);


    return ns->effective_weber_fraction;
}

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

int number_sense_get_stats(
    const number_sense_t* ns,
    number_sense_stats_t* stats
) {
    if (!ns) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "number_sense_get_stats: ns is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "number_sense_get_stats: stats is NULL");
        return -1;
    }

    /* Cast away const for lock - safe because we only read */
    /* Phase 8: Heartbeat at operation start */
    number_sense_heartbeat("number_sense_get_stats", 0.0f);


    nimcp_mutex_lock(((number_sense_t*)ns)->lock);

    stats->estimates_performed = ns->estimates_performed;
    stats->subitizing_count = ns->subitizing_count;
    stats->comparisons_performed = ns->comparisons_performed;
    stats->arithmetic_operations = ns->arithmetic_operations;

    if (ns->estimates_performed > 0) {
        stats->avg_estimation_error = (float)(ns->total_estimation_error /
                                               (double)ns->estimates_performed);
        stats->avg_processing_time_us = (float)(ns->total_processing_time_us /
                                                 (double)ns->estimates_performed);
    } else {
        stats->avg_estimation_error = 0.0f;
        stats->avg_processing_time_us = 0.0f;
    }

    stats->current_weber_fraction = ns->effective_weber_fraction;

    nimcp_mutex_unlock(((number_sense_t*)ns)->lock);

    return 0;
}

void number_sense_reset_stats(number_sense_t* ns) {
    if (!ns) return;

    /* Phase 8: Heartbeat at operation start */
    number_sense_heartbeat("number_sense_reset_stats", 0.0f);


    nimcp_mutex_lock(ns->lock);

    ns->estimates_performed = 0;
    ns->subitizing_count = 0;
    ns->comparisons_performed = 0;
    ns->arithmetic_operations = 0;
    ns->total_estimation_error = 0.0;
    ns->total_processing_time_us = 0.0;

    nimcp_mutex_unlock(ns->lock);
}

const char* number_sense_get_last_error(void) {
    return g_error_message;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int number_sense_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    number_sense_heartbeat("number_sense_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Number_Sense");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                number_sense_heartbeat("number_sense_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            /* Module self-knowledge logged */
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Number_Sense");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Number_Sense");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void number_sense_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_number_sense_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Functions
 * ============================================================================ */

int number_sense_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "number_sense_training_begin: NULL argument");
        return -1;
    }
    number_sense_heartbeat_instance(g_number_sense_health_agent, "number_sense_training_begin", 0.0f);
    number_sense_t* ns = (number_sense_t*)instance;
    ns->estimates_performed = 0;
    ns->subitizing_count = 0;
    ns->comparisons_performed = 0;
    ns->arithmetic_operations = 0;
    ns->total_estimation_error = 0.0;
    ns->total_processing_time_us = 0.0;
    ns->effective_weber_fraction = ns->config.weber_fraction;
    NIMCP_LOGGING_INFO("Number sense training begin: counters reset, weber_fraction=%.4f",
                       ns->config.weber_fraction);
    return 0;
}

int number_sense_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "number_sense_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    number_sense_heartbeat_instance(g_number_sense_health_agent, "number_sense_training_step", progress);
    number_sense_t* ns = (number_sense_t*)instance;
    ns->estimates_performed++;
    /* Sharpen Weber fraction with training (better discrimination) */
    float weber_decay = 1.0f - 0.2f * progress;
    if (weber_decay < 0.6f) weber_decay = 0.6f;
    ns->effective_weber_fraction = ns->config.weber_fraction * weber_decay;
    if (ns->effective_weber_fraction < 0.05f)
        ns->effective_weber_fraction = 0.05f;
    /* Reduce estimation noise with training experience */
    ns->config.estimation_noise *= (1.0f - 0.05f * progress);
    if (ns->config.estimation_noise < 0.01f)
        ns->config.estimation_noise = 0.01f;
    /* Improve sleep deprivation resilience */
    ns->config.sleep_deprivation_factor *= (1.0f - 0.03f * progress);
    if (ns->config.sleep_deprivation_factor < 0.1f)
        ns->config.sleep_deprivation_factor = 0.1f;
    return 0;
}

int number_sense_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "number_sense_training_end: NULL argument");
        return -1;
    }
    number_sense_heartbeat_instance(g_number_sense_health_agent, "number_sense_training_end", 1.0f);
    number_sense_t* ns = (number_sense_t*)instance;
    float avg_error = (ns->estimates_performed > 0)
        ? (float)(ns->total_estimation_error / (double)ns->estimates_performed)
        : 0.0f;
    float avg_time = (ns->estimates_performed > 0)
        ? (float)(ns->total_processing_time_us / (double)ns->estimates_performed)
        : 0.0f;
    NIMCP_LOGGING_INFO("Number sense training end: %lu estimates, avg_error=%.4f, "
                       "avg_time=%.2f us, weber=%.4f",
                       (unsigned long)ns->estimates_performed, avg_error,
                       avg_time, ns->effective_weber_fraction);
    return 0;
}
