/**
 * @file test_sensor_hub.cpp
 * @brief Unit tests for NIMCP sensor hub — registration, readings, feature composition.
 *
 * WHAT: Test sensor hub lifecycle, registration, reading submission/retrieval,
 *       feature vector composition, callbacks, and NULL safety.
 * WHY:  The sensor hub is the primary sensory input for edge brain inference;
 *       regressions here silently break brain perception.
 * HOW:  Google Test, stub mode (no real hardware).
 *
 * NOTE: nimcp_sensor_register() returns the assigned sensor_id (0-based slot index).
 *       All subsequent operations must use this returned id, not the descriptor's
 *       original sensor_id field.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "edge/nimcp_sensor.h"
}

// ============================================================================
// Lifecycle
// ============================================================================

TEST(SensorHub, CreateDestroy) {
    nimcp_sensor_hub_t* hub = nimcp_sensor_hub_create(16);
    ASSERT_NE(hub, nullptr);
    EXPECT_EQ(nimcp_sensor_get_count(hub), 0u);
    nimcp_sensor_hub_destroy(hub);
}

TEST(SensorHub, DestroyNull) {
    nimcp_sensor_hub_destroy(NULL);
    SUCCEED() << "nimcp_sensor_hub_destroy(NULL) did not crash";
}

TEST(SensorHub, CreateZeroCapacity) {
    nimcp_sensor_hub_t* hub = nimcp_sensor_hub_create(0);
    EXPECT_EQ(hub, nullptr) << "Zero capacity should return NULL";
}

// ============================================================================
// Registration
// ============================================================================

TEST(SensorHub, RegisterSingleSensor) {
    nimcp_sensor_hub_t* hub = nimcp_sensor_hub_create(16);
    ASSERT_NE(hub, nullptr);

    nimcp_sensor_descriptor_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.type = NIMCP_SENSOR_IMU;
    desc.format = NIMCP_SENSOR_FMT_VECTOR6;
    strncpy(desc.name, "imu_0", sizeof(desc.name) - 1);
    desc.sample_rate_hz = 100.0f;
    desc.max_data_count = 6;

    int sid = nimcp_sensor_register(hub, &desc);
    EXPECT_GE(sid, 0) << "Registration should succeed";
    EXPECT_EQ(nimcp_sensor_get_count(hub), 1u);

    nimcp_sensor_hub_destroy(hub);
}

TEST(SensorHub, RegisterMultipleSensorTypes) {
    nimcp_sensor_hub_t* hub = nimcp_sensor_hub_create(16);
    ASSERT_NE(hub, nullptr);

    struct { nimcp_sensor_type_t type; nimcp_sensor_format_t fmt; const char* name; } sensors[] = {
        {NIMCP_SENSOR_LIDAR,       NIMCP_SENSOR_FMT_FLOAT_ARRAY, "lidar_0"},
        {NIMCP_SENSOR_IMU,         NIMCP_SENSOR_FMT_VECTOR6,     "imu_0"},
        {NIMCP_SENSOR_GPS,         NIMCP_SENSOR_FMT_VECTOR3,     "gps_0"},
        {NIMCP_SENSOR_BUMPER,      NIMCP_SENSOR_FMT_BOOL,        "bumper_0"},
        {NIMCP_SENSOR_TEMPERATURE, NIMCP_SENSOR_FMT_SCALAR,      "temp_0"},
    };

    for (auto& s : sensors) {
        nimcp_sensor_descriptor_t desc;
        memset(&desc, 0, sizeof(desc));
        desc.type = s.type;
        desc.format = s.fmt;
        strncpy(desc.name, s.name, sizeof(desc.name) - 1);
        desc.max_data_count = 6;
        int sid = nimcp_sensor_register(hub, &desc);
        EXPECT_GE(sid, 0) << "Failed to register sensor " << s.name;
    }

    EXPECT_EQ(nimcp_sensor_get_count(hub), 5u);
    nimcp_sensor_hub_destroy(hub);
}

TEST(SensorHub, RegisterNullHub) {
    nimcp_sensor_descriptor_t desc;
    memset(&desc, 0, sizeof(desc));
    int rc = nimcp_sensor_register(NULL, &desc);
    EXPECT_LT(rc, 0) << "NULL hub should fail";
}

TEST(SensorHub, RegisterNullDescriptor) {
    nimcp_sensor_hub_t* hub = nimcp_sensor_hub_create(16);
    ASSERT_NE(hub, nullptr);
    int rc = nimcp_sensor_register(hub, NULL);
    EXPECT_LT(rc, 0) << "NULL descriptor should fail";
    nimcp_sensor_hub_destroy(hub);
}

TEST(SensorHub, RegisterOverCapacity) {
    nimcp_sensor_hub_t* hub = nimcp_sensor_hub_create(2);
    ASSERT_NE(hub, nullptr);

    for (uint32_t i = 0; i < 2; i++) {
        nimcp_sensor_descriptor_t desc;
        memset(&desc, 0, sizeof(desc));
        desc.type = NIMCP_SENSOR_IMU;
        desc.format = NIMCP_SENSOR_FMT_VECTOR3;
        desc.max_data_count = 3;
        EXPECT_GE(nimcp_sensor_register(hub, &desc), 0);
    }

    nimcp_sensor_descriptor_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.type = NIMCP_SENSOR_BUMPER;
    desc.format = NIMCP_SENSOR_FMT_BOOL;
    desc.max_data_count = 1;
    int rc = nimcp_sensor_register(hub, &desc);
    EXPECT_LT(rc, 0) << "Over-capacity registration should fail";

    nimcp_sensor_hub_destroy(hub);
}

TEST(SensorHub, UnregisterSensor) {
    nimcp_sensor_hub_t* hub = nimcp_sensor_hub_create(16);
    ASSERT_NE(hub, nullptr);

    nimcp_sensor_descriptor_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.type = NIMCP_SENSOR_GPS;
    desc.format = NIMCP_SENSOR_FMT_VECTOR3;
    desc.max_data_count = 3;
    int sid = nimcp_sensor_register(hub, &desc);
    ASSERT_GE(sid, 0);
    EXPECT_EQ(nimcp_sensor_get_count(hub), 1u);

    int rc = nimcp_sensor_unregister(hub, (uint32_t)sid);
    EXPECT_EQ(rc, 0);

    nimcp_sensor_hub_destroy(hub);
}

TEST(SensorHub, UnregisterNonexistent) {
    nimcp_sensor_hub_t* hub = nimcp_sensor_hub_create(16);
    ASSERT_NE(hub, nullptr);
    int rc = nimcp_sensor_unregister(hub, 999);
    EXPECT_LT(rc, 0) << "Unregistering nonexistent sensor should fail";
    nimcp_sensor_hub_destroy(hub);
}

// ============================================================================
// Reading Submission and Retrieval
// ============================================================================

TEST(SensorHub, SubmitAndGetLatest) {
    nimcp_sensor_hub_t* hub = nimcp_sensor_hub_create(16);
    ASSERT_NE(hub, nullptr);

    nimcp_sensor_descriptor_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.type = NIMCP_SENSOR_TEMPERATURE;
    desc.format = NIMCP_SENSOR_FMT_SCALAR;
    desc.max_data_count = 1;
    int sid = nimcp_sensor_register(hub, &desc);
    ASSERT_GE(sid, 0);

    float data = 25.5f;
    nimcp_sensor_reading_t reading;
    memset(&reading, 0, sizeof(reading));
    reading.sensor_id = (uint32_t)sid;
    reading.type = NIMCP_SENSOR_TEMPERATURE;
    reading.format = NIMCP_SENSOR_FMT_SCALAR;
    reading.data = &data;
    reading.data_count = 1;
    reading.confidence = 0.95f;
    reading.valid = true;
    reading.timestamp_us = 1000000;

    int rc = nimcp_sensor_submit_reading(hub, &reading);
    EXPECT_EQ(rc, 0);

    nimcp_sensor_reading_t latest;
    memset(&latest, 0, sizeof(latest));
    rc = nimcp_sensor_get_latest(hub, (uint32_t)sid, &latest);
    EXPECT_EQ(rc, 0);
    ASSERT_NE(latest.data, nullptr);
    EXPECT_NEAR(latest.data[0], 25.5f, 0.01f);

    nimcp_sensor_hub_destroy(hub);
}

TEST(SensorHub, SubmitNullHub) {
    nimcp_sensor_reading_t reading;
    memset(&reading, 0, sizeof(reading));
    int rc = nimcp_sensor_submit_reading(NULL, &reading);
    EXPECT_LT(rc, 0);
}

TEST(SensorHub, GetLatestNoReading) {
    nimcp_sensor_hub_t* hub = nimcp_sensor_hub_create(16);
    ASSERT_NE(hub, nullptr);

    nimcp_sensor_descriptor_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.type = NIMCP_SENSOR_IMU;
    desc.format = NIMCP_SENSOR_FMT_VECTOR3;
    desc.max_data_count = 3;
    int sid = nimcp_sensor_register(hub, &desc);
    ASSERT_GE(sid, 0);

    nimcp_sensor_reading_t latest;
    memset(&latest, 0, sizeof(latest));
    int rc = nimcp_sensor_get_latest(hub, (uint32_t)sid, &latest);
    EXPECT_LT(rc, 0) << "Should fail when no reading has been submitted";

    nimcp_sensor_hub_destroy(hub);
}

TEST(SensorHub, GetLatestNullHub) {
    nimcp_sensor_reading_t latest;
    memset(&latest, 0, sizeof(latest));
    int rc = nimcp_sensor_get_latest(NULL, 0, &latest);
    EXPECT_LT(rc, 0);
}

// ============================================================================
// Feature Vector Composition
// ============================================================================

TEST(SensorHub, ComposeFeatureVectorEmpty) {
    nimcp_sensor_hub_t* hub = nimcp_sensor_hub_create(16);
    ASSERT_NE(hub, nullptr);

    float features[32];
    memset(features, 0xFF, sizeof(features));
    int count = nimcp_sensor_compose_feature_vector(hub, features, 32);
    EXPECT_EQ(count, 0);

    nimcp_sensor_hub_destroy(hub);
}

TEST(SensorHub, ComposeFeatureVectorWithData) {
    nimcp_sensor_hub_t* hub = nimcp_sensor_hub_create(16);
    ASSERT_NE(hub, nullptr);

    nimcp_sensor_descriptor_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.type = NIMCP_SENSOR_TEMPERATURE;
    desc.format = NIMCP_SENSOR_FMT_SCALAR;
    desc.max_data_count = 1;
    int sid = nimcp_sensor_register(hub, &desc);
    ASSERT_GE(sid, 0);

    float data = 22.0f;
    nimcp_sensor_reading_t reading;
    memset(&reading, 0, sizeof(reading));
    reading.sensor_id = (uint32_t)sid;
    reading.type = NIMCP_SENSOR_TEMPERATURE;
    reading.format = NIMCP_SENSOR_FMT_SCALAR;
    reading.data = &data;
    reading.data_count = 1;
    reading.confidence = 1.0f;
    reading.valid = true;
    reading.timestamp_us = 2000000;
    ASSERT_EQ(nimcp_sensor_submit_reading(hub, &reading), 0);

    float features[32];
    int count = nimcp_sensor_compose_feature_vector(hub, features, 32);
    EXPECT_GT(count, 0);

    nimcp_sensor_hub_destroy(hub);
}

TEST(SensorHub, ComposeFeatureVectorNullHub) {
    float features[32];
    int rc = nimcp_sensor_compose_feature_vector(NULL, features, 32);
    EXPECT_LT(rc, 0);
}

// ============================================================================
// Query Functions
// ============================================================================

TEST(SensorHub, GetCountNull) {
    EXPECT_EQ(nimcp_sensor_get_count(NULL), 0u);
}

TEST(SensorHub, GetDescriptor) {
    nimcp_sensor_hub_t* hub = nimcp_sensor_hub_create(16);
    ASSERT_NE(hub, nullptr);

    nimcp_sensor_descriptor_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.type = NIMCP_SENSOR_GPS;
    desc.format = NIMCP_SENSOR_FMT_VECTOR3;
    strncpy(desc.name, "gps_main", sizeof(desc.name) - 1);
    desc.max_data_count = 3;
    desc.sample_rate_hz = 10.0f;
    int sid = nimcp_sensor_register(hub, &desc);
    ASSERT_GE(sid, 0);

    nimcp_sensor_descriptor_t out;
    memset(&out, 0, sizeof(out));
    int rc = nimcp_sensor_get_descriptor(hub, (uint32_t)sid, &out);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(out.type, NIMCP_SENSOR_GPS);
    EXPECT_STREQ(out.name, "gps_main");

    nimcp_sensor_hub_destroy(hub);
}

TEST(SensorHub, SensorTypeNames) {
    const char* name = nimcp_sensor_type_name(NIMCP_SENSOR_LIDAR);
    EXPECT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = nimcp_sensor_type_name(NIMCP_SENSOR_IMU);
    EXPECT_NE(name, nullptr);

    name = nimcp_sensor_type_name(NIMCP_SENSOR_GPS);
    EXPECT_NE(name, nullptr);

    name = nimcp_sensor_type_name(NIMCP_SENSOR_BUMPER);
    EXPECT_NE(name, nullptr);
}

// ============================================================================
// Callback
// ============================================================================

static void test_sensor_callback(const nimcp_sensor_reading_t* reading, void* user_data) {
    (void)reading;
    int* counter = (int*)user_data;
    if (counter) (*counter)++;
}

TEST(SensorHub, SetCallbackAndInvoke) {
    nimcp_sensor_hub_t* hub = nimcp_sensor_hub_create(16);
    ASSERT_NE(hub, nullptr);

    nimcp_sensor_descriptor_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.type = NIMCP_SENSOR_TEMPERATURE;
    desc.format = NIMCP_SENSOR_FMT_SCALAR;
    desc.max_data_count = 1;
    int sid = nimcp_sensor_register(hub, &desc);
    ASSERT_GE(sid, 0);

    int counter = 0;
    int rc = nimcp_sensor_set_callback(hub, (uint32_t)sid, test_sensor_callback, &counter);
    EXPECT_EQ(rc, 0);

    float data = 30.0f;
    nimcp_sensor_reading_t reading;
    memset(&reading, 0, sizeof(reading));
    reading.sensor_id = (uint32_t)sid;
    reading.type = NIMCP_SENSOR_TEMPERATURE;
    reading.format = NIMCP_SENSOR_FMT_SCALAR;
    reading.data = &data;
    reading.data_count = 1;
    reading.valid = true;
    reading.timestamp_us = 3000000;
    ASSERT_EQ(nimcp_sensor_submit_reading(hub, &reading), 0);

    EXPECT_GT(counter, 0) << "Callback should have been invoked";

    nimcp_sensor_hub_destroy(hub);
}

TEST(SensorHub, ClearCallback) {
    nimcp_sensor_hub_t* hub = nimcp_sensor_hub_create(16);
    ASSERT_NE(hub, nullptr);

    nimcp_sensor_descriptor_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.type = NIMCP_SENSOR_BUMPER;
    desc.format = NIMCP_SENSOR_FMT_BOOL;
    desc.max_data_count = 1;
    int sid = nimcp_sensor_register(hub, &desc);
    ASSERT_GE(sid, 0);

    int counter = 0;
    nimcp_sensor_set_callback(hub, (uint32_t)sid, test_sensor_callback, &counter);
    nimcp_sensor_set_callback(hub, (uint32_t)sid, NULL, NULL);

    float data = 1.0f;
    nimcp_sensor_reading_t reading;
    memset(&reading, 0, sizeof(reading));
    reading.sensor_id = (uint32_t)sid;
    reading.type = NIMCP_SENSOR_BUMPER;
    reading.format = NIMCP_SENSOR_FMT_BOOL;
    reading.data = &data;
    reading.data_count = 1;
    reading.valid = true;
    nimcp_sensor_submit_reading(hub, &reading);

    EXPECT_EQ(counter, 0) << "Callback should NOT have been invoked after clearing";

    nimcp_sensor_hub_destroy(hub);
}

// ============================================================================
// Various Sensor Formats
// ============================================================================

TEST(SensorHub, Vector3Reading) {
    nimcp_sensor_hub_t* hub = nimcp_sensor_hub_create(16);
    ASSERT_NE(hub, nullptr);

    nimcp_sensor_descriptor_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.type = NIMCP_SENSOR_IMU;
    desc.format = NIMCP_SENSOR_FMT_VECTOR3;
    desc.max_data_count = 3;
    int sid = nimcp_sensor_register(hub, &desc);
    ASSERT_GE(sid, 0);

    float data[3] = {0.1f, 9.81f, -0.3f};
    nimcp_sensor_reading_t reading;
    memset(&reading, 0, sizeof(reading));
    reading.sensor_id = (uint32_t)sid;
    reading.type = NIMCP_SENSOR_IMU;
    reading.format = NIMCP_SENSOR_FMT_VECTOR3;
    reading.data = data;
    reading.data_count = 3;
    reading.valid = true;
    EXPECT_EQ(nimcp_sensor_submit_reading(hub, &reading), 0);

    nimcp_sensor_reading_t latest;
    memset(&latest, 0, sizeof(latest));
    EXPECT_EQ(nimcp_sensor_get_latest(hub, (uint32_t)sid, &latest), 0);
    ASSERT_NE(latest.data, nullptr);
    EXPECT_NEAR(latest.data[1], 9.81f, 0.01f);

    nimcp_sensor_hub_destroy(hub);
}

TEST(SensorHub, QuaternionReading) {
    nimcp_sensor_hub_t* hub = nimcp_sensor_hub_create(16);
    ASSERT_NE(hub, nullptr);

    nimcp_sensor_descriptor_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.type = NIMCP_SENSOR_IMU;
    desc.format = NIMCP_SENSOR_FMT_QUATERNION;
    desc.max_data_count = 4;
    int sid = nimcp_sensor_register(hub, &desc);
    ASSERT_GE(sid, 0);

    float data[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    nimcp_sensor_reading_t reading;
    memset(&reading, 0, sizeof(reading));
    reading.sensor_id = (uint32_t)sid;
    reading.type = NIMCP_SENSOR_IMU;
    reading.format = NIMCP_SENSOR_FMT_QUATERNION;
    reading.data = data;
    reading.data_count = 4;
    reading.valid = true;
    EXPECT_EQ(nimcp_sensor_submit_reading(hub, &reading), 0);

    nimcp_sensor_hub_destroy(hub);
}

TEST(SensorHub, BoolReading) {
    nimcp_sensor_hub_t* hub = nimcp_sensor_hub_create(16);
    ASSERT_NE(hub, nullptr);

    nimcp_sensor_descriptor_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.type = NIMCP_SENSOR_BUMPER;
    desc.format = NIMCP_SENSOR_FMT_BOOL;
    desc.max_data_count = 1;
    int sid = nimcp_sensor_register(hub, &desc);
    ASSERT_GE(sid, 0);

    float data = 1.0f;
    nimcp_sensor_reading_t reading;
    memset(&reading, 0, sizeof(reading));
    reading.sensor_id = (uint32_t)sid;
    reading.type = NIMCP_SENSOR_BUMPER;
    reading.format = NIMCP_SENSOR_FMT_BOOL;
    reading.data = &data;
    reading.data_count = 1;
    reading.valid = true;
    EXPECT_EQ(nimcp_sensor_submit_reading(hub, &reading), 0);

    nimcp_sensor_hub_destroy(hub);
}
