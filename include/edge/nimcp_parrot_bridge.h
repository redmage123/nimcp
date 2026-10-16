/**
 * @file nimcp_parrot_bridge.h
 * @brief Parrot Olympe Bridge — NIMCP brain <-> Parrot ANAFI drone.
 *
 * Two compilation modes:
 *   - NIMCP_HAS_OLYMPE defined: full Parrot arsdk-ng/Olympe C integration
 *     (Wi-Fi/USB, telemetry, position/velocity control, camera)
 *   - NIMCP_HAS_OLYMPE not defined: stub mode with zeroed telemetry and
 *     logged-but-rejected commands. Allows testing the integration pipeline
 *     without hardware or Olympe SDK.
 *
 * Typical deployment: companion computer connected to Parrot ANAFI via Wi-Fi
 * (default 192.168.42.1) running NIMCP brain for autonomous navigation.
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#ifndef NIMCP_PARROT_BRIDGE_H
#define NIMCP_PARROT_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Parrot Bridge Configuration
 * ============================================================================ */

typedef struct {
    char drone_ip[64];              /* Default "192.168.42.1" (ANAFI default) */
    uint16_t port;                  /* Default 44444 */

    /* Safety */
    float geofence_radius_m;        /* Max distance from home (default 100) */
    float max_altitude_m;           /* Max altitude AGL (default 50) */
    float min_battery_pct;          /* RTL trigger (default 20) */
    float max_tilt_degrees;         /* Max tilt angle (default 30) */
    bool enable_geofence;
    bool enable_altitude_limit;
    bool enable_battery_rtl;
} nimcp_parrot_config_t;

/* ============================================================================
 * Opaque Handle
 * ============================================================================ */

typedef struct nimcp_parrot_bridge nimcp_parrot_bridge_t;

/* ============================================================================
 * Telemetry Data Types
 * ============================================================================ */

/** Attitude data from Parrot ANAFI */
typedef struct {
    float roll;                     /* Degrees */
    float pitch;
    float yaw;
    uint64_t timestamp_us;
} nimcp_parrot_attitude_t;

/** Position data from Parrot ANAFI */
typedef struct {
    double latitude;                /* Degrees */
    double longitude;
    float altitude_msl;             /* Meters above sea level */
    float altitude_rel;             /* Meters above takeoff point */
    float heading;                  /* Degrees, 0=North */
    uint8_t gps_fix;                /* 0=none, 1=fix */
    uint8_t satellites;
    uint64_t timestamp_us;
} nimcp_parrot_position_t;

/** Battery state from Parrot ANAFI */
typedef struct {
    float remaining_pct;            /* 0-100 */
    float voltage;                  /* Volts */
    float temperature;              /* Celsius */
    uint64_t timestamp_us;
} nimcp_parrot_battery_t;

/** Speed data from Parrot ANAFI */
typedef struct {
    float speed_north;              /* m/s */
    float speed_east;
    float speed_down;
    float ground_speed;             /* m/s horizontal */
    uint64_t timestamp_us;
} nimcp_parrot_speed_t;

/* ============================================================================
 * Feature vector layout for brain input
 * ============================================================================ */

#define NIMCP_PARROT_FEATURE_COUNT  14

/* Feature indices */
#define PARROT_FEAT_ROLL            0   /* normalized [-1,1] by 180 */
#define PARROT_FEAT_PITCH           1
#define PARROT_FEAT_YAW             2
#define PARROT_FEAT_SPEED_N         3   /* m/s north */
#define PARROT_FEAT_SPEED_E         4   /* m/s east */
#define PARROT_FEAT_SPEED_D         5   /* m/s down */
#define PARROT_FEAT_ALT_REL         6   /* meters above takeoff */
#define PARROT_FEAT_HEADING         7   /* [0,1] normalized by 360 */
#define PARROT_FEAT_BATTERY         8   /* [0,1] */
#define PARROT_FEAT_GPS_FIX         9   /* [0,1] */
#define PARROT_FEAT_DIST_HOME       10  /* meters from home */
#define PARROT_FEAT_GROUND_SPEED    11  /* m/s */
#define PARROT_FEAT_TILT_MAGNITUDE  12  /* [0,1] combined roll+pitch */
#define PARROT_FEAT_SATELLITES      13  /* [0,1] normalized by 20 */

/* ============================================================================
 * API — Lifecycle
 * ============================================================================ */

/**
 * @brief Create a Parrot bridge with the given configuration.
 * @param config Pointer to configuration. NULL uses defaults.
 * @return Bridge handle, or NULL on allocation failure.
 */
nimcp_parrot_bridge_t* nimcp_parrot_bridge_create(const nimcp_parrot_config_t* config);

/**
 * @brief Destroy a Parrot bridge and free all resources. NULL-safe.
 */
void nimcp_parrot_bridge_destroy(nimcp_parrot_bridge_t* bridge);

/**
 * @brief Open the connection to the Parrot ANAFI drone.
 * @return 0 on success, -1 on failure.
 */
int nimcp_parrot_bridge_connect(nimcp_parrot_bridge_t* bridge);

/**
 * @brief Close the connection to the Parrot ANAFI drone.
 * @return 0 on success, -1 on failure.
 */
int nimcp_parrot_bridge_disconnect(nimcp_parrot_bridge_t* bridge);

/**
 * @brief Check if the bridge is currently connected.
 */
bool nimcp_parrot_bridge_is_connected(const nimcp_parrot_bridge_t* bridge);

/**
 * @brief Start the telemetry receive thread.
 * @return 0 on success, -1 on failure.
 */
int nimcp_parrot_bridge_start(nimcp_parrot_bridge_t* bridge);

/**
 * @brief Stop the telemetry receive thread.
 * @return 0 on success, -1 on failure.
 */
int nimcp_parrot_bridge_stop(nimcp_parrot_bridge_t* bridge);

/* ============================================================================
 * API — Telemetry Getters (thread-safe, return latest cached values)
 * ============================================================================ */

int nimcp_parrot_get_attitude(const nimcp_parrot_bridge_t* bridge, nimcp_parrot_attitude_t* att);
int nimcp_parrot_get_position(const nimcp_parrot_bridge_t* bridge, nimcp_parrot_position_t* pos);
int nimcp_parrot_get_battery(const nimcp_parrot_bridge_t* bridge, nimcp_parrot_battery_t* bat);
int nimcp_parrot_get_speed(const nimcp_parrot_bridge_t* bridge, nimcp_parrot_speed_t* spd);

/* ============================================================================
 * API — Commands to Drone
 * ============================================================================ */

int nimcp_parrot_takeoff(nimcp_parrot_bridge_t* bridge);
int nimcp_parrot_land(nimcp_parrot_bridge_t* bridge);

/**
 * @brief Move the drone by relative offsets.
 * @param dx    Forward (meters, positive = forward).
 * @param dy    Right (meters, positive = right).
 * @param dz    Down (meters, positive = down, negative = up).
 * @param dyaw  Yaw rotation (degrees, positive = clockwise).
 * @return 0 on success, -1 on failure.
 */
int nimcp_parrot_move_by(nimcp_parrot_bridge_t* bridge,
                          float dx, float dy, float dz, float dyaw);

int nimcp_parrot_goto_position(nimcp_parrot_bridge_t* bridge,
                                double lat, double lon, float alt, float heading);
int nimcp_parrot_set_max_tilt(nimcp_parrot_bridge_t* bridge, float degrees);
int nimcp_parrot_set_max_speed(nimcp_parrot_bridge_t* bridge, float speed_ms);

/* ============================================================================
 * API — Brain Integration
 * ============================================================================ */

/**
 * @brief Compose a brain-input feature vector from current Parrot telemetry.
 *
 * Writes up to NIMCP_PARROT_FEATURE_COUNT (14) features.
 * See PARROT_FEAT_* defines for layout.
 *
 * @param bridge       Bridge handle.
 * @param features     Output float array (caller-allocated).
 * @param max_features Maximum number of floats to write.
 * @return Number of features written, or -1 on error.
 */
int nimcp_parrot_compose_features(const nimcp_parrot_bridge_t* bridge,
                                   float* features, uint32_t max_features);

/* ============================================================================
 * API — Safety Checks
 * ============================================================================ */

/**
 * @brief Check if geofence is violated.
 * @return 1 if violated, 0 if within bounds, -1 on error.
 */
int nimcp_parrot_check_geofence(const nimcp_parrot_bridge_t* bridge);

/**
 * @brief Return a default Parrot bridge configuration.
 */
nimcp_parrot_config_t nimcp_parrot_config_default(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PARROT_BRIDGE_H */
