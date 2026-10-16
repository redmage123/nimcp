/**
 * @file nimcp_portia_sensor_fusion.h
 * @brief Lightweight sensor fusion for Portia spider multi-modal integration
 *
 * Implements efficient sensor fusion algorithms inspired by Portia spider's
 * ability to integrate visual, vibrational, and chemical cues with minimal
 * neural resources. Supports both simple weighted averaging and Extended
 * Kalman Filter approaches.
 */

#ifndef NIMCP_PORTIA_SENSOR_FUSION_H
#define NIMCP_PORTIA_SENSOR_FUSION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Supported sensor types for fusion
 */
typedef enum {
    SENSOR_TYPE_VISUAL,      // Visual/camera data
    SENSOR_TYPE_AUDIO,       // Audio/acoustic sensors
    SENSOR_TYPE_VIBRATION,   // Vibration/seismic sensors
    SENSOR_TYPE_CHEMICAL,    // Chemical/olfactory sensors
    SENSOR_TYPE_THERMAL,     // Thermal/infrared sensors
    SENSOR_TYPE_PROXIMITY,   // Proximity/distance sensors
    SENSOR_TYPE_IMU,         // Inertial measurement unit
    SENSOR_TYPE_GPS,         // GPS/location sensors
    SENSOR_TYPE_COUNT
} sensor_type_t;

/**
 * Individual sensor reading with metadata
 */
typedef struct {
    sensor_type_t type;
    float value;             // Primary sensor value
    float confidence;        // Reading confidence [0.0-1.0]
    uint64_t timestamp_ms;   // Reading timestamp
    bool valid;              // Reading validity flag
} sensor_reading_t;

/**
 * Fused state estimate with metadata
 */
typedef struct {
    float x, y, z;               // Estimated position
    float vx, vy, vz;            // Estimated velocity
    float heading;               // Direction (radians)
    float confidence;            // Overall confidence [0.0-1.0]
    uint64_t timestamp_ms;       // State timestamp
    uint32_t contributing_sensors;  // Bitmask of active sensors
} fused_state_t;

/**
 * Per-sensor configuration
 */
typedef struct {
    sensor_type_t type;
    float weight;                // Fusion weight [0.0-1.0]
    float noise_variance;        // Expected measurement noise
    uint32_t update_rate_hz;     // Expected update rate
    bool enabled;                // Sensor enabled flag
    bool required;               // Mandatory for fusion
} sensor_config_t;

/**
 * Overall fusion system configuration
 */
typedef struct {
    sensor_config_t sensors[SENSOR_TYPE_COUNT];
    float process_noise;         // State transition noise
    uint32_t fusion_rate_hz;     // Output update rate
    bool enable_kalman;          // Use Extended Kalman filter
    bool enable_fallback;        // Fallback to single sensor
    float outlier_threshold;     // Outlier rejection threshold (std devs)
    uint32_t min_sensors;        // Minimum sensors for fusion
} portia_fusion_config_t;

/**
 * Fusion statistics for monitoring
 */
typedef struct {
    uint64_t total_updates;
    uint64_t successful_fusions;
    uint64_t outliers_rejected;
    uint64_t sensor_dropouts;
    float average_confidence;
    uint32_t active_sensor_count;
} portia_fusion_stats_t;

// Forward declarations
typedef struct portia_fusion_ctx portia_fusion_ctx_t;
typedef struct nimcp_bio_ctx nimcp_bio_ctx_t;

/**
 * Initialize sensor fusion system
 *
 * @param config Fusion configuration
 * @param bio_ctx Bio-async context for event broadcasting
 * @return Fusion context or NULL on failure
 */
portia_fusion_ctx_t* portia_fusion_init(
    const portia_fusion_config_t* config,
    nimcp_bio_ctx_t* bio_ctx
);

/**
 * Destroy fusion system and free resources
 *
 * @param ctx Fusion context
 */
void portia_fusion_destroy(portia_fusion_ctx_t* ctx);

/**
 * Update sensor reading
 *
 * @param ctx Fusion context
 * @param reading New sensor reading
 * @return true on success, false on failure
 */
bool portia_fusion_update_sensor(
    portia_fusion_ctx_t* ctx,
    const sensor_reading_t* reading
);

/**
 * Process fusion algorithm and update state estimate
 *
 * @param ctx Fusion context
 * @return true on success, false on failure
 */
bool portia_fusion_process(portia_fusion_ctx_t* ctx);

/**
 * Get current fused state estimate
 *
 * @param ctx Fusion context
 * @param state Output state estimate
 * @return true on success, false on failure
 */
bool portia_fusion_get_state(
    const portia_fusion_ctx_t* ctx,
    fused_state_t* state
);

/**
 * Set sensor weight for fusion
 *
 * @param ctx Fusion context
 * @param type Sensor type
 * @param weight New weight [0.0-1.0]
 * @return true on success, false on failure
 */
bool portia_fusion_set_weight(
    portia_fusion_ctx_t* ctx,
    sensor_type_t type,
    float weight
);

/**
 * Enable or disable a sensor
 *
 * @param ctx Fusion context
 * @param type Sensor type
 * @param enabled Enable flag
 * @return true on success, false on failure
 */
bool portia_fusion_enable_sensor(
    portia_fusion_ctx_t* ctx,
    sensor_type_t type,
    bool enabled
);

/**
 * Get overall fusion confidence
 *
 * @param ctx Fusion context
 * @return Confidence value [0.0-1.0]
 */
float portia_fusion_get_confidence(const portia_fusion_ctx_t* ctx);

/**
 * Get fusion statistics
 *
 * @param ctx Fusion context
 * @param stats Output statistics structure
 * @return true on success, false on failure
 */
bool portia_fusion_get_stats(
    const portia_fusion_ctx_t* ctx,
    portia_fusion_stats_t* stats
);

/**
 * Reset fusion state
 *
 * @param ctx Fusion context
 * @return true on success, false on failure
 */
bool portia_fusion_reset(portia_fusion_ctx_t* ctx);

/**
 * Get sensor name string
 *
 * @param type Sensor type
 * @return Sensor name string
 */
const char* portia_fusion_sensor_name(sensor_type_t type);

/**
 * Create default fusion configuration
 *
 * @return Default configuration
 */
portia_fusion_config_t portia_fusion_default_config(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PORTIA_SENSOR_FUSION_H
