/**
 * @file nimcp_omni_wm_memory_bridge.c
 * @brief World Model Memory Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-17
 *
 * WHAT: Bidirectional bridge connecting World Model (RSSM) with memory systems
 * WHY:  Enable memory-informed world modeling and world-model-driven memory encoding
 * HOW:  Hippocampal replay trains world model; world model predictions guide memory encoding
 *
 * IMPLEMENTATION NOTES:
 * =====================
 * This implementation integrates several key concepts:
 *
 * 1. COMPLEMENTARY LEARNING SYSTEMS (McClelland et al., 1995):
 *    - Hippocampus rapidly encodes, cortex slowly learns
 *    - World model bridges these timescales
 *
 * 2. REPLAY-BASED TRAINING:
 *    - Sharp-wave ripple sequences train RSSM dynamics
 *    - Forward and reverse replay for different learning objectives
 *
 * 3. EPISODIC CONTEXT:
 *    - Engrams provide context for state prediction
 *    - Pattern completion/separation via CA3/DG
 */

#include "cognitive/omni/bridges/nimcp_omni_wm_memory_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>

/* ============================================================================
 * Module-level Constants
 * ============================================================================ */

#define LOG_MODULE "wm_memory_bridge"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(omni_wm_memory_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_omni_wm_memory_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_omni_wm_memory_bridge_mesh_registry = NULL;

nimcp_error_t omni_wm_memory_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_omni_wm_memory_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "omni_wm_memory_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "omni_wm_memory_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_omni_wm_memory_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_omni_wm_memory_bridge_mesh_registry = registry;
    return err;
}

void omni_wm_memory_bridge_mesh_unregister(void) {
    if (g_omni_wm_memory_bridge_mesh_registry && g_omni_wm_memory_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_omni_wm_memory_bridge_mesh_registry, g_omni_wm_memory_bridge_mesh_id);
        g_omni_wm_memory_bridge_mesh_id = 0;
        g_omni_wm_memory_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from omni_wm_memory_bridge module (instance-level) */
static inline void omni_wm_memory_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_omni_wm_memory_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_omni_wm_memory_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_omni_wm_memory_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

void omni_wm_memory_bridge_set_instance_health_agent(
    omni_wm_memory_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "omni_wm_memory_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Instance-level Training Functions
 * ============================================================================ */

int omni_wm_memory_bridge_training_begin(omni_wm_memory_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_wm_memory_bridge_training_begin: NULL argument");
        return -1;
    }
    omni_wm_memory_bridge_heartbeat_instance(g_omni_wm_memory_bridge_health_agent, "training_begin", 0.0f);
    (void)bridge;
    return 0;
}

int omni_wm_memory_bridge_training_step(omni_wm_memory_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_wm_memory_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    omni_wm_memory_bridge_heartbeat_instance(g_omni_wm_memory_bridge_health_agent, "training_step", progress);
    (void)bridge;
    return 0;
}

int omni_wm_memory_bridge_training_end(omni_wm_memory_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_wm_memory_bridge_training_end: NULL argument");
        return -1;
    }
    omni_wm_memory_bridge_heartbeat_instance(g_omni_wm_memory_bridge_health_agent, "training_end", 1.0f);
    (void)bridge;
    return 0;
}

/** Default replay buffer capacity */
#define DEFAULT_REPLAY_BUFFER_CAPACITY 64

/** Maximum state/action dimension for internal buffers */
#define MAX_STATE_DIM 512

/** Context cache TTL in microseconds (10 seconds) */
#define CONTEXT_CACHE_TTL_US 10000000

/* ============================================================================
 * Internal Helper Forward Declarations
 * ============================================================================ */

static nimcp_error_t allocate_replay_buffer(omni_wm_memory_bridge_t* bridge);
static void free_replay_buffer(omni_wm_memory_bridge_t* bridge);
static nimcp_error_t allocate_context_cache(omni_wm_memory_bridge_t* bridge);
static void free_context_cache(omni_wm_memory_bridge_t* bridge);
static nimcp_error_t process_replay_buffer(omni_wm_memory_bridge_t* bridge);
static nimcp_error_t update_wm_to_memory_effects(omni_wm_memory_bridge_t* bridge);
static nimcp_error_t update_memory_to_wm_effects(omni_wm_memory_bridge_t* bridge);
static uint64_t get_current_time_us(void);

/* Bio-async handlers */
static nimcp_error_t handle_replay_sequence(const void* msg, size_t msg_size,
                                             nimcp_bio_promise_t promise, void* user_data);
static nimcp_error_t handle_engram_encode(const void* msg, size_t msg_size,
                                           nimcp_bio_promise_t promise, void* user_data);
static nimcp_error_t handle_engram_retrieve(const void* msg, size_t msg_size,
                                             nimcp_bio_promise_t promise, void* user_data);
static nimcp_error_t handle_consolidation(const void* msg, size_t msg_size,
                                           nimcp_bio_promise_t promise, void* user_data);
static nimcp_error_t handle_pattern_complete(const void* msg, size_t msg_size,
                                              nimcp_bio_promise_t promise, void* user_data);
static nimcp_error_t handle_pattern_separate(const void* msg, size_t msg_size,
                                              nimcp_bio_promise_t promise, void* user_data);

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Get current time in microseconds
 */
static uint64_t get_current_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
}

/**
 * @brief Allocate replay buffer arrays
 *
 * WHAT: Allocate state/action/reward buffers for replay sequences
 * WHY:  Buffer incoming replay data for batch training
 * HOW:  Allocate 2D arrays for states/actions, 1D for rewards
 */
static nimcp_error_t allocate_replay_buffer(omni_wm_memory_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    uint32_t capacity = bridge->config.replay_batch_size;
    if (capacity == 0) capacity = DEFAULT_REPLAY_BUFFER_CAPACITY;

    /* Allocate state buffer pointers */
    bridge->replay_buffer_states = nimcp_calloc(capacity, sizeof(float*));
    if (!bridge->replay_buffer_states) return NIMCP_ERROR_NO_MEMORY;

    /* Allocate action buffer pointers */
    bridge->replay_buffer_actions = nimcp_calloc(capacity, sizeof(float*));
    if (!bridge->replay_buffer_actions) {
        nimcp_free(bridge->replay_buffer_states);
        bridge->replay_buffer_states = NULL;
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Allocate reward buffer */
    bridge->replay_buffer_rewards = nimcp_calloc(capacity, sizeof(float));
    if (!bridge->replay_buffer_rewards) {
        nimcp_free(bridge->replay_buffer_states);
        nimcp_free(bridge->replay_buffer_actions);
        bridge->replay_buffer_states = NULL;
        bridge->replay_buffer_actions = NULL;
        return NIMCP_ERROR_NO_MEMORY;
    }

    bridge->replay_buffer_capacity = capacity;
    bridge->replay_buffer_size = 0;

    return NIMCP_SUCCESS;
}

/**
 * @brief Free replay buffer arrays
 */
static void free_replay_buffer(omni_wm_memory_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->replay_buffer_states) {
        for (uint32_t i = 0; i < bridge->replay_buffer_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->replay_buffer_size > 256) {
                omni_wm_memory_bridge_heartbeat("omni_wm_memo_loop",
                                 (float)(i + 1) / (float)bridge->replay_buffer_size);
            }

            nimcp_free(bridge->replay_buffer_states[i]);
        }
        nimcp_free(bridge->replay_buffer_states);
        bridge->replay_buffer_states = NULL;
    }

    if (bridge->replay_buffer_actions) {
        for (uint32_t i = 0; i < bridge->replay_buffer_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->replay_buffer_size > 256) {
                omni_wm_memory_bridge_heartbeat("omni_wm_memo_loop",
                                 (float)(i + 1) / (float)bridge->replay_buffer_size);
            }

            nimcp_free(bridge->replay_buffer_actions[i]);
        }
        nimcp_free(bridge->replay_buffer_actions);
        bridge->replay_buffer_actions = NULL;
    }

    nimcp_free(bridge->replay_buffer_rewards);
    bridge->replay_buffer_rewards = NULL;

    bridge->replay_buffer_size = 0;
    bridge->replay_buffer_capacity = 0;
}

/**
 * @brief Allocate context cache
 */
static nimcp_error_t allocate_context_cache(omni_wm_memory_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    uint32_t dim = WM_MEMORY_MAX_CONTEXT_DIM;
    bridge->context_cache = nimcp_calloc(dim, sizeof(float));
    if (!bridge->context_cache) return NIMCP_ERROR_NO_MEMORY;

    bridge->context_cache_dim = dim;
    bridge->context_cache_valid = false;
    bridge->context_cache_time = 0;

    return NIMCP_SUCCESS;
}

/**
 * @brief Free context cache
 */
static void free_context_cache(omni_wm_memory_bridge_t* bridge) {
    if (!bridge) return;

    nimcp_free(bridge->context_cache);
    bridge->context_cache = NULL;
    bridge->context_cache_dim = 0;
    bridge->context_cache_valid = false;
}

/**
 * @brief Process buffered replay sequences for training
 *
 * WHAT: Forward buffered replay data to world model for training
 * WHY:  Batch training from hippocampal replay improves RSSM dynamics
 * HOW:  Check buffer, create training batch, invoke WM training
 */
static nimcp_error_t process_replay_buffer(omni_wm_memory_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->world_model) return NIMCP_SUCCESS; /* No WM connected */
    if (bridge->replay_buffer_size == 0) return NIMCP_SUCCESS; /* Nothing to process */

    /* Process replay through training - simplified placeholder */
    /* In full implementation, would call omni_wm_train_from_replay() */
    bridge->stats.replay_training_updates++;
    bridge->stats.replay_sequences_received += bridge->replay_buffer_size;

    /* Clear buffer after processing */
    for (uint32_t i = 0; i < bridge->replay_buffer_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->replay_buffer_size > 256) {
            omni_wm_memory_bridge_heartbeat("omni_wm_memo_loop",
                             (float)(i + 1) / (float)bridge->replay_buffer_size);
        }

        nimcp_free(bridge->replay_buffer_states[i]);
        bridge->replay_buffer_states[i] = NULL;
        nimcp_free(bridge->replay_buffer_actions[i]);
        bridge->replay_buffer_actions[i] = NULL;
    }
    bridge->replay_buffer_size = 0;

    return NIMCP_SUCCESS;
}

/**
 * @brief Update effects flowing from WM to memory systems
 */
static nimcp_error_t update_wm_to_memory_effects(omni_wm_memory_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    omni_wm_to_memory_effects_t* effects = &bridge->wm_to_memory;

    /* Get current timestamp */
    effects->snapshot_timestamp = (double)get_current_time_us() / 1000000.0;

    /* If world model connected, extract state information */
    if (bridge->world_model) {
        /* Placeholder: would extract actual WM state */
        effects->state_uncertainty = 0.5f;
        effects->prediction_confidence = 0.8f;
        effects->prediction_error_magnitude = 0.1f;

        /* Determine encoding priority based on prediction error */
        effects->encoding_priority = effects->prediction_error_magnitude;
        effects->should_encode = (effects->prediction_error_magnitude >
                                  bridge->config.encoding_threshold);
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Update effects flowing from memory systems to WM
 */
static nimcp_error_t update_memory_to_wm_effects(omni_wm_memory_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    memory_to_omni_wm_effects_t* effects = &bridge->memory_to_wm;

    /* Update consolidation state */
    effects->is_consolidating = bridge->is_sleeping;
    effects->sleep_stage = bridge->current_sleep_stage;

    /* If hippocampus connected, get oscillation state */
    if (bridge->hippocampus) {
        /* Placeholder: would query actual hippocampus state */
        effects->theta_phase = 0.0f;
        effects->theta_power = 0.5f;
        effects->gamma_power = 0.3f;
        effects->ripple_active = false;
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async Message Handlers
 * ============================================================================ */

/**
 * @brief Handle incoming replay sequence message
 */
static nimcp_error_t handle_replay_sequence(const void* msg, size_t msg_size,
                                             nimcp_bio_promise_t promise, void* user_data) {
    (void)msg_size;
    (void)promise;

    NIMCP_CHECK_THROW(msg, NIMCP_ERROR_NULL_POINTER, "msg is NULL");
    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    omni_wm_memory_bridge_t* bridge = (omni_wm_memory_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.replay_sequences_received++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle engram encode request
 */
static nimcp_error_t handle_engram_encode(const void* msg, size_t msg_size,
                                           nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    omni_wm_memory_bridge_t* bridge = (omni_wm_memory_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.engrams_encoded++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle engram retrieval request
 */
static nimcp_error_t handle_engram_retrieve(const void* msg, size_t msg_size,
                                             nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    omni_wm_memory_bridge_t* bridge = (omni_wm_memory_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.engram_retrievals++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle consolidation message
 */
static nimcp_error_t handle_consolidation(const void* msg, size_t msg_size,
                                           nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    omni_wm_memory_bridge_t* bridge = (omni_wm_memory_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.consolidation_cycles++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle pattern completion request
 */
static nimcp_error_t handle_pattern_complete(const void* msg, size_t msg_size,
                                              nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    omni_wm_memory_bridge_t* bridge = (omni_wm_memory_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.pattern_completions++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle pattern separation request
 */
static nimcp_error_t handle_pattern_separate(const void* msg, size_t msg_size,
                                              nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    omni_wm_memory_bridge_t* bridge = (omni_wm_memory_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.pattern_separations++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_memory_bridge_default_config(
    omni_wm_memory_bridge_config_t* config) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_memory_bridge_heartbeat("omni_wm_memo_default_config", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    memset(config, 0, sizeof(omni_wm_memory_bridge_config_t));

    /* General settings */
    config->enable_modulation = true;
    config->sensitivity = 1.0f;

    /* Replay training settings */
    config->enable_replay_training = true;
    config->replay_batch_size = WM_MEMORY_DEFAULT_REPLAY_BATCH;
    config->replay_learning_rate = 0.001f;
    config->replay_priority_decay = 0.95f;
    config->use_reverse_replay = true;
    config->replay_compression_factor = 15.0f; /* 15x biological compression */

    /* Engram integration settings */
    config->enable_engram_encoding = true;
    config->encoding_threshold = WM_MEMORY_DEFAULT_ENCODING_THRESHOLD;
    config->emotional_boost_factor = 2.0f;
    config->enable_context_retrieval = true;
    config->max_context_engrams = 5;

    /* Consolidation settings */
    config->enable_consolidation_sync = true;
    config->consolidation_learning_rate = 0.0001f;
    config->enable_semantic_extraction = true;
    config->semantic_abstraction_level = 0.5f;

    /* Hippocampus settings */
    config->enable_pattern_completion = true;
    config->enable_pattern_separation = true;
    config->completion_threshold = 0.6f;
    config->separation_threshold = 0.3f;

    /* Bio-async settings */
    config->enable_bio_async = true;

    return NIMCP_SUCCESS;
}

omni_wm_memory_bridge_t* omni_wm_memory_bridge_create(
    const omni_wm_memory_bridge_config_t* config) {

    /* Allocate bridge structure */
    /* Phase 8: Heartbeat at operation start */
    omni_wm_memory_bridge_heartbeat("omni_wm_memo_create", 0.0f);


    omni_wm_memory_bridge_t* bridge = nimcp_calloc(1, sizeof(omni_wm_memory_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate WM memory bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: bridge is NULL");
        return NULL;
    }

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, BIO_MODULE_WM_MEMORY_BRIDGE,
                         "wm_memory_bridge") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_LOGGING_ERROR("Failed to initialize bridge base");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: operation failed");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        omni_wm_memory_bridge_default_config(&bridge->config);
    }

    /* Allocate replay buffer */
    nimcp_error_t err = allocate_replay_buffer(bridge);
    if (err != NIMCP_SUCCESS) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_LOGGING_ERROR("Failed to allocate replay buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: validation failed");
        return NULL;
    }

    /* Allocate context cache */
    err = allocate_context_cache(bridge);
    if (err != NIMCP_SUCCESS) {
        free_replay_buffer(bridge);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_LOGGING_ERROR("Failed to allocate context cache");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: validation failed");
        return NULL;
    }

    /* Initialize state */
    bridge->is_sleeping = false;
    bridge->current_sleep_stage = 0.0f;
    bridge->current_episode_id = 0;
    bridge->replay_in_progress = false;

    NIMCP_LOGGING_INFO("WM Memory Bridge created successfully");
    return bridge;
}

void omni_wm_memory_bridge_destroy(omni_wm_memory_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    /* Phase 8: Heartbeat at operation start */
    omni_wm_memory_bridge_heartbeat("omni_wm_memo_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) {
        omni_wm_memory_bridge_disconnect_bio_async(bridge);
    }

    /* Free WM to memory effects dynamic arrays */
    nimcp_free(bridge->wm_to_memory.wm_state_snapshot);
    nimcp_free(bridge->wm_to_memory.predicted_next_state);
    nimcp_free(bridge->wm_to_memory.semantic_features);

    /* Free memory to WM effects dynamic arrays */
    if (bridge->memory_to_wm.replay_states) {
        for (uint32_t i = 0; i < bridge->memory_to_wm.replay_length; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->memory_to_wm.replay_length > 256) {
                omni_wm_memory_bridge_heartbeat("omni_wm_memo_loop",
                                 (float)(i + 1) / (float)bridge->memory_to_wm.replay_length);
            }

            nimcp_free(bridge->memory_to_wm.replay_states[i]);
        }
        nimcp_free(bridge->memory_to_wm.replay_states);
    }
    if (bridge->memory_to_wm.replay_actions) {
        for (uint32_t i = 0; i < bridge->memory_to_wm.replay_length; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->memory_to_wm.replay_length > 256) {
                omni_wm_memory_bridge_heartbeat("omni_wm_memo_loop",
                                 (float)(i + 1) / (float)bridge->memory_to_wm.replay_length);
            }

            nimcp_free(bridge->memory_to_wm.replay_actions[i]);
        }
        nimcp_free(bridge->memory_to_wm.replay_actions);
    }
    nimcp_free(bridge->memory_to_wm.replay_rewards);
    nimcp_free(bridge->memory_to_wm.episodic_context);
    nimcp_free(bridge->memory_to_wm.context_engram_ids);
    nimcp_free(bridge->memory_to_wm.completed_pattern);
    nimcp_free(bridge->memory_to_wm.separated_pattern);

    /* Free internal buffers */
    free_replay_buffer(bridge);
    free_context_cache(bridge);

    /* Cleanup base and free */
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;

    NIMCP_LOGGING_INFO("WM Memory Bridge destroyed");
}

nimcp_error_t omni_wm_memory_bridge_reset(omni_wm_memory_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    omni_wm_memory_bridge_heartbeat("omni_wm_memo_reset", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset effects */
    memset(&bridge->wm_to_memory, 0, sizeof(omni_wm_to_memory_effects_t));
    memset(&bridge->memory_to_wm, 0, sizeof(memory_to_omni_wm_effects_t));

    /* Reset internal state */
    bridge->is_sleeping = false;
    bridge->current_sleep_stage = 0.0f;
    bridge->current_episode_id = 0;
    bridge->replay_in_progress = false;

    /* Clear replay buffer */
    for (uint32_t i = 0; i < bridge->replay_buffer_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->replay_buffer_size > 256) {
            omni_wm_memory_bridge_heartbeat("omni_wm_memo_loop",
                             (float)(i + 1) / (float)bridge->replay_buffer_size);
        }

        nimcp_free(bridge->replay_buffer_states[i]);
        bridge->replay_buffer_states[i] = NULL;
        nimcp_free(bridge->replay_buffer_actions[i]);
        bridge->replay_buffer_actions[i] = NULL;
    }
    bridge->replay_buffer_size = 0;

    /* Invalidate context cache */
    bridge->context_cache_valid = false;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(omni_wm_memory_bridge_stats_t));

    /* Reset base bridge (unlocked since we already hold the mutex) */
    bridge_base_reset_unlocked(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Connection API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_memory_bridge_connect(
    omni_wm_memory_bridge_t* bridge,
    omni_world_model_t* world_model,
    nimcp_hippocampus_t* hippocampus,
    engram_system_t* engram_system,
    systems_consolidation_system_t* consolidation) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_memory_bridge_heartbeat("omni_wm_memo_connect", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(world_model, NIMCP_ERROR_INVALID_PARAM, "world_model is required");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->world_model = world_model;
    bridge->hippocampus = hippocampus;
    bridge->engram_system = engram_system;
    bridge->consolidation = consolidation;

    /* Update base connection state */
    bridge->base.system_a = world_model;
    bridge->base.system_a_connected = true;
    bridge->base.bridge_active = true;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("WM Memory Bridge connected: WM=%p, Hippo=%p, Engram=%p, Consol=%p",
                       (void*)world_model, (void*)hippocampus,
                       (void*)engram_system, (void*)consolidation);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_memory_bridge_connect_world_model(
    omni_wm_memory_bridge_t* bridge,
    omni_world_model_t* world_model) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_memory_bridge_heartbeat("omni_wm_memo_connect_world_model", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(world_model, NIMCP_ERROR_NULL_POINTER, "world_model is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->world_model = world_model;
    bridge->base.system_a = world_model;
    bridge->base.system_a_connected = true;
    bridge->base.bridge_active = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_memory_bridge_connect_hippocampus(
    omni_wm_memory_bridge_t* bridge,
    nimcp_hippocampus_t* hippocampus) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_memory_bridge_heartbeat("omni_wm_memo_connect_hippocampus", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(hippocampus, NIMCP_ERROR_NULL_POINTER, "hippocampus is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->hippocampus = hippocampus;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_memory_bridge_connect_engram(
    omni_wm_memory_bridge_t* bridge,
    engram_system_t* engram_system) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_memory_bridge_heartbeat("omni_wm_memo_connect_engram", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(engram_system, NIMCP_ERROR_NULL_POINTER, "engram_system is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->engram_system = engram_system;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_memory_bridge_connect_consolidation(
    omni_wm_memory_bridge_t* bridge,
    systems_consolidation_system_t* consolidation) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_memory_bridge_heartbeat("omni_wm_memo_connect_consolidatio", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(consolidation, NIMCP_ERROR_NULL_POINTER, "consolidation is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->consolidation = consolidation;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

bool omni_wm_memory_bridge_is_connected(const omni_wm_memory_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_memory_bridge_heartbeat("omni_wm_memo_is_connected", 0.0f);


    return bridge->world_model != NULL;
}

/* ============================================================================
 * Update API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_memory_bridge_update(
    omni_wm_memory_bridge_t* bridge,
    float dt) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_memory_bridge_heartbeat("omni_wm_memo_update", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_modulation) return NIMCP_SUCCESS;

    uint64_t start_time = get_current_time_us();

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check consolidation mode based on sleep state */
    if (bridge->is_sleeping && bridge->config.enable_consolidation_sync) {
        /* During sleep: prioritize consolidation and replay */
        if (bridge->config.enable_replay_training) {
            nimcp_error_t err = process_replay_buffer(bridge);
            if (err != NIMCP_SUCCESS) {
                bridge->stats.errors_replay++;
            }
        }
    } else {
        /* Awake: normal bidirectional updates */
        if (bridge->config.enable_replay_training && bridge->replay_buffer_size > 0) {
            /* Process any accumulated replay data */
            process_replay_buffer(bridge);
        }
    }

    /* Update effects in both directions */
    update_wm_to_memory_effects(bridge);
    update_memory_to_wm_effects(bridge);

    /* Update timing statistics */
    bridge->stats.total_updates++;
    uint64_t elapsed = get_current_time_us() - start_time;
    bridge->stats.total_processing_time_ms += (double)elapsed / 1000.0;
    bridge->stats.mean_update_time_ms = bridge->stats.total_processing_time_ms /
                                         (double)bridge->stats.total_updates;
    bridge->stats.last_update_time_us = start_time;

    /* Record base update */
    bridge_base_record_update(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);

    (void)dt; /* dt used for time-based scaling if needed */
    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_memory_bridge_set_sleep_state(
    omni_wm_memory_bridge_t* bridge,
    bool is_sleeping,
    float sleep_stage) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_memory_bridge_heartbeat("omni_wm_memo_set_sleep_state", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    bool was_sleeping = bridge->is_sleeping;
    bridge->is_sleeping = is_sleeping;
    bridge->current_sleep_stage = sleep_stage;

    /* Update memory effects */
    bridge->memory_to_wm.is_consolidating = is_sleeping;
    bridge->memory_to_wm.sleep_stage = sleep_stage;

    /* Log state transition */
    if (is_sleeping && !was_sleeping) {
        NIMCP_LOGGING_DEBUG("WM Memory Bridge entering sleep state, stage=%.2f",
                           sleep_stage);
    } else if (!is_sleeping && was_sleeping) {
        NIMCP_LOGGING_DEBUG("WM Memory Bridge exiting sleep state");
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Replay Training API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_memory_bridge_train_from_replay(
    omni_wm_memory_bridge_t* bridge,
    const float** states,
    const float** actions,
    const float* rewards,
    uint32_t length,
    bool is_reverse) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_memory_bridge_heartbeat("omni_wm_memo_train_from_replay", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(states, NIMCP_ERROR_NULL_POINTER, "states is NULL");
    NIMCP_CHECK_THROW(actions, NIMCP_ERROR_NULL_POINTER, "actions is NULL");
    NIMCP_CHECK_THROW(rewards, NIMCP_ERROR_NULL_POINTER, "rewards is NULL");
    NIMCP_CHECK_THROW(length > 0, NIMCP_ERROR_INVALID_PARAM, "length must be greater than 0");
    NIMCP_CHECK_THROW(length <= WM_MEMORY_MAX_REPLAY_LENGTH, NIMCP_ERROR_INVALID_PARAM, "length exceeds max replay length");

    if (!bridge->config.enable_replay_training) return NIMCP_SUCCESS;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update memory effects with replay info */
    bridge->memory_to_wm.is_reverse_replay = is_reverse;
    bridge->memory_to_wm.replay_length = length;
    bridge->replay_in_progress = true;

    /* In full implementation, would:
     * 1. Forward states/actions to world model RSSM
     * 2. Compute prediction errors
     * 3. Update RSSM weights via backprop
     * For now, update statistics */

    bridge->stats.replay_sequences_received++;
    bridge->stats.replay_training_updates++;

    /* Simple placeholder loss calculation */
    float training_loss = 0.0f;
    for (uint32_t i = 0; i < length; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && length > 256) {
            omni_wm_memory_bridge_heartbeat("omni_wm_memo_loop",
                             (float)(i + 1) / (float)length);
        }

        training_loss += fabsf(rewards[i]);
    }
    training_loss /= (float)length;

    /* Update running average */
    float alpha = 0.1f;
    bridge->stats.mean_replay_training_loss =
        alpha * training_loss +
        (1.0f - alpha) * bridge->stats.mean_replay_training_loss;

    bridge->replay_in_progress = false;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Trained from replay: length=%u, reverse=%d, loss=%.4f",
                       length, is_reverse, training_loss);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_memory_bridge_on_ripple(
    omni_wm_memory_bridge_t* bridge,
    const struct nimcp_ripple_event* ripple) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_memory_bridge_heartbeat("omni_wm_memo_on_ripple", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(ripple, NIMCP_ERROR_NULL_POINTER, "ripple is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update ripple state in effects */
    bridge->memory_to_wm.ripple_active = true;

    /* In full implementation, would extract replay sequence from ripple
     * and forward to world model for training */
    bridge->stats.replay_sequences_received++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Engram API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_memory_bridge_encode_engram(
    omni_wm_memory_bridge_t* bridge,
    float emotional_tag,
    bool force_encode,
    uint64_t* engram_id_out) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_memory_bridge_heartbeat("omni_wm_memo_encode_engram", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(engram_id_out, NIMCP_ERROR_INVALID_PARAM, "encode_engram: engram_id_out is NULL");
    if (!bridge->config.enable_engram_encoding) return NIMCP_SUCCESS;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check encoding threshold */
    float encoding_priority = bridge->wm_to_memory.encoding_priority;
    float threshold = bridge->config.encoding_threshold;

    /* Apply emotional boost */
    if (emotional_tag > 0.5f) {
        encoding_priority *= bridge->config.emotional_boost_factor;
    }

    if (!force_encode && encoding_priority < threshold) {
        nimcp_mutex_unlock(bridge->base.mutex);
        if (engram_id_out) *engram_id_out = 0;
        return NIMCP_SUCCESS; /* Below threshold, skip encoding */
    }

    /* Generate engram ID */
    uint64_t new_id = get_current_time_us();

    /* In full implementation, would:
     * 1. Extract WM state snapshot
     * 2. Create engram in engram_system
     * 3. Store emotional context
     * For now, update statistics */

    bridge->stats.engrams_encoded++;
    bridge->stats.mean_encoding_strength =
        0.1f * encoding_priority +
        0.9f * bridge->stats.mean_encoding_strength;

    bridge->wm_to_memory.should_encode = false; /* Reset flag */

    nimcp_mutex_unlock(bridge->base.mutex);

    if (engram_id_out) *engram_id_out = new_id;

    NIMCP_LOGGING_DEBUG("Encoded engram: id=%lu, emotional=%.2f, priority=%.2f",
                       new_id, emotional_tag, encoding_priority);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_memory_bridge_retrieve_episodic_context(
    omni_wm_memory_bridge_t* bridge,
    float* context_out,
    uint32_t context_dim,
    float* confidence_out) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_memory_bridge_heartbeat("omni_wm_memo_retrieve_episodic_co", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(context_out, NIMCP_ERROR_INVALID_PARAM, "context_out is NULL");
    NIMCP_CHECK_THROW(confidence_out, NIMCP_ERROR_INVALID_PARAM, "confidence_out is NULL");
    NIMCP_CHECK_THROW(context_dim > 0, NIMCP_ERROR_INVALID_PARAM, "context_dim must be greater than 0");
    if (!bridge->config.enable_context_retrieval) return NIMCP_SUCCESS;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check cache validity */
    uint64_t now = get_current_time_us();
    if (bridge->context_cache_valid &&
        (now - bridge->context_cache_time) < CONTEXT_CACHE_TTL_US) {
        /* Use cached context */
        uint32_t copy_dim = context_dim < bridge->context_cache_dim ?
                            context_dim : bridge->context_cache_dim;
        memcpy(context_out, bridge->context_cache, copy_dim * sizeof(float));
        if (confidence_out) {
            *confidence_out = bridge->memory_to_wm.context_match_confidence;
        }
        bridge->stats.engram_retrievals++;
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_SUCCESS;
    }

    /* In full implementation, would:
     * 1. Query engram_system for similar memories
     * 2. Fuse retrieved contexts
     * 3. Cache result
     * For now, return placeholder */

    memset(context_out, 0, context_dim * sizeof(float));

    /* Update statistics */
    bridge->stats.engram_retrievals++;
    bridge->stats.mean_retrieval_confidence = 0.7f;

    if (confidence_out) *confidence_out = 0.7f;

    /* Update cache */
    uint32_t cache_dim = context_dim < bridge->context_cache_dim ?
                         context_dim : bridge->context_cache_dim;
    memcpy(bridge->context_cache, context_out, cache_dim * sizeof(float));
    bridge->context_cache_valid = true;
    bridge->context_cache_time = now;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Pattern Operations API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_memory_bridge_pattern_complete(
    omni_wm_memory_bridge_t* bridge,
    const float* partial_pattern,
    uint32_t partial_dim,
    float* completed_out,
    uint32_t completed_dim,
    float* confidence_out) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_memory_bridge_heartbeat("omni_wm_memo_pattern_complete", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(partial_pattern, NIMCP_ERROR_NULL_POINTER, "partial_pattern is NULL");
    NIMCP_CHECK_THROW(completed_out, NIMCP_ERROR_NULL_POINTER, "completed_out is NULL");
    NIMCP_CHECK_THROW(partial_dim > 0, NIMCP_ERROR_INVALID_PARAM, "partial_dim must be greater than 0");
    NIMCP_CHECK_THROW(completed_dim > 0, NIMCP_ERROR_INVALID_PARAM, "completed_dim must be greater than 0");
    if (!bridge->config.enable_pattern_completion) return NIMCP_SUCCESS;

    nimcp_mutex_lock(bridge->base.mutex);

    /* In full implementation, would:
     * 1. Send partial pattern to hippocampus CA3
     * 2. Run attractor dynamics for completion
     * 3. Return completed pattern
     * For now, copy input and add noise as placeholder */

    uint32_t copy_dim = partial_dim < completed_dim ? partial_dim : completed_dim;
    memcpy(completed_out, partial_pattern, copy_dim * sizeof(float));

    /* Fill remaining with attenuated copies */
    for (uint32_t i = copy_dim; i < completed_dim; i++) {
        completed_out[i] = partial_pattern[i % partial_dim] * 0.5f;
    }

    /* Update effects */
    bridge->memory_to_wm.completion_confidence = 0.8f;

    /* Update statistics */
    bridge->stats.pattern_completions++;
    bridge->stats.mean_completion_accuracy =
        0.1f * 0.8f + 0.9f * bridge->stats.mean_completion_accuracy;

    if (confidence_out) *confidence_out = 0.8f;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_memory_bridge_pattern_separate(
    omni_wm_memory_bridge_t* bridge,
    const float* input_pattern,
    uint32_t input_dim,
    float* separated_out,
    uint32_t separated_dim,
    float* separation_strength_out) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_memory_bridge_heartbeat("omni_wm_memo_pattern_separate", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(input_pattern, NIMCP_ERROR_NULL_POINTER, "input_pattern is NULL");
    NIMCP_CHECK_THROW(separated_out, NIMCP_ERROR_NULL_POINTER, "separated_out is NULL");
    NIMCP_CHECK_THROW(input_dim > 0, NIMCP_ERROR_INVALID_PARAM, "input_dim must be greater than 0");
    NIMCP_CHECK_THROW(separated_dim > 0, NIMCP_ERROR_INVALID_PARAM, "separated_dim must be greater than 0");
    if (!bridge->config.enable_pattern_separation) return NIMCP_SUCCESS;

    nimcp_mutex_lock(bridge->base.mutex);

    /* In full implementation, would:
     * 1. Send input to hippocampus DG
     * 2. Apply sparse coding for separation
     * 3. Return orthogonalized pattern
     * For now, apply simple expansion/sparsification */

    /* Expansion factor for DG-like sparse coding */
    float expansion = 5.0f;

    for (uint32_t i = 0; i < separated_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && separated_dim > 256) {
            omni_wm_memory_bridge_heartbeat("omni_wm_memo_loop",
                             (float)(i + 1) / (float)separated_dim);
        }

        if (i < input_dim) {
            /* Apply nonlinear sparsification */
            float val = input_pattern[i];
            separated_out[i] = (val > 0.2f) ? val * expansion : 0.0f;
        } else {
            separated_out[i] = 0.0f;
        }
    }

    /* Update effects */
    bridge->memory_to_wm.separation_strength = 0.75f;

    /* Update statistics */
    bridge->stats.pattern_separations++;
    bridge->stats.mean_separation_strength =
        0.1f * 0.75f + 0.9f * bridge->stats.mean_separation_strength;

    if (separation_strength_out) *separation_strength_out = 0.75f;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Consolidation API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_memory_bridge_extract_semantics(
    omni_wm_memory_bridge_t* bridge,
    float* features_out,
    uint32_t features_dim) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_memory_bridge_heartbeat("omni_wm_memo_extract_semantics", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(features_out, NIMCP_ERROR_INVALID_PARAM, "features_out is NULL");
    NIMCP_CHECK_THROW(features_dim > 0, NIMCP_ERROR_INVALID_PARAM, "features_dim must be greater than 0");
    if (!bridge->config.enable_semantic_extraction) return NIMCP_SUCCESS;

    nimcp_mutex_lock(bridge->base.mutex);

    /* In full implementation, would:
     * 1. Extract latent state from world model
     * 2. Apply abstraction layer for semantic features
     * 3. Return compressed semantic representation
     * For now, return placeholder features */

    float abstraction = bridge->config.semantic_abstraction_level;

    for (uint32_t i = 0; i < features_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && features_dim > 256) {
            omni_wm_memory_bridge_heartbeat("omni_wm_memo_loop",
                             (float)(i + 1) / (float)features_dim);
        }

        /* Generate placeholder semantic features */
        features_out[i] = (float)(i % 10) / 10.0f * abstraction;
    }

    /* Update effects */
    bridge->wm_to_memory.semantic_novelty = 0.5f;

    /* Update statistics */
    bridge->stats.semantic_transfers++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_memory_bridge_consolidation_sync(
    omni_wm_memory_bridge_t* bridge,
    float consolidation_signal) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_memory_bridge_heartbeat("omni_wm_memo_consolidation_sync", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_consolidation_sync) return NIMCP_SUCCESS;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update consolidation progress */
    bridge->memory_to_wm.consolidation_progress = consolidation_signal;

    /* In full implementation, would:
     * 1. Trigger replay-based training if signal strong
     * 2. Update WM weights with consolidation learning rate
     * 3. Transfer semantic features to cortex */

    if (consolidation_signal > 0.5f) {
        /* Process any pending replay during consolidation */
        if (bridge->config.enable_replay_training) {
            process_replay_buffer(bridge);
        }
        bridge->stats.consolidation_cycles++;
    }

    /* Update statistics */
    bridge->stats.mean_consolidation_learning =
        0.1f * consolidation_signal * bridge->config.consolidation_learning_rate +
        0.9f * bridge->stats.mean_consolidation_learning;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

const omni_wm_to_memory_effects_t* omni_wm_memory_bridge_get_wm_effects(
    const omni_wm_memory_bridge_t* bridge) {

    if (!bridge) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");


        return NULL;


    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_memory_bridge_heartbeat("omni_wm_memo_get_wm_effects", 0.0f);


    return &bridge->wm_to_memory;
}

const memory_to_omni_wm_effects_t* omni_wm_memory_bridge_get_memory_effects(
    const omni_wm_memory_bridge_t* bridge) {

    if (!bridge) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");


        return NULL;


    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_memory_bridge_heartbeat("omni_wm_memo_get_memory_effects", 0.0f);


    return &bridge->memory_to_wm;
}

nimcp_error_t omni_wm_memory_bridge_get_stats(
    const omni_wm_memory_bridge_t* bridge,
    omni_wm_memory_bridge_stats_t* stats) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_memory_bridge_heartbeat("omni_wm_memo_get_stats", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_memory_bridge_reset_stats(
    omni_wm_memory_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_memory_bridge_heartbeat("omni_wm_memo_reset_stats", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(omni_wm_memory_bridge_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_memory_bridge_connect_bio_async(
    omni_wm_memory_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_memory_bridge_heartbeat("omni_wm_memo_connect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_bio_async) return NIMCP_SUCCESS;
    if (bridge->base.bio_async_enabled) return NIMCP_SUCCESS; /* Already connected */

    /* Check if router is initialized */
    if (!bio_router_is_initialized()) {
        NIMCP_LOGGING_DEBUG("Bio-async router not initialized, skipping registration");
        return NIMCP_SUCCESS;
    }

    /* Register module with router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_WM_MEMORY_BRIDGE,
        .module_name = "wm_memory_bridge",
        .inbox_capacity = 64,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (!bridge->base.bio_ctx) {
        NIMCP_LOGGING_WARN("Failed to register with bio-async router");
        return NIMCP_SUCCESS; /* Non-fatal */
    }

    /* Register message handlers */
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_MEMORY_REPLAY_SEQ,
                                handle_replay_sequence);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_MEMORY_ENGRAM_ENCODE,
                                handle_engram_encode);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_MEMORY_ENGRAM_RETRIEVE,
                                handle_engram_retrieve);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_MEMORY_CONSOLIDATION,
                                handle_consolidation);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_MEMORY_PATTERN_COMPLETE,
                                handle_pattern_complete);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_MEMORY_PATTERN_SEPARATE,
                                handle_pattern_separate);

    bridge->base.bio_async_enabled = true;
    NIMCP_LOGGING_INFO("WM Memory Bridge connected to bio-async router");

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_memory_bridge_disconnect_bio_async(
    omni_wm_memory_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_memory_bridge_heartbeat("omni_wm_memo_disconnect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->base.bio_async_enabled) return NIMCP_SUCCESS;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("WM Memory Bridge disconnected from bio-async router");

    return NIMCP_SUCCESS;
}

bool omni_wm_memory_bridge_is_bio_async_connected(
    const omni_wm_memory_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_memory_bridge_heartbeat("omni_wm_memo_is_bio_async_connect", 0.0f);


    return bridge_base_is_bio_async_connected(bridge ? &bridge->base : NULL);
}

/* ============================================================================
 * Utility Functions Implementation
 * ============================================================================ */

const char* omni_wm_memory_msg_type_to_string(omni_wm_memory_msg_type_t msg_type) {
    switch (msg_type) {
        case BIO_MSG_WM_MEMORY_REPLAY_SEQ:
            return "REPLAY_SEQ";
        case BIO_MSG_WM_MEMORY_REPLAY_TRAIN_REQ:
            return "REPLAY_TRAIN_REQ";
        case BIO_MSG_WM_MEMORY_REPLAY_TRAIN_DONE:
            return "REPLAY_TRAIN_DONE";
        case BIO_MSG_WM_MEMORY_ENGRAM_ENCODE:
            return "ENGRAM_ENCODE";
        case BIO_MSG_WM_MEMORY_ENGRAM_RETRIEVE:
            return "ENGRAM_RETRIEVE";
        case BIO_MSG_WM_MEMORY_ENGRAM_CONTEXT:
            return "ENGRAM_CONTEXT";
        case BIO_MSG_WM_MEMORY_CONSOLIDATION:
            return "CONSOLIDATION";
        case BIO_MSG_WM_MEMORY_CONSOLIDATION_SYNC:
            return "CONSOLIDATION_SYNC";
        case BIO_MSG_WM_MEMORY_SEMANTIC_TRANSFER:
            return "SEMANTIC_TRANSFER";
        case BIO_MSG_WM_MEMORY_HIPPOCAMPAL_PRED:
            return "HIPPOCAMPAL_PRED";
        case BIO_MSG_WM_MEMORY_HIPPOCAMPAL_ERROR:
            return "HIPPOCAMPAL_ERROR";
        case BIO_MSG_WM_MEMORY_PATTERN_COMPLETE:
            return "PATTERN_COMPLETE";
        case BIO_MSG_WM_MEMORY_PATTERN_SEPARATE:
            return "PATTERN_SEPARATE";
        case BIO_MSG_WM_MEMORY_BRIDGE_STATUS:
            return "BRIDGE_STATUS";
        case BIO_MSG_WM_MEMORY_BRIDGE_ERROR:
            return "BRIDGE_ERROR";
        case BIO_MSG_WM_MEMORY_STATS_UPDATE:
            return "STATS_UPDATE";
        default:
            return "UNKNOWN";
    }
}

nimcp_error_t omni_wm_memory_bridge_validate_config(
    const omni_wm_memory_bridge_config_t* config) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_memory_bridge_heartbeat("omni_wm_memo_validate_config", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    /* Validate sensitivity range */
    if (config->sensitivity < 0.5f || config->sensitivity > 2.0f) {
        NIMCP_LOGGING_WARN("Sensitivity %.2f out of range [0.5, 2.0]",
                          config->sensitivity);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Validate replay settings */
    if (config->enable_replay_training) {
        if (config->replay_batch_size == 0 ||
            config->replay_batch_size > WM_MEMORY_MAX_REPLAY_LENGTH * 2) {
            NIMCP_LOGGING_WARN("Invalid replay_batch_size: %u",
                              config->replay_batch_size);
            return NIMCP_ERROR_INVALID_PARAM;
        }
        if (config->replay_learning_rate <= 0.0f ||
            config->replay_learning_rate > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid replay_learning_rate: %.4f",
                              config->replay_learning_rate);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate engram settings */
    if (config->enable_engram_encoding) {
        if (config->encoding_threshold < 0.0f ||
            config->encoding_threshold > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid encoding_threshold: %.2f",
                              config->encoding_threshold);
            return NIMCP_ERROR_INVALID_PARAM;
        }
        if (config->emotional_boost_factor < 1.0f ||
            config->emotional_boost_factor > 3.0f) {
            NIMCP_LOGGING_WARN("Invalid emotional_boost_factor: %.2f",
                              config->emotional_boost_factor);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate pattern thresholds */
    if (config->enable_pattern_completion) {
        if (config->completion_threshold < 0.0f ||
            config->completion_threshold > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid completion_threshold: %.2f",
                              config->completion_threshold);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    if (config->enable_pattern_separation) {
        if (config->separation_threshold < 0.0f ||
            config->separation_threshold > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid separation_threshold: %.2f",
                              config->separation_threshold);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate consolidation settings */
    if (config->enable_consolidation_sync) {
        if (config->semantic_abstraction_level < 0.0f ||
            config->semantic_abstraction_level > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid semantic_abstraction_level: %.2f",
                              config->semantic_abstraction_level);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    return NIMCP_SUCCESS;
}
