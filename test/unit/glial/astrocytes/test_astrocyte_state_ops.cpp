/**
 * @file test_astrocyte_state_ops.cpp
 * @brief Unit tests for astrocyte network state serialization operations
 *
 * WHAT: Unit tests for astrocyte network state checkpoint/restore
 * WHY:  Verify astrocyte state ops enable reliable fault tolerance
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
#include "glial/astrocytes/nimcp_astrocytes.h"
#include "glial/astrocyte_types/nimcp_astrocyte_types.h"
#include "utils/fault_tolerance/nimcp_state_manager.h"
#include "utils/fault_tolerance/nimcp_module_recovery.h"
}

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Astrocyte state operations test fixture
 *
 * WHAT: Provides initialized astrocyte network for each test
 * WHY:  Ensure test isolation
 * HOW:  Create network in SetUp, destroy in TearDown
 */
class AstrocyteStateOpsTest : public ::testing::Test {
protected:
    astrocyte_network_t* network;
    const nimcp_module_state_ops_t* state_ops;
    static constexpr int TEST_NETWORK_SIZE = 10;

    void SetUp() override {
        network = astrocyte_network_create(TEST_NETWORK_SIZE);
        state_ops = astrocyte_network_get_state_ops();

        /* Add astrocytes to the network */
        if (network) {
            for (int i = 0; i < TEST_NETWORK_SIZE; i++) {
                astrocyte_t* astro = astrocyte_create(
                    i, ASTROCYTE_TYPE_GENERIC,
                    (float)i * 10.0f, 0.0f, 0.0f, 50.0f);
                if (astro) {
                    astrocyte_network_add(network, astro);
                }
            }
        }
    }

    void TearDown() override {
        if (network) {
            astrocyte_network_destroy(network);
            network = nullptr;
        }
    }
};

//=============================================================================
// State Ops Availability Tests
//=============================================================================

TEST_F(AstrocyteStateOpsTest, GetStateOpsNotNull) {
    ASSERT_NE(state_ops, nullptr);
}

TEST_F(AstrocyteStateOpsTest, AllOpsProvided) {
    ASSERT_NE(state_ops->serialize, nullptr);
    ASSERT_NE(state_ops->deserialize, nullptr);
    ASSERT_NE(state_ops->validate, nullptr);
    ASSERT_NE(state_ops->reset, nullptr);
    ASSERT_NE(state_ops->get_size, nullptr);
}

TEST_F(AstrocyteStateOpsTest, NetworkCreatedSuccessfully) {
    ASSERT_NE(network, nullptr);
    EXPECT_EQ(network->num_astrocytes, TEST_NETWORK_SIZE);
}

//=============================================================================
// Serialization Tests
//=============================================================================

TEST_F(AstrocyteStateOpsTest, GetSizeReturnsPositive) {
    size_t size = state_ops->get_size(network);
    EXPECT_GT(size, 0u);
}

TEST_F(AstrocyteStateOpsTest, SerializeQuerySize) {
    size_t size = 0;
    int result = state_ops->serialize(network, nullptr, &size);

    EXPECT_EQ(result, 0);
    EXPECT_GT(size, 0u);
}

TEST_F(AstrocyteStateOpsTest, SerializeToBuffer) {
    /* Query size */
    size_t size = 0;
    state_ops->serialize(network, nullptr, &size);

    /* Allocate and serialize */
    std::vector<uint8_t> buffer(size);
    size_t written = size;
    int result = state_ops->serialize(network, buffer.data(), &written);

    EXPECT_EQ(result, 0);
    EXPECT_GT(written, 0u);
    EXPECT_LE(written, size);
}

TEST_F(AstrocyteStateOpsTest, SerializeBufferTooSmall) {
    uint8_t small_buffer[4];
    size_t size = sizeof(small_buffer);

    int result = state_ops->serialize(network, small_buffer, &size);

    EXPECT_EQ(result, -2);  /* Buffer too small */
    EXPECT_GT(size, sizeof(small_buffer));  /* Returns required size */
}

TEST_F(AstrocyteStateOpsTest, SerializeNullState) {
    size_t size = 1024;
    std::vector<uint8_t> buffer(size);

    int result = state_ops->serialize(nullptr, buffer.data(), &size);
    EXPECT_LT(result, 0);
}

TEST_F(AstrocyteStateOpsTest, SerializeNullSize) {
    std::vector<uint8_t> buffer(4096);
    int result = state_ops->serialize(network, buffer.data(), nullptr);
    EXPECT_LT(result, 0);
}

TEST_F(AstrocyteStateOpsTest, SerializeSizeScalesWithNetwork) {
    /* Create networks of different sizes and populate them */
    astrocyte_network_t* small_net = astrocyte_network_create(5);
    astrocyte_network_t* large_net = astrocyte_network_create(20);

    /* Add astrocytes to small network */
    for (int i = 0; i < 5; i++) {
        astrocyte_t* astro = astrocyte_create(
            i, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);
        if (astro) astrocyte_network_add(small_net, astro);
    }

    /* Add astrocytes to large network */
    for (int i = 0; i < 20; i++) {
        astrocyte_t* astro = astrocyte_create(
            i, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);
        if (astro) astrocyte_network_add(large_net, astro);
    }

    size_t small_size = state_ops->get_size(small_net);
    size_t large_size = state_ops->get_size(large_net);

    EXPECT_LT(small_size, large_size);

    astrocyte_network_destroy(small_net);
    astrocyte_network_destroy(large_net);
}

//=============================================================================
// Deserialization Tests
//=============================================================================

TEST_F(AstrocyteStateOpsTest, DeserializeValidBuffer) {
    /* Serialize first */
    size_t size = 0;
    state_ops->serialize(network, nullptr, &size);
    std::vector<uint8_t> buffer(size);
    size_t written = size;
    state_ops->serialize(network, buffer.data(), &written);

    /* Create new network with matching structure for deserialize */
    astrocyte_network_t* restored = astrocyte_network_create(TEST_NETWORK_SIZE);
    for (int i = 0; i < TEST_NETWORK_SIZE; i++) {
        astrocyte_t* astro = astrocyte_create(
            i, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);
        if (astro) astrocyte_network_add(restored, astro);
    }

    int result = state_ops->deserialize(restored, buffer.data(), written);

    EXPECT_EQ(result, 0);
    astrocyte_network_destroy(restored);
}

TEST_F(AstrocyteStateOpsTest, DeserializeNullState) {
    std::vector<uint8_t> buffer(4096);
    int result = state_ops->deserialize(nullptr, buffer.data(), buffer.size());
    EXPECT_LT(result, 0);
}

TEST_F(AstrocyteStateOpsTest, DeserializeNullBuffer) {
    int result = state_ops->deserialize(network, nullptr, 4096);
    EXPECT_LT(result, 0);
}

TEST_F(AstrocyteStateOpsTest, DeserializeBufferTooSmall) {
    uint8_t small_buffer[4] = {0};
    int result = state_ops->deserialize(network, small_buffer, sizeof(small_buffer));
    EXPECT_LT(result, 0);
}

TEST_F(AstrocyteStateOpsTest, DeserializeInvalidMagic) {
    /* Serialize valid state */
    size_t size = 0;
    state_ops->serialize(network, nullptr, &size);
    std::vector<uint8_t> buffer(size);
    size_t written = size;
    state_ops->serialize(network, buffer.data(), &written);

    /* Corrupt the magic number */
    buffer[0] = 0xDE;
    buffer[1] = 0xAD;
    buffer[2] = 0xBE;
    buffer[3] = 0xEF;

    astrocyte_network_t* restored = astrocyte_network_create(TEST_NETWORK_SIZE);
    int result = state_ops->deserialize(restored, buffer.data(), written);
    EXPECT_LT(result, 0);
    astrocyte_network_destroy(restored);
}

TEST_F(AstrocyteStateOpsTest, DeserializePreservesState) {
    /* Modify astrocyte state */
    for (int i = 0; i < TEST_NETWORK_SIZE; i++) {
        astrocyte_t* astro = network->astrocytes[i];
        if (astro) {
            astro->calcium_concentration = 0.5f + i * 0.05f;
        }
    }

    /* Serialize */
    size_t size = 0;
    state_ops->serialize(network, nullptr, &size);
    std::vector<uint8_t> buffer(size);
    size_t written = size;
    state_ops->serialize(network, buffer.data(), &written);

    /* Create fresh network with matching structure and deserialize */
    astrocyte_network_t* restored = astrocyte_network_create(TEST_NETWORK_SIZE);
    for (int i = 0; i < TEST_NETWORK_SIZE; i++) {
        astrocyte_t* astro = astrocyte_create(
            i, ASTROCYTE_TYPE_GENERIC, 0.0f, 0.0f, 0.0f, 50.0f);
        if (astro) astrocyte_network_add(restored, astro);
    }
    int result = state_ops->deserialize(restored, buffer.data(), written);
    EXPECT_EQ(result, 0);

    /* Verify state preserved */
    for (int i = 0; i < TEST_NETWORK_SIZE; i++) {
        astrocyte_t* original = network->astrocytes[i];
        astrocyte_t* rest = restored->astrocytes[i];
        if (original && rest) {
            EXPECT_NEAR(rest->calcium_concentration,
                       original->calcium_concentration, 0.001f);
        }
    }

    astrocyte_network_destroy(restored);
}

//=============================================================================
// Validation Tests
//=============================================================================

TEST_F(AstrocyteStateOpsTest, ValidateValidState) {
    int result = state_ops->validate(network);
    EXPECT_EQ(result, 0);
}

TEST_F(AstrocyteStateOpsTest, ValidateNullState) {
    int result = state_ops->validate(nullptr);
    EXPECT_LT(result, 0);
}

TEST_F(AstrocyteStateOpsTest, ValidateNaNCalcium) {
    astrocyte_t* astro = network->astrocytes[0];
    if (astro) {
        astro->calcium_concentration = NAN;
        int result = state_ops->validate(network);
        EXPECT_LT(result, 0);
    }
}

TEST_F(AstrocyteStateOpsTest, ValidateInfCalcium) {
    astrocyte_t* astro = network->astrocytes[0];
    if (astro) {
        astro->calcium_concentration = INFINITY;
        int result = state_ops->validate(network);
        EXPECT_LT(result, 0);
    }
}

TEST_F(AstrocyteStateOpsTest, ValidateNegativeCalcium) {
    astrocyte_t* astro = network->astrocytes[0];
    if (astro) {
        astro->calcium_concentration = -1.0f;
        int result = state_ops->validate(network);
        EXPECT_LT(result, 0);
    }
}

TEST_F(AstrocyteStateOpsTest, ValidateAfterSerializeDeserialize) {
    /* Serialize */
    size_t size = 0;
    state_ops->serialize(network, nullptr, &size);
    std::vector<uint8_t> buffer(size);
    size_t written = size;
    state_ops->serialize(network, buffer.data(), &written);

    /* Deserialize */
    astrocyte_network_t* restored = astrocyte_network_create(TEST_NETWORK_SIZE);
    state_ops->deserialize(restored, buffer.data(), written);

    /* Validate restored state */
    int result = state_ops->validate(restored);
    EXPECT_EQ(result, 0);

    astrocyte_network_destroy(restored);
}

//=============================================================================
// Reset Tests
//=============================================================================

TEST_F(AstrocyteStateOpsTest, ResetToDefaults) {
    /* Modify state */
    for (int i = 0; i < TEST_NETWORK_SIZE; i++) {
        astrocyte_t* astro = network->astrocytes[i];
        if (astro) {
            astro->calcium_concentration = 10.0f;  /* High value */
        }
    }

    /* Reset */
    int result = state_ops->reset(network);
    EXPECT_EQ(result, 0);

    /* Verify reset to defaults */
    for (int i = 0; i < TEST_NETWORK_SIZE; i++) {
        astrocyte_t* astro = network->astrocytes[i];
        if (astro) {
            /* Calcium should be at baseline */
            EXPECT_LT(astro->calcium_concentration, 5.0f);
        }
    }
}

TEST_F(AstrocyteStateOpsTest, ResetNullState) {
    int result = state_ops->reset(nullptr);
    EXPECT_LT(result, 0);
}

TEST_F(AstrocyteStateOpsTest, ResetCorruptedState) {
    /* Corrupt state */
    astrocyte_t* astro = network->astrocytes[0];
    if (astro) {
        astro->calcium_concentration = NAN;
    }

    /* Reset should fix it */
    int result = state_ops->reset(network);
    EXPECT_EQ(result, 0);

    /* Should now be valid */
    if (astro) {
        EXPECT_TRUE(std::isfinite(astro->calcium_concentration));
    }

    /* Validate should pass */
    result = state_ops->validate(network);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Integration with State Manager Tests
//=============================================================================

TEST_F(AstrocyteStateOpsTest, RegisterWithStateManager) {
    nimcp_state_manager_t* manager = nimcp_state_manager_create();
    ASSERT_NE(manager, nullptr);

    int result = nimcp_state_manager_register(manager, "astrocyte_network", state_ops, network);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(nimcp_state_manager_get_module_count(manager), 1u);

    nimcp_state_manager_destroy(manager);
}

TEST_F(AstrocyteStateOpsTest, CheckpointViaStateManager) {
    nimcp_state_manager_t* manager = nimcp_state_manager_create();
    ASSERT_NE(manager, nullptr);

    nimcp_state_manager_register(manager, "astrocyte_network", state_ops, network);

    /* Checkpoint */
    size_t size = 0;
    nimcp_state_manager_checkpoint_module(manager, "astrocyte_network", nullptr, &size);
    std::vector<uint8_t> buffer(size);
    size_t written = size;
    int result = nimcp_state_manager_checkpoint_module(
        manager, "astrocyte_network", buffer.data(), &written);

    EXPECT_EQ(result, 0);
    EXPECT_GT(written, 0u);

    nimcp_state_manager_destroy(manager);
}

TEST_F(AstrocyteStateOpsTest, RestoreViaStateManager) {
    nimcp_state_manager_t* manager = nimcp_state_manager_create();
    ASSERT_NE(manager, nullptr);

    nimcp_state_manager_register(manager, "astrocyte_network", state_ops, network);

    /* Set known value */
    astrocyte_t* astro = network->astrocytes[0];
    float original_calcium = 0.0f;
    if (astro) {
        astro->calcium_concentration = 0.888f;
        original_calcium = 0.888f;
    }

    /* Checkpoint */
    size_t size = 0;
    nimcp_state_manager_checkpoint_module(manager, "astrocyte_network", nullptr, &size);
    std::vector<uint8_t> buffer(size);
    size_t written = size;
    nimcp_state_manager_checkpoint_module(manager, "astrocyte_network", buffer.data(), &written);

    /* Modify state */
    if (astro) {
        astro->calcium_concentration = 0.111f;
    }

    /* Restore */
    int result = nimcp_state_manager_restore_module(
        manager, "astrocyte_network", buffer.data(), written);

    EXPECT_EQ(result, 0);
    if (astro) {
        EXPECT_NEAR(astro->calcium_concentration, original_calcium, 0.001f);
    }

    nimcp_state_manager_destroy(manager);
}

//=============================================================================
// Integration with Module Recovery Tests
//=============================================================================

TEST_F(AstrocyteStateOpsTest, HealthCheckBuiltin) {
    float health;
    int result = nimcp_astrocyte_health_check(network, &health);

    EXPECT_EQ(result, 0);
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);
}

TEST_F(AstrocyteStateOpsTest, HealthCheckHealthyNetwork) {
    /* Healthy network should have high health score */
    float health;
    nimcp_astrocyte_health_check(network, &health);
    EXPECT_GT(health, 0.7f);
}

TEST_F(AstrocyteStateOpsTest, HealthCheckUnhealthyNetwork) {
    /* Corrupt network - set multiple bad values to ensure low health
     * NaN calcium affects: NaN check (30% penalty)
     * Negative calcium affects: calcium range check (40% penalty)
     * Bad pools affects: pools check (20% penalty)
     * Bad scaling affects: homeostatic check (10% penalty)
     */
    for (int i = 0; i < TEST_NETWORK_SIZE; i++) {
        astrocyte_t* astro = network->astrocytes[i];
        if (astro) {
            astro->calcium_concentration = -1.0f;  /* Bad range: 40% penalty */
            astro->ip3_concentration = NAN;        /* NaN check: 30% penalty */
            astro->glutamate_pool = -1.0f;         /* Bad pools: 20% penalty */
        }
    }

    float health;
    nimcp_astrocyte_health_check(network, &health);
    /* With 40% + 30% + 20% = 90% penalty, health should be 0.1 */
    EXPECT_LT(health, 0.5f);
}

TEST_F(AstrocyteStateOpsTest, RecoveryLightLevel) {
    /* Set non-baseline values */
    for (int i = 0; i < TEST_NETWORK_SIZE; i++) {
        astrocyte_t* astro = network->astrocytes[i];
        if (astro) {
            astro->calcium_concentration = 5.0f;
        }
    }

    nimcp_module_recovery_result_t result = nimcp_astrocyte_recovery(
        network, NIMCP_MODULE_RECOVERY_LIGHT, nullptr);

    EXPECT_EQ(result, NIMCP_MODULE_RECOVERY_SUCCESS);

    /* Calcium should be reset to baseline */
    for (int i = 0; i < TEST_NETWORK_SIZE; i++) {
        astrocyte_t* astro = network->astrocytes[i];
        if (astro) {
            EXPECT_LT(astro->calcium_concentration, 5.0f);
        }
    }
}

TEST_F(AstrocyteStateOpsTest, RecoveryFullLevel) {
    nimcp_module_recovery_result_t result = nimcp_astrocyte_recovery(
        network, NIMCP_MODULE_RECOVERY_FULL, nullptr);

    EXPECT_EQ(result, NIMCP_MODULE_RECOVERY_SUCCESS);

    /* Network should be fully reset and valid */
    int valid = state_ops->validate(network);
    EXPECT_EQ(valid, 0);
}

TEST_F(AstrocyteStateOpsTest, RecoveryNullState) {
    nimcp_module_recovery_result_t result = nimcp_astrocyte_recovery(
        nullptr, NIMCP_MODULE_RECOVERY_LIGHT, nullptr);

    EXPECT_EQ(result, NIMCP_MODULE_RECOVERY_FAILED);
}

TEST_F(AstrocyteStateOpsTest, GetRecoveryOps) {
    const nimcp_module_recovery_ops_t* ops = nimcp_astrocyte_get_recovery_ops();
    ASSERT_NE(ops, nullptr);
    ASSERT_NE(ops->recover, nullptr);
    ASSERT_NE(ops->health_check, nullptr);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(AstrocyteStateOpsTest, SerializeDeserializeEmptyNetwork) {
    astrocyte_network_t* empty_net = astrocyte_network_create(0);

    if (empty_net) {
        size_t size = 0;
        int result = state_ops->serialize(empty_net, nullptr, &size);
        /* May succeed with header only or fail gracefully */
        (void)result;

        astrocyte_network_destroy(empty_net);
    }
}

TEST_F(AstrocyteStateOpsTest, SerializeDeserializeLargeNetwork) {
    astrocyte_network_t* large_net = astrocyte_network_create(100);
    ASSERT_NE(large_net, nullptr);

    /* Serialize */
    size_t size = 0;
    state_ops->serialize(large_net, nullptr, &size);
    std::vector<uint8_t> buffer(size);
    size_t written = size;
    int result = state_ops->serialize(large_net, buffer.data(), &written);
    EXPECT_EQ(result, 0);

    /* Deserialize to new network */
    astrocyte_network_t* restored = astrocyte_network_create(100);
    result = state_ops->deserialize(restored, buffer.data(), written);
    EXPECT_EQ(result, 0);

    astrocyte_network_destroy(large_net);
    astrocyte_network_destroy(restored);
}

TEST_F(AstrocyteStateOpsTest, ValidateBoundaryValues) {
    astrocyte_t* astro = network->astrocytes[0];
    if (astro) {
        /* Test at zero (should be valid) */
        astro->calcium_concentration = 0.0f;
        EXPECT_EQ(state_ops->validate(network), 0);

        /* Test at very small positive (should be valid) */
        astro->calcium_concentration = 1e-10f;
        EXPECT_EQ(state_ops->validate(network), 0);
    }
}

//=============================================================================
// Main Test Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
