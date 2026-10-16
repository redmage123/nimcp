/**
 * @file test_protein_regression.cpp
 * @brief Regression tests for Protein Synthesis System
 *
 * @author NIMCP Development Team
 * @date 2025-12-19
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
    #include "plasticity/protein/nimcp_protein_synthesis.h"

class ProteinRegressionTest : public ::testing::Test {
protected:
    protein_synthesis_system_t system;

    void SetUp() override {
        protein_synthesis_config_t config;
        protein_synthesis_default_config(&config);
        system = protein_synthesis_create(&config);
        ASSERT_NE(system, nullptr);
    }

    void TearDown() override {
        if (system) {
            protein_synthesis_destroy(system);
        }
    }
};

TEST_F(ProteinRegressionTest, PRPPoolStableUnderRepeatedUpdates) {
    float initial_pool = protein_synthesis_get_prp_pool(system);

    /* Update many times */
    for (int i = 0; i < 1000; i++) {
        ASSERT_EQ(protein_synthesis_update(system, 100), 0);
    }

    float final_pool = protein_synthesis_get_prp_pool(system);
    EXPECT_GT(final_pool, 0.0f);
    EXPECT_LT(final_pool, 1e6f);  /* No runaway growth */
}

TEST_F(ProteinRegressionTest, TagExpirationConsistent) {
    protein_synthesis_config_t config;
    protein_synthesis_default_config(&config);

    for (int trial = 0; trial < 10; trial++) {
        protein_synthesis_system_t sys = protein_synthesis_create(&config);
        ASSERT_NE(sys, nullptr);

        ASSERT_EQ(protein_synthesis_set_tag(sys, 1, 0.8f), 0);
        ASSERT_EQ(protein_synthesis_update(sys, config.tag_duration_ms + 1000), 0);

        EXPECT_FALSE(protein_synthesis_is_tagged(sys, 1));

        protein_synthesis_destroy(sys);
    }
}

TEST_F(ProteinRegressionTest, ConsolidationRepeatable) {
    protein_synthesis_config_t config;
    protein_synthesis_default_config(&config);

    for (int trial = 0; trial < 5; trial++) {
        protein_synthesis_system_t sys = protein_synthesis_create(&config);
        ASSERT_NE(sys, nullptr);

        ASSERT_EQ(protein_synthesis_add_prps(sys, 1000.0f), 0);
        ASSERT_EQ(protein_synthesis_set_tag(sys, 1, 0.8f), 0);
        ASSERT_EQ(protein_synthesis_consolidate_synapse(sys, 1), 0);

        synaptic_tag_t tag;
        ASSERT_EQ(protein_synthesis_get_tag(sys, 1, &tag), 0);
        EXPECT_EQ(tag.state, TAG_STATE_CONSOLIDATED);

        protein_synthesis_destroy(sys);
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
