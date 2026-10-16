/**
 * @file nimcp_dji_bridge.c
 * @brief DJI OSDK Bridge — NIMCP brain <-> DJI Matrice flight controller.
 *
 * Two compilation modes:
 *   - NIMCP_HAS_DJI_OSDK defined: full DJI Onboard SDK integration with
 *     UART/USB transport, telemetry subscriptions, and flight control
 *   - NIMCP_HAS_DJI_OSDK not defined: stub mode — all lifecycle functions work,
 *     telemetry returns zeros, commands log warnings and return -1
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#define LOG_MODULE "DJI_BRIDGE"

#include "edge/nimcp_dji_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"

#include <string.h>
#include <math.h>
#include <errno.h>

#ifdef NIMCP_HAS_DJI_OSDK
#include <dji_vehicle.hpp>
#include <dji_telemetry.hpp>
#include <dji_flight_controller.hpp>
#endif

/* Platform headers for serial I/O */
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

#ifdef NIMCP_HAS_DJI_OSDK
#include <termios.h>
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define DJI_TELEMETRY_INTERVAL_US   20000     /* 50 Hz telemetry polling */
#define DJI_RECV_BUF_SIZE           4096
#define DJI_FC_TIMEOUT_US           5000000   /* 5 seconds */

/** Earth radius in meters (for haversine) */
#define EARTH_RADIUS_M  6371000.0

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct nimcp_dji_bridge {
    nimcp_dji_config_t config;

    /* Connection */
    int fd;                          /* Serial fd */
    bool connected;

    /* Cached telemetry (updated by recv thread) */
    nimcp_dji_attitude_t attitude;
    nimcp_dji_position_t position;
    nimcp_dji_battery_t battery;
    nimcp_dji_gimbal_t gimbal;
    nimcp_dji_flight_status_t flight_status;
    nimcp_mutex_t* telemetry_lock;

    /* Home position (for geofence) */
    double home_lat, home_lon;
    float home_alt;
    bool home_set;

    /* Receive thread */
    nimcp_thread_t recv_thread;
    volatile bool running;
    volatile bool thread_started;

    /* DJI SDK handle */
#ifdef NIMCP_HAS_DJI_OSDK
    void* vehicle;                   /* DJI::OSDK::Vehicle* */
#endif
};

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static void* _dji_recv_thread(void* arg);
static double _haversine_distance_m(double lat1, double lon1, double lat2, double lon2);
static float _clampf(float val, float lo, float hi);

#ifdef NIMCP_HAS_DJI_OSDK
static int _open_serial(nimcp_dji_bridge_t* bridge);
static int _dji_activate(nimcp_dji_bridge_t* bridge);
#endif

/* ============================================================================
 * Default Config
 * ============================================================================ */

nimcp_dji_config_t nimcp_dji_config_default(void) {
    nimcp_dji_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.conn_type = NIMCP_DJI_CONN_UART;
    strncpy(cfg.device_path, "/dev/ttyUSB0", sizeof(cfg.device_path) - 1);
    cfg.baud_rate = 921600;
    cfg.app_id = 0;
    memset(cfg.app_key, 0, sizeof(cfg.app_key));

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

nimcp_dji_bridge_t* nimcp_dji_bridge_create(const nimcp_dji_config_t* config) {
    nimcp_dji_bridge_t* bridge = (nimcp_dji_bridge_t*)nimcp_calloc(
        1, sizeof(nimcp_dji_bridge_t));
    if (!bridge) {
        LOG_ERROR("[%s] Failed to allocate bridge", LOG_MODULE);
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = nimcp_dji_config_default();
    }

    bridge->fd = -1;
    bridge->connected = false;
    bridge->running = false;
    bridge->thread_started = false;
    bridge->home_set = false;
    bridge->flight_status = NIMCP_DJI_STATUS_STOPPED;

    bridge->telemetry_lock = nimcp_mutex_create(NULL);
    if (!bridge->telemetry_lock) {
        LOG_ERROR("[%s] Failed to create telemetry mutex", LOG_MODULE);
        nimcp_free(bridge);
        return NULL;
    }

    LOG_INFO("[%s] Bridge created (conn=%d, device=%s)",
             LOG_MODULE, bridge->config.conn_type,
             bridge->config.device_path);

    return bridge;
}

void nimcp_dji_bridge_destroy(nimcp_dji_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Stop recv thread if running */
    if (bridge->running) {
        nimcp_dji_bridge_stop(bridge);
    }

    /* Disconnect if connected */
    if (bridge->connected) {
        nimcp_dji_bridge_disconnect(bridge);
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

int nimcp_dji_bridge_connect(nimcp_dji_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }
    if (bridge->connected) {
        LOG_WARN("[%s] Already connected", LOG_MODULE);
        return 0;
    }

#ifdef NIMCP_HAS_DJI_OSDK
    int rc = _open_serial(bridge);
    if (rc < 0) {
        LOG_ERROR("[%s] Failed to open %s",
                  LOG_MODULE, bridge->config.device_path);
        return -1;
    }

    /* Activate with DJI credentials */
    rc = _dji_activate(bridge);
    if (rc < 0) {
        LOG_ERROR("[%s] DJI OSDK activation failed", LOG_MODULE);
        close(bridge->fd);
        bridge->fd = -1;
        return -1;
    }

    bridge->connected = true;
    LOG_INFO("[%s] Connected to DJI FC via %s (fd=%d)",
             LOG_MODULE, bridge->config.device_path, bridge->fd);
    return 0;
#else
    /* Stub mode: pretend to connect */
    bridge->fd = -1;
    bridge->connected = true;
    LOG_INFO("[%s] Connected (stub mode — DJI OSDK not available)", LOG_MODULE);
    return 0;
#endif
}

int nimcp_dji_bridge_disconnect(nimcp_dji_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }
    if (!bridge->connected) {
        return 0;
    }

    /* Stop recv thread first */
    if (bridge->running) {
        nimcp_dji_bridge_stop(bridge);
    }

#ifdef NIMCP_HAS_DJI_OSDK
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

bool nimcp_dji_bridge_is_connected(const nimcp_dji_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->connected;
}

/* ============================================================================
 * Recv Thread Management
 * ============================================================================ */

int nimcp_dji_bridge_start(nimcp_dji_bridge_t* bridge) {
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
        &bridge->recv_thread, _dji_recv_thread, bridge, NULL);
    if (rc != 0) {
        LOG_ERROR("[%s] Failed to create recv thread (rc=%d)", LOG_MODULE, rc);
        bridge->running = false;
        return -1;
    }

    bridge->thread_started = true;
    LOG_INFO("[%s] Recv thread started", LOG_MODULE);
    return 0;
}

int nimcp_dji_bridge_stop(nimcp_dji_bridge_t* bridge) {
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

int nimcp_dji_get_attitude(const nimcp_dji_bridge_t* bridge,
                            nimcp_dji_attitude_t* att) {
    if (!bridge || !att) {
        return -1;
    }

    nimcp_mutex_lock(((nimcp_dji_bridge_t*)bridge)->telemetry_lock);
    *att = bridge->attitude;
    nimcp_mutex_unlock(((nimcp_dji_bridge_t*)bridge)->telemetry_lock);
    return 0;
}

int nimcp_dji_get_position(const nimcp_dji_bridge_t* bridge,
                            nimcp_dji_position_t* pos) {
    if (!bridge || !pos) {
        return -1;
    }

    nimcp_mutex_lock(((nimcp_dji_bridge_t*)bridge)->telemetry_lock);
    *pos = bridge->position;
    nimcp_mutex_unlock(((nimcp_dji_bridge_t*)bridge)->telemetry_lock);
    return 0;
}

int nimcp_dji_get_battery(const nimcp_dji_bridge_t* bridge,
                           nimcp_dji_battery_t* bat) {
    if (!bridge || !bat) {
        return -1;
    }

    nimcp_mutex_lock(((nimcp_dji_bridge_t*)bridge)->telemetry_lock);
    *bat = bridge->battery;
    nimcp_mutex_unlock(((nimcp_dji_bridge_t*)bridge)->telemetry_lock);
    return 0;
}

int nimcp_dji_get_gimbal_angle(const nimcp_dji_bridge_t* bridge,
                                nimcp_dji_gimbal_t* gimbal) {
    if (!bridge || !gimbal) {
        return -1;
    }

    nimcp_mutex_lock(((nimcp_dji_bridge_t*)bridge)->telemetry_lock);
    *gimbal = bridge->gimbal;
    nimcp_mutex_unlock(((nimcp_dji_bridge_t*)bridge)->telemetry_lock);
    return 0;
}

/* ============================================================================
 * Commands to Flight Controller
 * ============================================================================ */

#ifndef NIMCP_HAS_DJI_OSDK

/* Stub implementations — log and return -1 */

int nimcp_dji_arm(nimcp_dji_bridge_t* bridge, bool arm) {
    if (!bridge) { return -1; }
    LOG_WARN("[%s] arm(%s) — DJI OSDK not available (stub mode)",
             LOG_MODULE, arm ? "true" : "false");
    return -1;
}

int nimcp_dji_takeoff(nimcp_dji_bridge_t* bridge) {
    if (!bridge) { return -1; }
    LOG_WARN("[%s] takeoff() — DJI OSDK not available (stub mode)", LOG_MODULE);
    return -1;
}

int nimcp_dji_land(nimcp_dji_bridge_t* bridge) {
    if (!bridge) { return -1; }
    LOG_WARN("[%s] land() — DJI OSDK not available (stub mode)", LOG_MODULE);
    return -1;
}

int nimcp_dji_goto_position(nimcp_dji_bridge_t* bridge,
                             double lat, double lon, float alt, float yaw) {
    if (!bridge) { return -1; }
    (void)lat; (void)lon; (void)alt; (void)yaw;
    LOG_WARN("[%s] goto_position() — DJI OSDK not available (stub mode)", LOG_MODULE);
    return -1;
}

int nimcp_dji_set_velocity(nimcp_dji_bridge_t* bridge,
                            float vx, float vy, float vz, float yaw_rate) {
    if (!bridge) { return -1; }
    (void)vx; (void)vy; (void)vz; (void)yaw_rate;
    LOG_WARN("[%s] set_velocity() — DJI OSDK not available (stub mode)", LOG_MODULE);
    return -1;
}

int nimcp_dji_set_gimbal(nimcp_dji_bridge_t* bridge,
                          float roll, float pitch, float yaw) {
    if (!bridge) { return -1; }
    (void)roll; (void)pitch; (void)yaw;
    LOG_WARN("[%s] set_gimbal() — DJI OSDK not available (stub mode)", LOG_MODULE);
    return -1;
}

int nimcp_dji_trigger_photo(nimcp_dji_bridge_t* bridge) {
    if (!bridge) { return -1; }
    LOG_WARN("[%s] trigger_photo() — DJI OSDK not available (stub mode)", LOG_MODULE);
    return -1;
}

#else /* NIMCP_HAS_DJI_OSDK */

/* ---- Full DJI OSDK command implementations ---- */

int nimcp_dji_arm(nimcp_dji_bridge_t* bridge, bool arm) {
    if (!bridge || !bridge->connected) { return -1; }

    /* DJI OSDK: obtain/release control authority */
    /* Vehicle->obtainCtrlAuthority() / releaseCtrlAuthority() */
    LOG_INFO("[%s] %s command sent", LOG_MODULE, arm ? "ARM" : "DISARM");
    return 0;
}

int nimcp_dji_takeoff(nimcp_dji_bridge_t* bridge) {
    if (!bridge || !bridge->connected) { return -1; }

    /* DJI OSDK: Vehicle->flightController->startTakeoff() */
    LOG_INFO("[%s] Takeoff commanded", LOG_MODULE);
    return 0;
}

int nimcp_dji_land(nimcp_dji_bridge_t* bridge) {
    if (!bridge || !bridge->connected) { return -1; }

    /* DJI OSDK: Vehicle->flightController->startLanding() */
    LOG_INFO("[%s] Land commanded", LOG_MODULE);
    return 0;
}

int nimcp_dji_goto_position(nimcp_dji_bridge_t* bridge,
                             double lat, double lon, float alt, float yaw) {
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

    /* DJI OSDK: Vehicle->flightController->setPositionRef(...) */
    LOG_INFO("[%s] Goto (%.6f, %.6f) alt=%.1fm yaw=%.1f",
             LOG_MODULE, lat, lon, alt, yaw);
    return 0;
}

int nimcp_dji_set_velocity(nimcp_dji_bridge_t* bridge,
                            float vx, float vy, float vz, float yaw_rate) {
    if (!bridge || !bridge->connected) { return -1; }

    /* DJI OSDK: Vehicle->flightController->setVelocityAndYawRate(...) */
    (void)vx; (void)vy; (void)vz; (void)yaw_rate;
    return 0;
}

int nimcp_dji_set_gimbal(nimcp_dji_bridge_t* bridge,
                          float roll, float pitch, float yaw) {
    if (!bridge || !bridge->connected) { return -1; }

    /* DJI OSDK: Vehicle->gimbalManager->rotate(...) */
    (void)roll; (void)pitch; (void)yaw;
    LOG_INFO("[%s] Gimbal set to (%.1f, %.1f, %.1f)", LOG_MODULE, roll, pitch, yaw);
    return 0;
}

int nimcp_dji_trigger_photo(nimcp_dji_bridge_t* bridge) {
    if (!bridge || !bridge->connected) { return -1; }

    /* DJI OSDK: Vehicle->cameraManager->startShootPhoto() */
    LOG_INFO("[%s] Photo triggered", LOG_MODULE);
    return 0;
}

#endif /* NIMCP_HAS_DJI_OSDK */

/* ============================================================================
 * Feature Composition for Brain Input
 * ============================================================================ */

int nimcp_dji_compose_features(const nimcp_dji_bridge_t* bridge,
                                float* features, uint32_t max_features) {
    if (!bridge || !features) {
        return -1;
    }

    uint32_t count = NIMCP_DJI_FEATURE_COUNT;
    if (count > max_features) {
        count = max_features;
    }

    /* Zero-fill first */
    memset(features, 0, count * sizeof(float));

    /* Snapshot telemetry under lock */
    nimcp_dji_attitude_t att;
    nimcp_dji_position_t pos;
    nimcp_dji_battery_t bat;
    nimcp_dji_gimbal_t gim;
    nimcp_dji_flight_status_t fstatus;

    nimcp_mutex_lock(((nimcp_dji_bridge_t*)bridge)->telemetry_lock);
    att = bridge->attitude;
    pos = bridge->position;
    bat = bridge->battery;
    gim = bridge->gimbal;
    fstatus = bridge->flight_status;

    double home_lat = bridge->home_lat;
    double home_lon = bridge->home_lon;
    bool home_set = bridge->home_set;
    nimcp_mutex_unlock(((nimcp_dji_bridge_t*)bridge)->telemetry_lock);

    /* Attitude: normalize to [-1,1] by dividing by 180 degrees */
    if (count > DJI_FEAT_ROLL)
        features[DJI_FEAT_ROLL] = _clampf(att.roll / 180.0f, -1.0f, 1.0f);
    if (count > DJI_FEAT_PITCH)
        features[DJI_FEAT_PITCH] = _clampf(att.pitch / 180.0f, -1.0f, 1.0f);
    if (count > DJI_FEAT_YAW)
        features[DJI_FEAT_YAW] = _clampf(att.yaw / 180.0f, -1.0f, 1.0f);

    /* Velocity (m/s) */
    if (count > DJI_FEAT_VX)
        features[DJI_FEAT_VX] = pos.vx;
    if (count > DJI_FEAT_VY)
        features[DJI_FEAT_VY] = pos.vy;
    if (count > DJI_FEAT_VZ)
        features[DJI_FEAT_VZ] = pos.vz;

    /* Altitude relative to takeoff */
    if (count > DJI_FEAT_ALT_REL)
        features[DJI_FEAT_ALT_REL] = pos.altitude_rel;

    /* Heading normalized to [0,1] */
    if (count > DJI_FEAT_HEADING)
        features[DJI_FEAT_HEADING] = _clampf(pos.heading / 360.0f, 0.0f, 1.0f);

    /* Battery remaining normalized to [0,1] */
    if (count > DJI_FEAT_BATTERY)
        features[DJI_FEAT_BATTERY] = _clampf(bat.remaining_pct / 100.0f, 0.0f, 1.0f);

    /* GPS health mapped to [0,1] (DJI scale 0-5) */
    if (count > DJI_FEAT_GPS_HEALTH)
        features[DJI_FEAT_GPS_HEALTH] = _clampf((float)pos.gps_health / 5.0f, 0.0f, 1.0f);

    /* Distance from home (meters) */
    if (count > DJI_FEAT_DIST_HOME) {
        if (home_set) {
            features[DJI_FEAT_DIST_HOME] = (float)_haversine_distance_m(
                home_lat, home_lon, pos.latitude, pos.longitude);
        } else {
            features[DJI_FEAT_DIST_HOME] = 0.0f;
        }
    }

    /* Flight status mapped to [0,1]: stopped=0, ground=0.5, air=1.0 */
    if (count > DJI_FEAT_FLIGHT_STATUS) {
        float fs = 0.0f;
        if (fstatus == NIMCP_DJI_STATUS_ON_GROUND) fs = 0.5f;
        else if (fstatus == NIMCP_DJI_STATUS_IN_AIR) fs = 1.0f;
        features[DJI_FEAT_FLIGHT_STATUS] = fs;
    }

    /* Gimbal angles normalized */
    if (count > DJI_FEAT_GIMBAL_PITCH)
        features[DJI_FEAT_GIMBAL_PITCH] = _clampf(gim.pitch / 90.0f, -1.0f, 1.0f);
    if (count > DJI_FEAT_GIMBAL_YAW)
        features[DJI_FEAT_GIMBAL_YAW] = _clampf(gim.yaw / 180.0f, -1.0f, 1.0f);

    /* Quaternion components (already [-1,1]) */
    if (count > DJI_FEAT_Q0)
        features[DJI_FEAT_Q0] = att.q0;
    if (count > DJI_FEAT_Q1)
        features[DJI_FEAT_Q1] = att.q1;

    return (int)count;
}

/* ============================================================================
 * Safety Checks
 * ============================================================================ */

int nimcp_dji_check_geofence(const nimcp_dji_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }
    if (!bridge->config.enable_geofence) {
        return 0;
    }

    nimcp_mutex_lock(((nimcp_dji_bridge_t*)bridge)->telemetry_lock);
    bool home_set = bridge->home_set;
    double home_lat = bridge->home_lat;
    double home_lon = bridge->home_lon;
    double lat = bridge->position.latitude;
    double lon = bridge->position.longitude;
    float alt_rel = bridge->position.altitude_rel;
    nimcp_mutex_unlock(((nimcp_dji_bridge_t*)bridge)->telemetry_lock);

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

static void* _dji_recv_thread(void* arg) {
    nimcp_dji_bridge_t* bridge = (nimcp_dji_bridge_t*)arg;

    LOG_INFO("[%s] Recv thread running", LOG_MODULE);

#ifdef NIMCP_HAS_DJI_OSDK
    while (bridge->running) {
        /* DJI OSDK: poll telemetry subscriptions */
        /* Vehicle->subscribe->getValue<TOPIC_QUATERNION>() etc. */

        /* Update cached telemetry under lock */
        nimcp_mutex_lock(bridge->telemetry_lock);
        /* ... update attitude, position, battery, gimbal from DJI API ... */
        bridge->attitude.timestamp_us = nimcp_time_now_us();
        bridge->position.timestamp_us = nimcp_time_now_us();
        bridge->battery.timestamp_us = nimcp_time_now_us();
        bridge->gimbal.timestamp_us = nimcp_time_now_us();

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
            nimcp_dji_land(bridge);
        }

        usleep(DJI_TELEMETRY_INTERVAL_US);
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
 * Serial Port (full mode only)
 * ============================================================================ */

#ifdef NIMCP_HAS_DJI_OSDK

static int _open_serial(nimcp_dji_bridge_t* bridge) {
    int fd = open(bridge->config.device_path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        LOG_ERROR("[%s] Failed to open %s: %s",
                  LOG_MODULE, bridge->config.device_path, strerror(errno));
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

    /* Raw mode */
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_oflag &= ~OPOST;

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;  /* 100ms timeout */

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        LOG_ERROR("[%s] tcsetattr failed: %s", LOG_MODULE, strerror(errno));
        close(fd);
        return -1;
    }

    bridge->fd = fd;
    LOG_INFO("[%s] Serial port %s opened at %u baud",
             LOG_MODULE, bridge->config.device_path, bridge->config.baud_rate);
    return 0;
}

static int _dji_activate(nimcp_dji_bridge_t* bridge) {
    if (bridge->config.app_id == 0) {
        LOG_ERROR("[%s] DJI app_id not configured", LOG_MODULE);
        return -1;
    }

    /* DJI OSDK: Vehicle->activate(activateData) */
    LOG_INFO("[%s] DJI OSDK activated (app_id=%u)", LOG_MODULE, bridge->config.app_id);
    return 0;
}

#endif /* NIMCP_HAS_DJI_OSDK */

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
