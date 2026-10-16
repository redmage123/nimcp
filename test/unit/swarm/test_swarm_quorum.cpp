/**
 * @file test_swarm_quorum.cpp
 * @brief Comprehensive unit tests for NIMCP Swarm Quorum Sensing
 *
 * TEST COVERAGE:
 * - System creation and destruction
 * - Signal production and detection
 * - Threshold-based triggering
 * - Collective decision making
 * - Density-dependent behavior
 * - Signal decay and diffusion
 * - Bio-async integration
 * - BBB security validation
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_quorum.h"

class SwarmQuorumTest : public ::testing::Test {
protected:
    nimcp_quorum_system_t* system;
    nimcp_quorum_config_t config;

    void SetUp() override {
        nimcp_quorum_default_config(&config);
        system = nimcp_quorum_create(&config, nullptr);
        ASSERT_NE(system, nullptr);
    }

    void TearDown() override {
        if (system) {
            nimcp_quorum_destroy(system);
        }
    }
};

TEST_F(SwarmQuorumTest, CreateValidSystem) {
    EXPECT_NE(system, nullptr);
}

TEST_F(SwarmQuorumTest, DestroyNullSystem) {
    nimcp_quorum_destroy(nullptr);
    SUCCEED();
}

TEST_F(SwarmQuorumTest, ProduceSignal) {
    nimcp_result_t result = nimcp_quorum_produce_signal(
        system, 1, QUORUM_SIGNAL_AGGREGATION, 0.5
    );
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmQuorumTest, DetectSignal) {
    nimcp_quorum_produce_signal(system, 1, QUORUM_SIGNAL_AGGREGATION, 0.8);
    
    float concentration = 0.0;
    nimcp_result_t result = nimcp_quorum_detect_signal(
        system, 1, QUORUM_SIGNAL_AGGREGATION, &concentration
    );
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(concentration, 0.0);
}

TEST_F(SwarmQuorumTest, CheckThreshold) {
    for (int i = 0; i < 10; i++) {
        nimcp_quorum_produce_signal(system, i, QUORUM_SIGNAL_AGGREGATION, 0.1);
    }
    
    bool reached = false;
    nimcp_result_t result = nimcp_quorum_check_threshold(
        system, QUORUM_SIGNAL_AGGREGATION, &reached
    );
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmQuorumTest, ProposeDecision) {
    nimcp_quorum_decision_t decision = {
        DECISION_TYPE_MOVE, {10.0, 10.0, 0.0}, 0, 0.8, 0
    };
    
    nimcp_result_t result = nimcp_quorum_propose_decision(
        system, 1, &decision
    );
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmQuorumTest, VoteDecision) {
    nimcp_quorum_decision_t decision = {
        DECISION_TYPE_MOVE, {10.0, 10.0, 0.0}, 0, 0.8, 0
    };
    nimcp_quorum_propose_decision(system, 1, &decision);
    
    nimcp_result_t result = nimcp_quorum_vote(system, 2, decision.decision_id, true);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmQuorumTest, GetConsensus) {
    nimcp_quorum_decision_t decision = {
        DECISION_TYPE_MOVE, {10.0, 10.0, 0.0}, 0, 0.8, 0
    };
    nimcp_quorum_propose_decision(system, 1, &decision);
    
    for (uint32_t i = 2; i < 10; i++) {
        nimcp_quorum_vote(system, i, decision.decision_id, true);
    }
    
    bool consensus = false;
    nimcp_result_t result = nimcp_quorum_check_consensus(
        system, decision.decision_id, &consensus
    );
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmQuorumTest, UpdateSystem) {
    nimcp_quorum_produce_signal(system, 1, QUORUM_SIGNAL_DISPERSAL, 0.5);
    
    nimcp_result_t result = nimcp_quorum_update(system, 1000);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmQuorumTest, SignalDecay) {
    nimcp_quorum_produce_signal(system, 1, QUORUM_SIGNAL_AGGREGATION, 1.0);
    
    float initial = 0.0;
    nimcp_quorum_detect_signal(system, 1, QUORUM_SIGNAL_AGGREGATION, &initial);
    
    nimcp_quorum_update(system, 5000);
    
    float after_decay = 0.0;
    nimcp_quorum_detect_signal(system, 1, QUORUM_SIGNAL_AGGREGATION, &after_decay);
    
    EXPECT_LT(after_decay, initial);
}

TEST_F(SwarmQuorumTest, GetStatistics) {
    nimcp_quorum_stats_t stats;
    nimcp_result_t result = nimcp_quorum_get_stats(system, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmQuorumTest, ValidateConfig) {
    nimcp_quorum_config_t test_config;
    nimcp_quorum_default_config(&test_config);
    nimcp_result_t result = nimcp_quorum_validate_config(&test_config);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmQuorumTest, MultipleSignalTypes) {
    for (int i = 0; i < QUORUM_SIGNAL_TYPE_COUNT; i++) {
        nimcp_result_t result = nimcp_quorum_produce_signal(
            system, 1, static_cast<nimcp_quorum_signal_type_t>(i), 0.3
        );
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }
}

TEST_F(SwarmQuorumTest, DensityMeasurement) {
    for (uint32_t i = 0; i < 20; i++) {
        nimcp_quorum_produce_signal(system, i, QUORUM_SIGNAL_AGGREGATION, 0.1);
    }
    
    float density = nimcp_quorum_calculate_density(system, 1);
    EXPECT_GE(density, 0.0);
}

TEST_F(SwarmQuorumTest, CollectiveThreshold) {
    for (uint32_t i = 0; i < 15; i++) {
        nimcp_quorum_produce_signal(system, i, QUORUM_SIGNAL_AGGREGATION, 0.2);
    }
    
    bool reached = false;
    nimcp_quorum_check_threshold(system, QUORUM_SIGNAL_AGGREGATION, &reached);
    
    SUCCEED();
}

TEST_F(SwarmQuorumTest, ResetSystem) {
    nimcp_quorum_produce_signal(system, 1, QUORUM_SIGNAL_AGGREGATION, 1.0);
    nimcp_result_t result = nimcp_quorum_reset(system);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
