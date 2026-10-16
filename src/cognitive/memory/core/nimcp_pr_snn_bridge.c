//=============================================================================
// nimcp_pr_snn_bridge.c - SNN Bridge for Prime Resonant Memory System
//=============================================================================
/**
 * @file nimcp_pr_snn_bridge.c
 * @brief Implementation of PR Memory <-> SNN bidirectional encoding bridge
 *
 * WHAT: Implements quaternion state <-> spike pattern encoding/decoding
 * WHY:  Enable biologically realistic memory representation through spikes
 * HOW:  Maps quaternion components to different spike coding schemes
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "constants/nimcp_buffer_constants.h"
#include "cognitive/memory/core/nimcp_pr_snn_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include "glial/myelin_sheath/nimcp_myelin_math.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <float.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include <stdio.h>   /* for printf */
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(pr_snn_bridge, MESH_ADAPTER_CATEGORY_MEMORY)


/** @brief Send heartbeat from pr_snn_bridge module (instance-level) */
static inline void pr_snn_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_pr_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_pr_snn_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_pr_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "PR_SNN_BRIDGE"

/* Security subsystem setters (Phase 1: Audit Gap Remediation) */
//=============================================================================
// Platform Abstraction
//=============================================================================

/* Simple mutex abstraction - in production would use nimcp_mutex */
#ifdef _WIN32
    #include <windows.h>
    typedef CRITICAL_SECTION pr_mutex_internal_t;
    #define PR_MUTEX_INIT(m) InitializeCriticalSection(&(m))
    #define PR_MUTEX_DESTROY(m) DeleteCriticalSection(&(m))
    #define PR_MUTEX_LOCK(m) EnterCriticalSection(&(m))
    #define PR_MUTEX_UNLOCK(m) LeaveCriticalSection(&(m))
#else
    #include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
    typedef nimcp_mutex_t pr_mutex_internal_t;
    /* PR_MUTEX macros for embedded (non-pointer) mutex fields */
    #define PR_MUTEX_INIT(m) nimcp_mutex_init(&(m), NULL)
    #define PR_MUTEX_DESTROY(m) nimcp_mutex_destroy(&(m))
    #define PR_MUTEX_LOCK(m) nimcp_mutex_lock(&(m))
    #define PR_MUTEX_UNLOCK(m) nimcp_mutex_unlock(&(m))
    /* PR_MUTEX_PTR macros for pointer mutex fields (nimcp_mutex_t*) */
    #define PR_MUTEX_PTR_INIT(m) nimcp_mutex_init((m), NULL)
    #define PR_MUTEX_PTR_DESTROY(m) nimcp_mutex_destroy((m))
    #define PR_MUTEX_PTR_LOCK(m) nimcp_mutex_lock((m))
    #define PR_MUTEX_PTR_UNLOCK(m) nimcp_mutex_unlock((m))
#endif

/* High-resolution timing */
#ifdef _WIN32
static uint64_t get_time_us(void) {
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (uint64_t)((count.QuadPart * 1000000) / freq.QuadPart);
}
#else
static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000ULL);
}
#endif

//=============================================================================
// Thread-Local Error Handling
//=============================================================================

static _Thread_local char g_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* msg) {
    if (msg) {
        strncpy(g_last_error, msg, sizeof(g_last_error) - 1);
        g_last_error[sizeof(g_last_error) - 1] = '\0';
    }
}

//=============================================================================
// Internal Data Structures
//=============================================================================

/**
 * @brief Internal bridge structure
 *
 * Contains all state for the SNN bridge including:
 * - Configuration parameters
 * - Working buffers
 * - Statistics
 * - Synchronization primitives
 * - RNG state
 */
struct pr_snn_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* Configuration */
    pr_snn_bridge_config_t config;
    pr_snn_burst_params_t burst_params;

    /* Working buffers */
    float* rate_buffer;           /* Firing rates per neuron */
    float* latency_buffer;        /* First spike times per neuron */
    uint8_t* active_mask;         /* Which neurons are active */
    float* isi_buffer;            /* Inter-spike intervals */
    size_t buffer_capacity;       /* Allocated buffer size */

    /* Statistics */
    pr_snn_bridge_stats_t stats;
    pr_snn_encode_stats_t last_encode_stats;

    /* RNG state (xorshift128+) */
    uint64_t rng_state[2];

    /* Thread safety */
    pr_mutex_internal_t mutex;
    bool mutex_initialized;

    /* State */
    bool initialized;

    /** Instance-level health agent (B25 Upgrade) */
    nimcp_health_agent_t* health_agent;
};

BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(pr_snn_bridge, struct pr_snn_bridge_struct)

//=============================================================================
// Random Number Generation (xorshift128+)
//=============================================================================

/**
 * @brief Initialize RNG with seed
 */
static void rng_seed(pr_snn_bridge_t bridge, uint64_t seed) {
    bridge->rng_state[0] = seed;
    bridge->rng_state[1] = seed ^ 0x5A5A5A5A5A5A5A5AULL;
    /* Warm up */
    for (int i = 0; i < 20; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 20 > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(i + 1) / (float)20);
        }

        uint64_t s1 = bridge->rng_state[0];
        uint64_t s0 = bridge->rng_state[1];
        bridge->rng_state[0] = s0;
        s1 ^= s1 << 23;
        bridge->rng_state[1] = s1 ^ s0 ^ (s1 >> 18) ^ (s0 >> 5);
    }
}

/**
 * @brief Generate random uint64
 */
static uint64_t rng_next(pr_snn_bridge_t bridge) {
    uint64_t s1 = bridge->rng_state[0];
    uint64_t s0 = bridge->rng_state[1];
    uint64_t result = s0 + s1;
    bridge->rng_state[0] = s0;
    s1 ^= s1 << 23;
    bridge->rng_state[1] = s1 ^ s0 ^ (s1 >> 18) ^ (s0 >> 5);
    return result;
}

/**
 * @brief Generate random float in [0, 1)
 */
static float rng_uniform(pr_snn_bridge_t bridge) {
    uint64_t x = rng_next(bridge);
    return (float)(x >> 11) * (1.0f / 9007199254740992.0f);
}

/**
 * @brief Generate Gaussian random with Box-Muller
 */
static float rng_gaussian(pr_snn_bridge_t bridge, float mean, float stddev) {
    float u1 = rng_uniform(bridge);
    float u2 = rng_uniform(bridge);
    if (u1 < 1e-10f) u1 = 1e-10f;
    float z0 = sqrtf(-2.0f * logf(u1)) * cosf(NIMCP_TWO_PI_F * u2);
    return mean + stddev * z0;
}

/**
 * @brief Generate exponential random (for Poisson process)
 */
static float rng_exponential(pr_snn_bridge_t bridge, float rate) {
    if (rate <= 0.0f) return FLT_MAX;
    float u = rng_uniform(bridge);
    if (u < 1e-10f) u = 1e-10f;
    return -logf(u) / rate;
}

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Get current time in milliseconds
 */
static uint64_t get_time_ms(void) {
    return get_time_us() / 1000ULL;
}

/**
 * @brief Compare function for qsort (float)
 */
static int compare_floats(const void* a, const void* b) {
    float fa = *(const float*)a;
    float fb = *(const float*)b;
    if (fa < fb) return -1;
    if (fa > fb) return 1;
    return 0;
}

/**
 * @brief Spike comparison for sorting by time
 */
typedef struct {
    float time;
    uint32_t neuron_id;
} spike_entry_t;

static int compare_spikes(const void* a, const void* b) {
    const spike_entry_t* sa = (const spike_entry_t*)a;
    const spike_entry_t* sb = (const spike_entry_t*)b;
    if (sa->time < sb->time) return -1;
    if (sa->time > sb->time) return 1;
    return 0;
}

/**
 * @brief Add noise to value
 */
static float add_noise(pr_snn_bridge_t bridge, float value, float noise_level) {
    if (!bridge->config.enable_noise || noise_level <= 0.0f) {
        return value;
    }
    float noise = rng_gaussian(bridge, 0.0f, noise_level);
    return value + noise;
}

//=============================================================================
// Configuration Functions
//=============================================================================

NIMCP_EXPORT pr_snn_bridge_config_t pr_snn_bridge_config_default(void) {
    pr_snn_bridge_config_t config;
    memset(&config, 0, sizeof(config));

    config.population_size = PR_SNN_DEFAULT_POPULATION_SIZE;
    config.simulation_dt_ms = PR_SNN_DEFAULT_DT_MS;
    config.max_rate_hz = PR_SNN_DEFAULT_MAX_RATE_HZ;
    config.min_latency_ms = PR_SNN_DEFAULT_MIN_LATENCY_MS;
    config.max_latency_ms = PR_SNN_DEFAULT_MAX_LATENCY_MS;
    config.burst_threshold_ms = PR_SNN_DEFAULT_BURST_THRESHOLD_MS;
    config.encoding_window_ms = PR_SNN_DEFAULT_ENCODING_WINDOW_MS;
    config.enable_noise = true;
    config.noise_level = PR_SNN_DEFAULT_NOISE_LEVEL;
    config.enable_phase_coding = false;
    config.theta_frequency_hz = 8.0f;  /* Theta oscillation ~8 Hz */
    config.track_statistics = true;

    return config;
}

NIMCP_EXPORT bool pr_snn_bridge_config_validate(const pr_snn_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_snn_bridge_config_validate: config is NULL");
        return false;
    }

    /* Population size */
    if (config->population_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_snn_bridge_config_validate: config->population_size is zero");
        return false;
    }
    if (config->population_size > PR_SNN_MAX_NEURONS_PER_POP) {
        return false;
    }

    /* Timing */
    if (config->simulation_dt_ms <= 0.0f) {
        return false;
    }
    if (config->max_rate_hz <= 0.0f || config->max_rate_hz > 1000.0f) {
        return false;
    }

    /* Latency */
    if (config->min_latency_ms < 0.0f) {
        return false;
    }
    if (config->max_latency_ms <= config->min_latency_ms) {
        return false;
    }

    /* Noise */
    if (config->noise_level < 0.0f || config->noise_level > 1.0f) {
        return false;
    }

    /* Encoding window */
    if (config->encoding_window_ms <= 0.0f) {
        return false;
    }

    return true;
}

NIMCP_EXPORT pr_snn_burst_params_t pr_snn_burst_params_default(void) {
    pr_snn_burst_params_t params;
    params.positive_spikes = 5;        /* 4-6 spikes for positive */
    params.positive_isi_ms = 5.0f;     /* 5 ms ISI for regular bursts */
    params.negative_spikes = 2;        /* 2-3 spikes for negative */
    params.negative_isi_ms = 20.0f;    /* 20 ms ISI for irregular */
    params.burst_probability = 0.7f;   /* 70% chance of bursting */
    return params;
}

//=============================================================================
// Bridge Lifecycle Functions
//=============================================================================

NIMCP_EXPORT pr_snn_bridge_t pr_snn_bridge_create(const pr_snn_bridge_config_t* config) {
    /* Use default config if not provided */
    pr_snn_bridge_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        cfg = pr_snn_bridge_config_default();
    }

    /* Validate */
    if (!pr_snn_bridge_config_validate(&cfg)) {
        set_error("Invalid configuration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_snn_bridge_create: pr_snn_bridge_config_validate is NULL");
        return NULL;
    }

    /* Allocate bridge */
    pr_snn_bridge_t bridge = (pr_snn_bridge_t)nimcp_calloc(1, sizeof(struct pr_snn_bridge_struct));
    if (!bridge) {
        set_error("Memory allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_snn_bridge_create: bridge is NULL");
        return NULL;
    }

    /* Store configuration */
    bridge->config = cfg;
    bridge->burst_params = pr_snn_burst_params_default();

    /* Allocate working buffers */
    bridge->buffer_capacity = cfg.population_size;

    bridge->rate_buffer = (float*)nimcp_calloc(bridge->buffer_capacity, sizeof(float));
    bridge->latency_buffer = (float*)nimcp_calloc(bridge->buffer_capacity, sizeof(float));
    bridge->active_mask = (uint8_t*)nimcp_calloc(bridge->buffer_capacity, sizeof(uint8_t));
    bridge->isi_buffer = (float*)nimcp_calloc(PR_SNN_MAX_SPIKES_PER_PATTERN, sizeof(float));

    if (!bridge->rate_buffer || !bridge->latency_buffer ||
        !bridge->active_mask || !bridge->isi_buffer) {
        set_error("Buffer allocation failed");
        pr_snn_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_snn_bridge_create: buffer allocation failed");
        return NULL;
    }

    /* Initialize mutex — base.mutex is a pointer, allocate+init */
    bridge->base.mutex = nimcp_mutex_create(NULL);
    bridge->mutex_initialized = (bridge->base.mutex != NULL);

    /* Initialize RNG with time-based seed */
    rng_seed(bridge, (uint64_t)time(NULL) ^ get_time_us());

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.last_reset_time_ms = get_time_ms();

    bridge->initialized = true;

    NIMCP_LOGGING_INFO("Created %s bridge", "pr_snn");
    return bridge;
}

NIMCP_EXPORT void pr_snn_bridge_destroy(pr_snn_bridge_t bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "pr_snn");

    /* Free buffers */
    nimcp_free(bridge->rate_buffer);
    nimcp_free(bridge->latency_buffer);
    nimcp_free(bridge->active_mask);
    nimcp_free(bridge->isi_buffer);

    /* Destroy mutex — base.mutex was heap-allocated via nimcp_mutex_create */
    if (bridge->mutex_initialized && bridge->base.mutex) {
        nimcp_mutex_free(bridge->base.mutex);
        bridge->base.mutex = NULL;
    }

    /* Free bridge */
    nimcp_free(bridge);
    bridge = NULL;
}

NIMCP_EXPORT pr_snn_error_t pr_snn_bridge_reset(pr_snn_bridge_t bridge) {
    if (!bridge) return PR_SNN_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_SNN_ERROR_INVALID_STATE;

    PR_MUTEX_PTR_LOCK(bridge->base.mutex);

    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.last_reset_time_ms = get_time_ms();

    PR_MUTEX_PTR_UNLOCK(bridge->base.mutex);

    return PR_SNN_SUCCESS;
}

NIMCP_EXPORT pr_snn_error_t pr_snn_bridge_get_config(
    const pr_snn_bridge_t bridge,
    pr_snn_bridge_config_t* config)
{
    if (!bridge || !config) return PR_SNN_ERROR_NULL_POINTER;
    *config = bridge->config;
    return PR_SNN_SUCCESS;
}

NIMCP_EXPORT pr_snn_error_t pr_snn_bridge_set_config(
    pr_snn_bridge_t bridge,
    const pr_snn_bridge_config_t* config)
{
    if (!bridge || !config) return PR_SNN_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_SNN_ERROR_INVALID_STATE;

    /* Validate new config */
    if (!pr_snn_bridge_config_validate(config)) {
        set_error("Invalid configuration");
        return PR_SNN_ERROR_INVALID_CONFIG;
    }

    /* population_size cannot be changed (would require buffer reallocation) */
    if (config->population_size != bridge->config.population_size) {
        set_error("Cannot change population_size after creation");
        return PR_SNN_ERROR_INVALID_CONFIG;
    }

    PR_MUTEX_PTR_LOCK(bridge->base.mutex);
    bridge->config = *config;
    PR_MUTEX_PTR_UNLOCK(bridge->base.mutex);

    return PR_SNN_SUCCESS;
}

//=============================================================================
// Pattern Operations
//=============================================================================

NIMCP_EXPORT pr_spike_pattern_t* pr_spike_pattern_create(
    size_t num_neurons,
    float duration_ms)
{
    /* Estimate capacity based on expected rate */
    size_t expected_spikes = (size_t)(num_neurons * duration_ms * 0.1f);  /* ~100Hz max */
    if (expected_spikes < 64) expected_spikes = 64;
    if (expected_spikes > PR_SNN_MAX_SPIKES_PER_PATTERN) {
        expected_spikes = PR_SNN_MAX_SPIKES_PER_PATTERN;
    }

    return pr_spike_pattern_create_with_capacity(num_neurons, duration_ms, expected_spikes);
}

NIMCP_EXPORT pr_spike_pattern_t* pr_spike_pattern_create_with_capacity(
    size_t num_neurons,
    float duration_ms,
    size_t capacity)
{
    if (num_neurons == 0 || duration_ms <= 0.0f) {
        set_error("Invalid pattern parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_spike_pattern_create_with_capacity: num_neurons is zero");
        return NULL;
    }

    if (capacity > PR_SNN_MAX_SPIKES_PER_PATTERN) {
        capacity = PR_SNN_MAX_SPIKES_PER_PATTERN;
    }

    pr_spike_pattern_t* pattern = (pr_spike_pattern_t*)nimcp_calloc(1, sizeof(pr_spike_pattern_t));
    if (!pattern) {
        set_error("Pattern allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_spike_pattern_create_with_capacity: pattern is NULL");
        return NULL;
    }

    pattern->spike_times = (float*)nimcp_calloc(capacity, sizeof(float));
    pattern->neuron_ids = (uint32_t*)nimcp_calloc(capacity, sizeof(uint32_t));

    if (!pattern->spike_times || !pattern->neuron_ids) {
        set_error("Pattern buffer allocation failed");
        pr_spike_pattern_destroy(pattern);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_spike_pattern_create_with_capacity: required parameter is NULL (pattern->spike_times, pattern->neuron_ids)");
        return NULL;
    }

    pattern->num_spikes = 0;
    pattern->capacity = capacity;
    pattern->duration_ms = duration_ms;
    pattern->num_neurons = num_neurons;

    return pattern;
}

NIMCP_EXPORT void pr_spike_pattern_destroy(pr_spike_pattern_t* pattern) {
    if (!pattern) return;
    nimcp_free(pattern->spike_times);
    nimcp_free(pattern->neuron_ids);
    nimcp_free(pattern);
    pattern = NULL;
}

NIMCP_EXPORT pr_spike_pattern_t* pr_spike_pattern_copy(const pr_spike_pattern_t* pattern) {
    if (!pattern) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern is NULL");

        return NULL;

    }

    pr_spike_pattern_t* copy = pr_spike_pattern_create_with_capacity(
        pattern->num_neurons, pattern->duration_ms, pattern->capacity);
    if (!copy) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "copy is NULL");

        return NULL;

    }

    memcpy(copy->spike_times, pattern->spike_times, pattern->num_spikes * sizeof(float));
    memcpy(copy->neuron_ids, pattern->neuron_ids, pattern->num_spikes * sizeof(uint32_t));
    copy->num_spikes = pattern->num_spikes;

    return copy;
}

NIMCP_EXPORT pr_snn_error_t pr_spike_pattern_clear(pr_spike_pattern_t* pattern) {
    if (!pattern) return PR_SNN_ERROR_NULL_POINTER;
    pattern->num_spikes = 0;
    return PR_SNN_SUCCESS;
}

NIMCP_EXPORT pr_snn_error_t pr_spike_pattern_add_spike(
    pr_spike_pattern_t* pattern,
    float time_ms,
    uint32_t neuron_id)
{
    if (!pattern) return PR_SNN_ERROR_NULL_POINTER;

    /* Grow if needed */
    if (pattern->num_spikes >= pattern->capacity) {
        size_t new_capacity = pattern->capacity * 2;
        if (new_capacity > PR_SNN_MAX_SPIKES_PER_PATTERN) {
            new_capacity = PR_SNN_MAX_SPIKES_PER_PATTERN;
        }
        if (new_capacity <= pattern->capacity) {
            return PR_SNN_ERROR_PATTERN_FULL;
        }

        float* new_times = (float*)nimcp_realloc(pattern->spike_times, new_capacity * sizeof(float));
        if (!new_times) {
            return PR_SNN_ERROR_NO_MEMORY;
        }
        pattern->spike_times = new_times;

        uint32_t* new_ids = (uint32_t*)nimcp_realloc(pattern->neuron_ids, new_capacity * sizeof(uint32_t));
        if (!new_ids) {
            return PR_SNN_ERROR_NO_MEMORY;
        }
        pattern->neuron_ids = new_ids;
        pattern->capacity = new_capacity;
    }

    pattern->spike_times[pattern->num_spikes] = time_ms;
    pattern->neuron_ids[pattern->num_spikes] = neuron_id;
    pattern->num_spikes++;

    return PR_SNN_SUCCESS;
}

NIMCP_EXPORT pr_spike_pattern_t* pr_spike_pattern_merge(
    const pr_spike_pattern_t* const* patterns,
    size_t count)
{
    if (!patterns || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_spike_pattern_merge: patterns is NULL");
        return NULL;
    }

    /* Count total spikes and find max params */
    size_t total_spikes = 0;
    size_t max_neurons = 0;
    float max_duration = 0.0f;

    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(i + 1) / (float)count);
        }

        if (!patterns[i]) continue;
        total_spikes += patterns[i]->num_spikes;
        if (patterns[i]->num_neurons > max_neurons) {
            max_neurons = patterns[i]->num_neurons;
        }
        if (patterns[i]->duration_ms > max_duration) {
            max_duration = patterns[i]->duration_ms;
        }
    }

    if (total_spikes == 0) {
        return pr_spike_pattern_create(max_neurons > 0 ? max_neurons : 64,
                                       max_duration > 0 ? max_duration : 100.0f);
    }

    /* Create merged pattern */
    pr_spike_pattern_t* merged = pr_spike_pattern_create_with_capacity(
        max_neurons, max_duration, total_spikes);
    if (!merged) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "merged is NULL");

        return NULL;

    }

    /* Copy all spikes */
    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(i + 1) / (float)count);
        }

        if (!patterns[i]) continue;
        for (size_t j = 0; j < patterns[i]->num_spikes; j++) {
            merged->spike_times[merged->num_spikes] = patterns[i]->spike_times[j];
            merged->neuron_ids[merged->num_spikes] = patterns[i]->neuron_ids[j];
            merged->num_spikes++;
        }
    }

    /* Sort by time */
    pr_spike_pattern_sort(merged);

    return merged;
}

NIMCP_EXPORT pr_snn_error_t pr_spike_pattern_sort(pr_spike_pattern_t* pattern) {
    if (!pattern) return PR_SNN_ERROR_NULL_POINTER;
    if (pattern->num_spikes <= 1) return PR_SNN_SUCCESS;

    /* Create temporary array for sorting */
    spike_entry_t* entries = (spike_entry_t*)nimcp_calloc(pattern->num_spikes, sizeof(spike_entry_t));
    if (!entries) return PR_SNN_ERROR_NO_MEMORY;

    for (size_t i = 0; i < pattern->num_spikes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pattern->num_spikes > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(i + 1) / (float)pattern->num_spikes);
        }

        entries[i].time = pattern->spike_times[i];
        entries[i].neuron_id = pattern->neuron_ids[i];
    }

    qsort(entries, pattern->num_spikes, sizeof(spike_entry_t), compare_spikes);

    for (size_t i = 0; i < pattern->num_spikes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pattern->num_spikes > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(i + 1) / (float)pattern->num_spikes);
        }

        pattern->spike_times[i] = entries[i].time;
        pattern->neuron_ids[i] = entries[i].neuron_id;
    }

    nimcp_free(entries);
    entries = NULL;
    return PR_SNN_SUCCESS;
}

NIMCP_EXPORT pr_spike_pattern_t* pr_spike_pattern_extract_neurons(
    const pr_spike_pattern_t* pattern,
    uint32_t neuron_start,
    uint32_t neuron_end)
{
    if (!pattern) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern is NULL");

        return NULL;

    }

    /* Count spikes in range */
    size_t count = 0;
    for (size_t i = 0; i < pattern->num_spikes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pattern->num_spikes > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(i + 1) / (float)pattern->num_spikes);
        }

        if (pattern->neuron_ids[i] >= neuron_start &&
            pattern->neuron_ids[i] < neuron_end) {
            count++;
        }
    }

    pr_spike_pattern_t* extracted = pr_spike_pattern_create_with_capacity(
        neuron_end - neuron_start, pattern->duration_ms, count > 0 ? count : 1);
    if (!extracted) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "extracted is NULL");

        return NULL;

    }

    for (size_t i = 0; i < pattern->num_spikes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pattern->num_spikes > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(i + 1) / (float)pattern->num_spikes);
        }

        if (pattern->neuron_ids[i] >= neuron_start &&
            pattern->neuron_ids[i] < neuron_end) {
            pr_spike_pattern_add_spike(extracted,
                pattern->spike_times[i],
                pattern->neuron_ids[i] - neuron_start);
        }
    }

    return extracted;
}

NIMCP_EXPORT pr_spike_pattern_t* pr_spike_pattern_extract_time(
    const pr_spike_pattern_t* pattern,
    float time_start,
    float time_end)
{
    if (!pattern) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern is NULL");

        return NULL;

    }

    /* Count spikes in range */
    size_t count = 0;
    for (size_t i = 0; i < pattern->num_spikes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pattern->num_spikes > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(i + 1) / (float)pattern->num_spikes);
        }

        if (pattern->spike_times[i] >= time_start &&
            pattern->spike_times[i] < time_end) {
            count++;
        }
    }

    pr_spike_pattern_t* extracted = pr_spike_pattern_create_with_capacity(
        pattern->num_neurons, time_end - time_start, count > 0 ? count : 1);
    if (!extracted) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "extracted is NULL");

        return NULL;

    }

    for (size_t i = 0; i < pattern->num_spikes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pattern->num_spikes > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(i + 1) / (float)pattern->num_spikes);
        }

        if (pattern->spike_times[i] >= time_start &&
            pattern->spike_times[i] < time_end) {
            pr_spike_pattern_add_spike(extracted,
                pattern->spike_times[i] - time_start,
                pattern->neuron_ids[i]);
        }
    }

    return extracted;
}

//=============================================================================
// Encoding Functions - Rate Coding
//=============================================================================

NIMCP_EXPORT pr_snn_error_t pr_snn_encode_rate(
    pr_snn_bridge_t bridge,
    float value,
    pr_spike_pattern_t* pattern)
{
    if (!bridge || !pattern) return PR_SNN_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_SNN_ERROR_INVALID_STATE;

    uint64_t start_time = get_time_us();

    /* Clamp value to [0, 1] */
    value = nimcp_myelin_clamp(value, 0.0f, 1.0f);

    /* Add noise if enabled */
    value = add_noise(bridge, value, bridge->config.noise_level * 0.5f);
    value = nimcp_myelin_clamp(value, 0.0f, 1.0f);

    /* Calculate target rate */
    float rate_hz = value * bridge->config.max_rate_hz;
    float rate_per_ms = rate_hz / 1000.0f;

    /* Clear pattern */
    pr_spike_pattern_clear(pattern);

    /* Generate Poisson spikes for each neuron */
    size_t pop_size = bridge->config.population_size;
    float duration = bridge->config.encoding_window_ms;

    for (size_t n = 0; n < pop_size; n++) {
        /* Phase 8: Loop progress heartbeat */
        if ((n & 0xFF) == 0 && pop_size > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(n + 1) / (float)pop_size);
        }

        float t = 0.0f;
        while (t < duration) {
            /* Inter-spike interval from exponential distribution */
            float isi = rng_exponential(bridge, rate_per_ms);
            t += isi;

            if (t < duration) {
                pr_snn_error_t err = pr_spike_pattern_add_spike(pattern, t, (uint32_t)n);
                if (err != PR_SNN_SUCCESS) break;
            }
        }
    }

    /* Update statistics */
    if (bridge->config.track_statistics) {
        PR_MUTEX_PTR_LOCK(bridge->base.mutex);
        bridge->stats.total_encodings++;
        bridge->stats.avg_spike_count =
            (bridge->stats.avg_spike_count * (bridge->stats.total_encodings - 1) +
             pattern->num_spikes) / bridge->stats.total_encodings;
        bridge->stats.avg_encoding_time_us =
            (bridge->stats.avg_encoding_time_us * (bridge->stats.total_encodings - 1) +
             (get_time_us() - start_time)) / bridge->stats.total_encodings;
        if (pattern->num_spikes > bridge->stats.peak_pattern_size) {
            bridge->stats.peak_pattern_size = pattern->num_spikes;
        }

        bridge->last_encode_stats.input_value = value;
        bridge->last_encode_stats.encoding = PR_SNN_ENCODE_RATE;
        bridge->last_encode_stats.spikes_generated = pattern->num_spikes;
        bridge->last_encode_stats.encoding_time_us = (float)(get_time_us() - start_time);
        bridge->last_encode_stats.mean_firing_rate_hz = rate_hz;
        PR_MUTEX_PTR_UNLOCK(bridge->base.mutex);
    }

    return PR_SNN_SUCCESS;
}

//=============================================================================
// Encoding Functions - Burst Coding
//=============================================================================

NIMCP_EXPORT pr_snn_error_t pr_snn_encode_burst(
    pr_snn_bridge_t bridge,
    float value,
    pr_spike_pattern_t* pattern)
{
    if (!bridge || !pattern) return PR_SNN_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_SNN_ERROR_INVALID_STATE;

    uint64_t start_time = get_time_us();

    /* Clamp value to [-1, 1] */
    value = nimcp_myelin_clamp(value, -1.0f, 1.0f);

    /* Add noise */
    value = add_noise(bridge, value, bridge->config.noise_level);
    value = nimcp_myelin_clamp(value, -1.0f, 1.0f);

    /* Clear pattern */
    pr_spike_pattern_clear(pattern);

    float duration = bridge->config.encoding_window_ms;
    size_t pop_size = bridge->config.population_size;
    pr_snn_burst_params_t* bp = &bridge->burst_params;

    /* Determine burst pattern based on valence */
    bool is_positive = value > 0.1f;
    bool is_negative = value < -0.1f;
    float abs_val = fabsf(value);

    /* Number of neurons that will burst */
    size_t bursting_neurons = (size_t)(pop_size * abs_val * bp->burst_probability);

    for (size_t n = 0; n < bursting_neurons; n++) {
        /* Phase 8: Loop progress heartbeat */
        if ((n & 0xFF) == 0 && bursting_neurons > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(n + 1) / (float)bursting_neurons);
        }

        /* Random burst onset */
        float onset = rng_uniform(bridge) * (duration * 0.5f);

        if (is_positive) {
            /* Regular burst: 4-6 spikes with 5ms ISI */
            uint32_t num_spikes = bp->positive_spikes;
            num_spikes += (uint32_t)(rng_uniform(bridge) * 3) - 1;  /* +/- 1 spike */
            if (num_spikes < 2) num_spikes = 2;

            for (uint32_t s = 0; s < num_spikes; s++) {
                /* Phase 8: Loop progress heartbeat */
                if ((s & 0xFF) == 0 && num_spikes > 256) {
                    pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                                     (float)(s + 1) / (float)num_spikes);
                }

                float t = onset + s * bp->positive_isi_ms;
                float jitter = rng_gaussian(bridge, 0.0f, 0.5f);
                t += jitter;
                if (t >= 0.0f && t < duration) {
                    pr_spike_pattern_add_spike(pattern, t, (uint32_t)n);
                }
            }
        } else if (is_negative) {
            /* Irregular burst: 2-3 spikes with variable ISI */
            uint32_t num_spikes = bp->negative_spikes;
            num_spikes += (uint32_t)(rng_uniform(bridge) * 2);
            if (num_spikes < 2) num_spikes = 2;

            float t = onset;
            for (uint32_t s = 0; s < num_spikes; s++) {
                /* Phase 8: Loop progress heartbeat */
                if ((s & 0xFF) == 0 && num_spikes > 256) {
                    pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                                     (float)(s + 1) / (float)num_spikes);
                }

                /* Highly variable ISI */
                float isi = bp->negative_isi_ms * (0.5f + rng_uniform(bridge));
                if (s > 0) t += isi;

                if (t >= 0.0f && t < duration) {
                    pr_spike_pattern_add_spike(pattern, t, (uint32_t)n);
                }
            }
        } else {
            /* Neutral: sparse random spikes, no bursts */
            if (rng_uniform(bridge) < 0.2f) {
                float t = rng_uniform(bridge) * duration;
                pr_spike_pattern_add_spike(pattern, t, (uint32_t)n);
            }
        }
    }

    /* Sort by time */
    pr_spike_pattern_sort(pattern);

    /* Update statistics */
    if (bridge->config.track_statistics) {
        PR_MUTEX_PTR_LOCK(bridge->base.mutex);
        bridge->stats.total_encodings++;
        bridge->last_encode_stats.input_value = value;
        bridge->last_encode_stats.encoding = PR_SNN_ENCODE_BURST;
        bridge->last_encode_stats.spikes_generated = pattern->num_spikes;
        bridge->last_encode_stats.encoding_time_us = (float)(get_time_us() - start_time);
        PR_MUTEX_PTR_UNLOCK(bridge->base.mutex);
    }

    return PR_SNN_SUCCESS;
}

//=============================================================================
// Encoding Functions - Population Coding
//=============================================================================

NIMCP_EXPORT pr_snn_error_t pr_snn_encode_population(
    pr_snn_bridge_t bridge,
    float value,
    pr_spike_pattern_t* pattern)
{
    if (!bridge || !pattern) return PR_SNN_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_SNN_ERROR_INVALID_STATE;

    uint64_t start_time = get_time_us();

    /* Clamp value to [0, 1] */
    value = nimcp_myelin_clamp(value, 0.0f, 1.0f);

    /* Add noise */
    value = add_noise(bridge, value, bridge->config.noise_level * 0.5f);
    value = nimcp_myelin_clamp(value, 0.0f, 1.0f);

    /* Clear pattern */
    pr_spike_pattern_clear(pattern);

    size_t pop_size = bridge->config.population_size;
    float duration = bridge->config.encoding_window_ms;

    /* Number of active neurons proportional to salience */
    size_t active_count = (size_t)(value * pop_size);
    if (active_count == 0 && value > 0.01f) active_count = 1;

    /* Select which neurons are active (random subset) */
    memset(bridge->active_mask, 0, pop_size);
    size_t selected = 0;
    while (selected < active_count) {
        size_t idx = (size_t)(rng_uniform(bridge) * pop_size);
        if (idx >= pop_size) idx = pop_size - 1;
        if (!bridge->active_mask[idx]) {
            bridge->active_mask[idx] = 1;
            selected++;
        }
    }

    /* Generate spikes for active neurons */
    float base_rate_hz = bridge->config.max_rate_hz * 0.5f;  /* Moderate rate */

    for (size_t n = 0; n < pop_size; n++) {
        /* Phase 8: Loop progress heartbeat */
        if ((n & 0xFF) == 0 && pop_size > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(n + 1) / (float)pop_size);
        }

        if (!bridge->active_mask[n]) continue;

        /* Generate spikes for this neuron */
        float rate_per_ms = base_rate_hz / 1000.0f;
        float t = 0.0f;

        while (t < duration) {
            float isi = rng_exponential(bridge, rate_per_ms);
            t += isi;
            if (t < duration) {
                pr_spike_pattern_add_spike(pattern, t, (uint32_t)n);
            }
        }
    }

    /* Sort by time */
    pr_spike_pattern_sort(pattern);

    /* Update statistics */
    if (bridge->config.track_statistics) {
        PR_MUTEX_PTR_LOCK(bridge->base.mutex);
        bridge->stats.total_encodings++;
        bridge->last_encode_stats.input_value = value;
        bridge->last_encode_stats.encoding = PR_SNN_ENCODE_POPULATION;
        bridge->last_encode_stats.spikes_generated = pattern->num_spikes;
        bridge->last_encode_stats.encoding_time_us = (float)(get_time_us() - start_time);
        PR_MUTEX_PTR_UNLOCK(bridge->base.mutex);
    }

    return PR_SNN_SUCCESS;
}

//=============================================================================
// Encoding Functions - Latency Coding
//=============================================================================

NIMCP_EXPORT pr_snn_error_t pr_snn_encode_latency(
    pr_snn_bridge_t bridge,
    float value,
    pr_spike_pattern_t* pattern)
{
    if (!bridge || !pattern) return PR_SNN_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_SNN_ERROR_INVALID_STATE;

    uint64_t start_time = get_time_us();

    /* Clamp value to [0, 1] */
    value = nimcp_myelin_clamp(value, 0.0f, 1.0f);

    /* Add noise */
    value = add_noise(bridge, value, bridge->config.noise_level * 0.3f);
    value = nimcp_myelin_clamp(value, 0.0f, 1.0f);

    /* Clear pattern */
    pr_spike_pattern_clear(pattern);

    /* Calculate target latency: higher value = shorter latency */
    float min_lat = bridge->config.min_latency_ms;
    float max_lat = bridge->config.max_latency_ms;
    float target_latency = max_lat - value * (max_lat - min_lat);

    size_t pop_size = bridge->config.population_size;

    /* Each neuron fires once at approximately the target latency */
    for (size_t n = 0; n < pop_size; n++) {
        /* Phase 8: Loop progress heartbeat */
        if ((n & 0xFF) == 0 && pop_size > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(n + 1) / (float)pop_size);
        }

        /* Add jitter */
        float jitter = rng_gaussian(bridge, 0.0f, target_latency * 0.1f);
        float t = target_latency + jitter;

        if (t < min_lat) t = min_lat;
        if (t > max_lat) t = max_lat;

        if (t < bridge->config.encoding_window_ms) {
            pr_spike_pattern_add_spike(pattern, t, (uint32_t)n);
        }
    }

    /* Sort by time */
    pr_spike_pattern_sort(pattern);

    /* Update statistics */
    if (bridge->config.track_statistics) {
        PR_MUTEX_PTR_LOCK(bridge->base.mutex);
        bridge->stats.total_encodings++;
        bridge->last_encode_stats.input_value = value;
        bridge->last_encode_stats.encoding = PR_SNN_ENCODE_LATENCY;
        bridge->last_encode_stats.spikes_generated = pattern->num_spikes;
        bridge->last_encode_stats.encoding_time_us = (float)(get_time_us() - start_time);
        bridge->last_encode_stats.first_spike_time_ms =
            pattern->num_spikes > 0 ? pattern->spike_times[0] : -1.0f;
        PR_MUTEX_PTR_UNLOCK(bridge->base.mutex);
    }

    return PR_SNN_SUCCESS;
}

//=============================================================================
// Encoding Functions - Phase Coding
//=============================================================================

NIMCP_EXPORT pr_snn_error_t pr_snn_encode_phase(
    pr_snn_bridge_t bridge,
    float value,
    float theta_phase,
    pr_spike_pattern_t* pattern)
{
    if (!bridge || !pattern) return PR_SNN_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_SNN_ERROR_INVALID_STATE;

    /* Clamp value to [0, 1] */
    value = nimcp_myelin_clamp(value, 0.0f, 1.0f);

    /* Clear pattern */
    pr_spike_pattern_clear(pattern);

    /* Map value to target phase */
    float target_phase = value * NIMCP_TWO_PI_F;

    /* Calculate theta period */
    float theta_period_ms = 1000.0f / bridge->config.theta_frequency_hz;
    float duration = bridge->config.encoding_window_ms;
    size_t pop_size = bridge->config.population_size;

    /* Generate phase-locked spikes */
    for (size_t n = 0; n < pop_size; n++) {
        /* Phase 8: Loop progress heartbeat */
        if ((n & 0xFF) == 0 && pop_size > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(n + 1) / (float)pop_size);
        }

        /* Calculate spike times aligned to theta phase */
        float t = 0.0f;
        float cycle_start = theta_phase / NIMCP_TWO_PI_F * theta_period_ms;

        while (t < duration) {
            /* Time within theta cycle for this spike */
            float spike_phase = target_phase + rng_gaussian(bridge, 0.0f, 0.3f);
            while (spike_phase < 0) spike_phase += NIMCP_TWO_PI_F;
            while (spike_phase >= NIMCP_TWO_PI_F) spike_phase -= NIMCP_TWO_PI_F;

            float spike_time = cycle_start + (spike_phase / NIMCP_TWO_PI_F) * theta_period_ms;

            if (spike_time >= 0.0f && spike_time < duration) {
                pr_spike_pattern_add_spike(pattern, spike_time, (uint32_t)n);
            }

            /* Move to next theta cycle */
            cycle_start += theta_period_ms;
            t = cycle_start;
        }
    }

    pr_spike_pattern_sort(pattern);

    return PR_SNN_SUCCESS;
}

//=============================================================================
// Encoding Functions - Component and Quaternion
//=============================================================================

NIMCP_EXPORT pr_snn_error_t pr_snn_encode_component(
    pr_snn_bridge_t bridge,
    float value,
    pr_snn_encoding_t encoding,
    pr_spike_pattern_t* pattern)
{
    if (!bridge || !pattern) return PR_SNN_ERROR_NULL_POINTER;

    switch (encoding) {
        case PR_SNN_ENCODE_RATE:
            return pr_snn_encode_rate(bridge, value, pattern);
        case PR_SNN_ENCODE_BURST:
            return pr_snn_encode_burst(bridge, value, pattern);
        case PR_SNN_ENCODE_POPULATION:
            return pr_snn_encode_population(bridge, value, pattern);
        case PR_SNN_ENCODE_LATENCY:
            return pr_snn_encode_latency(bridge, value, pattern);
        case PR_SNN_ENCODE_PHASE:
            return pr_snn_encode_phase(bridge, value, 0.0f, pattern);
        default:
            set_error("Invalid encoding type");
            return PR_SNN_ERROR_INVALID_ENCODING;
    }
}

NIMCP_EXPORT pr_snn_error_t pr_snn_encode_quaternion(
    pr_snn_bridge_t bridge,
    nimcp_quaternion_t quat,
    pr_spike_pattern_t* pattern)
{
    if (!bridge || !pattern) return PR_SNN_ERROR_NULL_POINTER;
    if (!bridge->initialized) return PR_SNN_ERROR_INVALID_STATE;

    uint64_t start_time = get_time_us();

    /* Create temporary patterns for each component */
    size_t pop_size = bridge->config.population_size;
    float duration = bridge->config.encoding_window_ms;

    pr_spike_pattern_t* w_pattern = pr_spike_pattern_create(pop_size, duration);
    pr_spike_pattern_t* x_pattern = pr_spike_pattern_create(pop_size, duration);
    pr_spike_pattern_t* y_pattern = pr_spike_pattern_create(pop_size, duration);
    pr_spike_pattern_t* z_pattern = pr_spike_pattern_create(pop_size, duration);

    if (!w_pattern || !x_pattern || !y_pattern || !z_pattern) {
        pr_spike_pattern_destroy(w_pattern);
        pr_spike_pattern_destroy(x_pattern);
        pr_spike_pattern_destroy(y_pattern);
        pr_spike_pattern_destroy(z_pattern);
        return PR_SNN_ERROR_NO_MEMORY;
    }

    /* Encode each component with appropriate scheme */
    pr_snn_error_t err;

    /* w (consolidation) -> Rate coding */
    err = pr_snn_encode_rate(bridge, quat.w, w_pattern);
    if (err != PR_SNN_SUCCESS) goto cleanup;

    /* x (emotion) -> Burst coding */
    err = pr_snn_encode_burst(bridge, quat.x, x_pattern);
    if (err != PR_SNN_SUCCESS) goto cleanup;

    /* y (salience) -> Population coding */
    err = pr_snn_encode_population(bridge, quat.y, y_pattern);
    if (err != PR_SNN_SUCCESS) goto cleanup;

    /* z (accessibility) -> Latency coding */
    err = pr_snn_encode_latency(bridge, quat.z, z_pattern);
    if (err != PR_SNN_SUCCESS) goto cleanup;

    /* Merge all patterns */
    /* Offset neuron IDs to separate components */
    for (size_t i = 0; i < x_pattern->num_spikes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && x_pattern->num_spikes > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(i + 1) / (float)x_pattern->num_spikes);
        }

        x_pattern->neuron_ids[i] += (uint32_t)pop_size;
    }
    for (size_t i = 0; i < y_pattern->num_spikes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && y_pattern->num_spikes > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(i + 1) / (float)y_pattern->num_spikes);
        }

        y_pattern->neuron_ids[i] += (uint32_t)(pop_size * 2);
    }
    for (size_t i = 0; i < z_pattern->num_spikes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && z_pattern->num_spikes > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(i + 1) / (float)z_pattern->num_spikes);
        }

        z_pattern->neuron_ids[i] += (uint32_t)(pop_size * 3);
    }

    /* Clear output and copy all spikes */
    pr_spike_pattern_clear(pattern);
    pattern->num_neurons = pop_size * 4;

    for (size_t i = 0; i < w_pattern->num_spikes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && w_pattern->num_spikes > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(i + 1) / (float)w_pattern->num_spikes);
        }

        pr_spike_pattern_add_spike(pattern, w_pattern->spike_times[i],
                                   w_pattern->neuron_ids[i]);
    }
    for (size_t i = 0; i < x_pattern->num_spikes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && x_pattern->num_spikes > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(i + 1) / (float)x_pattern->num_spikes);
        }

        pr_spike_pattern_add_spike(pattern, x_pattern->spike_times[i],
                                   x_pattern->neuron_ids[i]);
    }
    for (size_t i = 0; i < y_pattern->num_spikes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && y_pattern->num_spikes > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(i + 1) / (float)y_pattern->num_spikes);
        }

        pr_spike_pattern_add_spike(pattern, y_pattern->spike_times[i],
                                   y_pattern->neuron_ids[i]);
    }
    for (size_t i = 0; i < z_pattern->num_spikes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && z_pattern->num_spikes > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(i + 1) / (float)z_pattern->num_spikes);
        }

        pr_spike_pattern_add_spike(pattern, z_pattern->spike_times[i],
                                   z_pattern->neuron_ids[i]);
    }

    pr_spike_pattern_sort(pattern);

    /* Update statistics */
    if (bridge->config.track_statistics) {
        PR_MUTEX_PTR_LOCK(bridge->base.mutex);
        bridge->stats.total_encodings++;
        bridge->stats.avg_encoding_time_us =
            (bridge->stats.avg_encoding_time_us * (bridge->stats.total_encodings - 1) +
             (get_time_us() - start_time)) / bridge->stats.total_encodings;
        PR_MUTEX_PTR_UNLOCK(bridge->base.mutex);
    }

    err = PR_SNN_SUCCESS;

cleanup:
    pr_spike_pattern_destroy(w_pattern);
    pr_spike_pattern_destroy(x_pattern);
    pr_spike_pattern_destroy(y_pattern);
    pr_spike_pattern_destroy(z_pattern);

    return err;
}

NIMCP_EXPORT pr_snn_error_t pr_snn_encode_quaternion_components(
    pr_snn_bridge_t bridge,
    nimcp_quaternion_t quat,
    pr_snn_component_patterns_t* patterns)
{
    if (!bridge || !patterns) return PR_SNN_ERROR_NULL_POINTER;
    if (!patterns->w_pattern || !patterns->x_pattern ||
        !patterns->y_pattern || !patterns->z_pattern) {
        return PR_SNN_ERROR_NULL_POINTER;
    }

    pr_snn_error_t err;

    err = pr_snn_encode_rate(bridge, quat.w, patterns->w_pattern);
    if (err != PR_SNN_SUCCESS) return err;

    err = pr_snn_encode_burst(bridge, quat.x, patterns->x_pattern);
    if (err != PR_SNN_SUCCESS) return err;

    err = pr_snn_encode_population(bridge, quat.y, patterns->y_pattern);
    if (err != PR_SNN_SUCCESS) return err;

    err = pr_snn_encode_latency(bridge, quat.z, patterns->z_pattern);
    if (err != PR_SNN_SUCCESS) return err;

    /* Create combined pattern if requested */
    if (patterns->combined) {
        const pr_spike_pattern_t* all[] = {
            patterns->w_pattern, patterns->x_pattern,
            patterns->y_pattern, patterns->z_pattern
        };
        pr_spike_pattern_t* merged = pr_spike_pattern_merge(all, 4);
        if (merged) {
            /* Copy to combined */
            pr_spike_pattern_clear(patterns->combined);
            for (size_t i = 0; i < merged->num_spikes; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && merged->num_spikes > 256) {
                    pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                                     (float)(i + 1) / (float)merged->num_spikes);
                }

                pr_spike_pattern_add_spike(patterns->combined,
                    merged->spike_times[i], merged->neuron_ids[i]);
            }
            pr_spike_pattern_destroy(merged);
        }
    }

    return PR_SNN_SUCCESS;
}

//=============================================================================
// Decoding Functions - Component Statistics
//=============================================================================

NIMCP_EXPORT float pr_spike_compute_rate(const pr_spike_pattern_t* pattern) {
    if (!pattern || pattern->duration_ms <= 0.0f) return -1.0f;

    float spikes = (float)pattern->num_spikes;
    float duration_sec = pattern->duration_ms / 1000.0f;
    float num_neurons = (float)pattern->num_neurons;

    if (num_neurons <= 0) return -1.0f;

    /* Rate per neuron in Hz */
    return spikes / (duration_sec * num_neurons);
}

NIMCP_EXPORT pr_snn_error_t pr_spike_compute_neuron_rates(
    const pr_spike_pattern_t* pattern,
    float* rates)
{
    if (!pattern || !rates) return PR_SNN_ERROR_NULL_POINTER;

    /* Count spikes per neuron */
    memset(rates, 0, pattern->num_neurons * sizeof(float));

    for (size_t i = 0; i < pattern->num_spikes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pattern->num_spikes > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(i + 1) / (float)pattern->num_spikes);
        }

        uint32_t n = pattern->neuron_ids[i];
        if (n < pattern->num_neurons) {
            rates[n] += 1.0f;
        }
    }

    /* Convert counts to rates (Hz) */
    float duration_sec = pattern->duration_ms / 1000.0f;
    if (duration_sec > 0.0f) {
        for (size_t n = 0; n < pattern->num_neurons; n++) {
            /* Phase 8: Loop progress heartbeat */
            if ((n & 0xFF) == 0 && pattern->num_neurons > 256) {
                pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                                 (float)(n + 1) / (float)pattern->num_neurons);
            }

            rates[n] /= duration_sec;
        }
    }

    return PR_SNN_SUCCESS;
}

NIMCP_EXPORT float pr_spike_compute_burst_index(
    pr_snn_bridge_t bridge,
    const pr_spike_pattern_t* pattern)
{
    if (!bridge || !pattern) return 0.0f;
    if (pattern->num_spikes < 2) return 0.0f;

    float burst_thresh = bridge->config.burst_threshold_ms;

    /* Compute ISIs and analyze burst characteristics */
    size_t short_isi_count = 0;
    size_t total_isi = 0;
    float isi_variance = 0.0f;
    float mean_isi = 0.0f;

    /* Need to compute ISIs per neuron */
    float* last_spike = bridge->latency_buffer;
    memset(last_spike, 0, bridge->buffer_capacity * sizeof(float));
    for (size_t n = 0; n < bridge->buffer_capacity; n++) {
        /* Phase 8: Loop progress heartbeat */
        if ((n & 0xFF) == 0 && bridge->buffer_capacity > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(n + 1) / (float)bridge->buffer_capacity);
        }

        last_spike[n] = -1000.0f;
    }

    /* Collect ISIs */
    float* isis = bridge->isi_buffer;
    size_t isi_count = 0;

    for (size_t i = 0; i < pattern->num_spikes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pattern->num_spikes > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(i + 1) / (float)pattern->num_spikes);
        }

        uint32_t n = pattern->neuron_ids[i];
        if (n >= bridge->buffer_capacity) continue;

        if (last_spike[n] >= 0.0f) {
            float isi = pattern->spike_times[i] - last_spike[n];
            if (isi > 0.0f && isi_count < PR_SNN_MAX_SPIKES_PER_PATTERN) {
                isis[isi_count++] = isi;
                if (isi < burst_thresh) {
                    short_isi_count++;
                }
            }
        }
        last_spike[n] = pattern->spike_times[i];
    }

    if (isi_count == 0) return 0.0f;

    /* Compute mean ISI */
    for (size_t i = 0; i < isi_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && isi_count > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(i + 1) / (float)isi_count);
        }

        mean_isi += isis[i];
    }
    mean_isi /= isi_count;

    /* Compute variance */
    for (size_t i = 0; i < isi_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && isi_count > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(i + 1) / (float)isi_count);
        }

        float diff = isis[i] - mean_isi;
        isi_variance += diff * diff;
    }
    isi_variance /= isi_count;

    /* Coefficient of variation */
    float cv = (mean_isi > 0.0f) ? sqrtf(isi_variance) / mean_isi : 1.0f;

    /* Fraction of short ISIs (indicating bursting) */
    float burst_frac = (float)short_isi_count / (float)isi_count;

    /* Burst index:
     * - High burst_frac + low CV = regular bursting (positive emotion) -> +1
     * - High burst_frac + high CV = irregular bursting (negative emotion) -> -1
     * - Low burst_frac = no bursting (neutral) -> 0
     */
    if (burst_frac < 0.2f) {
        /* No significant bursting */
        return 0.0f;
    }

    /* Map CV to regularity */
    float regularity = 1.0f - nimcp_myelin_clamp(cv, 0.0f, 2.0f) / 2.0f;

    /* Combine: high regularity = positive, low regularity = negative */
    float burst_index = (2.0f * regularity - 1.0f) * burst_frac;

    return nimcp_myelin_clamp(burst_index, -1.0f, 1.0f);
}

NIMCP_EXPORT size_t pr_spike_compute_population_size(const pr_spike_pattern_t* pattern) {
    if (!pattern) return 0;
    if (pattern->num_spikes == 0) return 0;

    /* Count unique neurons that fired */
    /* Use bit mask if population is small enough */
    if (pattern->num_neurons <= 1024) {
        uint8_t mask[128] = {0};  /* 1024 bits */
        size_t count = 0;

        for (size_t i = 0; i < pattern->num_spikes; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && pattern->num_spikes > 256) {
                pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                                 (float)(i + 1) / (float)pattern->num_spikes);
            }

            uint32_t n = pattern->neuron_ids[i];
            if (n < 1024) {
                size_t byte = n / 8;
                size_t bit = n % 8;
                if (!(mask[byte] & (1 << bit))) {
                    mask[byte] |= (1 << bit);
                    count++;
                }
            }
        }
        return count;
    }

    /* For larger populations, use simple counting */
    /* This is O(n*m) but acceptable for typical usage */
    size_t count = 0;
    for (size_t n = 0; n < pattern->num_neurons; n++) {
        /* Phase 8: Loop progress heartbeat */
        if ((n & 0xFF) == 0 && pattern->num_neurons > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(n + 1) / (float)pattern->num_neurons);
        }

        for (size_t i = 0; i < pattern->num_spikes; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && pattern->num_spikes > 256) {
                pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                                 (float)(i + 1) / (float)pattern->num_spikes);
            }

            if (pattern->neuron_ids[i] == n) {
                count++;
                break;
            }
        }
    }

    return count;
}

NIMCP_EXPORT float pr_spike_compute_latency(const pr_spike_pattern_t* pattern) {
    if (!pattern || pattern->num_spikes == 0) return -1.0f;

    /* Find first spike for each neuron, compute mean */
    float sum_latency = 0.0f;
    size_t count = 0;

    /* Use temporary storage for first spike per neuron */
    float first_spike[1024];
    for (size_t i = 0; i < 1024 && i < pattern->num_neurons; i++) {
        first_spike[i] = -1.0f;
    }

    /* Find first spike for each neuron */
    for (size_t i = 0; i < pattern->num_spikes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pattern->num_spikes > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(i + 1) / (float)pattern->num_spikes);
        }

        uint32_t n = pattern->neuron_ids[i];
        if (n < 1024 && n < pattern->num_neurons) {
            if (first_spike[n] < 0.0f || pattern->spike_times[i] < first_spike[n]) {
                first_spike[n] = pattern->spike_times[i];
            }
        }
    }

    /* Compute mean of first spikes */
    for (size_t n = 0; n < 1024 && n < pattern->num_neurons; n++) {
        if (first_spike[n] >= 0.0f) {
            sum_latency += first_spike[n];
            count++;
        }
    }

    if (count == 0) return -1.0f;
    return sum_latency / count;
}

NIMCP_EXPORT pr_snn_error_t pr_spike_compute_isi(
    const pr_spike_pattern_t* pattern,
    pr_snn_isi_stats_t* stats)
{
    if (!pattern || !stats) return PR_SNN_ERROR_NULL_POINTER;

    memset(stats, 0, sizeof(pr_snn_isi_stats_t));

    if (pattern->num_spikes < 2) {
        stats->mean_isi_ms = 0.0f;
        return PR_SNN_SUCCESS;
    }

    /* Collect ISIs */
    float* isis = (float*)nimcp_calloc(pattern->num_spikes, sizeof(float));
    if (!isis) return PR_SNN_ERROR_NO_MEMORY;

    size_t isi_count = 0;
    float last_spike[1024];
    for (size_t i = 0; i < 1024; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 1024 > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(i + 1) / (float)1024);
        }

        last_spike[i] = -1000.0f;
    }

    for (size_t i = 0; i < pattern->num_spikes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pattern->num_spikes > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(i + 1) / (float)pattern->num_spikes);
        }

        uint32_t n = pattern->neuron_ids[i];
        if (n >= 1024) continue;

        if (last_spike[n] >= 0.0f) {
            float isi = pattern->spike_times[i] - last_spike[n];
            if (isi > 0.0f) {
                isis[isi_count++] = isi;
            }
        }
        last_spike[n] = pattern->spike_times[i];
    }

    if (isi_count == 0) {
        nimcp_free(isis);
        isis = NULL;
        return PR_SNN_SUCCESS;
    }

    /* Compute statistics */
    stats->isi_count = isi_count;
    stats->min_isi_ms = isis[0];
    stats->max_isi_ms = isis[0];

    float sum = 0.0f;
    for (size_t i = 0; i < isi_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && isi_count > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(i + 1) / (float)isi_count);
        }

        sum += isis[i];
        if (isis[i] < stats->min_isi_ms) stats->min_isi_ms = isis[i];
        if (isis[i] > stats->max_isi_ms) stats->max_isi_ms = isis[i];
    }
    stats->mean_isi_ms = sum / (fabsf(isi_count) > 1e-7f ? isi_count : 1e-7f);

    float var_sum = 0.0f;
    for (size_t i = 0; i < isi_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && isi_count > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(i + 1) / (float)isi_count);
        }

        float diff = isis[i] - stats->mean_isi_ms;
        var_sum += diff * diff;
    }
    stats->std_isi_ms = sqrtf(var_sum / (fabsf(isi_count) > 1e-7f ? isi_count : 1e-7f));

    stats->cv_isi = (stats->mean_isi_ms > 0.0f) ?
                    stats->std_isi_ms / stats->mean_isi_ms : 0.0f;

    nimcp_free(isis);
    isis = NULL;
    return PR_SNN_SUCCESS;
}

NIMCP_EXPORT float pr_spike_compute_synchrony(
    const pr_spike_pattern_t* pattern,
    float window_ms)
{
    if (!pattern || pattern->num_spikes < 2) return 0.0f;
    if (window_ms <= 0.0f) window_ms = 5.0f;

    /* Count spike coincidences */
    size_t coincidences = 0;
    size_t total_pairs = 0;

    for (size_t i = 0; i < pattern->num_spikes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pattern->num_spikes > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(i + 1) / (float)pattern->num_spikes);
        }

        for (size_t j = i + 1; j < pattern->num_spikes; j++) {
            if (pattern->neuron_ids[i] != pattern->neuron_ids[j]) {
                total_pairs++;
                float dt = fabsf(pattern->spike_times[i] - pattern->spike_times[j]);
                if (dt <= window_ms) {
                    coincidences++;
                }
            }
        }
    }

    if (total_pairs == 0) return 0.0f;
    return (float)coincidences / (float)total_pairs;
}

NIMCP_EXPORT float pr_spike_compute_cv(const pr_spike_pattern_t* pattern) {
    pr_snn_isi_stats_t stats;
    if (pr_spike_compute_isi(pattern, &stats) != PR_SNN_SUCCESS) {
        return -1.0f;
    }
    return stats.cv_isi;
}

//=============================================================================
// Decoding Functions - Quaternion Reconstruction
//=============================================================================

NIMCP_EXPORT float pr_snn_decode_rate(
    pr_snn_bridge_t bridge,
    const pr_spike_pattern_t* pattern)
{
    if (!bridge || !pattern) return -1.0f;

    float rate = pr_spike_compute_rate(pattern);
    if (rate < 0.0f) return 0.0f;

    /* Normalize by max rate */
    float normalized = rate / bridge->config.max_rate_hz;
    return nimcp_myelin_clamp(normalized, 0.0f, 1.0f);
}

NIMCP_EXPORT float pr_snn_decode_burst(
    pr_snn_bridge_t bridge,
    const pr_spike_pattern_t* pattern)
{
    if (!bridge || !pattern) return 0.0f;
    return pr_spike_compute_burst_index(bridge, pattern);
}

NIMCP_EXPORT float pr_snn_decode_population(
    pr_snn_bridge_t bridge,
    const pr_spike_pattern_t* pattern)
{
    if (!bridge || !pattern) return -1.0f;
    if (pattern->num_neurons == 0) return 0.0f;

    size_t active = pr_spike_compute_population_size(pattern);
    return (float)active / (float)pattern->num_neurons;
}

NIMCP_EXPORT float pr_snn_decode_latency(
    pr_snn_bridge_t bridge,
    const pr_spike_pattern_t* pattern)
{
    if (!bridge || !pattern) return -1.0f;

    float latency = pr_spike_compute_latency(pattern);
    if (latency < 0.0f) return 0.0f;

    float min_lat = bridge->config.min_latency_ms;
    float max_lat = bridge->config.max_latency_ms;

    /* Higher value = shorter latency */
    float normalized = (max_lat - latency) / (max_lat - min_lat);
    return nimcp_myelin_clamp(normalized, 0.0f, 1.0f);
}

NIMCP_EXPORT pr_snn_error_t pr_snn_decode_to_quaternion(
    pr_snn_bridge_t bridge,
    const pr_spike_pattern_t* pattern,
    pr_decoded_quat_t* decoded)
{
    if (!bridge || !pattern || !decoded) return PR_SNN_ERROR_NULL_POINTER;

    uint64_t start_time = get_time_us();

    memset(decoded, 0, sizeof(pr_decoded_quat_t));

    /* Extract component patterns if possible (assume 4x population) */
    size_t pop_size = bridge->config.population_size;

    if (pattern->num_neurons >= pop_size * 4) {
        /* Pattern contains all 4 components */
        pr_spike_pattern_t* w_part = pr_spike_pattern_extract_neurons(pattern, 0, (uint32_t)pop_size);
        pr_spike_pattern_t* x_part = pr_spike_pattern_extract_neurons(pattern, (uint32_t)pop_size, (uint32_t)(pop_size * 2));
        pr_spike_pattern_t* y_part = pr_spike_pattern_extract_neurons(pattern, (uint32_t)(pop_size * 2), (uint32_t)(pop_size * 3));
        pr_spike_pattern_t* z_part = pr_spike_pattern_extract_neurons(pattern, (uint32_t)(pop_size * 3), (uint32_t)(pop_size * 4));

        if (w_part && x_part && y_part && z_part) {
            decoded->quat.w = pr_snn_decode_rate(bridge, w_part);
            decoded->quat.x = pr_snn_decode_burst(bridge, x_part);
            decoded->quat.y = pr_snn_decode_population(bridge, y_part);
            decoded->quat.z = pr_snn_decode_latency(bridge, z_part);

            /* Compute confidence based on spike counts */
            decoded->w_confidence = (w_part->num_spikes > 10) ? 0.9f : 0.5f;
            decoded->x_confidence = (x_part->num_spikes > 5) ? 0.8f : 0.4f;
            decoded->y_confidence = (y_part->num_spikes > 0) ? 0.9f : 0.3f;
            decoded->z_confidence = (z_part->num_spikes > pop_size/2) ? 0.9f : 0.5f;
        }

        pr_spike_pattern_destroy(w_part);
        pr_spike_pattern_destroy(x_part);
        pr_spike_pattern_destroy(y_part);
        pr_spike_pattern_destroy(z_part);
    } else {
        /* Pattern is single component or unclear */
        /* Use heuristics to decode */
        float rate = pr_snn_decode_rate(bridge, pattern);
        float burst = pr_spike_compute_burst_index(bridge, pattern);
        float pop_frac = pr_snn_decode_population(bridge, pattern);
        float lat = pr_snn_decode_latency(bridge, pattern);

        decoded->quat.w = rate;
        decoded->quat.x = burst;
        decoded->quat.y = pop_frac;
        decoded->quat.z = lat;

        /* Lower confidence for ambiguous patterns */
        decoded->w_confidence = 0.5f;
        decoded->x_confidence = 0.5f;
        decoded->y_confidence = 0.5f;
        decoded->z_confidence = 0.5f;
    }

    /* Overall confidence is geometric mean of component confidences */
    decoded->confidence = powf(
        decoded->w_confidence * decoded->x_confidence *
        decoded->y_confidence * decoded->z_confidence, 0.25f);

    /* Reconstruction error (would need original to compute properly) */
    decoded->reconstruction_error = 1.0f - decoded->confidence;

    /* Update statistics */
    if (bridge->config.track_statistics) {
        PR_MUTEX_PTR_LOCK(bridge->base.mutex);
        bridge->stats.total_decodings++;
        bridge->stats.avg_reconstruction_error =
            (bridge->stats.avg_reconstruction_error * (bridge->stats.total_decodings - 1) +
             decoded->reconstruction_error) / bridge->stats.total_decodings;
        bridge->stats.avg_decoding_time_us =
            (bridge->stats.avg_decoding_time_us * (bridge->stats.total_decodings - 1) +
             (get_time_us() - start_time)) / bridge->stats.total_decodings;
        PR_MUTEX_PTR_UNLOCK(bridge->base.mutex);
    }

    return PR_SNN_SUCCESS;
}

NIMCP_EXPORT pr_snn_error_t pr_snn_decode_from_components(
    pr_snn_bridge_t bridge,
    const pr_snn_component_patterns_t* patterns,
    pr_decoded_quat_t* decoded)
{
    if (!bridge || !patterns || !decoded) return PR_SNN_ERROR_NULL_POINTER;

    memset(decoded, 0, sizeof(pr_decoded_quat_t));

    if (patterns->w_pattern) {
        decoded->quat.w = pr_snn_decode_rate(bridge, patterns->w_pattern);
        decoded->w_confidence = 0.9f;
    }

    if (patterns->x_pattern) {
        decoded->quat.x = pr_snn_decode_burst(bridge, patterns->x_pattern);
        decoded->x_confidence = 0.85f;
    }

    if (patterns->y_pattern) {
        decoded->quat.y = pr_snn_decode_population(bridge, patterns->y_pattern);
        decoded->y_confidence = 0.9f;
    }

    if (patterns->z_pattern) {
        decoded->quat.z = pr_snn_decode_latency(bridge, patterns->z_pattern);
        decoded->z_confidence = 0.85f;
    }

    decoded->confidence = powf(
        decoded->w_confidence * decoded->x_confidence *
        decoded->y_confidence * decoded->z_confidence, 0.25f);
    decoded->reconstruction_error = 1.0f - decoded->confidence;

    return PR_SNN_SUCCESS;
}

//=============================================================================
// Memory Node Integration
//=============================================================================

NIMCP_EXPORT pr_snn_error_t pr_snn_encode_node(
    pr_snn_bridge_t bridge,
    const pr_memory_node_t* node,
    pr_spike_pattern_t* pattern)
{
    if (!bridge || !node || !pattern) return PR_SNN_ERROR_NULL_POINTER;

    /* Encode the node's quaternion state */
    nimcp_quaternion_t state = pr_memory_node_get_state(node);

    pr_snn_error_t err = pr_snn_encode_quaternion(bridge, state, pattern);

    if (err == PR_SNN_SUCCESS && bridge->config.track_statistics) {
        PR_MUTEX_PTR_LOCK(bridge->base.mutex);
        bridge->stats.total_node_encodings++;
        PR_MUTEX_PTR_UNLOCK(bridge->base.mutex);
    }

    return err;
}

NIMCP_EXPORT pr_snn_error_t pr_snn_decode_to_node(
    pr_snn_bridge_t bridge,
    const pr_spike_pattern_t* pattern,
    pr_memory_node_t* node)
{
    if (!bridge || !pattern || !node) return PR_SNN_ERROR_NULL_POINTER;

    pr_decoded_quat_t decoded;
    pr_snn_error_t err = pr_snn_decode_to_quaternion(bridge, pattern, &decoded);
    if (err != PR_SNN_SUCCESS) return err;

    /* Update node state */
    pr_node_error_t node_err = pr_memory_node_update_state(node, decoded.quat);
    if (node_err != PR_NODE_SUCCESS) {
        return PR_SNN_ERROR_NODE_FAILED;
    }

    return PR_SNN_SUCCESS;
}

NIMCP_EXPORT int pr_snn_retrieve_via_snn(
    pr_snn_bridge_t bridge,
    const prime_signature_t* query_signature,
    snn_network_t* snn,
    size_t top_k,
    uint64_t* result_ids,
    float* result_scores)
{
    if (!bridge || !query_signature || !snn || !result_ids || !result_scores) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_snn_retrieve_via_snn: required parameter is NULL (bridge, query_signature, snn, result_ids, result_scores)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_snn_bridge_heartbeat("pr_snn_bridg_pr_snn_retrieve_via_", 0.0f);

    if (!bridge->initialized) {
        set_error("Bridge not initialized");
        return -1;
    }
    if (top_k == 0) return 0;

    /* Convert query signature exponents into input rate vector.
     * Each prime dimension maps to a population-coded firing rate. */
    size_t pop_size = bridge->config.population_size;
    float duration = bridge->config.encoding_window_ms;
    uint32_t input_dim = (PRIME_SIG_DIM < pop_size) ? PRIME_SIG_DIM : (uint32_t)pop_size;
    uint32_t output_dim = input_dim;

    float* input_rates = nimcp_calloc(input_dim, sizeof(float));
    float* output_rates = nimcp_calloc(output_dim, sizeof(float));
    if (!input_rates || !output_rates) {
        nimcp_free(input_rates);
        nimcp_free(output_rates);
        set_error("Failed to allocate rate buffers");
        return -1;
    }

    /* Encode signature exponents as normalized firing rates */
    float max_exp = 0.0f;
    for (uint32_t i = 0; i < input_dim && i < query_signature->num_factors; i++) {
        input_rates[i] = (float)query_signature->exponents[i];
        if (input_rates[i] > max_exp) max_exp = input_rates[i];
    }
    if (max_exp > 0.0f) {
        for (uint32_t i = 0; i < input_dim; i++) {
            input_rates[i] /= max_exp;
        }
    }

    /* Forward pass through SNN */
    int snn_rc = snn_network_forward(snn, input_rates, input_dim,
                                      output_rates, output_dim, duration);

    int num_results = 0;
    if (snn_rc >= 0) {
        /* Interpret SNN output as retrieval scores.
         * Each output neuron group represents a candidate memory.
         * Group output rates into top_k candidates by max activation. */
        uint32_t group_size = (output_dim > 0 && top_k > 0) ?
                               output_dim / (uint32_t)top_k : 1;
        if (group_size == 0) group_size = 1;

        for (size_t k = 0; k < top_k && k * group_size < output_dim; k++) {
            float score = 0.0f;
            for (uint32_t j = 0; j < group_size &&
                 (k * group_size + j) < output_dim; j++) {
                score += output_rates[k * group_size + j];
            }
            score /= (float)group_size;

            if (score > 0.01f) {
                result_ids[num_results] = (uint64_t)(k + 1);
                result_scores[num_results] = score;
                num_results++;
            }
        }
    }

    nimcp_free(input_rates);
    nimcp_free(output_rates);

    bridge->stats.total_decodings++;

    return num_results;
}

//=============================================================================
// Entanglement Integration
//=============================================================================

NIMCP_EXPORT pr_snn_error_t pr_snn_encode_entanglement(
    pr_snn_bridge_t bridge,
    const entangle_edge_t* edge,
    pr_spike_pattern_t* pattern1,
    pr_spike_pattern_t* pattern2)
{
    if (!bridge || !edge || !pattern1 || !pattern2) {
        return PR_SNN_ERROR_NULL_POINTER;
    }

    /* Generate correlated patterns based on resonance_score */
    float correlation = edge->resonance_score;

    /* Generate base pattern for pattern1 */
    float base_rate = bridge->config.max_rate_hz * 0.5f;
    float rate_per_ms = base_rate / 1000.0f;
    float duration = bridge->config.encoding_window_ms;
    size_t pop_size = bridge->config.population_size;

    pr_spike_pattern_clear(pattern1);
    pr_spike_pattern_clear(pattern2);

    /* Generate spikes for pattern1 */
    for (size_t n = 0; n < pop_size; n++) {
        /* Phase 8: Loop progress heartbeat */
        if ((n & 0xFF) == 0 && pop_size > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(n + 1) / (float)pop_size);
        }

        float t = 0.0f;
        while (t < duration) {
            float isi = rng_exponential(bridge, rate_per_ms);
            t += isi;
            if (t < duration) {
                pr_spike_pattern_add_spike(pattern1, t, (uint32_t)n);

                /* For pattern2, add correlated spike with probability = correlation */
                if (rng_uniform(bridge) < correlation) {
                    /* Add with small jitter */
                    float jitter = rng_gaussian(bridge, 0.0f, 2.0f);
                    float t2 = t + jitter;
                    if (t2 >= 0.0f && t2 < duration) {
                        pr_spike_pattern_add_spike(pattern2, t2, (uint32_t)n);
                    }
                } else {
                    /* Add uncorrelated spike */
                    float t2 = rng_uniform(bridge) * duration;
                    pr_spike_pattern_add_spike(pattern2, t2, (uint32_t)n);
                }
            }
        }
    }

    pr_spike_pattern_sort(pattern1);
    pr_spike_pattern_sort(pattern2);

    if (bridge->config.track_statistics) {
        PR_MUTEX_PTR_LOCK(bridge->base.mutex);
        bridge->stats.total_entangle_ops++;
        PR_MUTEX_PTR_UNLOCK(bridge->base.mutex);
    }

    return PR_SNN_SUCCESS;
}

NIMCP_EXPORT pr_snn_error_t pr_snn_decode_entanglement(
    pr_snn_bridge_t bridge,
    const pr_spike_pattern_t* pattern1,
    const pr_spike_pattern_t* pattern2,
    pr_snn_spike_correlation_t* correlation)
{
    if (!bridge || !pattern1 || !pattern2 || !correlation) {
        return PR_SNN_ERROR_NULL_POINTER;
    }

    memset(correlation, 0, sizeof(pr_snn_spike_correlation_t));

    if (pattern1->num_spikes == 0 || pattern2->num_spikes == 0) {
        return PR_SNN_SUCCESS;
    }

    /* Count coincident spikes within STDP window */
    float stdp_window = PR_SNN_STDP_WINDOW_MS;
    size_t coincident = 0;
    float total_delay = 0.0f;

    for (size_t i = 0; i < pattern1->num_spikes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pattern1->num_spikes > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(i + 1) / (float)pattern1->num_spikes);
        }

        float t1 = pattern1->spike_times[i];

        for (size_t j = 0; j < pattern2->num_spikes; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && pattern2->num_spikes > 256) {
                pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                                 (float)(j + 1) / (float)pattern2->num_spikes);
            }

            float t2 = pattern2->spike_times[j];
            float dt = fabsf(t1 - t2);

            if (dt <= stdp_window) {
                coincident++;
                total_delay += (t2 - t1);  /* Signed delay */
            }
        }
    }

    /* Compute correlation coefficient */
    size_t max_possible = pattern1->num_spikes * pattern2->num_spikes;
    if (max_possible > 0) {
        correlation->correlation = (float)coincident / sqrtf((float)max_possible);
        correlation->correlation = nimcp_myelin_clamp(correlation->correlation, 0.0f, 1.0f);
    }

    /* Compute synchrony */
    correlation->synchrony = pr_spike_compute_synchrony(pattern1, 5.0f);

    /* Mean delay */
    if (coincident > 0) {
        correlation->mean_delay_ms = total_delay / coincident;
    }

    correlation->coincident_spikes = coincident;

    /* STDP weight change: potentiation if pre before post, depression otherwise */
    if (coincident > 0) {
        float normalized_delay = correlation->mean_delay_ms / stdp_window;
        /* Asymmetric STDP curve */
        if (normalized_delay > 0) {
            /* Pre before post: potentiation */
            correlation->stdp_weight_change = PR_SNN_STDP_MAX_DELTA *
                                               expf(-normalized_delay);
        } else {
            /* Post before pre: depression */
            correlation->stdp_weight_change = -PR_SNN_STDP_MAX_DELTA *
                                               expf(normalized_delay);
        }
    }

    return PR_SNN_SUCCESS;
}

NIMCP_EXPORT float pr_snn_strengthen_via_spikes(
    pr_snn_bridge_t bridge,
    entangle_edge_t* edge,
    const pr_spike_pattern_t* pattern1,
    const pr_spike_pattern_t* pattern2)
{
    if (!bridge || !edge || !pattern1 || !pattern2) {
        return NAN;
    }

    pr_snn_spike_correlation_t corr;
    pr_snn_error_t err = pr_snn_decode_entanglement(bridge, pattern1, pattern2, &corr);
    if (err != PR_SNN_SUCCESS) {
        return NAN;
    }

    /* Apply weight change */
    float new_weight = edge->weight + corr.stdp_weight_change;
    new_weight = nimcp_myelin_clamp(new_weight, 0.0f, 1.0f);
    edge->weight = new_weight;

    return corr.stdp_weight_change;
}

//=============================================================================
// Synchronization Functions
//=============================================================================

NIMCP_EXPORT float pr_snn_sync_coherence(
    const pr_spike_pattern_t* const* patterns,
    size_t count)
{
    if (!patterns || count < 2) return -1.0f;

    /* Compute pairwise synchrony and average */
    float total_sync = 0.0f;
    size_t pairs = 0;

    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(i + 1) / (float)count);
        }

        if (!patterns[i]) continue;
        for (size_t j = i + 1; j < count; j++) {
            if (!patterns[j]) continue;

            /* Simple synchrony measure based on spike count correlation */
            float sync = 0.0f;
            float s1 = (float)patterns[i]->num_spikes;
            float s2 = (float)patterns[j]->num_spikes;

            if (s1 > 0 && s2 > 0) {
                float min_s = (s1 < s2) ? s1 : s2;
                float max_s = (s1 > s2) ? s1 : s2;
                sync = min_s / max_s;
            }

            total_sync += sync;
            pairs++;
        }
    }

    if (pairs == 0) return 0.0f;
    return total_sync / pairs;
}

NIMCP_EXPORT pr_snn_error_t pr_snn_phase_lock(
    pr_snn_bridge_t bridge,
    pr_spike_pattern_t* pattern,
    float theta_phase)
{
    if (!bridge || !pattern) return PR_SNN_ERROR_NULL_POINTER;

    float theta_period = 1000.0f / bridge->config.theta_frequency_hz;
    float phase_offset = (theta_phase / NIMCP_TWO_PI_F) * theta_period;

    /* Shift all spike times to align with theta phase */
    for (size_t i = 0; i < pattern->num_spikes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pattern->num_spikes > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(i + 1) / (float)pattern->num_spikes);
        }

        float t = pattern->spike_times[i];

        /* Find which theta cycle this spike is in */
        float cycle = floorf(t / theta_period);
        float phase_in_cycle = fmodf(t, theta_period);

        /* Shift to target phase */
        float new_phase = phase_offset + (phase_in_cycle - phase_offset) * 0.5f;
        pattern->spike_times[i] = cycle * theta_period + new_phase;
    }

    pr_spike_pattern_sort(pattern);

    return PR_SNN_SUCCESS;
}

NIMCP_EXPORT float pr_snn_compute_plv(
    const pr_spike_pattern_t* pattern1,
    const pr_spike_pattern_t* pattern2,
    float frequency)
{
    if (!pattern1 || !pattern2 || frequency <= 0.0f) return -1.0f;
    if (pattern1->num_spikes == 0 || pattern2->num_spikes == 0) return 0.0f;

    float period = 1000.0f / frequency;

    /* Compute phase at each spike time */
    float sum_cos = 0.0f;
    float sum_sin = 0.0f;
    size_t count = 0;

    for (size_t i = 0; i < pattern1->num_spikes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pattern1->num_spikes > 256) {
            pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                             (float)(i + 1) / (float)pattern1->num_spikes);
        }

        float phase1 = fmodf(pattern1->spike_times[i], period) / period * NIMCP_TWO_PI_F;

        /* Find nearest spike in pattern2 */
        float min_dt = FLT_MAX;
        size_t nearest = 0;
        for (size_t j = 0; j < pattern2->num_spikes; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && pattern2->num_spikes > 256) {
                pr_snn_bridge_heartbeat("pr_snn_bridg_loop",
                                 (float)(j + 1) / (float)pattern2->num_spikes);
            }

            float dt = fabsf(pattern1->spike_times[i] - pattern2->spike_times[j]);
            if (dt < min_dt) {
                min_dt = dt;
                nearest = j;
            }
        }

        if (min_dt < period) {
            float phase2 = fmodf(pattern2->spike_times[nearest], period) / period * NIMCP_TWO_PI_F;
            float phase_diff = phase1 - phase2;
            sum_cos += cosf(phase_diff);
            sum_sin += sinf(phase_diff);
            count++;
        }
    }

    if (count == 0) return 0.0f;

    /* PLV = |mean(exp(i * phase_diff))| */
    float plv = sqrtf(sum_cos * sum_cos + sum_sin * sum_sin) / count;
    return nimcp_myelin_clamp(plv, 0.0f, 1.0f);
}

//=============================================================================
// Statistics Functions
//=============================================================================

NIMCP_EXPORT pr_snn_error_t pr_snn_bridge_get_stats(
    const pr_snn_bridge_t bridge,
    pr_snn_bridge_stats_t* stats)
{
    if (!bridge || !stats) return PR_SNN_ERROR_NULL_POINTER;

    PR_MUTEX_LOCK(((pr_snn_bridge_t)bridge)->mutex);
    *stats = bridge->stats;
    PR_MUTEX_UNLOCK(((pr_snn_bridge_t)bridge)->mutex);

    return PR_SNN_SUCCESS;
}

NIMCP_EXPORT pr_snn_error_t pr_snn_bridge_get_last_encode_stats(
    const pr_snn_bridge_t bridge,
    pr_snn_encode_stats_t* stats)
{
    if (!bridge || !stats) return PR_SNN_ERROR_NULL_POINTER;

    PR_MUTEX_LOCK(((pr_snn_bridge_t)bridge)->mutex);
    *stats = bridge->last_encode_stats;
    PR_MUTEX_UNLOCK(((pr_snn_bridge_t)bridge)->mutex);

    return PR_SNN_SUCCESS;
}

NIMCP_EXPORT void pr_spike_pattern_print(const pr_spike_pattern_t* pattern) {
    if (!pattern) {
        printf("Spike Pattern: NULL\n");
        return;
    }

    printf("Spike Pattern:\n");
    printf("  Neurons: %zu\n", pattern->num_neurons);
    printf("  Duration: %.2f ms\n", pattern->duration_ms);
    printf("  Spikes: %zu (capacity %zu)\n", pattern->num_spikes, pattern->capacity);

    if (pattern->num_spikes > 0) {
        printf("  First spike: t=%.2f ms, n=%u\n",
               pattern->spike_times[0], pattern->neuron_ids[0]);
        if (pattern->num_spikes > 1) {
            printf("  Last spike: t=%.2f ms, n=%u\n",
                   pattern->spike_times[pattern->num_spikes - 1],
                   pattern->neuron_ids[pattern->num_spikes - 1]);
        }

        float rate = pr_spike_compute_rate(pattern);
        printf("  Mean rate: %.2f Hz\n", rate);
    }
}

NIMCP_EXPORT void pr_snn_bridge_print_state(const pr_snn_bridge_t bridge) {
    if (!bridge) {
        printf("SNN Bridge: NULL\n");
        return;
    }

    printf("SNN Bridge State:\n");
    printf("  Population size: %zu\n", bridge->config.population_size);
    printf("  Encoding window: %.2f ms\n", bridge->config.encoding_window_ms);
    printf("  Max rate: %.2f Hz\n", bridge->config.max_rate_hz);
    printf("  Noise enabled: %s (level=%.2f)\n",
           bridge->config.enable_noise ? "yes" : "no",
           bridge->config.noise_level);
    printf("  Statistics:\n");
    printf("    Total encodings: %lu\n", (unsigned long)bridge->stats.total_encodings);
    printf("    Total decodings: %lu\n", (unsigned long)bridge->stats.total_decodings);
    printf("    Avg reconstruction error: %.4f\n", bridge->stats.avg_reconstruction_error);
    printf("    Avg spike count: %.2f\n", bridge->stats.avg_spike_count);
    printf("    Avg encoding time: %.2f us\n", bridge->stats.avg_encoding_time_us);
}

//=============================================================================
// Utility Functions
//=============================================================================

NIMCP_EXPORT const char* pr_snn_error_string(pr_snn_error_t error) {
    switch (error) {
        case PR_SNN_SUCCESS: return "Success";
        case PR_SNN_ERROR_NULL_POINTER: return "Null pointer";
        case PR_SNN_ERROR_INVALID_CONFIG: return "Invalid configuration";
        case PR_SNN_ERROR_NO_MEMORY: return "Memory allocation failed";
        case PR_SNN_ERROR_INVALID_STATE: return "Invalid bridge state";
        case PR_SNN_ERROR_INVALID_PATTERN: return "Invalid spike pattern";
        case PR_SNN_ERROR_ENCODING_FAILED: return "Encoding failed";
        case PR_SNN_ERROR_DECODING_FAILED: return "Decoding failed";
        case PR_SNN_ERROR_SNN_FAILED: return "SNN operation failed";
        case PR_SNN_ERROR_NODE_FAILED: return "Memory node operation failed";
        case PR_SNN_ERROR_ENTANGLE_FAILED: return "Entanglement operation failed";
        case PR_SNN_ERROR_PATTERN_FULL: return "Pattern buffer full";
        case PR_SNN_ERROR_INVALID_ENCODING: return "Invalid encoding type";
        default: return "Unknown error";
    }
}

NIMCP_EXPORT const char* pr_snn_get_last_error(void) {
    if (g_last_error[0] == '\0') {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_snn_get_last_error: validation failed");
        return NULL;
    }
    return g_last_error;
}

NIMCP_EXPORT const char* pr_snn_encoding_name(pr_snn_encoding_t encoding) {
    switch (encoding) {
        case PR_SNN_ENCODE_RATE: return "RATE";
        case PR_SNN_ENCODE_BURST: return "BURST";
        case PR_SNN_ENCODE_POPULATION: return "POPULATION";
        case PR_SNN_ENCODE_LATENCY: return "LATENCY";
        case PR_SNN_ENCODE_PHASE: return "PHASE";
        case PR_SNN_ENCODE_RANK_ORDER: return "RANK_ORDER";
        default: return "UNKNOWN";
    }
}

NIMCP_EXPORT pr_snn_component_patterns_t* pr_snn_component_patterns_create(
    size_t num_neurons,
    float duration_ms)
{
    pr_snn_component_patterns_t* patterns =
        (pr_snn_component_patterns_t*)nimcp_calloc(1, sizeof(pr_snn_component_patterns_t));
    if (!patterns) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "patterns is NULL");

        return NULL;

    }

    patterns->w_pattern = pr_spike_pattern_create(num_neurons, duration_ms);
    patterns->x_pattern = pr_spike_pattern_create(num_neurons, duration_ms);
    patterns->y_pattern = pr_spike_pattern_create(num_neurons, duration_ms);
    patterns->z_pattern = pr_spike_pattern_create(num_neurons, duration_ms);
    patterns->combined = pr_spike_pattern_create(num_neurons * 4, duration_ms);

    if (!patterns->w_pattern || !patterns->x_pattern ||
        !patterns->y_pattern || !patterns->z_pattern || !patterns->combined) {
        pr_snn_component_patterns_destroy(patterns);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_snn_component_patterns_create: operation failed");
        return NULL;
    }

    return patterns;
}

NIMCP_EXPORT void pr_snn_component_patterns_destroy(pr_snn_component_patterns_t* patterns) {
    if (!patterns) return;

    pr_spike_pattern_destroy(patterns->w_pattern);
    pr_spike_pattern_destroy(patterns->x_pattern);
    pr_spike_pattern_destroy(patterns->y_pattern);
    pr_spike_pattern_destroy(patterns->z_pattern);
    pr_spike_pattern_destroy(patterns->combined);

    nimcp_free(patterns);
    patterns = NULL;
}

NIMCP_EXPORT uint64_t pr_snn_current_time_us(void) {
    return get_time_us();
}

//=============================================================================
// Instance Health Agent Setter (B25 Upgrade)
//=============================================================================

void pr_snn_bridge_set_instance_health_agent(
    pr_snn_bridge_t bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B25 Upgrade)
//=============================================================================

int pr_snn_bridge_training_begin(pr_snn_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_snn_bridge_training_begin: NULL argument");
        return -1;
    }
    pr_snn_bridge_heartbeat_instance(bridge->health_agent, "pr_snn_bridge_training_begin", 0.0f);
    return 0;
}

int pr_snn_bridge_training_end(pr_snn_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_snn_bridge_training_end: NULL argument");
        return -1;
    }
    pr_snn_bridge_heartbeat_instance(bridge->health_agent, "pr_snn_bridge_training_end", 1.0f);
    return 0;
}

int pr_snn_bridge_training_step(pr_snn_bridge_t bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_snn_bridge_training_step: NULL argument");
        return -1;
    }
    pr_snn_bridge_heartbeat_instance(bridge->health_agent, "pr_snn_bridge_training_step", progress);
    return 0;
}
