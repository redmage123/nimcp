/**
 * @file nimcp_msp_bridge.c
 * @brief Betaflight MSP Bridge — NIMCP brain <-> Betaflight/iNav/Cleanflight FC.
 *
 * Two compilation modes:
 *   - NIMCP_HAS_MSP defined: full MSP protocol integration with serial
 *     transport, telemetry polling, and RC channel override
 *   - NIMCP_HAS_MSP not defined: stub mode — all lifecycle functions work,
 *     telemetry returns zeros, commands log warnings and return -1
 *
 * MSP wire format: $M< + direction(1) + size(1) + command(1) + data(N) + checksum(1)
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#define LOG_MODULE "MSP_BRIDGE"

#include "edge/nimcp_msp_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"

#include <string.h>
#include <math.h>
#include <errno.h>

/* Platform headers for serial I/O */
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

#ifdef NIMCP_HAS_MSP
#include <termios.h>
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define MSP_HEADER_SIZE         3       /* $M< or $M> */
#define MSP_MAX_PAYLOAD_SIZE    256
#define MSP_RECV_BUF_SIZE       512
#define MSP_RESPONSE_TIMEOUT_US 100000  /* 100ms timeout for response */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct nimcp_msp_bridge {
    nimcp_msp_config_t config;

    /* Connection */
    int fd;                          /* Serial fd */
    bool connected;

    /* Cached telemetry (updated by poll thread) */
    nimcp_msp_attitude_t attitude;
    nimcp_msp_gps_t gps;
    nimcp_msp_battery_t battery;
    nimcp_msp_rc_channels_t rc_channels;
    float altitude_m;                /* From MSP_ALTITUDE */
    float vario_ms;                  /* From MSP_ALTITUDE */
    nimcp_mutex_t* telemetry_lock;

    /* Poll thread */
    nimcp_thread_t poll_thread;
    volatile bool running;
    volatile bool thread_started;

    /* MSP send/recv mutex (serial is half-duplex) */
    nimcp_mutex_t* serial_lock;
};

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static void* _msp_poll_thread(void* arg);
static float _clampf(float val, float lo, float hi);

#ifdef NIMCP_HAS_MSP
static int _open_serial(nimcp_msp_bridge_t* bridge);
static int _msp_send_request(nimcp_msp_bridge_t* bridge, uint8_t command);
static int _msp_send_command(nimcp_msp_bridge_t* bridge, uint8_t command,
                              const uint8_t* data, uint8_t size);
static int _msp_read_response(nimcp_msp_bridge_t* bridge,
                                uint8_t* command_out,
                                uint8_t* data_out, uint8_t* size_out);
static void _msp_parse_attitude(nimcp_msp_bridge_t* bridge,
                                 const uint8_t* data, uint8_t size);
static void _msp_parse_altitude(nimcp_msp_bridge_t* bridge,
                                 const uint8_t* data, uint8_t size);
static void _msp_parse_raw_gps(nimcp_msp_bridge_t* bridge,
                                const uint8_t* data, uint8_t size);
static void _msp_parse_analog(nimcp_msp_bridge_t* bridge,
                               const uint8_t* data, uint8_t size);
static void _msp_parse_rc(nimcp_msp_bridge_t* bridge,
                            const uint8_t* data, uint8_t size);
#endif

/* ============================================================================
 * Default Config
 * ============================================================================ */

nimcp_msp_config_t nimcp_msp_config_default(void) {
    nimcp_msp_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    strncpy(cfg.serial_port, "/dev/ttyACM0", sizeof(cfg.serial_port) - 1);
    cfg.baud_rate = 115200;
    cfg.poll_rate_hz = 100;

    cfg.min_battery_volts = 3.3f;   /* Per cell — warning threshold */
    cfg.max_angle_degrees = 45.0f;

    return cfg;
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

nimcp_msp_bridge_t* nimcp_msp_bridge_create(const nimcp_msp_config_t* config) {
    nimcp_msp_bridge_t* bridge = (nimcp_msp_bridge_t*)nimcp_calloc(
        1, sizeof(nimcp_msp_bridge_t));
    if (!bridge) {
        LOG_ERROR("[%s] Failed to allocate bridge", LOG_MODULE);
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = nimcp_msp_config_default();
    }

    bridge->fd = -1;
    bridge->connected = false;
    bridge->running = false;
    bridge->thread_started = false;

    bridge->telemetry_lock = nimcp_mutex_create(NULL);
    if (!bridge->telemetry_lock) {
        LOG_ERROR("[%s] Failed to create telemetry mutex", LOG_MODULE);
        nimcp_free(bridge);
        return NULL;
    }

    bridge->serial_lock = nimcp_mutex_create(NULL);
    if (!bridge->serial_lock) {
        LOG_ERROR("[%s] Failed to create serial mutex", LOG_MODULE);
        nimcp_mutex_free(bridge->telemetry_lock);
        nimcp_free(bridge);
        return NULL;
    }

    LOG_INFO("[%s] Bridge created (port=%s, baud=%u, poll=%uHz)",
             LOG_MODULE, bridge->config.serial_port,
             bridge->config.baud_rate, bridge->config.poll_rate_hz);

    return bridge;
}

void nimcp_msp_bridge_destroy(nimcp_msp_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Stop poll thread if running */
    if (bridge->running) {
        nimcp_msp_bridge_stop(bridge);
    }

    /* Close serial if connected */
    if (bridge->connected) {
#ifdef NIMCP_HAS_MSP
        if (bridge->fd >= 0) {
            close(bridge->fd);
            bridge->fd = -1;
        }
#endif
        bridge->connected = false;
    }

    if (bridge->serial_lock) {
        nimcp_mutex_free(bridge->serial_lock);
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

int nimcp_msp_bridge_connect(nimcp_msp_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }
    if (bridge->connected) {
        LOG_WARN("[%s] Already connected", LOG_MODULE);
        return 0;
    }

#ifdef NIMCP_HAS_MSP
    int rc = _open_serial(bridge);
    if (rc < 0) {
        LOG_ERROR("[%s] Failed to open %s",
                  LOG_MODULE, bridge->config.serial_port);
        return -1;
    }

    bridge->connected = true;
    LOG_INFO("[%s] Connected via %s (fd=%d)",
             LOG_MODULE, bridge->config.serial_port, bridge->fd);
    return 0;
#else
    /* Stub mode: pretend to connect */
    bridge->fd = -1;
    bridge->connected = true;
    LOG_INFO("[%s] Connected (stub mode — MSP not available)", LOG_MODULE);
    return 0;
#endif
}

/* ============================================================================
 * Poll Thread Management
 * ============================================================================ */

int nimcp_msp_bridge_start(nimcp_msp_bridge_t* bridge) {
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
        &bridge->poll_thread, _msp_poll_thread, bridge, NULL);
    if (rc != 0) {
        LOG_ERROR("[%s] Failed to create poll thread (rc=%d)", LOG_MODULE, rc);
        bridge->running = false;
        return -1;
    }

    bridge->thread_started = true;
    LOG_INFO("[%s] Poll thread started", LOG_MODULE);
    return 0;
}

int nimcp_msp_bridge_stop(nimcp_msp_bridge_t* bridge) {
    if (!bridge) {
        return -1;
    }
    if (!bridge->running) {
        return 0;
    }

    bridge->running = false;

    if (bridge->thread_started) {
        nimcp_thread_join(bridge->poll_thread, NULL);
        bridge->thread_started = false;
    }

    LOG_INFO("[%s] Poll thread stopped", LOG_MODULE);
    return 0;
}

/* ============================================================================
 * Telemetry Getters (thread-safe)
 * ============================================================================ */

int nimcp_msp_get_attitude(const nimcp_msp_bridge_t* bridge,
                            nimcp_msp_attitude_t* att) {
    if (!bridge || !att) {
        return -1;
    }

    nimcp_mutex_lock(((nimcp_msp_bridge_t*)bridge)->telemetry_lock);
    *att = bridge->attitude;
    nimcp_mutex_unlock(((nimcp_msp_bridge_t*)bridge)->telemetry_lock);
    return 0;
}

int nimcp_msp_get_gps(const nimcp_msp_bridge_t* bridge,
                       nimcp_msp_gps_t* gps) {
    if (!bridge || !gps) {
        return -1;
    }

    nimcp_mutex_lock(((nimcp_msp_bridge_t*)bridge)->telemetry_lock);
    *gps = bridge->gps;
    nimcp_mutex_unlock(((nimcp_msp_bridge_t*)bridge)->telemetry_lock);
    return 0;
}

int nimcp_msp_get_battery(const nimcp_msp_bridge_t* bridge,
                           nimcp_msp_battery_t* bat) {
    if (!bridge || !bat) {
        return -1;
    }

    nimcp_mutex_lock(((nimcp_msp_bridge_t*)bridge)->telemetry_lock);
    *bat = bridge->battery;
    nimcp_mutex_unlock(((nimcp_msp_bridge_t*)bridge)->telemetry_lock);
    return 0;
}

int nimcp_msp_get_rc_channels(const nimcp_msp_bridge_t* bridge,
                               nimcp_msp_rc_channels_t* rc) {
    if (!bridge || !rc) {
        return -1;
    }

    nimcp_mutex_lock(((nimcp_msp_bridge_t*)bridge)->telemetry_lock);
    *rc = bridge->rc_channels;
    nimcp_mutex_unlock(((nimcp_msp_bridge_t*)bridge)->telemetry_lock);
    return 0;
}

/* ============================================================================
 * RC Override (brain control)
 * ============================================================================ */

#ifndef NIMCP_HAS_MSP

/* Stub implementation — log and return -1 */

int nimcp_msp_set_rc_override(nimcp_msp_bridge_t* bridge,
                               uint16_t throttle, uint16_t roll,
                               uint16_t pitch, uint16_t yaw) {
    if (!bridge) { return -1; }
    (void)throttle; (void)roll; (void)pitch; (void)yaw;
    LOG_WARN("[%s] set_rc_override() — MSP not available (stub mode)", LOG_MODULE);
    return -1;
}

#else /* NIMCP_HAS_MSP */

int nimcp_msp_set_rc_override(nimcp_msp_bridge_t* bridge,
                               uint16_t throttle, uint16_t roll,
                               uint16_t pitch, uint16_t yaw) {
    if (!bridge || !bridge->connected) { return -1; }

    /* Safety: clamp all channels to valid PWM range */
    if (throttle < 1000) throttle = 1000;
    if (throttle > 2000) throttle = 2000;
    if (roll < 1000) roll = 1000;
    if (roll > 2000) roll = 2000;
    if (pitch < 1000) pitch = 1000;
    if (pitch > 2000) pitch = 2000;
    if (yaw < 1000) yaw = 1000;
    if (yaw > 2000) yaw = 2000;

    /* Safety: check max angle from attitude */
    nimcp_mutex_lock(bridge->telemetry_lock);
    float cur_roll = bridge->attitude.roll;
    float cur_pitch = bridge->attitude.pitch;
    nimcp_mutex_unlock(bridge->telemetry_lock);

    if (fabsf(cur_roll) > bridge->config.max_angle_degrees ||
        fabsf(cur_pitch) > bridge->config.max_angle_degrees) {
        LOG_WARN("[%s] Angle limit exceeded (roll=%.1f, pitch=%.1f, limit=%.1f) "
                 "— centering sticks",
                 LOG_MODULE, cur_roll, cur_pitch, bridge->config.max_angle_degrees);
        roll = 1500;
        pitch = 1500;
    }

    /* MSP_SET_RAW_RC: 16 channels * 2 bytes = 32 bytes max
     * We only set the first 4 channels (throttle, roll, pitch, yaw) */
    uint8_t data[8];
    /* Roll (channel 0) */
    data[0] = (uint8_t)(roll & 0xFF);
    data[1] = (uint8_t)((roll >> 8) & 0xFF);
    /* Pitch (channel 1) */
    data[2] = (uint8_t)(pitch & 0xFF);
    data[3] = (uint8_t)((pitch >> 8) & 0xFF);
    /* Throttle (channel 2) */
    data[4] = (uint8_t)(throttle & 0xFF);
    data[5] = (uint8_t)((throttle >> 8) & 0xFF);
    /* Yaw (channel 3) */
    data[6] = (uint8_t)(yaw & 0xFF);
    data[7] = (uint8_t)((yaw >> 8) & 0xFF);

    nimcp_mutex_lock(bridge->serial_lock);
    int rc = _msp_send_command(bridge, MSP_SET_RAW_RC, data, sizeof(data));
    nimcp_mutex_unlock(bridge->serial_lock);

    return rc;
}

#endif /* NIMCP_HAS_MSP */

/* ============================================================================
 * Feature Composition for Brain Input
 * ============================================================================ */

int nimcp_msp_compose_features(const nimcp_msp_bridge_t* bridge,
                                float* features, uint32_t max_features) {
    if (!bridge || !features) {
        return -1;
    }

    uint32_t count = NIMCP_MSP_FEATURE_COUNT;
    if (count > max_features) {
        count = max_features;
    }

    /* Zero-fill first */
    memset(features, 0, count * sizeof(float));

    /* Snapshot telemetry under lock */
    nimcp_msp_attitude_t att;
    nimcp_msp_battery_t bat;
    nimcp_msp_rc_channels_t rc;
    float altitude_m;
    float vario_ms;
    float speed;

    nimcp_mutex_lock(((nimcp_msp_bridge_t*)bridge)->telemetry_lock);
    att = bridge->attitude;
    bat = bridge->battery;
    rc = bridge->rc_channels;
    altitude_m = bridge->altitude_m;
    vario_ms = bridge->vario_ms;
    speed = bridge->gps.speed;
    nimcp_mutex_unlock(((nimcp_msp_bridge_t*)bridge)->telemetry_lock);

    /* Attitude: normalize to [-1,1] by dividing by 180 degrees */
    if (count > MSP_FEAT_ROLL)
        features[MSP_FEAT_ROLL] = _clampf(att.roll / 180.0f, -1.0f, 1.0f);
    if (count > MSP_FEAT_PITCH)
        features[MSP_FEAT_PITCH] = _clampf(att.pitch / 180.0f, -1.0f, 1.0f);
    if (count > MSP_FEAT_YAW)
        features[MSP_FEAT_YAW] = _clampf(att.yaw / 360.0f, 0.0f, 1.0f);

    /* Altitude (meters) */
    if (count > MSP_FEAT_ALTITUDE)
        features[MSP_FEAT_ALTITUDE] = altitude_m;

    /* Vario (vertical speed m/s) */
    if (count > MSP_FEAT_VARIO)
        features[MSP_FEAT_VARIO] = vario_ms;

    /* Ground speed m/s */
    if (count > MSP_FEAT_SPEED)
        features[MSP_FEAT_SPEED] = speed;

    /* Battery voltage normalized to [0,1] by 25.2V (6S LiPo full) */
    if (count > MSP_FEAT_BATTERY_V)
        features[MSP_FEAT_BATTERY_V] = _clampf(bat.voltage / 25.2f, 0.0f, 1.0f);

    /* RSSI normalized to [0,1] (0-1023 range) */
    if (count > MSP_FEAT_RSSI)
        features[MSP_FEAT_RSSI] = _clampf((float)bat.rssi / 1023.0f, 0.0f, 1.0f);

    /* RC sticks: normalize 1000-2000 → appropriate range */
    if (count > MSP_FEAT_THROTTLE && rc.channel_count > 2)
        features[MSP_FEAT_THROTTLE] = _clampf(
            (float)(rc.channels[2] - 1000) / 1000.0f, 0.0f, 1.0f);
    if (count > MSP_FEAT_ROLL_STICK && rc.channel_count > 0)
        features[MSP_FEAT_ROLL_STICK] = _clampf(
            (float)(rc.channels[0] - 1500) / 500.0f, -1.0f, 1.0f);
    if (count > MSP_FEAT_PITCH_STICK && rc.channel_count > 1)
        features[MSP_FEAT_PITCH_STICK] = _clampf(
            (float)(rc.channels[1] - 1500) / 500.0f, -1.0f, 1.0f);
    if (count > MSP_FEAT_YAW_STICK && rc.channel_count > 3)
        features[MSP_FEAT_YAW_STICK] = _clampf(
            (float)(rc.channels[3] - 1500) / 500.0f, -1.0f, 1.0f);

    return (int)count;
}

/* ============================================================================
 * Poll Thread
 * ============================================================================ */

static void* _msp_poll_thread(void* arg) {
    nimcp_msp_bridge_t* bridge = (nimcp_msp_bridge_t*)arg;

    LOG_INFO("[%s] Poll thread running", LOG_MODULE);

    uint32_t poll_interval_us = 1000000 / bridge->config.poll_rate_hz;
    if (poll_interval_us < 1000) {
        poll_interval_us = 1000;  /* Cap at 1000 Hz */
    }

#ifdef NIMCP_HAS_MSP
    uint8_t resp_data[MSP_MAX_PAYLOAD_SIZE];
    uint8_t resp_cmd;
    uint8_t resp_size;

    /* Cycle through telemetry requests */
    static const uint8_t poll_commands[] = {
        MSP_ATTITUDE, MSP_ALTITUDE, MSP_RAW_GPS, MSP_ANALOG, MSP_RC
    };
    uint32_t cmd_idx = 0;
    uint32_t num_cmds = sizeof(poll_commands) / sizeof(poll_commands[0]);

    while (bridge->running) {
        /* Send request for next telemetry item */
        nimcp_mutex_lock(bridge->serial_lock);

        int rc = _msp_send_request(bridge, poll_commands[cmd_idx]);
        if (rc == 0) {
            rc = _msp_read_response(bridge, &resp_cmd, resp_data, &resp_size);
            if (rc == 0) {
                switch (resp_cmd) {
                    case MSP_ATTITUDE:
                        _msp_parse_attitude(bridge, resp_data, resp_size);
                        break;
                    case MSP_ALTITUDE:
                        _msp_parse_altitude(bridge, resp_data, resp_size);
                        break;
                    case MSP_RAW_GPS:
                        _msp_parse_raw_gps(bridge, resp_data, resp_size);
                        break;
                    case MSP_ANALOG:
                        _msp_parse_analog(bridge, resp_data, resp_size);
                        break;
                    case MSP_RC:
                        _msp_parse_rc(bridge, resp_data, resp_size);
                        break;
                    default:
                        break;
                }
            }
        }

        nimcp_mutex_unlock(bridge->serial_lock);

        cmd_idx = (cmd_idx + 1) % num_cmds;
        usleep(poll_interval_us / num_cmds);
    }
#else
    /* Stub mode: just idle until stopped */
    while (bridge->running) {
        usleep(100000);  /* 100ms */
    }
#endif

    LOG_INFO("[%s] Poll thread exiting", LOG_MODULE);
    return NULL;
}

/* ============================================================================
 * MSP Protocol Helpers (full mode only)
 * ============================================================================ */

#ifdef NIMCP_HAS_MSP

static int _open_serial(nimcp_msp_bridge_t* bridge) {
    int fd = open(bridge->config.serial_port, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        LOG_ERROR("[%s] Failed to open %s: %s",
                  LOG_MODULE, bridge->config.serial_port, strerror(errno));
        return -1;
    }

    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(fd, &tty) != 0) {
        LOG_ERROR("[%s] tcgetattr failed: %s", LOG_MODULE, strerror(errno));
        close(fd);
        return -1;
    }

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
            LOG_WARN("[%s] Unsupported baud %u, using 115200",
                     LOG_MODULE, bridge->config.baud_rate);
            baud = B115200;
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
             LOG_MODULE, bridge->config.serial_port, bridge->config.baud_rate);
    return 0;
}

/**
 * @brief Send an MSP request (no payload).
 * Wire format: $M< + size(0) + command(1) + checksum(1)
 */
static int _msp_send_request(nimcp_msp_bridge_t* bridge, uint8_t command) {
    uint8_t buf[6];
    buf[0] = '$';
    buf[1] = 'M';
    buf[2] = '<';   /* Direction: to FC */
    buf[3] = 0;     /* Payload size = 0 */
    buf[4] = command;
    buf[5] = buf[3] ^ buf[4];  /* XOR checksum */

    ssize_t written = write(bridge->fd, buf, sizeof(buf));
    if (written != sizeof(buf)) {
        LOG_ERROR("[%s] Failed to send MSP request %u: %s",
                  LOG_MODULE, command, strerror(errno));
        return -1;
    }
    return 0;
}

/**
 * @brief Send an MSP command with payload.
 * Wire format: $M< + size(N) + command(1) + data(N) + checksum(1)
 */
static int _msp_send_command(nimcp_msp_bridge_t* bridge, uint8_t command,
                              const uint8_t* data, uint8_t size) {
    uint8_t buf[MSP_HEADER_SIZE + 2 + MSP_MAX_PAYLOAD_SIZE + 1];
    buf[0] = '$';
    buf[1] = 'M';
    buf[2] = '<';
    buf[3] = size;
    buf[4] = command;

    uint8_t checksum = buf[3] ^ buf[4];
    for (uint8_t i = 0; i < size; i++) {
        buf[5 + i] = data[i];
        checksum ^= data[i];
    }
    buf[5 + size] = checksum;

    uint32_t total = 5 + size + 1;
    ssize_t written = write(bridge->fd, buf, total);
    if (written != (ssize_t)total) {
        LOG_ERROR("[%s] Failed to send MSP command %u: %s",
                  LOG_MODULE, command, strerror(errno));
        return -1;
    }
    return 0;
}

/**
 * @brief Read an MSP response from the FC.
 * Wire format: $M> + size(1) + command(1) + data(N) + checksum(1)
 */
static int _msp_read_response(nimcp_msp_bridge_t* bridge,
                                uint8_t* command_out,
                                uint8_t* data_out, uint8_t* size_out) {
    uint8_t buf[MSP_RECV_BUF_SIZE];
    uint64_t start = nimcp_time_now_us();
    int pos = 0;
    uint32_t resets = 0;

    /* Read header: $M> — limit resets to prevent infinite retries */
    while (pos < 5 && resets < 10 && (nimcp_time_now_us() - start) < MSP_RESPONSE_TIMEOUT_US) {
        ssize_t n = read(bridge->fd, buf + pos, 1);
        if (n == 1) {
            if (pos == 0 && buf[0] != '$') continue;
            if (pos == 1 && buf[1] != 'M') { pos = 0; resets++; continue; }
            if (pos == 2 && buf[2] != '>') { pos = 0; resets++; continue; }
            pos++;
        } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            return -1;
        } else {
            usleep(100);
        }
    }

    if (pos < 5) {
        return -1;  /* Timeout */
    }

    uint8_t payload_size = buf[3];
    uint8_t command = buf[4];

    /* Read payload + checksum */
    int remaining = payload_size + 1;
    int read_pos = 0;
    uint8_t payload[MSP_MAX_PAYLOAD_SIZE + 1];

    while (read_pos < remaining &&
           (nimcp_time_now_us() - start) < MSP_RESPONSE_TIMEOUT_US) {
        ssize_t n = read(bridge->fd, payload + read_pos, remaining - read_pos);
        if (n > 0) {
            read_pos += (int)n;
        } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            return -1;
        } else {
            usleep(100);
        }
    }

    if (read_pos < remaining) {
        return -1;  /* Timeout */
    }

    /* Verify checksum */
    uint8_t checksum = buf[3] ^ buf[4];
    for (int i = 0; i < payload_size; i++) {
        checksum ^= payload[i];
    }
    if (checksum != payload[payload_size]) {
        LOG_WARN("[%s] MSP checksum error for command %u", LOG_MODULE, command);
        return -1;
    }

    *command_out = command;
    *size_out = payload_size;
    if (payload_size > 0) {
        memcpy(data_out, payload, payload_size);
    }

    return 0;
}

static void _msp_parse_attitude(nimcp_msp_bridge_t* bridge,
                                 const uint8_t* data, uint8_t size) {
    if (size < 6) return;

    int16_t roll_10  = (int16_t)(data[0] | (data[1] << 8));
    int16_t pitch_10 = (int16_t)(data[2] | (data[3] << 8));
    int16_t yaw      = (int16_t)(data[4] | (data[5] << 8));

    nimcp_mutex_lock(bridge->telemetry_lock);
    bridge->attitude.roll = (float)roll_10 / 10.0f;
    bridge->attitude.pitch = (float)pitch_10 / 10.0f;
    bridge->attitude.yaw = (float)yaw;
    bridge->attitude.timestamp_us = nimcp_time_now_us();
    nimcp_mutex_unlock(bridge->telemetry_lock);
}

static void _msp_parse_altitude(nimcp_msp_bridge_t* bridge,
                                 const uint8_t* data, uint8_t size) {
    if (size < 6) return;

    int32_t alt_cm = (int32_t)(data[0] | (data[1] << 8) |
                                (data[2] << 16) | (data[3] << 24));
    int16_t vario_cms = (int16_t)(data[4] | (data[5] << 8));

    nimcp_mutex_lock(bridge->telemetry_lock);
    bridge->altitude_m = (float)alt_cm / 100.0f;
    bridge->vario_ms = (float)vario_cms / 100.0f;
    nimcp_mutex_unlock(bridge->telemetry_lock);
}

static void _msp_parse_raw_gps(nimcp_msp_bridge_t* bridge,
                                const uint8_t* data, uint8_t size) {
    if (size < 16) return;

    nimcp_mutex_lock(bridge->telemetry_lock);
    bridge->gps.fix_type = data[0];
    bridge->gps.satellites = data[1];
    bridge->gps.latitude = (double)(int32_t)(data[2] | (data[3] << 8) |
                                              (data[4] << 16) | (data[5] << 24)) / 1e7;
    bridge->gps.longitude = (double)(int32_t)(data[6] | (data[7] << 8) |
                                               (data[8] << 16) | (data[9] << 24)) / 1e7;
    bridge->gps.altitude = (float)(int16_t)(data[10] | (data[11] << 8));  /* meters */
    bridge->gps.speed = (float)(uint16_t)(data[12] | (data[13] << 8)) / 100.0f;  /* cm/s → m/s */
    bridge->gps.ground_course = (float)(uint16_t)(data[14] | (data[15] << 8)) / 10.0f;  /* degrees*10 */
    bridge->gps.timestamp_us = nimcp_time_now_us();
    nimcp_mutex_unlock(bridge->telemetry_lock);
}

static void _msp_parse_analog(nimcp_msp_bridge_t* bridge,
                               const uint8_t* data, uint8_t size) {
    if (size < 7) return;

    nimcp_mutex_lock(bridge->telemetry_lock);
    bridge->battery.voltage = (float)data[0] / 10.0f;  /* decivolts → volts */
    /* data[1..2] = power_meter (unused) */
    bridge->battery.rssi = (uint16_t)(data[3] | (data[4] << 8));
    bridge->battery.amps = (float)(int16_t)(data[5] | (data[6] << 8)) / 100.0f;  /* cA → A */
    bridge->battery.timestamp_us = nimcp_time_now_us();
    nimcp_mutex_unlock(bridge->telemetry_lock);
}

static void _msp_parse_rc(nimcp_msp_bridge_t* bridge,
                            const uint8_t* data, uint8_t size) {
    uint8_t num_channels = size / 2;
    if (num_channels > 16) num_channels = 16;

    nimcp_mutex_lock(bridge->telemetry_lock);
    bridge->rc_channels.channel_count = num_channels;
    for (uint8_t i = 0; i < num_channels; i++) {
        bridge->rc_channels.channels[i] = (uint16_t)(data[i * 2] |
                                                      (data[i * 2 + 1] << 8));
    }
    bridge->rc_channels.timestamp_us = nimcp_time_now_us();
    nimcp_mutex_unlock(bridge->telemetry_lock);
}

#endif /* NIMCP_HAS_MSP */

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static float _clampf(float val, float lo, float hi) {
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}
