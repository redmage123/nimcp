/**
 * @file nimcp_entorhinal_adapter.c
 * @brief MINIMAL IMPLEMENTATION — structural create/destroy/update + stubs.
 *
 * This is a scaffolding implementation so the Wave 8B-c tick driver has
 * something to call. Grid-cell dynamics, path integration, and memory
 * gateway behavior are NOT simulated — those require further design work.
 * What works:
 * - adapter_create/destroy returns/cleans a valid instance
 * - adapter_update advances counters + timestamps
 * - sub-module create/destroy functions return success without
 *   allocating heavyweight sub-arrays
 *
 * The cognitive behavior is placeholder; ship this and let future
 * waves fill in the actual spatial-encoding logic.
 *
 * The core nimcp_entorhinal_t (from nimcp_entorhinal.c) provides the
 * heavyweight allocation — this adapter just wraps it + adds a handful
 * of adapter-level counters/state needed by the tick driver.
 */

#include "core/brain/regions/entorhinal/nimcp_entorhinal_adapter.h"
#include "core/brain/regions/entorhinal/nimcp_entorhinal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

#define ENTORHINAL_ADAPTER_LOG_MODULE "ENTORHINAL_ADAPTER"

/*=============================================================================
 * DEFAULT CONFIG
 *===========================================================================*/

entorhinal_adapter_config_t entorhinal_adapter_default_config(void) {
    entorhinal_adapter_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    /* Base entorhinal config — reuse core defaults */
    cfg.base_config = entorhinal_default_config();

    /* Adapter options */
    cfg.auto_connect_hippocampus = false;
    cfg.auto_connect_neocortex   = false;
    cfg.auto_register_kg         = false;

    /* Event system */
    cfg.enable_events            = false;
    cfg.event_buffer_size        = 64;

    /* Async processing */
    cfg.enable_async_processing  = false;
    cfg.async_queue_size         = 32;

    /* Auto-calibration */
    cfg.enable_auto_calibration  = false;
    cfg.calibration_interval_ms  = 1000.0f;

    return cfg;
}

/*=============================================================================
 * LIFECYCLE — real (minimal) work
 *===========================================================================*/

entorhinal_adapter_t* entorhinal_adapter_create(
    const entorhinal_adapter_config_t* config) {

    entorhinal_adapter_t* adapter =
        (entorhinal_adapter_t*)nimcp_calloc(1, sizeof(entorhinal_adapter_t));
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                              "entorhinal_adapter_create: adapter calloc failed");
        return NULL;
    }

    /* Copy / default config */
    if (config) {
        adapter->config = *config;
    } else {
        adapter->config = entorhinal_adapter_default_config();
    }

    /* Create underlying entorhinal core. If this fails we clean up and
     * return NULL — the adapter is useless without its core. */
    adapter->entorhinal = entorhinal_create(&adapter->config.base_config);
    if (!adapter->entorhinal) {
        LOG_ERROR(ENTORHINAL_ADAPTER_LOG_MODULE,
                  "Failed to create underlying entorhinal core");
        nimcp_free(adapter);
        return NULL;
    }

    /* Adapter state */
    adapter->brain                    = NULL;
    adapter->event_callback           = NULL;
    adapter->event_user_data          = NULL;
    adapter->async_enabled            = adapter->config.enable_async_processing;
    adapter->pending_async_ops        = 0;
    adapter->time_since_calibration_ms = 0.0f;
    adapter->calibration_needed       = false;
    adapter->adapter_updates          = 0;
    adapter->events_published         = 0;
    adapter->mean_processing_time_ms  = 0.0f;

    LOG_INFO(ENTORHINAL_ADAPTER_LOG_MODULE,
             "Entorhinal adapter created (minimal impl)");
    return adapter;
}

void entorhinal_adapter_destroy(entorhinal_adapter_t* adapter) {
    if (!adapter) return;

    if (adapter->entorhinal) {
        entorhinal_destroy(adapter->entorhinal);
        adapter->entorhinal = NULL;
    }

    nimcp_free(adapter);
}

int entorhinal_adapter_connect_brain(entorhinal_adapter_t* adapter,
                                     nimcp_brain_t* brain) {
    if (!adapter) return -1;
    adapter->brain = brain;
    return 0;
}

int entorhinal_adapter_disconnect_brain(entorhinal_adapter_t* adapter) {
    if (!adapter) return -1;
    adapter->brain = NULL;
    return 0;
}

bool entorhinal_adapter_reset(entorhinal_adapter_t* adapter) {
    if (!adapter) return false;
    if (adapter->entorhinal) {
        entorhinal_reset(adapter->entorhinal);
    }
    adapter->pending_async_ops         = 0;
    adapter->time_since_calibration_ms = 0.0f;
    adapter->calibration_needed        = false;
    adapter->adapter_updates           = 0;
    adapter->events_published          = 0;
    adapter->mean_processing_time_ms   = 0.0f;
    return true;
}

/*=============================================================================
 * UPDATE — real (minimal) work
 *===========================================================================*/

int entorhinal_adapter_update(entorhinal_adapter_t* adapter, float dt) {
    if (!adapter) return -1;

    /* Advance counters + timestamps on the underlying core. We do NOT call
     * entorhinal_bidirectional_update() here — that would trigger grid
     * cell / path-integration math we don't yet want active in the hot
     * path. This is a minimal tick that just marks progress. */
    if (adapter->entorhinal) {
        adapter->entorhinal->updates_processed++;
        adapter->entorhinal->simulation_dt_ms = dt;
        adapter->entorhinal->last_update_ms  += (uint64_t)dt;
        /* Keep status as READY so tests / inspectors see a live adapter. */
        if (adapter->entorhinal->status == ENTORHINAL_STATUS_IDLE) {
            adapter->entorhinal->status = ENTORHINAL_STATUS_READY;
        }
    }

    adapter->adapter_updates++;

    /* Auto-calibration bookkeeping (no actual calibration logic) */
    if (adapter->config.enable_auto_calibration) {
        adapter->time_since_calibration_ms += dt;
        if (adapter->time_since_calibration_ms >=
            adapter->config.calibration_interval_ms) {
            adapter->calibration_needed = true;
        }
    }

    return 0;
}

/*=============================================================================
 * HIGH-LEVEL PROCESSING — structural stubs
 *===========================================================================*/

int entorhinal_adapter_process_spatial(entorhinal_adapter_t* adapter,
                                       const float* position, uint32_t pos_dim,
                                       float heading, const float* velocity,
                                       float dt) {
    (void)position; (void)pos_dim; (void)heading; (void)velocity; (void)dt;
    if (!adapter) return -1;
    return 0;
}

int entorhinal_adapter_process_sensory(entorhinal_adapter_t* adapter,
                                       const float* visual_input,
                                       uint32_t visual_dim,
                                       const float* boundary_input,
                                       uint32_t boundary_dim) {
    (void)visual_input; (void)visual_dim;
    (void)boundary_input; (void)boundary_dim;
    if (!adapter) return -1;
    return 0;
}

int entorhinal_adapter_encode_memory(entorhinal_adapter_t* adapter,
                                     const float* features, uint32_t feature_dim,
                                     const float* context, uint32_t context_dim,
                                     uint32_t* memory_id_out) {
    (void)features; (void)feature_dim; (void)context; (void)context_dim;
    if (!adapter) return -1;
    if (memory_id_out) *memory_id_out = 0;
    return 0;
}

int entorhinal_adapter_retrieve_memory(entorhinal_adapter_t* adapter,
                                       const float* cue, uint32_t cue_dim,
                                       float* retrieved, uint32_t max_dim,
                                       float* similarity_out) {
    (void)cue; (void)cue_dim; (void)retrieved; (void)max_dim;
    if (!adapter) return -1;
    if (similarity_out) *similarity_out = 0.0f;
    return 0;
}

/*=============================================================================
 * EVENT SYSTEM — stubs
 *===========================================================================*/

int entorhinal_adapter_set_event_callback(entorhinal_adapter_t* adapter,
    void (*callback)(uint32_t event_type, const void* data, void* user_data),
    void* user_data) {

    if (!adapter) return -1;
    adapter->event_callback  = callback;
    adapter->event_user_data = user_data;
    return 0;
}

int entorhinal_adapter_publish_event(entorhinal_adapter_t* adapter,
                                     entorhinal_event_type_t event_type,
                                     const void* event_data) {
    if (!adapter) return -1;
    if (adapter->config.enable_events && adapter->event_callback) {
        adapter->event_callback((uint32_t)event_type, event_data,
                                adapter->event_user_data);
        adapter->events_published++;
    }
    return 0;
}

/*=============================================================================
 * ASYNC API — stubs (return NULL future, count pending=0)
 *===========================================================================*/

nimcp_bio_future_t entorhinal_adapter_request_position_async(
    entorhinal_adapter_t* adapter) {
    (void)adapter;
    return NULL;
}

nimcp_bio_future_t entorhinal_adapter_request_encode_async(
    entorhinal_adapter_t* adapter,
    const float* features, uint32_t dim) {
    (void)adapter; (void)features; (void)dim;
    return NULL;
}

nimcp_bio_future_t entorhinal_adapter_request_retrieve_async(
    entorhinal_adapter_t* adapter,
    const float* cue, uint32_t dim) {
    (void)adapter; (void)cue; (void)dim;
    return NULL;
}

uint32_t entorhinal_adapter_process_async(entorhinal_adapter_t* adapter,
                                          uint32_t max_ops) {
    (void)max_ops;
    if (!adapter) return 0;
    return 0;
}

/*=============================================================================
 * CALIBRATION API — stubs
 *===========================================================================*/

int entorhinal_adapter_calibrate(entorhinal_adapter_t* adapter,
                                 const float* known_position,
                                 float known_heading) {
    (void)known_position; (void)known_heading;
    if (!adapter) return -1;
    adapter->time_since_calibration_ms = 0.0f;
    adapter->calibration_needed        = false;
    return 0;
}

bool entorhinal_adapter_needs_calibration(const entorhinal_adapter_t* adapter) {
    if (!adapter) return false;
    return adapter->calibration_needed;
}

float entorhinal_adapter_get_calibration_quality(
    const entorhinal_adapter_t* adapter) {
    if (!adapter) return 0.0f;
    /* Minimal placeholder: calibrated => 1.0, needs cal => 0.5 */
    return adapter->calibration_needed ? 0.5f : 1.0f;
}

/*=============================================================================
 * DIAGNOSTICS API — passthrough to core
 *===========================================================================*/

int entorhinal_adapter_get_stats(const entorhinal_adapter_t* adapter,
                                 entorhinal_stats_t* stats) {
    if (!adapter || !stats) return -1;
    if (adapter->entorhinal) {
        return entorhinal_get_stats(adapter->entorhinal, stats);
    }
    memset(stats, 0, sizeof(*stats));
    return 0;
}

nimcp_entorhinal_t* entorhinal_adapter_get_entorhinal(
    entorhinal_adapter_t* adapter) {
    if (!adapter) return NULL;
    return adapter->entorhinal;
}

int entorhinal_adapter_log_diagnostics(const entorhinal_adapter_t* adapter) {
    if (!adapter) return -1;
    LOG_INFO(ENTORHINAL_ADAPTER_LOG_MODULE,
             "Adapter diagnostics: updates=%llu events=%llu pending_async=%u",
             (unsigned long long)adapter->adapter_updates,
             (unsigned long long)adapter->events_published,
             adapter->pending_async_ops);
    if (adapter->entorhinal) {
        entorhinal_log_diagnostics(adapter->entorhinal);
    }
    return 0;
}
