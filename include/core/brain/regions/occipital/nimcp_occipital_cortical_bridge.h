/**
 * @file nimcp_occipital_cortical_bridge.h
 * @brief Bridge between Occipital Cortex and Cortical Columns
 *
 * WHAT: Connects occipital visual processing to cortical column organization
 * WHY: Enable biologically-realistic V1-V5 processing with columnar architecture
 * HOW: Routes visual features to orientation hypercolumns and topographic maps
 *
 * BIOLOGICAL BASIS:
 * - V1 has columnar organization for orientation selectivity (pinwheel structure)
 * - Hypercolumns span ~1mm and contain all orientation preferences
 * - Retinotopic mapping preserves visual field topology
 * - Ocular dominance columns separate left/right eye inputs
 * - Blob regions in V1 process color independent of orientation
 *
 * ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                 OCCIPITAL-CORTICAL BRIDGE                               │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │                                                                         │
 * │  occipital_adapter_t              orientation_hypercolumn_t             │
 * │  ┌─────────────────┐             ┌────────────────────────┐            │
 * │  │ V1 Edge Detector │────────────▶│ Orientation Columns    │            │
 * │  │ V4 Color/Form   │             │ Pinwheel Organization  │            │
 * │  │ V5/MT Motion    │             │ Ocular Dominance       │            │
 * │  └─────────────────┘             └────────────────────────┘            │
 * │         │                                  │                            │
 * │         │                                  ▼                            │
 * │         │              ┌─────────────────────────────────┐             │
 * │         └─────────────▶│ topographic_map_t (Retinotopic) │             │
 * │                        │ Log-polar mapping               │             │
 * │                        │ Foveal magnification            │             │
 * │                        └─────────────────────────────────┘             │
 * │                                    │                                    │
 * │                                    ▼                                    │
 * │                        ┌─────────────────────────────────┐             │
 * │                        │ minicolumn_t (Local circuits)   │             │
 * │                        │ Feature binding                  │             │
 * │                        │ Lateral inhibition               │             │
 * │                        └─────────────────────────────────┘             │
 * │                                                                         │
 * └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * @version Phase O1: Occipital Cortical Integration
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#ifndef NIMCP_OCCIPITAL_CORTICAL_BRIDGE_H
#define NIMCP_OCCIPITAL_CORTICAL_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/*=============================================================================
 * Forward Declarations
 *===========================================================================*/

typedef struct occipital_adapter occipital_adapter_t;
typedef struct orientation_hypercolumn orientation_hypercolumn_t;
typedef struct topographic_map topographic_map_t;
typedef struct cortical_immune_system cortical_immune_system_t;

/* Forward declare bio_router_struct for bio-async (defined in nimcp_bio_router.h) */
struct bio_router_struct;

/* Opaque bridge type */
typedef struct occipital_cortical_bridge occipital_cortical_bridge_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief Column type for feature processing
 */
typedef enum {
    OCC_COLUMN_ORIENTATION = 0,  /**< Orientation-selective (V1) */
    OCC_COLUMN_COLOR_BLOB,       /**< Color-selective blob (V1) */
    OCC_COLUMN_MOTION,           /**< Motion-selective (V5/MT) */
    OCC_COLUMN_FORM,             /**< Form-selective (V4) */
    OCC_COLUMN_DEPTH,            /**< Disparity-selective */
    OCC_COLUMN_MIXED             /**< Multi-feature column */
} occipital_column_type_t;

/**
 * @brief Retinotopic mapping mode
 */
typedef enum {
    OCC_MAP_LINEAR = 0,          /**< Simple linear mapping */
    OCC_MAP_LOG_POLAR,           /**< Log-polar (foveal magnification) */
    OCC_MAP_CORTICAL_UNFOLDING   /**< Full cortical surface unfolding */
} occipital_map_mode_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Column organization */
    uint32_t num_hypercolumns;       /**< Number of hypercolumns */
    uint32_t orientations_per_column; /**< Orientations per hypercolumn */
    float column_spacing_mm;          /**< Inter-column spacing */
    bool enable_ocular_dominance;     /**< Enable ocular dominance columns */
    bool enable_color_blobs;          /**< Enable color blob processing */

    /* Retinotopic mapping */
    occipital_map_mode_t map_mode;   /**< Mapping mode */
    float foveal_magnification;       /**< Foveal magnification factor */
    float eccentricity_scale;         /**< Eccentricity scaling */
    uint32_t map_resolution_x;        /**< Map resolution X */
    uint32_t map_resolution_y;        /**< Map resolution Y */

    /* Processing parameters */
    float lateral_inhibition;         /**< Lateral inhibition strength */
    float surround_suppression;       /**< Surround suppression radius */
    float contrast_gain;              /**< Contrast gain control */
    bool enable_bio_async;            /**< Enable bio-async messaging */

    /* Immune system integration */
    bool enable_cortical_immune;      /**< Enable cortical immune modulation */
    float inflammation_threshold;     /**< Inflammation response threshold */
} occipital_cortical_config_t;

/**
 * @brief Column activity state
 */
typedef struct {
    uint32_t column_id;              /**< Column identifier */
    occipital_column_type_t type;    /**< Column type */
    float position_x;                /**< Retinotopic X position */
    float position_y;                /**< Retinotopic Y position */
    float preferred_orientation;     /**< Preferred orientation (radians) */
    float activity;                  /**< Current activation [0-1] */
    float tuning_width;              /**< Tuning width (radians) */
    uint32_t eye_dominance;          /**< 0=left, 1=binocular, 2=right */
} column_activity_t;

/**
 * @brief Hypercolumn response
 */
typedef struct {
    uint32_t hypercolumn_id;         /**< Hypercolumn ID */
    float position_x;                /**< Retinotopic X */
    float position_y;                /**< Retinotopic Y */
    float orientation_responses[16]; /**< Response per orientation */
    uint32_t num_orientations;       /**< Number of orientations */
    float dominant_orientation;      /**< Dominant orientation */
    float population_activity;       /**< Total population activity */
    float color_response[4];         /**< RGBA color response (if blob) */
} hypercolumn_response_t;

/**
 * @brief Bridge effects on processing
 */
typedef struct {
    /* Column activation */
    float mean_column_activity;       /**< Mean column activation */
    float max_column_activity;        /**< Maximum column activation */
    float orientation_selectivity;    /**< Population orientation selectivity */
    float spatial_resolution;         /**< Effective spatial resolution */

    /* Retinotopic effects */
    float foveal_enhancement;         /**< Foveal region enhancement */
    float peripheral_suppression;     /**< Peripheral suppression */

    /* Immune effects */
    float inflammation_level;         /**< Current inflammation */
    float immune_modulation;          /**< Immune system modulation */

    /* Overall effects */
    float processing_efficiency;      /**< Column processing efficiency */
    float signal_quality;             /**< Output signal quality */
} occipital_cortical_effects_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t columns_activated;       /**< Columns activated */
    uint64_t hypercolumn_updates;     /**< Hypercolumn updates */
    uint64_t retinotopic_mappings;    /**< Retinotopic mappings performed */
    uint64_t immune_events;           /**< Immune system events */
    float avg_column_activity;        /**< Average column activity */
    float avg_orientation_response;   /**< Average orientation response */
    uint64_t messages_sent;           /**< Bio-async messages sent */
    uint64_t messages_received;       /**< Bio-async messages received */
} occipital_cortical_stats_t;

/*=============================================================================
 * Configuration API
 *===========================================================================*/

/**
 * @brief Get default configuration
 */
occipital_cortical_config_t occipital_cortical_default_config(void);

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

/**
 * @brief Create cortical bridge
 *
 * @param occipital Occipital adapter (required)
 * @param config Configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
occipital_cortical_bridge_t* occipital_cortical_bridge_create(
    occipital_adapter_t* occipital,
    const occipital_cortical_config_t* config);

/**
 * @brief Destroy cortical bridge
 */
void occipital_cortical_bridge_destroy(occipital_cortical_bridge_t* bridge);

/**
 * @brief Reset bridge state
 */
int occipital_cortical_bridge_reset(occipital_cortical_bridge_t* bridge);

/*=============================================================================
 * Connection API
 *===========================================================================*/

/**
 * @brief Connect to orientation hypercolumn system
 */
int occipital_cortical_connect_hypercolumns(
    occipital_cortical_bridge_t* bridge,
    orientation_hypercolumn_t* hypercolumns);

/**
 * @brief Connect to topographic map
 */
int occipital_cortical_connect_topographic_map(
    occipital_cortical_bridge_t* bridge,
    topographic_map_t* map);

/**
 * @brief Connect to cortical immune system
 */
int occipital_cortical_connect_immune(
    occipital_cortical_bridge_t* bridge,
    cortical_immune_system_t* immune);

/**
 * @brief Register with bio-async router
 */
int occipital_cortical_bridge_register_bio_async(
    occipital_cortical_bridge_t* bridge,
    struct bio_router_struct* router);

/*=============================================================================
 * Processing API
 *===========================================================================*/

/**
 * @brief Map visual location to cortical location
 *
 * @param bridge Bridge instance
 * @param visual_x Visual field X (normalized 0-1)
 * @param visual_y Visual field Y (normalized 0-1)
 * @param cortical_x Output: Cortical X coordinate
 * @param cortical_y Output: Cortical Y coordinate
 * @return 0 on success, -1 on failure
 */
int occipital_cortical_map_visual_to_cortical(
    occipital_cortical_bridge_t* bridge,
    float visual_x, float visual_y,
    float* cortical_x, float* cortical_y);

/**
 * @brief Process visual features through columns
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on failure
 */
int occipital_cortical_bridge_process(occipital_cortical_bridge_t* bridge);

/**
 * @brief Update bridge state
 */
int occipital_cortical_bridge_update(occipital_cortical_bridge_t* bridge);

/**
 * @brief Get column activity at location
 */
int occipital_cortical_get_column_activity(
    const occipital_cortical_bridge_t* bridge,
    float x, float y,
    column_activity_t* activity);

/**
 * @brief Get hypercolumn response
 */
int occipital_cortical_get_hypercolumn_response(
    const occipital_cortical_bridge_t* bridge,
    uint32_t hypercolumn_id,
    hypercolumn_response_t* response);

/**
 * @brief Get current effects
 */
int occipital_cortical_bridge_get_effects(
    const occipital_cortical_bridge_t* bridge,
    occipital_cortical_effects_t* effects);

/*=============================================================================
 * Query API
 *===========================================================================*/

/**
 * @brief Get bridge statistics
 */
int occipital_cortical_bridge_get_stats(
    const occipital_cortical_bridge_t* bridge,
    occipital_cortical_stats_t* stats);

/**
 * @brief Reset statistics
 */
void occipital_cortical_bridge_reset_stats(occipital_cortical_bridge_t* bridge);

/**
 * @brief Check if hypercolumns connected
 */
bool occipital_cortical_is_hypercolumns_connected(
    const occipital_cortical_bridge_t* bridge);

/**
 * @brief Check if topographic map connected
 */
bool occipital_cortical_is_map_connected(
    const occipital_cortical_bridge_t* bridge);

/**
 * @brief Get configuration
 */
int occipital_cortical_bridge_get_config(
    const occipital_cortical_bridge_t* bridge,
    occipital_cortical_config_t* config);

/**
 * @brief Get number of active columns
 */
uint32_t occipital_cortical_get_active_column_count(
    const occipital_cortical_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OCCIPITAL_CORTICAL_BRIDGE_H */
