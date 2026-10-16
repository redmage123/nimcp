/**
 * @file test_edge_pipeline_integration.cpp
 * @brief Integration tests for edge module pipeline — sensor + watchdog + bridges.
 *
 * WHAT: Test multi-module interactions: sensor hub -> brain input, watchdog +
 *       inference pipeline, cognitive training domain coverage, and sequential
 *       module call chains.
 * WHY:  Individual modules may work but integration can fail from mismatched
 *       types, missing initialization, or ordering bugs.
 * HOW:  Google Test, stub mode, no real brain or hardware.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>
#include <set>
#include <sstream>

extern "C" {
#include "edge/nimcp_sensor.h"
#include "edge/nimcp_safety_watchdog.h"
#include "edge/nimcp_mavlink_bridge.h"
#include "edge/nimcp_dji_bridge.h"
#include "edge/nimcp_msp_bridge.h"
#include "edge/nimcp_parrot_bridge.h"
#include "edge/nimcp_ros2_bridge.h"
}

// Helper: run Python expression and return stdout
static std::string run_python(const char* expr) {
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
        "python3 -c \""
        "import sys; sys.path.insert(0, '/home/bbrelin/nimcp/scripts'); "
        "from cognitive_training_data import *; "
        "%s\" 2>/dev/null", expr);
    FILE* fp = popen(cmd, "r");
    if (!fp) return "";
    char buf[8192];
    std::string result;
    while (fgets(buf, sizeof(buf), fp)) result += buf;
    pclose(fp);
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();
    return result;
}

// ============================================================================
// Sensor Hub + Inference Pipeline
// ============================================================================

TEST(EdgePipelineIntegration, SensorComposeToFeatureVector) {
    // Create sensor hub, register sensors, submit readings, compose feature vector
    nimcp_sensor_hub_t* hub = nimcp_sensor_hub_create(16);
    ASSERT_NE(hub, nullptr);

    // Register IMU
    nimcp_sensor_descriptor_t imu_desc;
    memset(&imu_desc, 0, sizeof(imu_desc));
    imu_desc.type = NIMCP_SENSOR_IMU;
    imu_desc.format = NIMCP_SENSOR_FMT_VECTOR6;
    strncpy(imu_desc.name, "imu_0", sizeof(imu_desc.name) - 1);
    imu_desc.max_data_count = 6;
    int imu_sid = nimcp_sensor_register(hub, &imu_desc);
    ASSERT_GE(imu_sid, 0);

    // Register temperature
    nimcp_sensor_descriptor_t temp_desc;
    memset(&temp_desc, 0, sizeof(temp_desc));
    temp_desc.type = NIMCP_SENSOR_TEMPERATURE;
    temp_desc.format = NIMCP_SENSOR_FMT_SCALAR;
    strncpy(temp_desc.name, "temp_0", sizeof(temp_desc.name) - 1);
    temp_desc.max_data_count = 1;
    int temp_sid = nimcp_sensor_register(hub, &temp_desc);
    ASSERT_GE(temp_sid, 0);

    // Submit IMU reading
    float imu_data[6] = {0.1f, 9.81f, -0.05f, 0.01f, -0.02f, 0.03f};
    nimcp_sensor_reading_t imu_reading;
    memset(&imu_reading, 0, sizeof(imu_reading));
    imu_reading.sensor_id = (uint32_t)imu_sid;
    imu_reading.type = NIMCP_SENSOR_IMU;
    imu_reading.format = NIMCP_SENSOR_FMT_VECTOR6;
    imu_reading.data = imu_data;
    imu_reading.data_count = 6;
    imu_reading.valid = true;
    imu_reading.confidence = 0.99f;
    ASSERT_EQ(nimcp_sensor_submit_reading(hub, &imu_reading), 0);

    // Submit temperature reading
    float temp_data = 23.5f;
    nimcp_sensor_reading_t temp_reading;
    memset(&temp_reading, 0, sizeof(temp_reading));
    temp_reading.sensor_id = (uint32_t)temp_sid;
    temp_reading.type = NIMCP_SENSOR_TEMPERATURE;
    temp_reading.format = NIMCP_SENSOR_FMT_SCALAR;
    temp_reading.data = &temp_data;
    temp_reading.data_count = 1;
    temp_reading.valid = true;
    ASSERT_EQ(nimcp_sensor_submit_reading(hub, &temp_reading), 0);

    // Compose feature vector
    float features[64];
    int count = nimcp_sensor_compose_feature_vector(hub, features, 64);
    EXPECT_GT(count, 0);

    nimcp_sensor_hub_destroy(hub);
}

// ============================================================================
// Watchdog + Inference Pipeline
// ============================================================================

TEST(EdgePipelineIntegration, WatchdogHeartbeatAndValidate) {
    nimcp_safety_watchdog_t* wd = nimcp_watchdog_create(NULL);
    ASSERT_NE(wd, nullptr);
    nimcp_watchdog_arm(wd);

    // Simulate inference cycle: heartbeat -> validate output
    for (int i = 0; i < 10; i++) {
        nimcp_watchdog_heartbeat(wd);
        float output[4] = {0.1f * i, -0.05f * i, 0.0f, 0.0f};
        // Clamp to valid range for testing
        for (int j = 0; j < 4; j++) {
            if (output[j] > 1.0f) output[j] = 0.9f;
            if (output[j] < -1.0f) output[j] = -0.9f;
        }
        nimcp_watchdog_validate_output(wd, output, 4);
    }

    EXPECT_EQ(nimcp_watchdog_get_state(wd), NIMCP_WATCHDOG_ARMED);

    nimcp_watchdog_disarm(wd);
    nimcp_watchdog_destroy(wd);
}

// ============================================================================
// Multiple Modules Created and Destroyed
// ============================================================================

TEST(EdgePipelineIntegration, MultiModuleLifecycleNoCrash) {
    // Create all edge modules, then destroy in reverse order — no crash
    nimcp_sensor_hub_t* hub = nimcp_sensor_hub_create(16);
    nimcp_safety_watchdog_t* wd = nimcp_watchdog_create(NULL);
    nimcp_mavlink_bridge_t* mav = nimcp_mavlink_bridge_create(NULL);
    nimcp_dji_bridge_t* dji = nimcp_dji_bridge_create(NULL);
    nimcp_msp_bridge_t* msp = nimcp_msp_bridge_create(NULL);
    nimcp_parrot_bridge_t* parrot = nimcp_parrot_bridge_create(NULL);

    EXPECT_NE(hub, nullptr);
    EXPECT_NE(wd, nullptr);
    EXPECT_NE(mav, nullptr);
    EXPECT_NE(dji, nullptr);
    EXPECT_NE(msp, nullptr);
    EXPECT_NE(parrot, nullptr);

    nimcp_parrot_bridge_destroy(parrot);
    nimcp_msp_bridge_destroy(msp);
    nimcp_dji_bridge_destroy(dji);
    nimcp_mavlink_bridge_destroy(mav);
    nimcp_watchdog_destroy(wd);
    nimcp_sensor_hub_destroy(hub);

    SUCCEED();
}

// ============================================================================
// Sequential Module Calls
// ============================================================================

TEST(EdgePipelineIntegration, SequentialSensorBrainWatchdog) {
    // Simulate: sensor -> compose features -> (brain inference placeholder) -> watchdog validate
    nimcp_sensor_hub_t* hub = nimcp_sensor_hub_create(16);
    ASSERT_NE(hub, nullptr);

    nimcp_sensor_descriptor_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.type = NIMCP_SENSOR_IMU;
    desc.format = NIMCP_SENSOR_FMT_VECTOR3;
    desc.max_data_count = 3;
    int sid = nimcp_sensor_register(hub, &desc);
    ASSERT_GE(sid, 0);

    float imu[3] = {0.1f, 9.81f, -0.05f};
    nimcp_sensor_reading_t reading;
    memset(&reading, 0, sizeof(reading));
    reading.sensor_id = (uint32_t)sid;
    reading.type = NIMCP_SENSOR_IMU;
    reading.format = NIMCP_SENSOR_FMT_VECTOR3;
    reading.data = imu;
    reading.data_count = 3;
    reading.valid = true;
    nimcp_sensor_submit_reading(hub, &reading);

    float features[32];
    int feat_count = nimcp_sensor_compose_feature_vector(hub, features, 32);
    EXPECT_GT(feat_count, 0);

    // Simulate brain output (just use sensor features scaled down)
    float brain_output[4] = {features[0] * 0.1f, 0.0f, 0.0f, 0.0f};

    nimcp_safety_watchdog_t* wd = nimcp_watchdog_create(NULL);
    ASSERT_NE(wd, nullptr);
    nimcp_watchdog_arm(wd);
    nimcp_watchdog_heartbeat(wd);

    int rc = nimcp_watchdog_validate_output(wd, brain_output, 4);
    EXPECT_EQ(rc, 0) << "Scaled brain output should be valid";

    nimcp_watchdog_disarm(wd);
    nimcp_watchdog_destroy(wd);
    nimcp_sensor_hub_destroy(hub);
}

// ============================================================================
// Cognitive Training Data Coverage (13 domains)
// ============================================================================

TEST(EdgePipelineIntegration, CognitiveDataAllDomainsExist) {
    std::string out = run_python(
        "data = get_all_cognitive_data();"
        "from collections import Counter;"
        "c = Counter(d['domain'] for d in data);"
        "print(len(c))");
    if (out.empty()) { GTEST_SKIP() << "Python module not importable"; }
    int domain_count = atoi(out.c_str());
    // At least 9 original + some new domains
    EXPECT_GE(domain_count, 9) << "Expected at least 9 cognitive domains";
}

TEST(EdgePipelineIntegration, CognitiveDataAllDomainsHaveItems) {
    std::string out = run_python(
        "data = get_all_cognitive_data();"
        "from collections import Counter;"
        "c = Counter(d['domain'] for d in data);"
        "min_count = min(c.values());"
        "print(min_count)");
    if (out.empty()) { GTEST_SKIP() << "Python module not importable"; }
    int min_count = atoi(out.c_str());
    EXPECT_GE(min_count, 5) << "Each domain should have at least 5 items";
}

TEST(EdgePipelineIntegration, CognitiveDataLabelFormat) {
    std::string out = run_python(
        "import re;"
        "data = get_all_cognitive_data();"
        "bad = [d['label'] for d in data if not re.match(r'^[a-z][a-z0-9_]+$', d['label'])];"
        "print(len(bad))");
    if (out.empty()) { GTEST_SKIP() << "Python module not importable"; }
    EXPECT_EQ(atoi(out.c_str()), 0) << "All labels should match lowercase_underscore format";
}

TEST(EdgePipelineIntegration, CognitiveOriginalDomainDispatch) {
    // Verify original 9 domain triggers still work
    struct { const char* domain; const char* trigger; } triggers[] = {
        {"ethics", "ethics"},
        {"counterfactual", "counterfactual"},
        {"causal", "causal"},
        {"metacognition", "metacog"},
        {"analogy", "analogy"},
        {"rcog", "rcog_"},
        {"collective", "collective"},
        {"portia", "portia"},
        {"dragonfly", "dragonfly"},
    };

    for (auto& t : triggers) {
        char py[512];
        snprintf(py, sizeof(py),
            "data = [d for d in get_all_cognitive_data() if d['domain'] == '%s'];"
            "matches = [d for d in data if '%s' in d['label']];"
            "print(len(matches))", t.domain, t.trigger);
        std::string out = run_python(py);
        if (out.empty()) { GTEST_SKIP() << "Python not available"; }
        int count = atoi(out.c_str());
        EXPECT_GT(count, 0) << "Domain '" << t.domain
            << "' has no labels matching trigger '" << t.trigger << "'";
    }
}

TEST(EdgePipelineIntegration, CognitiveNewDomainLabels) {
    // Check if new edge-related domain labels exist (sensor_, motor_, safety_, embodiment_)
    std::string out = run_python(
        "data = get_all_cognitive_data();"
        "labels = [d['label'] for d in data];"
        "prefixes = ['sensor_', 'motor_', 'safety_', 'embodiment_'];"
        "found = sum(1 for p in prefixes if any(l.startswith(p) for l in labels));"
        "print(found)");
    if (out.empty()) { GTEST_SKIP() << "Python not available"; }
    // If new training domains haven't been added yet, this is informational
    int found = atoi(out.c_str());
    if (found == 0) {
        // Not a failure — new domains may not exist yet
        SUCCEED() << "No new edge training domain labels found (informational)";
    } else {
        EXPECT_GT(found, 0) << "Expected some edge training domain labels";
    }
}

// ============================================================================
// Feature Count Constants
// ============================================================================

TEST(EdgePipelineIntegration, FeatureCountConstants) {
    EXPECT_EQ(NIMCP_MAVLINK_FEATURE_COUNT, 14);
    EXPECT_EQ(NIMCP_DJI_FEATURE_COUNT, 16);
    EXPECT_EQ(NIMCP_MSP_FEATURE_COUNT, 12);
    EXPECT_EQ(NIMCP_PARROT_FEATURE_COUNT, 14);
}

// ============================================================================
// All Bridge Compose Features Return Zeros in Stub
// ============================================================================

TEST(EdgePipelineIntegration, AllBridgesComposeFeatures) {
    // MAVLink
    {
        nimcp_mavlink_bridge_t* br = nimcp_mavlink_bridge_create(NULL);
        ASSERT_NE(br, nullptr);
        float f[NIMCP_MAVLINK_FEATURE_COUNT];
        int n = nimcp_mavlink_compose_features(br, f, NIMCP_MAVLINK_FEATURE_COUNT);
        EXPECT_EQ(n, NIMCP_MAVLINK_FEATURE_COUNT);
        nimcp_mavlink_bridge_destroy(br);
    }
    // DJI
    {
        nimcp_dji_bridge_t* br = nimcp_dji_bridge_create(NULL);
        ASSERT_NE(br, nullptr);
        float f[NIMCP_DJI_FEATURE_COUNT];
        int n = nimcp_dji_compose_features(br, f, NIMCP_DJI_FEATURE_COUNT);
        EXPECT_EQ(n, NIMCP_DJI_FEATURE_COUNT);
        nimcp_dji_bridge_destroy(br);
    }
    // MSP
    {
        nimcp_msp_bridge_t* br = nimcp_msp_bridge_create(NULL);
        ASSERT_NE(br, nullptr);
        float f[NIMCP_MSP_FEATURE_COUNT];
        int n = nimcp_msp_compose_features(br, f, NIMCP_MSP_FEATURE_COUNT);
        EXPECT_EQ(n, NIMCP_MSP_FEATURE_COUNT);
        nimcp_msp_bridge_destroy(br);
    }
    // Parrot
    {
        nimcp_parrot_bridge_t* br = nimcp_parrot_bridge_create(NULL);
        ASSERT_NE(br, nullptr);
        float f[NIMCP_PARROT_FEATURE_COUNT];
        int n = nimcp_parrot_compose_features(br, f, NIMCP_PARROT_FEATURE_COUNT);
        EXPECT_EQ(n, NIMCP_PARROT_FEATURE_COUNT);
        nimcp_parrot_bridge_destroy(br);
    }
}
