/**
 * @file test_swarm_cascade.cpp
 * @brief Tests for Cascade Prevention System
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_cascade.h"

class CascadeTest : public ::testing::Test {
protected:
    nimcp_cascade_system_t* system;
    nimcp_cascade_config_t config;

    void SetUp() override {
        nimcp_cascade_get_default_config(&config);
        system = nimcp_cascade_create(&config, nullptr);
        ASSERT_NE(system, nullptr);
    }

    void TearDown() override {
        if (system) nimcp_cascade_destroy(system);
    }
};

TEST_F(CascadeTest, CreateSystem) { EXPECT_NE(system, nullptr); }
TEST_F(CascadeTest, UpdateTelemetry) {
    nimcp_health_telemetry_t t = {0.5, 0.6, 10.0, 0.01, 0.0, 5.0, 10.0, 25.0, 100, 0, 0};
    EXPECT_EQ(nimcp_cascade_update_telemetry(system, &t), NIMCP_SUCCESS);
}
TEST_F(CascadeTest, GetHealth) {
    nimcp_health_state_t h = nimcp_cascade_get_health_state(system);
    EXPECT_GE(h, HEALTH_OPTIMAL);
    EXPECT_LE(h, HEALTH_UNKNOWN);
}
TEST_F(CascadeTest, DetectAnomaly) {
    nimcp_health_telemetry_t t = {0.99, 0.95, 500.0, 0.5, 0.8, 100.0, 1000.0, 80.0, 10, 100, 0};
    nimcp_anomaly_detection_t a;
    EXPECT_EQ(nimcp_cascade_detect_anomaly(system, &t, &a), NIMCP_SUCCESS);
}
TEST_F(CascadeTest, PredictFailure) {
    nimcp_failure_prediction_t p;
    EXPECT_EQ(nimcp_cascade_predict_failure(system, &p), NIMCP_SUCCESS);
}
TEST_F(CascadeTest, RegisterBreaker) {
    uint32_t id;
    nimcp_breaker_config_t bc = {5, 30000000, 3, 5};
    EXPECT_EQ(nimcp_cascade_register_breaker(system, "test_svc", &bc, &id), NIMCP_SUCCESS);
}
TEST_F(CascadeTest, RecordOperation) {
    uint32_t id;
    nimcp_breaker_config_t bc = {5, 30000000, 3, 5};
    nimcp_cascade_register_breaker(system, "svc", &bc, &id);
    EXPECT_EQ(nimcp_cascade_record_operation(system, id, true), NIMCP_SUCCESS);
}
TEST_F(CascadeTest, CheckBreaker) {
    uint32_t id;
    nimcp_breaker_config_t bc = {5, 30000000, 3, 5};
    nimcp_cascade_register_breaker(system, "svc", &bc, &id);
    bool allowed;
    EXPECT_EQ(nimcp_cascade_check_breaker(system, id, &allowed), NIMCP_SUCCESS);
}
TEST_F(CascadeTest, RegisterCapability) {
    uint32_t id;
    nimcp_capability_t cap = {"test_cap", PRIORITY_MEDIUM, true, 0.2, 0};
    EXPECT_EQ(nimcp_cascade_register_capability(system, &cap, &id), NIMCP_SUCCESS);
}
TEST_F(CascadeTest, DecideLoadShedding) {
    nimcp_load_shedding_decision_t d;
    EXPECT_EQ(nimcp_cascade_decide_load_shedding(system, HEALTH_DEGRADED, &d), NIMCP_SUCCESS);
}
TEST_F(CascadeTest, RegisterRedundancy) {
    uint32_t id;
    nimcp_redundancy_group_t g = {"group1", 1, {2,3}, 2, {ROLE_HOT_STANDBY, ROLE_WARM_STANDBY}, 0, 60000000};
    EXPECT_EQ(nimcp_cascade_register_redundancy_group(system, &g, &id), NIMCP_SUCCESS);
}
TEST_F(CascadeTest, RecordFailure) {
    nimcp_failure_event_t e = {1, HEALTH_OPTIMAL, HEALTH_FAILED, SEVERITY_MAJOR, 0, "test"};
    EXPECT_EQ(nimcp_cascade_record_failure(system, &e), NIMCP_SUCCESS);
}
TEST_F(CascadeTest, DetectCascade) {
    nimcp_cascade_detection_t d;
    EXPECT_EQ(nimcp_cascade_detect_cascade(system, &d), NIMCP_SUCCESS);
}
TEST_F(CascadeTest, StartRecovery) {
    EXPECT_EQ(nimcp_cascade_start_recovery(system, 1, RECOVERY_GRADUAL), NIMCP_SUCCESS);
}
TEST_F(CascadeTest, GetStats) {
    uint64_t f, c, r;
    EXPECT_EQ(nimcp_cascade_get_statistics(system, &f, &c, &r), NIMCP_SUCCESS);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
