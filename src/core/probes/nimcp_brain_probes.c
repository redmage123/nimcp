/**
 * @file nimcp_brain_probes.c
 * @brief Probe registry — create/destroy, sampler thread, double-buffer, query
 */

#include "core/probes/nimcp_brain_probes.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"

#include <string.h>
#include <stdio.h>

#define LOG_MODULE "PROBE"

/* ============================================================================
 * Internal: Find probe by handle
 * ============================================================================ */

static probe_t* find_probe(probe_registry_t* reg, probe_handle_t handle) {
    if (!reg || handle == PROBE_INVALID_HANDLE) return NULL;
    for (uint32_t i = 0; i < reg->count; i++) {
        if (reg->probes[i].handle == handle && reg->probes[i].active) {
            return &reg->probes[i];
        }
    }
    return NULL;
}

/* ============================================================================
 * Internal: Allocate probe slot
 * ============================================================================ */

static probe_t* alloc_probe_slot(probe_registry_t* reg) {
    if (!reg) return NULL;
    /* First try to reuse an inactive slot */
    for (uint32_t i = 0; i < reg->count; i++) {
        if (!reg->probes[i].active) {
            return &reg->probes[i];
        }
    }
    /* Append if room */
    if (reg->count < PROBE_REGISTRY_MAX) {
        return &reg->probes[reg->count++];
    }
    return NULL;
}

/* ============================================================================
 * Internal: Sample one probe (called from sampler thread)
 * ============================================================================ */

static void sample_probe(probe_registry_t* reg, probe_t* probe) {
    if (!probe || !probe->active || !probe->sample_fn) return;

    /* Determine write buffer (opposite of active/read buffer) */
    uint32_t read_buf = __atomic_load_n(&probe->active_buf, __ATOMIC_ACQUIRE);
    uint32_t write_buf = 1 - read_buf;

    uint32_t total_count = 0;

    /* Sample each target module */
    for (uint32_t m = 0; m < probe->num_modules && total_count < PROBE_MAX_METRICS; m++) {
        void* module_ptr = probe_resolve_module(reg->brain, probe->modules[m]);
        uint32_t remaining = PROBE_MAX_METRICS - total_count;
        uint32_t filled = remaining;

        probe->sample_fn(
            module_ptr,
            probe->modules[m],
            reg->brain,
            &probe->metrics[write_buf][total_count],
            &filled,
            probe->sample_user_data
        );

        /* Namespace metrics with module name for composites */
        if (probe->num_modules > 1 && filled > 0) {
            const char* mod_name = probe_module_name(probe->modules[m]);
            if (mod_name) {
                for (uint32_t k = total_count; k < total_count + filled; k++) {
                    char prefixed[PROBE_KEY_LEN];
                    snprintf(prefixed, sizeof(prefixed), "%s.%s", mod_name, probe->metrics[write_buf][k].key);
                    strncpy(probe->metrics[write_buf][k].key, prefixed, PROBE_KEY_LEN - 1);
                    probe->metrics[write_buf][k].key[PROBE_KEY_LEN - 1] = '\0';
                }
            }
        }

        total_count += filled;
    }

    probe->metric_count[write_buf] = total_count;
    probe->last_sample_us = nimcp_time_get_us();

    /* Atomic flip: make write buffer the new read buffer */
    __atomic_store_n(&probe->active_buf, write_buf, __ATOMIC_RELEASE);
}

/* ============================================================================
 * Sampler Thread
 * ============================================================================ */

static void* sampler_thread_fn(void* arg) {
    probe_registry_t* reg = (probe_registry_t*)arg;
    NIMCP_LOGGING_INFO("Probe sampler thread started");

    while (reg->sampler_running) {
        uint64_t now = nimcp_time_get_us();
        uint64_t min_sleep_us = 100000;  /* 100ms default sleep */

        /* Snapshot count to avoid race with concurrent alloc_probe_slot */
        uint32_t snap_count = reg->count;
        for (uint32_t i = 0; i < snap_count && i < PROBE_REGISTRY_MAX; i++) {
            probe_t* p = &reg->probes[i];
            if (!p->active || !(p->mode & PROBE_MODE_ACTIVE)) continue;
            if (p->interval_ms == 0) continue;

            uint64_t interval_us = (uint64_t)p->interval_ms * 1000;
            uint64_t elapsed = now - p->last_sample_us;

            if (elapsed >= interval_us) {
                sample_probe(reg, p);
            } else {
                uint64_t remaining = interval_us - elapsed;
                if (remaining < min_sleep_us) min_sleep_us = remaining;
            }
        }

        /* Sleep until next probe is due */
        nimcp_time_sleep_us(min_sleep_us);
    }

    NIMCP_LOGGING_INFO("Probe sampler thread stopped");
    return NULL;
}

/* ============================================================================
 * Registry Lifecycle
 * ============================================================================ */

probe_registry_t* probe_registry_create(brain_t brain) {
    if (!brain) return NULL;

    probe_registry_t* reg = (probe_registry_t*)nimcp_calloc(1, sizeof(probe_registry_t));
    if (!reg) return NULL;

    reg->brain = brain;
    reg->event_bus = brain->event_bus;
    __atomic_store_n(&reg->next_handle, 1, __ATOMIC_SEQ_CST);
    reg->sampler_running = false;

    NIMCP_LOGGING_INFO("Probe registry created");
    return reg;
}

void probe_registry_destroy(probe_registry_t* reg) {
    if (!reg) return;

    probe_registry_stop(reg);

    /* Destroy all active probes */
    for (uint32_t i = 0; i < reg->count; i++) {
        if (reg->probes[i].active) {
            reg->probes[i].active = false;
        }
    }

    nimcp_free(reg);
    NIMCP_LOGGING_INFO("Probe registry destroyed");
}

bool probe_registry_start(probe_registry_t* reg) {
    if (!reg || reg->sampler_running) return false;

    reg->sampler_running = true;
    nimcp_thread_t thread = 0;
    int rc = nimcp_thread_create(&thread, sampler_thread_fn, reg, NULL);
    if (rc != 0) {
        reg->sampler_running = false;
        NIMCP_LOGGING_ERROR("Failed to start probe sampler thread");
        return false;
    }
    reg->sampler_thread = (void*)(uintptr_t)thread;
    return true;
}

void probe_registry_stop(probe_registry_t* reg) {
    if (!reg || !reg->sampler_running) return;

    reg->sampler_running = false;
    /* Wait for sampler thread to exit (up to 1 second) */
    if (reg->sampler_thread) {
        for (int i = 0; i < 10; i++) {
            nimcp_time_sleep_us(100000);  /* 100ms */
            if (!reg->sampler_running) break;
        }
        /* Thread should have exited by now. Even if not, we'll
         * proceed to destroy — the thread checks sampler_running
         * at the top of each loop iteration. */
    }
    reg->sampler_thread = NULL;
}

/* ============================================================================
 * Probe Creation
 * ============================================================================ */

static probe_handle_t init_probe(
    probe_registry_t* reg,
    const char* name,
    probe_mode_t mode,
    const uint16_t* modules, uint32_t num_modules,
    probe_sample_fn sample_fn, void* user_data,
    uint32_t interval_ms)
{
    if (!reg) return PROBE_INVALID_HANDLE;

    probe_t* p = alloc_probe_slot(reg);
    if (!p) {
        NIMCP_LOGGING_WARN("Probe registry full (%u/%u)", reg->count, PROBE_REGISTRY_MAX);
        return PROBE_INVALID_HANDLE;
    }

    /* Clear stale stage subscriber bits before reuse (Bug 8 fix) */
    uint32_t slot_idx = (uint32_t)(p - reg->probes);
    if (slot_idx < 32) {
        for (uint32_t s = 0; s < PROBE_STAGE_COUNT; s++) {
            reg->stage_subscribers[s] &= ~(1u << slot_idx);
        }
    }

    memset(p, 0, sizeof(probe_t));
    p->handle = __atomic_fetch_add(&reg->next_handle, 1, __ATOMIC_SEQ_CST);
    if (name) {
        strncpy(p->name, name, PROBE_NAME_LEN - 1);
    }
    p->mode = mode;
    p->active = true;

    /* Copy target modules */
    p->num_modules = (num_modules > PROBE_MAX_MODULES) ? PROBE_MAX_MODULES : num_modules;
    if (modules && p->num_modules > 0) {
        memcpy(p->modules, modules, p->num_modules * sizeof(uint16_t));
    }

    /* Active probe setup */
    p->sample_fn = sample_fn;
    p->sample_user_data = user_data;
    p->interval_ms = interval_ms;
    p->last_sample_us = nimcp_time_get_us();

    /* Initialize double buffer */
    __atomic_store_n(&p->active_buf, 0, __ATOMIC_SEQ_CST);

    /* Subscribe to all pipeline stages by default.
     * Probes that want specific stages can be filtered later.
     * The bitmask tracks which probe indices care about each stage. */
    uint32_t probe_idx = (uint32_t)(p - reg->probes);
    if (probe_idx < 32) {
        for (uint32_t s = 0; s < PROBE_STAGE_COUNT; s++) {
            reg->stage_subscribers[s] |= (1u << probe_idx);
        }
    }

    NIMCP_LOGGING_INFO("Probe created: \"%s\" (handle=%u, mode=%d, modules=%u)",
                       p->name, p->handle, mode, p->num_modules);
    return p->handle;
}

probe_handle_t probe_create_active(
    probe_registry_t* reg,
    const char* name,
    const uint16_t* modules, uint32_t num_modules,
    probe_sample_fn sample_fn, void* user_data,
    uint32_t interval_ms)
{
    if (!sample_fn) return PROBE_INVALID_HANDLE;
    return init_probe(reg, name, PROBE_MODE_ACTIVE, modules, num_modules,
                      sample_fn, user_data, interval_ms);
}

probe_handle_t probe_create_passive(
    probe_registry_t* reg,
    const char* name,
    const uint16_t* modules, uint32_t num_modules,
    const uint32_t* event_types, uint32_t num_event_types)
{
    probe_handle_t h = init_probe(reg, name, PROBE_MODE_PASSIVE,
                                   modules, num_modules, NULL, NULL, 0);
    if (h == PROBE_INVALID_HANDLE) return h;

    /* TODO: subscribe to event bus for specified event types */
    (void)event_types;
    (void)num_event_types;
    return h;
}

probe_handle_t probe_create_hybrid(
    probe_registry_t* reg,
    const char* name,
    const uint16_t* modules, uint32_t num_modules,
    probe_sample_fn sample_fn, void* user_data,
    uint32_t interval_ms,
    const uint32_t* event_types, uint32_t num_event_types)
{
    if (!sample_fn) return PROBE_INVALID_HANDLE;
    probe_handle_t h = init_probe(reg, name, PROBE_MODE_HYBRID,
                                   modules, num_modules,
                                   sample_fn, user_data, interval_ms);
    if (h == PROBE_INVALID_HANDLE) return h;

    /* TODO: subscribe to event bus */
    (void)event_types;
    (void)num_event_types;
    return h;
}

bool probe_destroy(probe_registry_t* reg, probe_handle_t handle) {
    probe_t* p = find_probe(reg, handle);
    if (!p) return false;

    /* Clear stage subscriber bits BEFORE deactivating (Bug 2 fix) */
    uint32_t probe_idx = (uint32_t)(p - reg->probes);
    if (probe_idx < 32) {
        for (uint32_t s = 0; s < PROBE_STAGE_COUNT; s++) {
            reg->stage_subscribers[s] &= ~(1u << probe_idx);
        }
    }

    p->active = false;
    NIMCP_LOGGING_INFO("Probe destroyed: \"%s\" (handle=%u)", p->name, handle);
    return true;
}

/* ============================================================================
 * Query
 * ============================================================================ */

uint32_t probe_get_metrics(
    probe_registry_t* reg,
    probe_handle_t handle,
    probe_metric_t* out_metrics,
    uint32_t max_metrics)
{
    if (!out_metrics || max_metrics == 0) return 0;

    probe_t* p = find_probe(reg, handle);
    if (!p) return 0;

    /* Read from the inactive (stable) buffer — lock-free */
    uint32_t active = __atomic_load_n(&p->active_buf, __ATOMIC_ACQUIRE);
    uint32_t read_buf = 1 - active;
    uint32_t count = p->metric_count[read_buf];
    if (count > max_metrics) count = max_metrics;

    memcpy(out_metrics, p->metrics[read_buf], count * sizeof(probe_metric_t));
    return count;
}

char* probe_get_all_metrics_json(probe_registry_t* reg) {
    if (!reg) return NULL;

    /* Estimate buffer size: ~200 bytes per metric, 64 metrics per probe, 32 probes max
     * Worst case: 32 * 64 * 200 = 409,600. Allocate 512 KB. */
    size_t buf_size = 512 * 1024;
    char* buf = (char*)nimcp_malloc(buf_size);
    if (!buf) return NULL;

    size_t pos = 0;
    pos += snprintf(buf + pos, buf_size - pos, "{");

    bool first_probe = true;
    for (uint32_t i = 0; i < reg->count; i++) {
        probe_t* p = &reg->probes[i];
        if (!p->active) continue;

        if (!first_probe) pos += snprintf(buf + pos, buf_size - pos, ",");
        first_probe = false;

        pos += snprintf(buf + pos, buf_size - pos, "\"%s\":{", p->name);

        uint32_t active = __atomic_load_n(&p->active_buf, __ATOMIC_ACQUIRE);
        uint32_t read_buf = 1 - active;
        uint32_t count = p->metric_count[read_buf];

        /* Reserve space for largest possible metric entry:
         * key (63) + value (63) + quotes/colon/comma (~10) ≈ 136 bytes */
        #define PROBE_JSON_ENTRY_MAX 200
        for (uint32_t j = 0; j < count && pos < buf_size - PROBE_JSON_ENTRY_MAX; j++) {
            probe_metric_t* m = &p->metrics[read_buf][j];
            if (j > 0) pos += snprintf(buf + pos, buf_size - pos, ",");

            switch (m->type) {
                case PROBE_METRIC_FLOAT:
                    pos += snprintf(buf + pos, buf_size - pos,
                                   "\"%s\":%.6f", m->key, m->value.f);
                    break;
                case PROBE_METRIC_INT:
                    pos += snprintf(buf + pos, buf_size - pos,
                                   "\"%s\":%lld", m->key, (long long)m->value.i);
                    break;
                case PROBE_METRIC_STRING:
                    pos += snprintf(buf + pos, buf_size - pos,
                                   "\"%s\":\"%s\"", m->key, m->value.s);
                    break;
            }
        }

        pos += snprintf(buf + pos, buf_size - pos, "}");
    }

    pos += snprintf(buf + pos, buf_size - pos, "}");
    return buf;
}

/* ============================================================================
 * Pipeline Stage Recording
 * ============================================================================ */

void probe_registry_record_stage(struct probe_registry* reg,
                                  const probe_stage_context_t* ctx)
{
    if (!reg || !ctx) return;
    if (ctx->stage_id >= PROBE_STAGE_COUNT) return;

    uint32_t subscribers = reg->stage_subscribers[ctx->stage_id];
    if (subscribers == 0) return;

    /* Distribute stage metrics to all subscribed probes */
    for (uint32_t i = 0; i < reg->count && subscribers != 0; i++) {
        if (!(subscribers & (1u << i))) continue;
        subscribers &= ~(1u << i);

        probe_t* p = &reg->probes[i];
        if (!p->active) continue;

        /* Write stage metrics into probe's write buffer */
        uint32_t read_buf = __atomic_load_n(&p->active_buf, __ATOMIC_ACQUIRE);
        uint32_t write_buf = 1 - read_buf;

        /* Append stage metrics (don't overwrite — accumulate from multiple stages) */
        uint32_t base = p->metric_count[write_buf];
        const char* stage_name = probe_stage_name(ctx->stage_id);

        for (uint32_t j = 0; j < ctx->metric_count && base + j < PROBE_MAX_METRICS; j++) {
            probe_metric_t* dst = &p->metrics[write_buf][base + j];
            /* Copy from stage metric (identical layout) */
            memcpy(dst->key, ctx->metrics[j].key, sizeof(dst->key));
            dst->type = (probe_metric_type_t)ctx->metrics[j].type;
            memcpy(&dst->value, &ctx->metrics[j].value, sizeof(dst->value));
            dst->timestamp_us = ctx->metrics[j].timestamp_us;

            /* Prefix with stage name */
            if (stage_name) {
                char prefixed[PROBE_KEY_LEN];
                snprintf(prefixed, sizeof(prefixed), "%s.%s", stage_name, dst->key);
                strncpy(dst->key, prefixed, PROBE_KEY_LEN - 1);
                dst->key[PROBE_KEY_LEN - 1] = '\0';
            }
        }

        uint32_t added = ctx->metric_count;
        if (base + added > PROBE_MAX_METRICS) added = PROBE_MAX_METRICS - base;
        p->metric_count[write_buf] = base + added;

        /* Flip buffer after last stage in a cycle completes.
         * For simplicity, flip on every stage record. Readers get
         * the most recent complete stage's data. */
        __atomic_store_n(&p->active_buf, write_buf, __ATOMIC_RELEASE);
    }
}

/* ============================================================================
 * Stage Name Lookup
 * ============================================================================ */

const char* probe_stage_name(probe_stage_id_t id) {
    switch (id) {
        case PROBE_INF_PRE_FORWARD:     return "inf_pre_forward";
        case PROBE_INF_ADAPTIVE_FWD:    return "inf_adaptive_fwd";
        case PROBE_INF_CNN_FWD:         return "inf_cnn_fwd";
        case PROBE_INF_SNN_FWD:         return "inf_snn_fwd";
        case PROBE_INF_LNN_GATE:        return "inf_lnn_gate";
        case PROBE_INF_PREDICTION_ERR:  return "inf_prediction_err";
        case PROBE_INF_REASONING:       return "inf_reasoning";
        case PROBE_INF_EMOTION:         return "inf_emotion";
        case PROBE_INF_ETHICS:          return "inf_ethics";
        case PROBE_INF_WORLD_PRIOR:     return "inf_world_prior";
        case PROBE_INF_DECISION:        return "inf_decision";
        case PROBE_INF_COGNITIVE:       return "inf_cognitive";
        case PROBE_TRAIN_INPUT:         return "train_input";
        case PROBE_TRAIN_ADAPTIVE:      return "train_adaptive";
        case PROBE_TRAIN_BPTT:          return "train_bptt";
        case PROBE_TRAIN_UTM:           return "train_utm";
        case PROBE_TRAIN_SNN:           return "train_snn";
        case PROBE_TRAIN_LNN:           return "train_lnn";
        case PROBE_TRAIN_PLASTICITY:    return "train_plasticity";
        case PROBE_TRAIN_COGNITIVE:     return "train_cognitive";
        case PROBE_TRAIN_ENGRAM:        return "train_engram";
        case PROBE_TRAIN_WM_CURRICULUM: return "train_wm_curriculum";
        default:                        return "unknown";
    }
}
