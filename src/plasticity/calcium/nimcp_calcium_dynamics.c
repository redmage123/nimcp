/**
 * @file nimcp_calcium_dynamics.c
 * @brief Calcium-Dependent Learning Rate Dynamics Implementation
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Implementation of calcium-dependent learning rate modulation
 * WHY:  Biological realism for bidirectional synaptic plasticity
 * HOW:  NMDA influx, buffering, extrusion, and omega function computation
 */

#include "plasticity/calcium/nimcp_calcium_dynamics.h"
#include "plasticity/nimcp_plasticity_constants.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(calcium_dynamics)

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Threshold callback entry
 */
typedef struct {
    calcium_threshold_callback_t callback;
    void* user_data;
    bool active;
} calcium_callback_entry_t;

/**
 * @brief Calcium dynamics system implementation
 */
struct calcium_dynamics_struct {
    calcium_config_t config;
    calcium_state_t state;
    calcium_callback_entry_t callbacks[CALCIUM_MAX_THRESHOLD_CALLBACKS];
    nimcp_platform_mutex_t* mutex;

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Deferred callback entry for threshold crossings
 * WHY:  Store callback data to invoke after mutex release to avoid deadlock
 */
typedef struct {
    calcium_threshold_callback_t callback;
    void* user_data;
    calcium_threshold_crossing_t crossing_type;
    float ca_concentration;
} deferred_threshold_callback_t;

#define MAX_DEFERRED_THRESHOLD_CALLBACKS (CALCIUM_MAX_THRESHOLD_CALLBACKS * 6)

/**
 * WHAT: Collect threshold crossing callbacks for deferred invocation
 * WHY:  Invoking callbacks inside mutex lock can cause deadlock
 * HOW:  Copy callback data to deferred array, return count
 * NOTE: Must be called while holding mutex; callbacks invoked after release
 */
static int collect_threshold_callbacks(
    calcium_dynamics_t calcium,
    calcium_threshold_crossing_t crossing_type,
    float ca_concentration,
    deferred_threshold_callback_t* deferred,
    int max_deferred,
    int current_count
) {
    if (!calcium || !deferred) return current_count;

    for (int i = 0; i < CALCIUM_MAX_THRESHOLD_CALLBACKS && current_count < max_deferred; i++) {
        if (calcium->callbacks[i].active && calcium->callbacks[i].callback) {
            deferred[current_count].callback = calcium->callbacks[i].callback;
            deferred[current_count].user_data = calcium->callbacks[i].user_data;
            deferred[current_count].crossing_type = crossing_type;
            deferred[current_count].ca_concentration = ca_concentration;
            current_count++;
        }
    }
    return current_count;
}

/**
 * WHAT: Invoke deferred threshold callbacks after mutex release
 * WHY:  Avoids deadlock by calling user code outside critical section
 * HOW:  Iterate collected callbacks and invoke each
 */
static void invoke_deferred_callbacks(
    deferred_threshold_callback_t* deferred,
    int count
) {
    for (int i = 0; i < count; i++) {
        if (deferred[i].callback) {
            deferred[i].callback(
                deferred[i].crossing_type,
                deferred[i].ca_concentration,
                deferred[i].user_data
            );
        }
    }
}

/**
 * WHAT: Collect threshold crossing callbacks for deferred invocation
 * WHY:  Fire callbacks when calcium crosses thresholds (after mutex release)
 * HOW:  Compare previous and current concentrations, collect callbacks
 * NOTE: Must be called while holding mutex; returns count of collected callbacks
 */
static int collect_threshold_crossings(
    calcium_dynamics_t calcium,
    deferred_threshold_callback_t* deferred,
    int max_deferred
) {
    if (!calcium || !deferred) return 0;

    int count = 0;
    float prev = calcium->state.ca_concentration_prev;
    float curr = calcium->state.ca_concentration;

    /* Check LTD threshold crossings */
    if (prev < calcium->config.threshold_ltd && curr >= calcium->config.threshold_ltd) {
        count = collect_threshold_callbacks(calcium, CALCIUM_CROSS_LTD_THRESHOLD_UP, curr,
                                            deferred, max_deferred, count);
    } else if (prev >= calcium->config.threshold_ltd && curr < calcium->config.threshold_ltd) {
        count = collect_threshold_callbacks(calcium, CALCIUM_CROSS_LTD_THRESHOLD_DOWN, curr,
                                            deferred, max_deferred, count);
    }

    /* Check LTP threshold crossings */
    if (prev < calcium->config.threshold_ltp && curr >= calcium->config.threshold_ltp) {
        count = collect_threshold_callbacks(calcium, CALCIUM_CROSS_LTP_THRESHOLD_UP, curr,
                                            deferred, max_deferred, count);
    } else if (prev >= calcium->config.threshold_ltp && curr < calcium->config.threshold_ltp) {
        count = collect_threshold_callbacks(calcium, CALCIUM_CROSS_LTP_THRESHOLD_DOWN, curr,
                                            deferred, max_deferred, count);
    }

    /* Check saturation threshold crossings */
    if (prev < calcium->config.threshold_saturation && curr >= calcium->config.threshold_saturation) {
        count = collect_threshold_callbacks(calcium, CALCIUM_CROSS_SATURATION_UP, curr,
                                            deferred, max_deferred, count);
    } else if (prev >= calcium->config.threshold_saturation && curr < calcium->config.threshold_saturation) {
        count = collect_threshold_callbacks(calcium, CALCIUM_CROSS_SATURATION_DOWN, curr,
                                            deferred, max_deferred, count);
    }

    return count;
}

/**
 * WHAT: Update plasticity regime based on current concentration
 * WHY:  Track which regime calcium is in
 * HOW:  Threshold-based classification
 */
static void update_plasticity_regime(calcium_dynamics_t calcium) {
    if (!calcium) return;

    float ca = calcium->state.ca_concentration;
    calcium_plasticity_regime_t old_regime = calcium->state.regime;

    if (ca < calcium->config.threshold_no_plasticity) {
        calcium->state.regime = CALCIUM_REGIME_NONE;
    } else if (ca < calcium->config.threshold_ltd) {
        calcium->state.regime = CALCIUM_REGIME_LTD;
    } else if (ca < calcium->config.threshold_ltp) {
        calcium->state.regime = CALCIUM_REGIME_TRANSITION;
    } else if (ca < calcium->config.threshold_saturation) {
        calcium->state.regime = CALCIUM_REGIME_LTP;
    } else {
        calcium->state.regime = CALCIUM_REGIME_SATURATED;
    }

    /* Count regime transitions */
    if (old_regime != calcium->state.regime) {
        if (calcium->state.regime == CALCIUM_REGIME_LTD) {
            calcium->state.ltd_events++;
        } else if (calcium->state.regime == CALCIUM_REGIME_LTP) {
            calcium->state.ltp_events++;
        }
    }
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int calcium_default_config(calcium_config_t* config) {
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_default_config: config is NULL");
        return -1;
    }

    /* Biologically-based defaults (Shouval et al. 2002) */
    config->baseline_concentration = CALCIUM_BASELINE_CONCENTRATION;
    config->threshold_ltd = CALCIUM_THRESHOLD_LTD;
    config->threshold_ltp = CALCIUM_THRESHOLD_LTP;
    config->threshold_saturation = CALCIUM_THRESHOLD_LTP_SATURATION;
    config->threshold_no_plasticity = CALCIUM_THRESHOLD_NO_PLASTICITY;
    config->max_concentration = CALCIUM_MAX_CONCENTRATION;

    /* Dynamics parameters */
    config->decay_tau_ms = CALCIUM_DECAY_TAU_DEFAULT;
    config->pump_rate = CALCIUM_PUMP_RATE_DEFAULT;
    config->buffer_capacity = CALCIUM_BUFFER_CAPACITY_DEFAULT;
    config->influx_alpha = CALCIUM_INFLUX_ALPHA_DEFAULT;

    /* Omega function */
    config->omega_max_learning_rate = CALCIUM_OMEGA_MAX_LEARNING_RATE;
    config->omega_power = CALCIUM_OMEGA_POWER;

    /* NMDA parameters */
    config->nmda_max_conductance = CALCIUM_NMDA_MAX_CONDUCTANCE;
    config->nmda_mg_block_voltage_mv = CALCIUM_NMDA_MG_BLOCK_VOLTAGE;

    /* All features enabled */
    config->enable_nmda_influx = true;
    config->enable_buffering = true;
    config->enable_pumps = true;
    config->enable_omega_function = true;

    return 0;
}

calcium_dynamics_t calcium_create(const calcium_config_t* config) {
    /* Allocate structure */
    struct calcium_dynamics_struct* calcium =
        (struct calcium_dynamics_struct*)nimcp_malloc(sizeof(struct calcium_dynamics_struct));
    if (!calcium) {
        NIMCP_LOGGING_ERROR("Failed to allocate calcium dynamics");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium is NULL");

        return NULL;
    }

    /* Zero-initialize */
    memset(calcium, 0, sizeof(struct calcium_dynamics_struct));

    /* Apply configuration */
    if (config) {
        calcium->config = *config;
    } else {
        calcium_default_config(&calcium->config);
    }

    /* Initialize state to baseline */
    calcium->state.ca_concentration = calcium->config.baseline_concentration;
    calcium->state.ca_concentration_prev = calcium->config.baseline_concentration;
    calcium->state.regime = CALCIUM_REGIME_NONE;
    calcium->state.nmda_activation = 0.0f;
    calcium->state.postsynaptic_voltage_mv = -65.0f;

    /* Create mutex */
    calcium->mutex = nimcp_platform_mutex_create();
    if (!calcium->mutex) {
        nimcp_free(calcium);
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "calcium_create: calcium->mutex is NULL");
        return NULL;
    }

    /* Initialize callbacks */
    for (int i = 0; i < CALCIUM_MAX_THRESHOLD_CALLBACKS; i++) {
        calcium->callbacks[i].active = false;
        calcium->callbacks[i].callback = NULL;
        calcium->callbacks[i].user_data = NULL;
    }

    NIMCP_LOGGING_INFO("Calcium dynamics created (baseline=%.3f μM)",
                       calcium->config.baseline_concentration);
    return calcium;
}

void calcium_destroy(calcium_dynamics_t calcium) {
    if (!calcium) return;

    /* Disconnect bio-async if connected */
    if (calcium->bio_async_enabled) {
        calcium_disconnect_bio_async(calcium);
    }

    /* Destroy mutex */
    if (calcium->mutex) {
        nimcp_platform_mutex_destroy(calcium->mutex);
        nimcp_free(calcium->mutex);
        calcium->mutex = NULL;
    }

    nimcp_free(calcium);
}

/* ============================================================================
 * Update Implementation
 * ============================================================================ */

int calcium_update(calcium_dynamics_t calcium, float delta_ms) {
    if (!calcium) {
        NIMCP_LOGGING_ERROR("NULL calcium pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_update: calcium is NULL");
        return -1;
    }
    if (delta_ms <= 0.0f) return 0;

    nimcp_platform_mutex_lock(calcium->mutex);

    /* Store previous concentration for crossing detection */
    calcium->state.ca_concentration_prev = calcium->state.ca_concentration;

    float ca = calcium->state.ca_concentration;
    float baseline = calcium->config.baseline_concentration;

    /* Get influx from NMDA trigger */
    float j_influx = calcium->state.last_influx;

    /* Apply exponential decay toward baseline using time constant
     * Formula: ca_new = baseline + (ca - baseline) * exp(-dt/tau)
     * This is more numerically stable than incremental updates
     * P1 fix: Clamp exponent to prevent underflow (exp(-x) denormal for x > ~88)
     */
    float tau = calcium->config.decay_tau_ms;
    if (tau > 0.0f) {
        /* Clamp ratio to prevent underflow: exp(-20) ≈ 2e-9, sufficient precision */
        float exp_arg = -delta_ms / tau;
        if (exp_arg < -20.0f) exp_arg = -20.0f;
        float decay_factor = expf(exp_arg);
        /* Validate result and flush denormals */
        if (isnan(decay_factor) || decay_factor < NIMCP_DENORMAL_EXP_THRESHOLD) {
            decay_factor = 0.0f;  /* Complete decay */
        }
        ca = baseline + (ca - baseline) * decay_factor;
    }

    /* Add influx after decay */
    ca += j_influx;

    /* Store last extrusion/buffering as the effective decay amount for reporting */
    float decay_amount = calcium->state.ca_concentration - ca + j_influx;
    calcium->state.last_extrusion = decay_amount > 0.0f ? decay_amount : 0.0f;
    calcium->state.last_buffering = 0.0f;

    /* Clamp to valid range */
    ca = nimcp_clampf(ca, CALCIUM_MIN_CONCENTRATION, calcium->config.max_concentration);

    /* Decay NMDA influx (clear for next update) */
    calcium->state.last_influx = 0.0f;

    /* Track time in regimes BEFORE update (time spent in current regime) */
    if (calcium->state.regime == CALCIUM_REGIME_LTD) {
        calcium->state.time_in_ltd_ms += (uint64_t)delta_ms;
    } else if (calcium->state.regime == CALCIUM_REGIME_LTP ||
               calcium->state.regime == CALCIUM_REGIME_SATURATED) {
        calcium->state.time_in_ltp_ms += (uint64_t)delta_ms;
    }

    /* Update state */
    calcium->state.ca_concentration = ca;
    /* Note: last_extrusion and last_buffering already set above */
    calcium->state.total_updates++;

    /* Update regime for next step */
    update_plasticity_regime(calcium);

    /* Compute learning rate - use inline call to avoid mutex re-lock */
    if (calcium->config.enable_omega_function) {
        calcium->state.current_learning_rate = calcium_omega_function(
            calcium->state.ca_concentration,
            calcium->config.threshold_ltd,
            calcium->config.threshold_ltp,
            calcium->config.omega_max_learning_rate,
            calcium->config.omega_power
        );
    }

    /* Collect threshold crossing callbacks while holding mutex */
    deferred_threshold_callback_t deferred[MAX_DEFERRED_THRESHOLD_CALLBACKS];
    int num_deferred = collect_threshold_crossings(calcium, deferred,
                                                   MAX_DEFERRED_THRESHOLD_CALLBACKS);

    nimcp_platform_mutex_unlock(calcium->mutex);

    /* Invoke callbacks after mutex release to avoid deadlock */
    if (num_deferred > 0) {
        invoke_deferred_callbacks(deferred, num_deferred);
    }

    return 0;
}

int calcium_trigger_nmda_influx(
    calcium_dynamics_t calcium,
    float nmda_activation,
    float postsynaptic_voltage_mv
) {
    if (!calcium) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium is NULL");

        return -1;

    }

    nmda_activation = nimcp_clampf(nmda_activation, 0.0f, 1.0f);

    nimcp_platform_mutex_lock(calcium->mutex);

    calcium->state.nmda_activation = nmda_activation;
    calcium->state.postsynaptic_voltage_mv = postsynaptic_voltage_mv;

    if (calcium->config.enable_nmda_influx) {
        /* Compute Mg²⁺ block factor */
        float mg_block = calcium_compute_mg_block(postsynaptic_voltage_mv);

        /* Compute influx: J_influx = α * NMDA * mg_block * (1 - [Ca²⁺]/max) */
        float ca_ratio = calcium->state.ca_concentration / calcium->config.max_concentration;
        float influx = calcium->config.influx_alpha * nmda_activation * mg_block * (1.0f - ca_ratio);

        calcium->state.last_influx = influx;
    }

    nimcp_platform_mutex_unlock(calcium->mutex);
    return 0;
}

int calcium_set_concentration(calcium_dynamics_t calcium, float concentration) {
    if (!calcium) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium is NULL");

        return -1;

    }

    nimcp_platform_mutex_lock(calcium->mutex);

    calcium->state.ca_concentration_prev = calcium->state.ca_concentration;
    calcium->state.ca_concentration = nimcp_clampf(
        concentration,
        CALCIUM_MIN_CONCENTRATION,
        calcium->config.max_concentration
    );

    update_plasticity_regime(calcium);

    /* Collect threshold crossing callbacks while holding mutex */
    deferred_threshold_callback_t deferred[MAX_DEFERRED_THRESHOLD_CALLBACKS];
    int num_deferred = collect_threshold_crossings(calcium, deferred,
                                                   MAX_DEFERRED_THRESHOLD_CALLBACKS);

    nimcp_platform_mutex_unlock(calcium->mutex);

    /* Invoke callbacks after mutex release to avoid deadlock */
    if (num_deferred > 0) {
        invoke_deferred_callbacks(deferred, num_deferred);
    }

    return 0;
}

int calcium_reset(calcium_dynamics_t calcium) {
    if (!calcium) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium is NULL");

        return -1;

    }

    nimcp_platform_mutex_lock(calcium->mutex);

    calcium->state.ca_concentration = calcium->config.baseline_concentration;
    calcium->state.ca_concentration_prev = calcium->config.baseline_concentration;
    calcium->state.regime = CALCIUM_REGIME_NONE;
    calcium->state.last_influx = 0.0f;
    calcium->state.last_extrusion = 0.0f;
    calcium->state.last_buffering = 0.0f;
    calcium->state.nmda_activation = 0.0f;
    calcium->state.current_learning_rate = 0.0f;

    nimcp_platform_mutex_unlock(calcium->mutex);
    return 0;
}

/* ============================================================================
 * Omega Function Implementation
 * ============================================================================ */

float calcium_compute_learning_rate(const calcium_dynamics_t calcium) {
    if (!calcium) return 0.0f;

    nimcp_platform_mutex_lock(calcium->mutex);

    float lr = calcium_omega_function(
        calcium->state.ca_concentration,
        calcium->config.threshold_ltd,
        calcium->config.threshold_ltp,
        calcium->config.omega_max_learning_rate,
        calcium->config.omega_power
    );

    nimcp_platform_mutex_unlock(calcium->mutex);
    return lr;
}

calcium_plasticity_regime_t calcium_get_regime(const calcium_dynamics_t calcium) {
    if (!calcium) return CALCIUM_REGIME_NONE;

    nimcp_platform_mutex_lock(calcium->mutex);
    calcium_plasticity_regime_t regime = calcium->state.regime;
    nimcp_platform_mutex_unlock(calcium->mutex);

    return regime;
}

bool calcium_is_ltp(const calcium_dynamics_t calcium) {
    calcium_plasticity_regime_t regime = calcium_get_regime(calcium);
    return (regime == CALCIUM_REGIME_LTP || regime == CALCIUM_REGIME_SATURATED);
}

bool calcium_is_ltd(const calcium_dynamics_t calcium) {
    return (calcium_get_regime(calcium) == CALCIUM_REGIME_LTD);
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

float calcium_get_concentration(const calcium_dynamics_t calcium) {
    if (!calcium) return 0.0f;

    nimcp_platform_mutex_lock(calcium->mutex);
    float ca = calcium->state.ca_concentration;
    nimcp_platform_mutex_unlock(calcium->mutex);

    return ca;
}

float calcium_get_learning_rate(const calcium_dynamics_t calcium) {
    if (!calcium) return 0.0f;

    nimcp_platform_mutex_lock(calcium->mutex);
    float lr = calcium->state.current_learning_rate;
    nimcp_platform_mutex_unlock(calcium->mutex);

    return lr;
}

int calcium_get_state(const calcium_dynamics_t calcium, calcium_state_t* state) {
    if (!calcium || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_get_state: required parameter is NULL (calcium, state)");
        return -1;
    }

    nimcp_platform_mutex_lock(calcium->mutex);
    *state = calcium->state;
    nimcp_platform_mutex_unlock(calcium->mutex);

    return 0;
}

int calcium_get_config(const calcium_dynamics_t calcium, calcium_config_t* config) {
    if (!calcium || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_get_config: required parameter is NULL (calcium, config)");
        return -1;
    }

    nimcp_platform_mutex_lock(calcium->mutex);
    *config = calcium->config;
    nimcp_platform_mutex_unlock(calcium->mutex);

    return 0;
}

/* ============================================================================
 * Callback Implementation
 * ============================================================================ */

int calcium_register_threshold_callback(
    calcium_dynamics_t calcium,
    calcium_threshold_callback_t callback,
    void* user_data
) {
    if (!calcium || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_register_threshold_callback: required parameter is NULL (calcium, callback)");
        return -1;
    }

    nimcp_platform_mutex_lock(calcium->mutex);

    /* Find empty slot with explicit bounds validation
     * WHAT: Search for inactive callback slot with bounds check
     * WHY:  Prevent array index out-of-bounds access
     * HOW:  Verify index < CALCIUM_MAX_THRESHOLD_CALLBACKS before access
     */
    int slot = -1;
    for (int i = 0; i < CALCIUM_MAX_THRESHOLD_CALLBACKS; i++) {
        /* Defensive bounds check (compile-time constant, but validates at runtime) */
        if (i >= CALCIUM_MAX_THRESHOLD_CALLBACKS) break;
        if (!calcium->callbacks[i].active) {
            slot = i;
            break;
        }
    }

    if (slot < 0 || slot >= CALCIUM_MAX_THRESHOLD_CALLBACKS) {
        nimcp_platform_mutex_unlock(calcium->mutex);
        NIMCP_LOGGING_WARN("Callback array full or invalid slot");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "calcium_register_threshold_callback: capacity exceeded");
        return -1;
    }

    calcium->callbacks[slot].callback = callback;
    calcium->callbacks[slot].user_data = user_data;
    calcium->callbacks[slot].active = true;

    nimcp_platform_mutex_unlock(calcium->mutex);
    return 0;
}

int calcium_unregister_threshold_callback(
    calcium_dynamics_t calcium,
    calcium_threshold_callback_t callback
) {
    if (!calcium || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_unregister_threshold_callback: required parameter is NULL (calcium, callback)");
        return -1;
    }

    nimcp_platform_mutex_lock(calcium->mutex);

    int found = -1;
    for (int i = 0; i < CALCIUM_MAX_THRESHOLD_CALLBACKS; i++) {
        if (calcium->callbacks[i].active && calcium->callbacks[i].callback == callback) {
            calcium->callbacks[i].active = false;
            calcium->callbacks[i].callback = NULL;
            calcium->callbacks[i].user_data = NULL;
            found = i;
            break;
        }
    }

    nimcp_platform_mutex_unlock(calcium->mutex);
    return (found >= 0) ? 0 : -1;
}

/* ============================================================================
 * Bio-Async Implementation
 * ============================================================================ */

int calcium_connect_bio_async(calcium_dynamics_t calcium) {
    if (!calcium) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium is NULL");

        return -1;

    }

    nimcp_platform_mutex_lock(calcium->mutex);

    if (calcium->bio_async_enabled) {
        nimcp_platform_mutex_unlock(calcium->mutex);
        return 0; /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = 0x0E00, /* BIO_MODULE_CALCIUM_DYNAMICS placeholder */
        .module_name = "calcium_dynamics",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = calcium
    };

    calcium->bio_ctx = bio_router_register_module(&info);
    if (calcium->bio_ctx) {
        calcium->bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available");
    }

    nimcp_platform_mutex_unlock(calcium->mutex);
    return 0;
}

int calcium_disconnect_bio_async(calcium_dynamics_t calcium) {
    if (!calcium) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium is NULL");

        return -1;

    }

    nimcp_platform_mutex_lock(calcium->mutex);

    if (calcium->bio_async_enabled && calcium->bio_ctx) {
        bio_router_unregister_module(calcium->bio_ctx);
        calcium->bio_ctx = NULL;
        calcium->bio_async_enabled = false;
        NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    }

    nimcp_platform_mutex_unlock(calcium->mutex);
    return 0;
}

bool calcium_is_bio_async_connected(const calcium_dynamics_t calcium) {
    if (!calcium) {
        return false;
    }

    nimcp_platform_mutex_lock(calcium->mutex);
    bool connected = calcium->bio_async_enabled;
    nimcp_platform_mutex_unlock(calcium->mutex);

    return connected;
}

/* ============================================================================
 * Helper Function Implementation
 * ============================================================================ */

float calcium_compute_mg_block(float voltage_mv) {
    /**
     * WHAT: Jahr-Stevens NMDA Mg²⁺ block model
     * WHY:  Voltage-dependent NMDA conductance
     * HOW:  B(V) = 1 / (1 + [Mg²⁺] * exp(-0.062 * V) / 3.57)
     *
     * Reference: Jahr & Stevens (1990) J Neurosci 10:3178-3182
     * [Mg²⁺]_out = 1.0 mM (typical extracellular)
     *
     * NUMERICAL STABILITY:
     * - Clamp exponential argument to prevent overflow/underflow
     * - exp(x) overflows for x > ~88, underflows for x < ~-88
     * - At -100mV: exp(6.2) ≈ 493, at +50mV: exp(-3.1) ≈ 0.045
     */
    const float mg_concentration_mm = 1.0f;
    const float voltage_coeff = 0.062f;  /* 1/mV at room temp */
    const float mg_k_half = 3.57f;        /* mM */

    /* Clamp voltage to physiological range to prevent extreme exponentials
     * WHY:  Voltages outside [-150, +100] mV are non-physiological
     *       At -150mV: exp(9.3) ≈ 10938 (still computable)
     *       At +100mV: exp(-6.2) ≈ 0.002 (small but computable)
     */
    float clamped_voltage = voltage_mv;
    if (clamped_voltage < -150.0f) clamped_voltage = -150.0f;
    if (clamped_voltage > 100.0f) clamped_voltage = 100.0f;

    float exp_arg = -voltage_coeff * clamped_voltage;

    /* Additional safety clamp for exponential argument */
    if (exp_arg > 20.0f) exp_arg = 20.0f;    /* Prevents overflow */
    if (exp_arg < -20.0f) exp_arg = -20.0f;  /* Prevents underflow to denormal */

    float exp_term = expf(exp_arg);

    /* Validate exponential result */
    if (isnan(exp_term) || isinf(exp_term)) {
        /* Return 0 (fully blocked) for invalid input */
        return 0.0f;
    }

    float denominator = 1.0f + (mg_concentration_mm * exp_term / mg_k_half);

    return 1.0f / denominator;
}

float calcium_omega_function(
    float ca_concentration,
    float threshold_ltd,
    float threshold_ltp,
    float omega_max,
    float power
) {
    /**
     * WHAT: Omega function for learning rate
     * WHY:  Maps [Ca²⁺] to learning rate (LTD negative, LTP positive)
     * HOW:  Sigmoidal function centered between theta_LTD and theta_LTP
     *
     * omega([Ca2+]) = omega_max * ((Ca - theta_LTD) / (theta_LTP - theta_LTD))^p
     *
     * Normalized so:
     *   Ca < theta_LTD -> negative (LTD)
     *   Ca = midpoint -> ~0 (transition)
     *   Ca > theta_LTP -> positive (LTP)
     *
     * Reference: Shouval et al. (2002) PNAS 99:10831-10836
     */

    /* CRITICAL: Input validation for numerical stability
     * WHY:  NaN/Inf inputs would propagate and corrupt all calculations
     * FIX:  Return 0 (no learning) for invalid inputs
     */
    if (!isfinite(ca_concentration) || !isfinite(threshold_ltd) ||
        !isfinite(threshold_ltp) || !isfinite(omega_max) || !isfinite(power)) {
        return 0.0f;
    }

    /* CRITICAL: Check for division by zero and near-zero ranges
     * WHY:  If threshold_ltp == threshold_ltd, denominator is zero
     *       Small ranges cause numerical instability in float division
     * FIX:  Return 0 if range is too small relative to the threshold scale
     *       Using 1e-6 as epsilon (larger than 1e-9 for float stability)
     */
    float range = threshold_ltp - threshold_ltd;
    float epsilon = fmaxf(1e-6f, fabsf(threshold_ltp) * 1e-6f);
    if (fabsf(range) < epsilon) {
        /* Range is too small for meaningful omega computation */
        return 0.0f;
    }

    /* Normalize to [0, 1] range */
    float normalized = (ca_concentration - threshold_ltd) / range;

    /* Clamp to prevent extreme values */
    normalized = nimcp_clampf(normalized, -2.0f, 3.0f);

    /* Apply power function */
    float omega = omega_max * powf(fabsf(normalized), power);

    /* Preserve sign for LTD (negative when Ca < midpoint) */
    float midpoint = (threshold_ltd + threshold_ltp) / 2.0f;
    if (ca_concentration < midpoint) {
        omega = -omega;
    }

    return omega;
}
