/**
 * @file nimcp_visual_cortical_bridge.h
 * @brief Visual-Cortical Bridge - Connects visual cortex with cortical columns
 *
 * WHAT: Integration layer that connects visual_cortex perception module with
 *       cortical column processing (orientation hypercolumns, retinotopic maps,
 *       cortical immune system).
 * WHY:  Provides biologically-realistic V1 processing by replacing visual_cortex's
 *       internal Gabor filtering with proper cortical column organization.
 * HOW:  Routes visual input through retinotopic mapping to orientation hypercolumns,
 *       applies cortical immune modulation, and uses bio-async messaging.
 *
 * ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                    VISUAL-CORTICAL BRIDGE                               │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │                                                                         │
 * │  visual_cortex_t                orientation_hypercolumn_t               │
 * │  ┌─────────────┐               ┌───────────────────────┐               │
 * │  │ Image Input │──────────────▶│ Gabor Filter Bank     │               │
 * │  │ V1 Filters  │    Retino-    │ Orientation Columns   │               │
 * │  │ Attention   │    topic      │ Pinwheel Organization │               │
 * │  └─────────────┘    Mapping    └───────────────────────┘               │
 * │        │                               │                                │
 * │        │                               ▼                                │
 * │        │           ┌───────────────────────────────┐                   │
 * │        └──────────▶│ topographic_map_t (Retinotopic)│                   │
 * │                    │ Log-polar mapping              │                   │
 * │                    │ Foveal magnification           │                   │
 * │                    └───────────────────────────────┘                   │
 * │                                │                                        │
 * │                                ▼                                        │
 * │                    ┌───────────────────────────────┐                   │
 * │                    │ cortical_immune_system_t       │                   │
 * │                    │ Microglial surveillance        │                   │
 * │                    │ Inflammation modulation        │                   │
 * │                    └───────────────────────────────┘                   │
 * │                                                                         │
 * └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * BIOLOGICAL BASIS:
 * - V1 visual cortex has columnar organization for orientation selectivity
 * - Retinotopic mapping provides spatial correspondence with visual field
 * - Cortical hypercolumns contain neurons tuned to all orientations
 * - Microglial surveillance monitors cortical health and modulates processing
 *
 * @version 1.0.0
 * @date 2025-12-19
 * @author NIMCP Development Team
 */

#ifndef NIMCP_VISUAL_CORTICAL_BRIDGE_H
#define NIMCP_VISUAL_CORTICAL_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Perception modules */
#include "perception/nimcp_visual_cortex.h"

/* Cortical column modules */
#include "core/cortical_columns/nimcp_orientation_columns.h"
#include "core/cortical_columns/nimcp_topographic_maps.h"
#include "core/cortical_columns/nimcp_cortical_immune.h"

/* Shared Gabor filter library */
#include "utils/gabor/nimcp_gabor.h"

/* Bio-async communication */
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

/* Memory management */
#include "utils/memory/nimcp_unified_memory.h"

/* Logging */
#include "utils/logging/nimcp_logging.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum number of orientation hypercolumns */
#define VISUAL_CORTICAL_MAX_HYPERCOLUMNS 256

/** Maximum number of retinotopic positions */
#define VISUAL_CORTICAL_MAX_POSITIONS 1024

/** Default number of orientations per hypercolumn */
#define VISUAL_CORTICAL_DEFAULT_ORIENTATIONS 8

/** Default spatial frequency for Gabor filters (wavelength = 1/freq, so 0.5 = 2 pixels) */
#define VISUAL_CORTICAL_DEFAULT_SPATIAL_FREQ 0.5f

/** Default tuning width in degrees */
#define VISUAL_CORTICAL_DEFAULT_TUNING_WIDTH 30.0f

/** Default immune inflammation modulation factor (0 = no immune influence) */
#define VISUAL_CORTICAL_DEFAULT_IMMUNE_FACTOR 0.0f

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Processing mode for the visual-cortical bridge
 */
typedef enum {
    VISUAL_CORTICAL_MODE_DIRECT = 0,     /**< Direct convolution (faster) */
    VISUAL_CORTICAL_MODE_HYPERCOLUMN,    /**< Full hypercolumn processing (more accurate) */
    VISUAL_CORTICAL_MODE_RETINOTOPIC,    /**< Include retinotopic mapping */
    VISUAL_CORTICAL_MODE_FULL            /**< All features enabled */
} visual_cortical_mode_t;

/**
 * @brief State of the visual-cortical bridge
 */
typedef enum {
    VISUAL_CORTICAL_STATE_UNINITIALIZED = 0,
    VISUAL_CORTICAL_STATE_READY,
    VISUAL_CORTICAL_STATE_PROCESSING,
    VISUAL_CORTICAL_STATE_ERROR
} visual_cortical_state_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Configuration for visual-cortical bridge
 */
typedef struct {
    /** Number of orientation hypercolumns to create */
    uint32_t num_hypercolumns;

    /** Number of orientations per hypercolumn (typically 8 or 16) */
    uint32_t orientations_per_hypercolumn;

    /** Spatial frequency for Gabor filters */
    float spatial_frequency;

    /** Orientation tuning width in degrees */
    float tuning_width;

    /** Processing mode */
    visual_cortical_mode_t mode;

    /** Enable retinotopic mapping */
    bool enable_retinotopic_mapping;

    /** Enable cortical immune integration */
    bool enable_cortical_immune;

    /** Enable bio-async messaging */
    bool enable_bio_async;

    /** Visual field coverage in degrees */
    float visual_field_degrees;

    /** Foveal radius in degrees (high-acuity central region) */
    float foveal_radius;

    /** Cortical magnification factor */
    float cortical_magnification;

    /** Immune modulation factor (0.0 = no effect, 1.0 = full effect) */
    float immune_modulation_factor;

    /** Use unified memory manager for allocations */
    bool use_umm;
} visual_cortical_config_t;

/**
 * @brief Statistics for visual-cortical bridge
 */
typedef struct {
    /** Total images processed */
    uint64_t images_processed;

    /** Total hypercolumn activations */
    uint64_t hypercolumn_activations;

    /** Total bio-async messages sent */
    uint64_t bio_messages_sent;

    /** Total bio-async messages received */
    uint64_t bio_messages_received;

    /** Average processing time in ms */
    float avg_processing_time_ms;

    /** Peak orientation response */
    float peak_orientation_response;

    /** Current dominant orientation */
    float current_dominant_orientation;

    /** Current immune modulation level */
    float current_immune_modulation;

    /** Number of active hypercolumns */
    uint32_t active_hypercolumns;
} visual_cortical_stats_t;

/**
 * @brief Result of orientation analysis
 */
typedef struct {
    /** Dominant orientation in degrees (0-180) */
    float dominant_orientation;

    /** Orientation selectivity index (0-1) */
    float selectivity_index;

    /** Orientation distribution [num_orientations] */
    float* orientation_responses;

    /** Number of orientations */
    uint32_t num_orientations;

    /** Confidence of orientation detection (0-1) */
    float confidence;

    /** Retinotopic position x (if mapped) */
    float retino_x;

    /** Retinotopic position y (if mapped) */
    float retino_y;
} visual_cortical_orientation_result_t;

/**
 * @brief Opaque handle to visual-cortical bridge instance
 */
typedef struct visual_cortical_bridge visual_cortical_bridge_t;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Initialize default configuration
 *
 * WHAT: Sets up default configuration for visual-cortical bridge.
 * WHY:  Provide sensible defaults for typical V1 processing.
 * HOW:  Fills config structure with biologically-plausible values.
 *
 * @param config Configuration structure to initialize
 */
void visual_cortical_default_config(visual_cortical_config_t* config);

/**
 * @brief Create visual-cortical bridge
 *
 * WHAT: Allocates and initializes the visual-cortical bridge.
 * WHY:  Connects visual perception with cortical column processing.
 * HOW:  Creates hypercolumns, retinotopic map, and immune connections.
 *
 * @param config Configuration (NULL for defaults)
 * @param visual_cortex Visual cortex instance to connect (may be NULL)
 * @return Bridge instance, or NULL on failure
 *
 * @note Caller must free with visual_cortical_bridge_destroy()
 */
visual_cortical_bridge_t* visual_cortical_bridge_create(
    const visual_cortical_config_t* config,
    visual_cortex_t* visual_cortex
);

/**
 * @brief Destroy visual-cortical bridge
 *
 * WHAT: Frees all resources associated with the bridge.
 * WHY:  Clean up memory and connections.
 * HOW:  Destroys hypercolumns, map, and disconnects immune.
 *
 * @param bridge Bridge to destroy (may be NULL)
 */
void visual_cortical_bridge_destroy(visual_cortical_bridge_t* bridge);

/* ============================================================================
 * Connection Functions
 * ============================================================================ */

/**
 * @brief Connect to visual cortex
 *
 * WHAT: Associates a visual cortex instance with this bridge.
 * WHY:  Enable bidirectional communication between perception and columns.
 * HOW:  Stores reference and sets up callbacks.
 *
 * @param bridge Visual-cortical bridge
 * @param visual_cortex Visual cortex to connect
 * @return 0 on success, non-zero on failure
 */
int visual_cortical_connect_visual_cortex(
    visual_cortical_bridge_t* bridge,
    visual_cortex_t* visual_cortex
);

/**
 * @brief Connect to cortical immune system
 *
 * WHAT: Associates cortical immune system for modulation.
 * WHY:  Enable inflammation-based processing modulation.
 * HOW:  Stores reference and registers for cytokine updates.
 *
 * @param bridge Visual-cortical bridge
 * @param immune Cortical immune system to connect
 * @return 0 on success, non-zero on failure
 */
int visual_cortical_connect_immune(
    visual_cortical_bridge_t* bridge,
    cortical_immune_system_t* immune
);

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Registers bridge with bio-async messaging system.
 * WHY:  Enable inter-module communication.
 * HOW:  Registers module and sets up message handlers.
 *
 * @param bridge Visual-cortical bridge
 * @return 0 on success, non-zero on failure
 */
int visual_cortical_connect_bio_async(visual_cortical_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregisters bridge from bio-async messaging system.
 * WHY:  Clean disconnection before shutdown.
 * HOW:  Unregisters module ID.
 *
 * @param bridge Visual-cortical bridge
 * @return 0 on success, non-zero on failure
 */
int visual_cortical_disconnect_bio_async(visual_cortical_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Visual-cortical bridge
 * @return true if connected, false otherwise
 */
bool visual_cortical_is_bio_async_connected(const visual_cortical_bridge_t* bridge);

/* ============================================================================
 * Processing Functions
 * ============================================================================ */

/**
 * @brief Process image through visual-cortical bridge
 *
 * WHAT: Routes image through retinotopic mapping and orientation hypercolumns.
 * WHY:  Biologically-realistic V1 processing with proper cortical organization.
 * HOW:  Maps image to cortical positions, processes through hypercolumns.
 *
 * @param bridge Visual-cortical bridge
 * @param image Grayscale image data (row-major, normalized 0-1)
 * @param width Image width in pixels
 * @param height Image height in pixels
 * @param result Output orientation analysis result
 * @return 0 on success, non-zero on failure
 *
 * @note Result's orientation_responses is allocated by this function.
 *       Caller must free with visual_cortical_free_result()
 */
int visual_cortical_process(
    visual_cortical_bridge_t* bridge,
    const float* image,
    uint32_t width,
    uint32_t height,
    visual_cortical_orientation_result_t* result
);

/**
 * @brief Process image patch at specific retinotopic position
 *
 * WHAT: Processes a single image patch through one hypercolumn.
 * WHY:  Efficient processing for attention-guided vision.
 * HOW:  Maps position to hypercolumn and processes.
 *
 * @param bridge Visual-cortical bridge
 * @param patch Image patch data
 * @param patch_width Patch width in pixels
 * @param patch_height Patch height in pixels
 * @param retino_x Retinotopic x position (visual field degrees)
 * @param retino_y Retinotopic y position (visual field degrees)
 * @param result Output orientation analysis result
 * @return 0 on success, non-zero on failure
 */
int visual_cortical_process_patch(
    visual_cortical_bridge_t* bridge,
    const float* patch,
    uint32_t patch_width,
    uint32_t patch_height,
    float retino_x,
    float retino_y,
    visual_cortical_orientation_result_t* result
);

/**
 * @brief Free orientation result resources
 *
 * WHAT: Frees memory allocated in orientation result.
 * WHY:  Clean up after processing.
 * HOW:  Frees orientation_responses array.
 *
 * @param result Result to free
 */
void visual_cortical_free_result(visual_cortical_orientation_result_t* result);

/**
 * @brief Get orientation map for entire image
 *
 * WHAT: Computes dominant orientation for each position in image.
 * WHY:  Generate orientation-based feature map.
 * HOW:  Processes all positions through hypercolumns.
 *
 * @param bridge Visual-cortical bridge
 * @param image Input image
 * @param width Image width
 * @param height Image height
 * @param orientation_map Output orientation map [height × width]
 * @param selectivity_map Optional selectivity map [height × width]
 * @return 0 on success, non-zero on failure
 */
int visual_cortical_get_orientation_map(
    visual_cortical_bridge_t* bridge,
    const float* image,
    uint32_t width,
    uint32_t height,
    float* orientation_map,
    float* selectivity_map
);

/* ============================================================================
 * Hypercolumn Functions
 * ============================================================================ */

/**
 * @brief Get hypercolumn at retinotopic position
 *
 * WHAT: Retrieves hypercolumn responsible for given visual field position.
 * WHY:  Access individual hypercolumn for analysis.
 * HOW:  Uses retinotopic mapping to find hypercolumn index.
 *
 * @param bridge Visual-cortical bridge
 * @param retino_x Retinotopic x position
 * @param retino_y Retinotopic y position
 * @return Hypercolumn pointer, or NULL if not found
 */
const orientation_hypercolumn_t* visual_cortical_get_hypercolumn(
    const visual_cortical_bridge_t* bridge,
    float retino_x,
    float retino_y
);

/**
 * @brief Get hypercolumn by index
 *
 * WHAT: Retrieves hypercolumn by array index.
 * WHY:  Direct access for iteration.
 * HOW:  Returns pointer from internal array.
 *
 * @param bridge Visual-cortical bridge
 * @param index Hypercolumn index
 * @return Hypercolumn pointer, or NULL if out of bounds
 */
const orientation_hypercolumn_t* visual_cortical_get_hypercolumn_by_index(
    const visual_cortical_bridge_t* bridge,
    uint32_t index
);

/**
 * @brief Get number of hypercolumns
 *
 * @param bridge Visual-cortical bridge
 * @return Number of hypercolumns, or 0 if bridge is NULL
 */
uint32_t visual_cortical_get_num_hypercolumns(const visual_cortical_bridge_t* bridge);

/* ============================================================================
 * Immune Modulation Functions
 * ============================================================================ */

/**
 * @brief Update immune modulation
 *
 * WHAT: Updates processing gains based on immune state.
 * WHY:  Apply inflammation effects to orientation processing.
 * HOW:  Queries immune system and adjusts gains.
 *
 * @param bridge Visual-cortical bridge
 * @return 0 on success, non-zero on failure
 */
int visual_cortical_update_immune_modulation(visual_cortical_bridge_t* bridge);

/**
 * @brief Set immune modulation factor
 *
 * WHAT: Manually sets immune modulation level.
 * WHY:  Allow direct control for testing or simulation.
 * HOW:  Sets internal modulation factor (0.0-1.0).
 *
 * @param bridge Visual-cortical bridge
 * @param factor Modulation factor (0.0 = no effect, 1.0 = full effect)
 * @return 0 on success, non-zero on failure
 */
int visual_cortical_set_immune_factor(
    visual_cortical_bridge_t* bridge,
    float factor
);

/**
 * @brief Get current immune modulation factor
 *
 * @param bridge Visual-cortical bridge
 * @return Current modulation factor, or 0.0 if bridge is NULL
 */
float visual_cortical_get_immune_factor(const visual_cortical_bridge_t* bridge);

/* ============================================================================
 * Statistics and State Functions
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * WHAT: Retrieves processing and performance statistics.
 * WHY:  Monitoring and debugging.
 * HOW:  Copies internal stats to output structure.
 *
 * @param bridge Visual-cortical bridge
 * @param stats Output statistics structure
 * @return 0 on success, non-zero on failure
 */
int visual_cortical_get_stats(
    const visual_cortical_bridge_t* bridge,
    visual_cortical_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * WHAT: Clears all statistics counters.
 * WHY:  Fresh start for benchmarking.
 * HOW:  Zeros stats structure.
 *
 * @param bridge Visual-cortical bridge
 * @return 0 on success, non-zero on failure
 */
int visual_cortical_reset_stats(visual_cortical_bridge_t* bridge);

/**
 * @brief Get bridge state
 *
 * @param bridge Visual-cortical bridge
 * @return Current state, or UNINITIALIZED if bridge is NULL
 */
visual_cortical_state_t visual_cortical_get_state(
    const visual_cortical_bridge_t* bridge
);

/**
 * @brief Get retinotopic map
 *
 * WHAT: Returns the internal retinotopic map.
 * WHY:  Allow external access for analysis.
 * HOW:  Returns pointer to internal map.
 *
 * @param bridge Visual-cortical bridge
 * @return Retinotopic map, or NULL if not enabled
 */
const topographic_map_t* visual_cortical_get_retinotopic_map(
    const visual_cortical_bridge_t* bridge
);

/* ============================================================================
 * Bio-Async Message Handling
 * ============================================================================ */

/**
 * @brief Process pending bio-async messages
 *
 * WHAT: Handles incoming messages from other modules.
 * WHY:  Respond to attention shifts, immune updates, etc.
 * HOW:  Processes inbox and invokes handlers.
 *
 * @param bridge Visual-cortical bridge
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t visual_cortical_process_bio_messages(
    visual_cortical_bridge_t* bridge,
    uint32_t max_messages
);

/**
 * @brief Broadcast orientation detection result
 *
 * WHAT: Sends orientation detection to interested modules.
 * WHY:  Notify downstream modules of visual features.
 * HOW:  Sends BIO_MSG_CORTICAL_ORIENTATION_DETECTED.
 *
 * @param bridge Visual-cortical bridge
 * @param result Orientation result to broadcast
 * @return 0 on success, non-zero on failure
 */
int visual_cortical_broadcast_orientation(
    visual_cortical_bridge_t* bridge,
    const visual_cortical_orientation_result_t* result
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VISUAL_CORTICAL_BRIDGE_H */
