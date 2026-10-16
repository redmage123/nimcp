/**
 * @file nimcp_imagination_workspace.h
 * @brief Imagination Workspace - Active scenario buffer and mental canvas
 *
 * WHAT: Working buffer for active imagination scenarios
 * WHY:  Provides scratch space for scene construction and manipulation
 * HOW:  Manages latent state, visual/audio buffers, and trajectory history
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal working memory maintains imagined content
 * - Visual cortex provides "mental canvas" for visualization
 * - Hippocampus provides contextual binding of elements
 *
 * @author NIMCP Development Team
 * @date 2026-01-02
 * @version 2.6.3
 */

#ifndef NIMCP_IMAGINATION_WORKSPACE_H
#define NIMCP_IMAGINATION_WORKSPACE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Include required headers for complete types */
#include "utils/tensor/nimcp_tensor.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

/** @brief Maximum concurrent scenarios */
#define IMAGINATION_MAX_SCENARIOS           8

/** @brief Maximum trajectory length (scene states) */
#define IMAGINATION_MAX_TRAJECTORY          128

/** @brief Default latent dimension */
#define IMAGINATION_DEFAULT_LATENT_DIM      256

/** @brief Default visual buffer size (64x64 grayscale) */
#define IMAGINATION_DEFAULT_VISUAL_SIZE     4096

/** @brief Default audio buffer size (1024 samples) */
#define IMAGINATION_DEFAULT_AUDIO_SIZE      1024

/*============================================================================
 * Types
 *============================================================================*/

/** @brief Unique scenario identifier */
typedef uint64_t scenario_id_t;

/**
 * @brief Imagination workspace configuration
 */
typedef struct imagination_workspace_config {
    size_t max_scenarios;           /**< Maximum concurrent scenarios */
    size_t latent_dim;              /**< Latent space dimensionality */
    size_t visual_width;            /**< Visual buffer width */
    size_t visual_height;           /**< Visual buffer height */
    size_t visual_channels;         /**< Visual buffer channels (1=grayscale, 3=RGB) */
    size_t audio_samples;           /**< Audio buffer size in samples */
    size_t max_trajectory_length;   /**< Maximum trajectory length */
    bool enable_history;            /**< Enable trajectory history */
} imagination_workspace_config_t;

/**
 * @brief Workspace statistics
 */
typedef struct imagination_workspace_stats {
    uint64_t scenarios_created;     /**< Total scenarios created */
    uint64_t scenarios_active;      /**< Currently active scenarios */
    uint64_t steps_executed;        /**< Total scenario steps */
    uint64_t generations_visual;    /**< Visual generations performed */
    uint64_t generations_audio;     /**< Audio generations performed */
    float avg_scenario_duration_ms; /**< Average scenario duration */
    float memory_usage_mb;          /**< Current memory usage */
} imagination_workspace_stats_t;

/**
 * @brief Imagination workspace instance
 */
typedef struct imagination_workspace {
    imagination_workspace_config_t config;

    /* Scenario storage */
    void* scenarios;                /**< Active scenario array */
    size_t scenario_count;          /**< Current scenario count */
    scenario_id_t next_id;          /**< Next scenario ID */

    /* Shared buffers */
    nimcp_tensor_t* temp_latent;    /**< Temporary latent buffer */
    nimcp_tensor_t* temp_visual;    /**< Temporary visual buffer */
    nimcp_tensor_t* temp_audio;     /**< Temporary audio buffer */

    /* Statistics */
    imagination_workspace_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;
} imagination_workspace_t;

/*============================================================================
 * Configuration API
 *============================================================================*/

/**
 * @brief Get default workspace configuration
 *
 * @return Default configuration with sensible values
 */
imagination_workspace_config_t imagination_workspace_default_config(void);

/*============================================================================
 * Lifecycle API
 *============================================================================*/

/**
 * @brief Create imagination workspace
 *
 * WHAT: Allocate and initialize workspace with specified configuration
 * WHY:  Provides managed buffer space for imagination scenarios
 * HOW:  Allocate scenario array, shared buffers, and statistics
 *
 * @param config Configuration (NULL for defaults)
 * @return New workspace or NULL on failure
 */
imagination_workspace_t* imagination_workspace_create(
    const imagination_workspace_config_t* config);

/**
 * @brief Destroy imagination workspace
 *
 * @param workspace Workspace to destroy (NULL-safe)
 */
void imagination_workspace_destroy(imagination_workspace_t* workspace);

/**
 * @brief Reset workspace to initial state
 *
 * WHAT: Clear all active scenarios and reset statistics
 * WHY:  Allow fresh start without reallocation
 *
 * @param workspace Workspace to reset
 * @return 0 on success, negative on error
 */
int imagination_workspace_reset(imagination_workspace_t* workspace);

/*============================================================================
 * Scenario Management API
 *============================================================================*/

/**
 * @brief Allocate a new scenario slot
 *
 * WHAT: Reserve workspace slot for new scenario
 * WHY:  Manage scenario lifecycle and limit concurrent scenarios
 *
 * @param workspace Workspace
 * @return Scenario ID, or 0 on failure (workspace full)
 */
scenario_id_t imagination_workspace_allocate_scenario(
    imagination_workspace_t* workspace);

/**
 * @brief Release scenario slot
 *
 * @param workspace Workspace
 * @param id Scenario ID to release
 * @return 0 on success, negative on error
 */
int imagination_workspace_release_scenario(
    imagination_workspace_t* workspace,
    scenario_id_t id);

/**
 * @brief Check if scenario exists
 *
 * @param workspace Workspace
 * @param id Scenario ID
 * @return true if scenario exists and is active
 */
bool imagination_workspace_has_scenario(
    const imagination_workspace_t* workspace,
    scenario_id_t id);

/**
 * @brief Get number of active scenarios
 *
 * @param workspace Workspace
 * @return Number of active scenarios
 */
size_t imagination_workspace_active_count(
    const imagination_workspace_t* workspace);

/*============================================================================
 * Buffer Access API
 *============================================================================*/

/**
 * @brief Get latent buffer for scenario
 *
 * @param workspace Workspace
 * @param id Scenario ID
 * @return Latent tensor or NULL if not found
 */
nimcp_tensor_t* imagination_workspace_get_latent(
    imagination_workspace_t* workspace,
    scenario_id_t id);

/**
 * @brief Get visual buffer for scenario
 *
 * @param workspace Workspace
 * @param id Scenario ID
 * @return Visual tensor or NULL if not found
 */
nimcp_tensor_t* imagination_workspace_get_visual(
    imagination_workspace_t* workspace,
    scenario_id_t id);

/**
 * @brief Get audio buffer for scenario
 *
 * @param workspace Workspace
 * @param id Scenario ID
 * @return Audio tensor or NULL if not found
 */
nimcp_tensor_t* imagination_workspace_get_audio(
    imagination_workspace_t* workspace,
    scenario_id_t id);

/**
 * @brief Get temporary latent buffer (shared)
 *
 * @param workspace Workspace
 * @return Temporary latent buffer
 */
nimcp_tensor_t* imagination_workspace_get_temp_latent(
    imagination_workspace_t* workspace);

/**
 * @brief Get temporary visual buffer (shared)
 *
 * @param workspace Workspace
 * @return Temporary visual buffer
 */
nimcp_tensor_t* imagination_workspace_get_temp_visual(
    imagination_workspace_t* workspace);

/*============================================================================
 * Statistics API
 *============================================================================*/

/**
 * @brief Get workspace statistics
 *
 * @param workspace Workspace
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int imagination_workspace_get_stats(
    const imagination_workspace_t* workspace,
    imagination_workspace_stats_t* stats);

/**
 * @brief Reset workspace statistics
 *
 * @param workspace Workspace
 * @return 0 on success, negative on error
 */
int imagination_workspace_reset_stats(imagination_workspace_t* workspace);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_IMAGINATION_WORKSPACE_H */
