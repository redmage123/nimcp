/**
 * @file nimcp_safety_watchdog.c
 * @brief Safety watchdog for robot/drone actuator control.
 *
 * Monitors brain heartbeats and output validity. If the brain goes silent,
 * crashes, or produces garbage (NaN, out-of-range), the watchdog triggers
 * a safe action — typically zeroing all motor outputs.
 *
 * This is safety-critical code. All paths are designed to fail safe:
 * any internal error defaults to stopping motors (zero output).
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#include "edge/nimcp_safety_watchdog.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"

#include <string.h>
#include <math.h>

#define LOG_MODULE "SAFETY_WATCHDOG"

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct nimcp_safety_watchdog {
    nimcp_watchdog_config_t config;
    nimcp_watchdog_state_t state;

    /* Timing */
    uint64_t last_heartbeat_ts;

    /* Output tracking */
    float* last_valid_output;       /* Most recent valid output */
    float* previous_output;         /* Output before last (for rate check) */
    uint32_t output_dim;
    uint32_t consecutive_nan_count;
    uint64_t last_output_ts;

    /* Thread safety */
    nimcp_mutex_t* lock;

    /* Watchdog timer thread */
    nimcp_thread_t timer_thread;
    volatile bool timer_running;
    volatile bool timer_started;
};

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static void* _watchdog_timer_func(void* arg);
static void _execute_safe_action(nimcp_safety_watchdog_t* wd);

/* ============================================================================
 * Default Config
 * ============================================================================ */

nimcp_watchdog_config_t nimcp_watchdog_config_default(void) {
    nimcp_watchdog_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.timeout_ms = 500;
    cfg.action = NIMCP_SAFE_ACTION_STOP;
    cfg.max_outputs = 32;

    cfg.validation.max_output_magnitude = 1.0f;
    cfg.validation.max_output_rate = 10.0f;
    cfg.validation.consecutive_nan_limit = 3;
    cfg.validation.check_nan = true;
    cfg.validation.check_magnitude = true;
    cfg.validation.check_rate = true;

    cfg.safe_pose_dim = 0;
    cfg.estop_callback = NULL;
    cfg.estop_user_data = NULL;

    return cfg;
}

/* ============================================================================
 * State Name
 * ============================================================================ */

const char* nimcp_watchdog_state_name(nimcp_watchdog_state_t state) {
    switch (state) {
        case NIMCP_WATCHDOG_IDLE:      return "IDLE";
        case NIMCP_WATCHDOG_ARMED:     return "ARMED";
        case NIMCP_WATCHDOG_TRIGGERED: return "TRIGGERED";
        case NIMCP_WATCHDOG_ESTOP:     return "ESTOP";
        default:                       return "UNKNOWN";
    }
}

/* ============================================================================
 * Create / Destroy
 * ============================================================================ */

nimcp_safety_watchdog_t* nimcp_watchdog_create(const nimcp_watchdog_config_t* config) {
    nimcp_safety_watchdog_t* wd = (nimcp_safety_watchdog_t*)nimcp_calloc(
        1, sizeof(nimcp_safety_watchdog_t));
    if (!wd) {
        LOG_ERROR("[%s] Failed to allocate watchdog", LOG_MODULE);
        return NULL;
    }

    /* Apply config (use defaults if NULL) */
    if (config) {
        memcpy(&wd->config, config, sizeof(nimcp_watchdog_config_t));
    } else {
        wd->config = nimcp_watchdog_config_default();
    }

    /* Clamp max_outputs to a sane range */
    if (wd->config.max_outputs == 0) {
        wd->config.max_outputs = 32;
    }
    if (wd->config.max_outputs > 256) {
        LOG_WARN("[%s] max_outputs %u clamped to 256",
                 LOG_MODULE, wd->config.max_outputs);
        wd->config.max_outputs = 256;
    }

    /* Ensure timeout is reasonable */
    if (wd->config.timeout_ms < 10) {
        LOG_WARN("[%s] timeout_ms %u too low, clamped to 10ms",
                 LOG_MODULE, wd->config.timeout_ms);
        wd->config.timeout_ms = 10;
    }

    /* Allocate output tracking buffers */
    uint32_t dim = wd->config.max_outputs;
    wd->last_valid_output = (float*)nimcp_calloc(dim, sizeof(float));
    wd->previous_output = (float*)nimcp_calloc(dim, sizeof(float));
    if (!wd->last_valid_output || !wd->previous_output) {
        LOG_ERROR("[%s] Failed to allocate output buffers", LOG_MODULE);
        nimcp_free(wd->last_valid_output);
        nimcp_free(wd->previous_output);
        nimcp_free(wd);
        return NULL;
    }
    wd->output_dim = dim;

    /* Create mutex */
    wd->lock = nimcp_mutex_create(NULL);
    if (!wd->lock) {
        LOG_ERROR("[%s] Failed to create mutex", LOG_MODULE);
        nimcp_free(wd->last_valid_output);
        nimcp_free(wd->previous_output);
        nimcp_free(wd);
        return NULL;
    }

    wd->state = NIMCP_WATCHDOG_IDLE;
    wd->timer_running = false;
    wd->timer_started = false;
    wd->consecutive_nan_count = 0;
    wd->last_heartbeat_ts = 0;
    wd->last_output_ts = 0;

    LOG_INFO("[%s] Created (timeout=%ums, action=%s, max_outputs=%u)",
             LOG_MODULE, wd->config.timeout_ms,
             wd->config.action == NIMCP_SAFE_ACTION_STOP ? "STOP" :
             wd->config.action == NIMCP_SAFE_ACTION_HOLD ? "HOLD" :
             wd->config.action == NIMCP_SAFE_ACTION_POSE ? "POSE" :
             wd->config.action == NIMCP_SAFE_ACTION_LAND ? "LAND" : "CALLBACK",
             wd->config.max_outputs);

    return wd;
}

void nimcp_watchdog_destroy(nimcp_safety_watchdog_t* watchdog) {
    if (!watchdog) {
        return;
    }

    /* Stop timer thread if running */
    if (watchdog->timer_running) {
        nimcp_watchdog_disarm(watchdog);
    }

    nimcp_free(watchdog->last_valid_output);
    nimcp_free(watchdog->previous_output);
    nimcp_mutex_free(watchdog->lock);
    nimcp_free(watchdog);

    LOG_INFO("[%s] Destroyed", LOG_MODULE);
}

/* ============================================================================
 * Arm / Disarm
 * ============================================================================ */

int nimcp_watchdog_arm(nimcp_safety_watchdog_t* watchdog) {
    if (!watchdog) {
        return -1;
    }

    nimcp_mutex_lock(watchdog->lock);

    if (watchdog->state == NIMCP_WATCHDOG_ARMED) {
        nimcp_mutex_unlock(watchdog->lock);
        return 0; /* Already armed */
    }

    if (watchdog->state == NIMCP_WATCHDOG_ESTOP) {
        LOG_WARN("[%s] Cannot arm from ESTOP state — reset first", LOG_MODULE);
        nimcp_mutex_unlock(watchdog->lock);
        return -1;
    }

    /* Reset counters */
    watchdog->consecutive_nan_count = 0;
    watchdog->last_heartbeat_ts = nimcp_time_now_us();
    watchdog->last_output_ts = 0;
    memset(watchdog->last_valid_output, 0,
           watchdog->output_dim * sizeof(float));
    memset(watchdog->previous_output, 0,
           watchdog->output_dim * sizeof(float));

    watchdog->state = NIMCP_WATCHDOG_ARMED;
    watchdog->timer_running = true;
    watchdog->timer_started = false;

    nimcp_mutex_unlock(watchdog->lock);

    /* Start timer thread */
    nimcp_result_t rc = nimcp_thread_create(
        &watchdog->timer_thread, _watchdog_timer_func, watchdog, NULL);
    if (rc != 0) {
        LOG_ERROR("[%s] Failed to create timer thread (rc=%d)", LOG_MODULE, rc);
        nimcp_mutex_lock(watchdog->lock);
        watchdog->state = NIMCP_WATCHDOG_IDLE;
        watchdog->timer_running = false;
        nimcp_mutex_unlock(watchdog->lock);
        return -1;
    }

    LOG_INFO("[%s] Armed (timeout=%ums)", LOG_MODULE, watchdog->config.timeout_ms);
    return 0;
}

int nimcp_watchdog_disarm(nimcp_safety_watchdog_t* watchdog) {
    if (!watchdog) {
        return -1;
    }

    /* Signal timer thread to stop */
    watchdog->timer_running = false;

    /* Wait for timer thread to finish (if it was started) */
    if (watchdog->timer_started) {
        nimcp_thread_join(watchdog->timer_thread, NULL);
        watchdog->timer_started = false;
    }

    nimcp_mutex_lock(watchdog->lock);
    watchdog->state = NIMCP_WATCHDOG_IDLE;
    watchdog->consecutive_nan_count = 0;
    nimcp_mutex_unlock(watchdog->lock);

    LOG_INFO("[%s] Disarmed", LOG_MODULE);
    return 0;
}

/* ============================================================================
 * Heartbeat
 * ============================================================================ */

void nimcp_watchdog_heartbeat(nimcp_safety_watchdog_t* watchdog) {
    if (!watchdog) {
        return;
    }

    nimcp_mutex_lock(watchdog->lock);

    if (watchdog->state == NIMCP_WATCHDOG_ARMED) {
        watchdog->last_heartbeat_ts = nimcp_time_now_us();
    }
    /* Heartbeats are silently ignored in non-ARMED states */

    nimcp_mutex_unlock(watchdog->lock);
}

/* ============================================================================
 * Output Validation
 * ============================================================================ */

int nimcp_watchdog_validate_output(nimcp_safety_watchdog_t* watchdog,
                                   float* output, uint32_t num_outputs) {
    if (!watchdog || !output || num_outputs == 0) {
        return -1;
    }

    nimcp_mutex_lock(watchdog->lock);

    /* Only validate when ARMED */
    if (watchdog->state != NIMCP_WATCHDOG_ARMED) {
        nimcp_mutex_unlock(watchdog->lock);
        return -1;
    }

    uint32_t dim = (num_outputs < watchdog->output_dim)
                   ? num_outputs : watchdog->output_dim;
    uint64_t now = nimcp_time_now_us();
    int result = 0;
    bool has_nan = false;

    /* --- Check 1: NaN / Inf --- */
    if (watchdog->config.validation.check_nan) {
        for (uint32_t i = 0; i < dim; i++) {
            if (isnan(output[i]) || isinf(output[i])) {
                has_nan = true;
                output[i] = 0.0f; /* Sanitize in-place */
            }
        }
        if (has_nan) {
            watchdog->consecutive_nan_count++;
            LOG_WARN("[%s] NaN/Inf detected in output (consecutive=%u/%u)",
                     LOG_MODULE, watchdog->consecutive_nan_count,
                     watchdog->config.validation.consecutive_nan_limit);

            if (watchdog->consecutive_nan_count >=
                watchdog->config.validation.consecutive_nan_limit) {
                LOG_ERROR("[%s] Consecutive NaN limit reached — triggering safe action",
                          LOG_MODULE);
                watchdog->state = NIMCP_WATCHDOG_TRIGGERED;
                _execute_safe_action(watchdog);
                nimcp_mutex_unlock(watchdog->lock);
                return -1;
            }
            result = -1;
        } else {
            watchdog->consecutive_nan_count = 0;
        }
    }

    /* --- Check 2: Magnitude --- */
    if (watchdog->config.validation.check_magnitude) {
        float max_mag = watchdog->config.validation.max_output_magnitude;
        for (uint32_t i = 0; i < dim; i++) {
            if (output[i] > max_mag) {
                LOG_WARN("[%s] Output[%u]=%.4f exceeds max magnitude %.4f, clamping",
                         LOG_MODULE, i, (double)output[i], (double)max_mag);
                output[i] = max_mag;
                result = -1;
            } else if (output[i] < -max_mag) {
                LOG_WARN("[%s] Output[%u]=%.4f exceeds max magnitude %.4f, clamping",
                         LOG_MODULE, i, (double)output[i], (double)max_mag);
                output[i] = -max_mag;
                result = -1;
            }
        }
    }

    /* --- Check 3: Rate of change --- */
    if (watchdog->config.validation.check_rate && watchdog->last_output_ts > 0) {
        float dt_sec = (float)(now - watchdog->last_output_ts) / 1000000.0f;
        if (dt_sec > 0.0001f) { /* Avoid division by near-zero */
            float max_rate = watchdog->config.validation.max_output_rate;
            for (uint32_t i = 0; i < dim; i++) {
                float rate = fabsf(output[i] - watchdog->previous_output[i]) / dt_sec;
                if (rate > max_rate) {
                    LOG_WARN("[%s] Output[%u] rate=%.2f/s exceeds limit %.2f/s",
                             LOG_MODULE, i, (double)rate, (double)max_rate);
                    /* Rate violations are logged but do NOT trigger safe action.
                     * They serve as early warning for erratic behavior. */
                    result = -1;
                }
            }
        }
    }

    /* Save as previous/last_valid if no NaN was found */
    if (!has_nan) {
        memcpy(watchdog->previous_output, watchdog->last_valid_output,
               dim * sizeof(float));
        memcpy(watchdog->last_valid_output, output, dim * sizeof(float));
    }
    watchdog->last_output_ts = now;

    nimcp_mutex_unlock(watchdog->lock);
    return result;
}

/* ============================================================================
 * Get Safe Output
 * ============================================================================ */

int nimcp_watchdog_get_safe_output(nimcp_safety_watchdog_t* watchdog,
                                   float* output, uint32_t num_outputs) {
    if (!watchdog || !output || num_outputs == 0) {
        /* Fail safe: zero the output even if watchdog is NULL */
        if (output && num_outputs > 0) {
            memset(output, 0, num_outputs * sizeof(float));
        }
        return -1;
    }

    nimcp_mutex_lock(watchdog->lock);

    uint32_t dim = (num_outputs < watchdog->output_dim)
                   ? num_outputs : watchdog->output_dim;

    switch (watchdog->config.action) {
        case NIMCP_SAFE_ACTION_HOLD:
            /* Return last valid output */
            memcpy(output, watchdog->last_valid_output, dim * sizeof(float));
            /* Zero any extra outputs beyond what we track */
            if (num_outputs > dim) {
                memset(output + dim, 0, (num_outputs - dim) * sizeof(float));
            }
            break;

        case NIMCP_SAFE_ACTION_POSE:
            /* Return configured safe pose */
            if (watchdog->config.safe_pose_dim > 0) {
                uint32_t pose_dim = (dim < watchdog->config.safe_pose_dim)
                                    ? dim : watchdog->config.safe_pose_dim;
                memcpy(output, watchdog->config.safe_pose,
                       pose_dim * sizeof(float));
                /* Zero remaining */
                if (num_outputs > pose_dim) {
                    memset(output + pose_dim, 0,
                           (num_outputs - pose_dim) * sizeof(float));
                }
            } else {
                /* No safe pose configured — fall back to zero */
                memset(output, 0, num_outputs * sizeof(float));
            }
            break;

        case NIMCP_SAFE_ACTION_STOP:
        case NIMCP_SAFE_ACTION_LAND:
        case NIMCP_SAFE_ACTION_CALLBACK:
        default:
            /* Zero all outputs */
            memset(output, 0, num_outputs * sizeof(float));
            break;
    }

    nimcp_mutex_unlock(watchdog->lock);
    return 0;
}

/* ============================================================================
 * Emergency Stop / Reset / Get State
 * ============================================================================ */

void nimcp_watchdog_estop(nimcp_safety_watchdog_t* watchdog) {
    if (!watchdog) {
        return;
    }

    nimcp_mutex_lock(watchdog->lock);

    nimcp_watchdog_state_t prev = watchdog->state;
    watchdog->state = NIMCP_WATCHDOG_ESTOP;
    _execute_safe_action(watchdog);

    LOG_ERROR("[%s] EMERGENCY STOP activated (was %s). Manual reset required.",
              LOG_MODULE, nimcp_watchdog_state_name(prev));

    nimcp_mutex_unlock(watchdog->lock);
}

int nimcp_watchdog_reset(nimcp_safety_watchdog_t* watchdog) {
    if (!watchdog) {
        return -1;
    }

    nimcp_mutex_lock(watchdog->lock);

    if (watchdog->state == NIMCP_WATCHDOG_IDLE) {
        nimcp_mutex_unlock(watchdog->lock);
        return 0; /* Already idle */
    }

    LOG_INFO("[%s] Reset from %s to IDLE",
             LOG_MODULE, nimcp_watchdog_state_name(watchdog->state));

    watchdog->state = NIMCP_WATCHDOG_IDLE;
    watchdog->consecutive_nan_count = 0;

    /* Stop timer thread if running */
    if (watchdog->timer_running) {
        watchdog->timer_running = false;
        nimcp_mutex_unlock(watchdog->lock);
        if (watchdog->timer_started) {
            nimcp_thread_join(watchdog->timer_thread, NULL);
            watchdog->timer_started = false;
        }
        return 0;
    }

    nimcp_mutex_unlock(watchdog->lock);
    return 0;
}

nimcp_watchdog_state_t nimcp_watchdog_get_state(nimcp_safety_watchdog_t* watchdog) {
    if (!watchdog) {
        return NIMCP_WATCHDOG_IDLE;
    }

    nimcp_mutex_lock(watchdog->lock);
    nimcp_watchdog_state_t s = watchdog->state;
    nimcp_mutex_unlock(watchdog->lock);
    return s;
}

/* ============================================================================
 * Internal: Execute Safe Action
 * ============================================================================ */

/**
 * Called with lock HELD. Executes the configured safe action.
 * For CALLBACK/LAND, calls user function (which may take time — be aware).
 */
static void _execute_safe_action(nimcp_safety_watchdog_t* wd) {
    switch (wd->config.action) {
        case NIMCP_SAFE_ACTION_STOP:
            /* Zero last_valid_output so get_safe_output returns zeros */
            memset(wd->last_valid_output, 0,
                   wd->output_dim * sizeof(float));
            LOG_WARN("[%s] Safe action: STOP — all outputs zeroed", LOG_MODULE);
            break;

        case NIMCP_SAFE_ACTION_HOLD:
            LOG_WARN("[%s] Safe action: HOLD — holding last valid output",
                     LOG_MODULE);
            /* last_valid_output already contains the right values */
            break;

        case NIMCP_SAFE_ACTION_POSE:
            LOG_WARN("[%s] Safe action: POSE — using configured safe pose",
                     LOG_MODULE);
            /* Safe pose is in config; get_safe_output reads it from there */
            break;

        case NIMCP_SAFE_ACTION_LAND:
        case NIMCP_SAFE_ACTION_CALLBACK:
            if (wd->config.estop_callback) {
                LOG_WARN("[%s] Safe action: %s — calling user handler",
                         LOG_MODULE,
                         wd->config.action == NIMCP_SAFE_ACTION_LAND
                         ? "LAND" : "CALLBACK");
                /*
                 * SAFETY NOTE: We release the lock before calling the user
                 * callback to prevent deadlocks if the callback calls back
                 * into the watchdog API. The state has already been set to
                 * TRIGGERED/ESTOP, so the watchdog is in a stable state.
                 */
                nimcp_mutex_unlock(wd->lock);
                wd->config.estop_callback(wd->config.estop_user_data);
                nimcp_mutex_lock(wd->lock);
            } else {
                /* No callback registered — fall back to STOP */
                LOG_WARN("[%s] Safe action: %s — no callback registered, "
                         "falling back to STOP",
                         LOG_MODULE,
                         wd->config.action == NIMCP_SAFE_ACTION_LAND
                         ? "LAND" : "CALLBACK");
                memset(wd->last_valid_output, 0,
                       wd->output_dim * sizeof(float));
            }
            break;

        default:
            /* Unknown action — fail safe: zero everything */
            memset(wd->last_valid_output, 0,
                   wd->output_dim * sizeof(float));
            LOG_ERROR("[%s] Unknown safe action %d — defaulting to STOP",
                      LOG_MODULE, (int)wd->config.action);
            break;
    }
}

/* ============================================================================
 * Internal: Timer Thread
 * ============================================================================ */

/**
 * Background thread that monitors heartbeat timing.
 * Checks every timeout_ms/4 whether the brain has missed its deadline.
 */
static void* _watchdog_timer_func(void* arg) {
    nimcp_safety_watchdog_t* wd = (nimcp_safety_watchdog_t*)arg;
    if (!wd) {
        return NULL;
    }

    wd->timer_started = true;

    /* Check interval: 1/4 of timeout for responsive detection */
    uint32_t check_interval_us = (wd->config.timeout_ms * 1000) / 4;
    if (check_interval_us < 1000) {
        check_interval_us = 1000; /* Floor: 1ms */
    }

    while (wd->timer_running) {
        usleep(check_interval_us);

        if (!wd->timer_running) {
            break;
        }

        nimcp_mutex_lock(wd->lock);

        if (wd->state == NIMCP_WATCHDOG_ARMED) {
            uint64_t now = nimcp_time_now_us();
            uint64_t elapsed_us = now - wd->last_heartbeat_ts;
            uint64_t timeout_us = (uint64_t)wd->config.timeout_ms * 1000;

            if (elapsed_us > timeout_us) {
                float elapsed_ms = (float)elapsed_us / 1000.0f;
                LOG_ERROR("[%s] HEARTBEAT TIMEOUT — elapsed=%.1fms, "
                          "limit=%ums. Triggering safe action.",
                          LOG_MODULE, (double)elapsed_ms,
                          wd->config.timeout_ms);

                wd->state = NIMCP_WATCHDOG_TRIGGERED;
                _execute_safe_action(wd);
            }
        }

        nimcp_mutex_unlock(wd->lock);
    }

    return NULL;
}
