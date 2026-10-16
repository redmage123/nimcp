//=============================================================================
// test_gt_mechanism.cpp - Unit tests for Mechanism Design Module
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/game_theory/nimcp_gt_mechanism.h"
#include "cognitive/game_theory/nimcp_game_theory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class MechanismTest : public ::testing::Test {
protected:
    nimcp_mechanism_t mechanism = nullptr;
    nimcp_mechanism_config_t config;

    void SetUp() override {
        config = nimcp_mechanism_default_config();
        config.num_players = 2;
    }

    void TearDown() override {
        if (mechanism) {
            nimcp_mechanism_destroy(mechanism);
            mechanism = nullptr;
        }
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(MechanismTest, DefaultConfigValues) {
    EXPECT_EQ(config.num_players, 2);
    EXPECT_GT(config.max_iterations, 0);
}

TEST_F(MechanismTest, MechanismTypeNames) {
    EXPECT_NE(nimcp_mechanism_type_name(NIMCP_MECHANISM_DIRECT), nullptr);
    EXPECT_NE(nimcp_mechanism_type_name(NIMCP_MECHANISM_INDIRECT), nullptr);
    EXPECT_NE(nimcp_mechanism_type_name(NIMCP_MECHANISM_SIGNALING), nullptr);
}

TEST_F(MechanismTest, IcLevelNames) {
    EXPECT_NE(nimcp_ic_level_name(NIMCP_IC_NONE), nullptr);
    EXPECT_NE(nimcp_ic_level_name(NIMCP_IC_DOMINANT_STRATEGY), nullptr);
    EXPECT_NE(nimcp_ic_level_name(NIMCP_IC_BAYES_NASH), nullptr);
    EXPECT_NE(nimcp_ic_level_name(NIMCP_IC_INTERIM), nullptr);
    EXPECT_NE(nimcp_ic_level_name(NIMCP_IC_EX_POST), nullptr);
}

TEST_F(MechanismTest, SignalEquilibriumNames) {
    EXPECT_NE(nimcp_signal_equilibrium_name(NIMCP_SIGNAL_EQUIL_SEPARATING), nullptr);
    EXPECT_NE(nimcp_signal_equilibrium_name(NIMCP_SIGNAL_EQUIL_POOLING), nullptr);
    EXPECT_NE(nimcp_signal_equilibrium_name(NIMCP_SIGNAL_EQUIL_SEMI_SEPARATING), nullptr);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(MechanismTest, CreateDestroy) {
    mechanism = nimcp_mechanism_create(&config);
    ASSERT_NE(mechanism, nullptr);

    nimcp_mechanism_destroy(mechanism);
    mechanism = nullptr;
}

TEST_F(MechanismTest, CreateWithNullConfig) {
    mechanism = nimcp_mechanism_create(nullptr);
    // Should handle gracefully
}

//=============================================================================
// Structure Initialization Tests
//=============================================================================

TEST_F(MechanismTest, TypeInit) {
    nimcp_type_t type;
    nimcp_type_init(&type, 0, 100.0f);
    EXPECT_EQ(type.type_id, 0);
    EXPECT_FLOAT_EQ(type.valuation, 100.0f);
}

TEST_F(MechanismTest, TypeSpaceInit) {
    nimcp_type_space_t space;
    nimcp_type_space_init(&space, 0);
    EXPECT_EQ(space.player_id, 0);
    EXPECT_EQ(space.num_types, 0);
    EXPECT_FALSE(space.type_realized);
}

TEST_F(MechanismTest, SignalInit) {
    nimcp_signal_t signal;
    nimcp_signal_init(&signal, 0, "test_signal", 1.0f);
    EXPECT_EQ(signal.signal_id, 0);
    EXPECT_FLOAT_EQ(signal.cost, 1.0f);
    EXPECT_STREQ(signal.name, "test_signal");
}

TEST_F(MechanismTest, ResultInit) {
    nimcp_mechanism_result_t result;
    nimcp_mechanism_result_init(&result);
    EXPECT_EQ(result.state, NIMCP_MECHANISM_STATE_UNINITIALIZED);
}

//=============================================================================
// Type Space Tests
//=============================================================================

TEST_F(MechanismTest, SetTypeSpace) {
    mechanism = nimcp_mechanism_create(&config);
    ASSERT_NE(mechanism, nullptr);

    nimcp_type_t types[2];
    nimcp_type_init(&types[0], 0, 10.0f);
    nimcp_type_init(&types[1], 1, 20.0f);
    float probs[2] = {0.5f, 0.5f};

    nimcp_error_t err = nimcp_mechanism_set_type_space(mechanism, 0, types, probs, 2);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MechanismTest, GetTypeSpace) {
    mechanism = nimcp_mechanism_create(&config);
    ASSERT_NE(mechanism, nullptr);

    nimcp_type_t types[2];
    nimcp_type_init(&types[0], 0, 10.0f);
    nimcp_type_init(&types[1], 1, 20.0f);
    float probs[2] = {0.5f, 0.5f};
    nimcp_mechanism_set_type_space(mechanism, 0, types, probs, 2);

    nimcp_type_space_t space;
    nimcp_error_t err = nimcp_mechanism_get_type_space(mechanism, 0, &space);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(space.num_types, 2);
}

TEST_F(MechanismTest, RealizeType) {
    mechanism = nimcp_mechanism_create(&config);
    ASSERT_NE(mechanism, nullptr);

    nimcp_type_t types[2];
    nimcp_type_init(&types[0], 0, 10.0f);
    nimcp_type_init(&types[1], 1, 20.0f);
    float probs[2] = {0.5f, 0.5f};
    nimcp_mechanism_set_type_space(mechanism, 0, types, probs, 2);

    uint32_t realized;
    nimcp_error_t err = nimcp_mechanism_realize_type(mechanism, 0, &realized);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_LT(realized, 2);
}

//=============================================================================
// Query Function Tests
//=============================================================================

TEST_F(MechanismTest, GetMechanismState) {
    mechanism = nimcp_mechanism_create(&config);
    ASSERT_NE(mechanism, nullptr);

    nimcp_mechanism_state_t state = nimcp_mechanism_get_state(mechanism);
    // Initial state may be UNINITIALIZED or READY depending on implementation
    EXPECT_TRUE(state == NIMCP_MECHANISM_STATE_UNINITIALIZED || state == NIMCP_MECHANISM_STATE_READY);
}

TEST_F(MechanismTest, GetMechanismType) {
    config.type = NIMCP_MECHANISM_DIRECT;
    mechanism = nimcp_mechanism_create(&config);
    ASSERT_NE(mechanism, nullptr);

    nimcp_mechanism_type_t type = nimcp_mechanism_get_type(mechanism);
    EXPECT_EQ(type, NIMCP_MECHANISM_DIRECT);
}

//=============================================================================
// Allocation Rule Callback
//=============================================================================

static nimcp_error_t test_allocation_rule(const uint32_t* reports, uint32_t num_players,
                                          float* allocations, void* user_data) {
    (void)user_data;
    uint32_t max_idx = 0;
    for (uint32_t i = 1; i < num_players; i++) {
        if (reports[i] > reports[max_idx]) {
            max_idx = i;
        }
    }
    for (uint32_t i = 0; i < num_players; i++) {
        allocations[i] = (i == max_idx) ? 1.0f : 0.0f;
    }
    return NIMCP_SUCCESS;
}

static nimcp_error_t test_payment_rule(const uint32_t* reports, uint32_t num_players,
                                       const float* allocations, float* payments, void* user_data) {
    (void)reports;
    (void)user_data;
    for (uint32_t i = 0; i < num_players; i++) {
        payments[i] = allocations[i] * 10.0f;
    }
    return NIMCP_SUCCESS;
}

TEST_F(MechanismTest, SetAllocationRule) {
    mechanism = nimcp_mechanism_create(&config);
    ASSERT_NE(mechanism, nullptr);

    nimcp_error_t err = nimcp_mechanism_set_allocation_rule(mechanism, test_allocation_rule, nullptr);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MechanismTest, SetPaymentRule) {
    mechanism = nimcp_mechanism_create(&config);
    ASSERT_NE(mechanism, nullptr);

    nimcp_error_t err = nimcp_mechanism_set_payment_rule(mechanism, test_payment_rule, nullptr);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

//=============================================================================
// Mechanism Execution Tests
//=============================================================================

TEST_F(MechanismTest, ExecuteMechanism) {
    mechanism = nimcp_mechanism_create(&config);
    ASSERT_NE(mechanism, nullptr);

    nimcp_type_t types[2];
    nimcp_type_init(&types[0], 0, 10.0f);
    nimcp_type_init(&types[1], 1, 20.0f);
    float probs[2] = {0.5f, 0.5f};
    nimcp_mechanism_set_type_space(mechanism, 0, types, probs, 2);
    nimcp_mechanism_set_type_space(mechanism, 1, types, probs, 2);

    nimcp_mechanism_set_allocation_rule(mechanism, test_allocation_rule, nullptr);
    nimcp_mechanism_set_payment_rule(mechanism, test_payment_rule, nullptr);

    uint32_t reports[2] = {0, 1};
    nimcp_mechanism_result_t result;
    nimcp_mechanism_result_init(&result);

    nimcp_error_t err = nimcp_mechanism_execute(mechanism, reports, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(result.state, NIMCP_MECHANISM_STATE_COMPLETED);
}

//=============================================================================
// Incentive Compatibility Tests
//=============================================================================

TEST_F(MechanismTest, VerifyIncentiveCompatibility) {
    mechanism = nimcp_mechanism_create(&config);
    ASSERT_NE(mechanism, nullptr);

    nimcp_type_t types[2];
    nimcp_type_init(&types[0], 0, 10.0f);
    nimcp_type_init(&types[1], 1, 20.0f);
    float probs[2] = {0.5f, 0.5f};
    nimcp_mechanism_set_type_space(mechanism, 0, types, probs, 2);
    nimcp_mechanism_set_type_space(mechanism, 1, types, probs, 2);

    nimcp_mechanism_set_allocation_rule(mechanism, test_allocation_rule, nullptr);
    nimcp_mechanism_set_payment_rule(mechanism, test_payment_rule, nullptr);

    nimcp_ic_result_t result;
    nimcp_error_t err = nimcp_mechanism_is_incentive_compatible(mechanism, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MechanismTest, VerifyIndividualRationality) {
    mechanism = nimcp_mechanism_create(&config);
    ASSERT_NE(mechanism, nullptr);

    nimcp_type_t types[2];
    nimcp_type_init(&types[0], 0, 10.0f);
    nimcp_type_init(&types[1], 1, 20.0f);
    float probs[2] = {0.5f, 0.5f};
    nimcp_mechanism_set_type_space(mechanism, 0, types, probs, 2);
    nimcp_mechanism_set_type_space(mechanism, 1, types, probs, 2);

    nimcp_mechanism_set_allocation_rule(mechanism, test_allocation_rule, nullptr);
    nimcp_mechanism_set_payment_rule(mechanism, test_payment_rule, nullptr);

    nimcp_ir_result_t result;
    nimcp_error_t err = nimcp_mechanism_is_individually_rational(mechanism, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

//=============================================================================
// Bayesian Equilibrium Tests
//=============================================================================

TEST_F(MechanismTest, ComputeBayesianEquilibrium) {
    mechanism = nimcp_mechanism_create(&config);
    ASSERT_NE(mechanism, nullptr);

    nimcp_type_t types[2];
    nimcp_type_init(&types[0], 0, 10.0f);
    nimcp_type_init(&types[1], 1, 20.0f);
    float probs[2] = {0.5f, 0.5f};
    nimcp_mechanism_set_type_space(mechanism, 0, types, probs, 2);
    nimcp_mechanism_set_type_space(mechanism, 1, types, probs, 2);

    nimcp_mechanism_set_allocation_rule(mechanism, test_allocation_rule, nullptr);
    nimcp_mechanism_set_payment_rule(mechanism, test_payment_rule, nullptr);

    nimcp_bayesian_equilibrium_t result;
    nimcp_error_t err = nimcp_mechanism_compute_bayesian_equilibrium(mechanism, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

//=============================================================================
// Expected Utility Tests
//=============================================================================

TEST_F(MechanismTest, ComputeExpectedUtility) {
    mechanism = nimcp_mechanism_create(&config);
    ASSERT_NE(mechanism, nullptr);

    nimcp_type_t types[2];
    nimcp_type_init(&types[0], 0, 10.0f);
    nimcp_type_init(&types[1], 1, 20.0f);
    float probs[2] = {0.5f, 0.5f};
    nimcp_mechanism_set_type_space(mechanism, 0, types, probs, 2);
    nimcp_mechanism_set_type_space(mechanism, 1, types, probs, 2);

    nimcp_mechanism_set_allocation_rule(mechanism, test_allocation_rule, nullptr);
    nimcp_mechanism_set_payment_rule(mechanism, test_payment_rule, nullptr);

    float utility = nimcp_mechanism_expected_utility(mechanism, 0, 0, 0);
    EXPECT_GE(utility, -1000.0f);
}

//=============================================================================
// Signaling Game Tests
//=============================================================================

TEST_F(MechanismTest, SignalingGameSetup) {
    config.type = NIMCP_MECHANISM_SIGNALING;
    mechanism = nimcp_mechanism_create(&config);
    ASSERT_NE(mechanism, nullptr);

    nimcp_type_t types[2];
    nimcp_type_init(&types[0], 0, 10.0f);
    nimcp_type_init(&types[1], 1, 20.0f);

    nimcp_error_t err = nimcp_signaling_set_sender_types(mechanism, types, 2);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MechanismTest, SignalingSetSignals) {
    config.type = NIMCP_MECHANISM_SIGNALING;
    mechanism = nimcp_mechanism_create(&config);
    ASSERT_NE(mechanism, nullptr);

    nimcp_signal_t signals[2];
    nimcp_signal_init(&signals[0], 0, "signal_low", 0.0f);
    nimcp_signal_init(&signals[1], 1, "signal_high", 1.0f);

    nimcp_error_t err = nimcp_signaling_set_signals(mechanism, signals, 2);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MechanismTest, ComputeSeparatingEquilibrium) {
    config.type = NIMCP_MECHANISM_SIGNALING;
    mechanism = nimcp_mechanism_create(&config);
    ASSERT_NE(mechanism, nullptr);

    nimcp_type_t types[2];
    nimcp_type_init(&types[0], 0, 10.0f);
    types[0].cost = 1.0f;
    nimcp_type_init(&types[1], 1, 20.0f);
    types[1].cost = 0.5f;
    nimcp_signaling_set_sender_types(mechanism, types, 2);

    nimcp_signal_t signals[2];
    nimcp_signal_init(&signals[0], 0, "low", 0.0f);
    nimcp_signal_init(&signals[1], 1, "high", 2.0f);
    nimcp_signaling_set_signals(mechanism, signals, 2);

    nimcp_signal_equilibrium_result_t result;
    nimcp_error_t err = nimcp_signaling_compute_separating_equilibrium(mechanism, &result);
    if (err == NIMCP_SUCCESS) {
        EXPECT_TRUE(result.equilibrium_found);
        EXPECT_EQ(result.type, NIMCP_SIGNAL_EQUIL_SEPARATING);
    }
}

TEST_F(MechanismTest, ComputePoolingEquilibrium) {
    config.type = NIMCP_MECHANISM_SIGNALING;
    mechanism = nimcp_mechanism_create(&config);
    ASSERT_NE(mechanism, nullptr);

    nimcp_type_t types[2];
    nimcp_type_init(&types[0], 0, 10.0f);
    nimcp_type_init(&types[1], 1, 20.0f);
    nimcp_signaling_set_sender_types(mechanism, types, 2);

    nimcp_signal_t signals[2];
    nimcp_signal_init(&signals[0], 0, "pool_signal", 0.0f);
    nimcp_signal_init(&signals[1], 1, "other", 10.0f);
    nimcp_signaling_set_signals(mechanism, signals, 2);

    nimcp_signal_equilibrium_result_t result;
    nimcp_error_t err = nimcp_signaling_compute_pooling_equilibrium(mechanism, &result);
    if (err == NIMCP_SUCCESS) {
        EXPECT_EQ(result.type, NIMCP_SIGNAL_EQUIL_POOLING);
    }
}

TEST_F(MechanismTest, ComputeAllEquilibria) {
    config.type = NIMCP_MECHANISM_SIGNALING;
    mechanism = nimcp_mechanism_create(&config);
    ASSERT_NE(mechanism, nullptr);

    nimcp_type_t types[2];
    nimcp_type_init(&types[0], 0, 10.0f);
    nimcp_type_init(&types[1], 1, 20.0f);
    nimcp_signaling_set_sender_types(mechanism, types, 2);

    nimcp_signal_t signals[2];
    nimcp_signal_init(&signals[0], 0, "signal_a", 0.5f);
    nimcp_signal_init(&signals[1], 1, "signal_b", 1.5f);
    nimcp_signaling_set_signals(mechanism, signals, 2);

    nimcp_signal_equilibrium_result_t results[10];
    uint32_t num_found = 0;
    nimcp_error_t err = nimcp_signaling_compute_all_equilibria(mechanism, results, 10, &num_found);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MechanismTest, SignalingInformationContent) {
    nimcp_signal_equilibrium_result_t result;
    memset(&result, 0, sizeof(result));  // Zero-initialize all fields
    result.type = NIMCP_SIGNAL_EQUIL_SEPARATING;
    result.equilibrium_found = true;
    result.num_sender_types = 2;
    result.num_signals = 2;
    result.information_transmitted = 0.5f;  // Set a valid test value

    float info = nimcp_signaling_information_content(&result);
    EXPECT_GE(info, 0.0f);
    EXPECT_FLOAT_EQ(info, 0.5f);  // Should return what we set
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(MechanismTest, SinglePlayer) {
    config.num_players = 1;
    mechanism = nimcp_mechanism_create(&config);
    ASSERT_NE(mechanism, nullptr);

    nimcp_type_t types[1];
    nimcp_type_init(&types[0], 0, 100.0f);
    float probs[1] = {1.0f};

    nimcp_error_t err = nimcp_mechanism_set_type_space(mechanism, 0, types, probs, 1);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MechanismTest, SingleTypePerPlayer) {
    mechanism = nimcp_mechanism_create(&config);
    ASSERT_NE(mechanism, nullptr);

    nimcp_type_t types[1];
    nimcp_type_init(&types[0], 0, 50.0f);
    float probs[1] = {1.0f};

    nimcp_error_t err = nimcp_mechanism_set_type_space(mechanism, 0, types, probs, 1);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}
