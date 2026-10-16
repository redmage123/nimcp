/**
 * @file nimcp_sensor.c
 * @brief Sensor Abstraction Layer — hub for heterogeneous sensor aggregation.
 *
 * Provides thread-safe sensor registration, reading submission with double
 * buffering, async callbacks, and feature vector composition for brain input.
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#include "edge/nimcp_sensor.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>
#include <math.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define SENSOR_HUB_LOG_PREFIX  "[edge/sensor] "

/** Earth gravity for IMU normalization (m/s^2) */
#define GRAVITY_MS2  9.81f

/** Approximate meters per degree of latitude at the equator */
#define METERS_PER_DEG_LAT  111320.0f

/** Degrees to radians */
#define DEG_TO_RAD  0.01745329251f

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * Double-buffered sensor reading slot.
 * Writers fill the back buffer and swap the active index.
 * Readers always read from the front buffer (active index).
 */
typedef struct {
    nimcp_sensor_descriptor_t descriptor;
    bool registered;

    /* Double-buffered readings */
    nimcp_sensor_reading_t readings[2];
    float* data_buffers[2];        /* Owned data storage for each buffer */
    uint32_t data_capacity;        /* Allocated floats per buffer */
    volatile int active_idx;       /* 0 or 1: which buffer is current */
    bool has_reading;              /* At least one reading received */

    /* Async callback */
    nimcp_sensor_callback_t callback;
    void* user_data;
} sensor_slot_t;

/**
 * Sensor hub — manages an array of sensor slots.
 */
struct nimcp_sensor_hub {
    sensor_slot_t* slots;
    uint32_t max_sensors;
    uint32_t sensor_count;         /* Number of registered sensors */
    nimcp_mutex_t* mutex;          /* Protects registration and submit */

    /* GPS reference point for local coordinate conversion */
    double gps_ref_lat;
    double gps_ref_lon;
    double gps_ref_alt;
    bool gps_ref_set;
};

/* ============================================================================
 * Static Helpers
 * ============================================================================ */

/**
 * Copy reading data into a slot's back buffer.
 */
static int slot_copy_reading(sensor_slot_t* slot,
                              const nimcp_sensor_reading_t* reading)
{
    int back = 1 - slot->active_idx;

    /* Ensure data buffer is large enough */
    if (reading->data_count > slot->data_capacity) {
        float* new_buf = (float*)nimcp_calloc(reading->data_count, sizeof(float));
        if (!new_buf) {
            return -1;
        }
        nimcp_free(slot->data_buffers[back]);
        slot->data_buffers[back] = new_buf;
        /* Also grow the other buffer to match so future swaps are safe */
        float* other_buf = (float*)nimcp_calloc(reading->data_count, sizeof(float));
        if (!other_buf) {
            LOG_WARN(SENSOR_HUB_LOG_PREFIX "Buffer allocation failed, "
                     "preserving existing active buffer data");
            /* Don't free the old buffer — keep working with what we have */
        } else {
            /* Preserve existing data in active buffer */
            nimcp_sensor_reading_t* active = &slot->readings[slot->active_idx];
            if (slot->data_buffers[slot->active_idx] && active->data_count > 0) {
                memcpy(other_buf, slot->data_buffers[slot->active_idx],
                       active->data_count * sizeof(float));
            }
            nimcp_free(slot->data_buffers[slot->active_idx]);
            slot->data_buffers[slot->active_idx] = other_buf;
            active->data = other_buf;
        }
        slot->data_capacity = reading->data_count;
    }

    /* Copy reading metadata */
    nimcp_sensor_reading_t* dest = &slot->readings[back];
    dest->type = reading->type;
    dest->format = reading->format;
    dest->sensor_id = reading->sensor_id;
    dest->timestamp_us = reading->timestamp_us;
    dest->data_count = reading->data_count;
    dest->confidence = reading->confidence;
    dest->valid = reading->valid;

    /* Copy data */
    if (reading->data && reading->data_count > 0) {
        memcpy(slot->data_buffers[back], reading->data,
               reading->data_count * sizeof(float));
    }
    dest->data = slot->data_buffers[back];

    /* Swap active index atomically for reader safety */
    __atomic_store_n(&slot->active_idx, back, __ATOMIC_RELEASE);
    slot->has_reading = true;

    return 0;
}

/* ============================================================================
 * Sensor Type Names
 * ============================================================================ */

static const char* s_sensor_type_names[NIMCP_SENSOR_TYPE_COUNT] = {
    "LIDAR",
    "DEPTH_CAMERA",
    "RGB_CAMERA",
    "IMU",
    "ENCODER",
    "FORCE_TORQUE",
    "BUMPER",
    "GPS",
    "ULTRASONIC",
    "TEMPERATURE",
    "BAROMETER",
    "CUSTOM"
};

/* ============================================================================
 * Sensor Hub Lifecycle
 * ============================================================================ */

nimcp_sensor_hub_t* nimcp_sensor_hub_create(uint32_t max_sensors)
{
    if (max_sensors == 0) {
        LOG_ERROR(SENSOR_HUB_LOG_PREFIX "max_sensors must be > 0");
        return NULL;
    }

    nimcp_sensor_hub_t* hub =
        (nimcp_sensor_hub_t*)nimcp_calloc(1, sizeof(nimcp_sensor_hub_t));
    if (!hub) {
        LOG_ERROR(SENSOR_HUB_LOG_PREFIX "Failed to allocate sensor hub");
        return NULL;
    }

    hub->slots = (sensor_slot_t*)nimcp_calloc(max_sensors, sizeof(sensor_slot_t));
    if (!hub->slots) {
        LOG_ERROR(SENSOR_HUB_LOG_PREFIX "Failed to allocate %u sensor slots",
                  max_sensors);
        nimcp_free(hub);
        return NULL;
    }

    hub->mutex = nimcp_mutex_create(NULL);
    if (!hub->mutex) {
        LOG_ERROR(SENSOR_HUB_LOG_PREFIX "Failed to create mutex");
        nimcp_free(hub->slots);
        nimcp_free(hub);
        return NULL;
    }

    hub->max_sensors = max_sensors;
    hub->sensor_count = 0;
    hub->gps_ref_set = false;

    LOG_INFO(SENSOR_HUB_LOG_PREFIX "Created sensor hub (capacity=%u)", max_sensors);
    return hub;
}

void nimcp_sensor_hub_destroy(nimcp_sensor_hub_t* hub)
{
    if (!hub) {
        return;
    }

    /* Free all slot data buffers */
    for (uint32_t i = 0; i < hub->max_sensors; i++) {
        sensor_slot_t* slot = &hub->slots[i];
        if (slot->registered) {
            nimcp_free(slot->data_buffers[0]);
            nimcp_free(slot->data_buffers[1]);
        }
    }

    nimcp_free(hub->slots);
    nimcp_mutex_free(hub->mutex);
    nimcp_free(hub);

    LOG_INFO(SENSOR_HUB_LOG_PREFIX "Destroyed sensor hub");
}

/* ============================================================================
 * Sensor Registration
 * ============================================================================ */

int nimcp_sensor_register(nimcp_sensor_hub_t* hub,
                           const nimcp_sensor_descriptor_t* descriptor)
{
    if (!hub || !descriptor) {
        return -1;
    }

    nimcp_mutex_lock(hub->mutex);

    if (hub->sensor_count >= hub->max_sensors) {
        nimcp_mutex_unlock(hub->mutex);
        LOG_ERROR(SENSOR_HUB_LOG_PREFIX "Hub full (%u/%u sensors)",
                  hub->sensor_count, hub->max_sensors);
        return -1;
    }

    /* Find first empty slot */
    uint32_t slot_id = hub->max_sensors;
    for (uint32_t i = 0; i < hub->max_sensors; i++) {
        if (!hub->slots[i].registered) {
            slot_id = i;
            break;
        }
    }

    if (slot_id >= hub->max_sensors) {
        nimcp_mutex_unlock(hub->mutex);
        LOG_ERROR(SENSOR_HUB_LOG_PREFIX "No empty slot found");
        return -1;
    }

    sensor_slot_t* slot = &hub->slots[slot_id];
    memset(slot, 0, sizeof(sensor_slot_t));

    /* Copy descriptor */
    slot->descriptor = *descriptor;
    slot->descriptor.sensor_id = slot_id;
    slot->registered = true;
    slot->active_idx = 0;
    slot->has_reading = false;
    slot->callback = NULL;
    slot->user_data = NULL;

    /* Pre-allocate data buffers if max_data_count is known */
    if (descriptor->max_data_count > 0) {
        slot->data_buffers[0] =
            (float*)nimcp_calloc(descriptor->max_data_count, sizeof(float));
        slot->data_buffers[1] =
            (float*)nimcp_calloc(descriptor->max_data_count, sizeof(float));
        if (!slot->data_buffers[0] || !slot->data_buffers[1]) {
            nimcp_free(slot->data_buffers[0]);
            nimcp_free(slot->data_buffers[1]);
            slot->data_buffers[0] = NULL;
            slot->data_buffers[1] = NULL;
            slot->registered = false;
            nimcp_mutex_unlock(hub->mutex);
            LOG_ERROR(SENSOR_HUB_LOG_PREFIX "Failed to allocate data buffers "
                      "for sensor '%s'", descriptor->name);
            return -1;
        }
        slot->data_capacity = descriptor->max_data_count;
    }

    hub->sensor_count++;

    nimcp_mutex_unlock(hub->mutex);

    LOG_INFO(SENSOR_HUB_LOG_PREFIX "Registered sensor %u: '%s' type=%s "
             "rate=%.1fHz max_data=%u",
             slot_id, descriptor->name,
             nimcp_sensor_type_name(descriptor->type),
             descriptor->sample_rate_hz, descriptor->max_data_count);

    return (int)slot_id;
}

int nimcp_sensor_unregister(nimcp_sensor_hub_t* hub, uint32_t sensor_id)
{
    if (!hub) {
        return -1;
    }

    nimcp_mutex_lock(hub->mutex);

    if (sensor_id >= hub->max_sensors || !hub->slots[sensor_id].registered) {
        nimcp_mutex_unlock(hub->mutex);
        LOG_ERROR(SENSOR_HUB_LOG_PREFIX "Invalid sensor_id %u for unregister",
                  sensor_id);
        return -1;
    }

    sensor_slot_t* slot = &hub->slots[sensor_id];

    nimcp_free(slot->data_buffers[0]);
    nimcp_free(slot->data_buffers[1]);
    memset(slot, 0, sizeof(sensor_slot_t));

    hub->sensor_count--;

    nimcp_mutex_unlock(hub->mutex);

    LOG_INFO(SENSOR_HUB_LOG_PREFIX "Unregistered sensor %u", sensor_id);
    return 0;
}

/* ============================================================================
 * Reading Submission & Retrieval
 * ============================================================================ */

int nimcp_sensor_submit_reading(nimcp_sensor_hub_t* hub,
                                 const nimcp_sensor_reading_t* reading)
{
    if (!hub || !reading) {
        return -1;
    }

    if (reading->sensor_id >= hub->max_sensors) {
        LOG_ERROR(SENSOR_HUB_LOG_PREFIX "submit: invalid sensor_id %u",
                  reading->sensor_id);
        return -1;
    }

    nimcp_mutex_lock(hub->mutex);

    sensor_slot_t* slot = &hub->slots[reading->sensor_id];
    if (!slot->registered) {
        nimcp_mutex_unlock(hub->mutex);
        LOG_ERROR(SENSOR_HUB_LOG_PREFIX "submit: sensor %u not registered",
                  reading->sensor_id);
        return -1;
    }

    /* Copy reading into back buffer and swap */
    int rc = slot_copy_reading(slot, reading);

    /* Set GPS reference from first GPS reading */
    if (rc == 0 && reading->type == NIMCP_SENSOR_GPS && !hub->gps_ref_set &&
        reading->valid && reading->data && reading->data_count >= 3) {
        hub->gps_ref_lat = (double)reading->data[0];
        hub->gps_ref_lon = (double)reading->data[1];
        hub->gps_ref_alt = (double)reading->data[2];
        hub->gps_ref_set = true;
        LOG_INFO(SENSOR_HUB_LOG_PREFIX "GPS reference set: lat=%.6f lon=%.6f alt=%.1f",
                 hub->gps_ref_lat, hub->gps_ref_lon, hub->gps_ref_alt);
    }

    /* Invoke callback if set */
    nimcp_sensor_callback_t cb = slot->callback;
    void* ud = slot->user_data;

    nimcp_mutex_unlock(hub->mutex);

    /* Call callback outside lock to avoid deadlock */
    if (rc == 0 && cb) {
        const nimcp_sensor_reading_t* active =
            &slot->readings[slot->active_idx];
        cb(active, ud);
    }

    return rc;
}

int nimcp_sensor_get_latest(nimcp_sensor_hub_t* hub, uint32_t sensor_id,
                             nimcp_sensor_reading_t* reading_out)
{
    if (!hub || !reading_out) {
        return -1;
    }

    if (sensor_id >= hub->max_sensors) {
        return -1;
    }

    sensor_slot_t* slot = &hub->slots[sensor_id];
    if (!slot->registered || !slot->has_reading) {
        return -1;
    }

    /* Read from active buffer (lock-free for single-writer scenarios;
     * the double-buffer swap is effectively atomic for int assignment) */
    int idx = slot->active_idx;
    *reading_out = slot->readings[idx];
    reading_out->data = slot->data_buffers[idx];

    return 0;
}

int nimcp_sensor_get_all_latest(nimcp_sensor_hub_t* hub,
                                 nimcp_sensor_reading_t* readings_out,
                                 uint32_t max_count)
{
    if (!hub || !readings_out || max_count == 0) {
        return -1;
    }

    uint32_t written = 0;

    for (uint32_t i = 0; i < hub->max_sensors && written < max_count; i++) {
        sensor_slot_t* slot = &hub->slots[i];
        if (!slot->registered || !slot->has_reading) {
            continue;
        }

        int idx = slot->active_idx;
        readings_out[written] = slot->readings[idx];
        readings_out[written].data = slot->data_buffers[idx];
        written++;
    }

    return (int)written;
}

/* ============================================================================
 * Async Callback
 * ============================================================================ */

int nimcp_sensor_set_callback(nimcp_sensor_hub_t* hub, uint32_t sensor_id,
                               nimcp_sensor_callback_t callback, void* user_data)
{
    if (!hub) {
        return -1;
    }

    if (sensor_id >= hub->max_sensors) {
        return -1;
    }

    nimcp_mutex_lock(hub->mutex);

    sensor_slot_t* slot = &hub->slots[sensor_id];
    if (!slot->registered) {
        nimcp_mutex_unlock(hub->mutex);
        return -1;
    }

    slot->callback = callback;
    slot->user_data = user_data;

    nimcp_mutex_unlock(hub->mutex);
    return 0;
}

/* ============================================================================
 * Feature Vector Composition
 * ============================================================================ */

int nimcp_sensor_compose_feature_vector(nimcp_sensor_hub_t* hub,
                                         float* features_out,
                                         uint32_t max_features)
{
    if (!hub || !features_out || max_features == 0) {
        return -1;
    }

    uint32_t offset = 0;

    /* Iterate sensors in slot order (ascending sensor_id) */
    for (uint32_t i = 0; i < hub->max_sensors; i++) {
        sensor_slot_t* slot = &hub->slots[i];
        if (!slot->registered || !slot->has_reading) {
            continue;
        }

        int idx = slot->active_idx;
        nimcp_sensor_reading_t* r = &slot->readings[idx];
        float* src = slot->data_buffers[idx];

        if (!r->valid || !src || r->data_count == 0) {
            continue;
        }

        /* Check remaining space */
        if (offset + r->data_count > max_features) {
            LOG_WARN(SENSOR_HUB_LOG_PREFIX "Feature vector truncated at sensor %u "
                     "(offset=%u, need=%u, max=%u)",
                     slot->descriptor.sensor_id, offset, r->data_count, max_features);
            break;
        }

        /* Copy with per-type normalization */
        switch (r->type) {
        case NIMCP_SENSOR_IMU:
            /* IMU typically: ax, ay, az, gx, gy, gz, mx, my, mz
             * Normalize accelerometer values (first 3) by gravity */
            for (uint32_t j = 0; j < r->data_count; j++) {
                if (j < 3) {
                    features_out[offset + j] = src[j] / GRAVITY_MS2;
                } else {
                    features_out[offset + j] = src[j];
                }
            }
            break;

        case NIMCP_SENSOR_GPS:
            /* GPS: lat, lon, alt → convert to local meters from reference */
            if (r->data_count >= 3 && hub->gps_ref_set) {
                double dlat = (double)src[0] - hub->gps_ref_lat;
                double dlon = (double)src[1] - hub->gps_ref_lon;
                double dalt = (double)src[2] - hub->gps_ref_alt;

                /* Convert degrees to meters */
                float lat_m = (float)(dlat * METERS_PER_DEG_LAT);
                float lon_m = (float)(dlon * METERS_PER_DEG_LAT *
                               cos((double)src[0] * DEG_TO_RAD));
                float alt_m = (float)dalt;

                features_out[offset + 0] = lat_m;
                features_out[offset + 1] = lon_m;
                features_out[offset + 2] = alt_m;

                /* Copy any extra GPS fields raw */
                for (uint32_t j = 3; j < r->data_count; j++) {
                    features_out[offset + j] = src[j];
                }
            } else {
                /* No reference set yet — pass through raw */
                memcpy(&features_out[offset], src,
                       r->data_count * sizeof(float));
            }
            break;

        case NIMCP_SENSOR_BUMPER:
            /* Bumper: force 0.0 or 1.0 */
            for (uint32_t j = 0; j < r->data_count; j++) {
                features_out[offset + j] = (src[j] > 0.5f) ? 1.0f : 0.0f;
            }
            break;

        default:
            /* All other types: pass through raw */
            memcpy(&features_out[offset], src,
                   r->data_count * sizeof(float));
            break;
        }

        offset += r->data_count;
    }

    return (int)offset;
}

/* ============================================================================
 * Query Functions
 * ============================================================================ */

uint32_t nimcp_sensor_get_count(const nimcp_sensor_hub_t* hub)
{
    if (!hub) {
        return 0;
    }
    return hub->sensor_count;
}

int nimcp_sensor_get_descriptor(const nimcp_sensor_hub_t* hub,
                                 uint32_t sensor_id,
                                 nimcp_sensor_descriptor_t* desc_out)
{
    if (!hub || !desc_out) {
        return -1;
    }

    if (sensor_id >= hub->max_sensors) {
        return -1;
    }

    const sensor_slot_t* slot = &hub->slots[sensor_id];
    if (!slot->registered) {
        return -1;
    }

    *desc_out = slot->descriptor;
    return 0;
}

const char* nimcp_sensor_type_name(nimcp_sensor_type_t type)
{
    if (type >= 0 && type < NIMCP_SENSOR_TYPE_COUNT) {
        return s_sensor_type_names[type];
    }
    return "UNKNOWN";
}
