//=============================================================================
// nimcp_pr_attention_bridge.c - Prime Resonant Attention Bridge Implementation
//=============================================================================
/**
 * @file nimcp_pr_attention_bridge.c
 * @brief Implementation of attention-memory integration bridge
 *
 * WHAT: Bidirectional bridge between attention mechanisms and PR memory
 * WHY:  Attention modulates memory encoding; memories guide attention
 * HOW:  Fuses bottom-up and top-down attention, updates quaternion salience
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#include "cognitive/memory/core/nimcp_pr_attention_bridge.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <time.h>
#include <stdarg.h>
#include <stdio.h>
#include "glial/myelin_sheath/nimcp_myelin_math.h"

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"

BRIDGE_BOILERPLATE(pr_attention_bridge, MESH_ADAPTER_CATEGORY_MEMORY)

#define LOG_MODULE "PR_ATTENTION_BRIDGE"


//=============================================================================
// Thread-Local Error State
//=============================================================================

static _Thread_local char g_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

/**
 * @brief Set last error message
 */
static void set_last_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_last_error, sizeof(g_last_error), fmt, args);
    va_end(args);
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Get index into attention map
 */
static inline size_t attn_map_index(
    const pr_attention_bridge_t* bridge,
    size_t x, size_t y, size_t t
) {
    return t * (bridge->spatial_width * bridge->spatial_height) +
           y * bridge->spatial_width + x;
}

/**
 * @brief Get current time in milliseconds
 */
static uint64_t get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000) + (uint64_t)(ts.tv_nsec / 1000000);
}

/**
 * @brief Allocate attention map
 */
static float* allocate_attention_map(size_t width, size_t height, size_t depth) {
    size_t total = width * height * depth;
    float* map = (float*)nimcp_calloc(total, sizeof(float));
    return map;
}

/**
 * @brief Initialize IOR state
 */
static pr_attn_error_t init_ior_state(
    pr_attn_ior_state_t* ior,
    size_t width,
    size_t height,
    float decay_tau,
    float strength
) {
    ior->width = width;
    ior->height = height;
    ior->decay_tau_ms = decay_tau;
    ior->strength = strength;

    size_t total = width * height;
    ior->ior_map = (float*)nimcp_calloc(total, sizeof(float));
    if (!ior->ior_map) {
        return PR_ATTN_ERROR_NO_MEMORY;
    }

    ior->last_attended_ms = (uint64_t*)nimcp_calloc(total, sizeof(uint64_t));
    if (!ior->last_attended_ms) {
        nimcp_free(ior->ior_map);
        ior->ior_map = NULL;
        return PR_ATTN_ERROR_NO_MEMORY;
    }

    return PR_ATTN_SUCCESS;
}

/**
 * @brief Free IOR state
 */
static void free_ior_state(pr_attn_ior_state_t* ior) {
    if (ior->ior_map) {
        nimcp_free(ior->ior_map);
        ior->ior_map = NULL;
    }
    if (ior->last_attended_ms) {
        nimcp_free(ior->last_attended_ms);
        ior->last_attended_ms = NULL;
    }
}

/**
 * @brief Find index of memory node in attended list
 */
static int find_memory_index(
    const pr_attention_bridge_t* bridge,
    const pr_memory_node_t* node
) {
    for (size_t i = 0; i < bridge->num_attended; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_attended > 256) {
            pr_attention_bridge_heartbeat("pr_attention_loop",
                             (float)(i + 1) / (float)bridge->num_attended);
        }

        if (bridge->attended_memories[i].node == node) {
            return (int)i;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_memory_index: validation failed");
    return -1;
}

/**
 * @brief U-shaped resonance-attention function
 */
static float u_shaped_attention(float resonance, float fam_thresh, float nov_thresh) {
    // U-shaped: high for low resonance (novel) and high resonance (familiar)
    // Low for intermediate resonance (habituated)
    float mid = (fam_thresh + nov_thresh) / 2.0f;
    float half_width = (fam_thresh - nov_thresh) / 2.0f;
    if (half_width < PR_ATTN_EPSILON) half_width = 0.25f;

    float distance = fabsf(resonance - mid) / half_width;
    return nimcp_myelin_clamp(distance, 0.0f, 1.0f);
}

/**
 * @brief Inverted U-shaped resonance-attention function
 */
static float inverted_u_attention(float resonance, float fam_thresh, float nov_thresh) {
    float mid = (fam_thresh + nov_thresh) / 2.0f;
    float half_width = (fam_thresh - nov_thresh) / 2.0f;
    if (half_width < PR_ATTN_EPSILON) half_width = 0.25f;

    float distance = fabsf(resonance - mid) / half_width;
    return nimcp_myelin_clamp(1.0f - distance, 0.0f, 1.0f);
}

//=============================================================================
// Configuration Functions
//=============================================================================

pr_attention_bridge_config_t pr_attention_bridge_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_config_default", 0.0f);


    pr_attention_bridge_config_t config = {
        // Spatial attention map dimensions
        .spatial_width = PR_ATTN_DEFAULT_SPATIAL_WIDTH,
        .spatial_height = PR_ATTN_DEFAULT_SPATIAL_HEIGHT,
        .temporal_depth = PR_ATTN_DEFAULT_TEMPORAL_DEPTH,

        // Bottom-up / top-down balance
        .bu_td_balance = PR_ATTN_DEFAULT_BU_TD_BALANCE,
        .fusion_mode = PR_ATTN_FUSE_WEIGHTED,

        // Source weights
        .visual_weight = 0.4f,
        .audio_weight = 0.25f,
        .speech_weight = 0.15f,
        .omni_weight = 0.1f,
        .memory_weight = 0.1f,

        // Resonance-attention coupling
        .resonance_mode = PR_ATTN_RESONANCE_U_SHAPED,
        .resonance_attention_gain = PR_ATTN_DEFAULT_RESONANCE_GAIN,
        .novelty_attention_gain = PR_ATTN_DEFAULT_NOVELTY_GAIN,
        .familiarity_threshold = 0.7f,
        .novelty_threshold = 0.3f,

        // Inhibition of Return
        .enable_ior = true,
        .ior_decay_ms = PR_ATTN_DEFAULT_IOR_DECAY_MS,
        .ior_strength = PR_ATTN_DEFAULT_IOR_STRENGTH,
        .ior_radius = 5.0f,

        // Oscillatory modulation
        .enable_theta_gamma = true,
        .alpha_suppression_base = PR_ATTN_DEFAULT_ALPHA_SUPPRESSION,
        .gamma_enhancement_base = PR_ATTN_DEFAULT_GAMMA_ENHANCEMENT,

        // Memory attention
        .max_attended_memories = PR_ATTN_MAX_ATTENDED_MEMORIES,
        .memory_attention_decay = NIMCP_EMA_DECAY_DEFAULT,

        // Integration flags
        .enable_quaternion_update = true,
        .track_statistics = true
    };

    return config;
}

bool pr_attention_bridge_config_validate(const pr_attention_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_attention_bridge_config_validate: config is NULL");
        return false;
    }

    // Spatial dimensions
    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_config_validate", 0.0f);


    if (config->spatial_width == 0 || config->spatial_height == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_attention_bridge_config_validate: config->spatial_width is zero");
        return false;
    }
    if (config->temporal_depth == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_attention_bridge_config_validate: config->temporal_depth is zero");
        return false;
    }

    // Balance must be in [0, 1]
    if (config->bu_td_balance < 0.0f || config->bu_td_balance > 1.0f) {
        return false;
    }

    // Weights must be non-negative
    if (config->visual_weight < 0.0f || config->audio_weight < 0.0f ||
        config->speech_weight < 0.0f || config->omni_weight < 0.0f ||
        config->memory_weight < 0.0f) {
        return false;
    }

    // Gains must be non-negative
    if (config->resonance_attention_gain < 0.0f ||
        config->novelty_attention_gain < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_attention_bridge_config_validate: operation failed");
        return false;
    }

    // IOR parameters
    if (config->ior_decay_ms <= 0.0f || config->ior_strength < 0.0f ||
        config->ior_strength > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_attention_bridge_config_validate: operation failed");
        return false;
    }

    // Memory attention
    if (config->max_attended_memories == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_attention_bridge_config_validate: config->max_attended_memories is zero");
        return false;
    }
    if (config->memory_attention_decay < 0.0f ||
        config->memory_attention_decay > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_attention_bridge_config_validate: config->max_attended_memories is zero");
        return false;
    }

    return true;
}

//=============================================================================
// Bridge Lifecycle Functions
//=============================================================================

pr_attention_bridge_t* pr_attention_bridge_create(
    const pr_attention_bridge_config_t* config
) {
    // Use default config if none provided
    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_create", 0.0f);


    pr_attention_bridge_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        cfg = pr_attention_bridge_config_default();
    }

    // Validate configuration
    if (!pr_attention_bridge_config_validate(&cfg)) {
        set_last_error("Invalid configuration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_attention_bridge_create: pr_attention_bridge_config_validate is NULL");
        return NULL;
    }

    // Allocate bridge structure
    pr_attention_bridge_t* bridge = (pr_attention_bridge_t*)nimcp_calloc(1, sizeof(pr_attention_bridge_t));
    if (!bridge) {
        set_last_error("Failed to allocate bridge structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    // Store configuration
    bridge->config = cfg;
    bridge->spatial_width = cfg.spatial_width;
    bridge->spatial_height = cfg.spatial_height;
    bridge->temporal_depth = cfg.temporal_depth;

    // Allocate attention maps
    bridge->unified_attention_map = allocate_attention_map(
        cfg.spatial_width, cfg.spatial_height, cfg.temporal_depth);
    if (!bridge->unified_attention_map) {
        set_last_error("Failed to allocate unified attention map");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_attention_bridge_create: bridge->unified_attention_map is NULL");
        return NULL;
    }

    bridge->bottom_up_map = allocate_attention_map(
        cfg.spatial_width, cfg.spatial_height, cfg.temporal_depth);
    if (!bridge->bottom_up_map) {
        set_last_error("Failed to allocate bottom-up map");
        nimcp_free(bridge->unified_attention_map);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_attention_bridge_create: bridge->bottom_up_map is NULL");
        return NULL;
    }

    bridge->top_down_map = allocate_attention_map(
        cfg.spatial_width, cfg.spatial_height, cfg.temporal_depth);
    if (!bridge->top_down_map) {
        set_last_error("Failed to allocate top-down map");
        nimcp_free(bridge->bottom_up_map);
        nimcp_free(bridge->unified_attention_map);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_attention_bridge_create: bridge->top_down_map is NULL");
        return NULL;
    }

    // Allocate memory attention array
    bridge->attended_memories = (pr_attn_memory_entry_t*)nimcp_calloc(
        cfg.max_attended_memories, sizeof(pr_attn_memory_entry_t));
    if (!bridge->attended_memories) {
        set_last_error("Failed to allocate memory attention array");
        nimcp_free(bridge->top_down_map);
        nimcp_free(bridge->bottom_up_map);
        nimcp_free(bridge->unified_attention_map);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_attention_bridge_create: bridge->attended_memories is NULL");
        return NULL;
    }
    bridge->num_attended = 0;
    bridge->max_attended = cfg.max_attended_memories;

    // Initialize IOR state
    if (cfg.enable_ior) {
        pr_attn_error_t err = init_ior_state(
            &bridge->ior_state,
            cfg.spatial_width,
            cfg.spatial_height,
            cfg.ior_decay_ms,
            cfg.ior_strength
        );
        if (err != PR_ATTN_SUCCESS) {
            set_last_error("Failed to initialize IOR state");
            nimcp_free(bridge->attended_memories);
            nimcp_free(bridge->top_down_map);
            nimcp_free(bridge->bottom_up_map);
            nimcp_free(bridge->unified_attention_map);
            nimcp_free(bridge);
            bridge = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_attention_bridge_create: validation failed");
            return NULL;
        }
    }

    // Initialize resonance configuration
    bridge->resonance_config = resonance_config_default();
    bridge->resonance_attention_gain = cfg.resonance_attention_gain;

    // Initialize oscillatory state
    bridge->alpha_suppression = cfg.alpha_suppression_base;
    bridge->gamma_enhancement = cfg.gamma_enhancement_base;
    bridge->theta_gamma = NULL;  // External connection

    // Initialize perception bridge pointers
    bridge->visual_bridge = NULL;
    bridge->audio_bridge = NULL;
    bridge->speech_bridge = NULL;
    bridge->omni_bridge = NULL;

    // Initialize statistics
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.last_reset_time_ms = get_current_time_ms();

    // Mark as initialized
    bridge->last_update_ms = get_current_time_ms();
    bridge->initialized = true;

    return bridge;
}

void pr_attention_bridge_destroy(pr_attention_bridge_t* bridge) {
    if (!bridge) {
        return;
        NIMCP_LOGGING_DEBUG("Destroying %s bridge", "pr_attention");
    }

    // Free attention maps
    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_destroy", 0.0f);


    if (bridge->unified_attention_map) {
        nimcp_free(bridge->unified_attention_map);
    }
    if (bridge->bottom_up_map) {
        nimcp_free(bridge->bottom_up_map);
    }
    if (bridge->top_down_map) {
        nimcp_free(bridge->top_down_map);
    }

    // Free memory attention array
    if (bridge->attended_memories) {
        nimcp_free(bridge->attended_memories);
    }

    // Free IOR state
    free_ior_state(&bridge->ior_state);

    // Free bridge structure
    nimcp_free(bridge);
    bridge = NULL;
}

pr_attn_error_t pr_attention_bridge_reset(pr_attention_bridge_t* bridge) {
    if (!bridge) {
        set_last_error("NULL bridge pointer");
        return PR_ATTN_ERROR_NULL_POINTER;
    }

    // Clear attention maps
    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_reset", 0.0f);


    size_t map_size = bridge->spatial_width * bridge->spatial_height *
                      bridge->temporal_depth;
    memset(bridge->unified_attention_map, 0, map_size * sizeof(float));
    memset(bridge->bottom_up_map, 0, map_size * sizeof(float));
    memset(bridge->top_down_map, 0, map_size * sizeof(float));

    // Clear memory attention
    memset(bridge->attended_memories, 0,
           bridge->max_attended * sizeof(pr_attn_memory_entry_t));
    bridge->num_attended = 0;

    // Clear IOR state
    if (bridge->config.enable_ior && bridge->ior_state.ior_map) {
        size_t ior_size = bridge->ior_state.width * bridge->ior_state.height;
        memset(bridge->ior_state.ior_map, 0, ior_size * sizeof(float));
        memset(bridge->ior_state.last_attended_ms, 0, ior_size * sizeof(uint64_t));
    }

    // Reset oscillatory state
    bridge->alpha_suppression = bridge->config.alpha_suppression_base;
    bridge->gamma_enhancement = bridge->config.gamma_enhancement_base;

    // Reset statistics
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.last_reset_time_ms = get_current_time_ms();

    bridge->last_update_ms = get_current_time_ms();

    return PR_ATTN_SUCCESS;
}

//=============================================================================
// Bridge Connection Functions
//=============================================================================

pr_attn_error_t pr_attention_bridge_connect_bridges(
    pr_attention_bridge_t* bridge,
    pr_visual_bridge_t visual,
    pr_audio_bridge_t audio,
    pr_speech_bridge_t speech,
    pr_omni_bridge_t omni
) {
    if (!bridge) {
        set_last_error("NULL bridge pointer");
        return PR_ATTN_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_connect_bridges", 0.0f);


    bridge->visual_bridge = visual;
    bridge->audio_bridge = audio;
    bridge->speech_bridge = speech;
    bridge->omni_bridge = omni;

    return PR_ATTN_SUCCESS;
}

pr_attn_error_t pr_attention_bridge_connect_theta_gamma(
    pr_attention_bridge_t* bridge,
    theta_gamma_manager_t theta_gamma
) {
    if (!bridge) {
        set_last_error("NULL bridge pointer");
        return PR_ATTN_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_connect_theta_gamma", 0.0f);


    bridge->theta_gamma = theta_gamma;

    return PR_ATTN_SUCCESS;
}

//=============================================================================
// Main Update Functions
//=============================================================================

pr_attn_error_t pr_attention_bridge_update(
    pr_attention_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        set_last_error("NULL bridge pointer");
        return PR_ATTN_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_update", 0.0f);


    pr_attn_error_t err;

    // 1. Compute bottom-up attention from perception
    err = pr_attention_bridge_compute_bottom_up(bridge);
    if (err != PR_ATTN_SUCCESS) {
        return err;
    }

    // 2. Compute top-down attention from memory
    err = pr_attention_bridge_compute_top_down(bridge);
    if (err != PR_ATTN_SUCCESS) {
        return err;
    }

    // 3. Fuse bottom-up and top-down
    err = pr_attention_bridge_fuse_attention(bridge);
    if (err != PR_ATTN_SUCCESS) {
        return err;
    }

    // 4. Apply inhibition of return
    if (bridge->config.enable_ior) {
        err = pr_attention_bridge_decay_ior(bridge, dt_ms);
        if (err != PR_ATTN_SUCCESS) {
            return err;
        }
        err = pr_attention_bridge_apply_ior(bridge);
        if (err != PR_ATTN_SUCCESS) {
            return err;
        }
    }

    // 5. Update oscillatory state
    bridge->alpha_suppression = pr_attention_bridge_compute_alpha_suppression(bridge);
    bridge->gamma_enhancement = bridge->config.gamma_enhancement_base *
                                (1.0f + pr_attention_bridge_get_mean_attention(bridge));

    // 6. Modulate by theta phase if enabled
    if (bridge->config.enable_theta_gamma && bridge->theta_gamma) {
        err = pr_attention_bridge_modulate_by_theta(bridge);
        if (err != PR_ATTN_SUCCESS) {
            // Non-fatal: continue without theta modulation
        }
    }

    // 7. Decay memory attention weights
    err = pr_attention_bridge_decay_memory_attention(bridge,
        bridge->config.memory_attention_decay);
    if (err != PR_ATTN_SUCCESS) {
        return err;
    }

    // 8. Mark attention peak for IOR
    if (bridge->config.enable_ior) {
        pr_attn_peak_t peak;
        err = pr_attention_bridge_get_attention_peak(bridge, &peak);
        if (err == PR_ATTN_SUCCESS && peak.attention_value > 0.5f) {
            pr_attention_bridge_mark_attended(bridge, peak.x, peak.y);
        }
    }

    // Update timestamp and stats
    bridge->last_update_ms = get_current_time_ms();
    if (bridge->config.track_statistics) {
        bridge->stats.total_updates++;
    }

    return PR_ATTN_SUCCESS;
}

pr_attn_error_t pr_attention_bridge_compute_bottom_up(
    pr_attention_bridge_t* bridge
) {
    if (!bridge) {
        set_last_error("NULL bridge pointer");
        return PR_ATTN_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_compute_bottom_up", 0.0f);


    size_t map_size = bridge->spatial_width * bridge->spatial_height;
    float* bu_map = bridge->bottom_up_map;

    // Initialize bottom-up map to zero
    memset(bu_map, 0, map_size * bridge->temporal_depth * sizeof(float));

    // Weight normalization
    float total_weight = bridge->config.visual_weight +
                         bridge->config.audio_weight +
                         bridge->config.speech_weight +
                         bridge->config.omni_weight;
    if (total_weight < PR_ATTN_EPSILON) {
        total_weight = 1.0f;
    }

    // For now, generate synthetic bottom-up attention based on connected bridges
    // In a real implementation, this would query the actual perception bridges

    // Visual contribution (if connected)
    if (bridge->visual_bridge) {
        float visual_weight = bridge->config.visual_weight / (fabsf(total_weight) > 1e-7f ? total_weight : 1e-7f);
        // Synthetic: center-weighted attention
        for (size_t y = 0; y < bridge->spatial_height; y++) {
            /* Phase 8: Loop progress heartbeat */
            if ((y & 0xFF) == 0 && bridge->spatial_height > 256) {
                pr_attention_bridge_heartbeat("pr_attention_loop",
                                 (float)(y + 1) / (float)bridge->spatial_height);
            }

            for (size_t x = 0; x < bridge->spatial_width; x++) {
                /* Phase 8: Loop progress heartbeat */
                if ((x & 0xFF) == 0 && bridge->spatial_width > 256) {
                    pr_attention_bridge_heartbeat("pr_attention_loop",
                                     (float)(x + 1) / (float)bridge->spatial_width);
                }

                float cx = (float)x / bridge->spatial_width - 0.5f;
                float cy = (float)y / bridge->spatial_height - 0.5f;
                float dist = sqrtf(cx * cx + cy * cy);
                float val = expf(-dist * dist * 8.0f) * visual_weight;
                bu_map[y * bridge->spatial_width + x] += val;
            }
        }
    }

    // Audio contribution (if connected)
    if (bridge->audio_bridge) {
        float audio_weight = bridge->config.audio_weight / (fabsf(total_weight) > 1e-7f ? total_weight : 1e-7f);
        // Synthetic: uniform low-level attention (audio is non-spatial)
        for (size_t i = 0; i < map_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && map_size > 256) {
                pr_attention_bridge_heartbeat("pr_attention_loop",
                                 (float)(i + 1) / (float)map_size);
            }

            bu_map[i] += 0.1f * audio_weight;
        }
    }

    // Omni-sensory contribution (if connected)
    if (bridge->omni_bridge) {
        float omni_weight = bridge->config.omni_weight / (fabsf(total_weight) > 1e-7f ? total_weight : 1e-7f);
        // Synthetic: edge-enhanced attention
        for (size_t y = 1; y < bridge->spatial_height - 1; y++) {
            for (size_t x = 1; x < bridge->spatial_width - 1; x++) {
                // Simple edge detection kernel simulation
                float edge = fabsf(bu_map[(y-1) * bridge->spatial_width + x] -
                                   bu_map[(y+1) * bridge->spatial_width + x]) * 0.5f +
                             fabsf(bu_map[y * bridge->spatial_width + (x-1)] -
                                   bu_map[y * bridge->spatial_width + (x+1)]) * 0.5f;
                bu_map[y * bridge->spatial_width + x] += edge * omni_weight;
            }
        }
    }

    // Normalize to [0, 1]
    float max_val = 0.0f;
    for (size_t i = 0; i < map_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && map_size > 256) {
            pr_attention_bridge_heartbeat("pr_attention_loop",
                             (float)(i + 1) / (float)map_size);
        }

        if (bu_map[i] > max_val) max_val = bu_map[i];
    }
    if (max_val > PR_ATTN_EPSILON) {
        for (size_t i = 0; i < map_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && map_size > 256) {
                pr_attention_bridge_heartbeat("pr_attention_loop",
                                 (float)(i + 1) / (float)map_size);
            }

            bu_map[i] /= max_val;
        }
    }

    if (bridge->config.track_statistics) {
        bridge->stats.bottom_up_computations++;
    }

    return PR_ATTN_SUCCESS;
}

pr_attn_error_t pr_attention_bridge_compute_top_down(
    pr_attention_bridge_t* bridge
) {
    if (!bridge) {
        set_last_error("NULL bridge pointer");
        return PR_ATTN_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_compute_top_down", 0.0f);


    size_t map_size = bridge->spatial_width * bridge->spatial_height;
    float* td_map = bridge->top_down_map;

    // Initialize top-down map to zero
    memset(td_map, 0, map_size * bridge->temporal_depth * sizeof(float));

    // If no attended memories, top-down is uniform
    if (bridge->num_attended == 0) {
        for (size_t i = 0; i < map_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && map_size > 256) {
                pr_attention_bridge_heartbeat("pr_attention_loop",
                                 (float)(i + 1) / (float)map_size);
            }

            td_map[i] = 0.5f;  // Neutral top-down
        }
        return PR_ATTN_SUCCESS;
    }

    // For each attended memory, generate attention template
    for (size_t m = 0; m < bridge->num_attended; m++) {
        /* Phase 8: Loop progress heartbeat */
        if ((m & 0xFF) == 0 && bridge->num_attended > 256) {
            pr_attention_bridge_heartbeat("pr_attention_loop",
                             (float)(m + 1) / (float)bridge->num_attended);
        }

        pr_attn_memory_entry_t* entry = &bridge->attended_memories[m];
        if (!entry->node || entry->attention_weight < PR_ATTN_EPSILON) {
            continue;
        }

        // Generate template based on memory content
        // In real implementation, this would use memory signature to create template
        // Here we use a Gaussian blob at a pseudo-random location based on memory ID

        uint64_t mem_id = pr_memory_node_get_id(entry->node);
        // Use memory ID to deterministically position the attention template
        float cx = (float)((mem_id * 31) % bridge->spatial_width) / bridge->spatial_width;
        float cy = (float)((mem_id * 37) % bridge->spatial_height) / bridge->spatial_height;

        float weight = entry->attention_weight;
        float sigma = 0.15f;  // Template spread

        for (size_t y = 0; y < bridge->spatial_height; y++) {
            /* Phase 8: Loop progress heartbeat */
            if ((y & 0xFF) == 0 && bridge->spatial_height > 256) {
                pr_attention_bridge_heartbeat("pr_attention_loop",
                                 (float)(y + 1) / (float)bridge->spatial_height);
            }

            for (size_t x = 0; x < bridge->spatial_width; x++) {
                /* Phase 8: Loop progress heartbeat */
                if ((x & 0xFF) == 0 && bridge->spatial_width > 256) {
                    pr_attention_bridge_heartbeat("pr_attention_loop",
                                     (float)(x + 1) / (float)bridge->spatial_width);
                }

                float dx = (float)x / bridge->spatial_width - cx;
                float dy = (float)y / bridge->spatial_height - cy;
                float dist_sq = dx * dx + dy * dy;
                float val = weight * expf(-dist_sq / (2.0f * sigma * sigma));
                td_map[y * bridge->spatial_width + x] += val;
            }
        }
    }

    // Normalize to [0, 1]
    float max_val = 0.0f;
    for (size_t i = 0; i < map_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && map_size > 256) {
            pr_attention_bridge_heartbeat("pr_attention_loop",
                             (float)(i + 1) / (float)map_size);
        }

        if (td_map[i] > max_val) max_val = td_map[i];
    }
    if (max_val > PR_ATTN_EPSILON) {
        for (size_t i = 0; i < map_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && map_size > 256) {
                pr_attention_bridge_heartbeat("pr_attention_loop",
                                 (float)(i + 1) / (float)map_size);
            }

            td_map[i] /= max_val;
        }
    }

    if (bridge->config.track_statistics) {
        bridge->stats.top_down_computations++;
    }

    return PR_ATTN_SUCCESS;
}

pr_attn_error_t pr_attention_bridge_fuse_attention(
    pr_attention_bridge_t* bridge
) {
    if (!bridge) {
        set_last_error("NULL bridge pointer");
        return PR_ATTN_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_fuse_attention", 0.0f);


    size_t map_size = bridge->spatial_width * bridge->spatial_height;
    float* unified = bridge->unified_attention_map;
    float* bu = bridge->bottom_up_map;
    float* td = bridge->top_down_map;
    float alpha = bridge->config.bu_td_balance;

    switch (bridge->config.fusion_mode) {
        case PR_ATTN_FUSE_WEIGHTED:
            // Weighted average: unified = alpha * BU + (1-alpha) * TD
            for (size_t i = 0; i < map_size; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && map_size > 256) {
                    pr_attention_bridge_heartbeat("pr_attention_loop",
                                     (float)(i + 1) / (float)map_size);
                }

                unified[i] = alpha * bu[i] + (1.0f - alpha) * td[i];
            }
            break;

        case PR_ATTN_FUSE_MAX:
            // Maximum: unified = max(BU, TD)
            for (size_t i = 0; i < map_size; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && map_size > 256) {
                    pr_attention_bridge_heartbeat("pr_attention_loop",
                                     (float)(i + 1) / (float)map_size);
                }

                unified[i] = (bu[i] > td[i]) ? bu[i] : td[i];
            }
            break;

        case PR_ATTN_FUSE_MULTIPLY:
            // Multiplicative: unified = BU * TD (AND-like)
            for (size_t i = 0; i < map_size; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && map_size > 256) {
                    pr_attention_bridge_heartbeat("pr_attention_loop",
                                     (float)(i + 1) / (float)map_size);
                }

                unified[i] = bu[i] * td[i];
            }
            break;

        case PR_ATTN_FUSE_BIASED_COMPETITION:
            // Biased competition: TD biases BU competition
            {
                // Compute mean TD as bias
                float mean_td = 0.0f;
                for (size_t i = 0; i < map_size; i++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((i & 0xFF) == 0 && map_size > 256) {
                        pr_attention_bridge_heartbeat("pr_attention_loop",
                                         (float)(i + 1) / (float)map_size);
                    }

                    mean_td += td[i];
                }
                mean_td /= map_size;

                // BU competes, biased by TD
                for (size_t i = 0; i < map_size; i++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((i & 0xFF) == 0 && map_size > 256) {
                        pr_attention_bridge_heartbeat("pr_attention_loop",
                                         (float)(i + 1) / (float)map_size);
                    }

                    float bias = (td[i] > mean_td) ? (td[i] / mean_td) : 1.0f;
                    unified[i] = bu[i] * nimcp_myelin_clamp(bias, 0.5f, 2.0f);
                }
            }
            break;

        default:
            // Default to weighted
            for (size_t i = 0; i < map_size; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && map_size > 256) {
                    pr_attention_bridge_heartbeat("pr_attention_loop",
                                     (float)(i + 1) / (float)map_size);
                }

                unified[i] = alpha * bu[i] + (1.0f - alpha) * td[i];
            }
            break;
    }

    // Normalize unified map to [0, 1]
    float max_val = 0.0f;
    for (size_t i = 0; i < map_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && map_size > 256) {
            pr_attention_bridge_heartbeat("pr_attention_loop",
                             (float)(i + 1) / (float)map_size);
        }

        if (unified[i] > max_val) max_val = unified[i];
    }
    if (max_val > PR_ATTN_EPSILON) {
        for (size_t i = 0; i < map_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && map_size > 256) {
                pr_attention_bridge_heartbeat("pr_attention_loop",
                                 (float)(i + 1) / (float)map_size);
            }

            unified[i] /= max_val;
        }
    }

    return PR_ATTN_SUCCESS;
}

//=============================================================================
// Resonance-Attention Functions
//=============================================================================

pr_attn_error_t pr_attention_bridge_apply_resonance_boost(
    pr_attention_bridge_t* bridge,
    const resonance_query_t* query,
    float* boost_output
) {
    if (!bridge || !boost_output) {
        set_last_error("NULL pointer argument");
        return PR_ATTN_ERROR_NULL_POINTER;
    }

    *boost_output = 0.0f;

    if (!query) {
        return PR_ATTN_SUCCESS;  // No query, no boost
    }

    // Compute mean resonance with attended memories
    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_apply_resonance_boos", 0.0f);


    float mean_resonance = 0.0f;
    size_t count = 0;

    for (size_t i = 0; i < bridge->num_attended; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_attended > 256) {
            pr_attention_bridge_heartbeat("pr_attention_loop",
                             (float)(i + 1) / (float)bridge->num_attended);
        }

        pr_attn_memory_entry_t* entry = &bridge->attended_memories[i];
        if (!entry->node) continue;

        // In a real implementation, compute resonance between query and memory
        // Here we use a placeholder based on memory state
        nimcp_quaternion_t mem_state = pr_memory_node_get_state(entry->node);
        float resonance = resonance_quaternion_similarity(query->quaternion, mem_state);

        mean_resonance += resonance;
        count++;
    }

    if (count > 0) {
        mean_resonance /= count;
    }

    // Apply resonance mode
    float boost = 0.0f;
    switch (bridge->config.resonance_mode) {
        case PR_ATTN_RESONANCE_LINEAR:
            boost = mean_resonance * bridge->config.resonance_attention_gain;
            break;

        case PR_ATTN_RESONANCE_U_SHAPED:
            boost = u_shaped_attention(
                mean_resonance,
                bridge->config.familiarity_threshold,
                bridge->config.novelty_threshold
            ) * bridge->config.resonance_attention_gain;
            break;

        case PR_ATTN_RESONANCE_INVERTED_U:
            boost = inverted_u_attention(
                mean_resonance,
                bridge->config.familiarity_threshold,
                bridge->config.novelty_threshold
            ) * bridge->config.resonance_attention_gain;
            break;

        case PR_ATTN_RESONANCE_ADAPTIVE:
            // Adaptive: use U-shaped for exploration, linear for exploitation
            {
                float exploration = 1.0f - pr_attention_bridge_get_mean_attention(bridge);
                float u_boost = u_shaped_attention(
                    mean_resonance,
                    bridge->config.familiarity_threshold,
                    bridge->config.novelty_threshold
                );
                float lin_boost = mean_resonance;
                boost = (exploration * u_boost + (1.0f - exploration) * lin_boost) *
                        bridge->config.resonance_attention_gain;
            }
            break;

        default:
            boost = mean_resonance * bridge->config.resonance_attention_gain;
            break;
    }

    *boost_output = nimcp_myelin_clamp(boost, 0.0f, 1.0f);

    if (bridge->config.track_statistics) {
        bridge->stats.mean_resonance_boost =
            0.99f * bridge->stats.mean_resonance_boost + 0.01f * (*boost_output);
    }

    return PR_ATTN_SUCCESS;
}

float pr_attention_bridge_compute_familiarity(
    pr_attention_bridge_t* bridge,
    const resonance_query_t* query
) {
    if (!bridge || !query) {
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_compute_familiarity", 0.0f);


    float max_resonance = 0.0f;

    for (size_t i = 0; i < bridge->num_attended; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_attended > 256) {
            pr_attention_bridge_heartbeat("pr_attention_loop",
                             (float)(i + 1) / (float)bridge->num_attended);
        }

        pr_attn_memory_entry_t* entry = &bridge->attended_memories[i];
        if (!entry->node) continue;

        nimcp_quaternion_t mem_state = pr_memory_node_get_state(entry->node);
        float resonance = resonance_quaternion_similarity(query->quaternion, mem_state);

        if (resonance > max_resonance) {
            max_resonance = resonance;
        }
    }

    return max_resonance;
}

float pr_attention_bridge_compute_novelty(
    pr_attention_bridge_t* bridge,
    const resonance_query_t* query
) {
    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_compute_novelty", 0.0f);


    float familiarity = pr_attention_bridge_compute_familiarity(bridge, query);
    if (familiarity < 0.0f) {
        return -1.0f;
    }
    return 1.0f - familiarity;
}

//=============================================================================
// Quaternion Update Functions
//=============================================================================

pr_attn_error_t pr_attention_bridge_update_quaternion_salience(
    pr_attention_bridge_t* bridge,
    pr_memory_node_t* node,
    size_t spatial_x,
    size_t spatial_y
) {
    if (!bridge || !node) {
        set_last_error("NULL pointer argument");
        return PR_ATTN_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_update_quaternion_sa", 0.0f);


    if (spatial_x >= bridge->spatial_width || spatial_y >= bridge->spatial_height) {
        set_last_error("Invalid spatial coordinates");
        return PR_ATTN_ERROR_INVALID_COORDS;
    }

    if (!bridge->config.enable_quaternion_update) {
        return PR_ATTN_SUCCESS;  // Updating disabled
    }

    // Get attention at location
    float attention = pr_attention_bridge_get_attention_at(
        bridge, spatial_x, spatial_y, 0);
    if (attention < 0.0f) {
        return PR_ATTN_ERROR_UPDATE_FAILED;
    }

    // Get current quaternion state
    nimcp_quaternion_t state = pr_memory_node_get_state(node);

    // Update salience (y component)
    state.y = nimcp_myelin_clamp(attention, 0.0f, 1.0f);

    // Apply state update
    pr_node_error_t err = pr_memory_node_update_state(node, state);
    if (err != PR_NODE_SUCCESS) {
        set_last_error("Failed to update quaternion state");
        return PR_ATTN_ERROR_UPDATE_FAILED;
    }

    if (bridge->config.track_statistics) {
        bridge->stats.quaternion_updates++;
    }

    return PR_ATTN_SUCCESS;
}

pr_attn_error_t pr_attention_bridge_modulate_encoding(
    pr_attention_bridge_t* bridge,
    const pr_memory_node_t* node,
    float base_strength,
    float* modulated_strength
) {
    if (!bridge || !modulated_strength) {
        set_last_error("NULL pointer argument");
        return PR_ATTN_ERROR_NULL_POINTER;
    }

    // Default: no modulation
    *modulated_strength = base_strength;

    if (!node) {
        return PR_ATTN_SUCCESS;
    }

    // Get attention weight for this memory
    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_modulate_encoding", 0.0f);


    float attention_weight = 0.5f;  // Default

    // If memory is in attended set, use its weight
    int idx = find_memory_index(bridge, node);
    if (idx >= 0) {
        attention_weight = bridge->attended_memories[idx].attention_weight;
    } else {
        // Use mean attention as proxy
        attention_weight = pr_attention_bridge_get_mean_attention(bridge);
    }

    // Modulate: higher attention = stronger encoding
    // Formula: modulated = base * attention^gamma
    // gamma < 1 for gentler modulation, > 1 for stronger
    float gamma = 0.5f;
    float modulation = powf(nimcp_myelin_clamp(attention_weight, 0.01f, 1.0f), gamma);

    *modulated_strength = nimcp_myelin_clamp(base_strength * modulation, 0.0f, 1.0f);

    return PR_ATTN_SUCCESS;
}

//=============================================================================
// Attention Query Functions
//=============================================================================

pr_attn_error_t pr_attention_bridge_get_attention_peak(
    const pr_attention_bridge_t* bridge,
    pr_attn_peak_t* peak
) {
    if (!bridge || !peak) {
        set_last_error("NULL pointer argument");
        return PR_ATTN_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_get_attention_peak", 0.0f);


    memset(peak, 0, sizeof(pr_attn_peak_t));

    float max_val = -FLT_MAX;
    size_t max_x = 0, max_y = 0, max_t = 0;

    for (size_t t = 0; t < bridge->temporal_depth; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && bridge->temporal_depth > 256) {
            pr_attention_bridge_heartbeat("pr_attention_loop",
                             (float)(t + 1) / (float)bridge->temporal_depth);
        }

        for (size_t y = 0; y < bridge->spatial_height; y++) {
            /* Phase 8: Loop progress heartbeat */
            if ((y & 0xFF) == 0 && bridge->spatial_height > 256) {
                pr_attention_bridge_heartbeat("pr_attention_loop",
                                 (float)(y + 1) / (float)bridge->spatial_height);
            }

            for (size_t x = 0; x < bridge->spatial_width; x++) {
                /* Phase 8: Loop progress heartbeat */
                if ((x & 0xFF) == 0 && bridge->spatial_width > 256) {
                    pr_attention_bridge_heartbeat("pr_attention_loop",
                                     (float)(x + 1) / (float)bridge->spatial_width);
                }

                size_t idx = attn_map_index(bridge, x, y, t);
                if (bridge->unified_attention_map[idx] > max_val) {
                    max_val = bridge->unified_attention_map[idx];
                    max_x = x;
                    max_y = y;
                    max_t = t;
                }
            }
        }
    }

    peak->x = max_x;
    peak->y = max_y;
    peak->t = max_t;
    peak->attention_value = max_val;

    // Determine dominant source
    float bu_val = bridge->bottom_up_map[attn_map_index(bridge, max_x, max_y, max_t)];
    float td_val = bridge->top_down_map[attn_map_index(bridge, max_x, max_y, max_t)];
    peak->dominant_source = (bu_val > td_val) ? PR_ATTN_SOURCE_VISUAL : PR_ATTN_SOURCE_MEMORY;

    if (bridge->config.track_statistics) {
        pr_attention_bridge_t* mutable_bridge = (pr_attention_bridge_t*)bridge;
        if (max_val > mutable_bridge->stats.max_attention_seen) {
            mutable_bridge->stats.max_attention_seen = max_val;
        }
    }

    return PR_ATTN_SUCCESS;
}

float pr_attention_bridge_get_attention_at(
    const pr_attention_bridge_t* bridge,
    size_t x,
    size_t y,
    size_t t
) {
    if (!bridge) {
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_get_attention_at", 0.0f);


    if (x >= bridge->spatial_width || y >= bridge->spatial_height ||
        t >= bridge->temporal_depth) {
        return -1.0f;
    }

    size_t idx = attn_map_index(bridge, x, y, t);
    return bridge->unified_attention_map[idx];
}

int pr_attention_bridge_get_top_k_peaks(
    const pr_attention_bridge_t* bridge,
    size_t k,
    pr_attn_peak_t* peaks
) {
    if (!bridge || !peaks || k == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_attention_bridge_get_top_k_peaks: required parameter is NULL (bridge, peaks)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_get_top_k_peaks", 0.0f);


    size_t map_size = bridge->spatial_width * bridge->spatial_height;
    size_t actual_k = (k < map_size) ? k : map_size;

    // Simple O(n*k) selection for small k
    // For large k, would use partial sort
    for (size_t i = 0; i < actual_k; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && actual_k > 256) {
            pr_attention_bridge_heartbeat("pr_attention_loop",
                             (float)(i + 1) / (float)actual_k);
        }

        peaks[i].attention_value = -FLT_MAX;
    }

    for (size_t y = 0; y < bridge->spatial_height; y++) {
        /* Phase 8: Loop progress heartbeat */
        if ((y & 0xFF) == 0 && bridge->spatial_height > 256) {
            pr_attention_bridge_heartbeat("pr_attention_loop",
                             (float)(y + 1) / (float)bridge->spatial_height);
        }

        for (size_t x = 0; x < bridge->spatial_width; x++) {
            /* Phase 8: Loop progress heartbeat */
            if ((x & 0xFF) == 0 && bridge->spatial_width > 256) {
                pr_attention_bridge_heartbeat("pr_attention_loop",
                                 (float)(x + 1) / (float)bridge->spatial_width);
            }

            float val = bridge->unified_attention_map[y * bridge->spatial_width + x];

            // Find position to insert
            for (size_t i = 0; i < actual_k; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && actual_k > 256) {
                    pr_attention_bridge_heartbeat("pr_attention_loop",
                                     (float)(i + 1) / (float)actual_k);
                }

                if (val > peaks[i].attention_value) {
                    // Shift down
                    for (size_t j = actual_k - 1; j > i; j--) {
                        peaks[j] = peaks[j - 1];
                    }
                    peaks[i].x = x;
                    peaks[i].y = y;
                    peaks[i].t = 0;
                    peaks[i].attention_value = val;
                    break;
                }
            }
        }
    }

    return (int)actual_k;
}

float pr_attention_bridge_get_mean_attention(
    const pr_attention_bridge_t* bridge
) {
    if (!bridge) {
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_get_mean_attention", 0.0f);


    size_t map_size = bridge->spatial_width * bridge->spatial_height *
                      bridge->temporal_depth;
    float sum = 0.0f;

    for (size_t i = 0; i < map_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && map_size > 256) {
            pr_attention_bridge_heartbeat("pr_attention_loop",
                             (float)(i + 1) / (float)map_size);
        }

        sum += bridge->unified_attention_map[i];
    }

    return sum / (fabsf(map_size) > 1e-7f ? map_size : 1e-7f);
}

//=============================================================================
// Inhibition of Return Functions
//=============================================================================

pr_attn_error_t pr_attention_bridge_apply_ior(
    pr_attention_bridge_t* bridge
) {
    if (!bridge) {
        set_last_error("NULL bridge pointer");
        return PR_ATTN_ERROR_NULL_POINTER;
    }

    if (!bridge->config.enable_ior || !bridge->ior_state.ior_map) {
        return PR_ATTN_SUCCESS;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_apply_ior", 0.0f);


    size_t map_size = bridge->spatial_width * bridge->spatial_height;
    float* unified = bridge->unified_attention_map;
    float* ior = bridge->ior_state.ior_map;

    // Apply IOR: attention = attention * (1 - ior)
    for (size_t i = 0; i < map_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && map_size > 256) {
            pr_attention_bridge_heartbeat("pr_attention_loop",
                             (float)(i + 1) / (float)map_size);
        }

        unified[i] *= (1.0f - ior[i]);
    }

    if (bridge->config.track_statistics) {
        bridge->stats.ior_applications++;
    }

    return PR_ATTN_SUCCESS;
}

pr_attn_error_t pr_attention_bridge_mark_attended(
    pr_attention_bridge_t* bridge,
    size_t x,
    size_t y
) {
    if (!bridge) {
        set_last_error("NULL bridge pointer");
        return PR_ATTN_ERROR_NULL_POINTER;
    }

    if (!bridge->config.enable_ior || !bridge->ior_state.ior_map) {
        return PR_ATTN_SUCCESS;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_mark_attended", 0.0f);


    if (x >= bridge->spatial_width || y >= bridge->spatial_height) {
        set_last_error("Invalid coordinates");
        return PR_ATTN_ERROR_INVALID_COORDS;
    }

    uint64_t now = get_current_time_ms();
    float radius = bridge->config.ior_radius;
    float strength = bridge->ior_state.strength;

    // Apply Gaussian inhibition around attended location
    for (size_t iy = 0; iy < bridge->spatial_height; iy++) {
        /* Phase 8: Loop progress heartbeat */
        if ((iy & 0xFF) == 0 && bridge->spatial_height > 256) {
            pr_attention_bridge_heartbeat("pr_attention_loop",
                             (float)(iy + 1) / (float)bridge->spatial_height);
        }

        for (size_t ix = 0; ix < bridge->spatial_width; ix++) {
            /* Phase 8: Loop progress heartbeat */
            if ((ix & 0xFF) == 0 && bridge->spatial_width > 256) {
                pr_attention_bridge_heartbeat("pr_attention_loop",
                                 (float)(ix + 1) / (float)bridge->spatial_width);
            }

            float dx = (float)ix - (float)x;
            float dy = (float)iy - (float)y;
            float dist_sq = dx * dx + dy * dy;
            float inhibition = strength * expf(-dist_sq / (2.0f * radius * radius));

            size_t idx = iy * bridge->spatial_width + ix;
            bridge->ior_state.ior_map[idx] = nimcp_myelin_clamp(
                bridge->ior_state.ior_map[idx] + inhibition, 0.0f, 1.0f);
            bridge->ior_state.last_attended_ms[idx] = now;
        }
    }

    return PR_ATTN_SUCCESS;
}

pr_attn_error_t pr_attention_bridge_decay_ior(
    pr_attention_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        set_last_error("NULL bridge pointer");
        return PR_ATTN_ERROR_NULL_POINTER;
    }

    if (!bridge->config.enable_ior || !bridge->ior_state.ior_map) {
        return PR_ATTN_SUCCESS;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_decay_ior", 0.0f);


    float decay = expf(-dt_ms / bridge->ior_state.decay_tau_ms);
    size_t map_size = bridge->ior_state.width * bridge->ior_state.height;

    for (size_t i = 0; i < map_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && map_size > 256) {
            pr_attention_bridge_heartbeat("pr_attention_loop",
                             (float)(i + 1) / (float)map_size);
        }

        bridge->ior_state.ior_map[i] *= decay;
    }

    return PR_ATTN_SUCCESS;
}

pr_attn_error_t pr_attention_bridge_clear_ior(
    pr_attention_bridge_t* bridge
) {
    if (!bridge) {
        set_last_error("NULL bridge pointer");
        return PR_ATTN_ERROR_NULL_POINTER;
    }

    if (!bridge->ior_state.ior_map) {
        return PR_ATTN_SUCCESS;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_clear_ior", 0.0f);


    size_t map_size = bridge->ior_state.width * bridge->ior_state.height;
    memset(bridge->ior_state.ior_map, 0, map_size * sizeof(float));
    memset(bridge->ior_state.last_attended_ms, 0, map_size * sizeof(uint64_t));

    return PR_ATTN_SUCCESS;
}

//=============================================================================
// Oscillatory Modulation Functions
//=============================================================================

float pr_attention_bridge_compute_alpha_suppression(
    pr_attention_bridge_t* bridge
) {
    if (!bridge) {
        return -1.0f;
    }

    // Alpha suppression proportional to mean attention
    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_compute_alpha_suppre", 0.0f);


    float mean_attn = pr_attention_bridge_get_mean_attention(bridge);
    if (mean_attn < 0.0f) {
        mean_attn = 0.5f;
    }

    // Higher attention = more alpha suppression
    float suppression = bridge->config.alpha_suppression_base +
                        mean_attn * (1.0f - bridge->config.alpha_suppression_base);

    return nimcp_myelin_clamp(suppression, 0.0f, 1.0f);
}

float pr_attention_bridge_get_gamma_enhancement(
    const pr_attention_bridge_t* bridge
) {
    if (!bridge) {
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_get_gamma_enhancemen", 0.0f);


    return bridge->gamma_enhancement;
}

pr_attn_error_t pr_attention_bridge_modulate_by_theta(
    pr_attention_bridge_t* bridge
) {
    if (!bridge) {
        set_last_error("NULL bridge pointer");
        return PR_ATTN_ERROR_NULL_POINTER;
    }

    if (!bridge->theta_gamma) {
        return PR_ATTN_SUCCESS;  // No theta-gamma connected, skip
    }

    // Get encoding strength from theta phase
    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_modulate_by_theta", 0.0f);


    float encode_strength = theta_gamma_get_encode_strength(bridge->theta_gamma);
    if (encode_strength < 0.0f) {
        return PR_ATTN_ERROR_THETA_GAMMA;
    }

    // Modulate attention by encoding phase
    // During encoding phase (high strength), boost attention
    size_t map_size = bridge->spatial_width * bridge->spatial_height;
    float modulation = 0.5f + 0.5f * encode_strength;

    for (size_t i = 0; i < map_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && map_size > 256) {
            pr_attention_bridge_heartbeat("pr_attention_loop",
                             (float)(i + 1) / (float)map_size);
        }

        bridge->unified_attention_map[i] *= modulation;
    }

    // Renormalize
    float max_val = 0.0f;
    for (size_t i = 0; i < map_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && map_size > 256) {
            pr_attention_bridge_heartbeat("pr_attention_loop",
                             (float)(i + 1) / (float)map_size);
        }

        if (bridge->unified_attention_map[i] > max_val) {
            max_val = bridge->unified_attention_map[i];
        }
    }
    if (max_val > PR_ATTN_EPSILON) {
        for (size_t i = 0; i < map_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && map_size > 256) {
                pr_attention_bridge_heartbeat("pr_attention_loop",
                                 (float)(i + 1) / (float)map_size);
            }

            bridge->unified_attention_map[i] /= max_val;
        }
    }

    return PR_ATTN_SUCCESS;
}

//=============================================================================
// Memory Attention Functions
//=============================================================================

pr_attn_error_t pr_attention_bridge_attend_memory(
    pr_attention_bridge_t* bridge,
    pr_memory_node_t* node,
    float initial_weight
) {
    if (!bridge || !node) {
        set_last_error("NULL pointer argument");
        return PR_ATTN_ERROR_NULL_POINTER;
    }

    // Check if already attended
    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_attend_memory", 0.0f);


    int idx = find_memory_index(bridge, node);
    if (idx >= 0) {
        // Update existing entry
        bridge->attended_memories[idx].attention_weight = initial_weight;
        bridge->attended_memories[idx].last_attended_ms = get_current_time_ms();
        bridge->attended_memories[idx].attend_count++;
        return PR_ATTN_SUCCESS;
    }

    // Check capacity
    if (bridge->num_attended >= bridge->max_attended) {
        set_last_error("Maximum attended memories exceeded");
        return PR_ATTN_ERROR_CAPACITY_EXCEEDED;
    }

    // Add new entry
    pr_attn_memory_entry_t* entry = &bridge->attended_memories[bridge->num_attended];
    entry->node = node;
    entry->attention_weight = nimcp_myelin_clamp(initial_weight, 0.0f, 1.0f);
    entry->resonance_score = 0.0f;
    entry->last_attended_ms = get_current_time_ms();
    entry->attend_count = 1;

    bridge->num_attended++;

    if (bridge->config.track_statistics) {
        bridge->stats.memories_attended++;
    }

    return PR_ATTN_SUCCESS;
}

pr_attn_error_t pr_attention_bridge_unattend_memory(
    pr_attention_bridge_t* bridge,
    pr_memory_node_t* node
) {
    if (!bridge || !node) {
        set_last_error("NULL pointer argument");
        return PR_ATTN_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_unattend_memory", 0.0f);


    int idx = find_memory_index(bridge, node);
    if (idx < 0) {
        return PR_ATTN_SUCCESS;  // Not found, already unattended
    }

    // Remove by shifting
    for (size_t i = (size_t)idx; i < bridge->num_attended - 1; i++) {
        bridge->attended_memories[i] = bridge->attended_memories[i + 1];
    }
    bridge->num_attended--;

    // Clear last entry
    memset(&bridge->attended_memories[bridge->num_attended], 0,
           sizeof(pr_attn_memory_entry_t));

    return PR_ATTN_SUCCESS;
}

pr_attn_error_t pr_attention_bridge_update_memory_attention(
    pr_attention_bridge_t* bridge,
    pr_memory_node_t* node,
    float new_weight
) {
    if (!bridge || !node) {
        set_last_error("NULL pointer argument");
        return PR_ATTN_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_update_memory_attent", 0.0f);


    int idx = find_memory_index(bridge, node);
    if (idx < 0) {
        set_last_error("Memory not in attended set");
        return PR_ATTN_ERROR_INVALID_STATE;
    }

    bridge->attended_memories[idx].attention_weight = nimcp_myelin_clamp(new_weight, 0.0f, 1.0f);
    bridge->attended_memories[idx].last_attended_ms = get_current_time_ms();

    return PR_ATTN_SUCCESS;
}

float pr_attention_bridge_get_memory_attention(
    const pr_attention_bridge_t* bridge,
    const pr_memory_node_t* node
) {
    if (!bridge || !node) {
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_get_memory_attention", 0.0f);


    int idx = find_memory_index(bridge, node);
    if (idx < 0) {
        return -1.0f;  // Not attended
    }

    return bridge->attended_memories[idx].attention_weight;
}

pr_attn_error_t pr_attention_bridge_decay_memory_attention(
    pr_attention_bridge_t* bridge,
    float decay_factor
) {
    if (!bridge) {
        set_last_error("NULL bridge pointer");
        return PR_ATTN_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_decay_memory_attenti", 0.0f);


    for (size_t i = 0; i < bridge->num_attended; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_attended > 256) {
            pr_attention_bridge_heartbeat("pr_attention_loop",
                             (float)(i + 1) / (float)bridge->num_attended);
        }

        bridge->attended_memories[i].attention_weight *= decay_factor;
    }

    // Remove memories with very low attention
    size_t write_idx = 0;
    for (size_t i = 0; i < bridge->num_attended; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_attended > 256) {
            pr_attention_bridge_heartbeat("pr_attention_loop",
                             (float)(i + 1) / (float)bridge->num_attended);
        }

        if (bridge->attended_memories[i].attention_weight > 0.01f) {
            if (write_idx != i) {
                bridge->attended_memories[write_idx] = bridge->attended_memories[i];
            }
            write_idx++;
        }
    }
    bridge->num_attended = write_idx;

    return PR_ATTN_SUCCESS;
}

pr_memory_node_t* pr_attention_bridge_get_most_attended_memory(
    const pr_attention_bridge_t* bridge
) {
    if (!bridge || bridge->num_attended == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_attention_bridge_get_most_attended_memory: bridge is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_get_most_attended_me", 0.0f);


    float max_weight = -1.0f;
    pr_memory_node_t* max_node = NULL;

    for (size_t i = 0; i < bridge->num_attended; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_attended > 256) {
            pr_attention_bridge_heartbeat("pr_attention_loop",
                             (float)(i + 1) / (float)bridge->num_attended);
        }

        if (bridge->attended_memories[i].attention_weight > max_weight) {
            max_weight = bridge->attended_memories[i].attention_weight;
            max_node = bridge->attended_memories[i].node;
        }
    }

    return max_node;
}

//=============================================================================
// Statistics Functions
//=============================================================================

pr_attn_error_t pr_attention_bridge_get_stats(
    const pr_attention_bridge_t* bridge,
    pr_attn_bridge_stats_t* stats
) {
    if (!bridge || !stats) {
        set_last_error("NULL pointer argument");
        return PR_ATTN_ERROR_NULL_POINTER;
    }

    *stats = bridge->stats;
    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_get_stats", 0.0f);


    return PR_ATTN_SUCCESS;
}

pr_attn_error_t pr_attention_bridge_reset_stats(
    pr_attention_bridge_t* bridge
) {
    if (!bridge) {
        set_last_error("NULL bridge pointer");
        return PR_ATTN_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_reset_stats", 0.0f);


    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.last_reset_time_ms = get_current_time_ms();

    return PR_ATTN_SUCCESS;
}

void pr_attention_bridge_print_state(const pr_attention_bridge_t* bridge) {
    if (!bridge) {
        printf("pr_attention_bridge: NULL\n");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_print_state", 0.0f);


    printf("=== PR Attention Bridge State ===\n");
    printf("Spatial dimensions: %zu x %zu\n",
           bridge->spatial_width, bridge->spatial_height);
    printf("Temporal depth: %zu\n", bridge->temporal_depth);
    printf("Mean attention: %.3f\n",
           pr_attention_bridge_get_mean_attention(bridge));
    printf("Alpha suppression: %.3f\n", bridge->alpha_suppression);
    printf("Gamma enhancement: %.3f\n", bridge->gamma_enhancement);
    printf("Attended memories: %zu / %zu\n",
           bridge->num_attended, bridge->max_attended);

    pr_attn_peak_t peak;
    if (pr_attention_bridge_get_attention_peak(bridge, &peak) == PR_ATTN_SUCCESS) {
        printf("Attention peak: (%zu, %zu) = %.3f\n",
               peak.x, peak.y, peak.attention_value);
    }

    printf("Stats:\n");
    printf("  Total updates: %lu\n", (unsigned long)bridge->stats.total_updates);
    printf("  BU computations: %lu\n",
           (unsigned long)bridge->stats.bottom_up_computations);
    printf("  TD computations: %lu\n",
           (unsigned long)bridge->stats.top_down_computations);
    printf("  Quaternion updates: %lu\n",
           (unsigned long)bridge->stats.quaternion_updates);
    printf("================================\n");
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* pr_attn_error_string(pr_attn_error_t error) {
    switch (error) {
        case PR_ATTN_SUCCESS:             return "Success";
        case PR_ATTN_ERROR_NULL_POINTER:  return "NULL pointer argument";
        case PR_ATTN_ERROR_INVALID_CONFIG:return "Invalid configuration";
        case PR_ATTN_ERROR_NO_MEMORY:     return "Memory allocation failed";
        case PR_ATTN_ERROR_INVALID_STATE: return "Invalid bridge state";
        case PR_ATTN_ERROR_NOT_CONNECTED: return "Required bridge not connected";
        case PR_ATTN_ERROR_CAPACITY_EXCEEDED: return "Maximum attended memories exceeded";
        case PR_ATTN_ERROR_INVALID_COORDS:return "Invalid spatial coordinates";
        case PR_ATTN_ERROR_UPDATE_FAILED: return "Update operation failed";
        case PR_ATTN_ERROR_THETA_GAMMA:   return "Theta-gamma operation failed";
        default:                          return "Unknown error";
    }
}

const char* pr_attn_get_last_error(void) {
    if (g_last_error[0] == '\0') {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_attn_get_last_error: validation failed");
        return NULL;
    }
    return g_last_error;
}

const char* pr_attn_source_name(pr_attn_source_t source) {
    switch (source) {
        case PR_ATTN_SOURCE_VISUAL:  return "VISUAL";
        case PR_ATTN_SOURCE_AUDIO:   return "AUDIO";
        case PR_ATTN_SOURCE_SPEECH:  return "SPEECH";
        case PR_ATTN_SOURCE_OMNI:    return "OMNI";
        case PR_ATTN_SOURCE_MEMORY:  return "MEMORY";
        default:                     return "UNKNOWN";
    }
}

const char* pr_attn_fusion_mode_name(pr_attn_fusion_mode_t mode) {
    switch (mode) {
        case PR_ATTN_FUSE_WEIGHTED:  return "WEIGHTED";
        case PR_ATTN_FUSE_MAX:       return "MAX";
        case PR_ATTN_FUSE_MULTIPLY:  return "MULTIPLY";
        case PR_ATTN_FUSE_BIASED_COMPETITION: return "BIASED_COMPETITION";
        default:                     return "UNKNOWN";
    }
}

uint64_t pr_attn_current_time_ms(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_attention_bridge_heartbeat("pr_attention_pr_attn_current_time", 0.0f);


    return get_current_time_ms();
}

//=============================================================================
// Instance Health Agent Setter (B25 Upgrade)
//=============================================================================

void pr_attention_bridge_set_instance_health_agent(
    pr_attention_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B25 Upgrade)
//=============================================================================

int pr_attention_bridge_training_begin(pr_attention_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_attention_bridge_training_begin: NULL argument");
        return -1;
    }
    pr_attention_bridge_heartbeat_instance(bridge->health_agent, "pr_attention_bridge_training_begin", 0.0f);
    return 0;
}

int pr_attention_bridge_training_end(pr_attention_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_attention_bridge_training_end: NULL argument");
        return -1;
    }
    pr_attention_bridge_heartbeat_instance(bridge->health_agent, "pr_attention_bridge_training_end", 1.0f);
    return 0;
}

int pr_attention_bridge_training_step(pr_attention_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_attention_bridge_training_step: NULL argument");
        return -1;
    }
    pr_attention_bridge_heartbeat_instance(bridge->health_agent, "pr_attention_bridge_training_step", progress);
    return 0;
}
