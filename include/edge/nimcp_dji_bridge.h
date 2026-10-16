/**
 * @file nimcp_dji_bridge.h
 * @brief DJI OSDK Bridge — NIMCP brain <-> DJI Matrice flight controller.
 *
 * Two compilation modes:
 *   - NIMCP_HAS_DJI_OSDK defined: full DJI Onboard SDK integration (UART/USB,
 *     telemetry, position/velocity control, gimbal, camera)
 *   - NIMCP_HAS_DJI_OSDK not defined: stub mode with zeroed telemetry and
 *     logged-but-rejected commands. Allows testing the integration pipeline
 *     without hardware or DJI OSDK headers.
 *
 * Typical deployment: companion computer (Jetson/Manifold) running NIMCP brain
 * communicates with DJI Matrice flight controller over UART or USB.
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#ifndef NIMCP_DJI_BRIDGE_H
#define NIMCP_DJI_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Connection Type
 * ============================================================================ */

typedef enum {
    NIMCP_DJI_CONN_UART = 0,       /* /dev/ttyUSB0, /dev/ttyTHS1 */
    NIMCP_DJI_CONN_USB,            /* USB connection to flight controller */
} nimcp_dji_conn_type_t;

/* ============================================================================
 * DJI Flight Status
 * ============================================================================ */

typedef enum {
    NIMCP_DJI_STATUS_STOPPED = 0,
    NIMCP_DJI_STATUS_ON_GROUND,
    NIMCP_DJI_STATUS_IN_AIR,
} nimcp_dji_flight_status_t;

/* ============================================================================
 * DJI Bridge Configuration
 * ============================================================================ */

typedef struct {
    nimcp_dji_conn_type_t conn_type;
    char device_path[256];          /* e.g., "/dev/ttyUSB0" */
    uint32_t baud_rate;             /* Serial baud (default 921600) */

    /* DJI developer credentials */
    uint32_t app_id;
    char app_key[128];

    /* Safety */
    float geofence_radius_m;        /* Max distance from home (default 100) */
    float max_altitude_m;           /* Max altitude AGL (default 50) */
    float min_battery_pct;          /* RTL trigger (default 20) */
    bool enable_geofence;
    bool enable_altitude_limit;
    bool enable_battery_rtl;
} nimcp_dji_config_t;

/* ============================================================================
 * Opaque Handle
 * ============================================================================ */

typedef struct nimcp_dji_bridge nimcp_dji_bridge_t;

/* ============================================================================
 * Telemetry Data Types
 * ============================================================================ */

/** Attitude data (quaternion + Euler) from DJI flight controller */
typedef struct {
    float q0, q1, q2, q3;          /* Quaternion (w, x, y, z) */
    float roll;                     /* Degrees */
    float pitch;
    float yaw;
    uint64_t timestamp_us;
} nimcp_dji_attitude_t;

/** Position data from DJI flight controller */
typedef struct {
    double latitude;                /* Degrees */
    double longitude;
    float altitude_msl;             /* Meters above sea level */
    float altitude_rel;             /* Meters above takeoff point */
    float vx, vy, vz;              /* m/s in body frame */
    float heading;                  /* Degrees, 0=North */
    uint8_t gps_health;             /* 0-5, 5=best */
    uint8_t satellites;
    uint64_t timestamp_us;
} nimcp_dji_position_t;

/** Battery state from DJI flight controller */
typedef struct {
    float voltage;                  /* Volts */
    float current;                  /* Amps */
    float remaining_pct;            /* 0-100 */
    float temperature;              /* Celsius */
    uint64_t timestamp_us;
} nimcp_dji_battery_t;

/** Gimbal angle data */
typedef struct {
    float roll;                     /* Degrees */
    float pitch;
    float yaw;
    uint64_t timestamp_us;
} nimcp_dji_gimbal_t;

/* ============================================================================
 * Feature vector layout for brain input
 * ============================================================================ */

#define NIMCP_DJI_FEATURE_COUNT  16

/* Feature indices */
#define DJI_FEAT_ROLL               0   /* normalized [-1,1] by 180 */
#define DJI_FEAT_PITCH              1
#define DJI_FEAT_YAW                2
#define DJI_FEAT_VX                 3   /* m/s */
#define DJI_FEAT_VY                 4
#define DJI_FEAT_VZ                 5
#define DJI_FEAT_ALT_REL            6   /* meters above takeoff */
#define DJI_FEAT_HEADING            7   /* [0,1] normalized by 360 */
#define DJI_FEAT_BATTERY            8   /* [0,1] */
#define DJI_FEAT_GPS_HEALTH         9   /* [0,1] from 0-5 scale */
#define DJI_FEAT_DIST_HOME          10  /* meters from home */
#define DJI_FEAT_FLIGHT_STATUS      11  /* [0,1] — stopped/ground/air */
#define DJI_FEAT_GIMBAL_PITCH       12  /* [-1,1] by 90 */
#define DJI_FEAT_GIMBAL_YAW         13  /* [-1,1] by 180 */
#define DJI_FEAT_Q0                 14  /* quaternion w */
#define DJI_FEAT_Q1                 15  /* quaternion x */

/* ============================================================================
 * API — Lifecycle
 * ============================================================================ */

/**
 * @brief Create a DJI bridge with the given configuration.
 * @param config Pointer to configuration. NULL uses defaults.
 * @return Bridge handle, or NULL on allocation failure.
 */
nimcp_dji_bridge_t* nimcp_dji_bridge_create(const nimcp_dji_config_t* config);

/**
 * @brief Destroy a DJI bridge and free all resources. NULL-safe.
 */
void nimcp_dji_bridge_destroy(nimcp_dji_bridge_t* bridge);

/**
 * @brief Open the connection to the DJI flight controller.
 * @return 0 on success, -1 on failure.
 */
int nimcp_dji_bridge_connect(nimcp_dji_bridge_t* bridge);

/**
 * @brief Close the connection to the DJI flight controller.
 * @return 0 on success, -1 on failure.
 */
int nimcp_dji_bridge_disconnect(nimcp_dji_bridge_t* bridge);

/**
 * @brief Check if the bridge is currently connected.
 */
bool nimcp_dji_bridge_is_connected(const nimcp_dji_bridge_t* bridge);

/**
 * @brief Start the telemetry receive thread.
 * @return 0 on success, -1 on failure.
 */
int nimcp_dji_bridge_start(nimcp_dji_bridge_t* bridge);

/**
 * @brief Stop the telemetry receive thread.
 * @return 0 on success, -1 on failure.
 */
int nimcp_dji_bridge_stop(nimcp_dji_bridge_t* bridge);

/* ============================================================================
 * API — Telemetry Getters (thread-safe, return latest cached values)
 * ============================================================================ */

int nimcp_dji_get_attitude(const nimcp_dji_bridge_t* bridge, nimcp_dji_attitude_t* att);
int nimcp_dji_get_position(const nimcp_dji_bridge_t* bridge, nimcp_dji_position_t* pos);
int nimcp_dji_get_battery(const nimcp_dji_bridge_t* bridge, nimcp_dji_battery_t* bat);
int nimcp_dji_get_gimbal_angle(const nimcp_dji_bridge_t* bridge, nimcp_dji_gimbal_t* gimbal);

/* ============================================================================
 * API — Commands to Flight Controller
 * ============================================================================ */

int nimcp_dji_arm(nimcp_dji_bridge_t* bridge, bool arm);
int nimcp_dji_takeoff(nimcp_dji_bridge_t* bridge);
int nimcp_dji_land(nimcp_dji_bridge_t* bridge);
int nimcp_dji_goto_position(nimcp_dji_bridge_t* bridge,
                             double lat, double lon, float alt, float yaw);
int nimcp_dji_set_velocity(nimcp_dji_bridge_t* bridge,
                            float vx, float vy, float vz, float yaw_rate);
int nimcp_dji_set_gimbal(nimcp_dji_bridge_t* bridge,
                          float roll, float pitch, float yaw);
int nimcp_dji_trigger_photo(nimcp_dji_bridge_t* bridge);

/* ============================================================================
 * API — Brain Integration
 * ============================================================================ */

/**
 * @brief Compose a brain-input feature vector from current DJI telemetry.
 *
 * Writes up to NIMCP_DJI_FEATURE_COUNT (16) features.
 * See DJI_FEAT_* defines for layout.
 *
 * @param bridge       Bridge handle.
 * @param features     Output float array (caller-allocated).
 * @param max_features Maximum number of floats to write.
 * @return Number of features written, or -1 on error.
 */
int nimcp_dji_compose_features(const nimcp_dji_bridge_t* bridge,
                                float* features, uint32_t max_features);

/* ============================================================================
 * API — Safety Checks
 * ============================================================================ */

/**
 * @brief Check if geofence is violated.
 * @return 1 if violated, 0 if within bounds, -1 on error.
 */
int nimcp_dji_check_geofence(const nimcp_dji_bridge_t* bridge);

/**
 * @brief Return a default DJI bridge configuration.
 */
nimcp_dji_config_t nimcp_dji_config_default(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DJI_BRIDGE_H */
