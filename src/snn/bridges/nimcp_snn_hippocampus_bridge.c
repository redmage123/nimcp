/**
 * @file nimcp_snn_hippocampus_bridge.c
 * @brief Implementation of SNN-hippocampus bridge
 */

#include "snn/bridges/nimcp_snn_hippocampus_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/validation/nimcp_common.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "async/nimcp_bio_async.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_hippocampus_bridge)

/* Bio-async module ID for hippocampus bridge */
#define BIO_MODULE_SNN_HIPPOCAMPUS 0x0608

/* Mathematical constants */

#define TWO_PI (2.0 * M_PI)

//=============================================================================
// Default Configuration
//=============================================================================

void snn_hippocampus_config_default(snn_hippocampus_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_hippocampus_config_default: null config pointer");
        return;
    }

    /* Theta rhythm (8 Hz typical) */
    config->theta_frequency = 8.0f;
    config->theta_amplitude = 0.5f;
    config->enable_phase_precession = true;
    config->precession_slope = 360.0f;      /* 360 deg/cm */

    /* Place cells */
    config->num_place_cells = 100;
    config->place_field_size = 0.3f;        /* 30cm field */
    config->place_field_peak_rate = 40.0f;  /* 40 Hz peak */
    config->background_rate = 0.5f;         /* 0.5 Hz background */
    config->spatial_resolution = 0.05f;     /* 5cm bins */

    /* Ripples (150 Hz typical) */
    config->ripple_frequency = 150.0f;
    config->ripple_duration = 100.0f;       /* 100ms ripple */
    config->ripple_threshold = 0.6f;
    config->ripple_participant_ratio = 40;  /* 40% of neurons */

    /* Episodic encoding */
    config->encoding_spike_threshold = 0.4f;
    config->min_spikes_for_episode = 10;
    config->episode_temporal_window = 500.0f; /* 500ms window */

    /* Pattern separation (DG) */
    config->dg_sparsity = 0.05f;            /* 5% active */
    config->dg_decorrelation_strength = 0.8f;

    /* Pattern completion (CA3) */
    config->ca3_recurrence_strength = 0.6f;
    config->ca3_completion_threshold = 0.3f;

    /* Bio-async */
    config->enable_bio_async = true;
}

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Compute distance between two 3D points
 */
static float compute_distance_3d(const float* p1, const float* p2) {
    float dx = p1[0] - p2[0];
    float dy = p1[1] - p2[1];
    float dz = p1[2] - p2[2];
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

/**
 * @brief Compute place field activity
 *
 * WHAT: Calculate firing rate based on distance from field center
 * WHY:  Gaussian place field tuning
 * HOW:  Gaussian: rate = peak * exp(-distance^2 / (2*sigma^2))
 */
static float compute_place_field_rate(
    const float* position,
    const place_cell_pattern_t* place_cell,
    float peak_rate,
    float background_rate
) {
    float field_center[3] = {
        place_cell->field_center_x,
        place_cell->field_center_y,
        0.0f
    };

    float distance = compute_distance_3d(position, field_center);
    float sigma = place_cell->field_radius;

    if (distance > 3.0f * sigma) {
        return background_rate;
    }

    float rate = peak_rate * expf(-(distance * distance) / (2.0f * sigma * sigma));
    return (rate > background_rate) ? rate : background_rate;
}

//=============================================================================
// Bridge Lifecycle
//=============================================================================

snn_hippocampus_bridge_t* snn_hippocampus_bridge_create(
    const snn_hippocampus_config_t* config,
    snn_network_t* network,
    brain_region_t* hippocampus_region
) {
    /* Guard clauses */
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_hippocampus_bridge_create: config is NULL");
        return NULL;
    }
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_hippocampus_bridge_create: network is NULL");
        return NULL;
    }
    if (!hippocampus_region) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_hippocampus_bridge_create: hippocampus_region is NULL");
        return NULL;
    }

    /* Allocate bridge */
    snn_hippocampus_bridge_t* bridge = nimcp_malloc(sizeof(snn_hippocampus_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate hippocampus bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_hippocampus_bridge_create: bridge is NULL");
        return NULL;
    }

    /* Initialize fields */
    memset(bridge, 0, sizeof(snn_hippocampus_bridge_t));
    if (bridge_base_init(&bridge->base, 0, "snn_hippocampus") != 0) { nimcp_free(bridge); return NULL; }
    bridge->network = network;
    bridge->hippocampus_region = hippocampus_region;
    bridge->config = *config;
    bridge->connected = true;

    /* Initialize theta state */
    bridge->theta_phase = 0.0f;
    bridge->theta_time = 0.0f;
    bridge->theta_active = true;

    /* Initialize position */
    bridge->current_position[0] = 0.0f;
    bridge->current_position[1] = 0.0f;
    bridge->current_position[2] = 0.0f;
    bridge->velocity[0] = 0.0f;
    bridge->velocity[1] = 0.0f;
    bridge->velocity[2] = 0.0f;

    /* Create place cells */
    bridge->n_place_cells = config->num_place_cells;
    bridge->place_cells = nimcp_malloc(bridge->n_place_cells * sizeof(place_cell_pattern_t*));
    if (!bridge->place_cells) {
        NIMCP_LOGGING_ERROR("Failed to allocate place cells");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_hippocampus_bridge_create: bridge->place_cells is NULL");
        return NULL;
    }

    /* Initialize place cells with random field centers */
    for (uint32_t i = 0; i < bridge->n_place_cells; i++) {
        place_cell_pattern_t* cell = nimcp_malloc(sizeof(place_cell_pattern_t));
        if (!cell) {
            /* Cleanup on failure */
            for (uint32_t j = 0; j < i; j++) {
                nimcp_free(bridge->place_cells[j]);
            }
            nimcp_free(bridge->place_cells);
            nimcp_free(bridge);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_hippocampus_bridge_create: cell is NULL");
            return NULL;
        }

        cell->cell_id = i;
        /* Random field centers (would use proper RNG in production) */
        cell->field_center_x = ((float)(i % 10)) * 0.2f;
        cell->field_center_y = ((float)(i / 10)) * 0.2f;
        cell->field_radius = config->place_field_size;
        cell->current_rate = config->background_rate;
        cell->theta_phase = 0.0f;
        cell->is_active = false;
        cell->last_spike_time = 0;

        bridge->place_cells[i] = cell;
    }

    /* Allocate episode storage */
    bridge->max_episodes = 1000;
    bridge->n_episodes = 0;
    bridge->episodes = nimcp_malloc(bridge->max_episodes * sizeof(episodic_memory_t*));
    if (!bridge->episodes) {
        NIMCP_LOGGING_ERROR("Failed to allocate episode storage");
        for (uint32_t i = 0; i < bridge->n_place_cells; i++) {
            nimcp_free(bridge->place_cells[i]);
        }
        nimcp_free(bridge->place_cells);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_hippocampus_bridge_create: bridge->episodes is NULL");
        return NULL;
    }
    memset(bridge->episodes, 0, bridge->max_episodes * sizeof(episodic_memory_t*));

    /* Allocate ripple tracking */
    bridge->max_ripples = 100;
    bridge->ripple_count = 0;
    bridge->recent_ripples = nimcp_malloc(bridge->max_ripples * sizeof(ripple_event_t));
    if (!bridge->recent_ripples) {
        nimcp_free(bridge->episodes);
        for (uint32_t i = 0; i < bridge->n_place_cells; i++) {
            nimcp_free(bridge->place_cells[i]);
        }
        nimcp_free(bridge->place_cells);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_hippocampus_bridge_create: bridge->recent_ripples is NULL");
        return NULL;
    }
    memset(bridge->recent_ripples, 0, bridge->max_ripples * sizeof(ripple_event_t));

    /* NOTE: Populations would be created here in full implementation */
    bridge->ca1_pop = NULL;
    bridge->ca3_pop = NULL;
    bridge->dg_pop = NULL;
    bridge->ec_pop = NULL;

    bridge->current_episode = NULL;

    NIMCP_LOGGING_INFO("Created SNN-hippocampus bridge with %u place cells",
                       bridge->n_place_cells);
    return bridge;
}

void snn_hippocampus_bridge_destroy(snn_hippocampus_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_hippocampus_bridge_destroy: null bridge pointer");
        return;
    }

    /* Free place cells */
    if (bridge->place_cells) {
        for (uint32_t i = 0; i < bridge->n_place_cells; i++) {
            nimcp_free(bridge->place_cells[i]);
        }
        nimcp_free(bridge->place_cells);
    }

    /* Free episodes */
    if (bridge->episodes) {
        for (uint32_t i = 0; i < bridge->n_episodes; i++) {
            if (bridge->episodes[i]) {
                if (bridge->episodes[i]->spike_sequence) {
                    nimcp_free(bridge->episodes[i]->spike_sequence);
                }
                if (bridge->episodes[i]->spike_times_relative) {
                    nimcp_free(bridge->episodes[i]->spike_times_relative);
                }
                nimcp_free(bridge->episodes[i]);
            }
        }
        nimcp_free(bridge->episodes);
    }

    /* Free ripple tracking */
    if (bridge->recent_ripples) {
        nimcp_free(bridge->recent_ripples);
    }

    /* Disconnect bio-async */
    if (bridge->base.bio_async_enabled) {
        snn_hippocampus_bridge_disconnect_bio_async(bridge);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

//=============================================================================
// Bio-async Integration
//=============================================================================

int snn_hippocampus_bridge_connect_bio_async(snn_hippocampus_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_hippocampus_bridge_connect_bio_async: null bridge pointer");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SNN_HIPPOCAMPUS,
        .module_name = "snn_hippocampus_bridge",
        .inbox_capacity = 64,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
    }
    return 0;
}

int snn_hippocampus_bridge_disconnect_bio_async(snn_hippocampus_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_hippocampus_bridge_disconnect_bio_async: null bridge pointer");
        return -1;
    }
    if (!bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool snn_hippocampus_bridge_is_bio_async_connected(const snn_hippocampus_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

//=============================================================================
// Processing Functions
//=============================================================================

int snn_hippocampus_bridge_process(
    snn_hippocampus_bridge_t* bridge,
    const float* position,
    const float* velocity,
    float* output,
    uint32_t output_size
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_hippocampus_bridge_process: null bridge pointer");
        return -1;
    }
    if (!position) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_hippocampus_bridge_process: null position pointer");
        return -1;
    }
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_hippocampus_bridge_process: null output pointer");
        return -1;
    }
    if (!bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "snn_hippocampus_bridge_process: bridge not connected");
        return -1;
    }

    /* Update spatial state */
    memcpy(bridge->current_position, position, 3 * sizeof(float));
    if (velocity) {
        memcpy(bridge->velocity, velocity, 3 * sizeof(float));
    }

    /* Update place cell activity */
    for (uint32_t i = 0; i < bridge->n_place_cells; i++) {
        place_cell_pattern_t* cell = bridge->place_cells[i];

        /* Compute rate based on position */
        float rate = compute_place_field_rate(
            position, cell,
            bridge->config.place_field_peak_rate,
            bridge->config.background_rate
        );

        cell->current_rate = rate;
        cell->is_active = (rate > bridge->config.background_rate * 2.0f);

        /* Phase precession */
        if (bridge->config.enable_phase_precession && cell->is_active) {
            float field_center[3] = {cell->field_center_x, cell->field_center_y, 0.0f};
            float distance = compute_distance_3d(position, field_center);
            float phase_shift = (distance / cell->field_radius) *
                               (bridge->config.precession_slope * M_PI / 180.0f);
            cell->theta_phase = fmodf(bridge->theta_phase + phase_shift, NIMCP_TWO_PI_F);
        } else {
            cell->theta_phase = bridge->theta_phase;
        }
    }

    /* Output place cell rates */
    uint32_t copy_size = (output_size < bridge->n_place_cells) ?
                         output_size : bridge->n_place_cells;
    for (uint32_t i = 0; i < copy_size; i++) {
        output[i] = bridge->place_cells[i]->current_rate;
    }

    return 0;
}

int snn_hippocampus_bridge_update(snn_hippocampus_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_hippocampus_bridge_update: null bridge pointer");
        return -1;
    }
    if (!bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "snn_hippocampus_bridge_update: bridge not connected");
        return -1;
    }

    /* Update time */
    bridge->last_update_time += dt;
    bridge->update_count++;

    /* Update theta rhythm */
    if (bridge->theta_active) {
        float theta_period = 1000.0f / bridge->config.theta_frequency; /* ms */
        bridge->theta_time = fmodf(bridge->theta_time + dt, theta_period);
        bridge->theta_phase = NIMCP_TWO_PI_F * (bridge->theta_time / theta_period);
    }

    /* Check for ripple conditions */
    /* Would analyze population activity and trigger ripples */

    return 0;
}

ripple_event_t* snn_hippocampus_generate_ripple(
    snn_hippocampus_bridge_t* bridge,
    episodic_memory_t* episode
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_hippocampus_generate_ripple: null bridge pointer");
        return NULL;
    }
    if (bridge->ripple_count >= bridge->max_ripples) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "snn_hippocampus_generate_ripple: ripple buffer full");
        return NULL;
    }

    ripple_event_t* ripple = &bridge->recent_ripples[bridge->ripple_count];
    bridge->ripple_count++;

    /* Initialize ripple event */
    ripple->start_time = (uint64_t)(bridge->last_update_time * 1000.0);
    ripple->end_time = ripple->start_time +
                       (uint64_t)(bridge->config.ripple_duration * 1000.0);
    ripple->peak_frequency = bridge->config.ripple_frequency;
    ripple->peak_amplitude = 0.8f;
    ripple->participating_neurons =
        (bridge->n_place_cells * bridge->config.ripple_participant_ratio) / 100;
    ripple->replayed_episode = episode;

    /* Would generate actual ripple spike pattern here */

    return ripple;
}

episodic_memory_t* snn_hippocampus_encode_episode(
    snn_hippocampus_bridge_t* bridge,
    float time_window
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_hippocampus_encode_episode: null bridge pointer");
        return NULL;
    }
    if (bridge->n_episodes >= bridge->max_episodes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "snn_hippocampus_encode_episode: episode storage full");
        return NULL;
    }

    /* Allocate new episode */
    episodic_memory_t* episode = nimcp_malloc(sizeof(episodic_memory_t));
    if (!episode) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_hippocampus_encode_episode: episode is NULL");
        return NULL;
    }

    memset(episode, 0, sizeof(episodic_memory_t));

    episode->episode_id = bridge->n_episodes;
    episode->start_time = (uint64_t)((bridge->last_update_time - time_window) * 1000.0);
    episode->end_time = (uint64_t)(bridge->last_update_time * 1000.0);

    /* Store spatial context */
    memcpy(episode->spatial_context, bridge->current_position, 3 * sizeof(float));

    /* Count active place cells for sequence */
    uint32_t active_count = 0;
    for (uint32_t i = 0; i < bridge->n_place_cells; i++) {
        if (bridge->place_cells[i]->is_active) {
            active_count++;
        }
    }

    if (active_count < bridge->config.min_spikes_for_episode) {
        nimcp_free(episode);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_hippocampus_encode_episode: validation failed");
        return NULL;
    }

    /* Allocate spike sequence */
    episode->sequence_length = active_count;
    episode->spike_sequence = nimcp_malloc(active_count * sizeof(uint32_t));
    episode->spike_times_relative = nimcp_malloc(active_count * sizeof(float));

    if (!episode->spike_sequence || !episode->spike_times_relative) {
        if (episode->spike_sequence) nimcp_free(episode->spike_sequence);
        if (episode->spike_times_relative) nimcp_free(episode->spike_times_relative);
        nimcp_free(episode);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_hippocampus_encode_episode: validation failed");
        return NULL;
    }

    /* Store active cells */
    uint32_t idx = 0;
    for (uint32_t i = 0; i < bridge->n_place_cells; i++) {
        if (bridge->place_cells[i]->is_active) {
            episode->spike_sequence[idx] = i;
            episode->spike_times_relative[idx] = 0.0f; /* Placeholder */
            idx++;
        }
    }

    episode->consolidated = false;

    /* Add to storage */
    bridge->episodes[bridge->n_episodes] = episode;
    bridge->n_episodes++;

    NIMCP_LOGGING_DEBUG("Encoded episode %u with %u spikes",
                        episode->episode_id, episode->sequence_length);

    return episode;
}

episodic_memory_t* snn_hippocampus_retrieve_episode(
    snn_hippocampus_bridge_t* bridge,
    const uint32_t* cue_spikes,
    uint32_t cue_length
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_hippocampus_retrieve_episode: null bridge pointer");
        return NULL;
    }
    if (!cue_spikes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_hippocampus_retrieve_episode: null cue_spikes pointer");
        return NULL;
    }
    if (cue_length == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_hippocampus_retrieve_episode: zero cue_length");
        return NULL;
    }

    /* Find most similar episode via pattern completion */
    episodic_memory_t* best_match = NULL;
    float best_similarity = 0.0f;

    for (uint32_t i = 0; i < bridge->n_episodes; i++) {
        episodic_memory_t* episode = bridge->episodes[i];
        if (!episode) continue;

        /* Compute overlap */
        uint32_t overlap = 0;
        for (uint32_t c = 0; c < cue_length; c++) {
            for (uint32_t e = 0; e < episode->sequence_length; e++) {
                if (cue_spikes[c] == episode->spike_sequence[e]) {
                    overlap++;
                    break;
                }
            }
        }

        float similarity = (float)overlap / (float)cue_length;

        if (similarity > best_similarity &&
            similarity >= bridge->config.ca3_completion_threshold) {
            best_similarity = similarity;
            best_match = episode;
        }
    }

    return best_match;
}

//=============================================================================
// Query Functions
//=============================================================================

float snn_hippocampus_get_theta_phase(const snn_hippocampus_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_hippocampus_get_theta_phase: null bridge pointer");
        return -1.0f;
    }
    return bridge->theta_phase;
}

int snn_hippocampus_get_place_cell(
    const snn_hippocampus_bridge_t* bridge,
    uint32_t cell_idx,
    place_cell_pattern_t* pattern
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_hippocampus_get_place_cell: null bridge pointer");
        return -1;
    }
    if (!pattern) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_hippocampus_get_place_cell: null pattern pointer");
        return -1;
    }
    if (cell_idx >= bridge->n_place_cells) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_hippocampus_get_place_cell: invalid cell_idx");
        return -1;
    }

    *pattern = *(bridge->place_cells[cell_idx]);
    return 0;
}

uint32_t snn_hippocampus_get_episode_count(const snn_hippocampus_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_hippocampus_get_episode_count: null bridge pointer");
        return 0;
    }
    return bridge->n_episodes;
}

float snn_hippocampus_bridge_get_activity(const snn_hippocampus_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_hippocampus_bridge_get_activity: null bridge pointer");
        return -1.0f;
    }

    /* Average place cell firing rate */
    float total_rate = 0.0f;
    for (uint32_t i = 0; i < bridge->n_place_cells; i++) {
        total_rate += bridge->place_cells[i]->current_rate;
    }

    float mean_rate = total_rate / (float)bridge->n_place_cells;

    /* Normalize by peak rate */
    return mean_rate / bridge->config.place_field_peak_rate;
}

//=============================================================================
// Statistics
//=============================================================================

int snn_hippocampus_get_stats(
    const snn_hippocampus_bridge_t* bridge,
    uint32_t* total_ripples,
    uint32_t* episodes_stored,
    uint32_t* updates
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_hippocampus_get_stats: null bridge pointer");
        return -1;
    }

    if (total_ripples) {
        *total_ripples = bridge->ripple_count;
    }

    if (episodes_stored) {
        *episodes_stored = bridge->n_episodes;
    }

    if (updates) {
        *updates = bridge->update_count;
    }

    return 0;
}

void snn_hippocampus_reset_stats(snn_hippocampus_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_hippocampus_reset_stats: null bridge pointer");
        return;
    }

    bridge->update_count = 0;
    bridge->last_update_time = 0.0f;
    bridge->ripple_count = 0;
    bridge->theta_phase = 0.0f;
    bridge->theta_time = 0.0f;
}
