/**
 * @file nimcp_rcog_engine.c
 * @brief Recursive Cognition Engine - Main Processing Coordinator Implementation
 * @version 1.0.0
 * @date 2026-01-03
 */

#include "cognitive/recursive/nimcp_rcog_engine.h"
#include "cognitive/recursive/nimcp_rcog_context_store.h"
#include "cognitive/recursive/nimcp_rcog_orchestrator.h"
#include "cognitive/recursive/nimcp_rcog_delegation_pool.h"
#include "cognitive/recursive/nimcp_rcog_tool_router.h"
#include "cognitive/recursive/nimcp_rcog_answer.h"
#include "cognitive/recursive/nimcp_rcog_bio_async_bridge.h"
#include "cognitive/recursive/nimcp_rcog_immune_bridge.h"
#include "cognitive/recursive/nimcp_rcog_imagination_bridge.h"
#include "cognitive/recursive/nimcp_rcog_collective_bridge.h"
#include "cognitive/recursive/nimcp_rcog_brain_kg_bridge.h"
#include "cognitive/recursive/nimcp_rcog_snn_bridge.h"
#include "cognitive/recursive/nimcp_rcog_plasticity_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(rcog_engine, MESH_ADAPTER_CATEGORY_COGNITIVE)

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Active request tracking
 */
typedef struct {
    rcog_request_handle_t* handle;
    rcog_process_request_t request;
    rcog_decomposition_t decomp;
    rcog_batch_handle_t* batch;
    rcog_answer_state_t* answer;
    uint64_t started_ms;
    bool active;
} rcog_active_request_t;

/**
 * @brief Engine internal state
 */
struct rcog_engine {
    /* Configuration */
    rcog_engine_config_t config;

    /* State */
    rcog_engine_state_t state;
    nimcp_mutex_t* mutex;

    /* Subsystems */
    rcog_context_store_t* context_store;
    rcog_orchestrator_t* orchestrator;
    rcog_delegation_pool_t* delegation_pool;
    rcog_tool_router_t* tool_router;
    rcog_answer_refiner_t* answer_refiner;

    /* Bridges */
    struct rcog_bio_async_bridge* bio_async_bridge;
    struct rcog_immune_bridge* immune_bridge;
    struct rcog_imagination_bridge* imagination_bridge;
    struct rcog_collective_bridge* collective_bridge;
    struct rcog_brain_kg_bridge* brain_kg_bridge;
    rcog_snn_bridge_t* snn_bridge;
    rcog_plasticity_bridge_t* plasticity_bridge;
    bool bridges_enabled;

    /* Active requests */
    rcog_active_request_t requests[RCOG_ENGINE_MAX_CONCURRENT_GOALS];
    uint32_t active_count;
    uint64_t next_request_id;
    nimcp_cond_t* request_cond;

    /* Immune modulation */
    rcog_immune_modulation_t current_modulation;
    bool in_degraded_mode;

    /* Statistics */
    rcog_engine_stats_t stats;
    uint64_t stats_start_ms;

    /* Flags */
    bool subsystems_created;
    bool subsystems_connected;
};

/*=============================================================================
 * HELPER FUNCTIONS
 *===========================================================================*/


/*=============================================================================
 * SUBSYSTEM ACCESS
 *===========================================================================*/

struct rcog_context_store* rcog_engine_get_context_store(rcog_engine_t* engine) {
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "engine is NULL");

        return NULL;
    }
    return engine->context_store;
}

struct rcog_orchestrator* rcog_engine_get_orchestrator(rcog_engine_t* engine) {
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "engine is NULL");

        return NULL;
    }
    return engine->orchestrator;
}

struct rcog_delegation_pool* rcog_engine_get_delegation_pool(rcog_engine_t* engine) {
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "engine is NULL");

        return NULL;
    }
    return engine->delegation_pool;
}

struct rcog_tool_router* rcog_engine_get_tool_router(rcog_engine_t* engine) {
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "engine is NULL");

        return NULL;
    }
    return engine->tool_router;
}

struct rcog_answer_refiner* rcog_engine_get_answer_refiner(rcog_engine_t* engine) {
    if (!engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "engine is NULL");

        return NULL;
    }
    return engine->answer_refiner;
}

/*=============================================================================
 * BRIDGE CONNECTION
 *===========================================================================*/

int rcog_engine_connect_bio_async(
    rcog_engine_t* engine,
    struct rcog_bio_async_bridge* bridge
) {
    if (!engine) {
        return RCOG_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_engine_heartbeat("rcog_engine_connect_bio_async", 0.0f);


    nimcp_mutex_lock(engine->mutex);
    engine->bio_async_bridge = bridge;

    /* Connect bridge to subsystems if enabled */
    if (bridge && engine->config.enable_bio_async) {
        if (engine->delegation_pool) {
            rcog_delegation_pool_connect_bio_async(engine->delegation_pool, bridge);
        }
        if (engine->tool_router) {
            rcog_tool_router_connect_bio_async(engine->tool_router, bridge);
        }
    }

    nimcp_mutex_unlock(engine->mutex);
    return 0;
}



// Forward declarations for static functions (SRP split)
static uint64_t engine_now_ms(void);
static rcog_active_request_t* find_request_by_id(rcog_engine_t* engine, uint64_t request_id);
static rcog_active_request_t* allocate_request_slot(rcog_engine_t* engine);
static void release_request_slot(rcog_engine_t* engine, rcog_active_request_t* req);
static void apply_modulation_to_config(rcog_engine_t* engine);

//=============================================================================
// SRP Split: Function implementations organized by responsibility
//=============================================================================
#include "nimcp_rcog_engine_part_accessors.c"  // 11 functions: accessors
#include "nimcp_rcog_engine_part_core.c"  // 22 functions: core
#include "nimcp_rcog_engine_part_processing.c"  // 4 functions: processing
#include "nimcp_rcog_engine_part_helpers.c"  // 4 functions: helpers
#include "nimcp_rcog_engine_part_lifecycle.c"  // 8 functions: lifecycle
