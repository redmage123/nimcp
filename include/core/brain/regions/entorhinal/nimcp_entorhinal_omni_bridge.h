/**
 * @file nimcp_entorhinal_omni_bridge.h
 * @brief Entorhinal-Omnidirectional System Bidirectional Bridge
 * @version Phase 5: Memory Circuit
 * @date 2025-01-12
 *
 * WHAT: Bridge connecting entorhinal cortex to the omnidirectional
 *       spatial awareness system for 360-degree environmental modeling.
 *
 * WHY:  The omnidirectional system provides:
 *       - Complete surround spatial awareness
 *       - Threat detection from all directions
 *       - Opportunity sensing in full environment
 *       - Multi-modal spatial integration
 *
 * HOW:  Bidirectional data flow:
 *       - Omni -> Entorhinal: 360° spatial map, threat/opportunity vectors
 *       - Entorhinal -> Omni: grid cell representation, spatial memory
 *
 * BIOLOGICAL BASIS:
 * - Superior colliculus: multi-modal spatial maps
 * - Posterior parietal cortex: egocentric spatial awareness
 * - Entorhinal grid cells: allocentric spatial representation
 */

#ifndef NIMCP_ENTORHINAL_OMNI_BRIDGE_H
#define NIMCP_ENTORHINAL_OMNI_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/*=============================================================================
 * FORWARD DECLARATIONS
 *===========================================================================*/

typedef struct nimcp_entorhinal nimcp_entorhinal_t;
typedef struct nimcp_omnidirectional_system nimcp_omnidirectional_system_t;
typedef struct nimcp_brain_handle* nimcp_brain_t;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define OMNI_ANGULAR_RESOLUTION     360     /* 1 degree resolution */
#define OMNI_RADIAL_BINS            32      /* Distance bins */
#define OMNI_ELEVATION_BINS         18      /* -90 to +90 degrees */
#define OMNI_MAX_TRACKED_OBJECTS    64
#define OMNI_MAX_THREATS            16
#define OMNI_MAX_OPPORTUNITIES      16

/*=============================================================================
 * SPATIAL SECTOR ENUMERATION
 *===========================================================================*/

/**
 * @brief Cardinal and ordinal directions for coarse spatial binning
 */
typedef enum {
    OMNI_SECTOR_FRONT = 0,          /* 0° ± 22.5° */
    OMNI_SECTOR_FRONT_RIGHT,        /* 45° ± 22.5° */
    OMNI_SECTOR_RIGHT,              /* 90° ± 22.5° */
    OMNI_SECTOR_BACK_RIGHT,         /* 135° ± 22.5° */
    OMNI_SECTOR_BACK,               /* 180° ± 22.5° */
    OMNI_SECTOR_BACK_LEFT,          /* 225° ± 22.5° */
    OMNI_SECTOR_LEFT,               /* 270° ± 22.5° */
    OMNI_SECTOR_FRONT_LEFT,         /* 315° ± 22.5° */
    OMNI_SECTOR_COUNT
} omni_sector_t;

/**
 * @brief Vertical sectors
 */
typedef enum {
    OMNI_ELEVATION_BELOW = 0,       /* Below horizon */
    OMNI_ELEVATION_HORIZON,         /* At horizon level */
    OMNI_ELEVATION_ABOVE,           /* Above horizon */
    OMNI_ELEVATION_COUNT
} omni_elevation_t;

/*=============================================================================
 * OMNIDIRECTIONAL SPATIAL MAP
 *===========================================================================*/

/**
 * @brief 360-degree spatial awareness map
 */
typedef struct {
    /* High-resolution azimuthal map [0-359 degrees] */
    float azimuth_salience[OMNI_ANGULAR_RESOLUTION];
    float azimuth_distance[OMNI_ANGULAR_RESOLUTION];
    float azimuth_velocity[OMNI_ANGULAR_RESOLUTION];

    /* 2D polar map [angle x distance] */
    float polar_occupancy[OMNI_ANGULAR_RESOLUTION][OMNI_RADIAL_BINS];
    float polar_threat[OMNI_ANGULAR_RESOLUTION][OMNI_RADIAL_BINS];
    float polar_opportunity[OMNI_ANGULAR_RESOLUTION][OMNI_RADIAL_BINS];

    /* 3D spherical map [azimuth x elevation x distance] - compressed */
    float* spherical_map;           /* Full 3D if needed */
    bool spherical_enabled;

    /* Sector-level summaries */
    float sector_salience[OMNI_SECTOR_COUNT];
    float sector_threat[OMNI_SECTOR_COUNT];
    float sector_opportunity[OMNI_SECTOR_COUNT];
    float sector_familiarity[OMNI_SECTOR_COUNT];

    /* Global statistics */
    float mean_surround_activity;
    float max_threat_direction;
    float max_threat_distance;
    float max_opportunity_direction;
    float max_opportunity_distance;
} omni_spatial_map_t;

/*=============================================================================
 * TRACKED OBJECT STRUCTURE
 *===========================================================================*/

/**
 * @brief Object tracked in omnidirectional space
 */
typedef struct {
    uint32_t object_id;
    float azimuth;                  /* Direction in radians */
    float elevation;                /* Elevation angle */
    float distance;                 /* Distance in meters */
    float velocity_radial;          /* Approaching/receding speed */
    float velocity_tangential;      /* Angular velocity */
    float size_estimate;            /* Estimated size */
    float salience;                 /* Attention-worthiness */
    float threat_level;             /* Threat assessment */
    float opportunity_level;        /* Opportunity assessment */
    uint32_t category;              /* Object category */
    uint64_t first_seen_ms;         /* When first detected */
    uint64_t last_seen_ms;          /* When last updated */
    bool is_moving;
    bool is_approaching;
} omni_tracked_object_t;

/*=============================================================================
 * THREAT/OPPORTUNITY VECTORS
 *===========================================================================*/

/**
 * @brief Threat vector from omnidirectional analysis
 */
typedef struct {
    float direction;                /* Direction of threat */
    float distance;                 /* Distance to threat */
    float magnitude;                /* Threat level [0,1] */
    float velocity;                 /* Approach speed */
    float uncertainty;              /* Location uncertainty */
    uint32_t source_object_id;      /* Associated object if any */
    uint64_t detection_time_ms;
} omni_threat_vector_t;

/**
 * @brief Opportunity vector
 */
typedef struct {
    float direction;                /* Direction of opportunity */
    float distance;                 /* Distance to opportunity */
    float value;                    /* Estimated value */
    float accessibility;            /* How easy to reach */
    float urgency;                  /* Time pressure */
    uint32_t source_object_id;
    uint64_t detection_time_ms;
} omni_opportunity_vector_t;

/*=============================================================================
 * BRIDGE CONFIGURATION
 *===========================================================================*/

/**
 * @brief Omnidirectional bridge configuration
 */
typedef struct {
    /* Enable flags */
    bool enable_threat_integration;
    bool enable_opportunity_integration;
    bool enable_object_tracking;
    bool enable_3d_mapping;
    bool enable_memory_overlay;     /* Overlay memories on spatial map */

    /* Resolution settings */
    uint32_t angular_resolution;    /* Degrees of resolution */
    uint32_t radial_bins;
    float max_tracking_distance;
    float min_tracking_distance;

    /* Integration weights */
    float threat_weight;            /* Weight in grid cell modulation */
    float opportunity_weight;
    float salience_weight;
    float memory_familiarity_weight;

    /* Update settings */
    float spatial_map_update_rate_hz;
    float object_tracking_rate_hz;
    float memory_overlay_rate_hz;

    /* Thresholds */
    float threat_detection_threshold;
    float opportunity_detection_threshold;
    float object_tracking_threshold;
    float memory_retrieval_threshold;

    /* Attention settings */
    float attention_decay_rate;
    float novelty_boost;
    float threat_attention_boost;
} entorhinal_omni_config_t;

/*=============================================================================
 * BRIDGE STATE STRUCTURE
 *===========================================================================*/

/**
 * @brief Full omnidirectional bridge state
 */
typedef struct {
    /* Configuration */
    entorhinal_omni_config_t config;

    /* Connected systems */
    nimcp_entorhinal_t* entorhinal;
    nimcp_omnidirectional_system_t* omni;

    /* Spatial map */
    omni_spatial_map_t spatial_map;

    /* Object tracking */
    omni_tracked_object_t tracked_objects[OMNI_MAX_TRACKED_OBJECTS];
    uint32_t num_tracked_objects;

    /* Threat/opportunity vectors */
    omni_threat_vector_t threats[OMNI_MAX_THREATS];
    uint32_t num_threats;
    omni_opportunity_vector_t opportunities[OMNI_MAX_OPPORTUNITIES];
    uint32_t num_opportunities;

    /* Grid cell overlay - memory familiarity at each direction */
    float memory_familiarity[OMNI_ANGULAR_RESOLUTION];
    float memory_confidence[OMNI_ANGULAR_RESOLUTION];

    /* Current attention focus */
    float attention_direction;
    float attention_distance;
    float attention_strength;

    /* Escape/approach vectors */
    float escape_vector[3];         /* Best escape direction */
    float approach_vector[3];       /* Best approach direction */

    /* State tracking */
    bool connected;
    uint64_t last_update_ms;
    uint64_t updates_processed;

    /* Statistics */
    uint64_t threats_detected;
    uint64_t opportunities_detected;
    uint64_t objects_tracked_total;
    float mean_spatial_activity;
} entorhinal_omni_bridge_state_t;

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

/**
 * @brief Get default configuration
 */
entorhinal_omni_config_t entorhinal_omni_default_config(void);

/**
 * @brief Create omnidirectional bridge
 */
entorhinal_omni_bridge_state_t* entorhinal_omni_bridge_create(
    const entorhinal_omni_config_t* config);

/**
 * @brief Destroy omnidirectional bridge
 */
void entorhinal_omni_bridge_destroy(
    entorhinal_omni_bridge_state_t* bridge);

/**
 * @brief Connect bridge to entorhinal and omnidirectional system
 */
int entorhinal_omni_bridge_connect(
    entorhinal_omni_bridge_state_t* bridge,
    nimcp_entorhinal_t* entorhinal,
    nimcp_omnidirectional_system_t* omni);

/**
 * @brief Disconnect bridge
 */
int entorhinal_omni_bridge_disconnect(
    entorhinal_omni_bridge_state_t* bridge);

/**
 * @brief Reset bridge state
 */
int entorhinal_omni_bridge_reset(
    entorhinal_omni_bridge_state_t* bridge);

/*=============================================================================
 * BIDIRECTIONAL DATA FLOW API
 *===========================================================================*/

/**
 * @brief Update bridge (full bidirectional cycle)
 */
int entorhinal_omni_bridge_update(
    entorhinal_omni_bridge_state_t* bridge,
    float dt);

/**
 * @brief Receive spatial map from omnidirectional system
 */
int entorhinal_omni_receive_spatial_map(
    entorhinal_omni_bridge_state_t* bridge,
    const omni_spatial_map_t* map);

/**
 * @brief Send grid cell representation to omnidirectional system
 */
int entorhinal_omni_send_grid_representation(
    entorhinal_omni_bridge_state_t* bridge);

/**
 * @brief Receive threat vectors
 */
int entorhinal_omni_receive_threats(
    entorhinal_omni_bridge_state_t* bridge,
    const omni_threat_vector_t* threats,
    uint32_t num_threats);

/**
 * @brief Receive opportunity vectors
 */
int entorhinal_omni_receive_opportunities(
    entorhinal_omni_bridge_state_t* bridge,
    const omni_opportunity_vector_t* opportunities,
    uint32_t num_opportunities);

/**
 * @brief Send memory familiarity overlay
 */
int entorhinal_omni_send_memory_overlay(
    entorhinal_omni_bridge_state_t* bridge);

/*=============================================================================
 * SPATIAL QUERY API
 *===========================================================================*/

/**
 * @brief Get salience at direction
 */
float entorhinal_omni_get_salience_at_direction(
    const entorhinal_omni_bridge_state_t* bridge,
    float azimuth);

/**
 * @brief Get threat level at direction
 */
float entorhinal_omni_get_threat_at_direction(
    const entorhinal_omni_bridge_state_t* bridge,
    float azimuth);

/**
 * @brief Get opportunity level at direction
 */
float entorhinal_omni_get_opportunity_at_direction(
    const entorhinal_omni_bridge_state_t* bridge,
    float azimuth);

/**
 * @brief Get memory familiarity at direction
 */
float entorhinal_omni_get_familiarity_at_direction(
    const entorhinal_omni_bridge_state_t* bridge,
    float azimuth);

/**
 * @brief Get sector summary
 */
int entorhinal_omni_get_sector_summary(
    const entorhinal_omni_bridge_state_t* bridge,
    omni_sector_t sector,
    float* salience, float* threat, float* opportunity);

/*=============================================================================
 * OBJECT TRACKING API
 *===========================================================================*/

/**
 * @brief Get tracked object by ID
 */
const omni_tracked_object_t* entorhinal_omni_get_tracked_object(
    const entorhinal_omni_bridge_state_t* bridge,
    uint32_t object_id);

/**
 * @brief Get nearest threat
 */
int entorhinal_omni_get_nearest_threat(
    const entorhinal_omni_bridge_state_t* bridge,
    omni_threat_vector_t* threat_out);

/**
 * @brief Get best opportunity
 */
int entorhinal_omni_get_best_opportunity(
    const entorhinal_omni_bridge_state_t* bridge,
    omni_opportunity_vector_t* opportunity_out);

/**
 * @brief Get escape vector
 */
int entorhinal_omni_get_escape_vector(
    const entorhinal_omni_bridge_state_t* bridge,
    float* vector_out);

/**
 * @brief Get approach vector
 */
int entorhinal_omni_get_approach_vector(
    const entorhinal_omni_bridge_state_t* bridge,
    float* vector_out);

/*=============================================================================
 * ATTENTION API
 *===========================================================================*/

/**
 * @brief Set attention focus direction
 */
int entorhinal_omni_set_attention_focus(
    entorhinal_omni_bridge_state_t* bridge,
    float azimuth, float distance);

/**
 * @brief Get current attention focus
 */
int entorhinal_omni_get_attention_focus(
    const entorhinal_omni_bridge_state_t* bridge,
    float* azimuth_out, float* distance_out, float* strength_out);

/**
 * @brief Compute attention-weighted spatial representation
 */
int entorhinal_omni_compute_attended_representation(
    const entorhinal_omni_bridge_state_t* bridge,
    float* representation_out,
    uint32_t representation_size);

/*=============================================================================
 * GRID CELL INTEGRATION API
 *===========================================================================*/

/**
 * @brief Modulate grid cells based on omnidirectional input
 */
int entorhinal_omni_modulate_grid_cells(
    entorhinal_omni_bridge_state_t* bridge);

/**
 * @brief Get boundary signals for border cells
 */
int entorhinal_omni_get_boundary_signals(
    const entorhinal_omni_bridge_state_t* bridge,
    float* boundary_distances,
    float* boundary_directions,
    uint32_t max_boundaries,
    uint32_t* num_boundaries);

/*=============================================================================
 * DIAGNOSTICS API
 *===========================================================================*/

/**
 * @brief Get bridge statistics
 */
int entorhinal_omni_bridge_get_stats(
    const entorhinal_omni_bridge_state_t* bridge,
    uint64_t* updates_processed,
    uint64_t* threats_detected,
    uint64_t* opportunities_detected);

/**
 * @brief Log bridge diagnostics
 */
int entorhinal_omni_bridge_log_diagnostics(
    const entorhinal_omni_bridge_state_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ENTORHINAL_OMNI_BRIDGE_H */
