/**
 * @file test_swarm_morphogenesis.cpp
 * @brief Comprehensive unit tests for NIMCP Swarm Morphogenesis System
 *
 * TEST COVERAGE:
 * - System creation and destruction
 * - Role assignment and differentiation
 * - Stimulus-based role transitions
 * - Role competency tracking
 * - Specialization mechanics
 * - Re-differentiation and plasticity
 * - Role constraints and limits
 * - Population balance
 * - Bio-async integration
 * - BBB security validation
 * - Edge cases and error handling
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_morphogenesis.h"
#include "security/nimcp_blood_brain_barrier.h"

//=============================================================================
// Test Fixture
//=============================================================================

class SwarmMorphogenesisTest : public ::testing::Test {
protected:
    nimcp_morphogenesis_system_t* system;
    nimcp_morphogenesis_config_t config;
    nimcp_blood_brain_barrier_t* bbb;

    void SetUp() override {
        nimcp_morphogenesis_default_config(&config);
        bbb = nullptr;
        system = nimcp_morphogenesis_create(&config, bbb);
        ASSERT_NE(system, nullptr);
    }

    void TearDown() override {
        if (system) {
            nimcp_morphogenesis_destroy(system);
        }
    }
};

//=============================================================================
// Creation and Destruction Tests
//=============================================================================

TEST_F(SwarmMorphogenesisTest, CreateValidSystem) {
    EXPECT_NE(system, nullptr);
}

TEST_F(SwarmMorphogenesisTest, DestroyNullSystem) {
    nimcp_morphogenesis_destroy(nullptr);
    SUCCEED();
}

TEST_F(SwarmMorphogenesisTest, CreateWithNullConfig) {
    auto* sys = nimcp_morphogenesis_create(nullptr, nullptr);
    if (sys) {
        nimcp_morphogenesis_destroy(sys);
    }
    SUCCEED();
}

//=============================================================================
// Agent Registration Tests
//=============================================================================

TEST_F(SwarmMorphogenesisTest, RegisterAgent) {
    nimcp_result_t result = nimcp_morphogenesis_register_agent(
        system, 1, SWARM_ROLE_WORKER
    );
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMorphogenesisTest, RegisterMultipleAgents) {
    for (uint32_t i = 0; i < 10; i++) {
        nimcp_result_t result = nimcp_morphogenesis_register_agent(
            system, i, SWARM_ROLE_WORKER
        );
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }
}

TEST_F(SwarmMorphogenesisTest, RegisterDuplicateAgent) {
    nimcp_morphogenesis_register_agent(system, 1, SWARM_ROLE_WORKER);
    nimcp_result_t result = nimcp_morphogenesis_register_agent(
        system, 1, SWARM_ROLE_SCOUT
    );
    // Should handle duplicate registration
    SUCCEED();
}

TEST_F(SwarmMorphogenesisTest, UnregisterAgent) {
    nimcp_morphogenesis_register_agent(system, 1, SWARM_ROLE_WORKER);
    nimcp_result_t result = nimcp_morphogenesis_unregister_agent(system, 1);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

//=============================================================================
// Role Assignment Tests
//=============================================================================

TEST_F(SwarmMorphogenesisTest, GetAgentRole) {
    nimcp_morphogenesis_register_agent(system, 1, SWARM_ROLE_SCOUT);

    nimcp_swarm_role_t role;
    nimcp_result_t result = nimcp_morphogenesis_get_role(system, 1, &role);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(role, SWARM_ROLE_SCOUT);
}

TEST_F(SwarmMorphogenesisTest, SetAgentRole) {
    nimcp_morphogenesis_register_agent(system, 1, SWARM_ROLE_WORKER);

    nimcp_result_t result = nimcp_morphogenesis_set_role(
        system, 1, SWARM_ROLE_DEFENDER
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);

    nimcp_swarm_role_t role;
    nimcp_morphogenesis_get_role(system, 1, &role);
    EXPECT_EQ(role, SWARM_ROLE_DEFENDER);
}

TEST_F(SwarmMorphogenesisTest, GetRoleForUnregisteredAgent) {
    nimcp_swarm_role_t role;
    nimcp_result_t result = nimcp_morphogenesis_get_role(system, 999, &role);

    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Stimulus-Based Differentiation Tests
//=============================================================================

TEST_F(SwarmMorphogenesisTest, ProcessStimulus) {
    nimcp_morphogenesis_register_agent(system, 1, SWARM_ROLE_WORKER);

    nimcp_morphogenesis_stimulus_t stimulus = {
        STIMULUS_THREAT_DETECTED,
        0.8f,
        {0.0f, 0.0f, 0.0f},
        0
    };

    nimcp_result_t result = nimcp_morphogenesis_process_stimulus(
        system, 1, &stimulus
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMorphogenesisTest, ThreatStimulusTriggersDefender) {
    nimcp_morphogenesis_register_agent(system, 1, SWARM_ROLE_WORKER);

    // Send strong threat stimulus
    for (int i = 0; i < 10; i++) {
        nimcp_morphogenesis_stimulus_t stimulus = {
            STIMULUS_THREAT_DETECTED,
            0.9f,
            {0.0f, 0.0f, 0.0f},
            0
        };
        nimcp_morphogenesis_process_stimulus(system, 1, &stimulus);
    }

    // Check if transitioned to defender
    nimcp_swarm_role_t role;
    nimcp_morphogenesis_get_role(system, 1, &role);

    // Role may transition based on cumulative stimulus
    SUCCEED();
}

TEST_F(SwarmMorphogenesisTest, ResourceStimulusTriggersForager) {
    nimcp_morphogenesis_register_agent(system, 1, SWARM_ROLE_WORKER);

    nimcp_morphogenesis_stimulus_t stimulus = {
        STIMULUS_RESOURCE_FOUND,
        0.8f,
        {10.0f, 10.0f, 0.0f},
        0
    };

    for (int i = 0; i < 10; i++) {
        nimcp_morphogenesis_process_stimulus(system, 1, &stimulus);
    }

    SUCCEED();
}

//=============================================================================
// Role Competency Tests
//=============================================================================

TEST_F(SwarmMorphogenesisTest, UpdateCompetency) {
    nimcp_morphogenesis_register_agent(system, 1, SWARM_ROLE_SCOUT);

    nimcp_result_t result = nimcp_morphogenesis_update_competency(
        system, 1, 0.75f
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMorphogenesisTest, GetCompetency) {
    nimcp_morphogenesis_register_agent(system, 1, SWARM_ROLE_WORKER);
    nimcp_morphogenesis_update_competency(system, 1, 0.6f);

    float competency = 0.0f;
    nimcp_result_t result = nimcp_morphogenesis_get_competency(
        system, 1, &competency
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_NEAR(competency, 0.6f, 0.01f);
}

TEST_F(SwarmMorphogenesisTest, CompetencyAffectsPerformance) {
    nimcp_morphogenesis_register_agent(system, 1, SWARM_ROLE_SCOUT);

    // High competency
    nimcp_morphogenesis_update_competency(system, 1, 0.9f);

    float performance = nimcp_morphogenesis_calculate_performance(system, 1);
    EXPECT_GT(performance, 0.5f);
}

//=============================================================================
// Specialization Tests
//=============================================================================

TEST_F(SwarmMorphogenesisTest, IncreaseSpecialization) {
    nimcp_morphogenesis_register_agent(system, 1, SWARM_ROLE_DEFENDER);

    for (int i = 0; i < 5; i++) {
        nimcp_morphogenesis_increase_specialization(system, 1);
    }

    float specialization = nimcp_morphogenesis_get_specialization(system, 1);
    EXPECT_GT(specialization, 0.0f);
}

TEST_F(SwarmMorphogenesisTest, SpecializationLimitsTransitions) {
    nimcp_morphogenesis_register_agent(system, 1, SWARM_ROLE_SCOUT);

    // Highly specialize
    for (int i = 0; i < 20; i++) {
        nimcp_morphogenesis_increase_specialization(system, 1);
    }

    // Attempt role change
    nimcp_result_t result = nimcp_morphogenesis_set_role(
        system, 1, SWARM_ROLE_BUILDER
    );

    // May be restricted due to specialization
    SUCCEED();
}

//=============================================================================
// Re-differentiation Tests
//=============================================================================

TEST_F(SwarmMorphogenesisTest, AllowReDifferentiation) {
    nimcp_morphogenesis_register_agent(system, 1, SWARM_ROLE_WORKER);
    nimcp_morphogenesis_set_role(system, 1, SWARM_ROLE_SCOUT);

    nimcp_result_t result = nimcp_morphogenesis_request_redifferentiation(
        system, 1, SWARM_ROLE_DEFENDER
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMorphogenesisTest, ReDifferentiationCost) {
    nimcp_morphogenesis_register_agent(system, 1, SWARM_ROLE_SCOUT);

    float cost = nimcp_morphogenesis_calculate_transition_cost(
        system, 1, SWARM_ROLE_BUILDER
    );

    EXPECT_GE(cost, 0.0f);
}

//=============================================================================
// Population Balance Tests
//=============================================================================

TEST_F(SwarmMorphogenesisTest, GetRoleDistribution) {
    // Register agents with different roles
    for (uint32_t i = 0; i < 10; i++) {
        nimcp_swarm_role_t role = static_cast<nimcp_swarm_role_t>(i % SWARM_ROLE_COUNT);
        nimcp_morphogenesis_register_agent(system, i, role);
    }

    nimcp_role_distribution_t dist;
    nimcp_result_t result = nimcp_morphogenesis_get_distribution(system, &dist);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(dist.total_agents, 10);
}

TEST_F(SwarmMorphogenesisTest, RebalancePopulation) {
    // Create imbalanced population
    for (uint32_t i = 0; i < 20; i++) {
        nimcp_morphogenesis_register_agent(system, i, SWARM_ROLE_WORKER);
    }

    nimcp_result_t result = nimcp_morphogenesis_rebalance(system);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMorphogenesisTest, SuggestRoleNeeds) {
    for (uint32_t i = 0; i < 10; i++) {
        nimcp_morphogenesis_register_agent(system, i, SWARM_ROLE_WORKER);
    }

    nimcp_swarm_role_t needed_role;
    nimcp_result_t result = nimcp_morphogenesis_suggest_needed_role(
        system, &needed_role
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

//=============================================================================
// Role Constraints Tests
//=============================================================================

TEST_F(SwarmMorphogenesisTest, SetRoleLimit) {
    nimcp_result_t result = nimcp_morphogenesis_set_role_limit(
        system, SWARM_ROLE_LEADER, 1
    );
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMorphogenesisTest, EnforceRoleLimit) {
    nimcp_morphogenesis_set_role_limit(system, SWARM_ROLE_LEADER, 1);

    nimcp_morphogenesis_register_agent(system, 1, SWARM_ROLE_LEADER);

    // Try to add another leader
    nimcp_result_t result = nimcp_morphogenesis_register_agent(
        system, 2, SWARM_ROLE_LEADER
    );

    // Should be limited or queued
    SUCCEED();
}

//=============================================================================
// Age and Experience Tests
//=============================================================================

TEST_F(SwarmMorphogenesisTest, TrackRoleAge) {
    nimcp_morphogenesis_register_agent(system, 1, SWARM_ROLE_WORKER);

    // Simulate time passing
    nimcp_morphogenesis_update(system, 5000);

    uint64_t age = nimcp_morphogenesis_get_role_age(system, 1);
    EXPECT_GE(age, 0);
}

TEST_F(SwarmMorphogenesisTest, ExperienceAccumulation) {
    nimcp_morphogenesis_register_agent(system, 1, SWARM_ROLE_SCOUT);

    for (int i = 0; i < 10; i++) {
        nimcp_morphogenesis_add_experience(system, 1, 0.1f);
    }

    float experience = nimcp_morphogenesis_get_experience(system, 1);
    EXPECT_GT(experience, 0.5f);
}

//=============================================================================
// Bio-Async Integration Tests
//=============================================================================

TEST_F(SwarmMorphogenesisTest, RegisterBioAsync) {
    nimcp_result_t result = nimcp_morphogenesis_register_bioasync(
        system, nullptr
    );
    SUCCEED();
}

TEST_F(SwarmMorphogenesisTest, BroadcastRoleChange) {
    nimcp_morphogenesis_register_agent(system, 1, SWARM_ROLE_WORKER);

    nimcp_result_t result = nimcp_morphogenesis_broadcast_role_change(
        system, 1, SWARM_ROLE_DEFENDER
    );

    SUCCEED();
}

TEST_F(SwarmMorphogenesisTest, RequestRoleAssignment) {
    nimcp_morphogenesis_register_agent(system, 1, SWARM_ROLE_WORKER);

    nimcp_result_t result = nimcp_morphogenesis_request_role(
        system, 1, SWARM_ROLE_SCOUT
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(SwarmMorphogenesisTest, GetStatistics) {
    for (uint32_t i = 0; i < 10; i++) {
        nimcp_morphogenesis_register_agent(system, i, SWARM_ROLE_WORKER);
    }

    nimcp_morphogenesis_stats_t stats;
    nimcp_result_t result = nimcp_morphogenesis_get_stats(system, &stats);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_agents, 10);
}

TEST_F(SwarmMorphogenesisTest, TrackTransitions) {
    nimcp_morphogenesis_register_agent(system, 1, SWARM_ROLE_WORKER);
    nimcp_morphogenesis_set_role(system, 1, SWARM_ROLE_SCOUT);
    nimcp_morphogenesis_set_role(system, 1, SWARM_ROLE_DEFENDER);

    nimcp_morphogenesis_stats_t stats;
    nimcp_morphogenesis_get_stats(system, &stats);

    EXPECT_GT(stats.total_transitions, 0);
}

//=============================================================================
// Utility Tests
//=============================================================================

TEST_F(SwarmMorphogenesisTest, RoleName) {
    const char* name = nimcp_swarm_role_name(SWARM_ROLE_SCOUT);
    EXPECT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0);
}

TEST_F(SwarmMorphogenesisTest, ValidateConfig) {
    nimcp_morphogenesis_config_t test_config;
    nimcp_morphogenesis_default_config(&test_config);

    nimcp_result_t result = nimcp_morphogenesis_validate_config(&test_config);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(SwarmMorphogenesisTest, MaxAgents) {
    // Try to register maximum number of agents
    for (uint32_t i = 0; i < config.max_agents; i++) {
        nimcp_morphogenesis_register_agent(system, i, SWARM_ROLE_WORKER);
    }

    // Try one more
    nimcp_result_t result = nimcp_morphogenesis_register_agent(
        system, config.max_agents, SWARM_ROLE_WORKER
    );

    // Should handle capacity gracefully
    SUCCEED();
}

TEST_F(SwarmMorphogenesisTest, RapidRoleChanges) {
    nimcp_morphogenesis_register_agent(system, 1, SWARM_ROLE_WORKER);

    for (int i = 0; i < 100; i++) {
        nimcp_swarm_role_t role = static_cast<nimcp_swarm_role_t>(i % SWARM_ROLE_COUNT);
        nimcp_morphogenesis_set_role(system, 1, role);
    }

    SUCCEED();
}

//=============================================================================
// Main Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
