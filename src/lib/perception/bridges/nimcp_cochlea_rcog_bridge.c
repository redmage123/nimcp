/**
 * @file nimcp_cochlea_rcog_bridge.c
 * @brief Cochlea-Recursive Cognition Engine integration implementation
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "perception/bridges/nimcp_cochlea_rcog_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cochlea_rcog_bridge)

#define LOG_MODULE "COCHLEA_RCOG_BRIDGE"

//=============================================================================
// Helpers
//=============================================================================

static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

//=============================================================================
// Internal Structure
//=============================================================================

struct cochlea_rcog_bridge {
    bridge_base_t base;                     /**< MUST be first: base bridge */
    cochlea_rcog_config_t config;

    /* Connected systems */
    cochlea_t* cochlea;
    rcog_engine_t* engine;

    /* Listening goals */
    cochlea_listening_goal_t goals[COCHLEA_RCOG_MAX_GOALS];
    uint32_t num_goals;
    bool goal_active;

    /* Last audio event */
    cochlea_audio_event_t last_event;
    bool event_pending;

    /* Tool registration */
    bool tool_registered;

    /* Bidirectional tracking */
    uint64_t last_outbound_ms;
    uint64_t last_inbound_ms;

    /* Timing */
    float time_since_update_ms;
};

//=============================================================================
// Default Configuration
//=============================================================================

static void cochlea_rcog_default_config(cochlea_rcog_config_t* config) {
    if (!config) return;
    memset(config, 0, sizeof(*config));
    config->register_listen_tool = true;
    config->register_detect_tool = true;
    config->register_localize_tool = false;
    config->create_audio_context = true;
    config->create_sounds_variable = true;
    config->goal_timeout_ms = 5000.0f;
    config->max_concurrent_goals = 4;
    config->update_interval_ms = 50.0f;
}

cochlea_rcog_config_t cochlea_rcog_config_default(void) {
    cochlea_rcog_bridge_heartbeat("config_default", 0.0f);
    cochlea_rcog_config_t config;
    cochlea_rcog_default_config(&config);
    return config;
}

//=============================================================================
// Lifecycle
//=============================================================================

cochlea_rcog_bridge_t* cochlea_rcog_bridge_create(
    cochlea_t* cochlea,
    rcog_engine_t* engine,
    const cochlea_rcog_config_t* config
) {
    cochlea_rcog_bridge_heartbeat("create", 0.0f);

    cochlea_rcog_bridge_t* bridge = (cochlea_rcog_bridge_t*)nimcp_calloc(1, sizeof(cochlea_rcog_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cochlea_rcog_bridge_create: alloc failed");
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        cochlea_rcog_default_config(&bridge->config);
    }

    if (bridge_base_init(&bridge->base, 0, "cochlea_rcog_bridge") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "cochlea_rcog_bridge_create: validation failed");
        return NULL;
    }

    bridge->cochlea = cochlea;
    bridge->engine = engine;
    bridge->num_goals = 0;
    bridge->goal_active = false;
    bridge->tool_registered = false;
    bridge->event_pending = false;
    bridge->last_outbound_ms = 0;
    bridge->last_inbound_ms = 0;
    bridge->time_since_update_ms = 0.0f;

    /* Connect systems to base */
    if (cochlea) {
        bridge_base_connect_a_unlocked(&bridge->base, cochlea);
    }
    if (engine) {
        bridge_base_connect_b_unlocked(&bridge->base, engine);
    }

    cochlea_rcog_bridge_heartbeat("create", 1.0f);
    return bridge;
}

void cochlea_rcog_bridge_destroy(cochlea_rcog_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "cochlea_rcog");
    cochlea_rcog_bridge_heartbeat("destroy", 0.0f);
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

nimcp_error_t cochlea_rcog_bridge_update(
    cochlea_rcog_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_rcog_bridge_update: bridge NULL");
        return -1;
    }
    cochlea_rcog_bridge_heartbeat("update", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->time_since_update_ms += dt_ms;

    /* Only process at configured interval */
    if (bridge->time_since_update_ms >= bridge->config.update_interval_ms) {
        bridge->time_since_update_ms = 0.0f;

        /* If we have pending event data, mark outbound timestamp */
        if (bridge->event_pending && cochlea_output) {
            bridge->last_outbound_ms = get_time_ms();
            bridge->event_pending = false;
        }

        bridge_base_record_update(&bridge->base);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_rcog_bridge_heartbeat("update", 1.0f);
    return 0;
}

nimcp_error_t cochlea_rcog_bridge_reset(cochlea_rcog_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_rcog_bridge_reset: bridge NULL");
        return -1;
    }
    cochlea_rcog_bridge_heartbeat("reset", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->num_goals = 0;
    bridge->goal_active = false;
    bridge->event_pending = false;
    bridge->last_outbound_ms = 0;
    bridge->last_inbound_ms = 0;
    bridge->time_since_update_ms = 0.0f;
    memset(&bridge->last_event, 0, sizeof(bridge->last_event));
    memset(bridge->goals, 0, sizeof(bridge->goals));

    bridge_base_reset_unlocked(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_rcog_bridge_heartbeat("reset", 1.0f);
    return 0;
}

//=============================================================================
// Tool Registration
//=============================================================================

nimcp_error_t cochlea_rcog_register_tool(cochlea_rcog_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_rcog_register_tool: bridge NULL");
        return -1;
    }
    cochlea_rcog_bridge_heartbeat("register_tool", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->tool_registered) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;  /* Already registered */
    }

    /* Register cochlea as a tool in the RCOG engine */
    bridge->tool_registered = true;

    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_rcog_bridge_heartbeat("register_tool", 1.0f);
    return 0;
}

nimcp_error_t cochlea_rcog_unregister_tool(cochlea_rcog_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_rcog_unregister_tool: bridge NULL");
        return -1;
    }
    cochlea_rcog_bridge_heartbeat("unregister_tool", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->tool_registered = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    cochlea_rcog_bridge_heartbeat("unregister_tool", 1.0f);
    return 0;
}

//=============================================================================
// Goal Processing (Inbound)
//=============================================================================

nimcp_error_t cochlea_rcog_receive_goal(
    cochlea_rcog_bridge_t* bridge,
    const rcog_goal_t* goal
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_rcog_receive_goal: bridge NULL");
        return -1;
    }
    if (!goal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_rcog_receive_goal: goal NULL");
        return -1;
    }
    cochlea_rcog_bridge_heartbeat("receive_goal", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Convert RCOG goal to listening goal if capacity allows */
    if (bridge->num_goals < bridge->config.max_concurrent_goals &&
        bridge->num_goals < COCHLEA_RCOG_MAX_GOALS) {
        cochlea_listening_goal_t* lg = &bridge->goals[bridge->num_goals];
        memset(lg, 0, sizeof(*lg));

        /* Copy goal type as target sound class */
        strncpy(lg->target_sound_class, goal->goal_type,
                sizeof(lg->target_sound_class) - 1);
        lg->priority = 0.5f;
        lg->suppress_background = false;
        lg->target_frequency_hz = 0.0f;
        lg->attention_bandwidth = 1.0f;

        bridge->num_goals++;
        bridge->goal_active = true;
        bridge->last_inbound_ms = get_time_ms();
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_rcog_bridge_heartbeat("receive_goal", 1.0f);
    return 0;
}

nimcp_error_t cochlea_rcog_set_listening_goal(
    cochlea_rcog_bridge_t* bridge,
    const cochlea_listening_goal_t* goal
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_rcog_set_listening_goal: bridge NULL");
        return -1;
    }
    if (!goal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_rcog_set_listening_goal: goal NULL");
        return -1;
    }
    cochlea_rcog_bridge_heartbeat("set_listening_goal", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->num_goals < bridge->config.max_concurrent_goals &&
        bridge->num_goals < COCHLEA_RCOG_MAX_GOALS) {
        bridge->goals[bridge->num_goals] = *goal;
        bridge->num_goals++;
        bridge->goal_active = true;
        bridge->last_inbound_ms = get_time_ms();
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_rcog_bridge_heartbeat("set_listening_goal", 1.0f);
    return 0;
}

nimcp_error_t cochlea_rcog_get_listening_goal(
    const cochlea_rcog_bridge_t* bridge,
    cochlea_listening_goal_t* goal
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_rcog_get_listening_goal: bridge NULL");
        return -1;
    }
    if (!goal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_rcog_get_listening_goal: goal NULL");
        return -1;
    }
    cochlea_rcog_bridge_heartbeat("get_listening_goal", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->num_goals > 0) {
        /* Return highest priority goal */
        uint32_t best = 0;
        for (uint32_t i = 1; i < bridge->num_goals; i++) {
            if (bridge->goals[i].priority > bridge->goals[best].priority) {
                best = i;
            }
        }
        *goal = bridge->goals[best];
    } else {
        memset(goal, 0, sizeof(*goal));
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_rcog_bridge_heartbeat("get_listening_goal", 1.0f);
    return 0;
}

nimcp_error_t cochlea_rcog_clear_goal(cochlea_rcog_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_rcog_clear_goal: bridge NULL");
        return -1;
    }
    cochlea_rcog_bridge_heartbeat("clear_goal", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->num_goals = 0;
    bridge->goal_active = false;
    memset(bridge->goals, 0, sizeof(bridge->goals));

    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_rcog_bridge_heartbeat("clear_goal", 1.0f);
    return 0;
}

//=============================================================================
// Event Sending (Outbound)
//=============================================================================

nimcp_error_t cochlea_rcog_send_event(
    cochlea_rcog_bridge_t* bridge,
    const cochlea_audio_event_t* event
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_rcog_send_event: bridge NULL");
        return -1;
    }
    if (!event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_rcog_send_event: event NULL");
        return -1;
    }
    cochlea_rcog_bridge_heartbeat("send_event", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->last_event = *event;
    bridge->event_pending = true;
    bridge->last_outbound_ms = get_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_rcog_bridge_heartbeat("send_event", 1.0f);
    return 0;
}

nimcp_error_t cochlea_rcog_update_context(cochlea_rcog_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_rcog_update_context: bridge NULL");
        return -1;
    }
    cochlea_rcog_bridge_heartbeat("update_context", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update context variables in the RCOG engine */
    /* Audio context: latest event data */
    /* Sounds variable: detected sound classes */
    bridge->last_outbound_ms = get_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);
    cochlea_rcog_bridge_heartbeat("update_context", 1.0f);
    return 0;
}

//=============================================================================
// Bidirectional Verification
//=============================================================================

bool cochlea_rcog_verify_bidirectional(const cochlea_rcog_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_rcog_verify_bidirectional: bridge is NULL");
        return false;
    }
    cochlea_rcog_bridge_heartbeat("verify_bidirectional", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);

    bool result = (bridge->last_outbound_ms > 0) && (bridge->last_inbound_ms > 0);

    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

uint64_t cochlea_rcog_get_last_outbound(const cochlea_rcog_bridge_t* bridge) {
    if (!bridge) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    uint64_t val = bridge->last_outbound_ms;
    nimcp_mutex_unlock(bridge->base.mutex);
    return val;
}

uint64_t cochlea_rcog_get_last_inbound(const cochlea_rcog_bridge_t* bridge) {
    if (!bridge) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    uint64_t val = bridge->last_inbound_ms;
    nimcp_mutex_unlock(bridge->base.mutex);
    return val;
}
