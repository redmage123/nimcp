/**
 * @file nimcp_cochlea_verification_bridge.c
 * @brief Cochlea bidirectional verification system implementation
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "perception/bridges/nimcp_cochlea_verification_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cochlea_verification_bridge)

#define LOG_MODULE "COCHLEA_VERIFICATION_BRIDGE"

//=============================================================================
// Helpers
//=============================================================================

static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

//=============================================================================
// Bridge Name Table
//=============================================================================

static const char* s_bridge_names[COCHLEA_BRIDGE_COUNT] = {
    "medulla",
    "thalamic",
    "audio_cortex",
    "fep",
    "sleep",
    "immune",
    "kg",
    "substrate",
    "rcog",
    "collective",
    "cortical_deep",
    "occipital",
    "broca",
    "bio_async"
};

//=============================================================================
// Internal Structure
//=============================================================================

struct cochlea_verification_bridge {
    bridge_base_t base;                     /**< MUST be first: base bridge */
    cochlea_verification_config_t config;

    /* Connected cochlea */
    cochlea_t* cochlea;

    /* Per-bridge verification state */
    cochlea_bridge_verify_state_t bridges[COCHLEA_VERIFY_MAX_BRIDGES];
    uint32_t num_registered;

    /* Overall result cache */
    cochlea_verify_result_t last_result;
    bool result_valid;

    /* Timing */
    float time_since_verify_ms;
};

//=============================================================================
// Default Configuration
//=============================================================================

static void cochlea_verification_default_config(cochlea_verification_config_t* config) {
    if (!config) return;
    memset(config, 0, sizeof(*config));
    config->verification_interval_ms = 1000.0f;
    config->freshness_threshold_ms = (float)COCHLEA_VERIFY_FRESHNESS_MS;
    config->latency_warning_ms = 100.0f;
    config->max_consecutive_fails = 5;
    config->auto_reset_on_fail = false;
    config->log_verifications = false;
    config->log_failures_only = true;
}

cochlea_verification_config_t cochlea_verification_config_default(void) {
    cochlea_verification_bridge_heartbeat("config_default", 0.0f);
    cochlea_verification_config_t config;
    cochlea_verification_default_config(&config);
    return config;
}

//=============================================================================
// Lifecycle
//=============================================================================

cochlea_verification_bridge_t* cochlea_verification_bridge_create(
    cochlea_t* cochlea,
    const cochlea_verification_config_t* config
) {
    cochlea_verification_bridge_heartbeat("create", 0.0f);

    cochlea_verification_bridge_t* bridge = (cochlea_verification_bridge_t*)nimcp_calloc(
        1, sizeof(cochlea_verification_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cochlea_verification_bridge_create: alloc failed");
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        cochlea_verification_default_config(&bridge->config);
    }

    if (bridge_base_init(&bridge->base, 0, "cochlea_verification_bridge") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "cochlea_verification_bridge_create: validation failed");
        return NULL;
    }

    bridge->cochlea = cochlea;
    bridge->num_registered = 0;
    bridge->result_valid = false;
    bridge->time_since_verify_ms = 0.0f;

    /* Connect cochlea to system_a */
    if (cochlea) {
        bridge_base_connect_a_unlocked(&bridge->base, cochlea);
    }

    cochlea_verification_bridge_heartbeat("create", 1.0f);
    return bridge;
}

void cochlea_verification_bridge_destroy(cochlea_verification_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "cochlea_verification");
    cochlea_verification_bridge_heartbeat("destroy", 0.0f);
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

nimcp_error_t cochlea_verification_bridge_update(
    cochlea_verification_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_verification_bridge_update: bridge NULL");
        return -1;
    }
    cochlea_verification_bridge_heartbeat("update", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->time_since_verify_ms += dt_ms;

    /* Auto-verify at configured interval */
    if (bridge->config.verification_interval_ms > 0.0f &&
        bridge->time_since_verify_ms >= bridge->config.verification_interval_ms) {
        bridge->time_since_verify_ms = 0.0f;

        /* Run verification on all registered bridges */
        nimcp_mutex_unlock(bridge->base.mutex);
        cochlea_verification_verify_all(bridge);
        nimcp_mutex_lock(bridge->base.mutex);
    }

    bridge_base_record_update(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_verification_bridge_heartbeat("update", 1.0f);
    return 0;
}

nimcp_error_t cochlea_verification_bridge_reset(cochlea_verification_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_verification_bridge_reset: bridge NULL");
        return -1;
    }
    cochlea_verification_bridge_heartbeat("reset", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset all per-bridge stats */
    for (uint32_t i = 0; i < COCHLEA_VERIFY_MAX_BRIDGES; i++) {
        bridge->bridges[i].status = VERIFY_STATUS_UNKNOWN;
        bridge->bridges[i].last_verified_ms = 0;
        bridge->bridges[i].last_outbound_ms = 0;
        bridge->bridges[i].last_inbound_ms = 0;
        bridge->bridges[i].latency_ms = 0.0f;
        bridge->bridges[i].verify_passes = 0;
        bridge->bridges[i].verify_fails = 0;
        bridge->bridges[i].consecutive_fails = 0;
    }

    bridge->result_valid = false;
    bridge->time_since_verify_ms = 0.0f;
    memset(&bridge->last_result, 0, sizeof(bridge->last_result));

    bridge_base_reset_unlocked(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_verification_bridge_heartbeat("reset", 1.0f);
    return 0;
}

//=============================================================================
// Bridge Registration
//=============================================================================

nimcp_error_t cochlea_verification_register(
    cochlea_verification_bridge_t* verifier,
    cochlea_bridge_type_t type,
    void* bridge_ptr,
    bool (*verify_fn)(const void*),
    uint64_t (*get_outbound_fn)(const void*),
    uint64_t (*get_inbound_fn)(const void*)
) {
    if (!verifier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_verification_register: verifier NULL");
        return -1;
    }
    if (!bridge_ptr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_verification_register: bridge_ptr NULL");
        return -1;
    }
    if ((int)type < 0 || type >= COCHLEA_BRIDGE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_verification_register: invalid type");
        return -1;
    }
    cochlea_verification_bridge_heartbeat("register", 0.0f);

    nimcp_mutex_lock(verifier->base.mutex);

    cochlea_bridge_verify_state_t* state = &verifier->bridges[type];
    state->type = type;
    state->name = cochlea_verification_get_bridge_name(type);
    state->bridge_ptr = bridge_ptr;
    state->verify_fn = verify_fn;
    state->get_outbound_fn = get_outbound_fn;
    state->get_inbound_fn = get_inbound_fn;
    state->status = VERIFY_STATUS_UNKNOWN;
    state->last_verified_ms = 0;
    state->last_outbound_ms = 0;
    state->last_inbound_ms = 0;
    state->latency_ms = 0.0f;
    state->verify_passes = 0;
    state->verify_fails = 0;
    state->consecutive_fails = 0;

    if (!state->registered) {
        state->registered = true;
        verifier->num_registered++;
    }

    verifier->result_valid = false;

    nimcp_mutex_unlock(verifier->base.mutex);
    cochlea_verification_bridge_heartbeat("register", 1.0f);
    return 0;
}

nimcp_error_t cochlea_verification_unregister(
    cochlea_verification_bridge_t* verifier,
    cochlea_bridge_type_t type
) {
    if (!verifier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_verification_unregister: verifier NULL");
        return -1;
    }
    if ((int)type < 0 || type >= COCHLEA_BRIDGE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_verification_unregister: invalid type");
        return -1;
    }
    cochlea_verification_bridge_heartbeat("unregister", 0.0f);

    nimcp_mutex_lock(verifier->base.mutex);

    cochlea_bridge_verify_state_t* state = &verifier->bridges[type];
    if (state->registered) {
        state->registered = false;
        state->bridge_ptr = NULL;
        state->verify_fn = NULL;
        state->get_outbound_fn = NULL;
        state->get_inbound_fn = NULL;
        if (verifier->num_registered > 0) {
            verifier->num_registered--;
        }
    }

    verifier->result_valid = false;

    nimcp_mutex_unlock(verifier->base.mutex);
    cochlea_verification_bridge_heartbeat("unregister", 1.0f);
    return 0;
}

bool cochlea_verification_is_registered(
    const cochlea_verification_bridge_t* verifier,
    cochlea_bridge_type_t type
) {
    if (!verifier) {
        return false;
    }
    if ((int)type < 0 || type >= COCHLEA_BRIDGE_COUNT) {
        return false;
    }

    nimcp_mutex_lock(verifier->base.mutex);
    bool result = verifier->bridges[type].registered;
    nimcp_mutex_unlock(verifier->base.mutex);
    return result;
}

//=============================================================================
// Verification
//=============================================================================

nimcp_error_t cochlea_verification_verify_bridge(
    cochlea_verification_bridge_t* verifier,
    cochlea_bridge_type_t type
) {
    if (!verifier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_verification_verify_bridge: verifier NULL");
        return -1;
    }
    if ((int)type < 0 || type >= COCHLEA_BRIDGE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_verification_verify_bridge: invalid type");
        return -1;
    }
    cochlea_verification_bridge_heartbeat("verify_bridge", 0.0f);

    nimcp_mutex_lock(verifier->base.mutex);

    cochlea_bridge_verify_state_t* state = &verifier->bridges[type];
    if (!state->registered) {
        nimcp_mutex_unlock(verifier->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_verification_verify_bridge: state->registered is NULL");
        return -1;
    }

    uint64_t now = get_time_ms();
    state->last_verified_ms = now;

    /* Get timestamps from bridge */
    if (state->get_outbound_fn && state->bridge_ptr) {
        state->last_outbound_ms = state->get_outbound_fn(state->bridge_ptr);
    }
    if (state->get_inbound_fn && state->bridge_ptr) {
        state->last_inbound_ms = state->get_inbound_fn(state->bridge_ptr);
    }

    /* Compute latency */
    if (state->last_outbound_ms > 0 && state->last_inbound_ms > 0) {
        if (state->last_inbound_ms > state->last_outbound_ms) {
            state->latency_ms = (float)(state->last_inbound_ms - state->last_outbound_ms);
        } else {
            state->latency_ms = (float)(state->last_outbound_ms - state->last_inbound_ms);
        }
    }

    /* Run bidirectional verification */
    bool passed = false;
    if (state->verify_fn && state->bridge_ptr) {
        passed = state->verify_fn(state->bridge_ptr);
    }

    /* Check freshness */
    bool fresh = true;
    if (state->last_outbound_ms > 0 && (now - state->last_outbound_ms) > (uint64_t)verifier->config.freshness_threshold_ms) {
        fresh = false;
    }
    if (state->last_inbound_ms > 0 && (now - state->last_inbound_ms) > (uint64_t)verifier->config.freshness_threshold_ms) {
        fresh = false;
    }

    /* Determine status */
    if (passed && fresh) {
        state->status = VERIFY_STATUS_PASSED;
        state->verify_passes++;
        state->consecutive_fails = 0;
    } else if (!fresh && (state->last_outbound_ms > 0 || state->last_inbound_ms > 0)) {
        state->status = VERIFY_STATUS_STALE;
        state->verify_fails++;
        state->consecutive_fails++;
    } else {
        state->status = VERIFY_STATUS_FAILED;
        state->verify_fails++;
        state->consecutive_fails++;
    }

    verifier->result_valid = false;

    nimcp_mutex_unlock(verifier->base.mutex);
    cochlea_verification_bridge_heartbeat("verify_bridge", 1.0f);
    return 0;
}

nimcp_error_t cochlea_verification_verify_all(
    cochlea_verification_bridge_t* verifier
) {
    if (!verifier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_verification_verify_all: verifier NULL");
        return -1;
    }
    cochlea_verification_bridge_heartbeat("verify_all", 0.0f);

    for (int i = 0; i < COCHLEA_BRIDGE_COUNT; i++) {
        if (verifier->bridges[i].registered) {
            cochlea_verification_verify_bridge(verifier, (cochlea_bridge_type_t)i);
        }
    }

    /* Rebuild cached result */
    nimcp_mutex_lock(verifier->base.mutex);

    cochlea_verify_result_t* r = &verifier->last_result;
    memset(r, 0, sizeof(*r));

    r->total_bridges = verifier->num_registered;
    r->verification_time_ms = get_time_ms();
    r->all_bidirectional = true;

    float total_latency = 0.0f;
    uint32_t latency_count = 0;

    for (int i = 0; i < COCHLEA_BRIDGE_COUNT; i++) {
        cochlea_bridge_verify_state_t* s = &verifier->bridges[i];
        if (!s->registered) continue;

        if (s->status == VERIFY_STATUS_PASSED) {
            r->passed_bridges++;
        } else if (s->status == VERIFY_STATUS_STALE) {
            r->stale_bridges++;
            r->all_bidirectional = false;
        } else if (s->status == VERIFY_STATUS_FAILED || s->status == VERIFY_STATUS_TIMEOUT) {
            r->failed_bridges++;
            r->all_bidirectional = false;
        } else {
            r->all_bidirectional = false;
        }

        if (s->latency_ms > 0.0f) {
            total_latency += s->latency_ms;
            latency_count++;
            if (s->latency_ms > r->max_latency_ms) {
                r->max_latency_ms = s->latency_ms;
            }
        }
    }

    if (latency_count > 0) {
        r->avg_latency_ms = total_latency / (float)latency_count;
    }

    if (r->total_bridges > 0) {
        r->overall_health = (float)r->passed_bridges / (float)r->total_bridges;
    } else {
        r->overall_health = 0.0f;
    }

    verifier->result_valid = true;

    nimcp_mutex_unlock(verifier->base.mutex);
    cochlea_verification_bridge_heartbeat("verify_all", 1.0f);
    return 0;
}

nimcp_error_t cochlea_verification_get_bridge_status(
    const cochlea_verification_bridge_t* verifier,
    cochlea_bridge_type_t type,
    cochlea_bridge_verify_state_t* state
) {
    if (!verifier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_verification_get_bridge_status: verifier NULL");
        return -1;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_verification_get_bridge_status: state NULL");
        return -1;
    }
    if ((int)type < 0 || type >= COCHLEA_BRIDGE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_verification_get_bridge_status: invalid type");
        return -1;
    }
    cochlea_verification_bridge_heartbeat("get_bridge_status", 0.0f);

    nimcp_mutex_lock(verifier->base.mutex);
    *state = verifier->bridges[type];
    nimcp_mutex_unlock(verifier->base.mutex);

    return 0;
}

nimcp_error_t cochlea_verification_get_result(
    const cochlea_verification_bridge_t* verifier,
    cochlea_verify_result_t* result
) {
    if (!verifier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_verification_get_result: verifier NULL");
        return -1;
    }
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_verification_get_result: result NULL");
        return -1;
    }
    cochlea_verification_bridge_heartbeat("get_result", 0.0f);

    nimcp_mutex_lock(verifier->base.mutex);
    *result = verifier->last_result;
    nimcp_mutex_unlock(verifier->base.mutex);

    return 0;
}

//=============================================================================
// Status Queries
//=============================================================================

bool cochlea_verification_all_bidirectional(
    const cochlea_verification_bridge_t* verifier
) {
    if (!verifier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_verification_all_bidirectional: verifier is NULL");
        return false;
    }
    cochlea_verification_bridge_heartbeat("all_bidirectional", 0.0f);

    nimcp_mutex_lock(verifier->base.mutex);

    if (!verifier->result_valid || verifier->num_registered == 0) {
        nimcp_mutex_unlock(verifier->base.mutex);
        /* No valid results yet - not an error, just return false */
        return false;
    }

    bool result = verifier->last_result.all_bidirectional;
    nimcp_mutex_unlock(verifier->base.mutex);
    return result;
}

uint32_t cochlea_verification_get_passing_count(
    const cochlea_verification_bridge_t* verifier
) {
    if (!verifier) return 0;

    nimcp_mutex_lock(verifier->base.mutex);

    uint32_t count = 0;
    for (int i = 0; i < COCHLEA_BRIDGE_COUNT; i++) {
        if (verifier->bridges[i].registered &&
            verifier->bridges[i].status == VERIFY_STATUS_PASSED) {
            count++;
        }
    }

    nimcp_mutex_unlock(verifier->base.mutex);
    return count;
}

uint32_t cochlea_verification_get_failing_count(
    const cochlea_verification_bridge_t* verifier
) {
    if (!verifier) return 0;

    nimcp_mutex_lock(verifier->base.mutex);

    uint32_t count = 0;
    for (int i = 0; i < COCHLEA_BRIDGE_COUNT; i++) {
        if (verifier->bridges[i].registered &&
            (verifier->bridges[i].status == VERIFY_STATUS_FAILED ||
             verifier->bridges[i].status == VERIFY_STATUS_TIMEOUT)) {
            count++;
        }
    }

    nimcp_mutex_unlock(verifier->base.mutex);
    return count;
}

float cochlea_verification_get_health(
    const cochlea_verification_bridge_t* verifier
) {
    if (!verifier) return 0.0f;

    nimcp_mutex_lock(verifier->base.mutex);

    float health = 0.0f;
    if (verifier->result_valid) {
        health = verifier->last_result.overall_health;
    }

    nimcp_mutex_unlock(verifier->base.mutex);
    return health;
}

//=============================================================================
// Latency
//=============================================================================

float cochlea_verification_get_latency(
    const cochlea_verification_bridge_t* verifier,
    cochlea_bridge_type_t type
) {
    if (!verifier) return 0.0f;
    if ((int)type < 0 || type >= COCHLEA_BRIDGE_COUNT) return 0.0f;

    nimcp_mutex_lock(verifier->base.mutex);
    float latency = verifier->bridges[type].latency_ms;
    nimcp_mutex_unlock(verifier->base.mutex);
    return latency;
}

float cochlea_verification_get_avg_latency(
    const cochlea_verification_bridge_t* verifier
) {
    if (!verifier) return 0.0f;

    nimcp_mutex_lock(verifier->base.mutex);

    float total = 0.0f;
    uint32_t count = 0;

    for (int i = 0; i < COCHLEA_BRIDGE_COUNT; i++) {
        if (verifier->bridges[i].registered && verifier->bridges[i].latency_ms > 0.0f) {
            total += verifier->bridges[i].latency_ms;
            count++;
        }
    }

    nimcp_mutex_unlock(verifier->base.mutex);
    return (count > 0) ? (total / (float)count) : 0.0f;
}

//=============================================================================
// Bridge Name Utility
//=============================================================================

const char* cochlea_verification_get_bridge_name(cochlea_bridge_type_t type) {
    if ((int)type < 0 || type >= COCHLEA_BRIDGE_COUNT) return "unknown";
    return s_bridge_names[type];
}
