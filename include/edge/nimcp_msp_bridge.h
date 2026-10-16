/**
 * @file nimcp_msp_bridge.h
 * @brief Betaflight MSP Bridge — NIMCP brain <-> Betaflight/iNav/Cleanflight FC.
 *
 * Two compilation modes:
 *   - NIMCP_HAS_MSP defined: full MSP integration (serial protocol, telemetry
 *     polling, RC channel override for brain-controlled flight)
 *   - NIMCP_HAS_MSP not defined: stub mode with zeroed telemetry and
 *     logged-but-rejected commands. Allows testing the integration pipeline
 *     without hardware.
 *
 * MSP wire format: $M< + direction(1) + size(1) + command(1) + data(N) + checksum(1)
 *
 * Typical deployment: companion computer connected to Betaflight FC via USB
 * serial (ttyACM0) for FPV racing or autonomous flight.
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#ifndef NIMCP_MSP_BRIDGE_H
#define NIMCP_MSP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * MSP Command IDs
 * ============================================================================ */

#define MSP_RC              105     /* 16 RC channels (int16 each) */
#define MSP_RAW_GPS         106     /* fix, numSat, lat, lon, alt, speed, course */
#define MSP_ATTITUDE        108     /* roll, pitch, yaw (int16, degrees*10) */
#define MSP_ALTITUDE        109     /* altitude (int32 cm), vario (int16 cm/s) */
#define MSP_ANALOG          110     /* vbat, power_meter, rssi, amps */
#define MSP_SET_RAW_RC      200     /* Override RC channels */

/* ============================================================================
 * MSP Bridge Configuration
 * ============================================================================ */

typedef struct {
    char serial_port[256];          /* e.g., "/dev/ttyACM0" */
    uint32_t baud_rate;             /* Serial baud (default 115200) */
    uint32_t poll_rate_hz;          /* Telemetry poll rate (default 100) */

    /* Safety */
    float min_battery_volts;        /* Low battery threshold (default 3.3V per cell) */
    float max_angle_degrees;        /* Max tilt angle for override (default 45) */
} nimcp_msp_config_t;

/* ============================================================================
 * Opaque Handle
 * ============================================================================ */

typedef struct nimcp_msp_bridge nimcp_msp_bridge_t;

/* ============================================================================
 * Telemetry Data Types
 * ============================================================================ */

/** Attitude data from Betaflight FC */
typedef struct {
    float roll;                     /* Degrees */
    float pitch;
    float yaw;                      /* Heading, degrees */
    uint64_t timestamp_us;
} nimcp_msp_attitude_t;

/** GPS data from Betaflight FC */
typedef struct {
    uint8_t fix_type;               /* 0=no fix, 1=fix, 2=DGPS */
    uint8_t satellites;
    double latitude;                /* Degrees */
    double longitude;
    float altitude;                 /* Meters */
    float speed;                    /* m/s */
    float ground_course;            /* Degrees */
    uint64_t timestamp_us;
} nimcp_msp_gps_t;

/** Battery data from Betaflight FC */
typedef struct {
    float voltage;                  /* Volts */
    float amps;                     /* Amps */
    uint16_t rssi;                  /* Signal strength 0-1023 */
    uint64_t timestamp_us;
} nimcp_msp_battery_t;

/** RC channel data (16 channels, 1000-2000 PWM range) */
typedef struct {
    uint16_t channels[16];
    uint8_t channel_count;
    uint64_t timestamp_us;
} nimcp_msp_rc_channels_t;

/* ============================================================================
 * Feature vector layout for brain input
 * ============================================================================ */

#define NIMCP_MSP_FEATURE_COUNT  12

/* Feature indices */
#define MSP_FEAT_ROLL               0   /* normalized [-1,1] by 180 */
#define MSP_FEAT_PITCH              1
#define MSP_FEAT_YAW                2   /* [0,1] normalized by 360 */
#define MSP_FEAT_ALTITUDE           3   /* meters */
#define MSP_FEAT_VARIO              4   /* m/s vertical speed */
#define MSP_FEAT_SPEED              5   /* m/s ground speed */
#define MSP_FEAT_BATTERY_V          6   /* [0,1] normalized by 25.2V (6S) */
#define MSP_FEAT_RSSI               7   /* [0,1] */
#define MSP_FEAT_THROTTLE           8   /* [0,1] from RC channel 1000-2000 */
#define MSP_FEAT_ROLL_STICK         9   /* [-1,1] from RC channel */
#define MSP_FEAT_PITCH_STICK        10  /* [-1,1] from RC channel */
#define MSP_FEAT_YAW_STICK          11  /* [-1,1] from RC channel */

/* ============================================================================
 * API — Lifecycle
 * ============================================================================ */

/**
 * @brief Create an MSP bridge with the given configuration.
 * @param config Pointer to configuration. NULL uses defaults.
 * @return Bridge handle, or NULL on allocation failure.
 */
nimcp_msp_bridge_t* nimcp_msp_bridge_create(const nimcp_msp_config_t* config);

/**
 * @brief Destroy an MSP bridge and free all resources. NULL-safe.
 */
void nimcp_msp_bridge_destroy(nimcp_msp_bridge_t* bridge);

/**
 * @brief Open the serial connection to the Betaflight FC.
 * @return 0 on success, -1 on failure.
 */
int nimcp_msp_bridge_connect(nimcp_msp_bridge_t* bridge);

/**
 * @brief Start the telemetry polling thread.
 * @return 0 on success, -1 on failure.
 */
int nimcp_msp_bridge_start(nimcp_msp_bridge_t* bridge);

/**
 * @brief Stop the telemetry polling thread.
 * @return 0 on success, -1 on failure.
 */
int nimcp_msp_bridge_stop(nimcp_msp_bridge_t* bridge);

/* ============================================================================
 * API — Telemetry Getters (thread-safe, return latest cached values)
 * ============================================================================ */

int nimcp_msp_get_attitude(const nimcp_msp_bridge_t* bridge, nimcp_msp_attitude_t* att);
int nimcp_msp_get_gps(const nimcp_msp_bridge_t* bridge, nimcp_msp_gps_t* gps);
int nimcp_msp_get_battery(const nimcp_msp_bridge_t* bridge, nimcp_msp_battery_t* bat);
int nimcp_msp_get_rc_channels(const nimcp_msp_bridge_t* bridge, nimcp_msp_rc_channels_t* rc);

/* ============================================================================
 * API — RC Override (brain control)
 * ============================================================================ */

/**
 * @brief Override the 4 primary RC channels for brain-controlled flight.
 *
 * All values in PWM range 1000-2000 (1500 = center/neutral).
 *
 * @param bridge   Bridge handle.
 * @param throttle Throttle (1000=min, 2000=max).
 * @param roll     Roll (1000=left, 2000=right).
 * @param pitch    Pitch (1000=nose down, 2000=nose up).
 * @param yaw      Yaw (1000=left, 2000=right).
 * @return 0 on success, -1 on failure.
 */
int nimcp_msp_set_rc_override(nimcp_msp_bridge_t* bridge,
                               uint16_t throttle, uint16_t roll,
                               uint16_t pitch, uint16_t yaw);

/* ============================================================================
 * API — Brain Integration
 * ============================================================================ */

/**
 * @brief Compose a brain-input feature vector from current MSP telemetry.
 *
 * Writes up to NIMCP_MSP_FEATURE_COUNT (12) features.
 * See MSP_FEAT_* defines for layout.
 *
 * @param bridge       Bridge handle.
 * @param features     Output float array (caller-allocated).
 * @param max_features Maximum number of floats to write.
 * @return Number of features written, or -1 on error.
 */
int nimcp_msp_compose_features(const nimcp_msp_bridge_t* bridge,
                                float* features, uint32_t max_features);

/**
 * @brief Return a default MSP bridge configuration.
 */
nimcp_msp_config_t nimcp_msp_config_default(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MSP_BRIDGE_H */
