/**
 * @file test_stdp_state_ops.cpp
 * @brief Unit tests for STDP synapse state serialization operations
 *
 * WHAT: Unit tests for STDP state checkpoint/restore
 * WHY:  Verify STDP state ops enable reliable fault tolerance
 * HOW:  Test serialization, deserialization, validation, and reset
 *
 * PHASE 8: System-Wide Health Integration
 *
 * @author NIMCP Team
 * @date 2026-01-20
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "plasticity/stdp/nimcp_stdp.h"
#include "utils/fault_tolerance/nimcp_state_manager.h"
#include "utils/fault_tolerance/nimcp_module_recovery.h"
}

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief STDP state operations test fixture
 *
 * WHAT: Provides initialized STDP synapse for each test
 * WHY:  Ensure test isolation
 * HOW:  Initialize synapse in SetUp
 */
class StdpStateOpsTest : public ::testing::Test {
protected:
    stdp_synapse_t synapse;
    const nimcp_module_state_ops_t* state_ops;

    void SetUp() override {
        stdp_synapse_init(&synapse);
        state_ops = stdp_get_state_ops();
    }

    void TearDown() override {
        /* Nothing to clean up - synapse is stack-allocated */
    }
};

//=============================================================================
// State Ops Availability Tests
//=============================================================================

TEST_F(StdpStateOpsTest, GetStateOpsNotNull) {
    ASSERT_NE(state_ops, nullptr);
}

TEST_F(StdpStateOpsTest, AllOpsProvided) {
    ASSERT_NE(state_ops->serialize, nullptr);
    ASSERT_NE(state_ops->deserialize, nullptr);
    ASSERT_NE(state_ops->validate, nullptr);
    ASSERT_NE(state_ops->reset, nullptr);
    ASSERT_NE(state_ops->get_size, nullptr);
}

//=============================================================================
// Serialization Tests
//=============================================================================

TEST_F(StdpStateOpsTest, GetSizeReturnsPositive) {
    size_t size = state_ops->get_size(&synapse);
    EXPECT_GT(size, 0u);
    /* Serialized size may be smaller than struct (excludes spinlock, padding) */
    EXPECT_GT(size, 50u);  /* Reasonable minimum for STDP state */
}

TEST_F(StdpStateOpsTest, SerializeQuerySize) {
    size_t size = 0;
    int result = state_ops->serialize(&synapse, nullptr, &size);

    EXPECT_EQ(result, 0);
    EXPECT_GT(size, 0u);
}

TEST_F(StdpStateOpsTest, SerializeToBuffer) {
    /* Query size */
    size_t size = 0;
    state_ops->serialize(&synapse, nullptr, &size);

    /* Allocate and serialize */
    std::vector<uint8_t> buffer(size);
    size_t written = size;
    int result = state_ops->serialize(&synapse, buffer.data(), &written);

    EXPECT_EQ(result, 0);
    EXPECT_GT(written, 0u);
    EXPECT_LE(written, size);
}

TEST_F(StdpStateOpsTest, SerializeBufferTooSmall) {
    uint8_t small_buffer[4];
    size_t size = sizeof(small_buffer);

    int result = state_ops->serialize(&synapse, small_buffer, &size);

    EXPECT_EQ(result, -2);  /* Buffer too small */
    EXPECT_GT(size, sizeof(small_buffer));  /* Returns required size */
}

TEST_F(StdpStateOpsTest, SerializeNullState) {
    size_t size = 1024;
    std::vector<uint8_t> buffer(size);

    int result = state_ops->serialize(nullptr, buffer.data(), &size);
    EXPECT_LT(result, 0);
}

TEST_F(StdpStateOpsTest, SerializeNullSize) {
    std::vector<uint8_t> buffer(1024);
    int result = state_ops->serialize(&synapse, buffer.data(), nullptr);
    EXPECT_LT(result, 0);
}

TEST_F(StdpStateOpsTest, SerializePreservesValues) {
    /* Set known values */
    synapse.weight = 0.75f;
    synapse.pre_trace = 0.123f;
    synapse.post_trace = 0.456f;
    synapse.learning_rate = 0.02f;
    synapse.num_potentiation_events = 100;
    synapse.num_depression_events = 50;

    /* Serialize */
    size_t size = 0;
    state_ops->serialize(&synapse, nullptr, &size);
    std::vector<uint8_t> buffer(size);
    size_t written = size;
    state_ops->serialize(&synapse, buffer.data(), &written);

    /* Create fresh synapse and deserialize */
    stdp_synapse_t restored;
    stdp_synapse_init(&restored);
    int result = state_ops->deserialize(&restored, buffer.data(), written);

    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(restored.weight, 0.75f);
    EXPECT_FLOAT_EQ(restored.pre_trace, 0.123f);
    EXPECT_FLOAT_EQ(restored.post_trace, 0.456f);
    EXPECT_FLOAT_EQ(restored.learning_rate, 0.02f);
    EXPECT_EQ(restored.num_potentiation_events, 100u);
    EXPECT_EQ(restored.num_depression_events, 50u);
}

//=============================================================================
// Deserialization Tests
//=============================================================================

TEST_F(StdpStateOpsTest, DeserializeValidBuffer) {
    /* Serialize first */
    size_t size = 0;
    state_ops->serialize(&synapse, nullptr, &size);
    std::vector<uint8_t> buffer(size);
    size_t written = size;
    state_ops->serialize(&synapse, buffer.data(), &written);

    /* Deserialize into new synapse (must initialize first for spinlock) */
    stdp_synapse_t restored;
    stdp_synapse_init(&restored);
    int result = state_ops->deserialize(&restored, buffer.data(), written);

    EXPECT_EQ(result, 0);
}

TEST_F(StdpStateOpsTest, DeserializeNullState) {
    std::vector<uint8_t> buffer(1024);
    int result = state_ops->deserialize(nullptr, buffer.data(), buffer.size());
    EXPECT_LT(result, 0);
}

TEST_F(StdpStateOpsTest, DeserializeNullBuffer) {
    int result = state_ops->deserialize(&synapse, nullptr, 1024);
    EXPECT_LT(result, 0);
}

TEST_F(StdpStateOpsTest, DeserializeBufferTooSmall) {
    uint8_t small_buffer[4] = {0};
    int result = state_ops->deserialize(&synapse, small_buffer, sizeof(small_buffer));
    EXPECT_LT(result, 0);
}

TEST_F(StdpStateOpsTest, DeserializeInvalidMagic) {
    /* Serialize valid state */
    size_t size = 0;
    state_ops->serialize(&synapse, nullptr, &size);
    std::vector<uint8_t> buffer(size);
    size_t written = size;
    state_ops->serialize(&synapse, buffer.data(), &written);

    /* Corrupt the magic number (first 4 bytes of header) */
    buffer[0] = 0xDE;
    buffer[1] = 0xAD;
    buffer[2] = 0xBE;
    buffer[3] = 0xEF;

    stdp_synapse_t restored;
    stdp_synapse_init(&restored);
    int result = state_ops->deserialize(&restored, buffer.data(), written);
    EXPECT_LT(result, 0);
}

//=============================================================================
// Validation Tests
//=============================================================================

TEST_F(StdpStateOpsTest, ValidateValidState) {
    int result = state_ops->validate(&synapse);
    EXPECT_EQ(result, 0);
}

TEST_F(StdpStateOpsTest, ValidateNullState) {
    int result = state_ops->validate(nullptr);
    EXPECT_LT(result, 0);
}

TEST_F(StdpStateOpsTest, ValidateWeightTooHigh) {
    synapse.weight = synapse.w_max + 0.5f;  /* Above max */
    int result = state_ops->validate(&synapse);
    EXPECT_LT(result, 0);
}

TEST_F(StdpStateOpsTest, ValidateWeightTooLow) {
    synapse.weight = synapse.w_min - 0.5f;  /* Below min */
    int result = state_ops->validate(&synapse);
    EXPECT_LT(result, 0);
}

TEST_F(StdpStateOpsTest, ValidateWeightNaN) {
    synapse.weight = NAN;
    int result = state_ops->validate(&synapse);
    EXPECT_LT(result, 0);
}

TEST_F(StdpStateOpsTest, ValidateWeightInf) {
    synapse.weight = INFINITY;
    int result = state_ops->validate(&synapse);
    EXPECT_LT(result, 0);
}

TEST_F(StdpStateOpsTest, ValidateTraceNaN) {
    synapse.pre_trace = NAN;
    int result = state_ops->validate(&synapse);
    EXPECT_LT(result, 0);
}

TEST_F(StdpStateOpsTest, ValidateLearningRateNegative) {
    synapse.learning_rate = -0.01f;
    int result = state_ops->validate(&synapse);
    EXPECT_LT(result, 0);
}

TEST_F(StdpStateOpsTest, ValidateAfterSerializeDeserialize) {
    /* Modify state */
    synapse.weight = 0.6f;
    synapse.pre_trace = 0.1f;

    /* Serialize */
    size_t size = 0;
    state_ops->serialize(&synapse, nullptr, &size);
    std::vector<uint8_t> buffer(size);
    size_t written = size;
    state_ops->serialize(&synapse, buffer.data(), &written);

    /* Deserialize */
    stdp_synapse_t restored;
    stdp_synapse_init(&restored);
    state_ops->deserialize(&restored, buffer.data(), written);

    /* Validate restored state */
    int result = state_ops->validate(&restored);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Reset Tests
//=============================================================================

TEST_F(StdpStateOpsTest, ResetToDefaults) {
    /* Modify state */
    synapse.weight = 0.9f;
    synapse.pre_trace = 0.5f;
    synapse.post_trace = 0.3f;
    synapse.num_potentiation_events = 1000;
    synapse.num_depression_events = 500;

    /* Reset */
    int result = state_ops->reset(&synapse);

    EXPECT_EQ(result, 0);
    /* After reset, traces should be zero and weight in valid range */
    EXPECT_FLOAT_EQ(synapse.pre_trace, 0.0f);
    EXPECT_FLOAT_EQ(synapse.post_trace, 0.0f);
    EXPECT_GE(synapse.weight, synapse.w_min);
    EXPECT_LE(synapse.weight, synapse.w_max);
}

TEST_F(StdpStateOpsTest, ResetNullState) {
    int result = state_ops->reset(nullptr);
    EXPECT_LT(result, 0);
}

TEST_F(StdpStateOpsTest, ResetCorruptedState) {
    /* Corrupt state */
    synapse.weight = INFINITY;
    synapse.pre_trace = NAN;

    /* Reset should fix it */
    int result = state_ops->reset(&synapse);
    EXPECT_EQ(result, 0);

    /* Should now be valid */
    EXPECT_TRUE(std::isfinite(synapse.weight));
    EXPECT_TRUE(std::isfinite(synapse.pre_trace));

    /* Validate should pass */
    result = state_ops->validate(&synapse);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Integration with State Manager Tests
//=============================================================================

TEST_F(StdpStateOpsTest, RegisterWithStateManager) {
    nimcp_state_manager_t* manager = nimcp_state_manager_create();
    ASSERT_NE(manager, nullptr);

    int result = nimcp_state_manager_register(manager, "stdp_synapse", state_ops, &synapse);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(nimcp_state_manager_get_module_count(manager), 1u);

    nimcp_state_manager_destroy(manager);
}

TEST_F(StdpStateOpsTest, CheckpointViaStateManager) {
    nimcp_state_manager_t* manager = nimcp_state_manager_create();
    ASSERT_NE(manager, nullptr);

    nimcp_state_manager_register(manager, "stdp_synapse", state_ops, &synapse);

    /* Modify state */
    synapse.weight = 0.77f;

    /* Checkpoint */
    size_t size = 0;
    nimcp_state_manager_checkpoint_module(manager, "stdp_synapse", nullptr, &size);
    std::vector<uint8_t> buffer(size);
    size_t written = size;
    int result = nimcp_state_manager_checkpoint_module(
        manager, "stdp_synapse", buffer.data(), &written);

    EXPECT_EQ(result, 0);
    EXPECT_GT(written, 0u);

    nimcp_state_manager_destroy(manager);
}

TEST_F(StdpStateOpsTest, RestoreViaStateManager) {
    nimcp_state_manager_t* manager = nimcp_state_manager_create();
    ASSERT_NE(manager, nullptr);

    nimcp_state_manager_register(manager, "stdp_synapse", state_ops, &synapse);

    /* Set known value */
    synapse.weight = 0.88f;

    /* Checkpoint */
    size_t size = 0;
    nimcp_state_manager_checkpoint_module(manager, "stdp_synapse", nullptr, &size);
    std::vector<uint8_t> buffer(size);
    size_t written = size;
    nimcp_state_manager_checkpoint_module(manager, "stdp_synapse", buffer.data(), &written);

    /* Modify state */
    synapse.weight = 0.11f;

    /* Restore */
    int result = nimcp_state_manager_restore_module(
        manager, "stdp_synapse", buffer.data(), written);

    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(synapse.weight, 0.88f);

    nimcp_state_manager_destroy(manager);
}

//=============================================================================
// Integration with Module Recovery Tests
//=============================================================================

TEST_F(StdpStateOpsTest, HealthCheckBuiltin) {
    float health;
    int result = nimcp_stdp_health_check(&synapse, &health);

    EXPECT_EQ(result, 0);
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);
}

TEST_F(StdpStateOpsTest, HealthCheckHealthySynapse) {
    /* Healthy synapse should have high health score */
    float health;
    nimcp_stdp_health_check(&synapse, &health);
    EXPECT_GT(health, 0.8f);
}

TEST_F(StdpStateOpsTest, HealthCheckUnhealthySynapse) {
    /* Corrupt synapse */
    synapse.weight = synapse.w_max * 2.0f;  /* Out of range */

    float health;
    nimcp_stdp_health_check(&synapse, &health);
    EXPECT_LT(health, 0.8f);
}

TEST_F(StdpStateOpsTest, RecoveryLightLevel) {
    synapse.pre_trace = 0.5f;
    synapse.post_trace = 0.3f;
    synapse.weight = 0.7f;

    nimcp_module_recovery_result_t result = nimcp_stdp_recovery(
        &synapse, NIMCP_MODULE_RECOVERY_LIGHT, nullptr);

    EXPECT_EQ(result, NIMCP_MODULE_RECOVERY_SUCCESS);
    EXPECT_FLOAT_EQ(synapse.pre_trace, 0.0f);
    EXPECT_FLOAT_EQ(synapse.post_trace, 0.0f);
    EXPECT_FLOAT_EQ(synapse.weight, 0.7f);  /* Weight preserved */
}

TEST_F(StdpStateOpsTest, RecoveryFullLevel) {
    synapse.pre_trace = 0.5f;
    synapse.post_trace = 0.3f;
    synapse.weight = 0.9f;
    synapse.num_potentiation_events = 1000;

    nimcp_module_recovery_result_t result = nimcp_stdp_recovery(
        &synapse, NIMCP_MODULE_RECOVERY_FULL, nullptr);

    EXPECT_EQ(result, NIMCP_MODULE_RECOVERY_SUCCESS);
    EXPECT_FLOAT_EQ(synapse.pre_trace, 0.0f);
    EXPECT_FLOAT_EQ(synapse.post_trace, 0.0f);
    /* Weight should be reset to midpoint */
    EXPECT_FLOAT_EQ(synapse.weight, 0.5f * synapse.w_max);
}

TEST_F(StdpStateOpsTest, RecoveryIsolateLevel) {
    synapse.weight = 0.8f;
    synapse.learning_rate = 0.01f;

    nimcp_module_recovery_result_t result = nimcp_stdp_recovery(
        &synapse, NIMCP_MODULE_RECOVERY_ISOLATE, nullptr);

    EXPECT_EQ(result, NIMCP_MODULE_RECOVERY_SUCCESS);
    /* Isolated synapse should have zero weight and learning rate */
    EXPECT_FLOAT_EQ(synapse.weight, 0.0f);
    EXPECT_FLOAT_EQ(synapse.learning_rate, 0.0f);
}

TEST_F(StdpStateOpsTest, RecoveryNullState) {
    nimcp_module_recovery_result_t result = nimcp_stdp_recovery(
        nullptr, NIMCP_MODULE_RECOVERY_LIGHT, nullptr);

    EXPECT_EQ(result, NIMCP_MODULE_RECOVERY_FAILED);
}

TEST_F(StdpStateOpsTest, GetRecoveryOps) {
    const nimcp_module_recovery_ops_t* ops = nimcp_stdp_get_recovery_ops();
    ASSERT_NE(ops, nullptr);
    ASSERT_NE(ops->recover, nullptr);
    ASSERT_NE(ops->health_check, nullptr);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(StdpStateOpsTest, SerializeDeserializeMultipleTimes) {
    std::vector<uint8_t> buffer;

    /* Serialize multiple times with different values */
    for (int i = 0; i < 5; i++) {
        synapse.weight = 0.1f * (i + 1);

        size_t size = 0;
        state_ops->serialize(&synapse, nullptr, &size);
        buffer.resize(size);
        size_t written = size;
        int result = state_ops->serialize(&synapse, buffer.data(), &written);
        EXPECT_EQ(result, 0);
    }

    /* Final deserialize */
    stdp_synapse_t restored;
    stdp_synapse_init(&restored);
    state_ops->deserialize(&restored, buffer.data(), buffer.size());

    EXPECT_FLOAT_EQ(restored.weight, 0.5f);  /* Last value */
}

TEST_F(StdpStateOpsTest, ValidateBoundaryValues) {
    /* Test at exact boundaries */
    synapse.weight = synapse.w_min;
    EXPECT_EQ(state_ops->validate(&synapse), 0);

    synapse.weight = synapse.w_max;
    EXPECT_EQ(state_ops->validate(&synapse), 0);

    synapse.weight = (synapse.w_min + synapse.w_max) / 2.0f;
    EXPECT_EQ(state_ops->validate(&synapse), 0);
}

TEST_F(StdpStateOpsTest, ResetPreservesConfiguration) {
    /* Set configuration */
    float original_w_max = synapse.w_max;
    float original_tau_plus = synapse.tau_plus;

    /* Reset */
    state_ops->reset(&synapse);

    /* Configuration should be preserved */
    EXPECT_FLOAT_EQ(synapse.w_max, original_w_max);
    EXPECT_FLOAT_EQ(synapse.tau_plus, original_tau_plus);
}

//=============================================================================
// Main Test Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
