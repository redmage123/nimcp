/**
 * @file nimcp_brain_probes.h
 * @brief Unified brain probe system — attach read-only observers to any module
 *
 * Probes collect key-value metrics from brain modules identified by bio_module_id_t.
 * Active probes sample periodically via a background thread. Passive probes subscribe
 * to the event bus. Double-buffered metric storage ensures zero contention with
 * brain_decide()/brain_learn_vector().
 *
 * Usage:
 *   probe_registry_t* reg = probe_registry_create(brain);
 *   probe_handle_t h = probe_attach_network_metrics(reg, 1000);
 *   probe_registry_start(reg);
 *   // ... later ...
 *   probe_metric_t metrics[64];
 *   uint32_t count = probe_get_metrics(reg, h, metrics, 64);
 */
#ifndef NIMCP_BRAIN_PROBES_H
#define NIMCP_BRAIN_PROBES_H

#include <stdint.h>
#include <stdbool.h>

/* Use volatile + GCC __atomic builtins for cross C/C++ compatibility.
 * Avoids C11 _Atomic (breaks in C++ extern "C") and std::atomic
 * (breaks with memset). The .c files use __atomic_load_n / __atomic_store_n. */

/* Forward declarations to avoid circular includes */
struct brain_struct;
typedef struct brain_struct* brain_t;

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define PROBE_MAX_METRICS       64   /**< Max key-value pairs per probe */
#define PROBE_MAX_MODULES       16   /**< Max modules per composite probe */
#define PROBE_MAX_EVENT_SUBS    16   /**< Max event subscriptions per probe */
#define PROBE_REGISTRY_MAX      32   /**< Max simultaneous probes */
#define PROBE_KEY_LEN           64   /**< Max metric key length */
#define PROBE_STRING_LEN        64   /**< Max string value length */
#define PROBE_NAME_LEN          64   /**< Max probe name length */

/* ============================================================================
 * Handle Type
 * ============================================================================ */

typedef uint32_t probe_handle_t;
#define PROBE_INVALID_HANDLE    0

/* ============================================================================
 * Metric Value (tagged union)
 * ============================================================================ */

typedef enum {
    PROBE_METRIC_FLOAT  = 0,
    PROBE_METRIC_INT    = 1,
    PROBE_METRIC_STRING = 2
} probe_metric_type_t;

typedef struct probe_metric {
    char                key[PROBE_KEY_LEN];
    probe_metric_type_t type;
    union {
        float           f;
        int64_t         i;
        char            s[PROBE_STRING_LEN];
    } value;
    uint64_t            timestamp_us;
} probe_metric_t;

/* ============================================================================
 * Probe Mode
 * ============================================================================ */

typedef enum {
    PROBE_MODE_ACTIVE  = 0x01,  /**< Periodic sampling via background thread */
    PROBE_MODE_PASSIVE = 0x02,  /**< Event bus subscriber */
    PROBE_MODE_HYBRID  = 0x03   /**< Both active + passive */
} probe_mode_t;

/* ============================================================================
 * Sample Function (vtable callback for active probes)
 * ============================================================================ */

/**
 * @brief Callback invoked by the sampler thread to collect metrics from a module.
 *
 * @param module_ptr     The module's void* pointer from brain_struct (may be NULL)
 * @param module_id      The bio_module_id identifying this module
 * @param brain          The brain struct (for accessing cross-module state)
 * @param metrics        Output array — caller provides, callee fills
 * @param metric_count   In: max slots available. Out: slots filled.
 * @param user_data      Opaque context from probe creation
 */
typedef void (*probe_sample_fn)(
    void*           module_ptr,
    uint16_t        module_id,
    brain_t         brain,
    probe_metric_t* metrics,
    uint32_t*       metric_count,
    void*           user_data
);

/* ============================================================================
 * Probe Struct
 * ============================================================================ */

typedef struct probe {
    probe_handle_t      handle;
    char                name[PROBE_NAME_LEN];
    probe_mode_t        mode;
    bool                active;

    /* Target modules (composite: >1 entry) */
    uint16_t            modules[PROBE_MAX_MODULES];
    uint32_t            num_modules;

    /* Active mode: periodic sampling */
    probe_sample_fn     sample_fn;
    void*               sample_user_data;
    uint32_t            interval_ms;
    uint64_t            last_sample_us;

    /* Passive mode: event subscriptions (opaque handles) */
    uint64_t            event_subs[PROBE_MAX_EVENT_SUBS];
    uint32_t            num_event_subs;

    /* Collected metrics — double-buffered for lock-free reads.
     * Writer fills metrics[write_buf], then flips active_buf atomically.
     * Readers always read metrics[!active_buf] (stable snapshot). */
    probe_metric_t      metrics[2][PROBE_MAX_METRICS];
    uint32_t            metric_count[2];
    volatile uint32_t   active_buf;     /**< 0 or 1 — which buffer is "live" */
} probe_t;

/* ============================================================================
 * Probe Registry
 * ============================================================================ */

typedef struct probe_registry {
    probe_t             probes[PROBE_REGISTRY_MAX];
    uint32_t            count;
    volatile uint32_t   next_handle;

    /* Sampler thread */
    void*               sampler_thread;     /**< nimcp_thread_t* */
    volatile bool       sampler_running;

    /* References */
    void*               event_bus;          /**< event_bus_t */
    brain_t             brain;

    /* Stage subscriptions: which probes want which pipeline stages.
     * stage_subscribers[stage_id] is a bitmask of probe indices (0-31). */
    uint32_t            stage_subscribers[64];
} probe_registry_t;

/* ============================================================================
 * Registry Lifecycle
 * ============================================================================ */

probe_registry_t*   probe_registry_create(brain_t brain);
void                probe_registry_destroy(probe_registry_t* reg);
bool                probe_registry_start(probe_registry_t* reg);
void                probe_registry_stop(probe_registry_t* reg);

/* ============================================================================
 * Probe Creation
 * ============================================================================ */

/**
 * @brief Create an active probe that samples periodically.
 *
 * @param reg           Probe registry
 * @param name          Human-readable probe name (e.g., "network_metrics")
 * @param modules       Array of bio_module_id_t values to target
 * @param num_modules   Number of modules (1 = single, >1 = composite)
 * @param sample_fn     Callback invoked by sampler thread
 * @param user_data     Opaque context passed to sample_fn
 * @param interval_ms   Sampling interval in milliseconds
 * @return              Probe handle, or PROBE_INVALID_HANDLE on failure
 */
probe_handle_t probe_create_active(
    probe_registry_t* reg,
    const char* name,
    const uint16_t* modules, uint32_t num_modules,
    probe_sample_fn sample_fn, void* user_data,
    uint32_t interval_ms
);

/**
 * @brief Create a passive probe that subscribes to event bus events.
 */
probe_handle_t probe_create_passive(
    probe_registry_t* reg,
    const char* name,
    const uint16_t* modules, uint32_t num_modules,
    const uint32_t* event_types, uint32_t num_event_types
);

/**
 * @brief Create a hybrid probe (active + passive).
 */
probe_handle_t probe_create_hybrid(
    probe_registry_t* reg,
    const char* name,
    const uint16_t* modules, uint32_t num_modules,
    probe_sample_fn sample_fn, void* user_data,
    uint32_t interval_ms,
    const uint32_t* event_types, uint32_t num_event_types
);

/**
 * @brief Destroy a probe and free its resources.
 */
bool probe_destroy(probe_registry_t* reg, probe_handle_t handle);

/* ============================================================================
 * Query
 * ============================================================================ */

/**
 * @brief Get current metrics from a single probe (lock-free read).
 *
 * Copies from the inactive (stable) buffer. Safe to call from any thread.
 *
 * @param reg           Probe registry
 * @param handle        Probe handle
 * @param out_metrics   Caller-allocated array
 * @param max_metrics   Size of out_metrics array
 * @return              Number of metrics copied, 0 on error
 */
uint32_t probe_get_metrics(
    probe_registry_t* reg,
    probe_handle_t handle,
    probe_metric_t* out_metrics,
    uint32_t max_metrics
);

/**
 * @brief Get all probe metrics as a JSON string.
 *
 * @param reg           Probe registry
 * @return              Heap-allocated JSON string (caller must free), or NULL
 */
char* probe_get_all_metrics_json(probe_registry_t* reg);

/* ============================================================================
 * Built-in Samplers (convenience functions)
 * ============================================================================ */

probe_handle_t probe_attach_network_metrics(probe_registry_t* reg, uint32_t interval_ms);
probe_handle_t probe_attach_cognitive_stats(probe_registry_t* reg, uint32_t interval_ms);
probe_handle_t probe_attach_training_dashboard(probe_registry_t* reg, uint32_t interval_ms);
probe_handle_t probe_attach_inference(probe_registry_t* reg, uint32_t interval_ms);
probe_handle_t probe_attach_glial(probe_registry_t* reg, uint32_t interval_ms);
probe_handle_t probe_attach_neurons(probe_registry_t* reg, uint32_t interval_ms);
probe_handle_t probe_attach_synapses(probe_registry_t* reg, uint32_t interval_ms);
probe_handle_t probe_attach_brain_regions(probe_registry_t* reg, uint32_t interval_ms);
probe_handle_t probe_attach_dispatch(probe_registry_t* reg, uint32_t interval_ms);

/* ============================================================================
 * Pipeline Stage Probes
 * ============================================================================ */

#include "core/probes/nimcp_probe_stages.h"

/* ============================================================================
 * Module Resolution (internal — maps bio_module_id to void*)
 * ============================================================================ */

void* probe_resolve_module(brain_t brain, uint16_t module_id);
const char* probe_module_name(uint16_t module_id);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_PROBES_H */
