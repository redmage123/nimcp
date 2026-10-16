/**
 * @file nimcp_sensor.h
 * @brief Sensor Abstraction Layer — unified interface for heterogeneous sensors.
 *
 * Provides a sensor hub that aggregates readings from LIDAR, depth cameras,
 * IMU, encoders, force/torque sensors, bumpers, GPS, and custom sensors.
 * Readings are fed into the brain's sensory cortex as a composed feature vector.
 *
 * Thread-safe: multiple sensor drivers may submit readings concurrently.
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#ifndef NIMCP_SENSOR_H
#define NIMCP_SENSOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Sensor Type Enumeration
 * ============================================================================ */

typedef enum {
    NIMCP_SENSOR_LIDAR = 0,        /* 2D/3D point cloud */
    NIMCP_SENSOR_DEPTH_CAMERA,     /* Depth image (structured light/ToF) */
    NIMCP_SENSOR_RGB_CAMERA,       /* Color image */
    NIMCP_SENSOR_IMU,              /* Accelerometer + gyroscope + magnetometer */
    NIMCP_SENSOR_ENCODER,          /* Rotary/linear position encoder */
    NIMCP_SENSOR_FORCE_TORQUE,     /* 6-axis force/torque */
    NIMCP_SENSOR_BUMPER,           /* Contact/proximity binary */
    NIMCP_SENSOR_GPS,              /* Lat/lon/alt */
    NIMCP_SENSOR_ULTRASONIC,       /* Range finder */
    NIMCP_SENSOR_TEMPERATURE,      /* Thermal */
    NIMCP_SENSOR_BAROMETER,        /* Altitude via pressure */
    NIMCP_SENSOR_CUSTOM,           /* User-defined */
    NIMCP_SENSOR_TYPE_COUNT
} nimcp_sensor_type_t;

/* ============================================================================
 * Sensor Data Format
 * ============================================================================ */

typedef enum {
    NIMCP_SENSOR_FMT_SCALAR,       /* Single float value */
    NIMCP_SENSOR_FMT_VECTOR3,      /* 3-float vector (x,y,z) */
    NIMCP_SENSOR_FMT_VECTOR6,      /* 6-float vector (e.g., force+torque) */
    NIMCP_SENSOR_FMT_QUATERNION,   /* 4-float quaternion (w,x,y,z) */
    NIMCP_SENSOR_FMT_POINT_CLOUD,  /* N x 3 float array */
    NIMCP_SENSOR_FMT_IMAGE,        /* W x H x C uint8 array (stored as float) */
    NIMCP_SENSOR_FMT_FLOAT_ARRAY,  /* Generic float array */
    NIMCP_SENSOR_FMT_BOOL          /* Single bool (bumper) */
} nimcp_sensor_format_t;

/* ============================================================================
 * Sensor Reading
 * ============================================================================ */

typedef struct {
    nimcp_sensor_type_t type;
    nimcp_sensor_format_t format;
    uint32_t sensor_id;            /* Unique ID within the hub */
    uint64_t timestamp_us;         /* Microsecond timestamp */
    float* data;                   /* Sensor data (format-dependent) */
    uint32_t data_count;           /* Number of floats in data */
    float confidence;              /* 0-1 reading confidence */
    bool valid;                    /* False if sensor error/timeout */
} nimcp_sensor_reading_t;

/* ============================================================================
 * Sensor Descriptor (registered at init)
 * ============================================================================ */

typedef struct {
    uint32_t sensor_id;
    nimcp_sensor_type_t type;
    nimcp_sensor_format_t format;
    char name[64];
    float sample_rate_hz;          /* Expected update rate */
    uint32_t max_data_count;       /* Max floats per reading */
    float noise_stddev;            /* Expected noise level */

    /* Transform relative to body frame */
    float position[3];             /* x,y,z offset from body center */
    float orientation[4];          /* quaternion w,x,y,z */
} nimcp_sensor_descriptor_t;

/* ============================================================================
 * Sensor Callback (async mode)
 * ============================================================================ */

typedef void (*nimcp_sensor_callback_t)(const nimcp_sensor_reading_t* reading,
                                        void* user_data);

/* ============================================================================
 * Opaque Sensor Hub Handle
 * ============================================================================ */

typedef struct nimcp_sensor_hub nimcp_sensor_hub_t;

/* ============================================================================
 * Sensor Hub Lifecycle
 * ============================================================================ */

/**
 * @brief Create a sensor hub with capacity for max_sensors sensors.
 * @param max_sensors Maximum number of sensors that can be registered.
 * @return Sensor hub handle, or NULL on failure.
 */
nimcp_sensor_hub_t* nimcp_sensor_hub_create(uint32_t max_sensors);

/**
 * @brief Destroy sensor hub and free all resources. NULL-safe.
 */
void nimcp_sensor_hub_destroy(nimcp_sensor_hub_t* hub);

/* ============================================================================
 * Sensor Registration
 * ============================================================================ */

/**
 * @brief Register a sensor with the hub.
 * @param hub Sensor hub handle.
 * @param descriptor Sensor descriptor (copied internally).
 * @return Assigned sensor_id (>= 0) on success, -1 on error.
 */
int nimcp_sensor_register(nimcp_sensor_hub_t* hub,
                           const nimcp_sensor_descriptor_t* descriptor);

/**
 * @brief Unregister a sensor from the hub.
 * @return 0 on success, -1 on error.
 */
int nimcp_sensor_unregister(nimcp_sensor_hub_t* hub, uint32_t sensor_id);

/* ============================================================================
 * Reading Submission & Retrieval
 * ============================================================================ */

/**
 * @brief Submit a new sensor reading (from driver/ROS callback).
 * Data is copied internally; caller retains ownership of the reading.
 * Thread-safe.
 * @return 0 on success, -1 on error.
 */
int nimcp_sensor_submit_reading(nimcp_sensor_hub_t* hub,
                                 const nimcp_sensor_reading_t* reading);

/**
 * @brief Get the most recent reading for a specific sensor.
 * @param reading_out Output reading. Data pointer is set to an internal buffer
 *        that is valid until the next submit for that sensor.
 * @return 0 on success, -1 on error or no reading available.
 */
int nimcp_sensor_get_latest(nimcp_sensor_hub_t* hub, uint32_t sensor_id,
                             nimcp_sensor_reading_t* reading_out);

/**
 * @brief Get the latest reading for all registered sensors.
 * @param readings_out Output array (caller-allocated).
 * @param max_count Maximum number of readings to return.
 * @return Number of readings written, or -1 on error.
 */
int nimcp_sensor_get_all_latest(nimcp_sensor_hub_t* hub,
                                 nimcp_sensor_reading_t* readings_out,
                                 uint32_t max_count);

/* ============================================================================
 * Async Callback
 * ============================================================================ */

/**
 * @brief Set a callback to be invoked on each new reading for a sensor.
 * Pass NULL callback to clear.
 * @return 0 on success, -1 on error.
 */
int nimcp_sensor_set_callback(nimcp_sensor_hub_t* hub, uint32_t sensor_id,
                               nimcp_sensor_callback_t callback, void* user_data);

/* ============================================================================
 * Feature Vector Composition
 * ============================================================================ */

/**
 * @brief Compose a single feature vector from all sensor readings.
 *
 * Iterates all registered sensors in sensor_id order and concatenates
 * their latest readings into a contiguous float array suitable for brain
 * input. Applies per-type normalization:
 *   - IMU accel: divide by 9.81 (normalize to g's)
 *   - GPS lat/lon: convert to local meters from reference point
 *   - Bumper: 0.0 or 1.0
 *   - Everything else: pass through raw
 *
 * @param hub Sensor hub handle.
 * @param features_out Output float array (caller-allocated).
 * @param max_features Maximum number of floats to write.
 * @return Number of features written, or -1 on error.
 */
int nimcp_sensor_compose_feature_vector(nimcp_sensor_hub_t* hub,
                                         float* features_out,
                                         uint32_t max_features);

/* ============================================================================
 * Query Functions
 * ============================================================================ */

/**
 * @brief Get the number of currently registered sensors.
 */
uint32_t nimcp_sensor_get_count(const nimcp_sensor_hub_t* hub);

/**
 * @brief Get the descriptor for a registered sensor.
 * @return 0 on success, -1 on error.
 */
int nimcp_sensor_get_descriptor(const nimcp_sensor_hub_t* hub, uint32_t sensor_id,
                                 nimcp_sensor_descriptor_t* desc_out);

/**
 * @brief Return a human-readable string name for a sensor type.
 * @return Static string, never NULL.
 */
const char* nimcp_sensor_type_name(nimcp_sensor_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SENSOR_H */
