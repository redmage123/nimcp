/**
 * @file nimcp_claustrum.c
 * @brief Claustrum Module Implementation - Consciousness Integration and Cross-Modal Binding
 *
 * WHAT: Implementation of claustrum for consciousness binding and cross-modal integration
 * WHY:  The claustrum is hypothesized to be the "conductor of consciousness" (Crick & Koch)
 * HOW:  Model cross-modal binding, salience detection, attention coordination, global workspace gating
 *
 * @version 1.0.0
 * @date 2026-01-13
 */

#include "core/brain/regions/claustrum/nimcp_claustrum.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(claustrum, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * LOGGING MODULE IDENTIFIER
 *===========================================================================*/

#define CLAUSTRUM_LOG_MODULE "CLAUSTRUM"

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define PI NIMCP_PI_F
#define TWO_PI (2.0f * PI)

/* Default feature dimension for modalities */
#define DEFAULT_FEATURE_DIM 64

/* Decay rates */
#define BINDING_DECAY_RATE 0.01f
#define COHERENCE_DECAY_RATE 0.05f
#define ACTIVITY_DECAY_RATE 0.1f

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Simple exponential decay toward target
 */
static inline float exponential_decay(float current, float target, float rate, float dt) {
    return current + (target - current) * (1.0f - expf(-rate * dt));
}

/**
 * @brief Get current time in microseconds (simplified)
 */
static uint64_t get_time_us(void) {
    /* In production, use platform-specific high-resolution timer */
    static uint64_t time_counter = 0;
    return time_counter++;
}

/**
 * @brief Initialize a modality input structure
 */
static void init_modality(nimcp_claustrum_modality_input_t* modality,
                          nimcp_claustrum_modality_t type,
                          const char* name) {
    memset(modality, 0, sizeof(*modality));
    modality->type = type;
    if (name) {
        strncpy(modality->name, name, sizeof(modality->name) - 1);
    }
    modality->active = false;
    modality->bound = false;
    modality->features = NULL;
    modality->feature_dim = 0;
}

/**
 * @brief Initialize a cortical link structure
 */
static void init_cortical_link(nimcp_claustrum_cortical_link_t* link,
                               nimcp_claustrum_region_t region,
                               const char* name) {
    memset(link, 0, sizeof(*link));
    link->region = region;
    if (name) {
        strncpy(link->region_name, name, sizeof(link->region_name) - 1);
    }
    link->forward_strength = 0.5f;
    link->backward_strength = 0.5f;
    link->active = true;
}

/**
 * @brief Initialize oscillator state
 */
static void init_oscillator(nimcp_claustrum_oscillator_t* osc,
                            const nimcp_claustrum_config_t* config) {
    osc->gamma_frequency = config->gamma_base_freq;
    osc->gamma_amplitude = 0.5f;
    osc->gamma_phase = 0.0f;

    osc->alpha_frequency = config->alpha_base_freq;
    osc->alpha_amplitude = 0.5f;
    osc->alpha_phase = 0.0f;

    osc->phase_amplitude_coupling = config->oscillation_coupling;
    osc->global_coherence = 0.5f;
    osc->binding_coherence = 0.5f;
}

/**
 * @brief Update oscillator state
 */
static void update_oscillator(nimcp_claustrum_oscillator_t* osc, float dt_ms) {
    float dt_s = dt_ms / 1000.0f;

    /* Update gamma phase */
    osc->gamma_phase += TWO_PI * osc->gamma_frequency * dt_s;
    while (osc->gamma_phase >= TWO_PI) {
        osc->gamma_phase -= TWO_PI;
    }

    /* Update alpha phase */
    osc->alpha_phase += TWO_PI * osc->alpha_frequency * dt_s;
    while (osc->alpha_phase >= TWO_PI) {
        osc->alpha_phase -= TWO_PI;
    }

    /* Update binding coherence based on gamma amplitude */
    osc->binding_coherence = exponential_decay(
        osc->binding_coherence,
        osc->gamma_amplitude,
        0.1f, dt_ms);
}

/**
 * @brief Compute coherence between two modalities
 */
static float compute_modality_coherence(const nimcp_claustrum_modality_input_t* m1,
                                        const nimcp_claustrum_modality_input_t* m2) {
    if (!m1->active || !m2->active) return 0.0f;

    /* Phase difference contribution */
    float phase_diff = fabsf(m1->phase - m2->phase);
    if (phase_diff > PI) phase_diff = TWO_PI - phase_diff;
    float phase_coherence = 1.0f - (phase_diff / PI);

    /* Activity similarity contribution */
    float activity_similarity = 1.0f - fabsf(m1->activity_level - m2->activity_level);

    /* Combined coherence */
    return nimcp_clamp01(0.5f * phase_coherence + 0.5f * activity_similarity);
}

/**
 * @brief Find empty percept slot
 */
static int find_empty_percept_slot(const nimcp_claustrum_t* claustrum) {
    for (int i = 0; i < CLAUSTRUM_MAX_BOUND_PERCEPTS; i++) {
        if (!claustrum->percepts[i].valid) {
            return i;
        }
    }
    return -1;  /* No empty slot found - normal "not found" return, not an error */
}

/**
 * @brief Find percept by ID
 */
static int find_percept_by_id(const nimcp_claustrum_t* claustrum, uint32_t id) {
    for (int i = 0; i < CLAUSTRUM_MAX_BOUND_PERCEPTS; i++) {
        if (claustrum->percepts[i].valid && claustrum->percepts[i].id == id) {
            return i;
        }
    }
    return -1;  /* Percept not found - normal "not found" return, not an error */
}

/**
 * @brief Create new bound percept
 */
static nimcp_claustrum_error_t create_percept(nimcp_claustrum_t* claustrum,
                                              uint32_t modality_mask,
                                              uint32_t* percept_id) {
    int slot = find_empty_percept_slot(claustrum);
    if (slot < 0) {
        return CLAUSTRUM_ERR_CAPACITY_EXCEEDED;
    }

    nimcp_claustrum_bound_percept_t* percept = &claustrum->percepts[slot];
    memset(percept, 0, sizeof(*percept));

    percept->id = claustrum->next_percept_id++;
    percept->modality_mask = modality_mask;
    percept->valid = true;
    percept->creation_time_us = get_time_us();
    percept->last_update_us = percept->creation_time_us;

    /* Count modalities */
    uint32_t count = 0;
    for (int i = 0; i < CLAUSTRUM_MODALITY_COUNT; i++) {
        if (modality_mask & (1U << i)) count++;
    }
    percept->num_modalities = count;

    /* Compute initial binding strength based on coherence */
    float total_coherence = 0.0f;
    int pairs = 0;
    for (int i = 0; i < CLAUSTRUM_MODALITY_COUNT; i++) {
        if (!(modality_mask & (1U << i))) continue;
        for (int j = i + 1; j < CLAUSTRUM_MODALITY_COUNT; j++) {
            if (!(modality_mask & (1U << j))) continue;
            total_coherence += compute_modality_coherence(
                &claustrum->modalities[i], &claustrum->modalities[j]);
            pairs++;
        }
    }
    percept->coherence = (pairs > 0) ? (total_coherence / pairs) : 0.5f;
    percept->binding_strength = percept->coherence;

    /* Compute salience from constituent modalities */
    float total_salience = 0.0f;
    for (int i = 0; i < CLAUSTRUM_MODALITY_COUNT; i++) {
        if (modality_mask & (1U << i)) {
            total_salience += claustrum->modalities[i].salience;
        }
    }
    percept->salience = (count > 0) ? (total_salience / count) : 0.0f;

    /* Initial consciousness level based on salience */
    if (percept->salience > 0.8f) {
        percept->consciousness_level = CLAUSTRUM_CONSCIOUSNESS_CONSCIOUS;
    } else if (percept->salience > 0.4f) {
        percept->consciousness_level = CLAUSTRUM_CONSCIOUSNESS_PRECONSCIOUS;
    } else {
        percept->consciousness_level = CLAUSTRUM_CONSCIOUSNESS_UNCONSCIOUS;
    }

    /* Mark modalities as bound */
    for (int i = 0; i < CLAUSTRUM_MODALITY_COUNT; i++) {
        if (modality_mask & (1U << i)) {
            claustrum->modalities[i].bound = true;
            claustrum->modalities[i].binding_id = percept->id;
        }
    }

    claustrum->num_active_percepts++;
    *percept_id = percept->id;

    claustrum->metrics.total_bindings++;
    claustrum->metrics.successful_bindings++;

    LOG_MODULE_DEBUG(CLAUSTRUM_LOG_MODULE,
              "Created percept %u with %u modalities, coherence=%.2f, salience=%.2f",
              percept->id, count, percept->coherence, percept->salience);

    return CLAUSTRUM_OK;
}

/**
 * @brief Update existing percept
 */
static void update_percept(nimcp_claustrum_t* claustrum,
                           nimcp_claustrum_bound_percept_t* percept,
                           float dt_ms) {
    if (!percept->valid) return;

    /* Update duration */
    percept->duration_ms += dt_ms;
    percept->last_update_us = get_time_us();

    /* Decay binding strength over time */
    percept->binding_strength = exponential_decay(
        percept->binding_strength, 0.0f,
        claustrum->config.binding_decay_rate, dt_ms);

    /* Recompute coherence from current modality states */
    float total_coherence = 0.0f;
    int pairs = 0;
    for (int i = 0; i < CLAUSTRUM_MODALITY_COUNT; i++) {
        if (!(percept->modality_mask & (1U << i))) continue;
        if (!claustrum->modalities[i].active) {
            /* Modality went inactive - reduce coherence */
            total_coherence += 0.0f;
            pairs++;
            continue;
        }
        for (int j = i + 1; j < CLAUSTRUM_MODALITY_COUNT; j++) {
            if (!(percept->modality_mask & (1U << j))) continue;
            total_coherence += compute_modality_coherence(
                &claustrum->modalities[i], &claustrum->modalities[j]);
            pairs++;
        }
    }
    if (pairs > 0) {
        percept->coherence = exponential_decay(
            percept->coherence, total_coherence / pairs, 0.1f, dt_ms);
    }

    /* Update stability based on coherence history */
    percept->stability = exponential_decay(
        percept->stability, percept->coherence, 0.05f, dt_ms);

    /* Update consciousness level based on salience and workspace access */
    if (percept->in_workspace) {
        percept->consciousness_level = CLAUSTRUM_CONSCIOUSNESS_FOCAL;
        percept->access_strength = 1.0f;
    } else if (percept->salience > claustrum->config.workspace_threshold) {
        percept->consciousness_level = CLAUSTRUM_CONSCIOUSNESS_CONSCIOUS;
        percept->access_strength = percept->salience;
    } else if (percept->binding_strength > 0.3f) {
        percept->consciousness_level = CLAUSTRUM_CONSCIOUSNESS_PRECONSCIOUS;
        percept->access_strength = percept->binding_strength * 0.5f;
    } else {
        percept->consciousness_level = CLAUSTRUM_CONSCIOUSNESS_UNCONSCIOUS;
        percept->access_strength = 0.0f;
    }

    /* Check if percept should be invalidated */
    if (percept->binding_strength < 0.1f && percept->coherence < 0.2f) {
        percept->valid = false;
        claustrum->num_active_percepts--;

        /* Unbind modalities */
        for (int i = 0; i < CLAUSTRUM_MODALITY_COUNT; i++) {
            if ((percept->modality_mask & (1U << i)) &&
                claustrum->modalities[i].binding_id == percept->id) {
                claustrum->modalities[i].bound = false;
                claustrum->modalities[i].binding_id = 0;
            }
        }

        LOG_MODULE_DEBUG(CLAUSTRUM_LOG_MODULE,
                  "Percept %u invalidated (binding=%.2f, coherence=%.2f)",
                  percept->id, percept->binding_strength, percept->coherence);
    }
}

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

nimcp_claustrum_config_t nimcp_claustrum_default_config(void) {
    nimcp_claustrum_config_t config;
    memset(&config, 0, sizeof(config));

    /* Binding parameters */
    config.binding_threshold = CLAUSTRUM_DEFAULT_BINDING_THRESHOLD;
    config.binding_decay_rate = BINDING_DECAY_RATE;
    config.temporal_window_ms = CLAUSTRUM_DEFAULT_SYNC_WINDOW_MS;

    /* Salience parameters */
    config.salience_threshold = CLAUSTRUM_DEFAULT_SALIENCE_THRESHOLD;
    config.salience_gain = 1.5f;
    config.novelty_weight = 0.4f;
    config.relevance_weight = 0.6f;

    /* Oscillation parameters */
    config.gamma_base_freq = CLAUSTRUM_GAMMA_FREQUENCY_HZ;
    config.alpha_base_freq = CLAUSTRUM_ALPHA_FREQUENCY_HZ;
    config.oscillation_coupling = 0.3f;

    /* Global workspace */
    config.workspace_threshold = 0.7f;
    config.broadcast_duration_ms = 200.0f;
    config.enable_workspace_gating = true;

    /* Task switching */
    config.switch_threshold = 0.6f;
    config.switch_duration_ms = 150.0f;
    config.enable_rapid_switching = true;

    /* Platform tier - medium by default */
    config.min_tier = 2;  /* PLATFORM_TIER_MEDIUM */

    /* Callbacks */
    config.on_binding = NULL;
    config.on_state_change = NULL;
    config.on_consciousness_change = NULL;
    config.on_workspace_broadcast = NULL;
    config.callback_data = NULL;

    /* Integration features */
    config.enable_immune_reporting = true;
    config.enable_logging = true;
    config.enable_kg_integration = false;
    config.enable_snn_output = false;

    return config;
}

nimcp_claustrum_error_t nimcp_claustrum_init(
    nimcp_claustrum_t* claustrum,
    const nimcp_claustrum_config_t* config
) {
    if (!claustrum) {
        return CLAUSTRUM_ERR_NULL_PTR;
    }

    if (claustrum->initialized) {
        return CLAUSTRUM_ERR_ALREADY_INITIALIZED;
    }

    LOG_MODULE_INFO(CLAUSTRUM_LOG_MODULE, "Initializing claustrum system");

    memset(claustrum, 0, sizeof(*claustrum));

    /* Set configuration */
    if (config) {
        claustrum->config = *config;
    } else {
        claustrum->config = nimcp_claustrum_default_config();
    }

    /* Initialize modalities */
    const char* modality_names[] = {
        "Visual", "Auditory", "Somatosensory", "Olfactory",
        "Gustatory", "Interoceptive", "Vestibular", "Motor"
    };
    for (int i = 0; i < CLAUSTRUM_MODALITY_COUNT; i++) {
        init_modality(&claustrum->modalities[i], (nimcp_claustrum_modality_t)i,
                      modality_names[i]);
    }

    /* Initialize cortical links */
    const char* region_names[] = {
        "Prefrontal", "Cingulate", "Insula", "Parietal", "Temporal",
        "Occipital", "Motor", "Somatosensory", "Auditory",
        "Hippocampus", "Amygdala"
    };
    for (int i = 0; i < CLAUSTRUM_REGION_COUNT; i++) {
        init_cortical_link(&claustrum->cortical_links[i],
                           (nimcp_claustrum_region_t)i, region_names[i]);
    }
    claustrum->active_region_mask = (1U << CLAUSTRUM_REGION_COUNT) - 1;

    /* Initialize oscillator */
    init_oscillator(&claustrum->oscillator, &claustrum->config);

    /* Initialize state */
    claustrum->state = CLAUSTRUM_STATE_IDLE;
    claustrum->status = CLAUSTRUM_STATUS_NORMAL;
    claustrum->brain_state = CLAUSTRUM_BRAIN_STATE_DEFAULT;

    /* Initialize workspace */
    claustrum->workspace_occupied = false;
    claustrum->workspace_percept_id = 0;
    claustrum->workspace_access_level = 0.0f;

    /* Initialize attention */
    claustrum->global_attention = 0.5f;
    for (int i = 0; i < 3; i++) claustrum->spatial_attention[i] = 0.0f;
    for (int i = 0; i < 8; i++) claustrum->feature_attention[i] = 0.0f;

    /* Initialize timing */
    claustrum->current_time_ms = 0.0f;
    claustrum->creation_time_us = get_time_us();
    claustrum->last_update_us = claustrum->creation_time_us;

    /* Initialize percept tracking */
    claustrum->num_active_percepts = 0;
    claustrum->next_percept_id = 1;

    /* Mark as initialized */
    claustrum->initialized = true;
    claustrum->running = true;

    LOG_MODULE_INFO(CLAUSTRUM_LOG_MODULE,
             "Claustrum initialized: binding_threshold=%.2f, gamma=%.1fHz, alpha=%.1fHz",
             claustrum->config.binding_threshold,
             claustrum->config.gamma_base_freq,
             claustrum->config.alpha_base_freq);

    return CLAUSTRUM_OK;
}

nimcp_claustrum_error_t nimcp_claustrum_shutdown(nimcp_claustrum_t* claustrum) {
    if (!claustrum) {
        return CLAUSTRUM_ERR_NULL_PTR;
    }

    if (!claustrum->initialized) {
        return CLAUSTRUM_ERR_NOT_INITIALIZED;
    }

    LOG_MODULE_INFO(CLAUSTRUM_LOG_MODULE, "Shutting down claustrum system");

    /* Free modality feature arrays */
    for (int i = 0; i < CLAUSTRUM_MODALITY_COUNT; i++) {
        if (claustrum->modalities[i].features) {
            nimcp_free(claustrum->modalities[i].features);
            claustrum->modalities[i].features = NULL;
        }
    }

    /* Log final metrics */
    LOG_MODULE_INFO(CLAUSTRUM_LOG_MODULE,
             "Final metrics: bindings=%lu, workspace_accesses=%lu, state_switches=%lu",
             (unsigned long)claustrum->metrics.total_bindings,
             (unsigned long)claustrum->metrics.workspace_accesses,
             (unsigned long)claustrum->metrics.state_switches);

    claustrum->initialized = false;
    claustrum->running = false;

    return CLAUSTRUM_OK;
}

nimcp_claustrum_error_t nimcp_claustrum_reset(nimcp_claustrum_t* claustrum) {
    if (!claustrum) {
        return CLAUSTRUM_ERR_NULL_PTR;
    }

    if (!claustrum->initialized) {
        return CLAUSTRUM_ERR_NOT_INITIALIZED;
    }

    LOG_MODULE_DEBUG(CLAUSTRUM_LOG_MODULE, "Resetting claustrum state");

    /* Reset modality states (preserve feature arrays) */
    for (int i = 0; i < CLAUSTRUM_MODALITY_COUNT; i++) {
        claustrum->modalities[i].activity_level = 0.0f;
        claustrum->modalities[i].salience = 0.0f;
        claustrum->modalities[i].active = false;
        claustrum->modalities[i].bound = false;
        claustrum->modalities[i].phase = 0.0f;
        claustrum->modalities[i].coherence = 0.0f;
    }

    /* Reset percepts */
    for (int i = 0; i < CLAUSTRUM_MAX_BOUND_PERCEPTS; i++) {
        claustrum->percepts[i].valid = false;
    }
    claustrum->num_active_percepts = 0;

    /* Reset cortical links */
    for (int i = 0; i < CLAUSTRUM_REGION_COUNT; i++) {
        claustrum->cortical_links[i].activity = 0.0f;
        claustrum->cortical_links[i].attention_bias = 0.0f;
        claustrum->cortical_links[i].synchronization = 0.0f;
    }

    /* Reset oscillator */
    init_oscillator(&claustrum->oscillator, &claustrum->config);

    /* Reset state */
    claustrum->state = CLAUSTRUM_STATE_IDLE;
    claustrum->status = CLAUSTRUM_STATUS_NORMAL;
    claustrum->brain_state = CLAUSTRUM_BRAIN_STATE_DEFAULT;

    /* Reset workspace */
    claustrum->workspace_occupied = false;
    claustrum->workspace_percept_id = 0;
    claustrum->workspace_access_level = 0.0f;

    /* Reset attention */
    claustrum->global_attention = 0.5f;
    claustrum->global_salience = 0.0f;

    /* Reset timing */
    claustrum->current_time_ms = 0.0f;

    return CLAUSTRUM_OK;
}

/*=============================================================================
 * CORE UPDATE API
 *===========================================================================*/

nimcp_claustrum_error_t nimcp_claustrum_update(
    nimcp_claustrum_t* claustrum,
    float dt
) {
    if (!claustrum) {
        return CLAUSTRUM_ERR_NULL_PTR;
    }

    if (!claustrum->initialized) {
        return CLAUSTRUM_ERR_NOT_INITIALIZED;
    }

    /* Update simulation time */
    claustrum->current_time_ms += dt;
    claustrum->last_update_us = get_time_us();
    claustrum->metrics.update_count++;
    claustrum->metrics.total_simulation_time_s += dt / 1000.0f;

    /* Update oscillator */
    update_oscillator(&claustrum->oscillator, dt);

    /* Update modality phases for synchronization */
    for (int i = 0; i < CLAUSTRUM_MODALITY_COUNT; i++) {
        if (claustrum->modalities[i].active) {
            /* Phase advances with gamma rhythm */
            claustrum->modalities[i].phase +=
                TWO_PI * claustrum->oscillator.gamma_frequency * dt / 1000.0f;
            while (claustrum->modalities[i].phase >= TWO_PI) {
                claustrum->modalities[i].phase -= TWO_PI;
            }

            /* Activity decays */
            claustrum->modalities[i].activity_level = exponential_decay(
                claustrum->modalities[i].activity_level, 0.0f,
                ACTIVITY_DECAY_RATE, dt);

            /* Deactivate if activity too low */
            if (claustrum->modalities[i].activity_level < 0.05f) {
                claustrum->modalities[i].active = false;
            }
        }
    }

    /* Update cortical link synchronization */
    for (int i = 0; i < CLAUSTRUM_REGION_COUNT; i++) {
        if (claustrum->cortical_links[i].active) {
            /* Activity decays */
            claustrum->cortical_links[i].activity = exponential_decay(
                claustrum->cortical_links[i].activity, 0.0f,
                ACTIVITY_DECAY_RATE, dt);

            /* Synchronization tracks global coherence */
            claustrum->cortical_links[i].synchronization = exponential_decay(
                claustrum->cortical_links[i].synchronization,
                claustrum->oscillator.global_coherence,
                0.1f, dt);
        }
    }

    /* Update all active percepts */
    for (int i = 0; i < CLAUSTRUM_MAX_BOUND_PERCEPTS; i++) {
        if (claustrum->percepts[i].valid) {
            update_percept(claustrum, &claustrum->percepts[i], dt);
        }
    }

    /* Update global coherence based on active modalities */
    int active_count = 0;
    float total_coherence = 0.0f;
    for (int i = 0; i < CLAUSTRUM_MODALITY_COUNT; i++) {
        if (claustrum->modalities[i].active) {
            total_coherence += claustrum->modalities[i].coherence;
            active_count++;
        }
    }
    if (active_count > 0) {
        claustrum->oscillator.global_coherence = exponential_decay(
            claustrum->oscillator.global_coherence,
            total_coherence / active_count,
            0.1f, dt);
    }

    /* Update task switching progress */
    if (claustrum->state == CLAUSTRUM_STATE_SWITCHING) {
        claustrum->switch_progress += dt / claustrum->config.switch_duration_ms;
        if (claustrum->switch_progress >= 1.0f) {
            nimcp_claustrum_brain_state_t old_state = claustrum->brain_state;
            claustrum->brain_state = claustrum->target_state;
            claustrum->state = CLAUSTRUM_STATE_IDLE;
            claustrum->switch_progress = 0.0f;
            claustrum->metrics.state_switches++;

            if (claustrum->config.on_state_change) {
                claustrum->config.on_state_change(claustrum, old_state,
                    claustrum->brain_state, claustrum->config.callback_data);
            }

            LOG_MODULE_DEBUG(CLAUSTRUM_LOG_MODULE, "State switch complete: %s -> %s",
                      nimcp_claustrum_brain_state_string(old_state),
                      nimcp_claustrum_brain_state_string(claustrum->brain_state));
        }
    }

    /* Update workspace access level decay */
    if (claustrum->workspace_occupied) {
        claustrum->workspace_access_level = exponential_decay(
            claustrum->workspace_access_level, 0.0f,
            1.0f / claustrum->config.broadcast_duration_ms, dt);

        if (claustrum->workspace_access_level < 0.1f) {
            claustrum->workspace_occupied = false;
            int idx = find_percept_by_id(claustrum, claustrum->workspace_percept_id);
            if (idx >= 0) {
                claustrum->percepts[idx].in_workspace = false;
            }
            claustrum->workspace_percept_id = 0;
        }
    }

    return CLAUSTRUM_OK;
}

/*=============================================================================
 * MODALITY INPUT API
 *===========================================================================*/

nimcp_claustrum_error_t nimcp_claustrum_update_modality(
    nimcp_claustrum_t* claustrum,
    nimcp_claustrum_modality_t modality,
    const float* features,
    uint32_t feature_dim,
    float activity
) {
    if (!claustrum) {
        return CLAUSTRUM_ERR_NULL_PTR;
    }

    if (!claustrum->initialized) {
        return CLAUSTRUM_ERR_NOT_INITIALIZED;
    }

    if (modality >= CLAUSTRUM_MODALITY_COUNT) {
        return CLAUSTRUM_ERR_INVALID_PARAM;
    }

    nimcp_claustrum_modality_input_t* mod = &claustrum->modalities[modality];

    /* Allocate/reallocate feature buffer if needed */
    if (features && feature_dim > 0) {
        if (mod->feature_dim != feature_dim) {
            if (mod->features) {
                nimcp_free(mod->features);
            }
            mod->features = (float*)nimcp_malloc(feature_dim * sizeof(float));
            if (!mod->features) {
                return CLAUSTRUM_ERR_NO_MEMORY;
            }
            mod->feature_dim = feature_dim;
        }
        memcpy(mod->features, features, feature_dim * sizeof(float));
    }

    /* Update state */
    mod->activity_level = nimcp_clamp01(activity);
    mod->active = (activity > 0.05f);
    mod->timestamp_us = get_time_us();

    /* Update active mask */
    if (mod->active) {
        claustrum->active_modality_mask |= (1U << modality);
    } else {
        claustrum->active_modality_mask &= ~(1U << modality);
    }

    /* Compute salience if not explicitly set */
    if (mod->active && mod->salience < 0.01f) {
        /* Default salience based on activity */
        mod->salience = mod->activity_level * claustrum->config.salience_gain;
        mod->salience = nimcp_clamp01(mod->salience);
    }

    /* Update metrics */
    claustrum->metrics.modality_updates[modality]++;
    claustrum->metrics.modality_activity[modality] = mod->activity_level;

    return CLAUSTRUM_OK;
}

nimcp_claustrum_error_t nimcp_claustrum_set_modality_salience(
    nimcp_claustrum_t* claustrum,
    nimcp_claustrum_modality_t modality,
    float salience
) {
    if (!claustrum) return CLAUSTRUM_ERR_NULL_PTR;
    if (!claustrum->initialized) return CLAUSTRUM_ERR_NOT_INITIALIZED;
    if (modality >= CLAUSTRUM_MODALITY_COUNT) return CLAUSTRUM_ERR_INVALID_PARAM;

    claustrum->modalities[modality].salience = nimcp_clamp01(salience);
    return CLAUSTRUM_OK;
}

nimcp_claustrum_error_t nimcp_claustrum_get_modality(
    const nimcp_claustrum_t* claustrum,
    nimcp_claustrum_modality_t modality,
    nimcp_claustrum_modality_input_t* input
) {
    if (!claustrum || !input) return CLAUSTRUM_ERR_NULL_PTR;
    if (!claustrum->initialized) return CLAUSTRUM_ERR_NOT_INITIALIZED;
    if (modality >= CLAUSTRUM_MODALITY_COUNT) return CLAUSTRUM_ERR_INVALID_PARAM;

    *input = claustrum->modalities[modality];
    /* Don't copy the feature pointer - it's internal */
    input->features = NULL;
    return CLAUSTRUM_OK;
}

/*=============================================================================
 * CROSS-MODAL BINDING API
 *===========================================================================*/

nimcp_claustrum_error_t nimcp_claustrum_bind_modalities(
    nimcp_claustrum_t* claustrum,
    uint32_t modality_mask,
    uint32_t* percept_id
) {
    if (!claustrum || !percept_id) return CLAUSTRUM_ERR_NULL_PTR;
    if (!claustrum->initialized) return CLAUSTRUM_ERR_NOT_INITIALIZED;

    /* Count active modalities in mask */
    uint32_t active_in_mask = modality_mask & claustrum->active_modality_mask;
    int count = 0;
    for (int i = 0; i < CLAUSTRUM_MODALITY_COUNT; i++) {
        if (active_in_mask & (1U << i)) count++;
    }

    if (count < 2) {
        claustrum->metrics.failed_bindings++;
        return CLAUSTRUM_ERR_BINDING_FAILED;
    }

    /* Compute coherence between modalities */
    float total_coherence = 0.0f;
    int pairs = 0;
    for (int i = 0; i < CLAUSTRUM_MODALITY_COUNT; i++) {
        if (!(active_in_mask & (1U << i))) continue;
        for (int j = i + 1; j < CLAUSTRUM_MODALITY_COUNT; j++) {
            if (!(active_in_mask & (1U << j))) continue;
            total_coherence += compute_modality_coherence(
                &claustrum->modalities[i], &claustrum->modalities[j]);
            pairs++;
        }
    }

    float coherence = (pairs > 0) ? (total_coherence / pairs) : 0.0f;

    /* Check binding threshold */
    if (coherence < claustrum->config.binding_threshold) {
        claustrum->metrics.failed_bindings++;
        LOG_MODULE_DEBUG(CLAUSTRUM_LOG_MODULE,
                  "Binding failed: coherence %.2f < threshold %.2f",
                  coherence, claustrum->config.binding_threshold);
        return CLAUSTRUM_ERR_BINDING_FAILED;
    }

    /* Set state to binding */
    claustrum->state = CLAUSTRUM_STATE_BINDING;

    /* Create the percept */
    nimcp_claustrum_error_t err = create_percept(claustrum, active_in_mask, percept_id);

    if (err == CLAUSTRUM_OK && claustrum->config.on_binding) {
        int idx = find_percept_by_id(claustrum, *percept_id);
        if (idx >= 0) {
            claustrum->config.on_binding(claustrum, &claustrum->percepts[idx],
                                          claustrum->config.callback_data);
        }
    }

    claustrum->state = CLAUSTRUM_STATE_IDLE;
    return err;
}

nimcp_claustrum_error_t nimcp_claustrum_get_percept(
    const nimcp_claustrum_t* claustrum,
    uint32_t percept_id,
    nimcp_claustrum_bound_percept_t* percept
) {
    if (!claustrum || !percept) return CLAUSTRUM_ERR_NULL_PTR;
    if (!claustrum->initialized) return CLAUSTRUM_ERR_NOT_INITIALIZED;

    int idx = find_percept_by_id(claustrum, percept_id);
    if (idx < 0) return CLAUSTRUM_ERR_MODALITY_NOT_FOUND;

    *percept = claustrum->percepts[idx];
    return CLAUSTRUM_OK;
}

nimcp_claustrum_error_t nimcp_claustrum_get_strongest_binding(
    const nimcp_claustrum_t* claustrum,
    uint32_t* percept_id,
    float* strength
) {
    if (!claustrum) return CLAUSTRUM_ERR_NULL_PTR;
    if (!claustrum->initialized) return CLAUSTRUM_ERR_NOT_INITIALIZED;

    float max_strength = 0.0f;
    int best_idx = -1;

    for (int i = 0; i < CLAUSTRUM_MAX_BOUND_PERCEPTS; i++) {
        if (claustrum->percepts[i].valid &&
            claustrum->percepts[i].binding_strength > max_strength) {
            max_strength = claustrum->percepts[i].binding_strength;
            best_idx = i;
        }
    }

    if (best_idx < 0) {
        if (percept_id) *percept_id = 0;
        if (strength) *strength = 0.0f;
        return CLAUSTRUM_OK;
    }

    if (percept_id) *percept_id = claustrum->percepts[best_idx].id;
    if (strength) *strength = max_strength;
    return CLAUSTRUM_OK;
}

nimcp_claustrum_error_t nimcp_claustrum_release_percept(
    nimcp_claustrum_t* claustrum,
    uint32_t percept_id
) {
    if (!claustrum) return CLAUSTRUM_ERR_NULL_PTR;
    if (!claustrum->initialized) return CLAUSTRUM_ERR_NOT_INITIALIZED;

    int idx = find_percept_by_id(claustrum, percept_id);
    if (idx < 0) return CLAUSTRUM_ERR_MODALITY_NOT_FOUND;

    nimcp_claustrum_bound_percept_t* percept = &claustrum->percepts[idx];

    /* Unbind modalities */
    for (int i = 0; i < CLAUSTRUM_MODALITY_COUNT; i++) {
        if ((percept->modality_mask & (1U << i)) &&
            claustrum->modalities[i].binding_id == percept_id) {
            claustrum->modalities[i].bound = false;
            claustrum->modalities[i].binding_id = 0;
        }
    }

    /* Clear workspace if this percept was in it */
    if (claustrum->workspace_percept_id == percept_id) {
        claustrum->workspace_occupied = false;
        claustrum->workspace_percept_id = 0;
    }

    percept->valid = false;
    claustrum->num_active_percepts--;

    LOG_MODULE_DEBUG(CLAUSTRUM_LOG_MODULE, "Released percept %u", percept_id);
    return CLAUSTRUM_OK;
}

/*=============================================================================
 * SYNCHRONIZATION API
 *===========================================================================*/

nimcp_claustrum_error_t nimcp_claustrum_synchronize(nimcp_claustrum_t* claustrum) {
    if (!claustrum) return CLAUSTRUM_ERR_NULL_PTR;
    if (!claustrum->initialized) return CLAUSTRUM_ERR_NOT_INITIALIZED;

    claustrum->state = CLAUSTRUM_STATE_SYNCHRONIZING;

    /* Align all active modality phases toward mean phase */
    float mean_phase = 0.0f;
    int active_count = 0;
    for (int i = 0; i < CLAUSTRUM_MODALITY_COUNT; i++) {
        if (claustrum->modalities[i].active) {
            mean_phase += claustrum->modalities[i].phase;
            active_count++;
        }
    }

    if (active_count > 0) {
        mean_phase /= active_count;

        /* Gradually align phases */
        for (int i = 0; i < CLAUSTRUM_MODALITY_COUNT; i++) {
            if (claustrum->modalities[i].active) {
                float phase_diff = mean_phase - claustrum->modalities[i].phase;
                claustrum->modalities[i].phase += phase_diff * 0.3f;

                /* Update coherence */
                float normalized_diff = fabsf(phase_diff) / PI;
                claustrum->modalities[i].coherence = nimcp_clamp01(1.0f - normalized_diff);
            }
        }

        /* Boost global coherence */
        claustrum->oscillator.global_coherence = nimcp_clamp01(
            claustrum->oscillator.global_coherence + 0.1f);
    }

    claustrum->state = CLAUSTRUM_STATE_IDLE;
    return CLAUSTRUM_OK;
}

nimcp_claustrum_error_t nimcp_claustrum_get_sync_level(
    const nimcp_claustrum_t* claustrum,
    float* coherence
) {
    if (!claustrum || !coherence) return CLAUSTRUM_ERR_NULL_PTR;
    if (!claustrum->initialized) return CLAUSTRUM_ERR_NOT_INITIALIZED;

    *coherence = claustrum->oscillator.global_coherence;
    return CLAUSTRUM_OK;
}

nimcp_claustrum_error_t nimcp_claustrum_set_gamma(
    nimcp_claustrum_t* claustrum,
    float frequency,
    float amplitude
) {
    if (!claustrum) return CLAUSTRUM_ERR_NULL_PTR;
    if (!claustrum->initialized) return CLAUSTRUM_ERR_NOT_INITIALIZED;

    claustrum->oscillator.gamma_frequency = frequency;
    claustrum->oscillator.gamma_amplitude = nimcp_clamp01(amplitude);
    return CLAUSTRUM_OK;
}

nimcp_claustrum_error_t nimcp_claustrum_set_alpha(
    nimcp_claustrum_t* claustrum,
    float frequency,
    float amplitude
) {
    if (!claustrum) return CLAUSTRUM_ERR_NULL_PTR;
    if (!claustrum->initialized) return CLAUSTRUM_ERR_NOT_INITIALIZED;

    claustrum->oscillator.alpha_frequency = frequency;
    claustrum->oscillator.alpha_amplitude = nimcp_clamp01(amplitude);
    return CLAUSTRUM_OK;
}

/*=============================================================================
 * SALIENCE AND ATTENTION API
 *===========================================================================*/

nimcp_claustrum_error_t nimcp_claustrum_detect_salience(
    nimcp_claustrum_t* claustrum,
    float* salience_out,
    nimcp_claustrum_modality_t* salient_modality
) {
    if (!claustrum) return CLAUSTRUM_ERR_NULL_PTR;
    if (!claustrum->initialized) return CLAUSTRUM_ERR_NOT_INITIALIZED;

    float max_salience = 0.0f;
    nimcp_claustrum_modality_t max_modality = CLAUSTRUM_MODALITY_VISUAL;
    float total_salience = 0.0f;
    int count = 0;

    for (int i = 0; i < CLAUSTRUM_MODALITY_COUNT; i++) {
        if (claustrum->modalities[i].active) {
            float sal = claustrum->modalities[i].salience;
            total_salience += sal;
            count++;

            if (sal > max_salience) {
                max_salience = sal;
                max_modality = (nimcp_claustrum_modality_t)i;
            }
        }
    }

    claustrum->global_salience = (count > 0) ? (total_salience / count) : 0.0f;
    claustrum->salient_modality = max_modality;
    claustrum->metrics.salience_detections++;

    if (max_salience > claustrum->metrics.peak_salience) {
        claustrum->metrics.peak_salience = max_salience;
    }

    if (salience_out) *salience_out = claustrum->global_salience;
    if (salient_modality) *salient_modality = max_modality;

    return CLAUSTRUM_OK;
}

nimcp_claustrum_error_t nimcp_claustrum_set_attention_bias(
    nimcp_claustrum_t* claustrum,
    nimcp_claustrum_region_t region,
    float bias
) {
    if (!claustrum) return CLAUSTRUM_ERR_NULL_PTR;
    if (!claustrum->initialized) return CLAUSTRUM_ERR_NOT_INITIALIZED;
    if (region >= CLAUSTRUM_REGION_COUNT) return CLAUSTRUM_ERR_INVALID_PARAM;

    claustrum->cortical_links[region].attention_bias = nimcp_clamp01(bias);
    return CLAUSTRUM_OK;
}

nimcp_claustrum_error_t nimcp_claustrum_get_attention(
    const nimcp_claustrum_t* claustrum,
    float* global_attention,
    float* spatial_attention,
    float* feature_attention
) {
    if (!claustrum) return CLAUSTRUM_ERR_NULL_PTR;
    if (!claustrum->initialized) return CLAUSTRUM_ERR_NOT_INITIALIZED;

    if (global_attention) *global_attention = claustrum->global_attention;
    if (spatial_attention) {
        memcpy(spatial_attention, claustrum->spatial_attention, 3 * sizeof(float));
    }
    if (feature_attention) {
        memcpy(feature_attention, claustrum->feature_attention, 8 * sizeof(float));
    }
    return CLAUSTRUM_OK;
}

/*=============================================================================
 * GLOBAL WORKSPACE API
 *===========================================================================*/

nimcp_claustrum_error_t nimcp_claustrum_gate_workspace(
    nimcp_claustrum_t* claustrum,
    uint32_t percept_id,
    bool* granted
) {
    if (!claustrum || !granted) return CLAUSTRUM_ERR_NULL_PTR;
    if (!claustrum->initialized) return CLAUSTRUM_ERR_NOT_INITIALIZED;

    *granted = false;

    if (!claustrum->config.enable_workspace_gating) {
        *granted = true;
        return CLAUSTRUM_OK;
    }

    int idx = find_percept_by_id(claustrum, percept_id);
    if (idx < 0) return CLAUSTRUM_ERR_MODALITY_NOT_FOUND;

    nimcp_claustrum_bound_percept_t* percept = &claustrum->percepts[idx];
    claustrum->state = CLAUSTRUM_STATE_GATING;

    /* Check if workspace is available or new percept is more salient */
    bool can_access = false;
    if (!claustrum->workspace_occupied) {
        can_access = percept->salience >= claustrum->config.workspace_threshold;
    } else {
        /* Compare with current occupant */
        int curr_idx = find_percept_by_id(claustrum, claustrum->workspace_percept_id);
        if (curr_idx >= 0) {
            float curr_salience = claustrum->percepts[curr_idx].salience;
            can_access = percept->salience > curr_salience * 1.2f; /* 20% higher */
        } else {
            can_access = true; /* Current occupant invalid */
        }
    }

    if (can_access) {
        /* Evict current occupant if any */
        if (claustrum->workspace_occupied) {
            int curr_idx = find_percept_by_id(claustrum, claustrum->workspace_percept_id);
            if (curr_idx >= 0) {
                claustrum->percepts[curr_idx].in_workspace = false;
            }
        }

        /* Grant access */
        claustrum->workspace_occupied = true;
        claustrum->workspace_percept_id = percept_id;
        claustrum->workspace_access_level = 1.0f;
        percept->in_workspace = true;
        percept->consciousness_level = CLAUSTRUM_CONSCIOUSNESS_FOCAL;
        claustrum->metrics.workspace_accesses++;
        *granted = true;

        LOG_MODULE_DEBUG(CLAUSTRUM_LOG_MODULE,
                  "Workspace access granted to percept %u (salience=%.2f)",
                  percept_id, percept->salience);
    }

    claustrum->state = CLAUSTRUM_STATE_IDLE;
    return CLAUSTRUM_OK;
}

nimcp_claustrum_error_t nimcp_claustrum_broadcast_workspace(
    nimcp_claustrum_t* claustrum,
    const void* content,
    size_t content_size
) {
    if (!claustrum) return CLAUSTRUM_ERR_NULL_PTR;
    if (!claustrum->initialized) return CLAUSTRUM_ERR_NOT_INITIALIZED;

    claustrum->state = CLAUSTRUM_STATE_BROADCASTING;
    claustrum->metrics.workspace_broadcasts++;

    if (claustrum->config.on_workspace_broadcast) {
        claustrum->config.on_workspace_broadcast(
            claustrum, content, content_size,
            claustrum->global_salience,
            claustrum->config.callback_data);
    }

    LOG_MODULE_DEBUG(CLAUSTRUM_LOG_MODULE,
              "Workspace broadcast: %zu bytes, salience=%.2f",
              content_size, claustrum->global_salience);

    claustrum->state = CLAUSTRUM_STATE_IDLE;
    return CLAUSTRUM_OK;
}

nimcp_claustrum_error_t nimcp_claustrum_get_workspace_state(
    const nimcp_claustrum_t* claustrum,
    bool* occupied,
    uint32_t* percept_id
) {
    if (!claustrum) return CLAUSTRUM_ERR_NULL_PTR;
    if (!claustrum->initialized) return CLAUSTRUM_ERR_NOT_INITIALIZED;

    if (occupied) *occupied = claustrum->workspace_occupied;
    if (percept_id) *percept_id = claustrum->workspace_percept_id;
    return CLAUSTRUM_OK;
}

/*=============================================================================
 * TASK SWITCHING API
 *===========================================================================*/

nimcp_claustrum_error_t nimcp_claustrum_switch_state(
    nimcp_claustrum_t* claustrum,
    nimcp_claustrum_brain_state_t target_state
) {
    if (!claustrum) return CLAUSTRUM_ERR_NULL_PTR;
    if (!claustrum->initialized) return CLAUSTRUM_ERR_NOT_INITIALIZED;

    if (claustrum->brain_state == target_state) {
        return CLAUSTRUM_OK; /* Already in target state */
    }

    LOG_MODULE_DEBUG(CLAUSTRUM_LOG_MODULE, "Initiating state switch: %s -> %s",
              nimcp_claustrum_brain_state_string(claustrum->brain_state),
              nimcp_claustrum_brain_state_string(target_state));

    claustrum->target_state = target_state;
    claustrum->switch_progress = 0.0f;

    /* Trigger immediate synchronization for switch - do this before setting
     * SWITCHING state since synchronize overwrites the state */
    nimcp_claustrum_synchronize(claustrum);

    /* Now set the switching state so update() will advance the switch */
    claustrum->state = CLAUSTRUM_STATE_SWITCHING;

    return CLAUSTRUM_OK;
}

nimcp_claustrum_brain_state_t nimcp_claustrum_get_brain_state(
    const nimcp_claustrum_t* claustrum
) {
    if (!claustrum || !claustrum->initialized) {
        return CLAUSTRUM_BRAIN_STATE_DEFAULT;
    }
    return claustrum->brain_state;
}

nimcp_claustrum_error_t nimcp_claustrum_get_switch_progress(
    const nimcp_claustrum_t* claustrum,
    float* progress,
    nimcp_claustrum_brain_state_t* target
) {
    if (!claustrum) return CLAUSTRUM_ERR_NULL_PTR;
    if (!claustrum->initialized) return CLAUSTRUM_ERR_NOT_INITIALIZED;

    if (progress) *progress = claustrum->switch_progress;
    if (target) *target = claustrum->target_state;
    return CLAUSTRUM_OK;
}

/*=============================================================================
 * CORTICAL COORDINATION API
 *===========================================================================*/

nimcp_claustrum_error_t nimcp_claustrum_update_cortical_region(
    nimcp_claustrum_t* claustrum,
    nimcp_claustrum_region_t region,
    float activity
) {
    if (!claustrum) return CLAUSTRUM_ERR_NULL_PTR;
    if (!claustrum->initialized) return CLAUSTRUM_ERR_NOT_INITIALIZED;
    if (region >= CLAUSTRUM_REGION_COUNT) return CLAUSTRUM_ERR_INVALID_PARAM;

    claustrum->cortical_links[region].activity = nimcp_clamp01(activity);
    claustrum->cortical_links[region].receiving = true;
    return CLAUSTRUM_OK;
}

nimcp_claustrum_error_t nimcp_claustrum_get_cortical_link(
    const nimcp_claustrum_t* claustrum,
    nimcp_claustrum_region_t region,
    nimcp_claustrum_cortical_link_t* link
) {
    if (!claustrum || !link) return CLAUSTRUM_ERR_NULL_PTR;
    if (!claustrum->initialized) return CLAUSTRUM_ERR_NOT_INITIALIZED;
    if (region >= CLAUSTRUM_REGION_COUNT) return CLAUSTRUM_ERR_INVALID_PARAM;

    *link = claustrum->cortical_links[region];
    return CLAUSTRUM_OK;
}

nimcp_claustrum_error_t nimcp_claustrum_set_cortical_link_strength(
    nimcp_claustrum_t* claustrum,
    nimcp_claustrum_region_t region,
    float forward_strength,
    float backward_strength
) {
    if (!claustrum) return CLAUSTRUM_ERR_NULL_PTR;
    if (!claustrum->initialized) return CLAUSTRUM_ERR_NOT_INITIALIZED;
    if (region >= CLAUSTRUM_REGION_COUNT) return CLAUSTRUM_ERR_INVALID_PARAM;

    claustrum->cortical_links[region].forward_strength = nimcp_clamp01(forward_strength);
    claustrum->cortical_links[region].backward_strength = nimcp_clamp01(backward_strength);
    return CLAUSTRUM_OK;
}

/*=============================================================================
 * STATE AND METRICS API
 *===========================================================================*/

nimcp_claustrum_state_t nimcp_claustrum_get_state(const nimcp_claustrum_t* claustrum) {
    if (!claustrum || !claustrum->initialized) {
        return CLAUSTRUM_STATE_IDLE;
    }
    return claustrum->state;
}

nimcp_claustrum_status_t nimcp_claustrum_get_status(const nimcp_claustrum_t* claustrum) {
    if (!claustrum || !claustrum->initialized) {
        return CLAUSTRUM_STATUS_NORMAL;
    }
    return claustrum->status;
}

nimcp_claustrum_error_t nimcp_claustrum_get_metrics(
    const nimcp_claustrum_t* claustrum,
    nimcp_claustrum_metrics_t* metrics
) {
    if (!claustrum || !metrics) return CLAUSTRUM_ERR_NULL_PTR;
    if (!claustrum->initialized) return CLAUSTRUM_ERR_NOT_INITIALIZED;

    *metrics = claustrum->metrics;
    return CLAUSTRUM_OK;
}

nimcp_claustrum_error_t nimcp_claustrum_reset_metrics(nimcp_claustrum_t* claustrum) {
    if (!claustrum) return CLAUSTRUM_ERR_NULL_PTR;
    if (!claustrum->initialized) return CLAUSTRUM_ERR_NOT_INITIALIZED;

    memset(&claustrum->metrics, 0, sizeof(claustrum->metrics));
    return CLAUSTRUM_OK;
}

/*=============================================================================
 * UTILITY API
 *===========================================================================*/

const char* nimcp_claustrum_error_string(nimcp_claustrum_error_t error) {
    switch (error) {
        case CLAUSTRUM_OK: return "OK";
        case CLAUSTRUM_ERR_NULL_PTR: return "NULL_PTR";
        case CLAUSTRUM_ERR_INVALID_PARAM: return "INVALID_PARAM";
        case CLAUSTRUM_ERR_NOT_INITIALIZED: return "NOT_INITIALIZED";
        case CLAUSTRUM_ERR_ALREADY_INITIALIZED: return "ALREADY_INITIALIZED";
        case CLAUSTRUM_ERR_NO_MEMORY: return "NO_MEMORY";
        case CLAUSTRUM_ERR_MODALITY_NOT_FOUND: return "MODALITY_NOT_FOUND";
        case CLAUSTRUM_ERR_CAPACITY_EXCEEDED: return "CAPACITY_EXCEEDED";
        case CLAUSTRUM_ERR_BINDING_FAILED: return "BINDING_FAILED";
        case CLAUSTRUM_ERR_SYNC_FAILED: return "SYNC_FAILED";
        case CLAUSTRUM_ERR_INVALID_STATE: return "INVALID_STATE";
        case CLAUSTRUM_ERR_SECURITY_VIOLATION: return "SECURITY_VIOLATION";
        default: return "UNKNOWN";
    }
}

const char* nimcp_claustrum_modality_string(nimcp_claustrum_modality_t modality) {
    switch (modality) {
        case CLAUSTRUM_MODALITY_VISUAL: return "VISUAL";
        case CLAUSTRUM_MODALITY_AUDITORY: return "AUDITORY";
        case CLAUSTRUM_MODALITY_SOMATOSENSORY: return "SOMATOSENSORY";
        case CLAUSTRUM_MODALITY_OLFACTORY: return "OLFACTORY";
        case CLAUSTRUM_MODALITY_GUSTATORY: return "GUSTATORY";
        case CLAUSTRUM_MODALITY_INTEROCEPTIVE: return "INTEROCEPTIVE";
        case CLAUSTRUM_MODALITY_VESTIBULAR: return "VESTIBULAR";
        case CLAUSTRUM_MODALITY_MOTOR_EFFERENCE: return "MOTOR_EFFERENCE";
        default: return "UNKNOWN";
    }
}

const char* nimcp_claustrum_state_string(nimcp_claustrum_state_t state) {
    switch (state) {
        case CLAUSTRUM_STATE_IDLE: return "IDLE";
        case CLAUSTRUM_STATE_BINDING: return "BINDING";
        case CLAUSTRUM_STATE_SYNCHRONIZING: return "SYNCHRONIZING";
        case CLAUSTRUM_STATE_SWITCHING: return "SWITCHING";
        case CLAUSTRUM_STATE_BROADCASTING: return "BROADCASTING";
        case CLAUSTRUM_STATE_GATING: return "GATING";
        default: return "UNKNOWN";
    }
}

const char* nimcp_claustrum_brain_state_string(nimcp_claustrum_brain_state_t state) {
    switch (state) {
        case CLAUSTRUM_BRAIN_STATE_DEFAULT: return "DEFAULT_MODE";
        case CLAUSTRUM_BRAIN_STATE_TASK_POSITIVE: return "TASK_POSITIVE";
        case CLAUSTRUM_BRAIN_STATE_SALIENCE: return "SALIENCE";
        case CLAUSTRUM_BRAIN_STATE_TRANSITION: return "TRANSITION";
        default: return "UNKNOWN";
    }
}

const char* nimcp_claustrum_region_string(nimcp_claustrum_region_t region) {
    switch (region) {
        case CLAUSTRUM_REGION_PREFRONTAL: return "PREFRONTAL";
        case CLAUSTRUM_REGION_CINGULATE: return "CINGULATE";
        case CLAUSTRUM_REGION_INSULA: return "INSULA";
        case CLAUSTRUM_REGION_PARIETAL: return "PARIETAL";
        case CLAUSTRUM_REGION_TEMPORAL: return "TEMPORAL";
        case CLAUSTRUM_REGION_OCCIPITAL: return "OCCIPITAL";
        case CLAUSTRUM_REGION_MOTOR: return "MOTOR";
        case CLAUSTRUM_REGION_SOMATOSENSORY: return "SOMATOSENSORY";
        case CLAUSTRUM_REGION_AUDITORY: return "AUDITORY";
        case CLAUSTRUM_REGION_HIPPOCAMPUS: return "HIPPOCAMPUS";
        case CLAUSTRUM_REGION_AMYGDALA: return "AMYGDALA";
        default: return "UNKNOWN";
    }
}

const char* nimcp_claustrum_bio_msg_type_string(nimcp_claustrum_bio_msg_type_t msg_type) {
    switch (msg_type) {
        case CLAUSTRUM_BIO_MSG_BINDING: return "BINDING";
        case CLAUSTRUM_BIO_MSG_SYNC: return "SYNC";
        case CLAUSTRUM_BIO_MSG_SALIENCE: return "SALIENCE";
        case CLAUSTRUM_BIO_MSG_ATTENTION_BIAS: return "ATTENTION_BIAS";
        case CLAUSTRUM_BIO_MSG_STATE_SWITCH: return "STATE_SWITCH";
        case CLAUSTRUM_BIO_MSG_WORKSPACE_GATE: return "WORKSPACE_GATE";
        case CLAUSTRUM_BIO_MSG_PERCEPT_BROADCAST: return "PERCEPT_BROADCAST";
        case CLAUSTRUM_BIO_MSG_GAMMA_MODULATION: return "GAMMA_MODULATION";
        case CLAUSTRUM_BIO_MSG_ALPHA_MODULATION: return "ALPHA_MODULATION";
        case CLAUSTRUM_BIO_MSG_REQUEST_BINDING: return "REQUEST_BINDING";
        case CLAUSTRUM_BIO_MSG_MODALITY_UPDATE: return "MODALITY_UPDATE";
        case CLAUSTRUM_BIO_MSG_CONSCIOUSNESS_CHANGE: return "CONSCIOUSNESS_CHANGE";
        default: return "UNKNOWN";
    }
}

const char* nimcp_claustrum_consciousness_string(
    nimcp_claustrum_consciousness_level_t level
) {
    switch (level) {
        case CLAUSTRUM_CONSCIOUSNESS_UNCONSCIOUS: return "UNCONSCIOUS";
        case CLAUSTRUM_CONSCIOUSNESS_PRECONSCIOUS: return "PRECONSCIOUS";
        case CLAUSTRUM_CONSCIOUSNESS_CONSCIOUS: return "CONSCIOUS";
        case CLAUSTRUM_CONSCIOUSNESS_FOCAL: return "FOCAL";
        default: return "UNKNOWN";
    }
}

void nimcp_claustrum_print_summary(const nimcp_claustrum_t* claustrum) {
    if (!claustrum) {
        LOG_MODULE_INFO(CLAUSTRUM_LOG_MODULE, "Claustrum: NULL");
        return;
    }

    if (!claustrum->initialized) {
        LOG_MODULE_INFO(CLAUSTRUM_LOG_MODULE, "Claustrum: NOT INITIALIZED");
        return;
    }

    LOG_MODULE_INFO(CLAUSTRUM_LOG_MODULE, "=== Claustrum Summary ===");
    LOG_MODULE_INFO(CLAUSTRUM_LOG_MODULE, "State: %s, Status: %s, Brain State: %s",
             nimcp_claustrum_state_string(claustrum->state),
             claustrum->status == CLAUSTRUM_STATUS_NORMAL ? "NORMAL" : "ABNORMAL",
             nimcp_claustrum_brain_state_string(claustrum->brain_state));

    /* Active modalities */
    LOG_MODULE_INFO(CLAUSTRUM_LOG_MODULE, "Active modalities: 0x%08X",
             claustrum->active_modality_mask);
    for (int i = 0; i < CLAUSTRUM_MODALITY_COUNT; i++) {
        if (claustrum->modalities[i].active) {
            LOG_MODULE_INFO(CLAUSTRUM_LOG_MODULE, "  %s: activity=%.2f, salience=%.2f, bound=%s",
                     nimcp_claustrum_modality_string((nimcp_claustrum_modality_t)i),
                     claustrum->modalities[i].activity_level,
                     claustrum->modalities[i].salience,
                     claustrum->modalities[i].bound ? "yes" : "no");
        }
    }

    /* Percepts */
    LOG_MODULE_INFO(CLAUSTRUM_LOG_MODULE, "Active percepts: %u", claustrum->num_active_percepts);
    for (int i = 0; i < CLAUSTRUM_MAX_BOUND_PERCEPTS; i++) {
        if (claustrum->percepts[i].valid) {
            LOG_MODULE_INFO(CLAUSTRUM_LOG_MODULE,
                     "  Percept %u: modalities=0x%X, binding=%.2f, salience=%.2f, consciousness=%s",
                     claustrum->percepts[i].id,
                     claustrum->percepts[i].modality_mask,
                     claustrum->percepts[i].binding_strength,
                     claustrum->percepts[i].salience,
                     nimcp_claustrum_consciousness_string(claustrum->percepts[i].consciousness_level));
        }
    }

    /* Oscillators */
    LOG_MODULE_INFO(CLAUSTRUM_LOG_MODULE, "Oscillator: gamma=%.1fHz/%.2f, alpha=%.1fHz/%.2f, coherence=%.2f",
             claustrum->oscillator.gamma_frequency,
             claustrum->oscillator.gamma_amplitude,
             claustrum->oscillator.alpha_frequency,
             claustrum->oscillator.alpha_amplitude,
             claustrum->oscillator.global_coherence);

    /* Workspace */
    LOG_MODULE_INFO(CLAUSTRUM_LOG_MODULE, "Workspace: occupied=%s, percept_id=%u, access=%.2f",
             claustrum->workspace_occupied ? "yes" : "no",
             claustrum->workspace_percept_id,
             claustrum->workspace_access_level);

    /* Metrics */
    LOG_MODULE_INFO(CLAUSTRUM_LOG_MODULE, "Metrics: bindings=%lu (success=%lu), workspace=%lu, switches=%lu",
             (unsigned long)claustrum->metrics.total_bindings,
             (unsigned long)claustrum->metrics.successful_bindings,
             (unsigned long)claustrum->metrics.workspace_accesses,
             (unsigned long)claustrum->metrics.state_switches);

    LOG_MODULE_INFO(CLAUSTRUM_LOG_MODULE, "=========================");
}
