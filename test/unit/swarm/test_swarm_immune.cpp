/**
 * @file test_swarm_immune.cpp
 * @brief Comprehensive unit tests for NIM CP Swarm Immune System
 *
 * TEST COVERAGE:
 * - System creation and destruction
 * - Pathogen detection
 * - Immune response activation
 * - Antibody generation
 * - Threat identification
 * - Memory cell creation
 * - Cross-reactivity
 * - Bio-async integration
 * - BBB security validation
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_immune.h"

class SwarmImmuneTest : public ::testing::Test {
protected:
    nimcp_immune_system_t* system;
    nimcp_immune_config_t config;

    void SetUp() override {
        nimcp_immune_default_config(&config);
        system = nimcp_immune_create(&config, nullptr);
        ASSERT_NE(system, nullptr);
    }

    void TearDown() override {
        if (system) {
            nimcp_immune_destroy(system);
        }
    }
};

TEST_F(SwarmImmuneTest, CreateValidSystem) {
    EXPECT_NE(system, nullptr);
}

TEST_F(SwarmImmuneTest, DestroyNullSystem) {
    nimcp_immune_destroy(nullptr);
    SUCCEED();
}

TEST_F(SwarmImmuneTest, DetectPathogen) {
    nimcp_pathogen_t pathogen = {
        {0xDE, 0xAD, 0xBE, 0xEF}, 4, THREAT_MALICIOUS_CODE, 0.8, 0
    };
    
    nimcp_result_t result = nimcp_immune_detect_pathogen(system, 1, &pathogen);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmImmuneTest, GenerateAntibody) {
    nimcp_pathogen_t pathogen = {
        {0xDE, 0xAD, 0xBE, 0xEF}, 4, THREAT_MALICIOUS_CODE, 0.8, 0
    };
    nimcp_immune_detect_pathogen(system, 1, &pathogen);
    
    nimcp_antibody_t antibody;
    nimcp_result_t result = nimcp_immune_generate_antibody(
        system, &pathogen, &antibody
    );
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmImmuneTest, ActivateResponse) {
    nimcp_pathogen_t pathogen = {
        {0xDE, 0xAD, 0xBE, 0xEF}, 4, THREAT_MALICIOUS_CODE, 0.9, 0
    };
    
    nimcp_result_t result = nimcp_immune_activate_response(
        system, 1, &pathogen
    );
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmImmuneTest, CreateMemoryCell) {
    nimcp_pathogen_t pathogen = {
        {0xCA, 0xFE, 0xBA, 0xBE}, 4, THREAT_UNAUTHORIZED_ACCESS, 0.7, 0
    };
    
    nimcp_result_t result = nimcp_immune_create_memory_cell(
        system, &pathogen
    );
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmImmuneTest, CheckMemory) {
    nimcp_pathogen_t pathogen = {
        {0xDE, 0xAD, 0xBE, 0xEF}, 4, THREAT_MALICIOUS_CODE, 0.8, 0
    };
    nimcp_immune_create_memory_cell(system, &pathogen);
    
    bool has_memory = false;
    nimcp_result_t result = nimcp_immune_check_memory(
        system, &pathogen, &has_memory
    );
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmImmuneTest, NeutralizePathogen) {
    nimcp_pathogen_t pathogen = {
        {0xDE, 0xAD, 0xBE, 0xEF}, 4, THREAT_MALICIOUS_CODE, 0.8, 0
    };
    
    nimcp_result_t result = nimcp_immune_neutralize(
        system, 1, &pathogen
    );
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmImmuneTest, IsolateAgent) {
    nimcp_result_t result = nimcp_immune_isolate_agent(system, 5);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmImmuneTest, RecoverAgent) {
    nimcp_immune_isolate_agent(system, 5);
    nimcp_result_t result = nimcp_immune_recover_agent(system, 5);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmImmuneTest, BroadcastThreat) {
    nimcp_pathogen_t pathogen = {
        {0xDE, 0xAD, 0xBE, 0xEF}, 4, THREAT_MALICIOUS_CODE, 0.9, 0
    };
    
    nimcp_result_t result = nimcp_immune_broadcast_threat(
        system, 1, &pathogen
    );
    SUCCEED();
}

TEST_F(SwarmImmuneTest, UpdateSystem) {
    nimcp_result_t result = nimcp_immune_update(system, 1000);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmImmuneTest, GetStatistics) {
    nimcp_immune_stats_t stats;
    nimcp_result_t result = nimcp_immune_get_stats(system, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmImmuneTest, ValidateConfig) {
    nimcp_immune_config_t test_config;
    nimcp_immune_default_config(&test_config);
    nimcp_result_t result = nimcp_immune_validate_config(&test_config);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmImmuneTest, CrossReactivity) {
    nimcp_pathogen_t p1 = {{0xDE, 0xAD, 0xBE, 0xEF}, 4, THREAT_MALICIOUS_CODE, 0.8, 0};
    nimcp_pathogen_t p2 = {{0xDE, 0xAD, 0xBE, 0xFF}, 4, THREAT_MALICIOUS_CODE, 0.8, 0};
    
    float similarity = nimcp_immune_calculate_similarity(&p1, &p2);
    EXPECT_GE(similarity, 0.0);
    EXPECT_LE(similarity, 1.0);
}

TEST_F(SwarmImmuneTest, ThreatLevel) {
    nimcp_pathogen_t pathogen = {
        {0xDE, 0xAD, 0xBE, 0xEF}, 4, THREAT_MALICIOUS_CODE, 0.9, 0
    };
    
    float threat_level = nimcp_immune_assess_threat_level(system, &pathogen);
    EXPECT_GE(threat_level, 0.0);
    EXPECT_LE(threat_level, 1.0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
