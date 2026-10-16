/**
 * @file nimcp_broca_thalamic_bridge.c
 * @brief Broca-Thalamic Bridge Implementation
 *
 * Routes language production signals through thalamic relay
 * for motor cortex coordination and attention gating.
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/broca/nimcp_broca_thalamic_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(broca_thalamic_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "BROCA_THALAMIC_BRIDGE"


//=============================================================================
// Internal Structure
//=============================================================================

struct broca_thalamic_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    void* broca;                         /**< Broca adapter handle */
    thalamic_router_t* router;           /**< Thalamic router handle */
    broca_thalamic_config_t config;      /**< Configuration */
    broca_thalamic_stats_t stats;        /**< Statistics */
    float attention_weight;              /**< Current attention level */
    uint32_t current_sequence_id;        /**< Active utterance ID */
    bool utterance_in_progress;          /**< Utterance active flag */
};

//=============================================================================
// Configuration API
//=============================================================================

broca_thalamic_config_t broca_thalamic_default_config(void) {
    return (broca_thalamic_config_t){
        .enable_attention_gating = true,
        .enable_motor_priority = true,
        .enable_syntax_routing = true,
        .min_urgency_threshold = 0.2f,
        .motor_boost = 1.5f,
        .attention_decay_rate = 0.1f
    };
}

//=============================================================================
// Lifecycle API
//=============================================================================

broca_thalamic_bridge_t* broca_thalamic_bridge_create(
    void* broca,
    thalamic_router_t* router,
    const broca_thalamic_config_t* config
) {
    broca_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(broca_thalamic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "broca_thalamic") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "broca_thalamic_bridge_create: bridge_base_init failed");
        return NULL;
    }

    bridge->broca = broca;
    bridge->router = router;
    bridge->config = config ? *config : broca_thalamic_default_config();

    bridge->attention_weight = 1.0f;
    bridge->current_sequence_id = 0;
    bridge->utterance_in_progress = false;

    memset(&bridge->stats, 0, sizeof(bridge->stats));

    return bridge;
}

void broca_thalamic_bridge_destroy(broca_thalamic_bridge_t* bridge) {
    if (bridge) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
    }
}

int broca_thalamic_bridge_reset(broca_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    bridge->attention_weight = 1.0f;
    bridge->current_sequence_id = 0;
    bridge->utterance_in_progress = false;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    return 0;
}

//=============================================================================
// Signal Routing API
//=============================================================================

int broca_thalamic_route_signal(
    broca_thalamic_bridge_t* bridge,
    const broca_thalamic_signal_t* signal
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (!signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "signal is NULL");
        return -1;
    }

    /* Apply attention gating */
    if (bridge->config.enable_attention_gating) {
        float effective_urgency = signal->speech_urgency * bridge->attention_weight;

        /* Motor commands get priority boost */
        if (bridge->config.enable_motor_priority &&
            signal->signal_type == BROCA_SIGNAL_MOTOR_COMMAND) {
            effective_urgency *= bridge->config.motor_boost;
            if (effective_urgency > 1.0f) effective_urgency = 1.0f;
        }

        if (effective_urgency < bridge->config.min_urgency_threshold) {
            bridge->stats.signals_gated++;
            return 0; /* Signal gated, but not an error */
        }
    }

    /* Route based on signal type */
    switch (signal->signal_type) {
        case BROCA_SIGNAL_MOTOR_COMMAND:
            bridge->stats.motor_commands_routed++;
            break;

        case BROCA_SIGNAL_PHONEME_SEQUENCE:
            bridge->stats.phoneme_sequences++;
            break;

        case BROCA_SIGNAL_SYNTAX_COMPLETE:
            if (bridge->config.enable_syntax_routing) {
                bridge->stats.syntax_completions++;
            }
            break;

        case BROCA_SIGNAL_LEXICAL_REQUEST:
            bridge->stats.lexical_requests++;
            break;

        case BROCA_SIGNAL_UTTERANCE_START:
            bridge->stats.utterances_started++;
            bridge->utterance_in_progress = true;
            bridge->current_sequence_id = signal->sequence_id;
            break;

        case BROCA_SIGNAL_UTTERANCE_END:
            bridge->stats.utterances_completed++;
            bridge->utterance_in_progress = false;
            break;

        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "broca_thalamic_route_signal: operation failed");
            return -1;
    }

    /* Update average urgency */
    uint64_t total_signals = bridge->stats.motor_commands_routed +
                             bridge->stats.phoneme_sequences +
                             bridge->stats.syntax_completions +
                             bridge->stats.lexical_requests;

    if (total_signals > 0) {
        float new_avg_urgency =
            (bridge->stats.avg_speech_urgency * (total_signals - 1) +
             signal->speech_urgency) / total_signals;
        if (isfinite(new_avg_urgency)) bridge->stats.avg_speech_urgency = new_avg_urgency;
    }

    /*
     * TODO: Actually route through thalamic router
     * Would call something like:
     * thalamic_router_route(bridge->router, THALAMIC_NUCLEUS_VA, signal->content, signal->content_size);
     */

    return 0;
}

int broca_thalamic_route_motor_command(
    broca_thalamic_bridge_t* bridge,
    const void* command,
    uint32_t command_size,
    float urgency
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    broca_thalamic_signal_t signal = {
        .signal_type = BROCA_SIGNAL_MOTOR_COMMAND,
        .speech_urgency = urgency < 0.0f ? 0.0f : (urgency > 1.0f ? 1.0f : urgency),
        .attention_weight = bridge->attention_weight,
        .sequence_id = bridge->current_sequence_id,
        .content = (void*)command,
        .content_size = command_size,
        .timestamp_us = nimcp_time_get_us()
    };

    return broca_thalamic_route_signal(bridge, &signal);
}

int broca_thalamic_route_phonemes(
    broca_thalamic_bridge_t* bridge,
    const uint8_t* phonemes,
    uint32_t count
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (!phonemes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "phonemes is NULL");
        return -1;
    }

    broca_thalamic_signal_t signal = {
        .signal_type = BROCA_SIGNAL_PHONEME_SEQUENCE,
        .speech_urgency = 0.7f,  /* Default urgency for phonemes */
        .attention_weight = bridge->attention_weight,
        .sequence_id = bridge->current_sequence_id,
        .phoneme_count = count,
        .content = (void*)phonemes,
        .content_size = count * sizeof(uint8_t),
        .timestamp_us = nimcp_time_get_us()
    };

    return broca_thalamic_route_signal(bridge, &signal);
}

int broca_thalamic_signal_utterance_start(
    broca_thalamic_bridge_t* bridge,
    uint32_t sequence_id,
    uint32_t word_count
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    broca_thalamic_signal_t signal = {
        .signal_type = BROCA_SIGNAL_UTTERANCE_START,
        .speech_urgency = 0.8f,  /* High urgency for utterance start */
        .attention_weight = bridge->attention_weight,
        .sequence_id = sequence_id,
        .word_count = word_count,
        .timestamp_us = nimcp_time_get_us()
    };

    return broca_thalamic_route_signal(bridge, &signal);
}

int broca_thalamic_signal_utterance_end(
    broca_thalamic_bridge_t* bridge,
    uint32_t sequence_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    broca_thalamic_signal_t signal = {
        .signal_type = BROCA_SIGNAL_UTTERANCE_END,
        .speech_urgency = 0.5f,
        .attention_weight = bridge->attention_weight,
        .sequence_id = sequence_id,
        .timestamp_us = nimcp_time_get_us()
    };

    return broca_thalamic_route_signal(bridge, &signal);
}

//=============================================================================
// Attention API
//=============================================================================

int broca_thalamic_set_attention(broca_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    bridge->attention_weight = attention < 0.0f ? 0.0f :
                               (attention > 1.0f ? 1.0f : attention);
    return 0;
}

int broca_thalamic_get_attention(const broca_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (!attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention is NULL");
        return -1;
    }

    *attention = bridge->attention_weight;
    return 0;
}

//=============================================================================
// Statistics API
//=============================================================================

int broca_thalamic_bridge_get_stats(
    const broca_thalamic_bridge_t* bridge,
    broca_thalamic_stats_t* stats
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stats is NULL");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

void broca_thalamic_bridge_reset_stats(broca_thalamic_bridge_t* bridge) {
    if (bridge) {
        memset(&bridge->stats, 0, sizeof(bridge->stats));
    }
}
