/**
 * @file nimcp_mavlink_bridge.h
 * @brief MAVLink Bridge — NIMCP brain <-> PX4/ArduPilot flight controller.
 *
 * Two compilation modes:
 *   - NIMCP_HAS_MAVLINK defined: full MAVLink v2 integration (serial/UDP/TCP,
 *     heartbeat, telemetry, commands)
 *   - NIMCP_HAS_MAVLINK not defined: stub mode with message framing,
 *     zeroed telemetry, and logged-but-rejected commands. Allows testing
 *     the integration pipeline without hardware or MAVLink headers.
 *
 * Typical deployment: companion computer (Jetson/Pi) running NIMCP brain
 * communicates with PX4/ArduPilot flight controller over serial UART or
 * UDP (MAVLink router).
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#ifndef NIMCP_MAVLINK_BRIDGE_H
#define NIMCP_MAVLINK_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Connection Type
 * ============================================================================ */

typedef enum {
    NIMCP_MAVLINK_SERIAL = 0,      /* /dev/ttyACM0, /dev/ttyUSB0 */
    NIMCP_MAVLINK_UDP,             /* UDP port 14550 (default) */
    NIMCP_MAVLINK_TCP,             /* TCP connection */
} nimcp_mavlink_conn_type_t;

/* ============================================================================
 * Flight Mode Mapping
 * ============================================================================ */

typedef enum {
    NIMCP_FLIGHT_MANUAL = 0,
    NIMCP_FLIGHT_STABILIZE,
    NIMCP_FLIGHT_LOITER,
    NIMCP_FLIGHT_GUIDED,           /* Brain controls position targets */
    NIMCP_FLIGHT_AUTO,             /* Mission waypoints */
    NIMCP_FLIGHT_RTL,              /* Return to launch */
    NIMCP_FLIGHT_LAND,
} nimcp_flight_mode_t;

/* ============================================================================
 * MAVLink Bridge Configuration
 * ============================================================================ */

typedef struct {
    nimcp_mavlink_conn_type_t conn_type;
    char connection_string[256];    /* e.g., "/dev/ttyACM0" or "udp:14550" */
    uint32_t baud_rate;             /* Serial baud (default 921600) */
    uint8_t system_id;              /* Our MAVLink system ID (default 1) */
    uint8_t component_id;           /* Our component ID (default 191 = onboard computer) */
    uint8_t target_system;          /* Flight controller system ID (default 1) */
    uint8_t target_component;       /* Flight controller component (default 1) */

    /* Telemetry rates (Hz, 0 = don't request) */
    float attitude_rate;            /* Roll/pitch/yaw (default 50) */
    float position_rate;            /* GPS position (default 10) */
    float imu_rate;                 /* Raw IMU (default 50) */
    float battery_rate;             /* Battery status (default 1) */
    float rc_rate;                  /* RC channels (default 10) */

    /* Safety */
    float geofence_radius_m;        /* Max distance from home (default 100) */
    float max_altitude_m;           /* Max altitude AGL (default 50) */
    float min_battery_pct;          /* RTL trigger (default 20) */
    bool enable_geofence;
    bool enable_altitude_limit;
    bool enable_battery_rtl;
} nimcp_mavlink_config_t;

/* ============================================================================
 * Opaque Handle
 * ============================================================================ */

typedef struct nimcp_mavlink_bridge nimcp_mavlink_bridge_t;

/* ============================================================================
 * Telemetry Data Types
 * ============================================================================ */

/** Attitude data received from flight controller */
typedef struct {
    float roll;                     /* Radians */
    float pitch;
    float yaw;
    float rollspeed;                /* rad/s */
    float pitchspeed;
    float yawspeed;
    uint64_t timestamp_us;
} nimcp_mavlink_attitude_t;

/** Position data from flight controller */
typedef struct {
    double latitude;                /* Degrees */
    double longitude;
    float altitude_msl;             /* Meters above sea level */
    float altitude_rel;             /* Meters above home */
    float vx, vy, vz;              /* m/s in NED frame */
    float heading;                  /* Degrees, 0=North */
    uint8_t fix_type;               /* GPS fix type */
    uint8_t satellites;
    uint64_t timestamp_us;
} nimcp_mavlink_position_t;

/** Battery state from flight controller */
typedef struct {
    float voltage;                  /* Volts */
    float current;                  /* Amps */
    float remaining_pct;            /* 0-100 */
    int32_t consumed_mah;
    uint64_t timestamp_us;
} nimcp_mavlink_battery_t;

/* ============================================================================
 * Feature vector layout for brain input
 * ============================================================================ */

#define NIMCP_MAVLINK_FEATURE_COUNT  14

/* Feature indices */
#define MAVLINK_FEAT_ROLL            0   /* normalized [-1,1] by PI */
#define MAVLINK_FEAT_PITCH           1
#define MAVLINK_FEAT_YAW             2
#define MAVLINK_FEAT_ROLLSPEED       3   /* rad/s clamped [-5,5] */
#define MAVLINK_FEAT_PITCHSPEED      4
#define MAVLINK_FEAT_YAWSPEED        5
#define MAVLINK_FEAT_VX              6   /* m/s */
#define MAVLINK_FEAT_VY              7
#define MAVLINK_FEAT_VZ              8
#define MAVLINK_FEAT_ALT_REL         9   /* meters above home */
#define MAVLINK_FEAT_HEADING         10  /* [0,1] normalized by 360 */
#define MAVLINK_FEAT_BATTERY         11  /* [0,1] */
#define MAVLINK_FEAT_GPS_QUALITY     12  /* [0,1] from fix_type */
#define MAVLINK_FEAT_DIST_HOME       13  /* meters from home */

/* ============================================================================
 * API — Lifecycle
 * ============================================================================ */

/**
 * @brief Create a MAVLink bridge with the given configuration.
 * @param config Pointer to configuration. NULL uses defaults.
 * @return Bridge handle, or NULL on allocation failure.
 */
nimcp_mavlink_bridge_t* nimcp_mavlink_bridge_create(const nimcp_mavlink_config_t* config);

/**
 * @brief Destroy a MAVLink bridge and free all resources. NULL-safe.
 */
void nimcp_mavlink_bridge_destroy(nimcp_mavlink_bridge_t* bridge);

/**
 * @brief Open the connection (serial port or socket).
 * @return 0 on success, -1 on failure.
 */
int nimcp_mavlink_bridge_connect(nimcp_mavlink_bridge_t* bridge);

/**
 * @brief Close the connection.
 * @return 0 on success, -1 on failure.
 */
int nimcp_mavlink_bridge_disconnect(nimcp_mavlink_bridge_t* bridge);

/**
 * @brief Check if the bridge is currently connected.
 */
bool nimcp_mavlink_bridge_is_connected(const nimcp_mavlink_bridge_t* bridge);

/**
 * @brief Start the receive thread (begins processing incoming MAVLink messages).
 * @return 0 on success, -1 on failure.
 */
int nimcp_mavlink_bridge_start(nimcp_mavlink_bridge_t* bridge);

/**
 * @brief Stop the receive thread.
 * @return 0 on success, -1 on failure.
 */
int nimcp_mavlink_bridge_stop(nimcp_mavlink_bridge_t* bridge);

/* ============================================================================
 * API — Telemetry Getters (thread-safe, return latest cached values)
 * ============================================================================ */

int nimcp_mavlink_get_attitude(const nimcp_mavlink_bridge_t* bridge, nimcp_mavlink_attitude_t* att);
int nimcp_mavlink_get_position(const nimcp_mavlink_bridge_t* bridge, nimcp_mavlink_position_t* pos);
int nimcp_mavlink_get_battery(const nimcp_mavlink_bridge_t* bridge, nimcp_mavlink_battery_t* bat);

/* ============================================================================
 * API — Commands to Flight Controller
 * ============================================================================ */

int nimcp_mavlink_set_mode(nimcp_mavlink_bridge_t* bridge, nimcp_flight_mode_t mode);
int nimcp_mavlink_arm(nimcp_mavlink_bridge_t* bridge, bool arm);
int nimcp_mavlink_takeoff(nimcp_mavlink_bridge_t* bridge, float altitude_m);
int nimcp_mavlink_land(nimcp_mavlink_bridge_t* bridge);
int nimcp_mavlink_goto(nimcp_mavlink_bridge_t* bridge, double lat, double lon, float alt);
int nimcp_mavlink_set_velocity(nimcp_mavlink_bridge_t* bridge,
                                float vx, float vy, float vz, float yaw_rate);
int nimcp_mavlink_rtl(nimcp_mavlink_bridge_t* bridge);

/* ============================================================================
 * API — Brain Integration
 * ============================================================================ */

/**
 * @brief Compose a brain-input feature vector from current MAVLink telemetry.
 *
 * Writes up to NIMCP_MAVLINK_FEATURE_COUNT (14) features.
 * See MAVLINK_FEAT_* defines for layout.
 *
 * @param bridge       Bridge handle.
 * @param features     Output float array (caller-allocated).
 * @param max_features Maximum number of floats to write.
 * @return Number of features written, or -1 on error.
 */
int nimcp_mavlink_compose_features(const nimcp_mavlink_bridge_t* bridge,
                                    float* features, uint32_t max_features);

/* ============================================================================
 * API — Safety Checks
 * ============================================================================ */

/**
 * @brief Check if geofence is violated.
 * @return 1 if violated, 0 if within bounds, -1 on error.
 */
int nimcp_mavlink_check_geofence(const nimcp_mavlink_bridge_t* bridge);

/**
 * @brief Check if battery is critically low.
 * @return 1 if critical (below min_battery_pct), 0 if OK, -1 on error.
 */
int nimcp_mavlink_check_battery(const nimcp_mavlink_bridge_t* bridge);

/**
 * @brief Return a default MAVLink bridge configuration.
 */
nimcp_mavlink_config_t nimcp_mavlink_config_default(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MAVLINK_BRIDGE_H */
