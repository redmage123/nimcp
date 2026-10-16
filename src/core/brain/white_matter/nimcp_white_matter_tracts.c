//=============================================================================
// nimcp_white_matter_tracts.c - White Matter Tract System Implementation
//=============================================================================
/**
 * @file nimcp_white_matter_tracts.c
 * @brief Core implementation of white matter tract modeling
 *
 * WHAT: Implements myelination-dependent conduction, pink noise jitter,
 *       signal routing with attenuation, and tract integrity dynamics
 * WHY:  Inter-region communication in the brain relies on myelinated fiber
 *       bundles whose conduction velocity directly impacts cognitive speed
 * HOW:  Per-tract state with biologically-calibrated defaults, mutex-protected
 *       updates, pink noise for velocity jitter, isfinite guards on all floats
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2026-03-05
 */

//=============================================================================
// Includes
//=============================================================================

#include "core/brain/white_matter/nimcp_white_matter_tracts.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/math/nimcp_math_helpers.h"
#include "utils/thread/nimcp_thread.h"
#include "plasticity/noise/nimcp_pink_noise.h"

#include <math.h>
#include <string.h>
#include <time.h>

#define LOG_MODULE "WHITE_MATTER"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(white_matter_tracts, MESH_ADAPTER_CATEGORY_SYSTEM)

//=============================================================================
// Internal System Structure
//=============================================================================

struct wmt_system {
    uint32_t magic;                     /* WMT_MAGIC for validation */
    wmt_config_t config;                /* Configuration snapshot */
    tract_state_t tracts[WMT_COUNT];    /* Per-tract state */
    pink_noise_generator_t pink_noise;  /* Velocity jitter generator */
    float fractal_dimension;            /* Fractal dimension of tract branching */
    nimcp_mutex_t* lock;                /* Thread safety mutex */
    uint64_t last_update_us;            /* Last update timestamp (microseconds) */
    uint64_t total_signals_routed;      /* Cumulative signal routing count */
    uint64_t total_updates;             /* Total update cycles */
};

//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * @brief Get current time in microseconds
 */
static uint64_t wmt_get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Compute conduction velocity from myelination level
 *
 * WHAT: Maps myelination [0,1] to velocity [min, max] m/s
 * WHY:  Myelination enables saltatory conduction (exponential speedup)
 * HOW:  Quadratic interpolation: v = min + (max - min) * myelin^2
 *       Quadratic because myelination has diminishing returns at high levels
 */
static float wmt_compute_velocity(float myelination, float min_vel, float max_vel) {
    float m = nimcp_clampf(myelination, 0.0f, 1.0f);
    return min_vel + (max_vel - min_vel) * m * m;
}

/**
 * @brief Recompute signal delay from tract length and velocity
 */
static float wmt_compute_delay(float length_m, float velocity_ms) {
    if (velocity_ms < 0.001f) {
        return 1000.0f;  /* Cap at 1 second for near-zero velocity */
    }
    return (length_m / velocity_ms) * 1000.0f;  /* Convert to milliseconds */
}

/**
 * @brief Initialize a single tract with anatomical defaults
 */
static void wmt_init_tract(tract_state_t* tract, white_matter_tract_t type,
                           float base_myelination, float base_integrity,
                           float min_vel, float max_vel) {
    tract->type = type;
    tract->myelination_level = nimcp_clampf(base_myelination, 0.0f, 1.0f);
    tract->integrity = nimcp_clampf(base_integrity, 0.0f, 1.0f);
    tract->bidirectional = true;  /* Most tracts are bidirectional */

    /* Set anatomical lengths */
    switch (type) {
        case WMT_CORPUS_CALLOSUM:
            tract->tract_length_m = WMT_LENGTH_CORPUS_CALLOSUM;
            tract->source_region = 0;  /* Left hemisphere */
            tract->target_region = 1;  /* Right hemisphere */
            tract->bandwidth = 1.0f;   /* Highest bandwidth (200M+ axons) */
            break;
        case WMT_ARCUATE_FASCICULUS:
            tract->tract_length_m = WMT_LENGTH_ARCUATE_FASCICULUS;
            tract->source_region = 10;  /* Broca's area */
            tract->target_region = 11;  /* Wernicke's area */
            tract->bandwidth = 0.7f;
            break;
        case WMT_UNCINATE_FASCICULUS:
            tract->tract_length_m = WMT_LENGTH_UNCINATE_FASCICULUS;
            tract->source_region = 20;  /* Temporal lobe */
            tract->target_region = 21;  /* Frontal lobe */
            tract->bandwidth = 0.6f;
            break;
        case WMT_CINGULUM:
            tract->tract_length_m = WMT_LENGTH_CINGULUM;
            tract->source_region = 30;  /* Cingulate cortex (anterior) */
            tract->target_region = 31;  /* Cingulate cortex (posterior) */
            tract->bandwidth = 0.65f;
            break;
        case WMT_IFOF:
            tract->tract_length_m = WMT_LENGTH_IFOF;
            tract->source_region = 40;  /* Frontal (inferior) */
            tract->target_region = 41;  /* Occipital */
            tract->bandwidth = 0.55f;
            break;
        case WMT_CORTICOSPINAL:
            tract->tract_length_m = WMT_LENGTH_CORTICOSPINAL;
            tract->source_region = 50;  /* Motor cortex */
            tract->target_region = 51;  /* Spinal cord */
            tract->bandwidth = 0.8f;
            tract->bidirectional = false;  /* Primarily descending */
            break;
        case WMT_SPINOTHALAMIC:
            tract->tract_length_m = WMT_LENGTH_SPINOTHALAMIC;
            tract->source_region = 60;  /* Spinal cord */
            tract->target_region = 61;  /* Thalamus */
            tract->bandwidth = 0.5f;
            tract->bidirectional = false;  /* Primarily ascending */
            break;
        case WMT_OPTIC_RADIATION:
            tract->tract_length_m = WMT_LENGTH_OPTIC_RADIATION;
            tract->source_region = 70;  /* LGN (lateral geniculate nucleus) */
            tract->target_region = 71;  /* V1 (primary visual cortex) */
            tract->bandwidth = 0.9f;    /* High bandwidth for visual stream */
            tract->bidirectional = false;  /* Primarily LGN -> V1 */
            break;
        default:
            tract->tract_length_m = 0.10f;
            tract->source_region = 0;
            tract->target_region = 0;
            tract->bandwidth = 0.5f;
            break;
    }

    /* Compute initial velocity and delay */
    tract->conduction_velocity_ms = wmt_compute_velocity(
        tract->myelination_level, min_vel, max_vel);
    tract->signal_delay_ms = wmt_compute_delay(
        tract->tract_length_m, tract->conduction_velocity_ms);
}

//=============================================================================
// Public API - Lifecycle
//=============================================================================

wmt_config_t wmt_default_config(void) {
    wmt_config_t config;
    memset(&config, 0, sizeof(config));

    config.base_myelination = 0.7f;
    config.base_integrity = 1.0f;
    config.velocity_noise_amplitude = 0.02f;
    config.velocity_noise_alpha = 1.0f;
    config.demyelination_rate = 0.001f;
    config.remyelination_rate = 0.005f;
    config.min_conduction_velocity = 1.0f;
    config.max_conduction_velocity = 120.0f;
    config.fractal_dimension = 1.3f;
    config.enable_velocity_jitter = true;
    config.enable_integrity_decay = false;
    config.pink_noise_seed = 0;

    return config;
}

wmt_system_t* wmt_create(const wmt_config_t* config) {
    wmt_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        cfg = wmt_default_config();
    }

    /* Allocate system */
    wmt_system_t* system = (wmt_system_t*)nimcp_calloc(1, sizeof(wmt_system_t));
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_MEMORY,
            "wmt_create: failed to allocate wmt_system_t");
        return NULL;
    }

    system->magic = WMT_MAGIC;
    system->config = cfg;
    system->fractal_dimension = cfg.fractal_dimension;
    system->total_signals_routed = 0;
    system->total_updates = 0;

    /* Create mutex */
    system->lock = nimcp_mutex_create(NULL);
    if (!system->lock) {
        NIMCP_LOGGING_WARN("wmt_create: failed to create mutex, continuing without thread safety");
    }

    /* Initialize all tracts */
    for (int i = 0; i < WMT_COUNT; i++) {
        wmt_init_tract(&system->tracts[i], (white_matter_tract_t)i,
                       cfg.base_myelination, cfg.base_integrity,
                       cfg.min_conduction_velocity, cfg.max_conduction_velocity);
    }

    /* Create pink noise generator for velocity jitter */
    system->pink_noise = NULL;
    if (cfg.enable_velocity_jitter) {
        pink_noise_config_t pn_config = pink_noise_default_config();
        pn_config.alpha = cfg.velocity_noise_alpha;
        pn_config.amplitude = cfg.velocity_noise_amplitude;
        pn_config.method = PINK_NOISE_VOSS;
        pn_config.seed = cfg.pink_noise_seed;

        system->pink_noise = pink_noise_create(&pn_config);
        if (!system->pink_noise) {
            NIMCP_LOGGING_WARN("wmt_create: failed to create pink noise generator, "
                              "velocity jitter disabled");
            system->config.enable_velocity_jitter = false;
        }
    }

    system->last_update_us = wmt_get_time_us();

    NIMCP_LOGGING_INFO("White matter tract system created: "
                      "%d tracts, myelination=%.2f, integrity=%.2f, jitter=%s",
                      WMT_COUNT, cfg.base_myelination, cfg.base_integrity,
                      cfg.enable_velocity_jitter ? "enabled" : "disabled");

    return system;
}

void wmt_destroy(wmt_system_t* system) {
    if (!system) {
        return;
    }

    if (system->magic != WMT_MAGIC) {
        NIMCP_LOGGING_WARN("wmt_destroy: invalid magic (corrupted or double-free)");
        return;
    }

    /* Invalidate magic to detect double-free */
    system->magic = 0;

    /* Destroy pink noise generator */
    if (system->pink_noise) {
        pink_noise_destroy(system->pink_noise);
        system->pink_noise = NULL;
    }

    /* Destroy mutex */
    if (system->lock) {
        nimcp_mutex_destroy(system->lock);
        system->lock = NULL;
    }

    nimcp_free(system);

    NIMCP_LOGGING_DEBUG("White matter tract system destroyed");
}

//=============================================================================
// Public API - Update
//=============================================================================

int wmt_update(wmt_system_t* system, float dt_s) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "wmt_update: system is NULL");
        return -1;
    }

    if (system->magic != WMT_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
            "wmt_update: invalid magic");
        return -1;
    }

    if (!isfinite(dt_s) || dt_s <= 0.0f) {
        return -1;  /* Invalid timestep, silent skip */
    }

    if (system->lock) {
        nimcp_mutex_lock(system->lock);
    }

    for (int i = 0; i < WMT_COUNT; i++) {
        tract_state_t* tract = &system->tracts[i];

        /* Apply optional integrity decay */
        if (system->config.enable_integrity_decay) {
            float decay = system->config.demyelination_rate * dt_s;
            if (isfinite(decay)) {
                tract->integrity = nimcp_clampf(tract->integrity - decay, 0.0f, 1.0f);
            }
        }

        /* Apply pink noise jitter to conduction velocity */
        float noise = 0.0f;
        if (system->config.enable_velocity_jitter && system->pink_noise) {
            bool ok = pink_noise_generate_sample(system->pink_noise, &noise);
            if (!ok || !isfinite(noise)) {
                noise = 0.0f;
            }
        }

        /* Recompute velocity from myelination + noise */
        float base_velocity = wmt_compute_velocity(
            tract->myelination_level,
            system->config.min_conduction_velocity,
            system->config.max_conduction_velocity);

        float jittered_velocity = base_velocity * (1.0f + noise);
        if (!isfinite(jittered_velocity)) {
            jittered_velocity = base_velocity;
        }

        tract->conduction_velocity_ms = nimcp_clampf(
            jittered_velocity,
            system->config.min_conduction_velocity,
            system->config.max_conduction_velocity);

        /* Scale velocity by integrity (damaged tracts conduct slower) */
        float effective_velocity = tract->conduction_velocity_ms * tract->integrity;
        if (!isfinite(effective_velocity) || effective_velocity < 0.001f) {
            effective_velocity = system->config.min_conduction_velocity;
        }

        /* Recompute delay */
        tract->signal_delay_ms = wmt_compute_delay(tract->tract_length_m, effective_velocity);
        if (!isfinite(tract->signal_delay_ms)) {
            tract->signal_delay_ms = 1000.0f;  /* Fallback: 1 second */
        }
    }

    system->last_update_us = wmt_get_time_us();
    system->total_updates++;

    if (system->lock) {
        nimcp_mutex_unlock(system->lock);
    }

    return 0;
}

//=============================================================================
// Public API - Query
//=============================================================================

float wmt_get_tract_delay(const wmt_system_t* system, white_matter_tract_t tract) {
    if (!system || system->magic != WMT_MAGIC) {
        return -1.0f;
    }
    if (tract < 0 || tract >= WMT_COUNT) {
        return -1.0f;
    }
    return system->tracts[tract].signal_delay_ms;
}

float wmt_get_tract_integrity(const wmt_system_t* system, white_matter_tract_t tract) {
    if (!system || system->magic != WMT_MAGIC) {
        return -1.0f;
    }
    if (tract < 0 || tract >= WMT_COUNT) {
        return -1.0f;
    }
    return system->tracts[tract].integrity;
}

float wmt_get_tract_myelination(const wmt_system_t* system, white_matter_tract_t tract) {
    if (!system || system->magic != WMT_MAGIC) {
        return -1.0f;
    }
    if (tract < 0 || tract >= WMT_COUNT) {
        return -1.0f;
    }
    return system->tracts[tract].myelination_level;
}

float wmt_get_tract_velocity(const wmt_system_t* system, white_matter_tract_t tract) {
    if (!system || system->magic != WMT_MAGIC) {
        return -1.0f;
    }
    if (tract < 0 || tract >= WMT_COUNT) {
        return -1.0f;
    }
    return system->tracts[tract].conduction_velocity_ms;
}

int wmt_get_tract_state(const wmt_system_t* system, white_matter_tract_t tract,
                        tract_state_t* out_state) {
    if (!system || system->magic != WMT_MAGIC) {
        return -1;
    }
    if (tract < 0 || tract >= WMT_COUNT) {
        return -1;
    }
    if (!out_state) {
        return -1;
    }
    *out_state = system->tracts[tract];
    return 0;
}

int wmt_get_stats(const wmt_system_t* system, wmt_stats_t* out_stats) {
    if (!system || system->magic != WMT_MAGIC || !out_stats) {
        return -1;
    }

    memset(out_stats, 0, sizeof(wmt_stats_t));

    float sum_myelin = 0.0f;
    float sum_integrity = 0.0f;
    float sum_velocity = 0.0f;
    float sum_delay = 0.0f;

    for (int i = 0; i < WMT_COUNT; i++) {
        const tract_state_t* t = &system->tracts[i];
        sum_myelin += t->myelination_level;
        sum_integrity += t->integrity;
        sum_velocity += t->conduction_velocity_ms;
        sum_delay += t->signal_delay_ms;

        if (t->myelination_level < 0.3f) {
            out_stats->demyelinated_tract_count++;
        }
        if (t->integrity < 0.5f) {
            out_stats->degraded_tract_count++;
        }
    }

    float inv_count = 1.0f / (float)WMT_COUNT;
    out_stats->mean_myelination = sum_myelin * inv_count;
    out_stats->mean_integrity = sum_integrity * inv_count;
    out_stats->mean_conduction_velocity = sum_velocity * inv_count;
    out_stats->mean_signal_delay_ms = sum_delay * inv_count;
    out_stats->total_signals_routed = system->total_signals_routed;
    out_stats->total_updates = system->total_updates;

    return 0;
}

//=============================================================================
// Public API - Modulation
//=============================================================================

int wmt_modulate_myelination(wmt_system_t* system, white_matter_tract_t tract,
                             float delta_myelination) {
    if (!system || system->magic != WMT_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "wmt_modulate_myelination: invalid system");
        return -1;
    }
    if (tract < 0 || tract >= WMT_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT,
            "wmt_modulate_myelination: invalid tract index");
        return -1;
    }
    if (!isfinite(delta_myelination)) {
        return -1;
    }

    if (system->lock) {
        nimcp_mutex_lock(system->lock);
    }

    tract_state_t* t = &system->tracts[tract];
    t->myelination_level = nimcp_clampf(
        t->myelination_level + delta_myelination, 0.0f, 1.0f);

    /* Recompute velocity and delay after myelination change */
    t->conduction_velocity_ms = wmt_compute_velocity(
        t->myelination_level,
        system->config.min_conduction_velocity,
        system->config.max_conduction_velocity);

    float effective_velocity = t->conduction_velocity_ms * t->integrity;
    if (!isfinite(effective_velocity) || effective_velocity < 0.001f) {
        effective_velocity = system->config.min_conduction_velocity;
    }

    t->signal_delay_ms = wmt_compute_delay(t->tract_length_m, effective_velocity);
    if (!isfinite(t->signal_delay_ms)) {
        t->signal_delay_ms = 1000.0f;
    }

    if (system->lock) {
        nimcp_mutex_unlock(system->lock);
    }

    return 0;
}

int wmt_modulate_integrity(wmt_system_t* system, white_matter_tract_t tract,
                           float delta_integrity) {
    if (!system || system->magic != WMT_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "wmt_modulate_integrity: invalid system");
        return -1;
    }
    if (tract < 0 || tract >= WMT_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT,
            "wmt_modulate_integrity: invalid tract index");
        return -1;
    }
    if (!isfinite(delta_integrity)) {
        return -1;
    }

    if (system->lock) {
        nimcp_mutex_lock(system->lock);
    }

    tract_state_t* t = &system->tracts[tract];
    t->integrity = nimcp_clampf(t->integrity + delta_integrity, 0.0f, 1.0f);

    /* Recompute delay with new integrity */
    float effective_velocity = t->conduction_velocity_ms * t->integrity;
    if (!isfinite(effective_velocity) || effective_velocity < 0.001f) {
        effective_velocity = system->config.min_conduction_velocity;
    }

    t->signal_delay_ms = wmt_compute_delay(t->tract_length_m, effective_velocity);
    if (!isfinite(t->signal_delay_ms)) {
        t->signal_delay_ms = 1000.0f;
    }

    if (system->lock) {
        nimcp_mutex_unlock(system->lock);
    }

    return 0;
}

//=============================================================================
// Public API - Signal Routing
//=============================================================================

int wmt_route_signal(wmt_system_t* system, white_matter_tract_t tract,
                     float signal_amplitude, float* out_attenuated_amplitude,
                     float* out_delay_ms) {
    if (!system || system->magic != WMT_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "wmt_route_signal: invalid system");
        return -1;
    }
    if (tract < 0 || tract >= WMT_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT,
            "wmt_route_signal: invalid tract index");
        return -1;
    }
    if (!out_attenuated_amplitude || !out_delay_ms) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "wmt_route_signal: NULL output parameter");
        return -1;
    }
    if (!isfinite(signal_amplitude)) {
        *out_attenuated_amplitude = 0.0f;
        *out_delay_ms = 0.0f;
        return -1;
    }

    if (system->lock) {
        nimcp_mutex_lock(system->lock);
    }

    const tract_state_t* t = &system->tracts[tract];

    /* Attenuation: signal * integrity * bandwidth */
    float attenuation = t->integrity * t->bandwidth;
    if (!isfinite(attenuation)) {
        attenuation = 0.0f;
    }
    *out_attenuated_amplitude = signal_amplitude * attenuation;
    *out_delay_ms = t->signal_delay_ms;

    system->total_signals_routed++;

    if (system->lock) {
        nimcp_mutex_unlock(system->lock);
    }

    return 0;
}

//=============================================================================
// Public API - Utility
//=============================================================================

const char* wmt_tract_name(white_matter_tract_t tract) {
    switch (tract) {
        case WMT_CORPUS_CALLOSUM:    return "Corpus Callosum";
        case WMT_ARCUATE_FASCICULUS: return "Arcuate Fasciculus";
        case WMT_UNCINATE_FASCICULUS: return "Uncinate Fasciculus";
        case WMT_CINGULUM:           return "Cingulum";
        case WMT_IFOF:               return "IFOF";
        case WMT_CORTICOSPINAL:      return "Corticospinal Tract";
        case WMT_SPINOTHALAMIC:      return "Spinothalamic Tract";
        case WMT_OPTIC_RADIATION:    return "Optic Radiation";
        default:                     return "UNKNOWN";
    }
}
