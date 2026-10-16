/**
 * @file nimcp_mavlink_bridge.c
 * @brief MAVLink Bridge — NIMCP brain <-> PX4/ArduPilot flight controller.
 *
 * Two compilation modes:
 *   - NIMCP_HAS_MAVLINK defined: full MAVLink v2 integration with serial/UDP
 *     transport, heartbeat exchange, telemetry parsing, and command sending
 *   - NIMCP_HAS_MAVLINK not defined: stub mode — all lifecycle functions work,
 *     telemetry returns zeros, commands log warnings and return -1
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#define LOG_MODULE "MAVLINK_BRIDGE"

#include "edge/nimcp_mavlink_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"

#include <string.h>
#include <math.h>
#include <errno.h>

#ifdef NIMCP_HAS_MAVLINK
#include <mavlink/common/mavlink.h>
#endif

/* Platform headers for serial/socket I/O */
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

#ifdef NIMCP_HAS_MAVLINK
#include <termios.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define MAVLINK_HEARTBEAT_INTERVAL_US   1000000   /* 1 second */
#define MAVLINK_RECV_BUF_SIZE           2048
#define MAVLINK_FC_TIMEOUT_US           5000000   /* 5 seconds no heartbeat = lost */

/** Earth radius in meters (for haversine) */
#define EARTH_RADIUS_M  6371000.0

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct nimcp_mavlink_bridge {
    nimcp_mavlink_config_t config;

    /* Connection */
    int fd;                          /* Serial fd or socket fd */
    bool connected;

    /* Cached telemetry (updated by recv thread) */
    nimcp_mavlink_attitude_t attitude;
    nimcp_mavlink_position_t position;
    nimcp_mavlink_battery_t battery;
    nimcp_mutex_t* telemetry_lock;

    /* Home position (for geofence) */
    double home_lat, home_lon;
    float home_alt;
    bool home_set;

    /* Receive thread */
    nimcp_thread_t recv_thread;
    volatile bool running;
    volatile bool thread_started;

    /* MAVLink parse state */
#ifdef NIMCP_HAS_MAVLINK
    mavlink_message_t msg;
    mavlink_status_t parse_status;
#endif

    /* Heartbeat tracking */
    uint64_t last_fc_heartbeat_us;
    uint64_t last_our_heartbeat_us;
};

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static void* _mavlink_recv_thread(void* arg);
static double _haversine_distance_m(double lat1, double lon1, double lat2, double lon2);

#ifdef NIMCP_HAS_MAVLINK
static int _open_serial(nimcp_mavlink_bridge_t* bridge);
static int _open_udp(nimcp_mavlink_bridge_t* bridge);
static int _open_tcp(nimcp_mavlink_bridge_t* bridge);
static void _handle_message(nimcp_mavlink_bridge_t* bridge, const mavlink_message_t* msg);
static int _send_heartbeat(nimcp_mavlink_bridge_t* bridge);
static int _send_command_long(nimcp_mavlink_bridge_t* bridge,
                               uint16_t command, float p1, float p2,
                               float p3, float p4, float p5, float p6, float p7);
static int _request_data_stream(nimcp_mavlink_bridge_t* bridge,
                                 uint16_t msg_id, float rate_hz);
static int _send_bytes(nimcp_mavlink_bridge_t* bridge,
                        const uint8_t* data, uint32_t len);
#endif

static float _clampf(float val, float lo, float hi);

/* ============================================================================
 * Default Config
 * ============================================================================ */

nimcp_mavlink_config_t nimcp_mavlink_config_default(void) {
    nimcp_mavlink_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.conn_type = NIMCP_MAVLINK_SERIAL;
    strncpy(cfg.connection_string, "/dev/ttyACM0", sizeof(cfg.connection_string) - 1);
    cfg.baud_rate = 921600;
    cfg.system_id = 1;
    cfg.component_id = 191;    /* MAV_COMP_ID_ONBOARD_COMPUTER */
    cfg.target_system = 1;
    cfg.target_component = 1;  /* MAV_COMP_ID_AUTOPILOT1 */

    cfg.attitude_rate = 50.0f;
    cfg.position_rate = 10.0f;
    cfg.imu_rate = 50.0f;
    cfg.battery_rate = 1.0f;
    cfg.rc_rate = 10.0f;

    cfg.geofence_radius_m = 100.0f;
    cfg.max_altitude_m = 50.0f;
    cfg.min_battery_pct = 20.0f;
    cfg.enable_geofence = true;
    cfg.enable_altitude_limit = true;
    cfg.enable_battery_rtl = true;

    return cfg;
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

nimcp_mavlink_bridge_t* nimcp_mavlink_bridge_create(const nimcp_mavlink_config_t* config) {
    nimcp_mavlink_bridge_t* bridge = (nimcp_mavlink_bridge_t*)nimcp_calloc(
        1, sizeof(nimcp_mavlink_bridge_t));
    if (!bridge) {
        LOG_ERROR("[%s] Failed to allocate bridge", LOG_MODULE);
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = nimcp_mavlink_config_default();
    }

    bridge->fd = -1;
    bridge->connected = false;
    bridge->running = false;
    bridge->thread_started = false;
    bridge->home_set = false;
    bridge->last_fc_heartbeat_us = 0;
    bridge->last_our_heartbeat_us = 0;

    bridge->telemetry_lock = nimcp_mutex_create(NULL);
    if (!bridge->telemetry_lock) {
        LOG_ERROR("[%s] Failed to create telemetry mutex", LOG_MODULE);
        nimcp_free(bridge);
        return NULL;
    }

    LOG_INFO("[%s] Bridge created (conn=%d, target=%d:%d)",
             LOG_MODULE, bridge->config.conn_type,
             bridge->config.target_system, bridge->config.target_component);

    return bridge;
}

void nimcp_mavlink_bridge_destroy(nimcp_mavlink_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Stop recv thread if running */
    if (bridge->running) {
        nimcp_mavlink_bridge_stop(bridge);
    }

    /* Disconnect if connected */
    if (bridge->connected) {
        nimcp_mavlink_bridge_disconnect(bridge);
    }

    if (bridge->telemetry_lock) {
        nimcp_mutex_free(bridge->telemetry_lock);
    }

    nimcp_free(bridge);
    LOG_INFO("[%s] Bridge destroyed", LOG_MODULE);
}

/* ============================================================================
 * Connection Management
 * ============================================================================ */

int nimcp_mavlink_bridge_connect(nimcp_mavlink_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }
    if (bridge->connected) {
        LOG_WARN("[%s] Already connected", LOG_MODULE);
        return 0;
    }

#ifdef NIMCP_HAS_MAVLINK
    int rc = -1;
    switch (bridge->config.conn_type) {
        case NIMCP_MAVLINK_SERIAL:
            rc = _open_serial(bridge);
            break;
        case NIMCP_MAVLINK_UDP:
            rc = _open_udp(bridge);
            break;
        case NIMCP_MAVLINK_TCP:
            rc = _open_tcp(bridge);
            break;
        default:
            LOG_ERROR("[%s] Unknown connection type %d",
                      LOG_MODULE, bridge->config.conn_type);
            return -1;
    }

    if (rc < 0) {
        LOG_ERROR("[%s] Failed to connect via %s",
                  LOG_MODULE, bridge->config.connection_string);
        return -1;
    }

    bridge->connected = true;
    LOG_INFO("[%s] Connected via %s (fd=%d)",
             LOG_MODULE, bridge->config.connection_string, bridge->fd);
    return 0;
#else
    /* Stub mode: pretend to connect */
    bridge->fd = -1;
    bridge->connected = true;
    LOG_INFO("[%s] Connected (stub mode — no MAVLink headers)", LOG_MODULE);
    return 0;
#endif
}

int nimcp_mavlink_bridge_disconnect(nimcp_mavlink_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }
    if (!bridge->connected) {
        return 0;
    }

    /* Stop recv thread first */
    if (bridge->running) {
        nimcp_mavlink_bridge_stop(bridge);
    }

#ifdef NIMCP_HAS_MAVLINK
    if (bridge->fd >= 0) {
        close(bridge->fd);
        bridge->fd = -1;
    }
#endif

    bridge->connected = false;
    bridge->home_set = false;
    LOG_INFO("[%s] Disconnected", LOG_MODULE);
    return 0;
}

bool nimcp_mavlink_bridge_is_connected(const nimcp_mavlink_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->connected;
}

/* ============================================================================
 * Recv Thread Management
 * ============================================================================ */

int nimcp_mavlink_bridge_start(nimcp_mavlink_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }
    if (!bridge->connected) {
        LOG_ERROR("[%s] Cannot start — not connected", LOG_MODULE);
        return -1;
    }
    if (bridge->running) {
        LOG_WARN("[%s] Already running", LOG_MODULE);
        return 0;
    }

    bridge->running = true;

    nimcp_result_t rc = nimcp_thread_create(
        &bridge->recv_thread, _mavlink_recv_thread, bridge, NULL);
    if (rc != 0) {
        LOG_ERROR("[%s] Failed to create recv thread (rc=%d)", LOG_MODULE, rc);
        bridge->running = false;
        return -1;
    }

    bridge->thread_started = true;
    LOG_INFO("[%s] Recv thread started", LOG_MODULE);
    return 0;
}

int nimcp_mavlink_bridge_stop(nimcp_mavlink_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }
    if (!bridge->running) {
        return 0;
    }

    bridge->running = false;

    if (bridge->thread_started) {
        nimcp_thread_join(bridge->recv_thread, NULL);
        bridge->thread_started = false;
    }

    LOG_INFO("[%s] Recv thread stopped", LOG_MODULE);
    return 0;
}

/* ============================================================================
 * Telemetry Getters (thread-safe)
 * ============================================================================ */

int nimcp_mavlink_get_attitude(const nimcp_mavlink_bridge_t* bridge,
                                nimcp_mavlink_attitude_t* att) {
    if (!bridge || !att) {
        return -1;
    }

    /* Cast away const for mutex lock — telemetry_lock protects read access */
    nimcp_mutex_lock(((nimcp_mavlink_bridge_t*)bridge)->telemetry_lock);
    *att = bridge->attitude;
    nimcp_mutex_unlock(((nimcp_mavlink_bridge_t*)bridge)->telemetry_lock);
    return 0;
}

int nimcp_mavlink_get_position(const nimcp_mavlink_bridge_t* bridge,
                                nimcp_mavlink_position_t* pos) {
    if (!bridge || !pos) {
        return -1;
    }

    nimcp_mutex_lock(((nimcp_mavlink_bridge_t*)bridge)->telemetry_lock);
    *pos = bridge->position;
    nimcp_mutex_unlock(((nimcp_mavlink_bridge_t*)bridge)->telemetry_lock);
    return 0;
}

int nimcp_mavlink_get_battery(const nimcp_mavlink_bridge_t* bridge,
                               nimcp_mavlink_battery_t* bat) {
    if (!bridge || !bat) {
        return -1;
    }

    nimcp_mutex_lock(((nimcp_mavlink_bridge_t*)bridge)->telemetry_lock);
    *bat = bridge->battery;
    nimcp_mutex_unlock(((nimcp_mavlink_bridge_t*)bridge)->telemetry_lock);
    return 0;
}

/* ============================================================================
 * Commands to Flight Controller
 * ============================================================================ */

#ifndef NIMCP_HAS_MAVLINK

/* Stub implementations — log and return -1 */

int nimcp_mavlink_set_mode(nimcp_mavlink_bridge_t* bridge, nimcp_flight_mode_t mode) {
    if (!bridge) { return -1; }
    LOG_WARN("[%s] set_mode(%d) — MAVLink not available (stub mode)",
             LOG_MODULE, (int)mode);
    return -1;
}

int nimcp_mavlink_arm(nimcp_mavlink_bridge_t* bridge, bool arm) {
    if (!bridge) { return -1; }
    LOG_WARN("[%s] arm(%s) — MAVLink not available (stub mode)",
             LOG_MODULE, arm ? "true" : "false");
    return -1;
}

int nimcp_mavlink_takeoff(nimcp_mavlink_bridge_t* bridge, float altitude_m) {
    if (!bridge) { return -1; }
    LOG_WARN("[%s] takeoff(%.1fm) — MAVLink not available (stub mode)",
             LOG_MODULE, altitude_m);
    return -1;
}

int nimcp_mavlink_land(nimcp_mavlink_bridge_t* bridge) {
    if (!bridge) { return -1; }
    LOG_WARN("[%s] land() — MAVLink not available (stub mode)", LOG_MODULE);
    return -1;
}

int nimcp_mavlink_goto(nimcp_mavlink_bridge_t* bridge,
                        double lat, double lon, float alt) {
    if (!bridge) { return -1; }
    (void)lat; (void)lon; (void)alt;
    LOG_WARN("[%s] goto() — MAVLink not available (stub mode)", LOG_MODULE);
    return -1;
}

int nimcp_mavlink_set_velocity(nimcp_mavlink_bridge_t* bridge,
                                float vx, float vy, float vz, float yaw_rate) {
    if (!bridge) { return -1; }
    (void)vx; (void)vy; (void)vz; (void)yaw_rate;
    LOG_WARN("[%s] set_velocity() — MAVLink not available (stub mode)", LOG_MODULE);
    return -1;
}

int nimcp_mavlink_rtl(nimcp_mavlink_bridge_t* bridge) {
    if (!bridge) { return -1; }
    LOG_WARN("[%s] rtl() — MAVLink not available (stub mode)", LOG_MODULE);
    return -1;
}

#else /* NIMCP_HAS_MAVLINK */

/* ---- Full MAVLink command implementations ---- */

int nimcp_mavlink_set_mode(nimcp_mavlink_bridge_t* bridge, nimcp_flight_mode_t mode) {
    if (!bridge || !bridge->connected) { return -1; }

    /* Map NIMCP flight mode to MAVLink custom mode.
     * PX4 uses MAV_MODE_FLAG_CUSTOM_MODE_ENABLED with PX4 mode values.
     * ArduPilot uses the same mechanism with ArduPilot mode values.
     * We use PX4 mapping as default. */
    uint32_t custom_mode = 0;
    switch (mode) {
        case NIMCP_FLIGHT_MANUAL:    custom_mode = 1;  break; /* MANUAL */
        case NIMCP_FLIGHT_STABILIZE: custom_mode = 7;  break; /* STABILIZED */
        case NIMCP_FLIGHT_LOITER:    custom_mode = 3;  break; /* HOLD/LOITER */
        case NIMCP_FLIGHT_GUIDED:    custom_mode = 6;  break; /* OFFBOARD (PX4) */
        case NIMCP_FLIGHT_AUTO:      custom_mode = 4;  break; /* MISSION */
        case NIMCP_FLIGHT_RTL:       custom_mode = 5;  break; /* RTL */
        case NIMCP_FLIGHT_LAND:      custom_mode = 8;  break; /* LAND */
        default:
            LOG_ERROR("[%s] Unknown flight mode %d", LOG_MODULE, (int)mode);
            return -1;
    }

    mavlink_message_t msg;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];

    mavlink_msg_set_mode_pack(
        bridge->config.system_id,
        bridge->config.component_id,
        &msg,
        bridge->config.target_system,
        MAV_MODE_FLAG_CUSTOM_MODE_ENABLED,
        custom_mode);

    uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
    if (_send_bytes(bridge, buf, len) < 0) {
        LOG_ERROR("[%s] Failed to send SET_MODE", LOG_MODULE);
        return -1;
    }

    LOG_INFO("[%s] Set mode → %d (custom_mode=%u)", LOG_MODULE, (int)mode, custom_mode);
    return 0;
}

int nimcp_mavlink_arm(nimcp_mavlink_bridge_t* bridge, bool arm) {
    if (!bridge || !bridge->connected) { return -1; }

    /* MAV_CMD_COMPONENT_ARM_DISARM (400): param1 = 1.0 arm, 0.0 disarm */
    int rc = _send_command_long(bridge, 400,
                                 arm ? 1.0f : 0.0f,
                                 0, 0, 0, 0, 0, 0);
    if (rc == 0) {
        LOG_INFO("[%s] %s command sent", LOG_MODULE, arm ? "ARM" : "DISARM");
    }
    return rc;
}

int nimcp_mavlink_takeoff(nimcp_mavlink_bridge_t* bridge, float altitude_m) {
    if (!bridge || !bridge->connected) { return -1; }

    /* Safety: enforce altitude limit */
    if (bridge->config.enable_altitude_limit &&
        altitude_m > bridge->config.max_altitude_m) {
        LOG_WARN("[%s] Takeoff altitude %.1fm exceeds limit %.1fm, clamping",
                 LOG_MODULE, altitude_m, bridge->config.max_altitude_m);
        altitude_m = bridge->config.max_altitude_m;
    }

    /* MAV_CMD_NAV_TAKEOFF (22): param7 = altitude */
    int rc = _send_command_long(bridge, 22,
                                 0, 0, 0, 0, 0, 0, altitude_m);
    if (rc == 0) {
        LOG_INFO("[%s] Takeoff to %.1fm commanded", LOG_MODULE, altitude_m);
    }
    return rc;
}

int nimcp_mavlink_land(nimcp_mavlink_bridge_t* bridge) {
    if (!bridge || !bridge->connected) { return -1; }

    /* MAV_CMD_NAV_LAND (21) */
    int rc = _send_command_long(bridge, 21,
                                 0, 0, 0, 0, 0, 0, 0);
    if (rc == 0) {
        LOG_INFO("[%s] Land commanded", LOG_MODULE);
    }
    return rc;
}

int nimcp_mavlink_goto(nimcp_mavlink_bridge_t* bridge,
                        double lat, double lon, float alt) {
    if (!bridge || !bridge->connected) { return -1; }

    /* Safety: check geofence before sending */
    if (bridge->config.enable_geofence && bridge->home_set) {
        double dist = _haversine_distance_m(bridge->home_lat, bridge->home_lon,
                                             lat, lon);
        if (dist > bridge->config.geofence_radius_m) {
            LOG_ERROR("[%s] Goto target (%.6f, %.6f) is %.0fm from home, "
                      "exceeds geofence %.0fm — command rejected",
                      LOG_MODULE, lat, lon, dist, bridge->config.geofence_radius_m);
            return -1;
        }
    }

    /* Safety: altitude limit */
    if (bridge->config.enable_altitude_limit &&
        alt > bridge->config.max_altitude_m) {
        LOG_WARN("[%s] Goto altitude %.1fm exceeds limit %.1fm, clamping",
                 LOG_MODULE, alt, bridge->config.max_altitude_m);
        alt = bridge->config.max_altitude_m;
    }

    /* SET_POSITION_TARGET_GLOBAL_INT (86) */
    mavlink_message_t msg;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];

    /* type_mask: use position only (ignore velocity+accel+yaw) = 0x0FF8 */
    uint16_t type_mask = 0x0FF8;

    mavlink_msg_set_position_target_global_int_pack(
        bridge->config.system_id,
        bridge->config.component_id,
        &msg,
        (uint32_t)(nimcp_time_now_us() / 1000),  /* time_boot_ms */
        bridge->config.target_system,
        bridge->config.target_component,
        MAV_FRAME_GLOBAL_RELATIVE_ALT_INT,
        type_mask,
        (int32_t)(lat * 1e7),   /* lat_int */
        (int32_t)(lon * 1e7),   /* lon_int */
        alt,
        0, 0, 0,                /* vx, vy, vz */
        0, 0, 0,                /* afx, afy, afz */
        0, 0);                  /* yaw, yaw_rate */

    uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
    if (_send_bytes(bridge, buf, len) < 0) {
        LOG_ERROR("[%s] Failed to send goto", LOG_MODULE);
        return -1;
    }

    LOG_INFO("[%s] Goto (%.6f, %.6f) alt=%.1fm", LOG_MODULE, lat, lon, alt);
    return 0;
}

int nimcp_mavlink_set_velocity(nimcp_mavlink_bridge_t* bridge,
                                float vx, float vy, float vz, float yaw_rate) {
    if (!bridge || !bridge->connected) { return -1; }

    /* SET_POSITION_TARGET_LOCAL_NED (84) with velocity mask */
    mavlink_message_t msg;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];

    /* type_mask: use velocity + yaw_rate only (ignore position+accel+yaw) = 0x01C7 */
    uint16_t type_mask = 0x01C7;

    mavlink_msg_set_position_target_local_ned_pack(
        bridge->config.system_id,
        bridge->config.component_id,
        &msg,
        (uint32_t)(nimcp_time_now_us() / 1000),
        bridge->config.target_system,
        bridge->config.target_component,
        MAV_FRAME_LOCAL_NED,
        type_mask,
        0, 0, 0,       /* x, y, z (ignored) */
        vx, vy, vz,    /* velocity NED */
        0, 0, 0,       /* accel (ignored) */
        0, yaw_rate);   /* yaw (ignored), yaw_rate */

    uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
    if (_send_bytes(bridge, buf, len) < 0) {
        LOG_ERROR("[%s] Failed to send velocity", LOG_MODULE);
        return -1;
    }

    return 0;
}

int nimcp_mavlink_rtl(nimcp_mavlink_bridge_t* bridge) {
    if (!bridge || !bridge->connected) { return -1; }

    /* MAV_CMD_NAV_RETURN_TO_LAUNCH (20) */
    int rc = _send_command_long(bridge, 20,
                                 0, 0, 0, 0, 0, 0, 0);
    if (rc == 0) {
        LOG_INFO("[%s] RTL commanded", LOG_MODULE);
    }
    return rc;
}

#endif /* NIMCP_HAS_MAVLINK */

/* ============================================================================
 * Feature Composition for Brain Input
 * ============================================================================ */

int nimcp_mavlink_compose_features(const nimcp_mavlink_bridge_t* bridge,
                                    float* features, uint32_t max_features) {
    if (!bridge || !features) {
        return -1;
    }

    uint32_t count = NIMCP_MAVLINK_FEATURE_COUNT;
    if (count > max_features) {
        count = max_features;
    }

    /* Zero-fill first */
    memset(features, 0, count * sizeof(float));

    /* Snapshot telemetry under lock */
    nimcp_mavlink_attitude_t att;
    nimcp_mavlink_position_t pos;
    nimcp_mavlink_battery_t bat;

    nimcp_mutex_lock(((nimcp_mavlink_bridge_t*)bridge)->telemetry_lock);
    att = bridge->attitude;
    pos = bridge->position;
    bat = bridge->battery;

    double home_lat = bridge->home_lat;
    double home_lon = bridge->home_lon;
    bool home_set = bridge->home_set;
    nimcp_mutex_unlock(((nimcp_mavlink_bridge_t*)bridge)->telemetry_lock);

    /* Attitude: normalize to [-1,1] by dividing by PI */
    if (count > MAVLINK_FEAT_ROLL)
        features[MAVLINK_FEAT_ROLL] = _clampf(att.roll / (float)M_PI, -1.0f, 1.0f);
    if (count > MAVLINK_FEAT_PITCH)
        features[MAVLINK_FEAT_PITCH] = _clampf(att.pitch / (float)M_PI, -1.0f, 1.0f);
    if (count > MAVLINK_FEAT_YAW)
        features[MAVLINK_FEAT_YAW] = _clampf(att.yaw / (float)M_PI, -1.0f, 1.0f);

    /* Angular rates: rad/s clamped to [-5,5] */
    if (count > MAVLINK_FEAT_ROLLSPEED)
        features[MAVLINK_FEAT_ROLLSPEED] = _clampf(att.rollspeed, -5.0f, 5.0f);
    if (count > MAVLINK_FEAT_PITCHSPEED)
        features[MAVLINK_FEAT_PITCHSPEED] = _clampf(att.pitchspeed, -5.0f, 5.0f);
    if (count > MAVLINK_FEAT_YAWSPEED)
        features[MAVLINK_FEAT_YAWSPEED] = _clampf(att.yawspeed, -5.0f, 5.0f);

    /* Velocity NED (m/s) */
    if (count > MAVLINK_FEAT_VX)
        features[MAVLINK_FEAT_VX] = pos.vx;
    if (count > MAVLINK_FEAT_VY)
        features[MAVLINK_FEAT_VY] = pos.vy;
    if (count > MAVLINK_FEAT_VZ)
        features[MAVLINK_FEAT_VZ] = pos.vz;

    /* Altitude relative to home */
    if (count > MAVLINK_FEAT_ALT_REL)
        features[MAVLINK_FEAT_ALT_REL] = pos.altitude_rel;

    /* Heading normalized to [0,1] */
    if (count > MAVLINK_FEAT_HEADING)
        features[MAVLINK_FEAT_HEADING] = _clampf(pos.heading / 360.0f, 0.0f, 1.0f);

    /* Battery remaining normalized to [0,1] */
    if (count > MAVLINK_FEAT_BATTERY)
        features[MAVLINK_FEAT_BATTERY] = _clampf(bat.remaining_pct / 100.0f, 0.0f, 1.0f);

    /* GPS fix quality mapped to [0,1]:
     * 0=no fix, 1=no fix, 2=2D, 3=3D, 4=DGPS, 5=RTK float, 6=RTK fixed */
    if (count > MAVLINK_FEAT_GPS_QUALITY) {
        float gps_q = 0.0f;
        if (pos.fix_type >= 6) gps_q = 1.0f;
        else if (pos.fix_type >= 3) gps_q = (float)pos.fix_type / 6.0f;
        features[MAVLINK_FEAT_GPS_QUALITY] = gps_q;
    }

    /* Distance from home (meters) */
    if (count > MAVLINK_FEAT_DIST_HOME) {
        if (home_set) {
            features[MAVLINK_FEAT_DIST_HOME] = (float)_haversine_distance_m(
                home_lat, home_lon, pos.latitude, pos.longitude);
        } else {
            features[MAVLINK_FEAT_DIST_HOME] = 0.0f;
        }
    }

    return (int)count;
}

/* ============================================================================
 * Safety Checks
 * ============================================================================ */

int nimcp_mavlink_check_geofence(const nimcp_mavlink_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }
    if (!bridge->config.enable_geofence) {
        return 0;
    }

    nimcp_mutex_lock(((nimcp_mavlink_bridge_t*)bridge)->telemetry_lock);
    bool home_set = bridge->home_set;
    double home_lat = bridge->home_lat;
    double home_lon = bridge->home_lon;
    double lat = bridge->position.latitude;
    double lon = bridge->position.longitude;
    float alt_rel = bridge->position.altitude_rel;
    nimcp_mutex_unlock(((nimcp_mavlink_bridge_t*)bridge)->telemetry_lock);

    if (!home_set) {
        return 0;  /* Can't check without home */
    }

    /* Horizontal geofence */
    double dist = _haversine_distance_m(home_lat, home_lon, lat, lon);
    if (dist > (double)bridge->config.geofence_radius_m) {
        LOG_WARN("[%s] GEOFENCE VIOLATED: %.0fm from home (limit %.0fm)",
                 LOG_MODULE, dist, bridge->config.geofence_radius_m);
        return 1;
    }

    /* Altitude limit */
    if (bridge->config.enable_altitude_limit &&
        alt_rel > bridge->config.max_altitude_m) {
        LOG_WARN("[%s] ALTITUDE LIMIT EXCEEDED: %.1fm (limit %.1fm)",
                 LOG_MODULE, alt_rel, bridge->config.max_altitude_m);
        return 1;
    }

    return 0;
}

int nimcp_mavlink_check_battery(const nimcp_mavlink_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }
    if (!bridge->config.enable_battery_rtl) {
        return 0;
    }

    nimcp_mutex_lock(((nimcp_mavlink_bridge_t*)bridge)->telemetry_lock);
    float pct = bridge->battery.remaining_pct;
    nimcp_mutex_unlock(((nimcp_mavlink_bridge_t*)bridge)->telemetry_lock);

    if (pct > 0.0f && pct < bridge->config.min_battery_pct) {
        LOG_WARN("[%s] BATTERY CRITICAL: %.1f%% (threshold %.1f%%)",
                 LOG_MODULE, pct, bridge->config.min_battery_pct);
        return 1;
    }

    return 0;
}

/* ============================================================================
 * Receive Thread
 * ============================================================================ */

static void* _mavlink_recv_thread(void* arg) {
    nimcp_mavlink_bridge_t* bridge = (nimcp_mavlink_bridge_t*)arg;

    LOG_INFO("[%s] Recv thread running", LOG_MODULE);

#ifdef NIMCP_HAS_MAVLINK
    uint8_t recv_buf[MAVLINK_RECV_BUF_SIZE];

    /* Request data streams at configured rates */
    if (bridge->config.attitude_rate > 0.0f)
        _request_data_stream(bridge, MAVLINK_MSG_ID_ATTITUDE,
                              bridge->config.attitude_rate);
    if (bridge->config.position_rate > 0.0f)
        _request_data_stream(bridge, MAVLINK_MSG_ID_GLOBAL_POSITION_INT,
                              bridge->config.position_rate);
    if (bridge->config.battery_rate > 0.0f)
        _request_data_stream(bridge, MAVLINK_MSG_ID_SYS_STATUS,
                              bridge->config.battery_rate);
    if (bridge->config.imu_rate > 0.0f)
        _request_data_stream(bridge, MAVLINK_MSG_ID_RAW_IMU,
                              bridge->config.imu_rate);

    while (bridge->running) {
        /* Send our heartbeat periodically */
        uint64_t now = nimcp_time_now_us();
        if (now - bridge->last_our_heartbeat_us >= MAVLINK_HEARTBEAT_INTERVAL_US) {
            _send_heartbeat(bridge);
            bridge->last_our_heartbeat_us = now;
        }

        /* Check FC heartbeat timeout */
        if (bridge->last_fc_heartbeat_us > 0 &&
            (now - bridge->last_fc_heartbeat_us) > MAVLINK_FC_TIMEOUT_US) {
            LOG_WARN("[%s] Flight controller heartbeat lost (%.1fs)",
                     LOG_MODULE,
                     (float)(now - bridge->last_fc_heartbeat_us) / 1e6f);
        }

        /* Read available data */
        ssize_t n = read(bridge->fd, recv_buf, sizeof(recv_buf));
        if (n <= 0) {
            if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                LOG_ERROR("[%s] Read error: %s", LOG_MODULE, strerror(errno));
            }
            /* Brief yield to avoid busy-spin on non-blocking fd */
            usleep(1000);
            continue;
        }

        /* Parse MAVLink bytes */
        for (ssize_t i = 0; i < n; i++) {
            if (mavlink_parse_char(MAVLINK_COMM_0, recv_buf[i],
                                    &bridge->msg, &bridge->parse_status)) {
                _handle_message(bridge, &bridge->msg);
            }
        }
    }
#else
    /* Stub mode: just idle until stopped */
    while (bridge->running) {
        usleep(100000);  /* 100ms */
    }
#endif

    LOG_INFO("[%s] Recv thread exiting", LOG_MODULE);
    return NULL;
}

/* ============================================================================
 * MAVLink Message Handling (full mode only)
 * ============================================================================ */

#ifdef NIMCP_HAS_MAVLINK

static void _handle_message(nimcp_mavlink_bridge_t* bridge,
                              const mavlink_message_t* msg) {
    switch (msg->msgid) {
        case MAVLINK_MSG_ID_HEARTBEAT: {
            bridge->last_fc_heartbeat_us = nimcp_time_now_us();
            break;
        }

        case MAVLINK_MSG_ID_ATTITUDE: {
            mavlink_attitude_t att;
            mavlink_msg_attitude_decode(msg, &att);

            nimcp_mutex_lock(bridge->telemetry_lock);
            bridge->attitude.roll = att.roll;
            bridge->attitude.pitch = att.pitch;
            bridge->attitude.yaw = att.yaw;
            bridge->attitude.rollspeed = att.rollspeed;
            bridge->attitude.pitchspeed = att.pitchspeed;
            bridge->attitude.yawspeed = att.yawspeed;
            bridge->attitude.timestamp_us = nimcp_time_now_us();
            nimcp_mutex_unlock(bridge->telemetry_lock);
            break;
        }

        case MAVLINK_MSG_ID_GLOBAL_POSITION_INT: {
            mavlink_global_position_int_t gp;
            mavlink_msg_global_position_int_decode(msg, &gp);

            nimcp_mutex_lock(bridge->telemetry_lock);
            bridge->position.latitude = gp.lat / 1e7;
            bridge->position.longitude = gp.lon / 1e7;
            bridge->position.altitude_msl = gp.alt / 1000.0f;
            bridge->position.altitude_rel = gp.relative_alt / 1000.0f;
            bridge->position.vx = gp.vx / 100.0f;   /* cm/s → m/s */
            bridge->position.vy = gp.vy / 100.0f;
            bridge->position.vz = gp.vz / 100.0f;
            bridge->position.heading = gp.hdg / 100.0f;  /* cdeg → deg */
            bridge->position.timestamp_us = nimcp_time_now_us();

            /* Set home position from first valid position */
            if (!bridge->home_set &&
                gp.lat != 0 && gp.lon != 0) {
                bridge->home_lat = gp.lat / 1e7;
                bridge->home_lon = gp.lon / 1e7;
                bridge->home_alt = gp.relative_alt / 1000.0f;
                bridge->home_set = true;
                LOG_INFO("[%s] Home position set: (%.6f, %.6f)",
                         LOG_MODULE, bridge->home_lat, bridge->home_lon);
            }
            nimcp_mutex_unlock(bridge->telemetry_lock);
            break;
        }

        case MAVLINK_MSG_ID_SYS_STATUS: {
            mavlink_sys_status_t sys;
            mavlink_msg_sys_status_decode(msg, &sys);

            nimcp_mutex_lock(bridge->telemetry_lock);
            bridge->battery.voltage = sys.voltage_battery / 1000.0f;  /* mV → V */
            bridge->battery.current = sys.current_battery / 100.0f;   /* cA → A */
            bridge->battery.remaining_pct = (float)sys.battery_remaining;
            bridge->battery.timestamp_us = nimcp_time_now_us();
            nimcp_mutex_unlock(bridge->telemetry_lock);

            /* Auto-RTL on low battery */
            if (bridge->config.enable_battery_rtl &&
                sys.battery_remaining > 0 &&
                sys.battery_remaining < (int8_t)bridge->config.min_battery_pct) {
                LOG_WARN("[%s] Battery %d%% < %.0f%% — triggering RTL",
                         LOG_MODULE, sys.battery_remaining,
                         bridge->config.min_battery_pct);
                nimcp_mavlink_rtl(bridge);
            }
            break;
        }

        case MAVLINK_MSG_ID_GPS_RAW_INT: {
            mavlink_gps_raw_int_t gps;
            mavlink_msg_gps_raw_int_decode(msg, &gps);

            nimcp_mutex_lock(bridge->telemetry_lock);
            bridge->position.fix_type = gps.fix_type;
            bridge->position.satellites = gps.satellites_visible;
            nimcp_mutex_unlock(bridge->telemetry_lock);
            break;
        }

        default:
            /* Ignore unhandled messages */
            break;
    }
}

/* ============================================================================
 * MAVLink Send Helpers (full mode only)
 * ============================================================================ */

static int _send_bytes(nimcp_mavlink_bridge_t* bridge,
                        const uint8_t* data, uint32_t len) {
    if (bridge->fd < 0) {
        return -1;
    }

    ssize_t written = write(bridge->fd, data, len);
    if (written < 0) {
        LOG_ERROR("[%s] Write failed: %s", LOG_MODULE, strerror(errno));
        return -1;
    }
    if ((uint32_t)written != len) {
        LOG_WARN("[%s] Partial write: %zd/%u bytes", LOG_MODULE, written, len);
    }
    return 0;
}

static int _send_heartbeat(nimcp_mavlink_bridge_t* bridge) {
    mavlink_message_t msg;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];

    mavlink_msg_heartbeat_pack(
        bridge->config.system_id,
        bridge->config.component_id,
        &msg,
        MAV_TYPE_ONBOARD_CONTROLLER,     /* type */
        MAV_AUTOPILOT_INVALID,           /* autopilot (we're not an autopilot) */
        0,                                /* base_mode */
        0,                                /* custom_mode */
        MAV_STATE_ACTIVE);               /* system_status */

    uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
    return _send_bytes(bridge, buf, len);
}

static int _send_command_long(nimcp_mavlink_bridge_t* bridge,
                               uint16_t command, float p1, float p2,
                               float p3, float p4, float p5, float p6, float p7) {
    if (!bridge->connected || bridge->fd < 0) {
        return -1;
    }

    mavlink_message_t msg;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];

    mavlink_msg_command_long_pack(
        bridge->config.system_id,
        bridge->config.component_id,
        &msg,
        bridge->config.target_system,
        bridge->config.target_component,
        command,
        0,                  /* confirmation */
        p1, p2, p3, p4, p5, p6, p7);

    uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
    if (_send_bytes(bridge, buf, len) < 0) {
        LOG_ERROR("[%s] Failed to send COMMAND_LONG %u", LOG_MODULE, command);
        return -1;
    }

    return 0;
}

static int _request_data_stream(nimcp_mavlink_bridge_t* bridge,
                                 uint16_t msg_id, float rate_hz) {
    if (rate_hz <= 0.0f) {
        return 0;
    }

    /* MAV_CMD_SET_MESSAGE_INTERVAL (511):
     * param1 = message_id, param2 = interval_us */
    float interval_us = 1e6f / rate_hz;

    int rc = _send_command_long(bridge, 511,
                                 (float)msg_id, interval_us,
                                 0, 0, 0, 0, 0);
    if (rc == 0) {
        LOG_INFO("[%s] Requested msg %u at %.0f Hz", LOG_MODULE, msg_id, rate_hz);
    }
    return rc;
}

/* ============================================================================
 * Transport: Serial
 * ============================================================================ */

static int _open_serial(nimcp_mavlink_bridge_t* bridge) {
    int fd = open(bridge->config.connection_string, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        LOG_ERROR("[%s] Failed to open %s: %s",
                  LOG_MODULE, bridge->config.connection_string, strerror(errno));
        return -1;
    }

    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(fd, &tty) != 0) {
        LOG_ERROR("[%s] tcgetattr failed: %s", LOG_MODULE, strerror(errno));
        close(fd);
        return -1;
    }

    /* Map baud rate to termios constant */
    speed_t baud;
    switch (bridge->config.baud_rate) {
        case 9600:    baud = B9600;    break;
        case 19200:   baud = B19200;   break;
        case 38400:   baud = B38400;   break;
        case 57600:   baud = B57600;   break;
        case 115200:  baud = B115200;  break;
        case 230400:  baud = B230400;  break;
        case 460800:  baud = B460800;  break;
        case 921600:  baud = B921600;  break;
        default:
            LOG_WARN("[%s] Unsupported baud %u, using 921600",
                     LOG_MODULE, bridge->config.baud_rate);
            baud = B921600;
            break;
    }

    cfsetospeed(&tty, baud);
    cfsetispeed(&tty, baud);

    /* 8N1 */
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;

    /* No flow control */
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= CREAD | CLOCAL;

    /* Raw mode (no canonical, no echo, no signals) */
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_oflag &= ~OPOST;

    /* Non-blocking read: return immediately with whatever is available */
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;  /* 100ms timeout */

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        LOG_ERROR("[%s] tcsetattr failed: %s", LOG_MODULE, strerror(errno));
        close(fd);
        return -1;
    }

    bridge->fd = fd;
    LOG_INFO("[%s] Serial port %s opened at %u baud",
             LOG_MODULE, bridge->config.connection_string, bridge->config.baud_rate);
    return 0;
}

/* ============================================================================
 * Transport: UDP
 * ============================================================================ */

static int _open_udp(nimcp_mavlink_bridge_t* bridge) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        LOG_ERROR("[%s] Failed to create UDP socket: %s", LOG_MODULE, strerror(errno));
        return -1;
    }

    /* Parse port from connection_string "udp:PORT" or just "PORT" */
    uint16_t port = 14550;
    const char* cs = bridge->config.connection_string;
    if (strncmp(cs, "udp:", 4) == 0) {
        port = (uint16_t)atoi(cs + 4);
    } else {
        port = (uint16_t)atoi(cs);
    }
    if (port == 0) {
        port = 14550;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    /* Allow address reuse */
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("[%s] Failed to bind UDP port %u: %s",
                  LOG_MODULE, port, strerror(errno));
        close(sock);
        return -1;
    }

    /* Set non-blocking */
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    }

    bridge->fd = sock;
    LOG_INFO("[%s] UDP socket bound to port %u", LOG_MODULE, port);
    return 0;
}

/* ============================================================================
 * Transport: TCP
 * ============================================================================ */

static int _open_tcp(nimcp_mavlink_bridge_t* bridge) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        LOG_ERROR("[%s] Failed to create TCP socket: %s", LOG_MODULE, strerror(errno));
        return -1;
    }

    /* Parse host:port from connection_string "tcp:HOST:PORT" or "HOST:PORT" */
    char host[256] = "127.0.0.1";
    uint16_t port = 5760;

    const char* cs = bridge->config.connection_string;
    if (strncmp(cs, "tcp:", 4) == 0) {
        cs += 4;
    }

    const char* colon = strrchr(cs, ':');
    if (colon) {
        size_t host_len = (size_t)(colon - cs);
        if (host_len > 0 && host_len < sizeof(host)) {
            strncpy(host, cs, host_len);
            host[host_len] = '\0';
        }
        port = (uint16_t)atoi(colon + 1);
    }
    if (port == 0) {
        port = 5760;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        LOG_ERROR("[%s] Invalid TCP host: %s", LOG_MODULE, host);
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("[%s] Failed to connect TCP %s:%u: %s",
                  LOG_MODULE, host, port, strerror(errno));
        close(sock);
        return -1;
    }

    /* Set non-blocking after connect */
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    }

    bridge->fd = sock;
    LOG_INFO("[%s] TCP connected to %s:%u", LOG_MODULE, host, port);
    return 0;
}

#endif /* NIMCP_HAS_MAVLINK */

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static float _clampf(float val, float lo, float hi) {
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

/**
 * @brief Haversine distance between two lat/lon points in meters.
 */
static double _haversine_distance_m(double lat1, double lon1,
                                     double lat2, double lon2) {
    double dlat = (lat2 - lat1) * M_PI / 180.0;
    double dlon = (lon2 - lon1) * M_PI / 180.0;
    double lat1_rad = lat1 * M_PI / 180.0;
    double lat2_rad = lat2 * M_PI / 180.0;

    double a = sin(dlat / 2.0) * sin(dlat / 2.0) +
               cos(lat1_rad) * cos(lat2_rad) *
               sin(dlon / 2.0) * sin(dlon / 2.0);
    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));

    return EARTH_RADIUS_M * c;
}
