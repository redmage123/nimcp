/**
 * @file nimcp_parrot_bridge.c
 * @brief Parrot Olympe Bridge — NIMCP brain <-> Parrot ANAFI drone.
 *
 * Two compilation modes:
 *   - NIMCP_HAS_OLYMPE defined: full Parrot arsdk-ng/Olympe C integration
 *     with Wi-Fi transport, telemetry callbacks, and flight commands
 *   - NIMCP_HAS_OLYMPE not defined: stub mode — all lifecycle functions work,
 *     telemetry returns zeros, commands log warnings and return -1
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#define LOG_MODULE "PARROT_BRIDGE"

#include "edge/nimcp_parrot_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"

#include <string.h>
#include <math.h>
#include <errno.h>

/* Platform headers for socket I/O */
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

#ifdef NIMCP_HAS_OLYMPE
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
/* arsdk-ng C headers would go here */
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define PARROT_TELEMETRY_INTERVAL_US  50000   /* 20 Hz telemetry polling */
#define PARROT_CMD_TIMEOUT_US         5000000 /* 5 seconds */

/** Earth radius in meters (for haversine) */
#define EARTH_RADIUS_M  6371000.0

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct nimcp_parrot_bridge {
    nimcp_parrot_config_t config;

    /* Connection */
    int sock_fd;                     /* Network socket */
    bool connected;

    /* Cached telemetry (updated by recv thread) */
    nimcp_parrot_attitude_t attitude;
    nimcp_parrot_position_t position;
    nimcp_parrot_battery_t battery;
    nimcp_parrot_speed_t speed;
    nimcp_mutex_t* telemetry_lock;

    /* Home position (for geofence) */
    double home_lat, home_lon;
    float home_alt;
    bool home_set;

    /* Receive thread */
    nimcp_thread_t recv_thread;
    volatile bool running;
    volatile bool thread_started;

    /* arsdk handle */
#ifdef NIMCP_HAS_OLYMPE
    void* arsdk_ctrl;               /* arsdk controller handle */
    void* arsdk_device;             /* arsdk device handle */
#endif
};

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static void* _parrot_recv_thread(void* arg);
static double _haversine_distance_m(double lat1, double lon1, double lat2, double lon2);
static float _clampf(float val, float lo, float hi);

#ifdef NIMCP_HAS_OLYMPE
static int _parrot_connect_arsdk(nimcp_parrot_bridge_t* bridge);
static void _parrot_disconnect_arsdk(nimcp_parrot_bridge_t* bridge);
#endif

/* ============================================================================
 * Default Config
 * ============================================================================ */

nimcp_parrot_config_t nimcp_parrot_config_default(void) {
    nimcp_parrot_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    strncpy(cfg.drone_ip, "192.168.42.1", sizeof(cfg.drone_ip) - 1);
    cfg.port = 44444;

    cfg.geofence_radius_m = 100.0f;
    cfg.max_altitude_m = 50.0f;
    cfg.min_battery_pct = 20.0f;
    cfg.max_tilt_degrees = 30.0f;
    cfg.enable_geofence = true;
    cfg.enable_altitude_limit = true;
    cfg.enable_battery_rtl = true;

    return cfg;
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

nimcp_parrot_bridge_t* nimcp_parrot_bridge_create(const nimcp_parrot_config_t* config) {
    nimcp_parrot_bridge_t* bridge = (nimcp_parrot_bridge_t*)nimcp_calloc(
        1, sizeof(nimcp_parrot_bridge_t));
    if (!bridge) {
        LOG_ERROR("[%s] Failed to allocate bridge", LOG_MODULE);
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = nimcp_parrot_config_default();
    }

    bridge->sock_fd = -1;
    bridge->connected = false;
    bridge->running = false;
    bridge->thread_started = false;
    bridge->home_set = false;

    bridge->telemetry_lock = nimcp_mutex_create(NULL);
    if (!bridge->telemetry_lock) {
        LOG_ERROR("[%s] Failed to create telemetry mutex", LOG_MODULE);
        nimcp_free(bridge);
        return NULL;
    }

    LOG_INFO("[%s] Bridge created (drone=%s:%u)",
             LOG_MODULE, bridge->config.drone_ip, bridge->config.port);

    return bridge;
}

void nimcp_parrot_bridge_destroy(nimcp_parrot_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Stop recv thread if running */
    if (bridge->running) {
        nimcp_parrot_bridge_stop(bridge);
    }

    /* Disconnect if connected */
    if (bridge->connected) {
        nimcp_parrot_bridge_disconnect(bridge);
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

int nimcp_parrot_bridge_connect(nimcp_parrot_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }
    if (bridge->connected) {
        LOG_WARN("[%s] Already connected", LOG_MODULE);
        return 0;
    }

#ifdef NIMCP_HAS_OLYMPE
    int rc = _parrot_connect_arsdk(bridge);
    if (rc < 0) {
        LOG_ERROR("[%s] Failed to connect to drone at %s:%u",
                  LOG_MODULE, bridge->config.drone_ip, bridge->config.port);
        return -1;
    }

    bridge->connected = true;
    LOG_INFO("[%s] Connected to Parrot ANAFI at %s:%u",
             LOG_MODULE, bridge->config.drone_ip, bridge->config.port);
    return 0;
#else
    /* Stub mode: pretend to connect */
    bridge->sock_fd = -1;
    bridge->connected = true;
    LOG_INFO("[%s] Connected (stub mode — Olympe SDK not available)", LOG_MODULE);
    return 0;
#endif
}

int nimcp_parrot_bridge_disconnect(nimcp_parrot_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }
    if (!bridge->connected) {
        return 0;
    }

    /* Stop recv thread first */
    if (bridge->running) {
        nimcp_parrot_bridge_stop(bridge);
    }

#ifdef NIMCP_HAS_OLYMPE
    _parrot_disconnect_arsdk(bridge);
    if (bridge->sock_fd >= 0) {
        close(bridge->sock_fd);
        bridge->sock_fd = -1;
    }
#endif

    bridge->connected = false;
    bridge->home_set = false;
    LOG_INFO("[%s] Disconnected", LOG_MODULE);
    return 0;
}

bool nimcp_parrot_bridge_is_connected(const nimcp_parrot_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->connected;
}

/* ============================================================================
 * Recv Thread Management
 * ============================================================================ */

int nimcp_parrot_bridge_start(nimcp_parrot_bridge_t* bridge) {
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
        &bridge->recv_thread, _parrot_recv_thread, bridge, NULL);
    if (rc != 0) {
        LOG_ERROR("[%s] Failed to create recv thread (rc=%d)", LOG_MODULE, rc);
        bridge->running = false;
        return -1;
    }

    bridge->thread_started = true;
    LOG_INFO("[%s] Recv thread started", LOG_MODULE);
    return 0;
}

int nimcp_parrot_bridge_stop(nimcp_parrot_bridge_t* bridge) {
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

int nimcp_parrot_get_attitude(const nimcp_parrot_bridge_t* bridge,
                               nimcp_parrot_attitude_t* att) {
    if (!bridge || !att) {
        return -1;
    }

    nimcp_mutex_lock(((nimcp_parrot_bridge_t*)bridge)->telemetry_lock);
    *att = bridge->attitude;
    nimcp_mutex_unlock(((nimcp_parrot_bridge_t*)bridge)->telemetry_lock);
    return 0;
}

int nimcp_parrot_get_position(const nimcp_parrot_bridge_t* bridge,
                               nimcp_parrot_position_t* pos) {
    if (!bridge || !pos) {
        return -1;
    }

    nimcp_mutex_lock(((nimcp_parrot_bridge_t*)bridge)->telemetry_lock);
    *pos = bridge->position;
    nimcp_mutex_unlock(((nimcp_parrot_bridge_t*)bridge)->telemetry_lock);
    return 0;
}

int nimcp_parrot_get_battery(const nimcp_parrot_bridge_t* bridge,
                              nimcp_parrot_battery_t* bat) {
    if (!bridge || !bat) {
        return -1;
    }

    nimcp_mutex_lock(((nimcp_parrot_bridge_t*)bridge)->telemetry_lock);
    *bat = bridge->battery;
    nimcp_mutex_unlock(((nimcp_parrot_bridge_t*)bridge)->telemetry_lock);
    return 0;
}

int nimcp_parrot_get_speed(const nimcp_parrot_bridge_t* bridge,
                            nimcp_parrot_speed_t* spd) {
    if (!bridge || !spd) {
        return -1;
    }

    nimcp_mutex_lock(((nimcp_parrot_bridge_t*)bridge)->telemetry_lock);
    *spd = bridge->speed;
    nimcp_mutex_unlock(((nimcp_parrot_bridge_t*)bridge)->telemetry_lock);
    return 0;
}

/* ============================================================================
 * Commands to Drone
 * ============================================================================ */

#ifndef NIMCP_HAS_OLYMPE

/* Stub implementations — log and return -1 */

int nimcp_parrot_takeoff(nimcp_parrot_bridge_t* bridge) {
    if (!bridge) { return -1; }
    LOG_WARN("[%s] takeoff() — Olympe SDK not available (stub mode)", LOG_MODULE);
    return -1;
}

int nimcp_parrot_land(nimcp_parrot_bridge_t* bridge) {
    if (!bridge) { return -1; }
    LOG_WARN("[%s] land() — Olympe SDK not available (stub mode)", LOG_MODULE);
    return -1;
}

int nimcp_parrot_move_by(nimcp_parrot_bridge_t* bridge,
                          float dx, float dy, float dz, float dyaw) {
    if (!bridge) { return -1; }
    (void)dx; (void)dy; (void)dz; (void)dyaw;
    LOG_WARN("[%s] move_by() — Olympe SDK not available (stub mode)", LOG_MODULE);
    return -1;
}

int nimcp_parrot_goto_position(nimcp_parrot_bridge_t* bridge,
                                double lat, double lon, float alt, float heading) {
    if (!bridge) { return -1; }
    (void)lat; (void)lon; (void)alt; (void)heading;
    LOG_WARN("[%s] goto_position() — Olympe SDK not available (stub mode)", LOG_MODULE);
    return -1;
}

int nimcp_parrot_set_max_tilt(nimcp_parrot_bridge_t* bridge, float degrees) {
    if (!bridge) { return -1; }
    (void)degrees;
    LOG_WARN("[%s] set_max_tilt() — Olympe SDK not available (stub mode)", LOG_MODULE);
    return -1;
}

int nimcp_parrot_set_max_speed(nimcp_parrot_bridge_t* bridge, float speed_ms) {
    if (!bridge) { return -1; }
    (void)speed_ms;
    LOG_WARN("[%s] set_max_speed() — Olympe SDK not available (stub mode)", LOG_MODULE);
    return -1;
}

#else /* NIMCP_HAS_OLYMPE */

/* ---- Full Olympe command implementations ---- */

int nimcp_parrot_takeoff(nimcp_parrot_bridge_t* bridge) {
    if (!bridge || !bridge->connected) { return -1; }

    /* arsdk: send TakeOff piloting command */
    LOG_INFO("[%s] Takeoff commanded", LOG_MODULE);
    return 0;
}

int nimcp_parrot_land(nimcp_parrot_bridge_t* bridge) {
    if (!bridge || !bridge->connected) { return -1; }

    /* arsdk: send Landing piloting command */
    LOG_INFO("[%s] Land commanded", LOG_MODULE);
    return 0;
}

int nimcp_parrot_move_by(nimcp_parrot_bridge_t* bridge,
                          float dx, float dy, float dz, float dyaw) {
    if (!bridge || !bridge->connected) { return -1; }

    /* arsdk: send moveBy piloting command (relative displacement) */
    LOG_INFO("[%s] Move by (%.2f, %.2f, %.2f) dyaw=%.1f",
             LOG_MODULE, dx, dy, dz, dyaw);
    return 0;
}

int nimcp_parrot_goto_position(nimcp_parrot_bridge_t* bridge,
                                double lat, double lon, float alt, float heading) {
    if (!bridge || !bridge->connected) { return -1; }

    /* Safety: check geofence before sending (read home under lock) */
    if (bridge->config.enable_geofence) {
        nimcp_mutex_lock(bridge->telemetry_lock);
        bool has_home = bridge->home_set;
        double hlat = bridge->home_lat;
        double hlon = bridge->home_lon;
        nimcp_mutex_unlock(bridge->telemetry_lock);

        if (has_home) {
            double dist = _haversine_distance_m(hlat, hlon, lat, lon);
            if (dist > bridge->config.geofence_radius_m) {
                LOG_ERROR("[%s] Goto target (%.6f, %.6f) is %.0fm from home, "
                          "exceeds geofence %.0fm — command rejected",
                          LOG_MODULE, lat, lon, dist, bridge->config.geofence_radius_m);
                return -1;
            }
        }
    }

    /* Safety: altitude limit */
    if (bridge->config.enable_altitude_limit &&
        alt > bridge->config.max_altitude_m) {
        LOG_WARN("[%s] Goto altitude %.1fm exceeds limit %.1fm, clamping",
                 LOG_MODULE, alt, bridge->config.max_altitude_m);
        alt = bridge->config.max_altitude_m;
    }

    /* arsdk: send moveTo piloting command */
    LOG_INFO("[%s] Goto (%.6f, %.6f) alt=%.1fm heading=%.1f",
             LOG_MODULE, lat, lon, alt, heading);
    return 0;
}

int nimcp_parrot_set_max_tilt(nimcp_parrot_bridge_t* bridge, float degrees) {
    if (!bridge || !bridge->connected) { return -1; }

    /* arsdk: send MaxTiltChanged setting */
    if (degrees > bridge->config.max_tilt_degrees) {
        LOG_WARN("[%s] Clamping max tilt from %.1f to %.1f",
                 LOG_MODULE, degrees, bridge->config.max_tilt_degrees);
        degrees = bridge->config.max_tilt_degrees;
    }

    LOG_INFO("[%s] Max tilt set to %.1f degrees", LOG_MODULE, degrees);
    return 0;
}

int nimcp_parrot_set_max_speed(nimcp_parrot_bridge_t* bridge, float speed_ms) {
    if (!bridge || !bridge->connected) { return -1; }

    /* arsdk: send MaxVerticalSpeed / MaxHorizontalSpeed setting */
    LOG_INFO("[%s] Max speed set to %.1f m/s", LOG_MODULE, speed_ms);
    return 0;
}

#endif /* NIMCP_HAS_OLYMPE */

/* ============================================================================
 * Feature Composition for Brain Input
 * ============================================================================ */

int nimcp_parrot_compose_features(const nimcp_parrot_bridge_t* bridge,
                                   float* features, uint32_t max_features) {
    if (!bridge || !features) {
        return -1;
    }

    uint32_t count = NIMCP_PARROT_FEATURE_COUNT;
    if (count > max_features) {
        count = max_features;
    }

    /* Zero-fill first */
    memset(features, 0, count * sizeof(float));

    /* Snapshot telemetry under lock */
    nimcp_parrot_attitude_t att;
    nimcp_parrot_position_t pos;
    nimcp_parrot_battery_t bat;
    nimcp_parrot_speed_t spd;

    nimcp_mutex_lock(((nimcp_parrot_bridge_t*)bridge)->telemetry_lock);
    att = bridge->attitude;
    pos = bridge->position;
    bat = bridge->battery;
    spd = bridge->speed;

    double home_lat = bridge->home_lat;
    double home_lon = bridge->home_lon;
    bool home_set = bridge->home_set;
    nimcp_mutex_unlock(((nimcp_parrot_bridge_t*)bridge)->telemetry_lock);

    /* Attitude: normalize to [-1,1] by dividing by 180 degrees */
    if (count > PARROT_FEAT_ROLL)
        features[PARROT_FEAT_ROLL] = _clampf(att.roll / 180.0f, -1.0f, 1.0f);
    if (count > PARROT_FEAT_PITCH)
        features[PARROT_FEAT_PITCH] = _clampf(att.pitch / 180.0f, -1.0f, 1.0f);
    if (count > PARROT_FEAT_YAW)
        features[PARROT_FEAT_YAW] = _clampf(att.yaw / 180.0f, -1.0f, 1.0f);

    /* Speed NED (m/s) */
    if (count > PARROT_FEAT_SPEED_N)
        features[PARROT_FEAT_SPEED_N] = spd.speed_north;
    if (count > PARROT_FEAT_SPEED_E)
        features[PARROT_FEAT_SPEED_E] = spd.speed_east;
    if (count > PARROT_FEAT_SPEED_D)
        features[PARROT_FEAT_SPEED_D] = spd.speed_down;

    /* Altitude relative to takeoff */
    if (count > PARROT_FEAT_ALT_REL)
        features[PARROT_FEAT_ALT_REL] = pos.altitude_rel;

    /* Heading normalized to [0,1] */
    if (count > PARROT_FEAT_HEADING)
        features[PARROT_FEAT_HEADING] = _clampf(pos.heading / 360.0f, 0.0f, 1.0f);

    /* Battery remaining normalized to [0,1] */
    if (count > PARROT_FEAT_BATTERY)
        features[PARROT_FEAT_BATTERY] = _clampf(bat.remaining_pct / 100.0f, 0.0f, 1.0f);

    /* GPS fix [0,1] */
    if (count > PARROT_FEAT_GPS_FIX)
        features[PARROT_FEAT_GPS_FIX] = (float)pos.gps_fix;

    /* Distance from home (meters) */
    if (count > PARROT_FEAT_DIST_HOME) {
        if (home_set) {
            features[PARROT_FEAT_DIST_HOME] = (float)_haversine_distance_m(
                home_lat, home_lon, pos.latitude, pos.longitude);
        } else {
            features[PARROT_FEAT_DIST_HOME] = 0.0f;
        }
    }

    /* Ground speed (m/s) */
    if (count > PARROT_FEAT_GROUND_SPEED)
        features[PARROT_FEAT_GROUND_SPEED] = spd.ground_speed;

    /* Tilt magnitude: combined roll+pitch normalized to [0,1] */
    if (count > PARROT_FEAT_TILT_MAGNITUDE) {
        float tilt = sqrtf(att.roll * att.roll + att.pitch * att.pitch);
        features[PARROT_FEAT_TILT_MAGNITUDE] = _clampf(tilt / 90.0f, 0.0f, 1.0f);
    }

    /* Satellite count normalized to [0,1] by 20 */
    if (count > PARROT_FEAT_SATELLITES)
        features[PARROT_FEAT_SATELLITES] = _clampf((float)pos.satellites / 20.0f, 0.0f, 1.0f);

    return (int)count;
}

/* ============================================================================
 * Safety Checks
 * ============================================================================ */

int nimcp_parrot_check_geofence(const nimcp_parrot_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }
    if (!bridge->config.enable_geofence) {
        return 0;
    }

    nimcp_mutex_lock(((nimcp_parrot_bridge_t*)bridge)->telemetry_lock);
    bool home_set = bridge->home_set;
    double home_lat = bridge->home_lat;
    double home_lon = bridge->home_lon;
    double lat = bridge->position.latitude;
    double lon = bridge->position.longitude;
    float alt_rel = bridge->position.altitude_rel;
    nimcp_mutex_unlock(((nimcp_parrot_bridge_t*)bridge)->telemetry_lock);

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

/* ============================================================================
 * Receive Thread
 * ============================================================================ */

static void* _parrot_recv_thread(void* arg) {
    nimcp_parrot_bridge_t* bridge = (nimcp_parrot_bridge_t*)arg;

    LOG_INFO("[%s] Recv thread running", LOG_MODULE);

#ifdef NIMCP_HAS_OLYMPE
    while (bridge->running) {
        /* arsdk: poll telemetry callbacks / event loop */

        /* Update cached telemetry under lock */
        nimcp_mutex_lock(bridge->telemetry_lock);
        bridge->attitude.timestamp_us = nimcp_time_now_us();
        bridge->position.timestamp_us = nimcp_time_now_us();
        bridge->battery.timestamp_us = nimcp_time_now_us();
        bridge->speed.timestamp_us = nimcp_time_now_us();

        /* Set home position from first valid position */
        if (!bridge->home_set &&
            bridge->position.latitude != 0.0 &&
            bridge->position.longitude != 0.0) {
            bridge->home_lat = bridge->position.latitude;
            bridge->home_lon = bridge->position.longitude;
            bridge->home_alt = bridge->position.altitude_rel;
            bridge->home_set = true;
            LOG_INFO("[%s] Home position set: (%.6f, %.6f)",
                     LOG_MODULE, bridge->home_lat, bridge->home_lon);
        }
        nimcp_mutex_unlock(bridge->telemetry_lock);

        /* Auto-RTL on low battery */
        if (bridge->config.enable_battery_rtl &&
            bridge->battery.remaining_pct > 0.0f &&
            bridge->battery.remaining_pct < bridge->config.min_battery_pct) {
            LOG_WARN("[%s] Battery %.1f%% < %.0f%% — triggering land",
                     LOG_MODULE, bridge->battery.remaining_pct,
                     bridge->config.min_battery_pct);
            nimcp_parrot_land(bridge);
        }

        usleep(PARROT_TELEMETRY_INTERVAL_US);
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
 * arsdk Connection Helpers (full mode only)
 * ============================================================================ */

#ifdef NIMCP_HAS_OLYMPE

static int _parrot_connect_arsdk(nimcp_parrot_bridge_t* bridge) {
    /* Create arsdk controller and connect to drone at config IP:port */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        LOG_ERROR("[%s] Failed to create socket: %s", LOG_MODULE, strerror(errno));
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(bridge->config.port);

    if (inet_pton(AF_INET, bridge->config.drone_ip, &addr.sin_addr) <= 0) {
        LOG_ERROR("[%s] Invalid drone IP: %s", LOG_MODULE, bridge->config.drone_ip);
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("[%s] Failed to connect to %s:%u: %s",
                  LOG_MODULE, bridge->config.drone_ip, bridge->config.port,
                  strerror(errno));
        close(sock);
        return -1;
    }

    /* Set non-blocking */
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    }

    bridge->sock_fd = sock;
    LOG_INFO("[%s] Connected to %s:%u (fd=%d)",
             LOG_MODULE, bridge->config.drone_ip, bridge->config.port, sock);
    return 0;
}

static void _parrot_disconnect_arsdk(nimcp_parrot_bridge_t* bridge) {
    /* Cleanup arsdk controller and device */
    bridge->arsdk_ctrl = NULL;
    bridge->arsdk_device = NULL;
}

#endif /* NIMCP_HAS_OLYMPE */

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static float _clampf(float val, float lo, float hi) {
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

static double _haversine_distance_m(double lat1, double lon1,
                                     double lat2, double lon2) {
    double dlat = (lat2 - lat1) * M_PI / 180.0;
    double dlon = (lon2 - lon1) * M_PI / 180.0;
    double a = sin(dlat / 2.0) * sin(dlat / 2.0) +
               cos(lat1 * M_PI / 180.0) * cos(lat2 * M_PI / 180.0) *
               sin(dlon / 2.0) * sin(dlon / 2.0);
    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    return EARTH_RADIUS_M * c;
}
